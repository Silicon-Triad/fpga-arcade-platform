# `hw/rtl/`

**Owner:** Mujtaba (bridge, renderer, top), Huzaifa (VGA, tone, I2S)  
**Issues:** #4, #25–#33, #41, #42

SystemVerilog RTL. `arcade_regs_pkg.sv` is FROZEN (mirror of `sw/hal/register_map.hpp`). SRS: SR-VGA, SR-REN, SR-AUD, §3.1.4.

## VGA Controller (Huzaifa, #25)

XGA 1024×768 @ 60 Hz timing (SR-VGA-02 to SR-VGA-07), in three modules, each with its own testbench and QuestaSim log:

| Module | Does |
|---|---|
| `vga_controller.sv` | Top: connects the two below, registers the pins, ties `VGA_SYNC_N` to 0 |
| `vga_timing.sv` | Line and frame counters; `pixel_x`, `pixel_y`, `active` and the sync pulses in cycle 0 (active high) |
| `vga_delay.sv` | Delay line: output = input from `DEPTH` clocks earlier, cleared by reset |

Decided by the area owner in #25:

- **Clock:** `clk` is the 65.000 MHz pixel clock. The PLL that makes it from 50 MHz (CON-03) is outside this module.
- **Line and frame order:** each line and frame starts with its active region (App. A `h_state`/`v_state`: ACTIVE, FRONT_PORCH, SYNC, BACK_PORCH). So `h_count = 0` is the first visible pixel, and `VGA_VS` changes only at the start of a line.
- **Tile Renderer interface (SR-REN-06):** the controller gives `pixel_x`, `pixel_y` (10 bits each) and `pixel_active` in cycle 0, and takes the renderer's colour on `rgb_in` (`{R, G, B}`) `PIXEL_LATENCY` clocks later (default 4, the renderer's N). It delays sync and blanking by the same amount, then registers every pin output together, so colour, sync and blanking change on the same clock edge.
- **Outputs:** `VGA_HS`/`VGA_VS` active low (SR-VGA-04), `VGA_BLANK_N` high only in the active region (SR-VGA-05), `VGA_SYNC_N` tied to 0 (SR-VGA-06). Outside the active region `VGA_R/G/B` are 0 as well as blanked. Reset: HS and VS high, blanked, black.
- **No `` `timescale ``** in the module or its testbench, matching `arcade_regs_pkg.sv`: Verilator stops with `TIMESCALEMOD` if some files have one and others do not.
- **Not yet:** `vblank_start` and display enable (#26), the 640×480 mode and `VIDEO_MODE` (#27), `VGA_CLK` and the PLL (top level and `hw/ip/`).

Rules: see [`AGENTS.md`](/AGENTS.md).
