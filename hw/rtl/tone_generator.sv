// Tone Generator (#42): one square-wave tone at a time. SR-AUD-04 to SR-AUD-09, CON-13, SF-05.
// Ports as agreed on #41 with #116.
//
// Two clock domains. The tone registers come from bridge_regs on pix_clk; the sound is made on
// aud_clk (12.288 MHz), timed by i2s_tx's sample_tick (48 kHz). Crossings (R-4.2.5), all
// pix_clk -> aud_clk, the two clocks asynchronous (set_clock_groups -asynchronous in the .sdc):
//   start_toggle_q (pix)  -> start_meta_q -> start_sync_q   toggle synchronizer for tone_start
//   audio_enable          -> enable_meta_q -> enable_q     2-FF synchronizer, level
//   audio_mute            -> mute_meta_q -> mute_q         2-FF synchronizer, level
//   tone_frequency, tone_duration: read in aud_clk only on the clock the toggle edge arrives.
//
// HAL dependency: the capture is safe because the HAL writes Tone Frequency before Tone Duration
// (and Tone Duration pulses tone_start, bridge_regs A6), then writes neither again for at least a
// game tick (SR-HAL-11, #117). By the time the toggle has crossed (2-3 aud_clk) both values have
// been stable for that long. If the HAL write order changes, this capture has to change with it.
module tone_generator (
    // pix_clk domain: from bridge_regs (§3.1.4)
    input logic        pix_clk,         // pixel clock, 65 MHz (or ~25 MHz in a bring-up build)
    input logic        pix_rst,         // synchronous to pix_clk, active high
    input logic [15:0] tone_frequency,  // Hz (SR-AUD-04)
    input logic [15:0] tone_duration,   // ms (SR-AUD-05)
    input logic        tone_start,      // 1-pix_clk pulse on a Tone Duration write (bridge_regs A6)
    input logic        audio_enable,    // Audio Control bit 0 (SR-AUD-09)
    input logic        audio_mute,      // Audio Control bit 1 (SR-AUD-08)

    // aud_clk domain
    input  logic               aud_clk,      // 12.288 MHz codec master clock (SR-AUD-01)
    input  logic               aud_rst,      // synchronous to aud_clk, active high
    input  logic               sample_tick,  // from i2s_tx: 1 clock per 48 kHz frame
    output logic signed [15:0] sample        // to i2s_tx
);

  // verilog_lint: waive-start parameter-name-style
  localparam int AUD_SAMPLE_RATE_HZ = 48_000;  // SR-AUD-02, App. A AUD_SAMPLE_RATE_HZ
  localparam int SAMPLES_PER_MS = AUD_SAMPLE_RATE_HZ / 1000;  // 48
  localparam logic [15:0] TONE_FREQ_MIN = 16'd100;  // SR-AUD-04, App. A TONE_FREQ_MIN
  localparam logic [15:0] TONE_FREQ_MAX = 16'd4000;  // SR-AUD-04, App. A TONE_FREQ_MAX
  localparam logic [15:0] TONE_DURATION_MIN = 16'd10;  // SR-AUD-05, App. A TONE_DURATION_MIN
  localparam logic [15:0] TONE_DURATION_MAX = 16'd2000;  // SR-AUD-05, App. A TONE_DURATION_MAX
  localparam logic signed [15:0] TONE_AMPLITUDE_MAX = 16'sd8192;  // SR-AUD-07, SF-05, App. A
  localparam int PHASE_W = 17;  // holds 2 x TONE_FREQ_MAX + AUD_SAMPLE_RATE_HZ < 2^17
  localparam int REMAIN_W = 17;  // holds TONE_DURATION_MAX x SAMPLES_PER_MS = 96,000 < 2^17
  // verilog_lint: waive-stop parameter-name-style

  // ---------------------------------------------------------------- pix_clk domain
  logic start_toggle_q;  // flips on every tone_start
  always_ff @(posedge pix_clk) begin
    if (pix_rst) start_toggle_q <= 1'b0;
    else if (tone_start) start_toggle_q <= ~start_toggle_q;
  end

  // ---------------------------------------------------------------- pix_clk -> aud_clk
  logic start_meta_q, start_sync_q, start_seen_q;
  logic enable_meta_q, enable_q, mute_meta_q, mute_q;
  always_ff @(posedge aud_clk) begin
    if (aud_rst) begin
      start_meta_q <= 1'b0;
      start_sync_q <= 1'b0;
      start_seen_q <= 1'b0;
      enable_meta_q <= 1'b0;  // reset value of Audio Control: silent (§3.1.4, SR-AUD-09)
      enable_q <= 1'b0;
      mute_meta_q <= 1'b0;
      mute_q <= 1'b0;
    end else begin
      start_meta_q <= start_toggle_q;
      start_sync_q <= start_meta_q;
      start_seen_q <= start_sync_q;
      enable_meta_q <= audio_enable;
      enable_q <= enable_meta_q;
      mute_meta_q <= audio_mute;
      mute_q <= mute_meta_q;
    end
  end

  logic new_tone;  // one aud_clk when a tone_start has crossed
  assign new_tone = start_sync_q ^ start_seen_q;

  // ---------------------------------------------------------------- aud_clk: the tone
  // Square wave from a phase accumulator stepped once per sample: it gains 2 x frequency per
  // sample and the output flips each time it passes AUD_SAMPLE_RATE_HZ, so there are exactly
  // 2 x frequency flips per second (SR-AUD-04). Duration counts samples: 48 per ms (SR-AUD-05).
  logic tone_active_q;  // App. A tone_active
  logic level_high_q;  // square-wave level: 1 = +amplitude, 0 = -amplitude
  logic [15:0] freq_q;
  logic [PHASE_W-1:0] phase_q;
  logic [REMAIN_W-1:0] remaining_q;  // samples left (App. A tone_remaining_ms x 48)
  logic [PHASE_W-1:0] phase_next;
  logic in_range;

  assign phase_next = phase_q + {freq_q, 1'b0};
  // Values outside the SRS ranges play nothing; the HAL never writes them (SR-HAL-11 checks
  // TBL-SoundEffect against the same limits).
  assign in_range = (tone_frequency >= TONE_FREQ_MIN) && (tone_frequency <= TONE_FREQ_MAX) &&
      (tone_duration >= TONE_DURATION_MIN) && (tone_duration <= TONE_DURATION_MAX);

  always_ff @(posedge aud_clk) begin
    if (aud_rst) begin
      tone_active_q <= 1'b0;
      level_high_q <= 1'b1;
      freq_q <= '0;
      phase_q <= '0;
      remaining_q <= '0;
    end else if (new_tone) begin
      // SR-AUD-06: the new tone replaces the old one at once, starting on a high half-period.
      tone_active_q <= in_range;
      level_high_q <= 1'b1;
      freq_q <= tone_frequency;
      phase_q <= '0;
      remaining_q <= REMAIN_W'(tone_duration) * REMAIN_W'(SAMPLES_PER_MS);
    end else if (sample_tick && tone_active_q) begin
      if (remaining_q == REMAIN_W'(1))
        tone_active_q <= 1'b0;  // SR-AUD-05: this was the last sample
      remaining_q <= remaining_q - REMAIN_W'(1);
      if (phase_next >= PHASE_W'(AUD_SAMPLE_RATE_HZ)) begin
        phase_q <= phase_next - PHASE_W'(AUD_SAMPLE_RATE_HZ);
        level_high_q <= ~level_high_q;
      end else begin
        phase_q <= phase_next;
      end
    end
  end

  // SR-AUD-07: only +/- TONE_AMPLITUDE_MAX or 0. SR-AUD-08: 0 while muted. SR-AUD-09: 0 until
  // audio enable is set. SR-AUD-05: 0 when no tone is playing.
  always_ff @(posedge aud_clk) begin
    if (aud_rst) sample <= '0;
    else if (!tone_active_q || !enable_q || mute_q) sample <= '0;
    else sample <= level_high_q ? TONE_AMPLITUDE_MAX : -TONE_AMPLITUDE_MAX;
  end

endmodule
