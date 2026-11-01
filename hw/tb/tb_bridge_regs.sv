// Self-checking testbench for bridge_regs (§3.1.4, TBL-RegisterMap, SR-REN-03, AS-02). Refs #28.
// bridge_regs is connected to the real tilemap_banks, so Swap Request, swap pending and Front
// Bank are checked end to end. The VGA Controller is not merged yet; vblank_start is driven by
// this testbench (a 1-clock pulse, as SR-VGA-08 specifies).
//
// Acceptance check of #28: every register's reset value (ID = 0xA2CADE01), status bits as
// defined, frame counter increments, swap pending set on write, Front Bank readable.

// verilog_lint: waive-start parameter-name-style
module tb_bridge_regs;
  import arcade_regs_pkg::*;

  localparam int TM_ADDR_W  = $clog2(TILEMAP_BANK_SIZE);
  localparam int PAT_ADDR_W = $clog2(TILE_PATTERN_RAM_SIZE * 8 / 32);
  localparam int PAL_ADDR_W = $clog2(PALETTE_SIZE);

  logic clk = 1'b0;
  always #1 clk = ~clk;

  logic rst;
  logic [13:0] avs_address;
  logic avs_read, avs_write, avs_waitrequest;
  logic [31:0] avs_writedata, avs_readdata;
  logic [3:0] avs_byteenable;

  logic tm_wr_en, tm_wr_bank, swap_req, front_bank, swap_pending, clear_busy;
  logic [TM_ADDR_W-1:0] tm_wr_addr, rd_addr;
  logic [7:0] tm_wr_data, rd_data;
  logic pat_wr_en, pal_wr_en;
  logic [PAT_ADDR_W-1:0] pat_wr_addr;
  logic [PAL_ADDR_W-1:0] pal_wr_addr;
  logic [31:0] pat_wr_data, pal_wr_data;
  logic [3:0] pat_wr_strb;
  logic vblank_start, reset_tilemap, display_enable;
  logic [15:0] tone_frequency, tone_duration;
  logic tone_start, audio_enable, audio_mute;

  int errors = 0;

  bridge_regs dut (
      .clk,
      .rst,
      .avs_address,
      .avs_read,
      .avs_write,
      .avs_writedata,
      .avs_byteenable,
      .avs_readdata,
      .avs_waitrequest,
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
      .fpga_ready(!clear_busy),
      .reset_tilemap,
      .display_enable,
      .tone_frequency,
      .tone_duration,
      .tone_start,
      .audio_enable,
      .audio_mute
  );

  tilemap_banks banks (
      .clk,
      .rst,
      .wr_en  (tm_wr_en),
      .wr_bank(tm_wr_bank),
      .wr_addr(tm_wr_addr),
      .wr_data(tm_wr_data),
      .swap_req,
      .reset_tilemap,
      .vblank_start,
      .rd_addr,
      .rd_data,
      .front_bank,
      .swap_pending,
      .clear_busy
  );

  // Write-port monitors: record the last pattern / palette write and tone_start pulses.
  int pat_writes = 0, pal_writes = 0, tone_starts = 0;
  logic [PAT_ADDR_W-1:0] last_pat_addr;
  logic [31:0] last_pat_data, last_pal_data;
  logic [3:0] last_pat_strb;
  logic [PAL_ADDR_W-1:0] last_pal_addr;
  always @(posedge clk) begin
    if (pat_wr_en) begin
      pat_writes++;
      last_pat_addr = pat_wr_addr;
      last_pat_data = pat_wr_data;
      last_pat_strb = pat_wr_strb;
    end
    if (pal_wr_en) begin
      pal_writes++;
      last_pal_addr = pal_wr_addr;
      last_pal_data = pal_wr_data;
    end
    if (tone_start) tone_starts++;
  end

  task automatic tick();
    @(posedge clk);
    @(negedge clk);
  endtask

  task automatic check(input logic cond, input string msg);
    if (!cond) begin
      $display("FAIL: %s", msg);
      errors++;
    end
  endtask

  // Avalon write; returns the number of clocks waitrequest held it.
  task automatic av_write(input logic [31:0] off, input logic [31:0] data, input logic [3:0] be,
                          output int waits);
    avs_address = off[15:2];
    avs_writedata = data;
    avs_byteenable = be;
    avs_write = 1'b1;
    waits = 0;
    // Avalon: the write is accepted at the first rising edge where waitrequest is low.
    forever begin
      @(posedge clk);
      if (!avs_waitrequest) break;
      waits++;
    end
    @(negedge clk);
    avs_write = 1'b0;
    avs_byteenable = 4'h0;
  endtask

  task automatic wr(input logic [31:0] off, input logic [31:0] data);
    int w;
    av_write(off, data, 4'hF, w);
  endtask

  // Avalon read, fixed latency 1 (assumption A3): data valid the clock after the read.
  task automatic rd(input logic [31:0] off, output logic [31:0] data);
    avs_address = off[15:2];
    check(!avs_waitrequest, "reads must never wait (A3)");
    avs_read = 1'b1;
    tick();
    avs_read = 1'b0;
    data = avs_readdata;
  endtask

  task automatic expect_reg(input logic [31:0] off, input logic [31:0] exp, input string what);
    logic [31:0] got;
    rd(off, got);
    if (got !== exp) begin
      $display("FAIL: %s: read 0x%08h, expected 0x%08h", what, got, exp);
      errors++;
    end
  endtask

  task automatic pulse_vblank();
    vblank_start = 1'b1;
    tick();
    vblank_start = 1'b0;
  endtask

  // Read one cell of the front bank through the renderer port of tilemap_banks.
  task automatic front_cell(input int addr, output logic [7:0] data);
    rd_addr = TM_ADDR_W'(addr);
    tick();
    tick();
    data = rd_data;
  endtask

  logic [31:0] v;
  logic [7:0] c;
  int waits;

  initial begin
    rst = 1'b1;
    avs_address = '0;
    avs_read = 1'b0;
    avs_write = 1'b0;
    avs_writedata = '0;
    avs_byteenable = '0;
    vblank_start = 1'b0;
    rd_addr = '0;
    tick();
    tick();
    rst = 1'b0;

    // ---- §3.1.4 reset values of every register (tilemap clear still running) ----
    expect_reg(OFFSET_CONTROL_REG, RESET_CONTROL_REG, "Control reset");
    expect_reg(OFFSET_STATUS_REG, RESET_STATUS_REG, "Status reset (FPGA ready not yet set)");
    expect_reg(OFFSET_FRAME_COUNTER, RESET_FRAME_COUNTER, "Frame Counter reset");
    expect_reg(OFFSET_SWAP_REQ_FRONT_BANK, RESET_SWAP_REQ_FRONT_BANK, "Front Bank reset");
    expect_reg(OFFSET_ID_VERSION, 32'hA2CA_DE01, "ID / Version");
    expect_reg(OFFSET_ID_VERSION, HAL_ID_MAGIC, "ID / Version = HAL_ID_MAGIC (SR-HAL-09)");
    expect_reg(OFFSET_TONE_FREQUENCY, {16'h0, RESET_TONE_FREQUENCY}, "Tone Frequency reset");
    expect_reg(OFFSET_TONE_DURATION, {16'h0, RESET_TONE_DURATION}, "Tone Duration reset");
    expect_reg(OFFSET_AUDIO_CONTROL, RESET_AUDIO_CONTROL, "Audio Control reset");
    expect_reg(OFFSET_PALETTE_BASE, RESET_PALETTE_ENTRY, "Palette region reads 0 (A4)");
    expect_reg(OFFSET_TILEMAP_BANK0, 32'h0, "Tilemap region reads 0 (A4)");
    expect_reg(OFFSET_TILE_PATTERN_RAM, 32'h0, "Pattern region reads 0 (A4)");
    expect_reg(32'h8060, 32'h0, "unmapped offset reads 0");
    check(!display_enable && !reset_tilemap, "Control outputs low after reset");
    check(!audio_enable && !audio_mute, "Audio outputs low after reset");

    // ---- Status bit 1: FPGA ready once the reset clear is done ----
    while (clear_busy) tick();
    rd(OFFSET_STATUS_REG, v);
    check(v[BIT_STATUS_FPGA_READY], "Status bit 1 (FPGA ready) set after clear");
    check(!v[BIT_STATUS_VSYNC] && !v[BIT_STATUS_SWAP_PENDING], "Status bits 0, 2 still 0");

    // ---- Frame Counter increments once per frame; vsync flag set, then cleared on read ----
    for (int i = 0; i < 3; i++) begin
      pulse_vblank();
      repeat (5) tick();
    end
    expect_reg(OFFSET_FRAME_COUNTER, 32'd3, "Frame Counter after 3 vblanks");
    rd(OFFSET_STATUS_REG, v);
    check(v[BIT_STATUS_VSYNC], "Status bit 0 (vsync flag) set by vblank");
    rd(OFFSET_STATUS_REG, v);
    check(!v[BIT_STATUS_VSYNC], "Status bit 0 cleared on read");

    // ---- Tilemap writes: byte write, and a 4-byte word write held by waitrequest (A3) ----
    av_write(OFFSET_TILEMAP_BANK1 + 32'd100, 32'h0000_5A00, 4'b0010, waits);  // cell 101
    check(waits == 0, "single-byte tilemap write does not wait");
    av_write(OFFSET_TILEMAP_BANK1 + 32'd200, 32'h4433_2211, 4'hF, waits);  // cells 200-203
    check(waits == 3, $sformatf("4-byte tilemap write waits 3 clocks, got %0d", waits));
    av_write(OFFSET_TILEMAP_BANK0 + 32'd100, 32'h0000_7700, 4'b0010, waits);  // bank 0 cell 101

    // ---- Swap Request: write 0 does nothing; write 1 sets swap pending (SR-REN-03) ----
    wr(OFFSET_SWAP_REQ_FRONT_BANK, 32'h0);
    rd(OFFSET_STATUS_REG, v);
    check(!v[BIT_STATUS_SWAP_PENDING], "writing 0 to Swap Request does not request a swap");
    wr(OFFSET_SWAP_REQ_FRONT_BANK, 32'h1 << BIT_SWAP_REQUEST);
    rd(OFFSET_STATUS_REG, v);
    check(v[BIT_STATUS_SWAP_PENDING], "Status bit 2 (swap pending) set on Swap Request");
    expect_reg(OFFSET_SWAP_REQ_FRONT_BANK, 32'h0, "Front Bank still 0 before vblank");
    repeat (10) tick();
    rd(OFFSET_STATUS_REG, v);
    check(v[BIT_STATUS_SWAP_PENDING], "swap pending stays set until vblank (SR-REN-03)");

    pulse_vblank();
    rd(OFFSET_STATUS_REG, v);
    check(!v[BIT_STATUS_SWAP_PENDING], "swap pending cleared once swap applied (SR-REN-03)");
    expect_reg(OFFSET_SWAP_REQ_FRONT_BANK, 32'h1 << BIT_FRONT_BANK, "Front Bank = 1 after swap");

    // Bank 1 is now front: the cells written through the bridge are displayed.
    front_cell(101, c);
    check(c == 8'h5A, $sformatf("bank 1 cell 101 = 0x5A, got 0x%02h", c));
    for (int i = 0; i < 4; i++) begin
      front_cell(200 + i, c);
      check(c == 8'(8'h11 * (i + 1)), $sformatf("bank 1 cell %0d, got 0x%02h", 200 + i, c));
    end
    front_cell(100, c);
    check(c == 8'h00, "bank 1 cell 100 untouched by the byte-lane write");

    // Swap back: bank 0 front, its cell 101 holds 0x77.
    wr(OFFSET_SWAP_REQ_FRONT_BANK, 32'h1);
    pulse_vblank();
    expect_reg(OFFSET_SWAP_REQ_FRONT_BANK, 32'h0, "Front Bank = 0 after second swap");
    front_cell(101, c);
    check(c == 8'h77, $sformatf("bank 0 cell 101 = 0x77, got 0x%02h", c));

    // ---- Pattern RAM and palette write ports (TBL-TilePattern, TBL-Palette) ----
    av_write(OFFSET_TILE_PATTERN_RAM + 32'd20, 32'hCAFE_F00D, 4'b0101, waits);
    check(pat_writes == 1 && last_pat_addr == PAT_ADDR_W'(5), "pattern write to word 5");
    check(last_pat_data == 32'hCAFE_F00D && last_pat_strb == 4'b0101, "pattern data and strb");
    wr(OFFSET_PALETTE_BASE + 32'd12, 32'h00FF_8000);
    check(pal_writes == 1 && last_pal_addr == PAL_ADDR_W'(3), "palette write to entry 3");
    check(last_pal_data == 32'h00FF_8000, "palette data 0x00RRGGBB (CON-06)");
    wr(OFFSET_PALETTE_BASE + 32'd60, 32'h0012_3456);
    check(pal_writes == 2 && last_pal_addr == PAL_ADDR_W'(15), "palette write to entry 15");
    check(pat_writes == 1, "palette writes do not reach the pattern port");

    // ---- Control Register: bits 1, 2 drive outputs; byte enables respected ----
    wr(OFFSET_CONTROL_REG, 32'h1 << BIT_CONTROL_DISPLAY_ENABLE);
    check(display_enable && !reset_tilemap, "Control bit 2 -> display_enable (SR-HAL-13)");
    check(pal_writes == 2, "Control write is not a palette write");
    expect_reg(OFFSET_CONTROL_REG, 32'h1 << BIT_CONTROL_DISPLAY_ENABLE, "Control read back");
    av_write(OFFSET_CONTROL_REG, 32'hFFFF_FF00, 4'b0010, waits);
    expect_reg(OFFSET_CONTROL_REG, 32'h0000_FF04, "Control byte-enable write");

    // ---- Tone and Audio registers (SR-AUD-04, SR-AUD-05, SR-HAL-10, SR-HAL-12) ----
    wr(OFFSET_TONE_FREQUENCY, 32'd440);
    check(tone_frequency == 16'd440 && tone_starts == 0, "Tone Frequency output, no start");
    wr(OFFSET_TONE_DURATION, 32'd100);
    check(tone_duration == 16'd100 && tone_starts == 1, "Tone Duration write pulses tone_start");
    expect_reg(OFFSET_TONE_FREQUENCY, 32'd440, "Tone Frequency read back");
    expect_reg(OFFSET_TONE_DURATION, 32'd100, "Tone Duration read back");
    wr(OFFSET_AUDIO_CONTROL, (32'h1 << BIT_AUDIO_ENABLE) | (32'h1 << BIT_AUDIO_MUTE));
    check(audio_enable && audio_mute, "Audio Control bits 0, 1 drive outputs");
    expect_reg(OFFSET_ID_VERSION, 32'hA2CA_DE01, "ID / Version is read-only");
    wr(OFFSET_ID_VERSION, 32'h0);
    expect_reg(OFFSET_ID_VERSION, 32'hA2CA_DE01, "ID / Version unchanged by a write");

    // ---- Frame Counter keeps counting (5 frames so far) ----
    expect_reg(OFFSET_FRAME_COUNTER, 32'd5, "Frame Counter after 5 vblanks");

    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule : tb_bridge_regs
