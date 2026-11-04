// Video path of the arcade: tilemap banks, crop, Tile Renderer pipeline and VGA Controller,
// wired together on one clock (CON-01, CON-03, CON-04). Refs #33.
//
// This is everything of the FPGA design that is plain RTL. The board top level
// (hw/quartus/arcade_top.sv) adds the pixel-clock PLL and the Platform Designer system
// (HPS + bridge_regs, hw/ip/arcade_soc.qsys) around it. The ports below are the bridge_regs
// "video" conduit (hw/ip/bridge_regs_hw.tcl) plus the VGA pins, so the top level only connects
// names.
//
// Pixel path (one new pixel per clock):
//   cycle 0  vga_controller gives pixel_x, pixel_y, pixel_active, h_sync, v_sync.
//            tilemap_crop turns the position into a front-bank cell address (SR-REN-08).
//   cycle 1  tilemap_banks returns the tile index (1 clock read latency) into stage 1 of
//            tile_renderer_pipeline (SR-REN-06).
//   cycle 4  colour, sync and blank leave the pipeline aligned (SR-REN-06) and
//            vga_controller registers them onto the VGA pins (SR-VGA-04 to SR-VGA-07).
//
// Design decisions the SRS does not give (listed in the #33 PR for review):
//   B1 fpga_ready (Status bit 1, §3.1.4) is high when this block is out of reset and the
//      tilemap clear (SR-REN-07) is not running, so host tilemap writes would be accepted.
//      The top level also holds reset until the PLL has locked.
//   B2 One VIDEO_MODE (SR-VGA-11) is passed to both vga_controller and tilemap_crop, so the
//      timing and the crop always agree; the top level picks the matching PLL output.
//
// Clock domain: everything on clk, the pixel clock (65.000 MHz in XGA, SR-VGA-01). Reset:
// synchronous, active high (R-4.2.6).
// verilog_lint: waive-start parameter-name-style
module arcade_video #(
    // SR-VGA-11, App. A VIDEO_MODE: "XGA_1024x768" (default) or "VGA_640x480" (SR-VGA-10).
    parameter string VIDEO_MODE = "XGA_1024x768"
) (
    input logic clk,
    input logic rst,

    // From bridge_regs (video conduit, §3.1.4)
    input  logic                                                               tm_wr_en,
    input  logic                                                               tm_wr_bank,
    input  logic [             $clog2(arcade_regs_pkg::TILEMAP_BANK_SIZE)-1:0] tm_wr_addr,
    input  logic [                                                        7:0] tm_wr_data,
    input  logic                                                               swap_req,
    output logic                                                               front_bank,
    output logic                                                               swap_pending,
    input  logic                                                               pat_wr_en,
    input  logic [$clog2(arcade_regs_pkg::TILE_PATTERN_RAM_SIZE * 8 / 32)-1:0] pat_wr_addr,
    input  logic [                                                       31:0] pat_wr_data,
    input  logic [                                                        3:0] pat_wr_strb,
    input  logic                                                               pal_wr_en,
    input  logic [                  $clog2(arcade_regs_pkg::PALETTE_SIZE)-1:0] pal_wr_addr,
    input  logic [                                                       31:0] pal_wr_data,
    output logic                                                               vblank_start,
    output logic                                                               fpga_ready,
    input  logic                                                               reset_tilemap,
    input  logic                                                               display_enable,

    // verilog_lint: waive-start port-name-style
    output logic [7:0] VGA_R,
    output logic [7:0] VGA_G,
    output logic [7:0] VGA_B,
    output logic       VGA_HS,
    output logic       VGA_VS,
    output logic       VGA_BLANK_N,
    output logic       VGA_SYNC_N
    // verilog_lint: waive-stop port-name-style
);
  import arcade_regs_pkg::*;

  localparam int TM_ADDR_W = $clog2(TILEMAP_BANK_SIZE);

  // Cycle 0: timing from the VGA Controller
  logic [9:0] pixel_x, pixel_y;
  logic pixel_active, h_sync, v_sync;

  // Cycle 0: cell address and crop (SR-REN-08)
  logic [TM_ADDR_W-1:0] cell_addr;
  logic cell_visible, crop_blank;

  // Cycle 1: tile index from the front bank
  logic [7:0] tile_index;
  logic clear_busy;

  // Cycle 4: renderer output
  logic [7:0] ren_r, ren_g, ren_b;
  logic ren_hs, ren_vs, ren_blank;

  vga_controller #(
      .VIDEO_MODE(VIDEO_MODE)
  ) u_vga (
      .clk,
      .rst,
      .display_enable,
      .pixel_x,
      .pixel_y,
      .pixel_active,
      .h_sync,
      .v_sync,
      .vblank_start,
      .rgb_in   ({ren_r, ren_g, ren_b}),  // {R, G, B} (SR-VGA-07)
      .h_sync_in(ren_hs),
      .v_sync_in(ren_vs),
      .blank_in (ren_blank),
      .VGA_R,
      .VGA_G,
      .VGA_B,
      .VGA_HS,
      .VGA_VS,
      .VGA_BLANK_N,
      .VGA_SYNC_N
  );

  tilemap_crop #(
      .VIDEO_MODE(VIDEO_MODE)
  ) u_crop (
      .vga_x    ({1'b0, pixel_x}),
      .vga_y    (pixel_y),
      .blank_in (!pixel_active),
      .cell_addr,
      .cell_visible,
      .blank_out(crop_blank)
  );

  // cell_visible is already folded into blank_out by tilemap_crop; nothing else needs it.
  logic unused_cell_visible;
  assign unused_cell_visible = cell_visible;

  tilemap_banks u_banks (
      .clk,
      .rst,
      .wr_en  (tm_wr_en),
      .wr_bank(tm_wr_bank),
      .wr_addr(tm_wr_addr),
      .wr_data(tm_wr_data),
      .swap_req,
      .reset_tilemap,
      .vblank_start,
      .rd_addr(cell_addr),
      .rd_data(tile_index),
      .front_bank,
      .swap_pending,
      .clear_busy
  );

  tile_renderer_pipeline u_pipe (
      .clk,
      .rst,
      .vga_x(pixel_x),
      .vga_y(pixel_y),
      .hsync_in(h_sync),
      .vsync_in(v_sync),
      .blank_in(crop_blank),
      .tile_index,  // one clock after pixel_x/pixel_y (SR-REN-06 stage 1)
      .pat_wr_en,
      .pat_wr_addr,
      .pat_wr_data,
      .pat_wr_strb,
      .pal_wr_en,
      .pal_wr_addr,
      .pal_wr_data,
      .vga_r(ren_r),
      .vga_g(ren_g),
      .vga_b(ren_b),
      .hsync_out(ren_hs),
      .vsync_out(ren_vs),
      .blank_out(ren_blank)
  );

  // B1: Status bit 1, FPGA ready (§3.1.4)
  assign fpga_ready = !rst && !clear_busy;

endmodule
// verilog_lint: waive-stop parameter-name-style
