`timescale 1ns / 1ps

// Tile Renderer Pipeline: Stages 2-4 (Pattern Read, Palette Read, RGB Output)
// Covers SR-REN-04, SR-REN-05, SR-REN-06, TBL-TilePattern, TBL-Palette.
module tile_renderer_pipeline (
    input logic clk,
    input logic rst,  // Synchronous reset

    // VGA Controller Inputs (Cycle 0)
    input logic [9:0] vga_x,
    input logic [9:0] vga_y,
    input logic       hsync_in,
    input logic       vsync_in,
    input logic       blank_in,

    // Stage 1 Input (Cycle 1): Tile index from Tilemap Buffer
    input logic [7:0] tile_index,

    // Pattern RAM Write Port (mapped to 0x6000-0x7FFF)
    input logic        pat_wr_en,
    input logic [10:0] pat_wr_addr,  // 2048 x 32-bit words (8KB total)
    input logic [31:0] pat_wr_data,
    input logic [ 3:0] pat_wr_strb,  // Byte enables for 32-bit writes

    // Palette RAM Write Port (mapped to 0x8000)
    input logic        pal_wr_en,
    input logic [ 3:0] pal_wr_addr,  // 16 entries
    input logic [31:0] pal_wr_data,  // 0x00RRGGBB

    // VGA Output (Cycle 4)
    output logic [7:0] vga_r,
    output logic [7:0] vga_g,
    output logic [7:0] vga_b,
    output logic       hsync_out,
    output logic       vsync_out,
    output logic       blank_out
);

  // Sink unused top bits to satisfy Verilator without pragmas

  // Explicit constants imported strictly from the package
  localparam int PalSize = arcade_regs_pkg::PALETTE_SIZE;

  // =========================================================================
  // Memory Declarations (SR-REN-04, SR-REN-05)
  // =========================================================================

  // Pattern RAM: 2048 entries of 32 bits (256 tiles * 8 rows = 8KB)
  logic [31:0] pattern_ram[2048];

  // Palette RAM: 16 entries of 32 bits (0x00RRGGBB)
  logic [31:0] palette_ram[PalSize];

  // =========================================================================
  // Pipeline Registers
  // =========================================================================

  // Shift registers for sync and blank signals to achieve exactly N=4 latency
  logic [4:1] hsync_shift;
  logic [4:1] vsync_shift;
  logic [4:1] blank_shift;

  // Shift registers for pixel coordinates (x[2:0] and y[2:0] within the tile)
  logic [2:0] x_shift[1:2];
  logic [2:0] y_shift[1:1];

  // Pipeline Data
  logic [31:0] stage2_pattern_row;
  logic [3:0] stage3_palette_idx;
  logic [31:0] stage4_rgb;

  // Combinational extraction of the palette index
  // TBL-TilePattern: 2 pixels/byte; low nibble = left pixel.
  assign stage3_palette_idx = stage2_pattern_row[{x_shift[2], 2'b00}+:4];

  // =========================================================================
  // Memory Write Ports
  // =========================================================================
  always_ff @(posedge clk) begin
    if (pat_wr_en) begin
      if (pat_wr_strb[0]) pattern_ram[pat_wr_addr][7:0] <= pat_wr_data[7:0];
      if (pat_wr_strb[1]) pattern_ram[pat_wr_addr][15:8] <= pat_wr_data[15:8];
      if (pat_wr_strb[2]) pattern_ram[pat_wr_addr][23:16] <= pat_wr_data[23:16];
      if (pat_wr_strb[3]) pattern_ram[pat_wr_addr][31:24] <= pat_wr_data[31:24];
    end
    if (pal_wr_en) begin
      palette_ram[pal_wr_addr] <= pal_wr_data;
    end
  end

  // =========================================================================
  // 4-Stage Read Pipeline (SR-REN-06)
  // =========================================================================
  always_ff @(posedge clk) begin
    if (rst) begin
      hsync_shift <= '0;
      vsync_shift <= '0;
      blank_shift <= '1;  // Blank active by default
      vga_r <= '0;
      vga_g <= '0;
      vga_b <= '0;
    end else begin
      // -----------------------------------------------------------------
      // Cycle 0 -> 1 (Stage 1 tracking)
      // -----------------------------------------------------------------
      hsync_shift[1]     <= hsync_in;
      vsync_shift[1]     <= vsync_in;
      blank_shift[1]     <= blank_in;
      x_shift[1]         <= vga_x[2:0];
      y_shift[1]         <= vga_y[2:0];

      // -----------------------------------------------------------------
      // Cycle 1 -> 2: Stage 2 (Pattern Read)
      // -----------------------------------------------------------------
      stage2_pattern_row <= pattern_ram[{tile_index, y_shift[1]}];

      hsync_shift[2]     <= hsync_shift[1];
      vsync_shift[2]     <= vsync_shift[1];
      blank_shift[2]     <= blank_shift[1];
      x_shift[2]         <= x_shift[1];

      // -----------------------------------------------------------------
      // Cycle 2 -> 3: Stage 3 (Palette Read)
      // -----------------------------------------------------------------
      stage4_rgb         <= palette_ram[stage3_palette_idx];

      hsync_shift[3]     <= hsync_shift[2];
      vsync_shift[3]     <= vsync_shift[2];
      blank_shift[3]     <= blank_shift[2];

      // -----------------------------------------------------------------
      // Cycle 3 -> 4: Stage 4 (Colour Output)
      // -----------------------------------------------------------------
      vga_r              <= stage4_rgb[23:16];
      vga_g              <= stage4_rgb[15:8];
      vga_b              <= stage4_rgb[7:0];

      hsync_shift[4]     <= hsync_shift[3];
      vsync_shift[4]     <= vsync_shift[3];
      blank_shift[4]     <= blank_shift[3];
    end
  end

  // Map delayed syncs to module outputs
  assign hsync_out = hsync_shift[4];
  assign vsync_out = vsync_shift[4];
  assign blank_out = blank_shift[4];

  wire _unused = &{1'b0, vga_x[9:3], vga_y[9:3], stage4_rgb[31:24]};
endmodule
