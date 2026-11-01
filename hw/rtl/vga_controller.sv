// VGA Controller, XGA 1024x768 @ 60 Hz (#25, #26). SR-VGA-02 to SR-VGA-09, CON-02.
// Timing goes to the Tile Renderer in cycle 0; the renderer delays sync and blank with its colour
// (SR-REN-06), and this module registers them onto the pins.
module vga_controller (
    input logic clk,  // 65.000 MHz pixel clock (SR-VGA-01)
    input logic rst,  // synchronous, active high

    // Control Register display-enable bit (§3.1.4), from bridge_regs on this same clock (A2)
    input logic display_enable,

    // To the Tile Renderer, cycle 0 (active high)
    output logic [9:0] pixel_x,       // valid only while pixel_active
    output logic [9:0] pixel_y,       // valid only while pixel_active
    output logic       pixel_active,
    output logic       h_sync,
    output logic       v_sync,
    output logic       vblank_start,  // 1-clock pulse to the tilemap bank swap (SR-VGA-08)

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
      .v_sync_pulse(v_sync),
      .vblank_start(vblank_start)
  );

  // Same clock as bridge_regs (A2), no synchronizer needed (R-4.2.5). One register to break up the
  // path. Resets to 0, the register map reset value (black, SR-VGA-09).
  logic display_enable_q;
  always_ff @(posedge clk) begin
    if (rst) display_enable_q <= 1'b0;
    else display_enable_q <= display_enable;
  end

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
      // SR-VGA-07; SR-VGA-09: black until display enable is set, sync unchanged
      {VGA_R, VGA_G, VGA_B} <= (blank_in || !display_enable_q) ? 24'h00_0000 : rgb_in;
    end
  end

  assign VGA_SYNC_N = 1'b0;  // SR-VGA-06

endmodule
