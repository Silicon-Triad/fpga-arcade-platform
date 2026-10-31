// Self-checking testbench for tilemap_crop (R-4.2.10). Refs #31.
// Verifies SR-REN-08 with a frame dump:
//   - VGA_640x480: every active pixel of a 640x480 frame is swept; the set of displayed cells
//     is exactly columns 0-79 and rows 0-59 (80 x 60 = 4800 cells), each cell is hit by
//     exactly 8 x 8 pixels, and cell_addr = row * 128 + col (TBL-Tilemap);
//   - VGA_640x480 driven with a full 1024x768 sweep: no cell outside cols 0-79 / rows 0-59 is
//     ever displayed (blank_out = 1 there), so the rest of the bank is held but not shown;
//   - XGA_1024x768 (default): all 128 x 96 cells are displayed;
//   - blank_in = 1 always gives blank_out = 1.
// The VGA_640x480 frame dump is written to tilemap_crop_frame.txt in the simulation directory
// ('#' = displayed cell, '.' = not displayed), one line per tilemap row.
module tb_tilemap_crop;

  // verilog_lint: waive-start parameter-name-style (R-4.2.3: UPPER_SNAKE_CASE)
  localparam int GRID_COLS = arcade_regs_pkg::GRID_COLS;  // SR-FW-12: 128
  localparam int GRID_ROWS = arcade_regs_pkg::GRID_ROWS;  // SR-FW-12: 96
  localparam int TILE_SIZE = 8;  // SR-REN-04, App. A TILE_SIZE
  localparam int BRINGUP_W = 640;  // SR-VGA-10: 640x480 active area
  localparam int BRINGUP_H = 480;  // SR-VGA-10
  localparam int XGA_W = 1024;  // App. A H_ACTIVE, SR-VGA-02
  localparam int XGA_H = 768;  // App. A V_ACTIVE, SR-VGA-03
  localparam int BRINGUP_COLS = 80;  // SR-REN-08, App. A BRINGUP_VISIBLE_TILES
  localparam int BRINGUP_ROWS = 60;  // SR-REN-08, App. A BRINGUP_VISIBLE_TILES
  // verilog_lint: waive-stop parameter-name-style

  logic [10:0] vga_x;
  logic [ 9:0] vga_y;
  logic        blank_in;

  logic [13:0] addr_vga, addr_xga;
  logic vis_vga, vis_xga, blank_vga, blank_xga;

  tilemap_crop #(
      .VIDEO_MODE("VGA_640x480")
  ) dut_vga (
      .vga_x,
      .vga_y,
      .blank_in,
      .cell_addr(addr_vga),
      .cell_visible(vis_vga),
      .blank_out(blank_vga)
  );

  tilemap_crop dut_xga (  // default VIDEO_MODE: XGA_1024x768 (SR-VGA-11)
      .vga_x,
      .vga_y,
      .blank_in,
      .cell_addr(addr_xga),
      .cell_visible(vis_xga),
      .blank_out(blank_xga)
  );

  int errors = 0;
  int hits[GRID_ROWS][GRID_COLS];

  task automatic fail(input string msg);
    errors++;
    if (errors <= 20) $display("FAIL: %s", msg);
  endtask

  task automatic clear_hits();
    for (int r = 0; r < GRID_ROWS; r++) for (int c = 0; c < GRID_COLS; c++) hits[r][c] = 0;
  endtask

  initial begin
    int fd, shown, col, row;
    string line;

    // --- 1. Frame dump: VGA_640x480 over a 640x480 active frame (SR-REN-08) ---
    clear_hits();
    blank_in = 1'b0;
    for (int y = 0; y < BRINGUP_H; y++) begin
      for (int x = 0; x < BRINGUP_W; x++) begin
        vga_x = 11'(x);
        vga_y = 10'(y);
        #1;
        col = x / TILE_SIZE;
        row = y / TILE_SIZE;
        if (vis_vga && !blank_vga) begin
          hits[row][col]++;
          if (int'(addr_vga) != row * GRID_COLS + col)
            fail($sformatf("SR-REN-08 x=%0d y=%0d: cell_addr %0d, expected %0d", x, y, addr_vga,
                           row * GRID_COLS + col));
        end else begin
          fail($sformatf("SR-REN-08 x=%0d y=%0d: pixel in 640x480 frame not displayed", x, y));
        end
      end
    end
    shown = 0;
    fd = $fopen("tilemap_crop_frame.txt", "w");
    for (int r = 0; r < GRID_ROWS; r++) begin
      line = "";
      for (int c = 0; c < GRID_COLS; c++) begin
        bit expect_shown;
        expect_shown = (c < BRINGUP_COLS) && (r < BRINGUP_ROWS);
        line = {line, (hits[r][c] != 0) ? "#" : "."};
        if (hits[r][c] != 0) shown++;
        if (expect_shown && hits[r][c] != TILE_SIZE * TILE_SIZE)
          fail($sformatf("SR-REN-08 cell (%0d,%0d): %0d pixels, expected %0d", c, r,
                         hits[r][c], TILE_SIZE * TILE_SIZE));
        if (!expect_shown && hits[r][c] != 0)
          fail($sformatf("SR-REN-08 cell (%0d,%0d) displayed outside 80x60", c, r));
      end
      if (fd != 0) $fdisplay(fd, "%s", line);
    end
    if (fd != 0) $fclose(fd);
    $display("frame dump VGA_640x480: %0d cells displayed (expected %0d), cols 0-%0d, rows 0-%0d",
             shown, BRINGUP_COLS * BRINGUP_ROWS, BRINGUP_COLS - 1, BRINGUP_ROWS - 1);
    if (shown != BRINGUP_COLS * BRINGUP_ROWS) fail("SR-REN-08: wrong number of displayed cells");

    // --- 2. VGA_640x480 driven over the whole 1024x768 grid: crop holds (SR-REN-08) ---
    // --- 3. XGA_1024x768 default: whole 128x96 grid displayed (SR-VGA-11) ---
    for (int y = 0; y < XGA_H; y++) begin
      for (int x = 0; x < XGA_W; x++) begin
        bit in_crop;
        vga_x = 11'(x);
        vga_y = 10'(y);
        #1;
        col = x / TILE_SIZE;
        row = y / TILE_SIZE;
        in_crop = (col < BRINGUP_COLS) && (row < BRINGUP_ROWS);
        if (vis_vga != in_crop || blank_vga != !in_crop)
          fail($sformatf("SR-REN-08 x=%0d y=%0d: visible=%0b blank=%0b, crop expects %0b", x,
                         y, vis_vga, blank_vga, in_crop));
        if (int'(addr_vga) != row * GRID_COLS + col)
          fail($sformatf("TBL-Tilemap x=%0d y=%0d: cell_addr %0d", x, y, addr_vga));
        if (!vis_xga || blank_xga || int'(addr_xga) != row * GRID_COLS + col)
          fail($sformatf("XGA x=%0d y=%0d: cell not displayed or wrong address", x, y));
      end
    end
    $display("full-grid sweep: VGA_640x480 crops outside 80x60, XGA_1024x768 shows 128x96");

    // --- 4. Blanking passes through in both modes ---
    blank_in = 1'b1;
    for (int i = 0; i < 4; i++) begin
      vga_x = (i[0]) ? 11'd0 : 11'd1016;
      vga_y = (i[1]) ? 10'd0 : 10'd760;
      #1;
      if (!blank_vga || !blank_xga) fail("blank_in=1 did not give blank_out=1");
    end

    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end
endmodule
