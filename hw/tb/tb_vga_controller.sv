// Testbench for vga_controller (#25, #26, #27): SR-VGA-02 to SR-VGA-11, SR-REN-06 alignment.
// Both VIDEO_MODE values (SR-VGA-10, SR-VGA-11) are checked side by side, one DUT per mode on the
// same clock. In each, display enable is 0 for the first whole frame (black, correct sync) and 1
// after it.
module tb_vga_controller;

  // verilog_lint: waive-start parameter-name-style
  localparam int NUM_MODES = 2;
  localparam longint XGA_H_TOTAL = 1344;  // SR-VGA-02
  localparam longint XGA_V_TOTAL = 806;  // SR-VGA-03
  localparam int RENDER_LATENCY = 4;  // SR-REN-06: Tile Renderer N = 4
  localparam longint FRAMES_TO_CHECK = 2;
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic clk = 1'b0;
  logic rst = 1'b1;

  // Checks count clocks, so the period does not matter; the pixel clocks (SR-VGA-01, SR-VGA-10)
  // are measured on the board in #46.
  always #1 clk = ~clk;

  function automatic logic [23:0] colour_of(input longint x, input longint y);
    logic [9:0] xb, yb;
    xb = 10'(x);
    yb = 10'(y);
    return {xb[7:0], yb[7:0], xb[9:8], yb[9:8], 4'b0101};
  endfunction

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

    logic       display_enable = 1'b0;
    logic       vblank_start;
    logic [9:0] pixel_x;
    logic [9:0] pixel_y;
    logic       pixel_active;
    logic h_sync, v_sync;
    logic [23:0] rgb_in;
    logic h_sync_in, v_sync_in, blank_in;
    logic [7:0] vga_r, vga_g, vga_b;
    logic vga_hs, vga_vs, vga_blank_n, vga_sync_n;

    vga_controller #(
        .VIDEO_MODE(VIDEO_MODE)
    ) dut (
        .clk           (clk),
        .rst           (rst),
        .display_enable(display_enable),
        .pixel_x       (pixel_x),
        .pixel_y       (pixel_y),
        .pixel_active  (pixel_active),
        .h_sync        (h_sync),
        .v_sync        (v_sync),
        .vblank_start  (vblank_start),
        .rgb_in        (rgb_in),
        .h_sync_in     (h_sync_in),
        .v_sync_in     (v_sync_in),
        .blank_in      (blank_in),
        .VGA_R         (vga_r),
        .VGA_G         (vga_g),
        .VGA_B         (vga_b),
        .VGA_HS        (vga_hs),
        .VGA_VS        (vga_vs),
        .VGA_BLANK_N   (vga_blank_n),
        .VGA_SYNC_N    (vga_sync_n)
    );

    task automatic fail_m(input string what);
      fail({VIDEO_MODE, ": ", what});
    endtask

    task automatic expect_eq_m(input string what, input longint got, input longint want);
      if (got != want) fail_m($sformatf("%s: got %0d, expected %0d", what, got, want));
    endtask

    // Tile Renderer model (SR-REN-06): colour, sync and blank come back RENDER_LATENCY clocks
    // later. Blanked pixels get white, so the test shows the controller blanks them itself.
    logic [26:0] renderer_q[RENDER_LATENCY];  // {rgb, h_sync, v_sync, blank}
    always_ff @(posedge clk) begin
      if (rst) begin
        for (int i = 0; i < RENDER_LATENCY; i++) renderer_q[i] <= {24'h00_0000, 3'b001};
      end else begin
        renderer_q[0] <= {
          pixel_active ? colour_of(longint'(pixel_x), longint'(pixel_y)) : 24'hFF_FFFF,
          h_sync,
          v_sync,
          ~pixel_active
        };
        for (int i = 1; i < RENDER_LATENCY; i++) renderer_q[i] <= renderer_q[i-1];
      end
    end
    assign {rgb_in, h_sync_in, v_sync_in, blank_in} = renderer_q[RENDER_LATENCY-1];

    longint clk_count = 0;
    longint last_hs_fall = -1, last_hs_rise = -1;
    longint last_vs_fall = -1, last_vs_rise = -1;
    longint last_blank_rise = -1, last_blank_fall = -1;
    longint lines_since_vs_fall = 0;
    longint active_row = -1;
    longint frames_checked = 0;
    longint lines_checked = 0;
    logic prev_hs = 1'b1, prev_vs = 1'b1, prev_blank_n = 1'b0;

    longint vs_falls = 0, frames_dark = 0, frames_lit = 0, vblanks_since_vs = 0;
    longint hs_pulses_since_vs = 0;
    bit colour_on = 1'b0;  // display enable as the checker expects it for the current frame
    logic prev_pixel_active = 1'b0;
    logic [9:0] prev_pixel_y = '0;

    always @(negedge clk) begin
      if (rst) begin
        // Reset state: no sync, blanked, black (SR-VGA-04, SR-VGA-05).
        if (vga_hs !== 1'b1 || vga_vs !== 1'b1 || vga_blank_n !== 1'b0) begin
          fail_m("outputs during reset are not HS=1, VS=1, BLANK_N=0");
        end
      end else begin
        clk_count++;

        // SR-VGA-06: SYNC_N is 0 at all times.
        if (vga_sync_n !== 1'b0) fail_m("VGA_SYNC_N is not 0");
        if ($isunknown({vga_r, vga_g, vga_b, vga_hs, vga_vs, vga_blank_n, vblank_start})) begin
          fail_m("X or Z on outputs");
        end
        // SR-VGA-08: one clock after the last active pixel of the last line (cycle-0 outputs).
        if (vblank_start !==
            (prev_pixel_active && !pixel_active && prev_pixel_y == 10'(V_ACTIVE - 1))) begin
          fail_m($sformatf("vblank_start=%0b, pixel_y=%0d", vblank_start, prev_pixel_y));
        end
        if (vblank_start) vblanks_since_vs++;
        prev_pixel_active = pixel_active;
        prev_pixel_y = pixel_y;
        // SR-VGA-05: blanked during the sync pulses.
        if (vga_blank_n && (!vga_hs || !vga_vs)) fail_m("BLANK_N high during a sync pulse");

        // --- Horizontal (SR-VGA-02, SR-VGA-04) -------------------------------------------------
        if (prev_hs && !vga_hs) begin
          hs_pulses_since_vs++;
          if (last_hs_fall >= 0) begin
            expect_eq_m("clocks per line (HS fall to HS fall)", clk_count - last_hs_fall, H_TOTAL);
            lines_checked++;
          end
          if (last_blank_fall > last_hs_fall && last_hs_fall >= 0) begin
            expect_eq_m("front porch (BLANK_N fall to HS fall)", clk_count - last_blank_fall,
                        H_FRONT_PORCH);
          end
          last_hs_fall = clk_count;
        end
        if (!prev_hs && vga_hs) begin
          if (last_hs_fall >= 0) begin
            expect_eq_m("HS low width", clk_count - last_hs_fall, H_SYNC);
          end
          last_hs_rise = clk_count;
        end

        // --- Blanking (SR-VGA-02, SR-VGA-05) ---------------------------------------------------
        if (!prev_blank_n && vga_blank_n) begin
          if (last_hs_rise >= 0) begin
            expect_eq_m("back porch (HS rise to BLANK_N rise)", clk_count - last_hs_rise,
                        H_BACK_PORCH);
          end
          if (last_vs_rise >= 0 && last_vs_rise > last_blank_rise) begin
            // First active line after the vertical sync pulse (SR-VGA-03).
            expect_eq_m("vertical back porch (VS rise to first BLANK_N rise)",
                        clk_count - last_vs_rise, V_BACK_PORCH * H_TOTAL);
            active_row = 0;
          end else if (active_row >= 0) begin
            active_row++;
          end
          lines_since_vs_fall++;
          last_blank_rise = clk_count;
        end
        if (prev_blank_n && !vga_blank_n) begin
          if (last_blank_rise >= 0) begin
            expect_eq_m("active pixels per line (BLANK_N high)", clk_count - last_blank_rise,
                        H_ACTIVE);
          end
          last_blank_fall = clk_count;
        end

        // --- Vertical (SR-VGA-03, SR-VGA-04) ---------------------------------------------------
        if (prev_vs && !vga_vs) begin
          if (last_vs_fall >= 0) begin
            expect_eq_m("clocks per frame (VS fall to VS fall)", clk_count - last_vs_fall,
                        V_TOTAL * H_TOTAL);
            expect_eq_m("active lines per frame", lines_since_vs_fall, V_ACTIVE);
            if (active_row >= 0) expect_eq_m("last active row index", active_row, V_ACTIVE - 1);
            expect_eq_m("vblank_start pulses per frame", vblanks_since_vs, 1);
            // SR-VGA-04, SR-VGA-09: one HS pulse per line in every frame, also while dark.
            expect_eq_m("HS pulses per frame", hs_pulses_since_vs, V_TOTAL);
            frames_checked++;
            if (colour_on) frames_lit++;
            else frames_dark++;
          end
          vblanks_since_vs   = 0;
          hs_pulses_since_vs = 0;
          vs_falls++;
          // SR-VGA-09: after one whole dark frame, the HAL sets display enable (in vertical sync,
          // so it has settled long before the next active line).
          if (vs_falls == 2) begin
            display_enable = 1'b1;
            colour_on = 1'b1;
          end
          if (last_blank_fall >= 0) begin
            expect_eq_m("vertical front porch (last BLANK_N fall to VS fall)",
                        clk_count - last_blank_fall,
                        (H_TOTAL - H_ACTIVE) + V_FRONT_PORCH * H_TOTAL);
          end
          lines_since_vs_fall = 0;
          last_vs_fall = clk_count;
        end
        if (!prev_vs && vga_vs) begin
          if (last_vs_fall >= 0) begin
            expect_eq_m("VS low width", clk_count - last_vs_fall, V_SYNC * H_TOTAL);
          end
          last_vs_rise = clk_count;
        end

        // --- Colour (SR-VGA-07, SR-REN-06) -----------------------------------------------------
        if (vga_blank_n && !colour_on) begin
          // SR-VGA-09: display enable 0, so every active pixel is black.
          if ({vga_r, vga_g, vga_b} !== 24'h00_0000) begin
            fail_m($sformatf("colour %06h while display enable is 0", {vga_r, vga_g, vga_b}));
          end
        end else if (vga_blank_n) begin
          if (active_row >= 0) begin
            longint col;
            col = clk_count - last_blank_rise;
            if ({vga_r, vga_g, vga_b} !== colour_of(col, active_row)) begin
              fail_m($sformatf(
                     "colour at (%0d,%0d): got %06h, expected %06h",
                     col,
                     active_row,
                     {
                       vga_r, vga_g, vga_b
                     },
                     colour_of(
                         col, active_row
                     )
                     ));
            end
          end
        end else if ({vga_r, vga_g, vga_b} !== 24'h00_0000) begin
          fail_m($sformatf("colour %06h while blanked", {vga_r, vga_g, vga_b}));
        end

        prev_hs = vga_hs;
        prev_vs = vga_vs;
        prev_blank_n = vga_blank_n;
      end
    end
  end

  // Reset is released on a rising edge, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge clk) if (release_rst) rst <= 1'b0;

  // Long enough for FRAMES_TO_CHECK whole frames of the longer (XGA) mode; bring-up sees more.
  initial begin
    repeat (10) @(posedge clk);
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
    if (g_mode[0].frames_dark < 1 || g_mode[0].frames_lit < 1 ||
        g_mode[1].frames_dark < 1 || g_mode[1].frames_lit < 1) begin
      fail("need a whole dark and a whole lit frame in each mode");
    end
    $display("XGA_1024x768: %0d frames (%0d dark, %0d lit), %0d lines, %0d clocks",
             g_mode[0].frames_checked, g_mode[0].frames_dark, g_mode[0].frames_lit,
             g_mode[0].lines_checked, g_mode[0].clk_count);
    $display("VGA_640x480: %0d frames (%0d dark, %0d lit), %0d lines, %0d clocks",
             g_mode[1].frames_checked, g_mode[1].frames_dark, g_mode[1].frames_lit,
             g_mode[1].lines_checked, g_mode[1].clk_count);
    $display("%0d errors", errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule
