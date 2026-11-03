// Testbench for vga_timing (#25, #26, #27): SR-VGA-02, SR-VGA-03, SR-VGA-04 pulse widths,
// SR-VGA-08, pixel position, in both VIDEO_MODE values (SR-VGA-10, SR-VGA-11). One DUT per mode
// runs side by side on the same clock; the checks are the same, only the counts differ.
module tb_vga_timing;

  // verilog_lint: waive-start parameter-name-style
  localparam int NUM_MODES = 2;
  // XGA (default, SR-VGA-11): SR-VGA-02, SR-VGA-03. Bring-up: SR-VGA-10, §3.1.2.1 640x480 table.
  localparam longint XGA_H_TOTAL = 1344;  // SR-VGA-02
  localparam longint XGA_V_TOTAL = 806;  // SR-VGA-03
  localparam longint FRAMES_TO_CHECK = 2;
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic clk = 1'b0;
  logic rst = 1'b1;

  always #1 clk = ~clk;

  int errors = 0;
  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask

  for (genvar m = 0; m < NUM_MODES; m++) begin : g_mode
    // verilog_lint: waive-start parameter-name-style
    localparam bit BRINGUP = (m == 1);
    localparam string VIDEO_MODE = BRINGUP ? "VGA_640x480" : "XGA_1024x768";
    localparam longint H_ACTIVE = BRINGUP ? 640 : 1024;  // SR-VGA-10 / SR-VGA-02
    localparam longint H_FRONT_PORCH = BRINGUP ? 16 : 24;  // SR-VGA-10 / SR-VGA-02
    localparam longint H_SYNC = BRINGUP ? 96 : 136;  // SR-VGA-10 / SR-VGA-02, SR-VGA-04
    localparam longint H_BACK_PORCH = BRINGUP ? 48 : 160;  // SR-VGA-10 / SR-VGA-02
    localparam longint H_TOTAL = BRINGUP ? 800 : 1344;  // SR-VGA-10 / SR-VGA-02
    localparam longint V_ACTIVE = BRINGUP ? 480 : 768;  // SR-VGA-10 / SR-VGA-03
    localparam longint V_FRONT_PORCH = BRINGUP ? 10 : 3;  // SR-VGA-10 / SR-VGA-03
    localparam longint V_SYNC = BRINGUP ? 2 : 6;  // SR-VGA-10 / SR-VGA-03, SR-VGA-04
    localparam longint V_BACK_PORCH = BRINGUP ? 33 : 29;  // SR-VGA-10 / SR-VGA-03
    localparam longint V_TOTAL = BRINGUP ? 525 : 806;  // SR-VGA-10 / SR-VGA-03
    // verilog_lint: waive-stop parameter-name-style

    logic [9:0] pixel_x, pixel_y;
    logic active, h_sync_pulse, v_sync_pulse, vblank_start;

    vga_timing #(
        .VIDEO_MODE(VIDEO_MODE)
    ) dut (
        .clk         (clk),
        .rst         (rst),
        .pixel_x     (pixel_x),
        .pixel_y     (pixel_y),
        .active      (active),
        .h_sync_pulse(h_sync_pulse),
        .v_sync_pulse(v_sync_pulse),
        .vblank_start(vblank_start)
    );

    task automatic fail_m(input string what);
      fail({VIDEO_MODE, ": ", what});
    endtask
    task automatic expect_eq_m(input string what, input longint got, input longint want);
      if (got != want) fail_m($sformatf("%s: got %0d, expected %0d", what, got, want));
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
    longint vblanks_since_vs = 0;

    always @(negedge clk) begin
      if (!rst) begin
        clk_count++;
        if ($isunknown({pixel_x, pixel_y, active, h_sync_pulse, v_sync_pulse, vblank_start})) begin
          fail_m("X or Z on outputs");
        end
        if (active && (h_sync_pulse || v_sync_pulse)) fail_m("active during a sync pulse");

        if (first) begin
          if (!(active && pixel_x == 0 && pixel_y == 0)) begin
            fail_m($sformatf(
                   "after reset: active=%0b x=%0d y=%0d, expected pixel (0,0)",
                   active,
                   pixel_x,
                   pixel_y
                   ));
          end
          row   = 0;
          first = 1'b0;
        end

        // Horizontal (SR-VGA-02, SR-VGA-04).
        if (!prev_hs && h_sync_pulse) begin
          if (last_hs_rise >= 0) begin
            expect_eq_m("clocks per line (sync start to sync start)", clk_count - last_hs_rise,
                        H_TOTAL);
            lines_checked++;
          end
          if (last_act_fall > last_hs_rise) begin
            expect_eq_m("front porch (active end to sync start)", clk_count - last_act_fall,
                        H_FRONT_PORCH);
          end
          last_hs_rise = clk_count;
        end
        if (prev_hs && !h_sync_pulse) begin
          if (last_hs_rise >= 0) expect_eq_m("HSYNC width", clk_count - last_hs_rise, H_SYNC);
          last_hs_fall = clk_count;
        end

        // Active region (SR-VGA-02, SR-VGA-03).
        if (!prev_act && active) begin
          if (last_hs_fall >= 0) begin
            expect_eq_m("back porch (sync end to active start)", clk_count - last_hs_fall,
                        H_BACK_PORCH);
          end
          if (last_vs_fall >= 0 && last_vs_fall > last_act_rise) begin
            expect_eq_m("vertical back porch (vsync end to first active line)",
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
            expect_eq_m("active pixels per line", clk_count - last_act_rise, H_ACTIVE);
          end
          last_act_fall = clk_count;
        end

        // SR-VGA-08: vblank_start is high exactly on the first clock after the last active pixel
        // of the last line (767 in XGA, 479 in bring-up), and on no other clock.
        if (vblank_start !== (prev_act && !active && row == V_ACTIVE - 1)) begin
          fail_m($sformatf(
                 "vblank_start=%0b at row %0d, %0d clocks after the active start",
                 vblank_start,
                 row,
                 clk_count - last_act_rise
                 ));
        end
        if (vblank_start) vblanks_since_vs++;

        // Vertical (SR-VGA-03, SR-VGA-04).
        if (!prev_vs && v_sync_pulse) begin
          if (last_vs_rise >= 0) begin
            expect_eq_m("clocks per frame", clk_count - last_vs_rise, V_TOTAL * H_TOTAL);
            expect_eq_m("active lines per frame", lines_since_vs, V_ACTIVE);
            frames_checked++;
          end
          if (last_act_fall >= 0) begin
            expect_eq_m("vertical front porch (last active end to vsync start)",
                        clk_count - last_act_fall, (H_TOTAL - H_ACTIVE) + V_FRONT_PORCH * H_TOTAL);
          end
          if (row >= 0) expect_eq_m("last active row", row, V_ACTIVE - 1);
          expect_eq_m("vblank_start pulses per frame", vblanks_since_vs, 1);
          vblanks_since_vs = 0;
          lines_since_vs = 0;
          last_vs_rise = clk_count;
        end
        if (prev_vs && !v_sync_pulse) begin
          if (last_vs_rise >= 0) begin
            expect_eq_m("VSYNC width", clk_count - last_vs_rise, V_SYNC * H_TOTAL);
          end
          last_vs_fall = clk_count;
        end

        if (active && row >= 0) begin
          if (longint'(pixel_x) != clk_count - last_act_rise || longint'(pixel_y) != row) begin
            fail_m($sformatf(
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
  end

  // Reset is released on a rising edge, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge clk) if (release_rst) rst <= 1'b0;

  // Long enough for FRAMES_TO_CHECK whole frames of the longer (XGA) mode; bring-up sees more.
  initial begin
    repeat (5) @(posedge clk);
    release_rst = 1'b1;
    repeat (int'((FRAMES_TO_CHECK + 1) * XGA_V_TOTAL * XGA_H_TOTAL + 4 * XGA_H_TOTAL)) begin
      @(posedge clk);
    end
    if (g_mode[0].frames_checked < FRAMES_TO_CHECK || g_mode[1].frames_checked < FRAMES_TO_CHECK)
    begin
      fail($sformatf(
           "only %0d (XGA) and %0d (bring-up) whole frames seen, expected %0d each",
           g_mode[0].frames_checked,
           g_mode[1].frames_checked,
           FRAMES_TO_CHECK
           ));
    end
    $display("XGA_1024x768: checked %0d frames, %0d lines, %0d clocks", g_mode[0].frames_checked,
             g_mode[0].lines_checked, g_mode[0].clk_count);
    $display("VGA_640x480: checked %0d frames, %0d lines, %0d clocks", g_mode[1].frames_checked,
             g_mode[1].lines_checked, g_mode[1].clk_count);
    $display("%0d errors", errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
