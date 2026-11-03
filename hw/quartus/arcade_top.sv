// FPGA top level of the arcade on the DE1-SoC (CON-01, CON-03, CON-04, SR-VGA-01). Refs #33,
// closes the PLL part of #25.
//
//   CLOCK_50 ──> pixel_pll ──> pix_clk ──┬─> arcade_soc (bridge_regs_0 + clock crossing)
//                     │                  ├─> arcade_video (tilemap, renderer, VGA Controller)
//                     │                  └─> VGA_CLK (DAC clock = pixel clock, §3.1.2.1)
//                     └─ locked ──> reset synchronizer ──> pix_rst
//
// Why this file is in hw/quartus/ and not hw/rtl/: it instantiates two Quartus-generated
// modules (arcade_soc from hw/ip/arcade_soc.qsys, pixel_pll from the IP Catalog) that do not
// exist outside Quartus, so Verilator lint and the testbench flow (hw/sim) cannot build it.
// Everything that can be simulated is in hw/rtl/arcade_video.sv and tested by
// hw/tb/tb_arcade_video.sv. This file only connects names.
//
// Design decisions the SRS does not give (listed in the #33 PR for review):
//   B3 One PLL, two outputs: outclk_0 = 65.000 MHz (XGA, CON-03: 50 MHz x 13 / 10,
//      SR-VGA-01) and outclk_1 in 25.0-25.2 MHz (bring-up, SR-VGA-10). VIDEO_MODE picks one
//      at synthesis time (SR-VGA-11: "a fixed PLL per build"); no clock is switched at run
//      time.
//   B4 pix_rst is held while the PLL is not locked. It is asserted at once and released
//      synchronously to pix_clk through 2 flip-flops (R-4.2.5: domain CLOCK_50/PLL lock ->
//      pix_clk). The video logic itself uses it as a synchronous reset (R-4.2.6).
//
// HPS ports: the DDR3 memory and HPS I/O ports of arcade_soc (exports "memory" and
// "hps_0_hps_io") are board-specific and not in the SRS (R-10.10). They are copied from the
// DE1-SoC GHRD top level and the port list Platform Designer writes to
// hw/ip/arcade_soc/arcade_soc_inst.v; see hw/quartus/README.md, step 3.
// verilog_lint: waive-start parameter-name-style
module arcade_top #(
    // SR-VGA-11, App. A VIDEO_MODE: "XGA_1024x768" (default) or "VGA_640x480" (SR-VGA-10).
    parameter string VIDEO_MODE = "XGA_1024x768"
) (
    // verilog_lint: waive-start port-name-style
    input logic CLOCK_50,  // 50 MHz board clock (CON-03, Manual §3.5 Table 3-5)

    // VGA (§3.1.2.1, Manual Table 3-16)
    output logic [7:0] VGA_R,
    output logic [7:0] VGA_G,
    output logic [7:0] VGA_B,
    output logic       VGA_CLK,
    output logic       VGA_BLANK_N,
    output logic       VGA_SYNC_N,
    output logic       VGA_HS,
    output logic       VGA_VS

    // HPS DDR3 and HPS I/O ports: paste here from the GHRD top level (README step 3).
    // verilog_lint: waive-stop port-name-style
);
  import arcade_regs_pkg::*;

  localparam int TM_ADDR_W  = $clog2(TILEMAP_BANK_SIZE);
  localparam int PAT_ADDR_W = $clog2(TILE_PATTERN_RAM_SIZE * 8 / 32);
  localparam int PAL_ADDR_W = $clog2(PALETTE_SIZE);

  // -------------------------------------------------------------------------
  // Pixel clock (B3)
  // -------------------------------------------------------------------------
  logic clk_xga, clk_bringup, pll_locked, pix_clk;

  pixel_pll u_pll (
      .refclk  (CLOCK_50),
      .reset   (1'b0),
      .outclk_0(clk_xga),      // 65.000 MHz (CON-03, SR-VGA-01)
      .outclk_1(clk_bringup),  // 25.0-25.2 MHz (SR-VGA-10)
      .locked  (pll_locked)
  );

  // Synthesis-time choice only (SR-VGA-11); the unused output is left unconnected.
  if (VIDEO_MODE == "VGA_640x480") begin : g_clk_bringup
    assign pix_clk = clk_bringup;
  end else begin : g_clk_xga
    assign pix_clk = clk_xga;
  end

  assign VGA_CLK = pix_clk;  // DAC clock = pixel clock (§3.1.2.1)

  // -------------------------------------------------------------------------
  // Reset (B4): PLL lock (async) -> pix_clk domain
  // -------------------------------------------------------------------------
  logic [1:0] rst_sync_q;
  logic pix_rst;
  always_ff @(posedge pix_clk or negedge pll_locked) begin
    if (!pll_locked) rst_sync_q <= 2'b11;
    else rst_sync_q <= {rst_sync_q[0], 1'b0};
  end
  assign pix_rst = rst_sync_q[1];

  // -------------------------------------------------------------------------
  // bridge_regs "video" conduit (hw/ip/bridge_regs_hw.tcl)
  // -------------------------------------------------------------------------
  logic tm_wr_en, tm_wr_bank, swap_req, front_bank, swap_pending;
  logic [TM_ADDR_W-1:0] tm_wr_addr;
  logic [7:0] tm_wr_data;
  logic pat_wr_en, pal_wr_en;
  logic [PAT_ADDR_W-1:0] pat_wr_addr;
  logic [PAL_ADDR_W-1:0] pal_wr_addr;
  logic [31:0] pat_wr_data, pal_wr_data;
  logic [3:0] pat_wr_strb;
  logic vblank_start, fpga_ready, reset_tilemap, display_enable;

  // "audio" conduit: the Tone Generator (#41, #42) connects here; unused until then.
  logic [15:0] tone_frequency, tone_duration;
  logic tone_start, audio_enable, audio_mute;

  arcade_soc u_soc (
      .clk_clk             (CLOCK_50),        // clk_0: 50 MHz (CON-03)
      .pix_clk_clk         (pix_clk),
      .pix_rst_reset       (pix_rst),
      .video_tm_wr_en      (tm_wr_en),
      .video_tm_wr_bank    (tm_wr_bank),
      .video_tm_wr_addr    (tm_wr_addr),
      .video_tm_wr_data    (tm_wr_data),
      .video_swap_req      (swap_req),
      .video_front_bank    (front_bank),
      .video_swap_pending  (swap_pending),
      .video_pat_wr_en     (pat_wr_en),
      .video_pat_wr_addr   (pat_wr_addr),
      .video_pat_wr_data   (pat_wr_data),
      .video_pat_wr_strb   (pat_wr_strb),
      .video_pal_wr_en     (pal_wr_en),
      .video_pal_wr_addr   (pal_wr_addr),
      .video_pal_wr_data   (pal_wr_data),
      .video_vblank_start  (vblank_start),
      .video_fpga_ready    (fpga_ready),
      .video_reset_tilemap (reset_tilemap),
      .video_display_enable(display_enable),
      .audio_tone_frequency(tone_frequency),
      .audio_tone_duration (tone_duration),
      .audio_tone_start    (tone_start),
      .audio_audio_enable  (audio_enable),
      .audio_audio_mute    (audio_mute)
      // clk_0 reset, HPS reset requests, memory_* and hps_0_hps_io_*: paste here (README
      // step 3), as the GHRD connects them.
  );

  arcade_video #(
      .VIDEO_MODE(VIDEO_MODE)
  ) u_video (
      .clk(pix_clk),
      .rst(pix_rst),
      .tm_wr_en,
      .tm_wr_bank,
      .tm_wr_addr,
      .tm_wr_data,
      .swap_req,
      .front_bank,
      .swap_pending,
      .pat_wr_en,
      .pat_wr_addr,
      .pat_wr_data,
      .pat_wr_strb,
      .pal_wr_en,
      .pal_wr_addr,
      .pal_wr_data,
      .vblank_start,
      .fpga_ready,
      .reset_tilemap,
      .display_enable,
      .VGA_R,
      .VGA_G,
      .VGA_B,
      .VGA_HS,
      .VGA_VS,
      .VGA_BLANK_N,
      .VGA_SYNC_N
  );

endmodule
// verilog_lint: waive-stop parameter-name-style
