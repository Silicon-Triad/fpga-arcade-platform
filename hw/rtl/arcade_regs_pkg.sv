// FROZEN INTERFACE
package arcade_regs_pkg;

  // verilog_lint: waive-start parameter-name-style

  // SRS §3.1.4, TBL-RegisterMap: Register offsets
  localparam bit [31:0] OFFSET_TILEMAP_BANK0       = 32'h0000;
  localparam bit [31:0] OFFSET_TILEMAP_BANK1       = 32'h3000;
  localparam bit [31:0] OFFSET_TILE_PATTERN_RAM    = 32'h6000;
  localparam bit [31:0] OFFSET_PALETTE_BASE        = 32'h8000;
  localparam bit [31:0] OFFSET_CONTROL_REG         = 32'h8040;
  localparam bit [31:0] OFFSET_STATUS_REG          = 32'h8044;
  localparam bit [31:0] OFFSET_FRAME_COUNTER       = 32'h8048;
  localparam bit [31:0] OFFSET_SWAP_REQ_FRONT      = 32'h804C;
  localparam bit [31:0] OFFSET_ID_VERSION          = 32'h8050;
  localparam bit [31:0] OFFSET_TONE_FREQUENCY      = 32'h8054;
  localparam bit [31:0] OFFSET_TONE_DURATION       = 32'h8058;
  localparam bit [31:0] OFFSET_AUDIO_CONTROL       = 32'h805C;

  // SRS §3.1.4, SR-HAL-09: Expected magic ID value
  localparam bit [31:0] HAL_ID_MAGIC               = 32'hA2CADE01;

  // SRS §3.1.4, Control Register bits
  localparam int        BIT_CONTROL_RESET_TILEMAP  = 1;
  localparam int        BIT_CONTROL_DISPLAY_ENABLE = 2;

  // SRS §3.1.4, Status Register bits
  localparam int        BIT_STATUS_VSYNC_FLAG      = 0;
  localparam int        BIT_STATUS_FPGA_READY      = 1;
  localparam int        BIT_STATUS_SWAP_PENDING    = 2;

  // SRS §3.1.4, Audio Control Register bits
  localparam int        BIT_AUDIO_CONTROL_ENABLE   = 0;
  localparam int        BIT_AUDIO_CONTROL_MUTE     = 1;

  // verilog_lint: waive-stop parameter-name-style

endpackage : arcade_regs_pkg
