// Testbench for vga_timing (#25): SR-VGA-02, SR-VGA-03, SR-VGA-04 pulse widths, pixel position.
module tb_vga_timing;

  // verilog_lint: waive-start parameter-name-style
  localparam longint H_ACTIVE = 1024;  // SR-VGA-02
  localparam longint H_FRONT_PORCH = 24;  // SR-VGA-02
  localparam longint H_SYNC = 136;  // SR-VGA-02, SR-VGA-04
  localparam longint H_BACK_PORCH = 160;  // SR-VGA-02
  localparam longint H_TOTAL = 1344;  // SR-VGA-02
  localparam longint V_ACTIVE = 768;  // SR-VGA-03
  localparam longint V_FRONT_PORCH = 3;  // SR-VGA-03
  localparam longint V_SYNC = 6;  // SR-VGA-03, SR-VGA-04
  localparam longint V_BACK_PORCH = 29;  // SR-VGA-03
  localparam longint V_TOTAL = 806;  // SR-VGA-03
  localparam longint FRAMES_TO_CHECK = 2;
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic clk = 1'b0;
  logic rst = 1'b1;
  logic [9:0] pixel_x, pixel_y;
  logic active, h_sync_pulse, v_sync_pulse;

  vga_timing dut (
      .clk         (clk),
      .rst         (rst),
      .pixel_x     (pixel_x),
      .pixel_y     (pixel_y),
      .active      (active),
      .h_sync_pulse(h_sync_pulse),
      .v_sync_pulse(v_sync_pulse)
  );

  always #1 clk = ~clk;

  int errors = 0;
  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask
  task automatic expect_eq(input string what, input longint got, input longint want);
    if (got != want) fail($sformatf("%s: got %0d, expected %0d", what, got, want));
  endtask

  longint clk_count = 0;
  longint last_hs_rise = -1, last_hs_fall = -1;
  longint last_vs_rise = -1, last_vs_fall = -1;
  longint last_act_rise = -1, last_act_fall = -1;
  longint lines_since_vs = 0;
  longint row = -1;
  longint frames_checked = 0, lines_checked = 0;
  logic prev_hs = 1'b0, prev_vs = 1'b0, prev_act = 1'b0;
  bit first = 1'b1;

  always @(negedge clk) begin
    if (!rst) begin
      clk_count++;
      if ($isunknown({pixel_x, pixel_y, active, h_sync_pulse, v_sync_pulse})) begin
        fail("X or Z on outputs");
      end
      if (active && (h_sync_pulse || v_sync_pulse)) fail("active during a sync pulse");

      if (first) begin
        if (!(active && pixel_x == 0 && pixel_y == 0)) begin
          fail($sformatf(
               "after reset: active=%0b x=%0d y=%0d, expected pixel (0,0)", active, pixel_x, pixel_y
               ));
        end
        row   = 0;
        first = 1'b0;
      end

      // Horizontal (SR-VGA-02, SR-VGA-04).
      if (!prev_hs && h_sync_pulse) begin
        if (last_hs_rise >= 0) begin
          expect_eq("clocks per line (sync start to sync start)", clk_count - last_hs_rise,
                    H_TOTAL);
          lines_checked++;
        end
        if (last_act_fall > last_hs_rise) begin
          expect_eq("front porch (active end to sync start)", clk_count - last_act_fall,
                    H_FRONT_PORCH);
        end
        last_hs_rise = clk_count;
      end
      if (prev_hs && !h_sync_pulse) begin
        if (last_hs_rise >= 0) expect_eq("HSYNC width", clk_count - last_hs_rise, H_SYNC);
        last_hs_fall = clk_count;
      end

      // Active region (SR-VGA-02, SR-VGA-03).
      if (!prev_act && active) begin
        if (last_hs_fall >= 0) begin
          expect_eq("back porch (sync end to active start)", clk_count - last_hs_fall,
                    H_BACK_PORCH);
        end
        if (last_vs_fall >= 0 && last_vs_fall > last_act_rise) begin
          expect_eq("vertical back porch (vsync end to first active line)",
                    clk_count - last_vs_fall, V_BACK_PORCH * H_TOTAL);
          row = 0;
        end else if (clk_count > 1) begin
          row++;
        end
        lines_since_vs++;
        last_act_rise = clk_count;
      end
      if (prev_act && !active) begin
        if (last_act_rise >= 0) begin
          expect_eq("active pixels per line", clk_count - last_act_rise, H_ACTIVE);
        end
        last_act_fall = clk_count;
      end

      // Vertical (SR-VGA-03, SR-VGA-04).
      if (!prev_vs && v_sync_pulse) begin
        if (last_vs_rise >= 0) begin
          expect_eq("clocks per frame", clk_count - last_vs_rise, V_TOTAL * H_TOTAL);
          expect_eq("active lines per frame", lines_since_vs, V_ACTIVE);
          frames_checked++;
        end
        if (last_act_fall >= 0) begin
          expect_eq("vertical front porch (last active end to vsync start)",
                    clk_count - last_act_fall, (H_TOTAL - H_ACTIVE) + V_FRONT_PORCH * H_TOTAL);
        end
        if (row >= 0) expect_eq("last active row", row, V_ACTIVE - 1);
        lines_since_vs = 0;
        last_vs_rise   = clk_count;
      end
      if (prev_vs && !v_sync_pulse) begin
        if (last_vs_rise >= 0) begin
          expect_eq("VSYNC width", clk_count - last_vs_rise, V_SYNC * H_TOTAL);
        end
        last_vs_fall = clk_count;
      end

      if (active && row >= 0) begin
        if (longint'(pixel_x) != clk_count - last_act_rise || longint'(pixel_y) != row) begin
          fail($sformatf(
               "position: got (%0d,%0d), expected (%0d,%0d)",
               pixel_x,
               pixel_y,
               clk_count - last_act_rise,
               row
               ));
        end
      end

      prev_hs  = h_sync_pulse;
      prev_vs  = v_sync_pulse;
      prev_act = active;
    end
  end

  // Reset is released on a rising edge, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge clk) if (release_rst) rst <= 1'b0;

  initial begin
    repeat (5) @(posedge clk);
    release_rst = 1'b1;
    repeat (int'((FRAMES_TO_CHECK + 1) * V_TOTAL * H_TOTAL + 4 * H_TOTAL)) @(posedge clk);
    if (frames_checked < FRAMES_TO_CHECK) begin
      fail($sformatf("only %0d whole frames seen, expected %0d", frames_checked, FRAMES_TO_CHECK));
    end
    $display("checked %0d frames, %0d lines, %0d clocks, %0d errors", frames_checked,
             lines_checked, clk_count, errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
