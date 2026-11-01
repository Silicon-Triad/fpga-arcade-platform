// Tile Renderer pipeline: tile pattern RAM, palette and the 4-stage pixel pipeline.
// Implements SR-REN-04, SR-REN-05, SR-REN-06, TBL-TilePattern, TBL-Palette, CON-05, CON-06.
// Refs #30.
//
// Stages (SR-REN-06, N = 4 cycles from tilemap read to colour output):
//   stage 1  tile_index arrives from the tilemap read; pixel x/y within the tile are registered
//   stage 2  pattern RAM read: one 32-bit row (8 px x 4 bpp) of tile tile_index
//   stage 3  palette RAM read: the 4-bit index of pixel x selects one 0x00RRGGBB entry
//   stage 4  colour output: R, G, B unpacked and registered; black while blanking
// HSYNC, VSYNC and blank go through a 4-deep shift register, so they stay aligned with the
// colour (SR-REN-06). A new pixel enters every cycle, so all four stages are busy.
//
// Interface with the VGA Controller (#25) and the tilemap read (#29):
//   - vga_x, vga_y: active-pixel coordinates; only bits [2:0] (position inside the tile) are
//     used here. The tile-grid bits select the tilemap cell upstream.
//   - blank_in: active-high, 1 = blanking (the opposite polarity of the board's VGA_BLANK_N).
//   - hsync_in, vsync_in: passed through unchanged (no polarity change), delayed by 4 clocks.
//   - tile_index: presented one clock after the vga_x/vga_y it belongs to (stage 1).
//   - Module name: hw/rtl/tile_renderer.sv (#29, tilemap banks and swap) is the top-level Tile
//     Renderer and instantiates this pipeline.
module tile_renderer_pipeline (
    input logic clk,
    input logic rst,  // synchronous reset (R-4.2.6)

    // VGA Controller inputs (cycle 0). Only the pixel position inside the tile is used.
    input logic [9:0] vga_x,
    input logic [9:0] vga_y,
    input logic       hsync_in,
    input logic       vsync_in,
    input logic       blank_in,

    // Stage 1 input (cycle 1): tile index from the tilemap read (SR-REN-04: up to 256 patterns)
    input logic [$clog2(arcade_regs_pkg::TILE_PATTERN_COUNT)-1:0] tile_index,

    // Pattern RAM write port (TBL-TilePattern, §3.1.4 offset 0x6000): one 32-bit row per word
    input logic pat_wr_en,
    // Word address = byte offset / 4: 8192 bytes (SR-REN-04) as 32-bit words is 2048 words.
    input logic [$clog2(arcade_regs_pkg::TILE_PATTERN_RAM_SIZE * 8 / 32)-1:0] pat_wr_addr,
    input logic [31:0] pat_wr_data,
    input logic [3:0] pat_wr_strb,  // byte enables

    // Palette RAM write port (TBL-Palette, §3.1.4 offset 0x8000)
    input logic                                             pal_wr_en,
    input logic [$clog2(arcade_regs_pkg::PALETTE_SIZE)-1:0] pal_wr_addr,
    input logic [                                     31:0] pal_wr_data,  // 0x00RRGGBB (CON-06)

    // VGA output (cycle 4)
    output logic [7:0] vga_r,
    output logic [7:0] vga_g,
    output logic [7:0] vga_b,
    output logic       hsync_out,
    output logic       vsync_out,
    output logic       blank_out
);

  // ---------------------------------------------------------------------------
  // Sizes, all derived from the frozen package (R-4.2.7, R-4.2.8)
  // ---------------------------------------------------------------------------
  // verilog_lint: waive-start parameter-name-style (R-4.2.3: UPPER_SNAKE_CASE, as the package)
  localparam int TILE_SIZE = 8;  // SR-REN-04, CON-05, App. A TILE_SIZE: 8 x 8 px
  localparam int BITS_PER_PIXEL = 8 / arcade_regs_pkg::PIXELS_PER_BYTE;  // §3.1.4: 4 bpp
  localparam int BYTES_PER_ROW = TILE_SIZE / arcade_regs_pkg::PIXELS_PER_BYTE;  // 4
  localparam int ROWS_PER_TILE = arcade_regs_pkg::BYTES_PER_TILE_PATTERN / BYTES_PER_ROW;  // 8
  localparam int PATTERN_WORDS = arcade_regs_pkg::TILE_PATTERN_RAM_SIZE / BYTES_PER_ROW;  // 2048
  localparam int PATTERN_ADDR_W = $clog2(PATTERN_WORDS);  // 11
  localparam int PALETTE_ADDR_W = $clog2(arcade_regs_pkg::PALETTE_SIZE);  // SR-REN-05: 4-bit index
  localparam int SUB_W = $clog2(TILE_SIZE);  // pixel position inside a tile: 3 bits
  localparam int NIBBLE_SEL_W = $clog2(BITS_PER_PIXEL);  // x * 4 = x << 2

  // verilog_lint: waive-stop parameter-name-style

  // ---------------------------------------------------------------------------
  // Memories (SR-REN-04, SR-REN-05), written by the bridge, loaded at start-up
  // ---------------------------------------------------------------------------
  // Pattern RAM: word {tile, row} holds 8 pixels; pixel x is nibble x (low nibble = left).
  logic [31:0] pattern_ram[PATTERN_WORDS];
  // Palette RAM: 16 entries packed 0x00RRGGBB.
  logic [31:0] palette_ram[arcade_regs_pkg::PALETTE_SIZE];

  // ---------------------------------------------------------------------------
  // Pipeline registers
  // ---------------------------------------------------------------------------
  logic [4:1] hsync_shift;  // [n] = value at stage n
  logic [4:1] vsync_shift;
  logic [4:1] blank_shift;

  logic [SUB_W-1:0] x_stage1_q, x_stage2_q;
  logic [SUB_W-1:0] y_stage1_q;

  logic [31:0] stage2_pattern_row_q;
  logic [PALETTE_ADDR_W-1:0] stage2_palette_idx;
  logic [31:0] stage3_rgb_q;

  // TBL-TilePattern, §3.1.4: 2 px/byte, low nibble = left pixel, so pixel x is nibble x.
  assign stage2_palette_idx = stage2_pattern_row_q[{
    x_stage2_q, {NIBBLE_SEL_W{1'b0}}
  }+:BITS_PER_PIXEL];

  // ---------------------------------------------------------------------------
  // Memory write ports
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk) begin
    if (pat_wr_en) begin
      for (int b = 0; b < 4; b++) begin
        if (pat_wr_strb[b]) pattern_ram[pat_wr_addr][8*b+:8] <= pat_wr_data[8*b+:8];
      end
    end
    if (pal_wr_en) palette_ram[pal_wr_addr] <= pal_wr_data;
  end

  // ---------------------------------------------------------------------------
  // 4-stage read pipeline (SR-REN-06)
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk) begin
    if (rst) begin
      hsync_shift <= '0;
      vsync_shift <= '0;
      blank_shift <= '1;  // blank until real timing flows through
      vga_r       <= '0;
      vga_g       <= '0;
      vga_b       <= '0;
    end else begin
      // Stage 1: register pixel position and syncs alongside the tilemap read.
      hsync_shift[1]       <= hsync_in;
      vsync_shift[1]       <= vsync_in;
      blank_shift[1]       <= blank_in;
      x_stage1_q           <= vga_x[SUB_W-1:0];
      y_stage1_q           <= vga_y[SUB_W-1:0];

      // Stage 2: pattern read, word address {tile_index, row} (SR-REN-04).
      stage2_pattern_row_q <= pattern_ram[{tile_index, y_stage1_q}];
      x_stage2_q           <= x_stage1_q;
      hsync_shift[2]       <= hsync_shift[1];
      vsync_shift[2]       <= vsync_shift[1];
      blank_shift[2]       <= blank_shift[1];

      // Stage 3: palette read by the 4-bit index (SR-REN-05).
      stage3_rgb_q         <= palette_ram[stage2_palette_idx];
      hsync_shift[3]       <= hsync_shift[2];
      vsync_shift[3]       <= vsync_shift[2];
      blank_shift[3]       <= blank_shift[2];

      // Stage 4: colour output, 0x00RRGGBB at 8 bits/channel (CON-06). No picture is sent
      // during blanking (glossary "Blanking"), so RGB is forced to black there.
      if (blank_shift[3]) begin
        vga_r <= '0;
        vga_g <= '0;
        vga_b <= '0;
      end else begin
        vga_r <= stage3_rgb_q[arcade_regs_pkg::PALETTE_RED_SHIFT+:8];
        vga_g <= stage3_rgb_q[arcade_regs_pkg::PALETTE_GREEN_SHIFT+:8];
        vga_b <= stage3_rgb_q[arcade_regs_pkg::PALETTE_BLUE_SHIFT+:8];
      end
      hsync_shift[4] <= hsync_shift[3];
      vsync_shift[4] <= vsync_shift[3];
      blank_shift[4] <= blank_shift[3];
    end
  end

  assign hsync_out = hsync_shift[4];
  assign vsync_out = vsync_shift[4];
  assign blank_out = blank_shift[4];

  // Elaboration checks: one 32-bit word is one tile row, and the word address is exactly
  // {tile_index, row}, so the port widths above agree with the derived sizes.
  if (BYTES_PER_ROW * 8 != 32 || PATTERN_ADDR_W != $bits(
          pat_wr_addr
      ) || PATTERN_ADDR_W != $bits(
          tile_index
      ) + $clog2(
          ROWS_PER_TILE
      )) begin : g_bad_size
    $error("tile_renderer_pipeline: pattern RAM geometry does not match arcade_regs_pkg");
  end

  // Bits not used by design: tile-grid part of x/y (the tilemap read uses them upstream)
  // and the unused top byte of 0x00RRGGBB.
  wire _unused = &{1'b0, vga_x[9:SUB_W], vga_y[9:SUB_W], stage3_rgb_q[31:24]};
endmodule
