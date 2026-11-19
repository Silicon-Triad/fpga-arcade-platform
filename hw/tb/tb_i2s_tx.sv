// Testbench for i2s_tx (#41): SR-AUD-02, SR-AUD-03.
// Runs one simulated second (12,288,000 aud_clk cycles at 12.288 MHz, SR-AUD-01), counts DACLRCK
// periods, and decodes AUD_DACDAT the way the codec does in slave I2S mode (R7: I2S, 16 bits,
// LRP = 0, BCLKINV = 0): bits are read on BCLK rising edges, the MSB on the second rising edge
// after each DACLRCK edge. Every decoded left and right word must equal the sample handed over at
// the matching sample_tick.
module tb_i2s_tx;

  // verilog_lint: waive-start parameter-name-style
  localparam longint AUD_MCLK_HZ = 12_288_000;  // SR-AUD-01, App. A AUD_MCLK_HZ
  localparam longint AUD_SAMPLE_RATE_HZ = 48_000;  // SR-AUD-02, App. A AUD_SAMPLE_RATE_HZ
  localparam longint RATE_TOL_PPM = 5_000;  // SR-AUD-02: +/- 0.5 %
  localparam int SAMPLE_BITS = 16;  // SR-AUD-03, App. A AUD_SAMPLE_BITS
  localparam longint CLKS_PER_FRAME = AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ;  // 256
  localparam longint CLKS_PER_BCLK = 4;  // design choice in i2s_tx (#41)
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic aud_clk = 1'b0;
  logic aud_rst = 1'b1;
  logic signed [15:0] sample = '0;
  logic sample_tick;
  logic aud_bclk, aud_daclrck, aud_dacdat;

  i2s_tx dut (
      .aud_clk    (aud_clk),
      .aud_rst    (aud_rst),
      .sample     (sample),
      .sample_tick(sample_tick),
      .aud_bclk   (aud_bclk),
      .aud_daclrck(aud_daclrck),
      .aud_dacdat (aud_dacdat)
  );

  // Checks count clocks, so the period does not matter: one second = AUD_MCLK_HZ clocks.
  always #1 aud_clk = ~aud_clk;

  int errors = 0;
  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask
  task automatic expect_eq(input string what, input longint got, input longint want);
    if (got != want) fail($sformatf("%s: got %0d, expected %0d", what, got, want));
  endtask

  // --- Sample source: a new value at every sample_tick -------------------------------------
  // Corner values first, then a 16-bit LFSR, so every bit position takes both values.
  localparam logic [15:0] CORNERS[6] = '{
      16'h7FFF,
      16'h8000,
      16'h0000,
      16'hFFFF,
      16'h5555,
      16'hAAAA
  };
  logic [15:0] lfsr_q = 16'hACE1;
  int corner_i = 0;
  logic [15:0] expected[$];  // samples in the order they must come out
  initial expected.push_back(16'h0000);  // the first frame after reset sends the reset value

  always @(posedge aud_clk) begin
    if (!aud_rst && sample_tick) begin
      // i2s_tx takes `sample` at this edge; record it, then present the next one.
      expected.push_back(sample);
      if (corner_i < 6) begin
        sample   <= CORNERS[corner_i];
        corner_i <= corner_i + 1;
      end else begin
        sample <= lfsr_q;
        lfsr_q <= {lfsr_q[14:0], lfsr_q[15] ^ lfsr_q[13] ^ lfsr_q[12] ^ lfsr_q[10]};
      end
    end
  end

  // --- Checker, on the falling aud_clk edge ---------------------------------------------------
  longint clk_count = 0;
  longint lrck_periods = 0, frames_checked = 0, ticks = 0;
  longint last_lrck_fall = -1, last_lrck_rise = -1, last_bclk_rise = -1, last_tick = -1;
  longint bclks_in_frame = 0;
  longint first_lrck_fall = -1, falls_in_second = 0;  // SR-AUD-02 window: 1 s from the first fall
  logic prev_bclk = 1'b0, prev_lrck = 1'b0, prev_dat = 1'b0;
  // Codec model state
  logic lrck_at_rise = 1'b0;
  int   bit_idx = -1;  // -1 until the first DACLRCK edge has been seen
  logic [15:0] word = '0, left_word = '0;
  bit have_left = 1'b0;

  always @(negedge aud_clk) begin
    if (aud_rst) begin
      if (aud_bclk !== 1'b0 || aud_daclrck !== 1'b0 || aud_dacdat !== 1'b0 || sample_tick !== 1'b0)
      begin
        fail("outputs during reset are not all 0");
      end
    end else begin
      clk_count++;
      if ($isunknown({aud_bclk, aud_daclrck, aud_dacdat, sample_tick})) fail("X or Z on outputs");

      // DACLRCK and DACDAT change only together with a BCLK falling edge (stable at the rising
      // edge, where the codec samples: BCLKINV = 0).
      if ((aud_daclrck != prev_lrck || aud_dacdat != prev_dat) && !(prev_bclk && !aud_bclk)) begin
        fail("DACLRCK or DACDAT changed without a BCLK falling edge");
      end

      // sample_tick: one clock, once per frame, on the last clock of the frame.
      if (sample_tick) begin
        if (last_tick >= 0)
          expect_eq("clocks between sample_ticks", clk_count - last_tick, CLKS_PER_FRAME);
        if (last_lrck_fall >= 0) begin
          // The pins are registered from the frame counter, so they lag it by one clock: the tick
          // (last counter clock of a frame) comes 2 clocks before the next DACLRCK fall.
          expect_eq("sample_tick position (clocks after DACLRCK fall)", clk_count - last_lrck_fall,
                    CLKS_PER_FRAME - 2);
        end
        last_tick = clk_count;
        ticks++;
      end

      // DACLRCK: 48 kHz, 50 % duty (SR-AUD-02).
      if (prev_lrck && !aud_daclrck) begin
        if (last_lrck_fall >= 0) begin
          expect_eq("clocks per DACLRCK period", clk_count - last_lrck_fall, CLKS_PER_FRAME);
          expect_eq("BCLK periods per frame", bclks_in_frame, CLKS_PER_FRAME / CLKS_PER_BCLK);
          lrck_periods++;
        end
        if (last_lrck_rise >= 0) begin
          expect_eq("DACLRCK high (right) time", clk_count - last_lrck_rise, CLKS_PER_FRAME / 2);
        end
        if (first_lrck_fall < 0) first_lrck_fall = clk_count;
        if (clk_count - first_lrck_fall < AUD_MCLK_HZ) falls_in_second++;
        bclks_in_frame = 0;
        last_lrck_fall = clk_count;
      end
      if (!prev_lrck && aud_daclrck) begin
        if (last_lrck_fall >= 0) begin
          expect_eq("DACLRCK low (left) time", clk_count - last_lrck_fall, CLKS_PER_FRAME / 2);
        end
        last_lrck_rise = clk_count;
      end

      // BCLK rising edge: period, and the codec reading one bit (SR-AUD-03).
      if (!prev_bclk && aud_bclk) begin
        if (last_bclk_rise >= 0)
          expect_eq("clocks per BCLK", clk_count - last_bclk_rise, CLKS_PER_BCLK);
        last_bclk_rise = clk_count;
        bclks_in_frame++;

        if (aud_daclrck != lrck_at_rise) bit_idx = 0;  // first rising edge after a DACLRCK edge
        else if (bit_idx >= 0) bit_idx++;
        lrck_at_rise = aud_daclrck;

        if (bit_idx >= 1 && bit_idx <= SAMPLE_BITS) begin
          word = {word[14:0], aud_dacdat};  // MSB first
        end else if (bit_idx >= 0 && aud_dacdat !== 1'b0) begin
          fail($sformatf("DACDAT=1 in slot bit %0d (outside the 16 data bits)", bit_idx));
        end

        if (bit_idx == SAMPLE_BITS) begin
          if (!aud_daclrck) begin
            left_word = word;
            have_left = 1'b1;
          end else if (have_left) begin
            // SR-AUD-03: left equals right, and both equal the sample taken for this frame.
            if (word != left_word) begin
              fail($sformatf("left %04h != right %04h", left_word, word));
            end
            if (expected.size() == 0) fail("frame decoded with no sample handed over");
            else begin
              logic [15:0] want;
              want = expected.pop_front();
              if (left_word != want) begin
                fail($sformatf(
                     "frame %0d: decoded %04h, expected %04h", frames_checked, left_word, want));
              end
            end
            frames_checked++;
            have_left = 1'b0;
          end else begin
            // First frame after reset: its left word started before the decoder had seen a
            // DACLRCK edge, so only drop its expected sample.
            if (expected.size() != 0) void'(expected.pop_front());
          end
        end
      end

      prev_bclk = aud_bclk;
      prev_lrck = aud_daclrck;
      prev_dat  = aud_dacdat;
    end
  end

  // Reset is released on a rising edge, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge aud_clk) if (release_rst) aud_rst <= 1'b0;

  initial begin
    longint lo, hi;
    repeat (5) @(posedge aud_clk);
    release_rst = 1'b1;
    @(posedge aud_clk);
    // One simulated second (SR-AUD-02 verification: "per second of simulated time"), counted from
    // the first DACLRCK fall, plus two frames so the window closes.
    repeat (int'(AUD_MCLK_HZ + 3 * CLKS_PER_FRAME)) @(posedge aud_clk);
    @(negedge aud_clk);

    // SR-AUD-02: 48,000 frames per second +/- 0.5 %.
    lo = AUD_SAMPLE_RATE_HZ - AUD_SAMPLE_RATE_HZ * RATE_TOL_PPM / 1_000_000;
    hi = AUD_SAMPLE_RATE_HZ + AUD_SAMPLE_RATE_HZ * RATE_TOL_PPM / 1_000_000;
    if (falls_in_second < lo || falls_in_second > hi) begin
      fail($sformatf("DACLRCK periods in 1 s: %0d, expected %0d to %0d", falls_in_second, lo, hi));
    end
    if (frames_checked < AUD_SAMPLE_RATE_HZ - 2) begin
      fail($sformatf("only %0d frames decoded", frames_checked));
    end
    $display("1 s = %0d aud_clk: %0d DACLRCK periods (%0d to %0d allowed)", AUD_MCLK_HZ,
             falls_in_second, lo, hi);
    $display("%0d clocks run, %0d whole periods checked, %0d sample_ticks", clk_count,
             lrck_periods, ticks);
    $display("decoded %0d frames, left = right and equal to the sample in each, %0d errors",
             frames_checked, errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
