// Self-checking testbench for tile_renderer_pipeline (R-4.2.10). Refs #30.
// Verifies SR-REN-04, SR-REN-05, SR-REN-06, TBL-TilePattern, TBL-Palette, CON-05, CON-06:
//   - colour == palette[pattern[tile][row][x]] for 4 tiles x 8 rows x all 8 pixel positions,
//     so every nibble of every byte lane is read, low nibble = left pixel (TBL-TilePattern);
//   - all 16 palette entries (including entry 0) are used, unpacked as 0x00RRGGBB (CON-06);
//   - a back-to-back stream, one new pixel and one new tile_index per cycle, crossing tile
//     boundaries: all four stages busy, one pixel per cycle (SR-REN-06);
//   - latency is exactly 4 cycles, and HSYNC, VSYNC and blank are delayed by the same 4
//     (SR-REN-06): every check compares against the input of exactly 4 cycles earlier, with
//     sync and blank changing pseudo-randomly, so any other latency fails;
//   - RGB is black while blank_out is 1;
//   - byte enables on the pattern write port change only the selected byte.
module tb_tile_renderer_pipeline;

  // verilog_lint: waive-start parameter-name-style (R-4.2.3: UPPER_SNAKE_CASE)
  localparam int LATENCY = 4;  // SR-REN-06: N = 4
  localparam int TILE_SIZE = 8;  // SR-REN-04, App. A TILE_SIZE
  localparam int NUM_TILES_USED = 4;
  localparam int PATTERN_WORDS = arcade_regs_pkg::TILE_PATTERN_RAM_SIZE / 4;  // SR-REN-04: 2048
  localparam int PALETTE_SIZE = arcade_regs_pkg::PALETTE_SIZE;  // SR-REN-05
  // verilog_lint: waive-stop parameter-name-style

  logic clk = 1'b0;
  logic rst;
  logic [9:0] vga_x, vga_y;
  logic hsync_in, vsync_in, blank_in;
  logic [7:0] tile_index;
  logic pat_wr_en;
  logic [10:0] pat_wr_addr;
  logic [31:0] pat_wr_data;
  logic [3:0] pat_wr_strb;
  logic pal_wr_en;
  logic [3:0] pal_wr_addr;
  logic [31:0] pal_wr_data;
  logic [7:0] vga_r, vga_g, vga_b;
  logic hsync_out, vsync_out, blank_out;

  int errors = 0;
  int checks = 0;

  // Reference model of the two RAMs.
  logic [31:0] pat_model[PATTERN_WORDS];
  logic [31:0] pal_model[PALETTE_SIZE];

  // Tiles placed side by side on one row of the screen: 0, 7, 128 and 255 (first, small,
  // top address bit set, last).
  logic [7:0] tiles_used[NUM_TILES_USED] = '{8'd0, 8'd7, 8'd128, 8'd255};

  // Expected output for each input cycle; checked LATENCY cycles later.
  typedef struct packed {
    logic [23:0] rgb;
    logic hs, vs, bl;
  } expect_t;
  expect_t expected[$];

  function automatic expect_t mk_expect(input logic [23:0] rgb, input logic hs, input logic vs,
                                        input logic bl);
    expect_t e;
    e.rgb = rgb;
    e.hs  = hs;
    e.vs  = vs;
    e.bl  = bl;
    return e;
  endfunction

  logic [15:0] lfsr = 16'hACE1;

  tile_renderer_pipeline dut (.*);

  always #5 clk = ~clk;

  // Inputs change on the falling edge, so the DUT samples them cleanly on the rising edge.
  task automatic write_palette(input int idx, input logic [31:0] rgb);
    @(negedge clk);
    pal_wr_en   = 1'b1;
    pal_wr_addr = 4'(idx);
    pal_wr_data = rgb;
    @(negedge clk);
    pal_wr_en = 1'b0;
    pal_model[idx] = rgb;
  endtask

  task automatic write_pattern(input int word, input logic [31:0] data, input logic [3:0] strb);
    @(negedge clk);
    pat_wr_en   = 1'b1;
    pat_wr_addr = 11'(word);
    pat_wr_data = data;
    pat_wr_strb = strb;
    @(negedge clk);
    pat_wr_en = 1'b0;
    for (int b = 0; b < 4; b++) if (strb[b]) pat_model[word][8*b+:8] = data[8*b+:8];
  endtask

  function automatic logic [23:0] model_rgb(input logic [7:0] tile, input int row, input int x);
    logic [3:0] idx;
    idx = pat_model[{tile, 3'(row)}][4*x+:4];  // TBL-TilePattern: low nibble = left pixel
    return pal_model[idx][23:0];  // TBL-Palette: 0x00RRGGBB
  endfunction

  // Compare the DUT outputs with the input of exactly LATENCY cycles ago (SR-REN-06).
  task automatic check_output(input string what);
    expect_t e;
    logic [23:0] want_rgb;
    if (expected.size() < LATENCY) return;
    e = expected.pop_front();
    want_rgb = e.bl ? 24'h0 : e.rgb;
    checks++;
    if ({vga_r, vga_g, vga_b} !== want_rgb || hsync_out !== e.hs || vsync_out !== e.vs ||
        blank_out !== e.bl) begin
      errors++;
      $display("FAIL: %s: want RGB %06h H=%b V=%b B=%b, got %02h%02h%02h H=%b V=%b B=%b", what,
               want_rgb, e.hs, e.vs, e.bl, vga_r, vga_g, vga_b, hsync_out, vsync_out, blank_out);
    end
  endtask

  // Streams one screen row segment: x = 0 .. 8*NUM_TILES_USED-1 at screen line y, a new pixel
  // every cycle, then one blanked idle pixel. tile_index for a pixel is presented one cycle after its x/y (stage 1).
  // blank_pct sets how often blank is asserted.
  task automatic stream_row(input int y, input int blank_pct);
    int n = TILE_SIZE * NUM_TILES_USED;
    for (int c = 0; c <= n; c++) begin
      @(negedge clk);
      check_output($sformatf("y=%0d stream cycle %0d", y, c));
      // Stage 1 tile index for the pixel driven in the previous cycle.
      if (c > 0) tile_index = tiles_used[(c-1)/TILE_SIZE];
      if (c < n) begin
        lfsr = {lfsr[14:0], lfsr[15] ^ lfsr[13] ^ lfsr[12] ^ lfsr[10]};
        vga_x = 10'(c);
        vga_y = 10'(y);
        hsync_in = lfsr[0];
        vsync_in = lfsr[1];
        blank_in = (int'(lfsr[15:9]) % 100) < blank_pct;
        expected.push_back(mk_expect(
                           model_rgb(
                               tiles_used[c/TILE_SIZE], y % TILE_SIZE, c % TILE_SIZE
                           ),
                           hsync_in,
                           vsync_in,
                           blank_in
                           ));
      end else begin
        // One blanked idle pixel after the segment, so streams can follow each other.
        hsync_in = 1'b0;
        vsync_in = 1'b0;
        blank_in = 1'b1;
        expected.push_back(mk_expect(24'h0, 1'b0, 1'b0, 1'b1));
      end
    end
  endtask

  // Drain the pipeline: keep checking while idle (blanked) pixels flow in.
  task automatic drain();
    for (int c = 0; c < LATENCY + 1; c++) begin
      @(negedge clk);
      check_output("drain");
      vga_x = '0;
      vga_y = '0;
      hsync_in = 1'b0;
      vsync_in = 1'b0;
      blank_in = 1'b1;
      tile_index = '0;
      expected.push_back(mk_expect(24'h0, 1'b0, 1'b0, 1'b1));
    end
    expected.delete();
  endtask

  initial begin
    rst = 1'b1;
    vga_x = '0;
    vga_y = '0;
    hsync_in = 1'b0;
    vsync_in = 1'b0;
    blank_in = 1'b1;
    tile_index = '0;
    pat_wr_en = 1'b0;
    pat_wr_addr = '0;
    pat_wr_data = '0;
    pat_wr_strb = '0;
    pal_wr_en = 1'b0;
    pal_wr_addr = '0;
    pal_wr_data = '0;
    repeat (3) @(negedge clk);
    rst = 1'b0;

    // Reset values: blank active, syncs low, black.
    if (blank_out !== 1'b1 || hsync_out !== 1'b0 || vsync_out !== 1'b0 ||
        {vga_r, vga_g, vga_b} !== 24'h0) begin
      errors++;
      $display("FAIL: reset values: B=%b H=%b V=%b RGB=%02h%02h%02h", blank_out, hsync_out,
               vsync_out, vga_r, vga_g, vga_b);
    end

    // SR-REN-05, CON-06: all 16 entries, R, G and B all different so a swapped channel fails.
    for (int i = 0; i < PALETTE_SIZE; i++) begin
      write_palette(i, {8'h00, 8'(16 + i), 8'(80 + 3 * i), 8'(160 + 5 * i)});
    end

    // SR-REN-04, TBL-TilePattern: pixel x of row r of tile t uses palette index
    // (t + 3r + x) mod 16, so each row holds 8 different nibbles and all 16 indices appear.
    for (int t = 0; t < NUM_TILES_USED; t++) begin
      for (int r = 0; r < TILE_SIZE; r++) begin
        logic [31:0] row;
        for (int x = 0; x < TILE_SIZE; x++) row[4*x+:4] = 4'(int'(tiles_used[t]) + 3 * r + x);
        write_pattern(int'({tiles_used[t], 3'(r)}), row, 4'b1111);
      end
    end

    // Back-to-back stream over every row of the four tiles; blank mostly off (SR-REN-06).
    for (int y = 0; y < TILE_SIZE; y++) stream_row(y, 10);
    // A line in another tile row of the screen: only y mod 8 selects the pattern row.
    stream_row(8 * 41 + 5, 10);
    // A stream with blank on about half the time: RGB must be black whenever blank_out is 1.
    stream_row(2, 50);
    drain();

    // Byte enables: overwrite only byte 2 of tile 7 row 4, then check that row again.
    write_pattern(int'({8'd7, 3'd4}), 32'hDEAD_BEEF, 4'b0100);
    stream_row(4, 0);
    drain();

    if (checks < 9 * TILE_SIZE * NUM_TILES_USED) begin
      errors++;
      $display("FAIL: only %0d output checks ran", checks);
    end
    $display("tb_tile_renderer_pipeline: %0d output checks, %0d errors", checks, errors);
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end
endmodule
