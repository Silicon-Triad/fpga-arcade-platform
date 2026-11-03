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
// "hps_0_hps_io") are board-specific and not in the SRS (R-10.10). Names, widths and
// connections are copied unchanged from the Terasic DE1-SoC GHRD (rev E, ghrd_top.v), the same
// source as the HPS settings in #28.
//   B5 No FPGA-side reset requests to the HPS (see u_soc).
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
    output logic       VGA_VS,

    // HPS (copied from the Terasic DE1-SoC GHRD rev E, ghrd_top.v; not in the SRS, see #28)
    inout  wire        HPS_CONV_USB_N,
    output wire [14:0] HPS_DDR3_ADDR,
    output wire [ 2:0] HPS_DDR3_BA,
    output wire        HPS_DDR3_CAS_N,
    output wire        HPS_DDR3_CKE,
    output wire        HPS_DDR3_CK_N,
    output wire        HPS_DDR3_CK_P,
    output wire        HPS_DDR3_CS_N,
    output wire [ 3:0] HPS_DDR3_DM,
    inout  wire [31:0] HPS_DDR3_DQ,
    inout  wire [ 3:0] HPS_DDR3_DQS_N,
    inout  wire [ 3:0] HPS_DDR3_DQS_P,
    output wire        HPS_DDR3_ODT,
    output wire        HPS_DDR3_RAS_N,
    output wire        HPS_DDR3_RESET_N,
    input  wire        HPS_DDR3_RZQ,
    output wire        HPS_DDR3_WE_N,
    output wire        HPS_ENET_GTX_CLK,
    inout  wire        HPS_ENET_INT_N,
    output wire        HPS_ENET_MDC,
    inout  wire        HPS_ENET_MDIO,
    input  wire        HPS_ENET_RX_CLK,
    input  wire [ 3:0] HPS_ENET_RX_DATA,
    input  wire        HPS_ENET_RX_DV,
    output wire [ 3:0] HPS_ENET_TX_DATA,
    output wire        HPS_ENET_TX_EN,
    inout  wire [ 3:0] HPS_FLASH_DATA,
    output wire        HPS_FLASH_DCLK,
    output wire        HPS_FLASH_NCSO,
    inout  wire        HPS_GSENSOR_INT,
    inout  wire        HPS_I2C1_SCLK,
    inout  wire        HPS_I2C1_SDAT,
    inout  wire        HPS_I2C2_SCLK,
    inout  wire        HPS_I2C2_SDAT,
    inout  wire        HPS_I2C_CONTROL,
    inout  wire        HPS_KEY,
    inout  wire        HPS_LED,
    inout  wire        HPS_LTC_GPIO,
    output wire        HPS_SD_CLK,
    inout  wire        HPS_SD_CMD,
    inout  wire [ 3:0] HPS_SD_DATA,
    output wire        HPS_SPIM_CLK,
    input  wire        HPS_SPIM_MISO,
    output wire        HPS_SPIM_MOSI,
    inout  wire        HPS_SPIM_SS,
    input  wire        HPS_UART_RX,
    output wire        HPS_UART_TX,
    input  wire        HPS_USB_CLKOUT,
    inout  wire [ 7:0] HPS_USB_DATA,
    input  wire        HPS_USB_DIR,
    input  wire        HPS_USB_NXT,
    output wire        HPS_USB_STP
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
      .rst     (1'b0),
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
      .audio_audio_mute    (audio_mute),

      // B5: no FPGA-side reset of the system or the HPS. GHRD ties reset_reset_n to 1; the
      // GHRD's reset requests come from a debug source/probe and its STM events from LEDs and
      // switches, neither used here, so the requests are inactive (active low) and events 0.
      .reset_reset_n                       (1'b1),
      .hps_0_f2h_cold_reset_req_reset_n    (1'b1),
      .hps_0_f2h_debug_reset_req_reset_n   (1'b1),
      .hps_0_f2h_warm_reset_req_reset_n    (1'b1),
      .hps_0_f2h_stm_hw_events_stm_hwevents('0),
      .hps_0_h2f_reset_reset_n             (),

      // HPS DDR3 and I/O: connections as in the GHRD ghrd_top.v
      .memory_mem_a                         (HPS_DDR3_ADDR),
      .memory_mem_ba                        (HPS_DDR3_BA),
      .memory_mem_ck                        (HPS_DDR3_CK_P),
      .memory_mem_ck_n                      (HPS_DDR3_CK_N),
      .memory_mem_cke                       (HPS_DDR3_CKE),
      .memory_mem_cs_n                      (HPS_DDR3_CS_N),
      .memory_mem_ras_n                     (HPS_DDR3_RAS_N),
      .memory_mem_cas_n                     (HPS_DDR3_CAS_N),
      .memory_mem_we_n                      (HPS_DDR3_WE_N),
      .memory_mem_reset_n                   (HPS_DDR3_RESET_N),
      .memory_mem_dq                        (HPS_DDR3_DQ),
      .memory_mem_dqs                       (HPS_DDR3_DQS_P),
      .memory_mem_dqs_n                     (HPS_DDR3_DQS_N),
      .memory_mem_odt                       (HPS_DDR3_ODT),
      .memory_mem_dm                        (HPS_DDR3_DM),
      .memory_oct_rzqin                     (HPS_DDR3_RZQ),
      .hps_0_hps_io_hps_io_emac1_inst_TX_CLK(HPS_ENET_GTX_CLK),
      .hps_0_hps_io_hps_io_emac1_inst_TXD0  (HPS_ENET_TX_DATA[0]),
      .hps_0_hps_io_hps_io_emac1_inst_TXD1  (HPS_ENET_TX_DATA[1]),
      .hps_0_hps_io_hps_io_emac1_inst_TXD2  (HPS_ENET_TX_DATA[2]),
      .hps_0_hps_io_hps_io_emac1_inst_TXD3  (HPS_ENET_TX_DATA[3]),
      .hps_0_hps_io_hps_io_emac1_inst_RXD0  (HPS_ENET_RX_DATA[0]),
      .hps_0_hps_io_hps_io_emac1_inst_MDIO  (HPS_ENET_MDIO),
      .hps_0_hps_io_hps_io_emac1_inst_MDC   (HPS_ENET_MDC),
      .hps_0_hps_io_hps_io_emac1_inst_RX_CTL(HPS_ENET_RX_DV),
      .hps_0_hps_io_hps_io_emac1_inst_TX_CTL(HPS_ENET_TX_EN),
      .hps_0_hps_io_hps_io_emac1_inst_RX_CLK(HPS_ENET_RX_CLK),
      .hps_0_hps_io_hps_io_emac1_inst_RXD1  (HPS_ENET_RX_DATA[1]),
      .hps_0_hps_io_hps_io_emac1_inst_RXD2  (HPS_ENET_RX_DATA[2]),
      .hps_0_hps_io_hps_io_emac1_inst_RXD3  (HPS_ENET_RX_DATA[3]),
      .hps_0_hps_io_hps_io_qspi_inst_IO0    (HPS_FLASH_DATA[0]),
      .hps_0_hps_io_hps_io_qspi_inst_IO1    (HPS_FLASH_DATA[1]),
      .hps_0_hps_io_hps_io_qspi_inst_IO2    (HPS_FLASH_DATA[2]),
      .hps_0_hps_io_hps_io_qspi_inst_IO3    (HPS_FLASH_DATA[3]),
      .hps_0_hps_io_hps_io_qspi_inst_SS0    (HPS_FLASH_NCSO),
      .hps_0_hps_io_hps_io_qspi_inst_CLK    (HPS_FLASH_DCLK),
      .hps_0_hps_io_hps_io_sdio_inst_CMD    (HPS_SD_CMD),
      .hps_0_hps_io_hps_io_sdio_inst_D0     (HPS_SD_DATA[0]),
      .hps_0_hps_io_hps_io_sdio_inst_D1     (HPS_SD_DATA[1]),
      .hps_0_hps_io_hps_io_sdio_inst_CLK    (HPS_SD_CLK),
      .hps_0_hps_io_hps_io_sdio_inst_D2     (HPS_SD_DATA[2]),
      .hps_0_hps_io_hps_io_sdio_inst_D3     (HPS_SD_DATA[3]),
      .hps_0_hps_io_hps_io_usb1_inst_D0     (HPS_USB_DATA[0]),
      .hps_0_hps_io_hps_io_usb1_inst_D1     (HPS_USB_DATA[1]),
      .hps_0_hps_io_hps_io_usb1_inst_D2     (HPS_USB_DATA[2]),
      .hps_0_hps_io_hps_io_usb1_inst_D3     (HPS_USB_DATA[3]),
      .hps_0_hps_io_hps_io_usb1_inst_D4     (HPS_USB_DATA[4]),
      .hps_0_hps_io_hps_io_usb1_inst_D5     (HPS_USB_DATA[5]),
      .hps_0_hps_io_hps_io_usb1_inst_D6     (HPS_USB_DATA[6]),
      .hps_0_hps_io_hps_io_usb1_inst_D7     (HPS_USB_DATA[7]),
      .hps_0_hps_io_hps_io_usb1_inst_CLK    (HPS_USB_CLKOUT),
      .hps_0_hps_io_hps_io_usb1_inst_STP    (HPS_USB_STP),
      .hps_0_hps_io_hps_io_usb1_inst_DIR    (HPS_USB_DIR),
      .hps_0_hps_io_hps_io_usb1_inst_NXT    (HPS_USB_NXT),
      .hps_0_hps_io_hps_io_spim1_inst_CLK   (HPS_SPIM_CLK),
      .hps_0_hps_io_hps_io_spim1_inst_MOSI  (HPS_SPIM_MOSI),
      .hps_0_hps_io_hps_io_spim1_inst_MISO  (HPS_SPIM_MISO),
      .hps_0_hps_io_hps_io_spim1_inst_SS0   (HPS_SPIM_SS),
      .hps_0_hps_io_hps_io_uart0_inst_RX    (HPS_UART_RX),
      .hps_0_hps_io_hps_io_uart0_inst_TX    (HPS_UART_TX),
      .hps_0_hps_io_hps_io_i2c0_inst_SDA    (HPS_I2C1_SDAT),
      .hps_0_hps_io_hps_io_i2c0_inst_SCL    (HPS_I2C1_SCLK),
      .hps_0_hps_io_hps_io_i2c1_inst_SDA    (HPS_I2C2_SDAT),
      .hps_0_hps_io_hps_io_i2c1_inst_SCL    (HPS_I2C2_SCLK),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO09 (HPS_CONV_USB_N),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO35 (HPS_ENET_INT_N),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO40 (HPS_LTC_GPIO),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO48 (HPS_I2C_CONTROL),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO53 (HPS_LED),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO54 (HPS_KEY),
      .hps_0_hps_io_hps_io_gpio_inst_GPIO61 (HPS_GSENSOR_INT)
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
