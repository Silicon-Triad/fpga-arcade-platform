// VGA Controller, XGA 1024x768 @ 60 Hz (#25). SR-VGA-02 to SR-VGA-07, CON-02.
module vga_controller #(
    parameter int PIXEL_LATENCY = 4  // Tile Renderer latency N (SR-REN-06)
) (
    input logic clk,  // 65.000 MHz pixel clock (SR-VGA-01)
    input logic rst,  // synchronous, active high

    // To the Tile Renderer (SR-REN-06)
    output logic [ 9:0] pixel_x,
    output logic [ 9:0] pixel_y,
    output logic        pixel_active,
    // From the Tile Renderer, PIXEL_LATENCY clocks later: {R, G, B} (SR-VGA-07)
    input  logic [23:0] rgb_in,

    // verilog_lint: waive-start port-name-style
    output logic [7:0] VGA_R,
    output logic [7:0] VGA_G,
    output logic [7:0] VGA_B,
    output logic       VGA_HS,
    output logic       VGA_VS,
    output logic       VGA_BLANK_N,
    output logic       VGA_SYNC_N
    // verilog_lint: waive-stop port-name-style
);

  logic active, h_sync_pulse, v_sync_pulse;
  logic active_d, h_sync_d, v_sync_d;

  vga_timing u_timing (
      .clk         (clk),
      .rst         (rst),
      .pixel_x     (pixel_x),
      .pixel_y     (pixel_y),
      .active      (active),
      .h_sync_pulse(h_sync_pulse),
      .v_sync_pulse(v_sync_pulse)
  );

  assign pixel_active = active;

  // Align sync and blanking with rgb_in (SR-REN-06)
  vga_delay #(
      .WIDTH(3),
      .DEPTH(PIXEL_LATENCY)
  ) u_delay (
      .clk  (clk),
      .rst  (rst),
      .d_in ({active, h_sync_pulse, v_sync_pulse}),
      .d_out({active_d, h_sync_d, v_sync_d})
  );

  always_ff @(posedge clk) begin
    if (rst) begin
      VGA_HS <= 1'b1;
      VGA_VS <= 1'b1;
      VGA_BLANK_N <= 1'b0;
      {VGA_R, VGA_G, VGA_B} <= 24'h00_0000;
    end else begin
      VGA_HS <= ~h_sync_d;  // SR-VGA-04
      VGA_VS <= ~v_sync_d;  // SR-VGA-04
      VGA_BLANK_N <= active_d;  // SR-VGA-05
      {VGA_R, VGA_G, VGA_B} <= active_d ? rgb_in : 24'h00_0000;  // SR-VGA-07
    end
  end

  assign VGA_SYNC_N = 1'b0;  // SR-VGA-06

endmodule
