# `hw/rtl/`

**Owner:** Mujtaba (bridge, renderer, top), Huzaifa (VGA, tone, I2S)  
**Issues:** #4, #25–#33, #41, #42

SystemVerilog RTL. `arcade_regs_pkg.sv` is FROZEN (mirror of `sw/hal/register_map.hpp`). SRS: SR-VGA, SR-REN, SR-AUD, §3.1.4.

## VGA Controller (Huzaifa, #25, #26)

XGA 1024×768 @ 60 Hz (SR-VGA-02 to SR-VGA-07), in two modules, each with its own testbench and QuestaSim log:

| Module | Does |
|---|---|
| `vga_controller.sv` | Top: sends cycle-0 timing to the Tile Renderer, takes back its delayed sync, blank and colour, and registers them onto the pins |
| `vga_timing.sv` | Line and frame counters; `pixel_x`, `pixel_y`, `active` and the sync pulses in cycle 0 (active high) |

Decided in #25 (PR #91 review):

- **The Tile Renderer delays sync and blank (SR-REN-06).** The controller gives the renderer `pixel_x`, `pixel_y`, `pixel_active`, `h_sync` and `v_sync` in cycle 0, all active high. The renderer returns `h_sync_in`, `v_sync_in`, `blank_in` (active high) and `rgb_in`, already aligned. The controller only inverts sync, registers everything and drives the pins, so there is one sync path. In the top level, `blank_in` of the renderer is `~pixel_active`.
- **`pixel_x` and `pixel_y` are valid only while `pixel_active`.** During blanking `pixel_x` wraps (`h_count[9:0]`), so the renderer gates on `pixel_active`.
- **Line and frame order:** each line and frame starts with its active region (App. A `h_state`/`v_state`), so `h_count = 0` is the first visible pixel and the vertical sync changes only at a line start.
- **Pins:** `VGA_HS`/`VGA_VS` active low (SR-VGA-04), `VGA_BLANK_N` low outside the active region (SR-VGA-05), `VGA_SYNC_N` tied to 0 (SR-VGA-06), `VGA_R/G/B` 0 while blanked (SR-VGA-07). Reset: HS and VS high, blanked, black.
- **Clock:** `clk` is the 65.000 MHz pixel clock, the same clock as the renderer. The PLL (SR-VGA-01, CON-03) and `VGA_CLK` come with the top level (#33).
- **`vblank_start` (SR-VGA-08, #26):** a one-clock pulse from `vga_timing`, registered, high on the clock with `h_count = 1024`, `v_count = 767`: the first clock after the last active pixel. It is cycle-0 timing, after the renderer's last tilemap read of the frame, so the tilemap bank swap (`tilemap_banks`, SR-REN-03) never lands mid-frame. Same pixel clock as the renderer, so no synchronizer.
- **Display enable (SR-VGA-09, #26):** input `display_enable` is the Control Register bit (§3.1.4, `BIT_CONTROL_DISPLAY_ENABLE`) from the bridge registers (#28). It comes from the bridge clock domain, so it goes through a two-flop synchronizer into the pixel clock domain (R-4.2.5). While it is 0, sync and `VGA_BLANK_N` run normally and every active pixel is black. Reset value 0, as the register map. The top level's timing constraints (#33) should treat the path into the first flop as asynchronous.
- **Not yet:** the 640×480 mode and `VIDEO_MODE` (#27).

Rules: see [`AGENTS.md`](/AGENTS.md).
