// Self-checking testbench for tilemap_banks (SR-REN-01, SR-REN-03, SR-REN-07, TBL-Tilemap).
// Time is counted in clocks; durations use the 65 MHz period (no timescale needed).

// verilog_lint: waive-start parameter-name-style
module tb_tilemap_banks;

  localparam int CELLS = arcade_regs_pkg::TILEMAP_BANK_SIZE;  // SR-REN-01
  localparam int ADDR_W = $clog2(CELLS);
  // SR-REN-06 / App. A PIXEL_CLK_HZ: 65 MHz pixel clock -> 15.385 ns period.
  localparam real CLK_PERIOD_NS = 1.0e9 / 65.0e6;
  // SR-REN-07: clear within one frame period (<= 16.7 ms).
  localparam real FRAME_PERIOD_NS = 16.7e6;

  logic clk = 1'b0;
  logic rst;
  logic wr_en, wr_bank;
  logic [ADDR_W-1:0] wr_addr, rd_addr;
  logic [7:0] wr_data, rd_data;
  logic swap_req, reset_tilemap, vblank_start;
  logic front_bank, swap_pending, clear_busy;
  int errors = 0;

  always #1 clk = ~clk;

  tilemap_banks dut (.*);

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

  task automatic write_cell(input logic bank, input int addr, input logic [7:0] data);
    wr_en   = 1'b1;
    wr_bank = bank;
    wr_addr = ADDR_W'(addr);
    wr_data = data;
    tick();
    wr_en = 1'b0;
  endtask

  // Read from the front bank (2 clocks: RAM read + output register).
  task automatic read_cell(input int addr, output logic [7:0] data);
    rd_addr = ADDR_W'(addr);
    tick();
    tick();
    data = rd_data;
  endtask

  task automatic pulse_vblank();
    vblank_start = 1'b1;
    tick();
    vblank_start = 1'b0;
  endtask

  task automatic pulse_swap_req();
    swap_req = 1'b1;
    tick();
    swap_req = 1'b0;
  endtask

  // Count clocks until clear_busy falls; check the SR-REN-07 time bound.
  task automatic wait_clear(input string what);
    int cycles = 0;
    while (clear_busy) begin
      tick();
      cycles++;
    end
    $display("%s: clear took %0d clocks (%0.3f ms)", what, cycles, cycles * CLK_PERIOD_NS / 1.0e6);
    check(cycles * CLK_PERIOD_NS <= FRAME_PERIOD_NS, {what, ": SR-REN-07 clear exceeds 16.7 ms"});
  endtask

  // Every cell of the front bank must hold 0 (SR-REN-07).
  task automatic check_front_all_zero(input string what);
    logic [7:0] d;
    int bad = 0;
    for (int a = 0; a < CELLS; a++) begin
      read_cell(a, d);
      if (d !== 8'h00) bad++;
    end
    check(bad == 0, $sformatf("%s: %0d cells of bank %0d not 0", what, bad, front_bank));
  endtask

  task automatic swap_now();
    pulse_swap_req();
    pulse_vblank();
  endtask

  initial begin
    logic [7:0] d;
    rst = 1'b1;
    wr_en = 1'b0;
    wr_bank = 1'b0;
    wr_addr = '0;
    wr_data = '0;
    rd_addr = '0;
    swap_req = 1'b0;
    reset_tilemap = 1'b0;
    vblank_start = 1'b0;
    tick();
    rst = 1'b0;

    // Reset values (§3.1.4): front bank 0, swap pending 0; banks cleared on reset.
    check(front_bank == 1'b0, "front bank not 0 after reset");
    check(swap_pending == 1'b0, "swap pending not 0 after reset");
    check(clear_busy == 1'b1, "SR-REN-07: no clear after reset");
    wait_clear("reset");
    check_front_all_zero("after reset");
    swap_now();
    check_front_all_zero("after reset");
    swap_now();

    // SR-REN-01: two independent banks of 12,288 cells, first and last cell addressable.
    write_cell(1'b0, 0, 8'h11);
    write_cell(1'b1, 0, 8'h22);
    write_cell(1'b0, CELLS - 1, 8'h33);
    write_cell(1'b1, CELLS - 1, 8'h44);
    read_cell(0, d);
    check(d == 8'h11, $sformatf("SR-REN-01: bank0[0] = %h, want 11", d));
    read_cell(CELLS - 1, d);
    check(d == 8'h33, $sformatf("SR-REN-01: bank0[last] = %h, want 33", d));

    // SR-REN-03: swap pending set on request, front bank unchanged until vblank_start.
    pulse_swap_req();
    check(swap_pending == 1'b1, "SR-REN-03: swap pending not set on Swap Request");
    repeat (1000) tick();
    check(front_bank == 1'b0, "SR-REN-03: swap applied before vblank_start");
    check(swap_pending == 1'b1, "SR-REN-03: swap pending cleared before vblank_start");
    read_cell(0, d);
    check(d == 8'h11, "SR-REN-03: display changed before vblank_start");
    pulse_vblank();
    check(front_bank == 1'b1, "SR-REN-03: swap not applied at vblank_start");
    check(swap_pending == 1'b0, "SR-REN-03: swap pending not cleared after swap");
    read_cell(0, d);
    check(d == 8'h22, $sformatf("SR-REN-03: front bank1[0] = %h, want 22", d));
    read_cell(CELLS - 1, d);
    check(d == 8'h44, $sformatf("SR-REN-03: front bank1[last] = %h, want 44", d));

    // SR-REN-03: vblank_start without a request does not swap.
    pulse_vblank();
    check(front_bank == 1'b1, "SR-REN-03: swap without a request");

    // SR-REN-03: a request in the same clock as vblank_start waits for the next one.
    swap_req = 1'b1;
    vblank_start = 1'b1;
    tick();
    swap_req = 1'b0;
    vblank_start = 1'b0;
    check(front_bank == 1'b1, "SR-REN-03: same-clock request swapped mid-frame");
    check(swap_pending == 1'b1, "SR-REN-03: same-clock request lost");
    pulse_vblank();
    check(front_bank == 1'b0, "SR-REN-03: pending swap not applied at next vblank_start");
    check(swap_pending == 1'b0, "SR-REN-03: swap pending not cleared");

    // SR-REN-07: fill both banks, set Control bit 1, both banks back to 0 within a frame.
    for (int a = 0; a < CELLS; a++) begin
      write_cell(1'b0, a, 8'(a % 255 + 1));
      write_cell(1'b1, a, 8'hFF);
    end
    read_cell(100, d);
    check(d == 8'(100 % 255 + 1), "fill of bank 0 failed");
    reset_tilemap = 1'b1;
    tick();
    check(clear_busy == 1'b1, "SR-REN-07: Control bit 1 did not start a clear");
    wait_clear("control bit 1");
    check_front_all_zero("after control reset");
    swap_now();
    check_front_all_zero("after control reset");

    // Holding bit 1 high does not restart the clear; a new rising edge does.
    write_cell(front_bank, 7, 8'h5A);
    repeat (10) tick();
    read_cell(7, d);
    check(d == 8'h5A, "SR-REN-07: held Control bit 1 restarted the clear");
    reset_tilemap = 1'b0;
    tick();
    reset_tilemap = 1'b1;
    tick();
    wait_clear("second edge");
    read_cell(7, d);
    check(d == 8'h00, "SR-REN-07: second rising edge did not clear");

    if (errors == 0) $display("PASS");
    else $display("FAIL");
    $finish;
  end

endmodule : tb_tilemap_banks
// verilog_lint: waive-stop parameter-name-style
