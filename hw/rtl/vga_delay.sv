// Delay line: d_out = d_in from DEPTH clocks earlier; DEPTH = 0 is a wire (#25, SR-REN-06).
module vga_delay #(
    parameter int WIDTH = 1,
    parameter int DEPTH = 1
) (
    input  logic             clk,
    input  logic             rst,   // synchronous, active high; clears every stage
    input  logic [WIDTH-1:0] d_in,
    output logic [WIDTH-1:0] d_out
);

  if (DEPTH == 0) begin : g_wire
    assign d_out = d_in;
  end else begin : g_shift
    logic [WIDTH-1:0] stage_q[DEPTH];
    always_ff @(posedge clk) begin
      if (rst) begin
        for (int i = 0; i < DEPTH; i++) stage_q[i] <= '0;
      end else begin
        stage_q[0] <= d_in;
        for (int i = 1; i < DEPTH; i++) stage_q[i] <= stage_q[i-1];
      end
    end
    assign d_out = stage_q[DEPTH-1];
  end

endmodule
