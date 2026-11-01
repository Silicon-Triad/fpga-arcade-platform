// Bridge register block: Avalon-MM slave behind the lightweight HPS-to-FPGA bridge
// (§3.1.4, TBL-RegisterMap, SR-REN-03 status bit, AS-02). Refs #28.
//
// Decodes the frozen register map (arcade_regs_pkg) into write ports for the Tile Renderer
// (tilemap banks, pattern RAM, palette), the Control / Audio registers, and reads back
// Status, Frame Counter, Front Bank and ID / Version.
//
// Design assumptions NOT given by the SRS (owner's decisions, listed in the #28 PR for review):
//   A1 Avalon-MM slave, 32-bit data, 4-bit byteenable, word addressing. Byte offset within
//      the bridge window = {avs_address, 2'b00}; span 64 KB (covers offsets up to 0x805C).
//   A2 Runs on the renderer clock (65 MHz pixel clock, CON-03). The crossing from the HPS
//      bridge clock is done by a Platform Designer clock-crossing bridge, so no RTL
//      synchronizer is needed here (R-4.2.5).
//   A3 Fixed read latency 1 clock; reads never wait. waitrequest is raised only for a tilemap
//      write with more than one byte enabled: one cell is written per clock (tilemap_banks has
//      one write port), so the write is held for one extra clock per extra cell.
//   A4 The tilemap, pattern RAM and palette regions are write-only here: reads return 0. The
//      HAL keeps a shadow tilemap (SR-HAL-03/04) and never reads them back.
//   A5 Palette entries are written as whole 32-bit words; byteenable is ignored there.
//   A6 A write to Tone Duration also pulses tone_start, which the Tone Generator (#41) uses to
//      start the tone (SR-AUD-05 "the current tone").
//   A7 Status bit 1 (FPGA ready) is the fpga_ready input; the top level (#33) drives it.
//
// Clock domain: everything on clk. Reset: synchronous, active high (R-4.2.6).
// verilog_lint: waive-start parameter-name-style
module bridge_regs (
    input logic clk,
    input logic rst,

    // Avalon-MM slave (A1, A3)
    input  logic [13:0] avs_address,     // word address: byte offset / 4
    input  logic        avs_read,
    input  logic        avs_write,
    input  logic [31:0] avs_writedata,
    input  logic [ 3:0] avs_byteenable,
    output logic [31:0] avs_readdata,
    output logic        avs_waitrequest,

    // Tilemap write port to tilemap_banks (TBL-Tilemap, §3.1.4 0x0000-0x5FFF)
    output logic                                                  tm_wr_en,
    output logic                                                  tm_wr_bank,
    output logic [$clog2(arcade_regs_pkg::TILEMAP_BANK_SIZE)-1:0] tm_wr_addr,
    output logic [                                           7:0] tm_wr_data,
    output logic                                                  swap_req,     // 1-clock pulse
    input  logic                                                  front_bank,
    input  logic                                                  swap_pending,

    // Pattern RAM write port to tile_renderer_pipeline (TBL-TilePattern, §3.1.4 0x6000)
    output logic                                                               pat_wr_en,
    output logic [$clog2(arcade_regs_pkg::TILE_PATTERN_RAM_SIZE * 8 / 32)-1:0] pat_wr_addr,
    output logic [                                                       31:0] pat_wr_data,
    output logic [                                                        3:0] pat_wr_strb,

    // Palette write port (TBL-Palette, §3.1.4 0x8000)
    output logic                                             pal_wr_en,
    output logic [$clog2(arcade_regs_pkg::PALETTE_SIZE)-1:0] pal_wr_addr,
    output logic [                                     31:0] pal_wr_data,

    // Frame timing and status inputs
    input logic vblank_start,  // 1-clock pulse from the VGA Controller (SR-VGA-08)
    input logic fpga_ready,    // Status bit 1 (A7)

    // Control Register outputs (§3.1.4)
    output logic reset_tilemap,  // bit 1 (SR-REN-07)
    output logic display_enable, // bit 2 (SR-HAL-13, SR-VGA-09)

    // Audio outputs (§3.1.4, SR-AUD-04, SR-AUD-05)
    output logic [15:0] tone_frequency,
    output logic [15:0] tone_duration,
    output logic        tone_start,      // 1-clock pulse on a Tone Duration write (A6)
    output logic        audio_enable,    // Audio Control bit 0 (SR-HAL-10)
    output logic        audio_mute       // Audio Control bit 1 (SR-HAL-12)
);
  import arcade_regs_pkg::*;

  localparam int TM_ADDR_W = $clog2(TILEMAP_BANK_SIZE);  // SR-REN-01
  localparam int PAT_ADDR_W = $clog2(TILE_PATTERN_RAM_SIZE * 8 / 32);  // SR-REN-04
  localparam int PAL_ADDR_W = $clog2(PALETTE_SIZE);  // SR-REN-05
  localparam int BADDR_W = 16;  // A1: 64 KB span

  // §3.1.4 offsets, truncated to the 64 KB span
  localparam logic [BADDR_W-1:0] A_BANK1 = OFFSET_TILEMAP_BANK1[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_PATTERN = OFFSET_TILE_PATTERN_RAM[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_PALETTE = OFFSET_PALETTE_BASE[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_CONTROL = OFFSET_CONTROL_REG[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_STATUS = OFFSET_STATUS_REG[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_FRAME = OFFSET_FRAME_COUNTER[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_SWAP = OFFSET_SWAP_REQ_FRONT_BANK[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_ID = OFFSET_ID_VERSION[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_TONE_F = OFFSET_TONE_FREQUENCY[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_TONE_D = OFFSET_TONE_DURATION[BADDR_W-1:0];
  localparam logic [BADDR_W-1:0] A_AUDIO = OFFSET_AUDIO_CONTROL[BADDR_W-1:0];
  // TBL-Palette: PALETTE_SIZE entries of BYTES_PER_PALETTE_ENTRY bytes from Palette Base
  localparam logic [BADDR_W-1:0] A_PALETTE_END = BADDR_W'(OFFSET_PALETTE_BASE +
                                                         PALETTE_SIZE * BYTES_PER_PALETTE_ENTRY);

  logic [BADDR_W-1:0] baddr;
  assign baddr = {avs_address, 2'b00};

  // ---------------------------------------------------------------------
  // Tilemap writes, one cell per clock (A3)
  // ---------------------------------------------------------------------
  logic [3:0] done_q;  // lanes of the held write already written
  logic [3:0] todo, rest;
  logic [1:0] lane;
  logic is_tm_wr;
  logic [BADDR_W-1:0] cell_baddr;

  assign is_tm_wr = avs_write && (baddr < A_PATTERN);
  assign todo = avs_byteenable & ~done_q;

  always_comb begin
    lane = 2'd0;
    for (int i = 3; i >= 0; i--) if (todo[i]) lane = 2'(i);
  end

  assign rest = todo & ~(4'b0001 << lane);
  assign avs_waitrequest = is_tm_wr && (rest != 4'b0000);
  assign cell_baddr = baddr | BADDR_W'(lane);

  always_ff @(posedge clk) begin
    if (rst || !avs_waitrequest) done_q <= 4'b0000;
    else done_q <= done_q | (4'b0001 << lane);
  end

  // Bank 1 starts at 0x3000, a word boundary, so all lanes of one word are in one bank.
  assign tm_wr_bank = (baddr >= A_BANK1);
  assign tm_wr_en = is_tm_wr && (todo != 4'b0000);
  assign tm_wr_addr = TM_ADDR_W'(tm_wr_bank ? (cell_baddr - A_BANK1) : cell_baddr);
  assign tm_wr_data = avs_writedata[8*lane+:8];

  // ---------------------------------------------------------------------
  // Pattern RAM and palette writes (§3.1.4)
  // ---------------------------------------------------------------------
  assign pat_wr_en = avs_write && (baddr >= A_PATTERN) && (baddr < A_PALETTE);
  assign pat_wr_addr = PAT_ADDR_W'((baddr - A_PATTERN) >> 2);
  assign pat_wr_data = avs_writedata;
  assign pat_wr_strb = avs_byteenable;
  assign pal_wr_en = avs_write && (baddr >= A_PALETTE) && (baddr < A_PALETTE_END);
  assign pal_wr_addr = PAL_ADDR_W'((baddr - A_PALETTE) >> 2);
  assign pal_wr_data = avs_writedata;  // A5

  // ---------------------------------------------------------------------
  // Registers
  // ---------------------------------------------------------------------
  logic [31:0] control_q, audio_q, frame_q;
  logic [15:0] tone_f_q, tone_d_q;
  logic vsync_q;

  // Byte-enable merge of a write into a 32-bit register.
  function automatic logic [31:0] merge(input logic [31:0] old, input logic [31:0] data,
                                        input logic [3:0] be);
    for (int i = 0; i < 4; i++) if (be[i]) old[8*i+:8] = data[8*i+:8];
    return old;
  endfunction

  // 16-bit registers (Tone Frequency, Tone Duration) use byte lanes 0 and 1 only.
  function automatic logic [15:0] merge16(input logic [15:0] old, input logic [31:0] data,
                                          input logic [3:0] be);
    for (int i = 0; i < 2; i++) if (be[i]) old[8*i+:8] = data[8*i+:8];
    return old;
  endfunction

  logic wr_control, wr_tone_f, wr_tone_d, wr_audio, rd_status;
  assign wr_control = avs_write && (baddr == A_CONTROL);
  assign wr_tone_f  = avs_write && (baddr == A_TONE_F);
  assign wr_tone_d  = avs_write && (baddr == A_TONE_D);
  assign wr_audio   = avs_write && (baddr == A_AUDIO);
  assign rd_status  = avs_read && (baddr == A_STATUS);

  always_ff @(posedge clk) begin
    if (rst) begin
      control_q <= RESET_CONTROL_REG;  // §3.1.4
      audio_q   <= RESET_AUDIO_CONTROL;  // §3.1.4
      tone_f_q  <= RESET_TONE_FREQUENCY;  // §3.1.4
      tone_d_q  <= RESET_TONE_DURATION;  // §3.1.4
      frame_q   <= RESET_FRAME_COUNTER;  // §3.1.4
      vsync_q   <= RESET_STATUS_REG[BIT_STATUS_VSYNC];  // §3.1.4
    end else begin
      if (wr_control) control_q <= merge(control_q, avs_writedata, avs_byteenable);
      if (wr_audio) audio_q <= merge(audio_q, avs_writedata, avs_byteenable);
      if (wr_tone_f) tone_f_q <= merge16(tone_f_q, avs_writedata, avs_byteenable);
      if (wr_tone_d) tone_d_q <= merge16(tone_d_q, avs_writedata, avs_byteenable);
      // §3.1.4: Frame Counter increments once per displayed frame (one vblank per frame).
      if (vblank_start) frame_q <= frame_q + 32'd1;
      // §3.1.4 Status bit 0: vsync flag set at vblank start, cleared on read. A set in the
      // same clock as the read wins, so a vblank is never lost.
      if (vblank_start) vsync_q <= 1'b1;
      else if (rd_status) vsync_q <= 1'b0;
    end
  end

  // §3.1.4 0x804C: writing 1 to bit 0 requests a swap (SR-REN-03, applied by tilemap_banks).
  assign swap_req = avs_write && (baddr == A_SWAP) && avs_byteenable[0] &&
                    avs_writedata[BIT_SWAP_REQUEST];
  assign tone_start = wr_tone_d;  // A6

  logic [31:0] status;
  always_comb begin
    status                          = RESET_STATUS_REG;
    status[BIT_STATUS_VSYNC]        = vsync_q;
    status[BIT_STATUS_FPGA_READY]   = fpga_ready;
    status[BIT_STATUS_SWAP_PENDING] = swap_pending;
  end

  // Read mux, registered: fixed read latency 1 (A3). Memory regions read 0 (A4).
  logic [31:0] rdata_d, rdata_q;
  always_comb begin
    rdata_d = 32'h0000_0000;
    unique case (baddr)
      A_CONTROL: rdata_d = control_q;
      A_STATUS:  rdata_d = status;
      A_FRAME:   rdata_d = frame_q;
      A_SWAP:    rdata_d[BIT_FRONT_BANK] = front_bank;
      A_ID:      rdata_d = RESET_ID_VERSION;  // §3.1.4: fixed magic 0xA2CADE01
      A_TONE_F:  rdata_d = {16'h0000, tone_f_q};
      A_TONE_D:  rdata_d = {16'h0000, tone_d_q};
      A_AUDIO:   rdata_d = audio_q;
      default:   rdata_d = 32'h0000_0000;
    endcase
  end

  always_ff @(posedge clk) begin
    if (rst) rdata_q <= 32'h0000_0000;
    else if (avs_read) rdata_q <= rdata_d;
  end

  assign avs_readdata   = rdata_q;
  assign reset_tilemap  = control_q[BIT_CONTROL_RESET_TILEMAP];
  assign display_enable = control_q[BIT_CONTROL_DISPLAY_ENABLE];
  assign tone_frequency = tone_f_q;
  assign tone_duration  = tone_d_q;
  assign audio_enable   = audio_q[BIT_AUDIO_ENABLE];
  assign audio_mute     = audio_q[BIT_AUDIO_MUTE];

endmodule : bridge_regs
