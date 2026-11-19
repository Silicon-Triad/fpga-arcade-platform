# `hw/rtl/`

**Owner:** Mujtaba (bridge, renderer, top), Huzaifa (VGA, tone, I2S)  
**Issues:** #4, #25–#33, #41, #42

SystemVerilog RTL. `arcade_regs_pkg.sv` is FROZEN (mirror of `sw/hal/register_map.hpp`). SRS: SR-VGA, SR-REN, SR-AUD, §3.1.4.

## VGA Controller (Huzaifa, #25, #26, #27)

XGA 1024×768 @ 60 Hz (SR-VGA-02 to SR-VGA-07) or the 640×480 bring-up mode (SR-VGA-10), in two modules, each with its own testbench and QuestaSim log:

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
- **Display enable (SR-VGA-09, #26):** input `display_enable` is the Control Register bit (§3.1.4, `BIT_CONTROL_DISPLAY_ENABLE`) from `bridge_regs` (#96). `bridge_regs` runs on the same pixel clock (its assumption A2), so this is not a clock-domain crossing and there is no synchronizer (R-4.2.5); one register breaks up the path. While it is 0, sync and `VGA_BLANK_N` run normally and every active pixel is black. Reset value 0, as the register map. A change takes effect on the next clock, so the first lit frame can start partway through a line (SR-VGA-09 allows this).
- **`VIDEO_MODE` (SR-VGA-10, SR-VGA-11, #27):** a `string` parameter of `vga_controller` and `vga_timing`, the same name and values as `tilemap_crop` (App. A): `"XGA_1024x768"` (default) or `"VGA_640x480"`. Any other value stops elaboration with `$error`. Bring-up counts are the §3.1.2.1 640×480 table: 640/16/96/48 px (800 per line) and 480/10/2/33 lines (525 per frame). Line and frame order, sync polarity, `vblank_start` (one clock after the last active pixel of line 479) and display enable work as in XGA. The top level (#33) sets one value for `vga_controller` and `tilemap_crop` and builds the matching pixel clock: 65.000 MHz (SR-VGA-01) or 25.0–25.2 MHz (SR-VGA-10).

## Audio (Huzaifa, #41, #42)

The sound path is two modules on the 12.288 MHz codec master clock `aud_clk` (SR-AUD-01), the same PLL output the top level sends to `AUD_XCK` (#116). Ports were agreed on #41 with #116.

| Module | Does |
|---|---|
| `i2s_tx.sv` (#41) | Makes `AUD_BCLK` and `AUD_DACLRCK` by dividing `aud_clk`, and sends each sample on `AUD_DACDAT` in I²S format (SR-AUD-02, SR-AUD-03) |
| `tone_generator.sv` (#42) | The square-wave tone (SR-AUD-04 to SR-AUD-09); not yet |

- **Format (SR-AUD-03):** matches the codec set-up in #43 (R7: I²S, 16 bits, slave, LRP = 0, BCLKINV = 0). DACLRCK low = left. In each channel the MSB comes one BCLK after the DACLRCK edge, then the other 15 bits, then zeros. Left and right carry the same sample.
- **Rates (SR-AUD-02):** one frame is 256 `aud_clk` (12.288 MHz / 48 kHz). BCLK = `aud_clk` / 4 = 3.072 MHz, 64 BCLK per frame (32 per channel). The BCLK rate is a design choice: the SRS fixes only 48 kHz, 16 bits and I²S, and the codec in slave mode accepts it.
- **Edges:** BCLK, DACLRCK and DACDAT are all registered on `aud_clk`; DACLRCK and DACDAT change only on BCLK falling edges, so they are stable at the rising edge, where the codec samples. No logic on clock nets.
- **`sample_tick`:** a one-clock pulse per frame, on the last counter clock of the frame (2 `aud_clk` before the DACLRCK fall on the pin). `sample` is taken at the end of that clock and sent in the next frame. It is the 48 kHz time base for `tone_generator`.
- **Reset:** `aud_rst`, synchronous, active high (R-4.2.6). All outputs 0; the first frame sends 0.

Rules: see [`AGENTS.md`](/AGENTS.md).
