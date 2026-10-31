// VGA timing generator, XGA 1024x768 @ 60 Hz (#25). SR-VGA-02, SR-VGA-03, CON-02.
module vga_timing (
    input logic clk,  // 65.000 MHz pixel clock (SR-VGA-01)
    input logic rst,  // synchronous, active high

    output logic [9:0] pixel_x,       // valid while active
    output logic [9:0] pixel_y,       // valid while active
    output logic       active,        // inside 1024 x 768
    output logic       h_sync_pulse,  // active high (SR-VGA-04)
    output logic       v_sync_pulse   // active high (SR-VGA-04)
);

  // verilog_lint: waive-start parameter-name-style
  localparam logic [10:0] H_ACTIVE = 11'd1024;  // SR-VGA-02
  localparam logic [10:0] H_FRONT_PORCH = 11'd24;  // SR-VGA-02
  localparam logic [10:0] H_SYNC = 11'd136;  // SR-VGA-02, SR-VGA-04
  localparam logic [10:0] H_BACK_PORCH = 11'd160;  // SR-VGA-02
  localparam logic [9:0] V_ACTIVE = 10'd768;  // SR-VGA-03
  localparam logic [9:0] V_FRONT_PORCH = 10'd3;  // SR-VGA-03
  localparam logic [9:0] V_SYNC = 10'd6;  // SR-VGA-03, SR-VGA-04
  localparam logic [9:0] V_BACK_PORCH = 10'd29;  // SR-VGA-03

  localparam logic [10:0] H_SYNC_START = H_ACTIVE + H_FRONT_PORCH;
  localparam logic [10:0] H_SYNC_END = H_SYNC_START + H_SYNC;
  localparam logic [10:0] H_TOTAL = H_SYNC_END + H_BACK_PORCH;  // 1344 (SR-VGA-02)
  localparam logic [9:0] V_SYNC_START = V_ACTIVE + V_FRONT_PORCH;
  localparam logic [9:0] V_SYNC_END = V_SYNC_START + V_SYNC;
  localparam logic [9:0] V_TOTAL = V_SYNC_END + V_BACK_PORCH;  // 806 (SR-VGA-03)
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

  assign pixel_x = h_count_q[9:0];
  assign pixel_y = v_count_q;

endmodule
