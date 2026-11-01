// VGA Controller, XGA 1024x768 @ 60 Hz (#25). SR-VGA-02 to SR-VGA-07, CON-02.
// Timing goes to the Tile Renderer in cycle 0; the renderer delays sync and blank with its colour
// (SR-REN-06), and this module registers them onto the pins.
module vga_controller (
    input logic clk,  // 65.000 MHz pixel clock (SR-VGA-01)
    input logic rst,  // synchronous, active high

    // To the Tile Renderer, cycle 0 (active high)
    output logic [9:0] pixel_x,       // valid only while pixel_active
    output logic [9:0] pixel_y,       // valid only while pixel_active
    output logic       pixel_active,
    output logic       h_sync,
    output logic       v_sync,

    // From the Tile Renderer, aligned with each other (SR-REN-06)
    input logic [23:0] rgb_in,     // {R, G, B} (SR-VGA-07)
    input logic        h_sync_in,  // active high
    input logic        v_sync_in,  // active high
    input logic        blank_in,   // active high: 1 outside the active region

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

  vga_timing u_timing (
      .clk         (clk),
      .rst         (rst),
      .pixel_x     (pixel_x),
      .pixel_y     (pixel_y),
      .active      (pixel_active),
      .h_sync_pulse(h_sync),
      .v_sync_pulse(v_sync)
  );

  always_ff @(posedge clk) begin
    if (rst) begin
      VGA_HS <= 1'b1;
      VGA_VS <= 1'b1;
      VGA_BLANK_N <= 1'b0;
      {VGA_R, VGA_G, VGA_B} <= 24'h00_0000;
    end else begin
      VGA_HS <= ~h_sync_in;  // SR-VGA-04
      VGA_VS <= ~v_sync_in;  // SR-VGA-04
      VGA_BLANK_N <= ~blank_in;  // SR-VGA-05
      {VGA_R, VGA_G, VGA_B} <= blank_in ? 24'h00_0000 : rgb_in;  // SR-VGA-07
    end
  end

  assign VGA_SYNC_N = 1'b0;  // SR-VGA-06

endmodule
