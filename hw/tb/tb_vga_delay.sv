// Testbench for vga_delay (#25): DEPTH 4, 1 and 0, and reset (SR-REN-06).
module tb_vga_delay;

  // verilog_lint: waive-start parameter-name-style
  localparam int CYCLES = 2000;
  localparam int RESET_AT = 1000;
  localparam int MAX_REPORTS = 20;
  localparam int HIST = 8;
  // verilog_lint: waive-stop parameter-name-style

  logic clk = 1'b0;
  logic rst = 1'b1;
  always #1 clk = ~clk;

  // Three instances: the depth used by vga_controller (4, SR-REN-06), the shortest real delay
  logic [7:0] in4, out4;
  logic [2:0] in1, out1;
  logic [4:0] in0, out0;

  vga_delay #(
      .WIDTH(8),
      .DEPTH(4)
  ) dut4 (
      .clk  (clk),
      .rst  (rst),
      .d_in (in4),
      .d_out(out4)
  );
  vga_delay #(
      .WIDTH(3),
      .DEPTH(1)
  ) dut1 (
      .clk  (clk),
      .rst  (rst),
      .d_in (in1),
      .d_out(out1)
  );
  vga_delay #(
      .WIDTH(5),
      .DEPTH(0)
  ) dut0 (
      .clk  (clk),
      .rst  (rst),
      .d_in (in0),
      .d_out(out0)
  );

  int errors = 0;
  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask

  logic [7:0] hist4[HIST];
  logic [2:0] hist1[HIST];
  int edges_since_reset = 0;
  int checks = 0;

  initial begin
    in4 = '0;
    in1 = '0;
    in0 = '0;
    for (int k = 0; k < HIST; k++) begin
      hist4[k] = '0;
      hist1[k] = '0;
    end
    repeat (3) @(negedge clk);
    rst = 1'b0;
    for (int cycle = 0; cycle < CYCLES; cycle++) begin
      in4 = 8'($urandom);
      in1 = 3'($urandom);
      in0 = 5'($urandom);
      #0;
      if (out0 !== in0) fail($sformatf("DEPTH 0: d_out %0h, expected d_in %0h", out0, in0));
      if (edges_since_reset >= 4) begin
        if (out4 !== hist4[3]) fail($sformatf("DEPTH 4: d_out %0h, expected %0h", out4, hist4[3]));
      end else if (out4 !== 8'h00) begin
        fail($sformatf("DEPTH 4: d_out %0h within 4 clocks of reset, expected 0", out4));
      end
      if (edges_since_reset >= 1) begin
        if (out1 !== hist1[0]) fail($sformatf("DEPTH 1: d_out %0h, expected %0h", out1, hist1[0]));
      end else if (out1 !== 3'h0) begin
        fail($sformatf("DEPTH 1: d_out %0h right after reset, expected 0", out1));
      end

      @(posedge clk);
      for (int k = HIST - 1; k > 0; k--) begin
        hist4[k] = hist4[k-1];
        hist1[k] = hist1[k-1];
      end
      hist4[0] = rst ? 8'h00 : in4;
      hist1[0] = rst ? 3'h0 : in1;
      edges_since_reset = rst ? 0 : edges_since_reset + 1;

      checks++;
      @(negedge clk);

      if (cycle == RESET_AT) begin
        rst = 1'b1;
        @(posedge clk);
        @(negedge clk);
        if (out4 !== 8'h00 || out1 !== 3'h0) fail("reset did not clear the delay lines");
        for (int k = 0; k < HIST; k++) begin
          hist4[k] = '0;
          hist1[k] = '0;
        end
        edges_since_reset = 0;
        rst = 1'b0;
      end
    end

    $display("checked %0d cycles, %0d errors", checks, errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
