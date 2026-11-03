// Self-checking testbench for i2s_tx (#41): SR-AUD-02, SR-AUD-03.
// Time is counted in clocks: one simulated second is AUD_MCLK_HZ clocks of the 12.288 MHz
// master clock (SR-AUD-01), so no timescale is needed. The testbench decodes the I2S stream as
// the codec does (sampling on the rising edge of BCLK) and compares each word with the samples
// it gave the transmitter.

// verilog_lint: waive-start parameter-name-style
module tb_i2s_tx;

  localparam longint AUD_MCLK_HZ = 12_288_000;  // App. A, SR-AUD-01
  localparam longint AUD_SAMPLE_RATE_HZ = 48_000;  // App. A, SR-AUD-02
  localparam longint AUD_SAMPLE_BITS = 16;  // App. A, SR-AUD-03
  localparam longint BCLK_PER_FRAME = 2 * AUD_SAMPLE_BITS;
  localparam longint SIM_CLOCKS = AUD_MCLK_HZ;  // one simulated second
  // SR-AUD-02: 48,000 frames per second +/- 0.5 % -> 47,760 to 48,240.
  localparam longint LRCK_MIN = AUD_SAMPLE_RATE_HZ * 995 / 1000;
  localparam longint LRCK_MAX = AUD_SAMPLE_RATE_HZ * 1005 / 1000;
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic clk = 1'b0;
  logic rst = 1'b1;
  logic signed [15:0] sample = '0;
  logic sample_load, aud_bclk, aud_daclrck, aud_dacdat;

  i2s_tx dut (
      .clk        (clk),
      .rst        (rst),
      .sample     (sample),
      .sample_load(sample_load),
      .aud_bclk   (aud_bclk),
      .aud_daclrck(aud_daclrck),
      .aud_dacdat (aud_dacdat)
  );

  always #1 clk = ~clk;

  int errors = 0;
  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask

  // Edge values first, then pseudo-random ones.
  logic [15:0] fixed_samples[8] = '{
      16'h8000,
      16'h7FFF,
      16'h0000,
      16'hFFFF,
      16'h5555,
      16'hAAAA,
      16'h0001,
      16'hE000
  };
  int next_fixed = 0;
  logic [15:0] sent[$];  // samples latched by the DUT, oldest first

  // Stimulus and edge checks on the falling clock edge, away from the DUT's rising edge.
  longint clk_count = 0;
  longint lrck_rises = 0, bclk_rises = 0, loads = 0;
  longint clocks_since_load = 0;
  logic prev_bclk = 1'b0, prev_lrck = 1'b0, prev_dat = 1'b0;
  always @(negedge clk) begin
    if (!rst) begin
      clk_count++;
      if ($isunknown({sample_load, aud_bclk, aud_daclrck, aud_dacdat})) fail("X or Z on outputs");
      if (aud_daclrck && !prev_lrck) lrck_rises++;
      if (aud_bclk && !prev_bclk) bclk_rises++;
      // SR-AUD-03 (I2S): LRCK and DACDAT change only when BCLK falls.
      if ((aud_daclrck != prev_lrck || aud_dacdat != prev_dat) && !(prev_bclk && !aud_bclk)) begin
        fail("LRCK or DACDAT changed without a falling BCLK edge");
      end
      clocks_since_load++;
      if (sample_load) begin
        // SR-AUD-02: one sample per frame.
        if (loads > 0 && clocks_since_load != AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ) begin
          fail($sformatf(
               "%0d clocks between sample loads, expected %0d",
               clocks_since_load,
               AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ
               ));
        end
        if (aud_daclrck) fail("sample loaded while LRCK is high (right channel)");
        clocks_since_load = 0;
        loads++;
        sent.push_back(sample);
        if (next_fixed < 8) sample = fixed_samples[next_fixed++];
        else sample = 16'($urandom);
      end
      prev_bclk = aud_bclk;
      prev_lrck = aud_daclrck;
      prev_dat  = aud_dacdat;
    end
  end

  // Decoder, as the codec: on each rising BCLK edge, the bit at an LRCK change is the LSB of the
  // word of the previous channel; the 15 bits before it are its upper bits, MSB first.
  logic [15:0] acc = '0;
  longint acc_bits = 0;
  logic dec_lrck = 1'b0;
  logic [15:0] left_word = '0;
  bit have_left = 1'b0;
  longint frames_decoded = 0, words_seen = 0;
  always @(posedge aud_bclk) begin
    if (!rst) begin
      if (aud_daclrck != dec_lrck) begin
        logic [15:0] word = {acc[14:0], aud_dacdat};
        words_seen++;
        // Frame 0 after reset is sent before the first sample is latched: skip its two words.
        if (words_seen > 2) begin
          if (acc_bits != AUD_SAMPLE_BITS - 1) begin
            fail($sformatf("word has %0d bits, expected %0d", acc_bits + 1, AUD_SAMPLE_BITS));
          end
          if (!dec_lrck) begin
            left_word = word;
            have_left = 1'b1;
          end else if (have_left) begin
            // SR-AUD-03: left and right carry the same sample.
            if (word != left_word) begin
              fail($sformatf("L = %h but R = %h", left_word, word));
            end
            if (sent.size() == 0) begin
              fail("decoded a frame with no sample sent");
            end else begin
              logic [15:0] want = sent.pop_front();
              if (left_word != want) begin
                fail($sformatf("decoded %h, sent %h (frame %0d)", left_word, want, frames_decoded));
              end
            end
            frames_decoded++;
            have_left = 1'b0;
          end
        end
        acc = '0;
        acc_bits = 0;
        dec_lrck = aud_daclrck;
      end else begin
        acc = {acc[14:0], aud_dacdat};
        acc_bits++;
      end
    end
  end

  // Reset is released on a rising edge, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge clk) if (release_rst) rst <= 1'b0;

  initial begin
    repeat (5) @(posedge clk);
    release_rst = 1'b1;
    @(negedge clk);
    // After reset: BCLK, LRCK and DACDAT low (left channel, slot 0).
    if (aud_bclk || aud_daclrck || aud_dacdat) fail("outputs not low after reset");
    repeat (int'(SIM_CLOCKS)) @(negedge clk);

    // SR-AUD-02: LRCK periods in one simulated second.
    if (lrck_rises < LRCK_MIN || lrck_rises > LRCK_MAX) begin
      fail($sformatf("%0d LRCK periods in 1 s, expected %0d to %0d", lrck_rises, LRCK_MIN, LRCK_MAX
           ));
    end
    if (bclk_rises / BCLK_PER_FRAME != lrck_rises) begin
      fail($sformatf(
           "%0d BCLK periods for %0d LRCK periods, expected %0d per frame",
           bclk_rises,
           lrck_rises,
           BCLK_PER_FRAME
           ));
    end
    // SR-AUD-03: every frame was decoded and matched, except frame 0 (sent before the first
    // sample load) and the last one (still being sent when the second ends).
    if (frames_decoded < lrck_rises - 2) begin
      fail($sformatf("only %0d frames decoded for %0d LRCK periods", frames_decoded, lrck_rises));
    end
    $display("clocks %0d, LRCK periods %0d, BCLK periods %0d, samples %0d, frames decoded %0d",
             clk_count, lrck_rises, bclk_rises, loads, frames_decoded);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
