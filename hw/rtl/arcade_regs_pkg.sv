/* verilator lint_off UNUSEDPARAM */
/* verilator lint_off TIMESCALEMOD */
// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
package arcade_regs_pkg;

  // verilog_lint: waive-start parameter-name-style

  // ---------------------------------------------------------------------
  // Bridge window (SRS §3.1.4, SR-HAL-01; App. A BRIDGE_BASE, BRIDGE_WINDOW_SIZE)
  // ---------------------------------------------------------------------
  localparam bit [31:0] BRIDGE_BASE = 32'hFF200000;  // §3.1.4, SR-HAL-01
  localparam bit [31:0] BRIDGE_WINDOW_SIZE = 32'h0020_0000;  // §3.1.4, SR-HAL-01, 2 MB

  // ---------------------------------------------------------------------
  // TBL-Tilemap
  // ---------------------------------------------------------------------
  localparam int GRID_COLS = 128;  // SR-FW-12, UR-06
  localparam int GRID_ROWS = 96;  // SR-FW-12, UR-06
  localparam int TILEMAP_BANK_SIZE = 12288;  // SR-REN-01, SR-HAL-03/04

  // ---------------------------------------------------------------------
  // TBL-TilePattern
  // ---------------------------------------------------------------------
  localparam int TILE_PATTERN_COUNT = 256;  // SR-REN-04
  localparam int TILE_PATTERN_RAM_SIZE = 8192;  // SR-REN-04
  localparam int BYTES_PER_TILE_PATTERN = 32;  // §3.1.4: 8x8 px, 4bpp, 2 px/byte

  // Pixel packing (§3.1.4): 2 pixels/byte; low nibble = left pixel
  localparam int PIXELS_PER_BYTE = 2;  // §3.1.4
  localparam int LEFT_PIXEL_NIBBLE_SHIFT = 0;  // §3.1.4: low nibble = left pixel
  localparam int RIGHT_PIXEL_NIBBLE_SHIFT = 4;  // §3.1.4: high nibble = right pixel

  // ---------------------------------------------------------------------
  // TBL-Palette
  // ---------------------------------------------------------------------
  localparam int PALETTE_SIZE = 16;  // SR-REN-05
  localparam int BYTES_PER_PALETTE_ENTRY = 4;  // §3.1.4: 32 bits, Palette Base

  // 0x00RRGGBB, 8 bits/channel (§3.1.4, CON-06)
  localparam int PALETTE_RED_SHIFT = 16;  // §3.1.4, CON-06
  localparam int PALETTE_GREEN_SHIFT = 8;  // §3.1.4, CON-06
  localparam int PALETTE_BLUE_SHIFT = 0;  // §3.1.4, CON-06
  localparam bit [31:0] PALETTE_CHANNEL_MASK = 32'h0000_00FF;  // §3.1.4, CON-06

  // ---------------------------------------------------------------------
  // TBL-RegisterMap (§3.1.4, Table 3.3) -- all 12 offsets, in table order
  // ---------------------------------------------------------------------
  localparam bit [31:0] OFFSET_TILEMAP_BANK0 = 32'h0000;  // §3.1.4
  localparam bit [7:0] RESET_TILEMAP_BANK0 = 8'h00;  // §3.1.4

  localparam bit [31:0] OFFSET_TILEMAP_BANK1 = 32'h3000;  // §3.1.4
  localparam bit [7:0] RESET_TILEMAP_BANK1 = 8'h00;  // §3.1.4

  localparam bit [31:0] OFFSET_TILE_PATTERN_RAM = 32'h6000;  // §3.1.4
  localparam bit [7:0] RESET_TILE_PATTERN_RAM = 8'h00;  // §3.1.4

  localparam bit [31:0] OFFSET_PALETTE_BASE = 32'h8000;  // §3.1.4
  localparam bit [31:0] RESET_PALETTE_ENTRY = 32'h0000_0000;  // §3.1.4

  localparam bit [31:0] OFFSET_CONTROL_REG = 32'h8040;  // §3.1.4
  localparam bit [31:0] RESET_CONTROL_REG = 32'h0000_0000;  // §3.1.4

  localparam bit [31:0] OFFSET_STATUS_REG = 32'h8044;  // §3.1.4
  localparam bit [31:0] RESET_STATUS_REG = 32'h0000_0000;  // §3.1.4 (NOT FPGA-ready-set)

  localparam bit [31:0] OFFSET_FRAME_COUNTER = 32'h8048;  // §3.1.4
  localparam bit [31:0] RESET_FRAME_COUNTER = 32'h0000_0000;  // §3.1.4

  localparam bit [31:0] OFFSET_SWAP_REQ_FRONT_BANK = 32'h804C;  // §3.1.4
  localparam bit [31:0] RESET_SWAP_REQ_FRONT_BANK = 32'h0000_0000;  // §3.1.4

  localparam bit [31:0] OFFSET_ID_VERSION = 32'h8050;  // §3.1.4
  localparam bit [31:0] RESET_ID_VERSION = 32'hA2CA_DE01;  // §3.1.4

  localparam bit [31:0] OFFSET_TONE_FREQUENCY = 32'h8054;  // §3.1.4, SR-AUD-04
  localparam bit [15:0] RESET_TONE_FREQUENCY = 16'h0000;  // §3.1.4

  localparam bit [31:0] OFFSET_TONE_DURATION = 32'h8058;  // §3.1.4, SR-AUD-05
  localparam bit [15:0] RESET_TONE_DURATION = 16'h0000;  // §3.1.4

  localparam bit [31:0] OFFSET_AUDIO_CONTROL = 32'h805C;  // §3.1.4
  localparam bit [31:0] RESET_AUDIO_CONTROL = 32'h0000_0000;  // §3.1.4

  // ---------------------------------------------------------------------
  // HAL_ID_MAGIC (App. A; SR-HAL-09) -- same value as RESET_ID_VERSION above
  // ---------------------------------------------------------------------
  localparam bit [31:0] HAL_ID_MAGIC = 32'hA2CA_DE01;

  // ---------------------------------------------------------------------
  // Control Register bits (§3.1.4)
  // ---------------------------------------------------------------------
  localparam int BIT_CONTROL_RESET_TILEMAP = 1;  // §3.1.4
  localparam int BIT_CONTROL_DISPLAY_ENABLE = 2;  // §3.1.4, SR-HAL-13, SR-VGA-09

  // ---------------------------------------------------------------------
  // Status Register bits (§3.1.4)
  // ---------------------------------------------------------------------
  localparam int BIT_STATUS_VSYNC = 0;  // §3.1.4
  localparam int BIT_STATUS_FPGA_READY = 1;  // §3.1.4
  localparam int BIT_STATUS_SWAP_PENDING = 2;  // §3.1.4, SR-HAL-05, SR-REN-03

  // ---------------------------------------------------------------------
  // Swap Request / Front Bank, bit 0 (§3.1.4): write 1 = Swap Request,
  // read = Front Bank.
  // ---------------------------------------------------------------------
  localparam int BIT_SWAP_REQUEST = 0;  // §3.1.4, write side
  localparam int BIT_FRONT_BANK = 0;  // §3.1.4, read side

  // ---------------------------------------------------------------------
  // Audio Control Register bits (§3.1.4)
  // ---------------------------------------------------------------------
  localparam int BIT_AUDIO_ENABLE = 0;  // §3.1.4, SR-HAL-10
  localparam int BIT_AUDIO_MUTE = 1;  // §3.1.4, SR-HAL-12

  // verilog_lint: waive-stop parameter-name-style

endpackage : arcade_regs_pkg
