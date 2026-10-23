// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
package arcade_regs_pkg;

  // Bridge configuration (SRS §3.1.4, SR-HAL-01)
  parameter int BRIDGE_BASE_ADDRESS            = 32'hFF200000;
  parameter int BRIDGE_WINDOW_SIZE             = 32'h00200000;  // 2 MB

  // Register Offsets (SRS §3.1.4, TBL-RegisterMap)
  parameter int OFFSET_ID_VERSION_REG          = 32'h0000;
  parameter int OFFSET_CONTROL_REG             = 32'h0004;
  parameter int OFFSET_STATUS_REG              = 32'h0008;
  parameter int OFFSET_AUDIO_CONTROL_REG       = 32'h000C;
  parameter int OFFSET_AUDIO_STATUS_REG        = 32'h0010;
  parameter int OFFSET_AUDIO_DATA_REG          = 32'h0014;
  parameter int OFFSET_SWAP_REQ_FRONT_BANK_REG = 32'h8000;
  parameter int OFFSET_TILEMAP_BASE_REG        = 32'h8010;
  parameter int OFFSET_TILE_PATTERN_BASE_REG   = 32'h8020;
  parameter int OFFSET_PALETTE_BASE_REG        = 32'h8030;
  parameter int OFFSET_TIMER_REG               = 32'h8050;
  parameter int OFFSET_IRQ_MASK_REG            = 32'h805C;

  // Reset Values (SRS §3.1.4, R-4.2.6)
  parameter int RESET_ID_VERSION_REG           = 32'hA2CADE01;
  parameter int RESET_CONTROL_REG              = 32'h00000000;
  parameter int RESET_STATUS_REG               = 32'h00000000;
  parameter int RESET_AUDIO_CONTROL_REG        = 32'h00000000;
  parameter int RESET_AUDIO_STATUS_REG         = 32'h00000000;
  parameter int RESET_AUDIO_DATA_REG           = 32'h00000000;
  parameter int RESET_SWAP_REQ_FRONT_BANK_REG  = 32'h00000000;
  parameter int RESET_TILEMAP_BASE_REG         = 32'h0000;
  parameter int RESET_TILE_PATTERN_BASE_REG    = 32'h0000;
  parameter int RESET_PALETTE_BASE_REG         = 32'h0000;
  parameter int RESET_TIMER_REG                = 32'h00000000;
  parameter int RESET_IRQ_MASK_REG             = 32'h00000000;

  // Magic Constant (HAL_ID_MAGIC)
  parameter int HAL_ID_MAGIC                   = 32'hA2CADE01;

  // Tilemap Constants (TBL-Tilemap)
  parameter int GRID_COLS                      = 128;
  parameter int GRID_ROWS                      = 96;
  parameter int TILEMAP_BANK_SIZE              = 12288;

  // Tile Pattern Constants (TBL-TilePattern)
  parameter int TILE_PATTERN_COUNT             = 256;
  parameter int BYTES_PER_TILE_PATTERN         = 32;
  parameter int TILE_PATTERN_RAM_SIZE          = 8192;

  // Palette Constants (TBL-Palette)
  parameter int PALETTE_SIZE                   = 16;
  parameter int BYTES_PER_PALETTE_ENTRY        = 4;
  parameter int PALETTE_RED_SHIFT              = 16;
  parameter int PALETTE_GREEN_SHIFT            = 8;
  parameter int PALETTE_BLUE_SHIFT             = 0;
  parameter int PALETTE_MASK                   = 32'h00FFFFFF;

  // Pixel Packing & Nibble Order
  parameter int PIXELS_PER_BYTE                = 2;
  parameter int LEFT_PIXEL_NIBBLE_SHIFT        = 4;

  // Bit Positions
  parameter int BIT_CONTROL_ENABLE             = 0;
  parameter int BIT_CONTROL_RESET              = 1;

  parameter int BIT_STATUS_FPGA_READY          = 0;
  parameter int BIT_STATUS_VBLANK              = 1;
  parameter int BIT_STATUS_AUDIO_FULL          = 2;

  parameter int BIT_AUDIO_CONTROL_ENABLE       = 0;
  parameter int BIT_AUDIO_CONTROL_IRIN_EN      = 1;

  parameter int BIT_SWAP_REQUEST               = 0;
  parameter int BIT_FRONT_BANK                 = 0;

endpackage : arcade_regs_pkg
