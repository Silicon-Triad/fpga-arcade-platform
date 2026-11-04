// Self-checking testbench for arcade_video (CON-01, CON-03, CON-04). Refs #33.
// Checks the whole video path end to end, from the bridge_regs write ports to the VGA pins:
//   1. After reset the tilemap clear (SR-REN-07) runs and fpga_ready (Status bit 1, B1) rises
//      when it ends.
//   2. With display enable = 0 every active pixel is black (SR-VGA-09).
//   3. With display enable = 1, a tile written into the front bank, its pattern and its
//      palette entry appear at the right place on the pins: cell (0,0) holds tile 1, whose
//      8x8 pattern uses palette index 1 (TBL-TilePattern, TBL-Palette, SR-REN-04..06,
//      SR-VGA-07); cell (1,0) holds tile 0, all palette index 0.
//   4. vblank_start pulses once per frame and VGA_SYNC_N stays 0 (SR-VGA-06, SR-VGA-08).
//   5. A swap request flips the front bank at the next vblank_start (SR-REN-03), and the
//      other bank's contents are then shown.
//   6. PR-05 frame-update latency (Refs #32): the whole back bank is written and a swap is
//      requested halfway through a frame. Every pixel of the rest of that frame still shows
//      the old front bank, every pixel of the first frame scanned out after the swap matches
//      the new tilemap, and that frame is complete on the pins within 16.7 ms of the swap.
// The colour values below are test data, not SRS values.

// verilog_lint: waive-start parameter-name-style
module tb_arcade_video;
  import arcade_regs_pkg::*;

  localparam int TM_ADDR_W = $clog2(TILEMAP_BANK_SIZE);
  localparam int PAT_ADDR_W = $clog2(TILE_PATTERN_RAM_SIZE * 8 / 32);
  localparam int PAL_ADDR_W = $clog2(PALETTE_SIZE);
  localparam int PAT_WORDS_PER_TILE = BYTES_PER_TILE_PATTERN / 4;  // 8 rows of 32 bits
  localparam int TILE_PX = 8;  // CON-05: 8x8 px tiles

  localparam bit [31:0] COLOUR_A = 32'h0012_3456;  // test colour for palette index 1
  localparam bit [31:0] COLOUR_B = 32'h00AB_CDEF;  // test colour for palette index 2

  // Section 6 (PR-05). The DUT runs in its default VIDEO_MODE, XGA 1024x768 (SR-VGA-11):
  // GRID_COLS x GRID_ROWS cells of TILE_PX x TILE_PX pixels, all displayed.
  localparam int FRAME_W = GRID_COLS * TILE_PX;  // 1024 active pixels per line (SR-VGA-02)
  localparam int FRAME_H = GRID_ROWS * TILE_PX;  // 768 active lines per frame (SR-VGA-03)
  localparam longint PIXEL_CLOCK_HZ = 65_000_000;  // SR-VGA-01: 65.000 MHz
  localparam longint PR05_LIMIT_US = 16_700;  // PR-05: within 16.7 ms of the swap
  // PR-05 limit in pixel clocks: 16.7 ms x 65.000 MHz = 1,085,500
  localparam longint PR05_LIMIT_CLKS = PIXEL_CLOCK_HZ / 1_000_000 * PR05_LIMIT_US;

  logic clk = 1'b0;
  always #1 clk = ~clk;

  logic rst;
  logic tm_wr_en, tm_wr_bank, swap_req, front_bank, swap_pending;
  logic [TM_ADDR_W-1:0] tm_wr_addr;
  logic [7:0] tm_wr_data;
  logic pat_wr_en, pal_wr_en;
  logic [PAT_ADDR_W-1:0] pat_wr_addr;
  logic [PAL_ADDR_W-1:0] pal_wr_addr;
  logic [31:0] pat_wr_data, pal_wr_data;
  logic [3:0] pat_wr_strb;
  logic vblank_start, fpga_ready, reset_tilemap, display_enable;
  logic [7:0] vga_r, vga_g, vga_b;
  logic vga_hs, vga_vs, vga_blank_n, vga_sync_n;

  int errors = 0;

  arcade_video dut (
      .clk,
      .rst,
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
      .VGA_R(vga_r),
      .VGA_G(vga_g),
      .VGA_B(vga_b),
      .VGA_HS(vga_hs),
      .VGA_VS(vga_vs),
      .VGA_BLANK_N(vga_blank_n),
      .VGA_SYNC_N(vga_sync_n)
  );

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

  // Testbench copy of both tilemap banks. Both start at 0: the SR-REN-07 clear after reset.
  logic [7:0] model[2][TILEMAP_BANK_SIZE];
  initial
    for (int b = 0; b < 2; b++) for (int a = 0; a < TILEMAP_BANK_SIZE; a++) model[b][a] = 8'd0;

  task automatic write_cell(input logic bank, input int addr, input logic [7:0] tile);
    model[bank][addr] = tile;
    tm_wr_en = 1'b1;
    tm_wr_bank = bank;
    tm_wr_addr = TM_ADDR_W'(addr);
    tm_wr_data = tile;
    tick();
    tm_wr_en = 1'b0;
  endtask

  // Fill one tile's 8 pattern rows with a single palette index in every nibble.
  task automatic write_tile(input int tile, input logic [3:0] index);
    for (int row = 0; row < PAT_WORDS_PER_TILE; row++) begin
      pat_wr_en   = 1'b1;
      pat_wr_addr = PAT_ADDR_W'(tile * PAT_WORDS_PER_TILE + row);
      pat_wr_data = {8{index}};
      pat_wr_strb = 4'hF;
      tick();
    end
    pat_wr_en = 1'b0;
  endtask

  task automatic write_palette(input int index, input logic [31:0] colour);
    pal_wr_en   = 1'b1;
    pal_wr_addr = PAL_ADDR_W'(index);
    pal_wr_data = colour;
    tick();
    pal_wr_en = 1'b0;
  endtask

  task automatic wait_vblank();
    @(posedge clk iff vblank_start);
    @(negedge clk);
  endtask

  // Capture the first TILE_PX * 2 active pixels of the next frame (row 0, cells 0 and 1).
  logic [23:0] line0[TILE_PX*2];
  task automatic capture_first_pixels();
    int n = 0;
    wait_vblank();
    // vblank_start comes at timing cycle 0; the last pixels of line 767 are still in the
    // renderer and output registers, so wait until the pins are blanking.
    while (vga_blank_n) @(negedge clk);
    // Sample on the falling edge, half a clock after the pins change, so the testbench never
    // races the output registers.
    while (n < TILE_PX * 2) begin
      @(negedge clk);
      if (vga_blank_n) begin
        line0[n] = {vga_r, vga_g, vga_b};
        n++;
      end
    end
  endtask

  // Monitors running the whole time: SR-VGA-06 and no colour while blanking (SR-VGA-05).
  always @(posedge clk) begin
    if (!rst) begin
      if (vga_sync_n !== 1'b0) begin
        $display("FAIL: SR-VGA-06 VGA_SYNC_N is not 0");
        errors++;
      end
    end
  end

  int vblanks = 0;
  always @(posedge clk) if (vblank_start) vblanks++;

  longint cycle = 0;
  always @(posedge clk) cycle++;

  // Colour of a solid test tile: tile 1 = palette 1, tile 2 = palette 2, others palette 0.
  function automatic logic [23:0] tile_colour(input logic [7:0] tile);
    case (tile)
      8'd1: return COLOUR_A[23:0];
      8'd2: return COLOUR_B[23:0];
      default: return 24'h0;
    endcase
  endfunction

  // Section 6 (PR-05) pin monitor. Counts frames and active pixels as they leave the VGA pins:
  // a new frame starts when VGA_VS goes low (it is active low, SR-VGA-04, and comes between
  // two active regions). While chk_on, every active pixel of a frame >= chk_first_frame is
  // compared with the bank model: frames up to swap_frame must show old_bank, later frames
  // new_bank. For frame swap_frame + 1, the first frame after the swap, the cycle of its first
  // and last active pixel is recorded.
  int   pin_frame = 0;
  int   pin_px = 0;
  logic vga_vs_q = 1'b1;
  logic chk_on = 1'b0;
  int   chk_first_frame = 0;
  int   swap_frame = 32'h7FFF_FFFF;
  logic old_bank = 1'b0, new_bank = 1'b0;
  int pr05_mismatches = 0;
  int new_frame_px = 0;
  longint new_first_cycle = 0, new_last_cycle = 0;

  always @(negedge clk) begin
    if (vga_vs_q && !vga_vs) begin
      pin_frame++;
      pin_px = 0;
    end
    vga_vs_q = vga_vs;
    if (vga_blank_n) begin
      if (chk_on && pin_frame >= chk_first_frame) begin
        int x, y, cell_idx;
        logic bank;
        logic [23:0] exp;
        x = pin_px % FRAME_W;
        y = pin_px / FRAME_W;
        cell_idx = (y / TILE_PX) * GRID_COLS + x / TILE_PX;  // TBL-Tilemap: row * GRID_COLS + col
        bank = (pin_frame <= swap_frame) ? old_bank : new_bank;
        exp = tile_colour(model[bank][cell_idx]);
        if ({vga_r, vga_g, vga_b} !== exp) begin
          if (pr05_mismatches < 5)
            $display(
                "FAIL: PR-05 frame %0d pixel (%0d,%0d) = %06h, expected %06h (bank %0d)",
                pin_frame,
                x,
                y,
                {
                  vga_r, vga_g, vga_b
                },
                exp,
                bank
            );
          pr05_mismatches++;
        end
        if (pin_frame == swap_frame + 1) begin
          if (new_frame_px == 0) new_first_cycle = cycle;
          new_last_cycle = cycle;
          new_frame_px++;
        end
      end
      pin_px++;
    end
  end

  initial begin
    {tm_wr_en, tm_wr_bank, swap_req, pat_wr_en, pal_wr_en} = '0;
    {reset_tilemap, display_enable} = '0;
    tm_wr_addr = '0;
    tm_wr_data = '0;
    pat_wr_addr = '0;
    pat_wr_data = '0;
    pat_wr_strb = '0;
    pal_wr_addr = '0;
    pal_wr_data = '0;

    rst = 1'b1;
    repeat (4) tick();
    check(!fpga_ready, "B1: fpga_ready must be low during reset");
    rst = 1'b0;

    // 1. The SR-REN-07 clear runs after reset; fpga_ready rises when it ends.
    tick();
    check(!fpga_ready, "B1: fpga_ready must be low while the tilemap clear runs");
    repeat (TILEMAP_BANK_SIZE + 4) tick();
    check(fpga_ready, "B1: fpga_ready must be high after the tilemap clear");

    // Test data: palette 1 = COLOUR_A, palette 2 = COLOUR_B, palette 0 left black.
    // Tile 1 is all index 1, tile 2 all index 2, tile 0 left all index 0.
    write_palette(1, COLOUR_A);
    write_palette(2, COLOUR_B);
    write_tile(1, 4'd1);
    write_tile(2, 4'd2);
    write_cell(1'b0, 0, 8'd1);  // bank 0 (front after reset), cell (0,0) = tile 1
    write_cell(1'b1, 0, 8'd2);  // bank 1 (back), cell (0,0) = tile 2
    check(front_bank == 1'b0, "SR-REN-03: bank 0 must be the front bank after reset");

    // 2. Display enable = 0: active pixels are black (SR-VGA-09).
    capture_first_pixels();
    for (int i = 0; i < TILE_PX * 2; i++)
    check(line0[i] == 24'h0, $sformatf("SR-VGA-09: pixel %0d not black with display off", i));

    // 3. Display enable = 1: cell (0,0) shows COLOUR_A, cell (1,0) shows palette 0 (black).
    display_enable = 1'b1;
    capture_first_pixels();
    for (int i = 0; i < TILE_PX; i++)
    check(line0[i] == COLOUR_A[23:0], $sformatf(
          "SR-REN-06: pixel %0d = %06h, expected %06h", i, line0[i], COLOUR_A[23:0]));
    for (int i = TILE_PX; i < TILE_PX * 2; i++)
    check(line0[i] == 24'h0, $sformatf("SR-REN-06: pixel %0d = %06h, expected black", i, line0[i]));

    // 4. One vblank_start per frame (SR-VGA-08).
    begin
      int vb_start;
      vb_start = vblanks;
      wait_vblank();
      wait_vblank();
      check(vblanks - vb_start == 2, "SR-VGA-08: expected one vblank_start per frame");
    end

    // 5. Swap request: front bank flips at the next vblank_start; bank 1's tile 2 is shown.
    swap_req = 1'b1;
    tick();
    swap_req = 1'b0;
    check(swap_pending, "SR-REN-03: swap_pending must be set after a swap request");
    wait_vblank();
    tick();
    check(front_bank == 1'b1, "SR-REN-03: front bank must be 1 after the swap");
    check(!swap_pending, "SR-REN-03: swap_pending must clear once the swap is applied");
    capture_first_pixels();
    for (int i = 0; i < TILE_PX; i++)
    check(line0[i] == COLOUR_B[23:0], $sformatf(
          "SR-REN-03: pixel %0d = %06h after swap, expected %06h", i, line0[i], COLOUR_B[23:0]));

    // 6. PR-05: write the back bank and request a swap halfway through a frame; the first
    //    frame scanned out after the swap must match the new tilemap, within 16.7 ms.
    begin
      longint swap_cycle;
      old_bank = front_bank;  // bank 1 after section 5
      new_bank = !front_bank;
      check(!swap_pending, "PR-05: no swap may be pending before the test");
      // Start checking at the next frame on the pins, from its first pixel.
      wait_vblank();
      chk_first_frame = pin_frame + 1;
      chk_on = 1'b1;
      while (!(pin_frame == chk_first_frame && pin_px >= FRAME_W * FRAME_H / 2)) @(negedge clk);
      // Halfway through frame chk_first_frame: rewrite every cell of the back bank with a
      // pattern that differs from cell to cell, then request the swap, as present() does
      // (SR-HAL-08). Two thirds of the cells differ from the old front bank, so a missed or
      // early swap shows up as a pixel mismatch.
      swap_frame = pin_frame;
      for (int r = 0; r < GRID_ROWS; r++)
      for (int c = 0; c < GRID_COLS; c++)
      write_cell(new_bank, r * GRID_COLS + c, 8'((c + 2 * r) % 3));
      check(pin_frame == swap_frame, "PR-05: back-bank writes must finish inside the frame");
      check(front_bank == old_bank, "PR-05: writing the back bank must not change the front bank");
      swap_req = 1'b1;
      tick();
      swap_req = 1'b0;
      check(swap_pending, "PR-05: swap_pending must be set after the swap request");
      // The swap happens on the next vblank_start (SR-REN-03, SR-VGA-08).
      @(posedge clk iff vblank_start);
      swap_cycle = cycle;
      @(negedge clk);
      tick();
      check(front_bank == new_bank, "PR-05: front bank must flip at the vblank after the request");
      // Wait until the first frame after the swap has left the pins.
      while (pin_frame <= swap_frame + 1) @(negedge clk);
      chk_on = 1'b0;
      check(new_frame_px == FRAME_W * FRAME_H, $sformatf(
            "PR-05: saw %0d active pixels in the frame after the swap, expected %0d",
            new_frame_px,
            FRAME_W * FRAME_H
            ));
      check(pr05_mismatches == 0, $sformatf("PR-05: %0d pixels did not match", pr05_mismatches));
      check(new_first_cycle > swap_cycle, "PR-05: new frame must start after the swap");
      check(new_last_cycle - swap_cycle <= PR05_LIMIT_CLKS, $sformatf(
            "PR-05: frame complete %0d clocks after the swap, limit %0d (16.7 ms)",
            new_last_cycle - swap_cycle,
            PR05_LIMIT_CLKS
            ));
      $display("PR-05: swap at cycle %0d; new frame first pixel +%0d, last pixel +%0d clocks",
               swap_cycle, new_first_cycle - swap_cycle, new_last_cycle - swap_cycle);
      $display("PR-05: last pixel %0d us after the swap at 65.000 MHz, limit %0d us",
               (new_last_cycle - swap_cycle) * 1_000_000 / PIXEL_CLOCK_HZ, PR05_LIMIT_US);
    end

    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end
endmodule
// verilog_lint: waive-stop parameter-name-style
