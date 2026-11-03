// I2S transmitter to the WM8731 codec in slave mode (#41). SR-AUD-02, SR-AUD-03, CON-12.
// Runs on the codec master clock (AUD_XCK, 12.288 MHz, SR-AUD-01) and divides it down:
//   AUD_DACLRCK = clk / 256 = 48 kHz  (256 = AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ, SR-AUD-02)
//   AUD_BCLK    = clk / 8   = 1.536 MHz, 32 bit clocks per frame (2 x AUD_SAMPLE_BITS)
// I2S format (§3.1.2.4, SR-AUD-03): LRCK low = left, high = right; each 16-bit two's-complement
// word is sent MSB first, starting one bit clock after the LRCK edge; DACDAT and LRCK change on
// the falling edge of BCLK, so the codec samples them on the rising edge. Left and right carry
// the same sample. All outputs come straight from flip-flops, so they cannot glitch.
// The PLL that makes clk and drives AUD_XCK is outside this module (#41, top level #33).
module i2s_tx (
    input logic clk,  // codec master clock, 12.288 MHz (SR-AUD-01)
    input logic rst,  // synchronous, active high, in the clk domain

    // Sample source (Tone Generator, #42), in this same clk domain
    input  logic signed [15:0] sample,      // AUD_SAMPLE_BITS, two's complement (SR-AUD-03)
    output logic               sample_load, // 1-clock pulse: sample was latched on the last edge

    output logic aud_bclk,     // to AUD_BCLK
    output logic aud_daclrck,  // to AUD_DACLRCK
    output logic aud_dacdat    // to AUD_DACDAT
);

  // verilog_lint: waive-start parameter-name-style
  localparam int AUD_MCLK_HZ = 12_288_000;  // App. A, SR-AUD-01
  localparam int AUD_SAMPLE_RATE_HZ = 48_000;  // App. A, SR-AUD-02
  localparam int AUD_SAMPLE_BITS = 16;  // App. A, SR-AUD-03

  localparam int MCLK_PER_FRAME = AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ;  // 256
  localparam int BCLK_PER_FRAME = 2 * AUD_SAMPLE_BITS;  // 32: left word + right word
  localparam int MCLK_PER_BCLK = MCLK_PER_FRAME / BCLK_PER_FRAME;  // 8
  localparam int DIV_W = $clog2(MCLK_PER_FRAME);  // 8: div_q wraps once per frame
  localparam int SLOT_LSB = $clog2(MCLK_PER_BCLK);  // 3: div_q[7:3] is the bit slot
  // verilog_lint: waive-stop parameter-name-style

  // div_q counts clocks in the frame. Its top bit is LRCK (first half left), and bit SLOT_LSB-1
  // is BCLK: low for the first half of each slot, high for the second.
  logic [DIV_W-1:0] div_q;
  logic [2*AUD_SAMPLE_BITS-1:0] shift_q;  // {left, right}, MSB first
  logic lsb_q;  // right-channel LSB, sent in slot 0 of the next frame (I2S one-bit delay)
  logic dacdat_q;
  logic sample_load_q;

  wire frame_end = (div_q == DIV_W'(MCLK_PER_FRAME - 1));  // next clock starts slot 0
  wire slot_end = (div_q[SLOT_LSB-1:0] == SLOT_LSB'(MCLK_PER_BCLK - 1));  // BCLK falls next

  always_ff @(posedge clk) begin
    if (rst) begin
      div_q         <= '0;
      shift_q       <= '0;
      lsb_q         <= 1'b0;
      dacdat_q      <= 1'b0;
      sample_load_q <= 1'b0;
    end else begin
      div_q         <= div_q + 1'b1;
      sample_load_q <= frame_end;
      if (frame_end) begin
        // Slot 0: finish the last frame's right word, take the new sample for both channels.
        dacdat_q <= lsb_q;
        shift_q  <= {sample, sample};  // SR-AUD-03: L = R
        lsb_q    <= sample[0];
      end else if (slot_end) begin
        // Slots 1-31: left bits 15..0, then right bits 15..1.
        dacdat_q <= shift_q[2*AUD_SAMPLE_BITS-1];
        shift_q  <= shift_q << 1;
      end
    end
  end

  assign aud_bclk    = div_q[SLOT_LSB-1];
  assign aud_daclrck = div_q[DIV_W-1];
  assign aud_dacdat  = dacdat_q;
  assign sample_load = sample_load_q;

endmodule
