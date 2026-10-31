// Testbench for vga_controller (#25): SR-VGA-02 to SR-VGA-07, SR-REN-06 alignment.
module tb_vga_controller;

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
  localparam int PIXEL_LATENCY = 4;  // SR-REN-06: Tile Renderer N = 4
  localparam longint FRAMES_TO_CHECK = 2;
  localparam int MAX_REPORTS = 20;
  // verilog_lint: waive-stop parameter-name-style

  logic        clk = 1'b0;
  logic        rst = 1'b1;
  logic [ 9:0] pixel_x;
  logic [ 9:0] pixel_y;
  logic        pixel_active;
  logic [23:0] rgb_in;
  logic [7:0] vga_r, vga_g, vga_b;
  logic vga_hs, vga_vs, vga_blank_n, vga_sync_n;

  vga_controller #(
      .PIXEL_LATENCY(PIXEL_LATENCY)
  ) dut (
      .clk         (clk),
      .rst         (rst),
      .pixel_x     (pixel_x),
      .pixel_y     (pixel_y),
      .pixel_active(pixel_active),
      .rgb_in      (rgb_in),
      .VGA_R       (vga_r),
      .VGA_G       (vga_g),
      .VGA_B       (vga_b),
      .VGA_HS      (vga_hs),
      .VGA_VS      (vga_vs),
      .VGA_BLANK_N (vga_blank_n),
      .VGA_SYNC_N  (vga_sync_n)
  );

  // (SR-VGA-01) is measured on the board (issue #46).
  always #1 clk = ~clk;

  function automatic logic [23:0] colour_of(input longint x, input longint y);
    logic [9:0] xb, yb;
    xb = 10'(x);
    yb = 10'(y);
    return {xb[7:0], yb[7:0], xb[9:8], yb[9:8], 4'b0101};
  endfunction

  // Tile Renderer model (SR-REN-06): the colour for pixel_x/pixel_y appears on rgb_in
  logic [23:0] renderer_q[PIXEL_LATENCY];
  always_ff @(posedge clk) begin
    renderer_q[0] <= pixel_active ? colour_of(longint'(pixel_x), longint'(pixel_y)) : 24'hFF_FFFF;
    for (int i = 1; i < PIXEL_LATENCY; i++) renderer_q[i] <= renderer_q[i-1];
  end
  assign rgb_in = renderer_q[PIXEL_LATENCY-1];

  int errors = 0;

  task automatic fail(input string what);
    if (errors < MAX_REPORTS) $display("FAIL: t=%0t %s", $time, what);
    errors++;
  endtask

  task automatic expect_eq(input string what, input longint got, input longint want);
    if (got != want) fail($sformatf("%s: got %0d, expected %0d", what, got, want));
  endtask

  longint clk_count = 0;
  longint last_hs_fall = -1, last_hs_rise = -1;
  longint last_vs_fall = -1, last_vs_rise = -1;
  longint last_blank_rise = -1, last_blank_fall = -1;
  longint lines_since_vs_fall = 0;
  longint active_row = -1;
  longint frames_checked = 0;
  longint lines_checked = 0;
  logic prev_hs = 1'b1, prev_vs = 1'b1, prev_blank_n = 1'b0;
  bit checking = 1'b0;

  always @(negedge clk) begin
    if (rst) begin
      // Reset state: no sync, blanked, black (SR-VGA-04, SR-VGA-05).
      if (vga_hs !== 1'b1 || vga_vs !== 1'b1 || vga_blank_n !== 1'b0) begin
        fail("outputs during reset are not HS=1, VS=1, BLANK_N=0");
      end
    end else begin
      clk_count++;

      // SR-VGA-06: SYNC_N is 0 at all times.
      if (vga_sync_n !== 1'b0) fail("VGA_SYNC_N is not 0");
      if ($isunknown({vga_r, vga_g, vga_b, vga_hs, vga_vs, vga_blank_n})) fail("X or Z on outputs");
      // SR-VGA-05: blanked during the sync pulses.
      if (vga_blank_n && (!vga_hs || !vga_vs)) fail("BLANK_N high during a sync pulse");

      // --- Horizontal (SR-VGA-02, SR-VGA-04) -------------------------------------------------
      if (prev_hs && !vga_hs) begin
        if (last_hs_fall >= 0) begin
          expect_eq("clocks per line (HS fall to HS fall)", clk_count - last_hs_fall, H_TOTAL);
          lines_checked++;
        end
        if (last_blank_fall > last_hs_fall && last_hs_fall >= 0) begin
          expect_eq("front porch (BLANK_N fall to HS fall)", clk_count - last_blank_fall,
                    H_FRONT_PORCH);
        end
        last_hs_fall = clk_count;
      end
      if (!prev_hs && vga_hs) begin
        if (last_hs_fall >= 0) begin
          expect_eq("HS low width", clk_count - last_hs_fall, H_SYNC);
        end
        last_hs_rise = clk_count;
      end

      // --- Blanking (SR-VGA-02, SR-VGA-05) ---------------------------------------------------
      if (!prev_blank_n && vga_blank_n) begin
        if (last_hs_rise >= 0) begin
          expect_eq("back porch (HS rise to BLANK_N rise)", clk_count - last_hs_rise, H_BACK_PORCH);
        end
        if (last_vs_rise >= 0 && last_vs_rise > last_blank_rise) begin
          // First active line after the vertical sync pulse (SR-VGA-03).
          expect_eq("vertical back porch (VS rise to first BLANK_N rise)", clk_count - last_vs_rise,
                    V_BACK_PORCH * H_TOTAL);
          active_row = 0;
        end else if (active_row >= 0) begin
          active_row++;
        end
        lines_since_vs_fall++;
        last_blank_rise = clk_count;
      end
      if (prev_blank_n && !vga_blank_n) begin
        if (last_blank_rise >= 0) begin
          expect_eq("active pixels per line (BLANK_N high)", clk_count - last_blank_rise, H_ACTIVE);
        end
        last_blank_fall = clk_count;
      end

      // --- Vertical (SR-VGA-03, SR-VGA-04) ---------------------------------------------------
      if (prev_vs && !vga_vs) begin
        if (last_vs_fall >= 0) begin
          expect_eq("clocks per frame (VS fall to VS fall)", clk_count - last_vs_fall,
                    V_TOTAL * H_TOTAL);
          expect_eq("active lines per frame", lines_since_vs_fall, V_ACTIVE);
          if (active_row >= 0) expect_eq("last active row index", active_row, V_ACTIVE - 1);
          frames_checked++;
        end
        if (last_blank_fall >= 0) begin
          expect_eq("vertical front porch (last BLANK_N fall to VS fall)",
                    clk_count - last_blank_fall, (H_TOTAL - H_ACTIVE) + V_FRONT_PORCH * H_TOTAL);
        end
        lines_since_vs_fall = 0;
        last_vs_fall = clk_count;
      end
      if (!prev_vs && vga_vs) begin
        if (last_vs_fall >= 0) begin
          expect_eq("VS low width", clk_count - last_vs_fall, V_SYNC * H_TOTAL);
        end
        last_vs_rise = clk_count;
      end

      // --- Colour (SR-VGA-07, SR-REN-06) -----------------------------------------------------
      if (vga_blank_n) begin
        if (active_row >= 0) begin
          longint col;
          col = clk_count - last_blank_rise;
          if ({vga_r, vga_g, vga_b} !== colour_of(col, active_row)) begin
            fail($sformatf(
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
        fail($sformatf("colour %06h while blanked", {vga_r, vga_g, vga_b}));
      end

      checking = 1'b1;
      prev_hs = vga_hs;
      prev_vs = vga_vs;
      prev_blank_n = vga_blank_n;
    end
  end

  // Reset is released on a rising edge, never on the falling edge the checker samples on.
  logic release_rst = 1'b0;
  always @(posedge clk) if (release_rst) rst <= 1'b0;

  initial begin
    repeat (10) @(posedge clk);
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
