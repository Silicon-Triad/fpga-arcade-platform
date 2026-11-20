// I2S transmitter to the WM8731 DAC (#41). SR-AUD-02, SR-AUD-03, CON-12.
// The codec is in slave mode (R7 = I2S, 16 bits, LRP = 0, BCLKINV = 0, set by the HAL in #43):
// this module makes the bit clock and the left/right clock by dividing aud_clk, so both are
// synchronous to the codec master clock AUD_XCK (driven from the same PLL output in the top level).
//
// One frame = 256 aud_clk = 64 BCLK (32 per channel). 12.288 MHz / 256 = 48 kHz (SR-AUD-02).
// In each 32-bit channel slot the MSB is sent one BCLK after the DACLRCK edge (I2S), then the
// other 15 bits, then zeros. The same sample goes to left and right (SR-AUD-03).
module i2s_tx (
    input logic aud_clk,  // 12.288 MHz codec master clock (SR-AUD-01, App. A AUD_MCLK_HZ)
    input logic aud_rst,  // synchronous, active high

    input  logic signed [15:0] sample,      // two's complement (SR-AUD-03)
    output logic               sample_tick, // 1 clock per frame: `sample` is taken at its end

    output logic aud_bclk,     // to AUD_BCLK: aud_clk / 4 = 3.072 MHz, 64 per frame
    output logic aud_daclrck,  // to AUD_DACLRCK: 48 kHz, low = left (LRP = 0)
    output logic aud_dacdat    // to AUD_DACDAT: changes on BCLK falling edges
);

  // verilog_lint: waive-start parameter-name-style
  localparam int SAMPLE_BITS = 16;  // SR-AUD-03, App. A AUD_SAMPLE_BITS
  localparam int CLKS_PER_FRAME = 256;  // AUD_MCLK_HZ / AUD_SAMPLE_RATE_HZ (SR-AUD-01, -02)
  localparam int CLKS_PER_BCLK = 4;  // design choice: 64 BCLK per frame (issue #41)
  localparam int SLOT_BITS = CLKS_PER_FRAME / CLKS_PER_BCLK / 2;  // 32 BCLK per channel
  // verilog_lint: waive-stop parameter-name-style

  // frame_q[7] = channel (0 left, 1 right), frame_q[6:2] = bit within the slot, frame_q[1] = BCLK.
  logic [7:0] frame_q;
  logic [SAMPLE_BITS-1:0] word_q;  // sample being sent in this frame
  logic [4:0] slot_bit;
  logic [3:0] bit_from_lsb;  // SAMPLE_BITS - slot_bit: 15 (MSB) down to 0 (LSB) in slot bits 1-16
  logic data_bit;

  assign slot_bit = frame_q[6:2];
  assign bit_from_lsb = 4'(5'(SAMPLE_BITS) - slot_bit);
  // I2S: slot bit 0 is the one-BCLK delay, bits 1-16 are MSB to LSB, the rest are 0.
  assign data_bit = (slot_bit >= 5'd1 && slot_bit <= 5'(SAMPLE_BITS)) ? word_q[bit_from_lsb] : 1'b0;

  always_ff @(posedge aud_clk) begin
    if (aud_rst) begin
      frame_q <= '0;
      word_q <= '0;
      sample_tick <= 1'b0;
      aud_bclk <= 1'b0;
      aud_daclrck <= 1'b0;
      aud_dacdat <= 1'b0;
    end else begin
      frame_q <= frame_q + 8'd1;
      // sample_tick is high on the last counter clock of the frame (2 clocks before the next
      // DACLRCK fall on the pin); `sample` is taken at its end and sent in the next frame.
      sample_tick <= (frame_q == 8'(CLKS_PER_FRAME - 2));
      if (sample_tick) word_q <= sample;
      // Outputs are registered from frame_q, so all three move on the same clock edge.
      // BCLK is low for the first two clocks of each bit, so data and DACLRCK change on its
      // falling edge and are stable at its rising edge, where the codec samples (BCLKINV = 0).
      aud_bclk <= frame_q[1];
      aud_daclrck <= frame_q[7];
      aud_dacdat <= data_bit;
    end
  end

  // SLOT_BITS documents the slot length; the counter widths above follow from it.
  // Elaboration check only: Quartus Prime Lite does not support $error here, so Quartus
  // synthesis (which defines ALTERA_RESERVED_QIS) skips it; Verilator and QuestaSim run it.
`ifndef ALTERA_RESERVED_QIS
  generate
    if (SLOT_BITS != 32) begin : g_bad_slot
      $error("i2s_tx: frame_q bit fields assume 32 BCLK per channel slot");
    end
  endgenerate
`endif

endmodule
