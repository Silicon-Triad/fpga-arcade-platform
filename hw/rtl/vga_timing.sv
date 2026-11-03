// VGA timing generator (#25, #26, #27). SR-VGA-02, SR-VGA-03, SR-VGA-08, SR-VGA-10, SR-VGA-11,
// CON-02.
//   VIDEO_MODE = "XGA_1024x768" (default, SR-VGA-11): 1024x768 @ 60 Hz, 1344 x 806 frame.
//   VIDEO_MODE = "VGA_640x480"  (bring-up, SR-VGA-10): 640x480 @ 60 Hz, 800 x 525 frame.
// Both modes use the same line and frame order (active, front porch, sync, back porch) and the
// same active-low sync polarity on the pins (SR-VGA-04), so only the counts change.
module vga_timing #(
    // SR-VGA-11, App. A VIDEO_MODE: synthesis-time parameter, XGA by default. Same name and values
    // as tilemap_crop.
    // verilog_lint: waive parameter-name-style (App. A name, R-4.2.3)
    parameter string VIDEO_MODE = "XGA_1024x768"
) (
    // Pixel clock: 65.000 MHz in XGA (SR-VGA-01), 25.0-25.2 MHz in bring-up (SR-VGA-10). The
    // PLL that makes it is part of the top level, not this module.
    input logic clk,
    input logic rst,  // synchronous, active high

    output logic [9:0] pixel_x,       // valid while active
    output logic [9:0] pixel_y,       // valid while active
    output logic       active,        // inside the active region (1024 x 768 or 640 x 480)
    output logic       h_sync_pulse,  // active high (SR-VGA-04)
    output logic       v_sync_pulse,  // active high (SR-VGA-04)
    output logic       vblank_start   // 1 clock after the last active pixel of a frame (SR-VGA-08)
);

  // Only the two values of App. A VIDEO_MODE are legal.
  // generate/endgenerate: Quartus Prime Lite needs the keywords (#33).
  generate
    if (VIDEO_MODE != "XGA_1024x768" && VIDEO_MODE != "VGA_640x480") begin : g_bad_mode
      $error("vga_timing: VIDEO_MODE must be \"XGA_1024x768\" or \"VGA_640x480\"");
    end
  endgenerate

  // verilog_lint: waive-start parameter-name-style
  localparam bit BRINGUP = (VIDEO_MODE == "VGA_640x480");
  // XGA values: SR-VGA-02, SR-VGA-03. Bring-up values: SR-VGA-10, §3.1.2.1 "Timing - bring-up
  // mode 640x480 @ 60 Hz" (App. A BRINGUP_H_TOTAL / V_TOTAL = 800 / 525).
  localparam logic [10:0] H_ACTIVE = BRINGUP ? 11'd640 : 11'd1024;
  localparam logic [10:0] H_FRONT_PORCH = BRINGUP ? 11'd16 : 11'd24;
  localparam logic [10:0] H_SYNC = BRINGUP ? 11'd96 : 11'd136;  // SR-VGA-04 pulse width
  localparam logic [10:0] H_BACK_PORCH = BRINGUP ? 11'd48 : 11'd160;
  localparam logic [9:0] V_ACTIVE = BRINGUP ? 10'd480 : 10'd768;
  localparam logic [9:0] V_FRONT_PORCH = BRINGUP ? 10'd10 : 10'd3;
  localparam logic [9:0] V_SYNC = BRINGUP ? 10'd2 : 10'd6;  // SR-VGA-04 pulse width
  localparam logic [9:0] V_BACK_PORCH = BRINGUP ? 10'd33 : 10'd29;

  localparam logic [10:0] H_SYNC_START = H_ACTIVE + H_FRONT_PORCH;
  localparam logic [10:0] H_SYNC_END = H_SYNC_START + H_SYNC;
  localparam logic [10:0] H_TOTAL = H_SYNC_END + H_BACK_PORCH;  // 1344 (SR-VGA-02), 800 (SR-VGA-10)
  localparam logic [9:0] V_SYNC_START = V_ACTIVE + V_FRONT_PORCH;
  localparam logic [9:0] V_SYNC_END = V_SYNC_START + V_SYNC;
  localparam logic [9:0] V_TOTAL = V_SYNC_END + V_BACK_PORCH;  // 806 (SR-VGA-03), 525 (SR-VGA-10)
  // verilog_lint: waive-stop parameter-name-style

  logic [10:0] h_count_q;  // App. A h_count
  logic [ 9:0] v_count_q;  // App. A v_count

  always_ff @(posedge clk) begin
    if (rst) begin
      h_count_q <= '0;
      v_count_q <= '0;
    end else if (h_count_q == H_TOTAL - 11'd1) begin
      h_count_q <= '0;
      v_count_q <= (v_count_q == V_TOTAL - 10'd1) ? '0 : v_count_q + 10'd1;
    end else begin
      h_count_q <= h_count_q + 11'd1;
    end
  end

  assign active = (h_count_q < H_ACTIVE) && (v_count_q < V_ACTIVE);
  assign h_sync_pulse = (h_count_q >= H_SYNC_START) && (h_count_q < H_SYNC_END);
  assign v_sync_pulse = (v_count_q >= V_SYNC_START) && (v_count_q < V_SYNC_END);

  // SR-VGA-08: registered, so it is high exactly on the clock with h = H_ACTIVE, v = V_ACTIVE - 1.
  logic vblank_start_q;
  always_ff @(posedge clk) begin
    if (rst) vblank_start_q <= 1'b0;
    else vblank_start_q <= (h_count_q == H_ACTIVE - 11'd1) && (v_count_q == V_ACTIVE - 10'd1);
  end
  assign vblank_start = vblank_start_q;

  assign pixel_x = h_count_q[9:0];
  assign pixel_y = v_count_q;

endmodule
