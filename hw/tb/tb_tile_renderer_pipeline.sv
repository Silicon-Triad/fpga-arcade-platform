// Covers SR-REN-04, SR-REN-05, SR-REN-06, TBL-TilePattern, TBL-Palette.
`timescale 1ns / 1ps

module tb_tile_renderer_pipeline;

  // Inputs
  logic        clk;
  logic        rst;
  logic [ 9:0] vga_x;
  logic [ 9:0] vga_y;
  logic        hsync_in;
  logic        vsync_in;
  logic        blank_in;
  logic [ 7:0] tile_index;

  logic        pat_wr_en;
  logic [10:0] pat_wr_addr;
  logic [31:0] pat_wr_data;
  logic [ 3:0] pat_wr_strb;

  logic        pal_wr_en;
  logic [ 3:0] pal_wr_addr;
  logic [31:0] pal_wr_data;

  // Outputs
  logic [ 7:0] vga_r;
  logic [ 7:0] vga_g;
  logic [ 7:0] vga_b;
  logic        hsync_out;
  logic        vsync_out;
  logic        blank_out;

  // Error tracking as required by R-4.2.10
  int          errors = 0;

  // Instantiate the DUT
  tile_renderer_pipeline dut (.*);

  // Clock generation
  always #5 clk = ~clk;

  initial begin
    // Initialize Inputs
    clk = 0;
    rst = 1;
    vga_x = 0;
    vga_y = 0;
    hsync_in = 0;
    vsync_in = 0;
    blank_in = 1;
    tile_index = 0;
    pat_wr_en = 0;
    pat_wr_addr = 0;
    pat_wr_data = 0;
    pat_wr_strb = 0;
    pal_wr_en = 0;
    pal_wr_addr = 0;
    pal_wr_data = 0;

    #20 rst = 0;

    // -----------------------------------------------------------------
    // 1. Initialize Palette RAM (SR-REN-05)
    // -----------------------------------------------------------------
    @(posedge clk);
    pal_wr_en   = 1;
    // Index 5: 0x00_AA_BB_CC
    pal_wr_addr = 5;
    pal_wr_data = 32'h00AABBCC;
    @(posedge clk);
    // Index 10: 0x00_11_22_33
    pal_wr_addr = 10;
    pal_wr_data = 32'h00112233;
    @(posedge clk);
    pal_wr_en = 0;

    // -----------------------------------------------------------------
    // 2. Initialize Pattern RAM (SR-REN-04)
    // -----------------------------------------------------------------
    @(posedge clk);
    pat_wr_en   = 1;
    pat_wr_strb = 4'b1111;
    pat_wr_addr = 11'h03B;  // Tile 7, Row 3
    pat_wr_data = 32'h000000A5;  // Left pixel = 5, Right pixel = 10 (low nibble = left pixel)
    @(posedge clk);
    pat_wr_en = 0;

    #20;

    // -----------------------------------------------------------------
    // 3. Test 4-Stage Pipeline and Latency (SR-REN-06)
    // -----------------------------------------------------------------
    // Cycle 0
    @(posedge clk);
    vga_x = 10'd0;
    vga_y = 10'd3;
    hsync_in = 1;
    vsync_in = 0;
    blank_in = 1;

    // Cycle 1
    @(posedge clk);
    vga_x = 10'd1;
    tile_index = 8'd7;
    hsync_in = 0;
    vsync_in = 1;

    // Cycle 2
    @(posedge clk);
    vga_x = 10'd2;
    tile_index = 8'd7;

    // Cycle 3
    @(posedge clk);

    // Cycle 4: Check Pixel 0
    @(posedge clk);
    if (!(vga_r == 8'hAA && vga_g == 8'hBB && vga_b == 8'hCC &&
              hsync_out == 1 && vsync_out == 0)) begin
      $display("FAIL: Pixel 0 decode or latency mismatch. ",
               "Expected RGB AABBCC H=1 V=0, got %02X%02X%02X H=%b V=%b", vga_r, vga_g, vga_b,
               hsync_out, vsync_out);
      errors++;
    end

    // Cycle 5: Check Pixel 1
    @(posedge clk);
    if (!(vga_r == 8'h11 && vga_g == 8'h22 && vga_b == 8'h33 &&
              hsync_out == 0 && vsync_out == 1)) begin
      $display("FAIL: Pixel 1 decode or latency mismatch. ",
               "Expected RGB 112233 H=0 V=1, got %02X%02X%02X H=%b V=%b", vga_r, vga_g, vga_b,
               hsync_out, vsync_out);
      errors++;
    end

    #20;
    // Final verdict explicitly compliant with R-4.2.10
    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end
endmodule
