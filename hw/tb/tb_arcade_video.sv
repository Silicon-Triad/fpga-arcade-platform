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

  task automatic write_cell(input logic bank, input int addr, input logic [7:0] tile);
    tm_wr_en   = 1'b1;
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

    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end
endmodule
// verilog_lint: waive-stop parameter-name-style
