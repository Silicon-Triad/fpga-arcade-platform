// Testbench for tone_generator (#42): SR-AUD-04 to SR-AUD-09, SF-05.
// pix_clk and aud_clk run at unrelated rates. A model of i2s_tx gives sample_tick every 256 aud_clk
// (48 kHz at 12.288 MHz, SR-AUD-02); the checker reads `sample` the way i2s_tx takes it, at each
// tick. Tone register writes copy bridge_regs: Tone Frequency first, then Tone Duration with a
// one-pix_clk tone_start pulse (A6), as the HAL does (SR-HAL-11, #117).
module tb_tone_generator;

  // verilog_lint: waive-start parameter-name-style
  localparam longint AUD_MCLK_HZ = 12_288_000;  // SR-AUD-01, App. A AUD_MCLK_HZ
  localparam longint AUD_SAMPLE_RATE_HZ = 48_000;  // SR-AUD-02, App. A AUD_SAMPLE_RATE_HZ
  localparam longint CLKS_PER_SAMPLE = AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ;  // 256
  localparam longint CLKS_PER_MS = AUD_MCLK_HZ / 1000;  // 12,288
  // SR-AUD-07, SF-05, App. A TONE_AMPLITUDE_MAX
  localparam logic signed [15:0] TONE_AMPLITUDE_MAX = 16'sd8192;
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic pix_clk = 1'b0, aud_clk = 1'b0;
  logic pix_rst = 1'b1, aud_rst = 1'b1;
  logic [15:0] tone_frequency = '0, tone_duration = '0;
  logic tone_start = 1'b0, audio_enable = 1'b0, audio_mute = 1'b0;
  logic sample_tick = 1'b0;
  logic signed [15:0] sample;

  tone_generator dut (
      .pix_clk       (pix_clk),
      .pix_rst       (pix_rst),
      .tone_frequency(tone_frequency),
      .tone_duration (tone_duration),
      .tone_start    (tone_start),
      .audio_enable  (audio_enable),
      .audio_mute    (audio_mute),
      .aud_clk       (aud_clk),
      .aud_rst       (aud_rst),
      .sample_tick   (sample_tick),
      .sample        (sample)
  );

  // Checks count aud_clk cycles, so only the ratio matters. pix_clk is faster than aud_clk, as on
  // the board, at an unrelated ratio (14 : 10); the toggle crossing works at any ratio.
  // pix_clk runs only around register writes (pix_run): between writes nothing in the pix_clk
  // domain changes, and stopping it keeps this 2.6 s simulation inside run.sh's time limit.
  always #7 aud_clk = ~aud_clk;
  bit pix_run = 1'b1;
  always begin
    wait (pix_run);
    #5 pix_clk = ~pix_clk;
  end

  int errors = 0;
  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask

  // --- i2s_tx model: sample_tick on one clock in every 256 ------------------------------------
  int tick_div = 0;
  always @(posedge aud_clk) begin
    if (aud_rst) begin
      tick_div <= 0;
      sample_tick <= 1'b0;
    end else begin
      tick_div <= (tick_div == int'(CLKS_PER_SAMPLE) - 1) ? 0 : tick_div + 1;
      sample_tick <= (tick_div == int'(CLKS_PER_SAMPLE) - 2);
    end
  end

  // --- Checker on the falling aud_clk edge ------------------------------------------------------
  longint aud_count = 0, ticks = 0;
  logic signed [15:0] prev_taken = '0;
  longint last_rise_tick = -1;
  bit collect = 1'b0;
  longint periods[$];  // samples per square-wave period, while collect = 1
  longint nonzero_taken = 0;

  always @(negedge aud_clk) begin
    if (!aud_rst) begin
      aud_count++;
      if ($isunknown(sample)) fail("X or Z on sample");
      // SR-AUD-07, SF-05: the only values are 0 and +/- TONE_AMPLITUDE_MAX.
      if (sample != 0 && sample != TONE_AMPLITUDE_MAX && sample != -TONE_AMPLITUDE_MAX) begin
        fail($sformatf("sample %0d is not 0 or +/-%0d", sample, TONE_AMPLITUDE_MAX));
      end
      if (sample_tick) begin
        // The value i2s_tx takes at the end of this clock.
        ticks++;
        if (sample != 0) nonzero_taken++;
        if (prev_taken < 0 && sample > 0) begin
          if (collect && last_rise_tick >= 0) periods.push_back(ticks - last_rise_tick);
          last_rise_tick = ticks;
        end
        if (sample == 0) last_rise_tick = -1;
        prev_taken = sample;
      end
    end
  end

  // --- Register writes, as bridge_regs presents them (pix_clk domain) -------------------------
  // Inputs change on the falling pix_clk edge, so the DUT's pix_clk flip-flops never race them.
  longint start_count;  // aud_count when tone_start was pulsed
  task automatic play(input int freq_hz, input int dur_ms);
    pix_run = 1'b1;
    @(negedge pix_clk);
    tone_frequency = 16'(freq_hz);  // Tone Frequency write
    repeat (4) @(negedge pix_clk);
    tone_duration = 16'(dur_ms);  // Tone Duration write pulses tone_start (A6)
    tone_start = 1'b1;
    start_count = aud_count;
    @(negedge pix_clk);
    tone_start = 1'b0;
    repeat (2) @(negedge pix_clk);
    pix_run = 1'b0;
  endtask

  task automatic set_audio_control(input bit enable, input bit mute);
    pix_run = 1'b1;
    @(negedge pix_clk);
    audio_enable = enable;
    audio_mute   = mute;
    repeat (2) @(negedge pix_clk);
    pix_run = 1'b0;
  endtask

  task automatic wait_aud(input longint n);
    repeat (int'(n)) @(posedge aud_clk);
  endtask

  // Waits until sample is 0 again; returns the aud_clk count since tone_start.
  // First waits for the tone to reach `sample` (it takes a few aud_clk to cross), then for 0.
  task automatic wait_silent(input longint timeout_clks, output longint clks);
    longint t0;
    t0 = aud_count;
    @(negedge aud_clk);
    while (sample == 0 && aud_count - t0 < CLKS_PER_SAMPLE) @(negedge aud_clk);
    if (sample == 0) fail("tone did not start within 1 sample");
    while (sample != 0 && aud_count - t0 < timeout_clks) @(negedge aud_clk);
    clks = aud_count - start_count;
  endtask

  // SR-AUD-04: every whole period within +/- 1 % of 48,000 / f samples.
  task automatic check_frequency(input int freq_hz, input int dur_ms);
    real want, got_avg, sum;
    longint clks;
    periods.delete();
    last_rise_tick = -1;  // measure only this tone's own periods
    prev_taken = '0;
    collect = 1'b1;
    play(freq_hz, dur_ms);
    wait_silent((longint'(dur_ms) + 5) * CLKS_PER_MS, clks);
    collect = 1'b0;
    want = real'(AUD_SAMPLE_RATE_HZ) / real'(freq_hz);
    sum = 0.0;
    foreach (periods[i]) begin
      sum += real'(periods[i]);
      if (real'(periods[i]) < want * 0.99 || real'(periods[i]) > want * 1.01) begin
        fail($sformatf(
             "%0d Hz: period %0d samples, expected %.2f +/- 1 %%", freq_hz, periods[i], want));
      end
    end
    if (periods.size() < 5)
      fail($sformatf("%0d Hz: only %0d periods measured", freq_hz, periods.size()));
    else begin
      got_avg = sum / real'(periods.size());
      $display("SR-AUD-04 %4d Hz: %0d periods, mean %.3f samples (expected %.3f) = %.2f Hz",
               freq_hz, periods.size(), got_avg, want, real'(AUD_SAMPLE_RATE_HZ) / got_avg);
    end
  endtask

  // SR-AUD-05: the tone stops (sample 0) after the duration, within +/- 1 ms.
  task automatic check_duration(input int dur_ms);
    longint clks;
    play(440, dur_ms);
    wait_silent((longint'(dur_ms) + 5) * CLKS_PER_MS, clks);
    if (clks < (longint'(dur_ms) - 1) * CLKS_PER_MS ||
        clks > (longint'(dur_ms) + 1) * CLKS_PER_MS) begin
      fail($sformatf("%0d ms: tone lasted %.3f ms", dur_ms, real'(clks) / real'(CLKS_PER_MS)));
    end
    $display("SR-AUD-05 %4d ms: tone lasted %.3f ms", dur_ms, real'(clks) / real'(CLKS_PER_MS));
  endtask

  // Watchdog: a design that hangs a wait still ends with FAIL and a reason, before run.sh's limit.
  initial begin
    wait_aud(3 * AUD_MCLK_HZ);
    fail("watchdog: test did not finish within 3 s of simulated time");
    $display("FAIL");
    $finish;
  end

  // Resets are released on rising edges, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge pix_clk) if (release_rst) pix_rst <= 1'b0;
  always @(posedge aud_clk) if (release_rst) aud_rst <= 1'b0;

  initial begin
    longint clks, n0;
    repeat (4) @(posedge aud_clk);
    release_rst = 1'b1;
    wait_aud(10);
    pix_run = 1'b0;

    // SR-AUD-09: audio enable 0 (reset value), a tone plays but every sample is 0.
    n0 = nonzero_taken;
    play(440, 20);
    wait_aud(25 * CLKS_PER_MS);
    if (nonzero_taken != n0) fail("SR-AUD-09: sound while audio enable is 0");
    else $display("SR-AUD-09: 20 ms tone with audio enable 0: all samples 0");

    set_audio_control(1'b1, 1'b0);
    wait_aud(10);

    // SR-AUD-04: 100, 440 and 4,000 Hz.
    check_frequency(100, 100);
    check_frequency(440, 100);
    check_frequency(4000, 100);

    // SR-AUD-05: 10, 100 and 2,000 ms.
    check_duration(10);
    check_duration(100);
    check_duration(2000);

    // SR-AUD-06: a new tone during an old one starts within 1 sample period and replaces it.
    // Start it while the old tone is low, so the new tone's first (high) half shows the change.
    play(200, 2000);
    wait_aud(20 * CLKS_PER_MS);
    n0 = aud_count;
    @(negedge aud_clk);
    while (sample != -TONE_AMPLITUDE_MAX && aud_count - n0 < 10 * CLKS_PER_MS) @(negedge aud_clk);
    if (sample != -TONE_AMPLITUDE_MAX) fail("SR-AUD-06: old 200 Hz tone never went low");
    play(4000, 30);
    @(negedge aud_clk);
    while (sample != TONE_AMPLITUDE_MAX && aud_count - start_count < 4 * CLKS_PER_SAMPLE) begin
      @(negedge aud_clk);
    end
    clks = aud_count - start_count;
    if (clks > CLKS_PER_SAMPLE) begin
      fail($sformatf(
           "SR-AUD-06: new tone after %0d aud_clk, more than 1 sample (%0d)", clks, CLKS_PER_SAMPLE
           ));
    end
    $display("SR-AUD-06: new tone reached `sample` %0d aud_clk (%.2f us) after tone_start", clks,
             real'(clks) * 1.0e6 / real'(AUD_MCLK_HZ));
    wait_silent(40 * CLKS_PER_MS, clks);
    if (clks > 31 * CLKS_PER_MS) fail("SR-AUD-06: old tone kept playing after the new one ended");

    // SR-AUD-08: mute gives 0 while set; the tone resumes when cleared.
    play(440, 200);
    wait_aud(20 * CLKS_PER_MS);
    set_audio_control(1'b1, 1'b1);
    wait_aud(10);
    n0 = nonzero_taken;
    wait_aud(50 * CLKS_PER_MS);
    if (nonzero_taken != n0) fail("SR-AUD-08: sound while muted");
    set_audio_control(1'b1, 1'b0);
    wait_aud(20 * CLKS_PER_MS);
    if (nonzero_taken == n0) fail("SR-AUD-08: no sound after unmute");
    else $display("SR-AUD-08: muted for 50 ms: all samples 0; sound again after unmute");

    // SR-AUD-09 again: clearing audio enable silences a playing tone.
    set_audio_control(1'b0, 1'b0);
    wait_aud(10);
    n0 = nonzero_taken;
    wait_aud(20 * CLKS_PER_MS);
    if (nonzero_taken != n0) fail("SR-AUD-09: sound after audio enable was cleared");
    set_audio_control(1'b1, 1'b0);

    // Outside the SRS ranges (HAL never writes these): silence, not a stuck level.
    // The 440 Hz tone is still playing; each out-of-range write replaces it with silence.
    play(50, 100);
    wait_aud(2 * CLKS_PER_SAMPLE);
    n0 = nonzero_taken;
    wait_aud(10 * CLKS_PER_MS);
    play(440, 5000);
    wait_aud(10 * CLKS_PER_MS);
    if (nonzero_taken != n0) fail("out-of-range frequency or duration made sound");
    else $display("out of range (50 Hz; 5,000 ms): silence");

    $display("checked %0d samples, %0d errors", ticks, errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
