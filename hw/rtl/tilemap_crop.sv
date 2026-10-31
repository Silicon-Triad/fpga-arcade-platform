// Tilemap crop: maps a VGA pixel position to a tilemap cell and decides if it is shown.
// Implements SR-REN-08 (640x480 bring-up mode shows only tilemap columns 0-79, rows 0-59).
// Refs #31.
//
// The tilemap is always 128 x 96 cells (SR-REN-01, GRID_COLS x GRID_ROWS). This block does not
// resize or rescale it: it only limits which cells are displayed (SR-REN-08 rationale:
// "cropping rather than rescaling"). Every cell, displayed or not, is still written and held
// by the tilemap banks (#29); nothing here touches the write side.
//
//   VIDEO_MODE = "XGA_1024x768" (default, SR-VGA-11): all 128 x 96 cells can be shown.
//   VIDEO_MODE = "VGA_640x480"  (bring-up, SR-VGA-10): only cells with col < 80 and row < 60
//                                (App. A BRINGUP_VISIBLE_TILES) are shown.
//
// Purely combinational. The Tile Renderer top (#29) instantiates it in front of the tilemap
// read: cell_addr is the byte offset inside the front bank (row * GRID_COLS + col, TBL-Tilemap)
// and blank_out replaces blank_in on its way into the pipeline (#30), so a cropped cell is
// drawn as blanking (black) instead of its tile.
module tilemap_crop #(
    // SR-VGA-11, App. A VIDEO_MODE: same parameter name and values as the VGA Controller (#27).
    // verilog_lint: waive parameter-name-style (App. A name, R-4.2.3)
    parameter string VIDEO_MODE = "XGA_1024x768"
) (
    // Active-pixel position from the VGA Controller (H_ACTIVE 1024 fits in 11 bits, SR-VGA-02;
    // V_ACTIVE 768 fits in 10 bits, SR-VGA-03).
    input logic [10:0] vga_x,
    input logic [ 9:0] vga_y,
    input logic        blank_in, // active-high, 1 = blanking (as tile_renderer_pipeline)

    // Byte offset of the cell inside one tilemap bank (TBL-Tilemap, SR-REN-01).
    output logic [$clog2(arcade_regs_pkg::TILEMAP_BANK_SIZE)-1:0] cell_addr,
    output logic cell_visible,  // 1 = this cell is inside the displayed area (SR-REN-08)
    output logic blank_out  // blank_in, or 1 when the cell is cropped (SR-REN-08)
);

  // verilog_lint: waive-start parameter-name-style (R-4.2.3: UPPER_SNAKE_CASE, as App. A)
  localparam int TILE_SIZE = 8;  // SR-REN-04, App. A TILE_SIZE: 8 x 8 px
  localparam int SUB_W = $clog2(TILE_SIZE);  // 3 bits of position inside a tile
  localparam int COL_W = $clog2(arcade_regs_pkg::GRID_COLS);  // 7
  localparam int ROW_W = $clog2(arcade_regs_pkg::GRID_ROWS);  // 7 (96 rows)
  localparam int BRINGUP_VISIBLE_COLS = 80;  // SR-REN-08, App. A BRINGUP_VISIBLE_TILES: 0-79
  localparam int BRINGUP_VISIBLE_ROWS = 60;  // SR-REN-08, App. A BRINGUP_VISIBLE_TILES: 0-59
  localparam bit BRINGUP = (VIDEO_MODE == "VGA_640x480");
  localparam int VISIBLE_COLS = BRINGUP ? BRINGUP_VISIBLE_COLS : arcade_regs_pkg::GRID_COLS;
  localparam int VISIBLE_ROWS = BRINGUP ? BRINGUP_VISIBLE_ROWS : arcade_regs_pkg::GRID_ROWS;
  // verilog_lint: waive-stop parameter-name-style

  // Only the two values of App. A VIDEO_MODE are legal.
  if (VIDEO_MODE != "XGA_1024x768" && VIDEO_MODE != "VGA_640x480") begin : g_bad_mode
    $error("tilemap_crop: VIDEO_MODE must be \"XGA_1024x768\" or \"VGA_640x480\"");
  end

  // Tile column and row: pixel position / TILE_SIZE (CON-05). An x or y past the 128 x 96 grid
  // (only possible with a bad VGA timing) has non-zero high bits and is treated as cropped.
  logic [10-SUB_W:0] col_full;
  logic [ 9-SUB_W:0] row_full;
  logic [ COL_W-1:0] col;
  logic [ ROW_W-1:0] row;
  assign col_full = vga_x[10:SUB_W];
  assign row_full = vga_y[9:SUB_W];
  assign col = col_full[COL_W-1:0];
  assign row = row_full[ROW_W-1:0];

  // SR-REN-08: in bring-up mode only columns 0-79 and rows 0-59 are displayed.
  assign cell_visible = (int'(col_full) < VISIBLE_COLS) && (int'(row_full) < VISIBLE_ROWS);
  assign blank_out = blank_in || !cell_visible;

  // TBL-Tilemap: cell (col, row) is byte row * GRID_COLS + col of the bank. GRID_COLS = 128,
  // so the offset is {row, col}.
  assign cell_addr = {row, col};

  // Elaboration check: {row, col} spans exactly one tilemap bank.
  if (arcade_regs_pkg::GRID_COLS != (1 << COL_W) ||
      arcade_regs_pkg::GRID_COLS * arcade_regs_pkg::GRID_ROWS
      != arcade_regs_pkg::TILEMAP_BANK_SIZE || ROW_W + COL_W != $bits(
          cell_addr
      )) begin : g_bad_size
    $error("tilemap_crop: tilemap geometry does not match arcade_regs_pkg");
  end

  // Low bits (pixel inside the tile) are used by the pipeline (#30), not here.
  wire _unused = &{1'b0, vga_x[SUB_W-1:0], vga_y[SUB_W-1:0]};
endmodule
