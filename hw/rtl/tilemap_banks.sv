// Double-buffered tilemap of the Tile Renderer (SR-REN-01, SR-REN-03, SR-REN-07, TBL-Tilemap).
//
// Two banks of TILEMAP_BANK_SIZE bytes (GRID_COLS x GRID_ROWS cells, 1 byte/cell), bank 0 and
// bank 1. The host (bridge register block, #28) writes either bank; the renderer pipeline reads
// the front bank. A Swap Request flips the front bank only on vblank_start, then clears
// swap_pending. A rising edge of Control Register bit 1 (reset tilemap), or a module reset,
// clears both banks to tile index 0, one cell per clock in both banks at once:
// TILEMAP_BANK_SIZE clocks, far less than one frame period.
//
// Clock domain: everything runs on clk (the renderer clock). Inputs from another clock domain
// must be synchronised by the caller (R-4.2.5).
// verilog_lint: waive-start parameter-name-style
module tilemap_banks #(
    localparam int ADDR_W = $clog2(arcade_regs_pkg::TILEMAP_BANK_SIZE)  // SR-REN-01: 12,288 cells
) (
    input logic clk,
    input logic rst,  // synchronous, active high (R-4.2.6)

    // Host write port (TBL-Tilemap: bank_id, cell index, tile index)
    input logic              wr_en,
    input logic              wr_bank,
    input logic [ADDR_W-1:0] wr_addr,
    input logic [       7:0] wr_data,

    input logic swap_req,       // 1-clock pulse: Swap Request write of 1 (§3.1.4)
    input logic reset_tilemap,  // level: Control Register bit 1 (§3.1.4, SR-REN-07)
    input logic vblank_start,   // 1-clock pulse from the VGA Controller (SR-VGA-08)

    // Renderer read port: front bank, 1 clock latency
    input  logic [ADDR_W-1:0] rd_addr,
    output logic [       7:0] rd_data,

    output logic front_bank,    // Swap Request / Front Bank read value (§3.1.4)
    output logic swap_pending,  // Status Register bit 2 (§3.1.4, SR-REN-03)
    output logic clear_busy     // high while the SR-REN-07 clear runs
);

  // SR-REN-01: index of the last cell of a bank
  localparam logic [ADDR_W-1:0] LAST_CELL = ADDR_W'(arcade_regs_pkg::TILEMAP_BANK_SIZE - 1);

  logic [7:0] bank0_mem[arcade_regs_pkg::TILEMAP_BANK_SIZE];  // SR-REN-01: bank 0
  logic [7:0] bank1_mem[arcade_regs_pkg::TILEMAP_BANK_SIZE];  // SR-REN-01: bank 1

  logic front_q, pending_q, busy_q, reset_tilemap_q;
  logic [ADDR_W-1:0] clr_addr_q;
  logic [7:0] rd0_q, rd1_q;

  // Bank memory write port: the clear (SR-REN-07) has priority over host writes.
  logic we0, we1;
  logic [ADDR_W-1:0] waddr;
  logic [       7:0] wdata;
  always_comb begin
    if (busy_q) begin
      we0   = 1'b1;
      we1   = 1'b1;
      waddr = clr_addr_q;
      wdata = 8'h00;  // SR-REN-07: tile index 0
    end else begin
      we0   = wr_en && !wr_bank && (wr_addr <= LAST_CELL);
      we1   = wr_en && wr_bank && (wr_addr <= LAST_CELL);
      waddr = wr_addr;
      wdata = wr_data;
    end
  end

  always_ff @(posedge clk) begin
    if (we0) bank0_mem[waddr] <= wdata;
    if (we1) bank1_mem[waddr] <= wdata;
    rd0_q <= bank0_mem[rd_addr];
    rd1_q <= bank1_mem[rd_addr];
  end

  // SR-REN-07: start a clear on reset or on a rising edge of Control bit 1; walk every cell.
  always_ff @(posedge clk) begin
    if (rst) begin
      busy_q          <= 1'b1;
      clr_addr_q      <= '0;
      reset_tilemap_q <= 1'b0;
    end else begin
      reset_tilemap_q <= reset_tilemap;
      if (reset_tilemap && !reset_tilemap_q) begin
        busy_q     <= 1'b1;
        clr_addr_q <= '0;
      end else if (busy_q) begin
        if (clr_addr_q == LAST_CELL) busy_q <= 1'b0;
        clr_addr_q <= clr_addr_q + 1'b1;
      end
    end
  end

  // SR-REN-03: swap only at vblank_start, then clear swap-pending. A request that arrives in
  // the same clock as vblank_start waits for the next one, so a swap never lands mid-frame.
  always_ff @(posedge clk) begin
    if (rst) begin
      // §3.1.4 reset values: Front Bank and Status bit 2 (swap pending)
      front_q   <= arcade_regs_pkg::RESET_SWAP_REQ_FRONT_BANK[arcade_regs_pkg::BIT_FRONT_BANK];
      pending_q <= arcade_regs_pkg::RESET_STATUS_REG[arcade_regs_pkg::BIT_STATUS_SWAP_PENDING];
    end else begin
      if (vblank_start && pending_q) begin
        front_q   <= !front_q;
        pending_q <= 1'b0;
      end
      if (swap_req) pending_q <= 1'b1;
    end
  end

  // The front-bank select is registered with the read so a swap never splits a read.
  logic rd_sel_q;
  always_ff @(posedge clk) rd_sel_q <= front_q;

  assign rd_data      = rd_sel_q ? rd1_q : rd0_q;
  assign front_bank   = front_q;
  assign swap_pending = pending_q;
  assign clear_busy   = busy_q;

endmodule : tilemap_banks
// verilog_lint: waive-stop parameter-name-style
