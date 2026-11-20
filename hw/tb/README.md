# `hw/tb/`

**Owner:** Module owner  
**Issues:** #53

Testbenches `tb_<module>.sv`, in the SystemVerilog subset that both Verilator and QuestaSim accept. A first-line `// questa-only` tag excludes a testbench from the Verilator run. SRS: QA-04.

## Testbench index (QA-04, #53)

QA-04: "every SR-VGA, SR-REN and SR-AUD requirement verified by Test shall have a QuestaSim testbench." The table maps each of them to the testbench that checks it and its committed log. Every log below ends in `PASS` and is current (`python3 tools/check_sim_logs.py --all`: all 9 OK, 19 Nov 2026). Logs are in `hw/sim/logs/<module>.log`; run one with `make -C hw/sim questa TB=tb_<module>`.

### Video: SR-VGA (`srs-huzaifa-rev3.md` §3.2)

| ID | SRS verification | Testbench | What it checks |
|---|---|---|---|
| SR-VGA-01 | Test (measure VGA_CLK) | none: a PLL output frequency is measured, not simulated | Board: #46. The testbenches count clocks, so they hold at any period |
| SR-VGA-02 | Test (QuestaSim counts pixel clocks per region) | `tb_vga_timing`, `tb_vga_controller` | 1024 active, 24 front porch, 136 sync, 160 back porch, 1344 per line, every line of 2 frames |
| SR-VGA-03 | Test (QuestaSim counts lines per region) | `tb_vga_timing`, `tb_vga_controller` | 768 active, 3 front porch, 6 sync, 29 back porch, 806 per frame |
| SR-VGA-04 | Test (waveform) | `tb_vga_timing`, `tb_vga_controller` | HS and VS low only during the sync pulses (pulse widths); 806 HS pulses per frame, also while dark |
| SR-VGA-05 | Test (waveform) | `tb_vga_controller`, `tb_arcade_video` | `VGA_BLANK_N` low outside the active area and during sync; no colour while blanked |
| SR-VGA-07 | Test (output colour vs expected palette value) | `tb_vga_controller`, `tb_arcade_video` | Every active pixel's RGB equals the renderer's colour; end to end, a written tile, pattern and palette entry appear on the pins |
| SR-VGA-08 | Test (simulation) | `tb_vga_timing`, `tb_vga_controller`, `tb_arcade_video` | `vblank_start` high on exactly the first clock after the last active pixel of line 767 (479 in bring-up), once per frame |
| SR-VGA-09 | Test (power on; monitor black, no "no signal") | `tb_vga_controller`, `tb_arcade_video` | With display enable 0, every active pixel black and sync still correct for a whole frame. Board power-on part: see the note below |
| SR-VGA-10 | Test (monitor reports 640×480 @ 60 Hz) | `tb_vga_timing`, `tb_vga_controller` | Bring-up mode: 640/16/96/48 px (800), 480/10/2/33 lines (525). Monitor part: board, #46 |

Inspection only (no testbench required): SR-VGA-06 (`VGA_SYNC_N` tied to 0; also checked in `tb_vga_controller` and `tb_arcade_video`), SR-VGA-11 (`VIDEO_MODE` default; both modes run in `tb_vga_timing` and `tb_vga_controller`).

### Tile Renderer: SR-REN (`srs-mujtaba-drop1.md` §3.2)

| ID | SRS verification | Testbench | What it checks |
|---|---|---|---|
| SR-REN-03 | Test | `tb_tilemap_banks`, `tb_arcade_video` | A swap request is applied only at the next `vblank_start`; `swap_pending` is set, then cleared once the swap is applied; the other bank is shown after it |
| SR-REN-06 | Analysis (testbench waveform) | `tb_tile_renderer_pipeline`, `tb_arcade_video` | One pixel per clock; colour, HSYNC, VSYNC and blank all come out exactly 4 clocks after the tilemap read |
| SR-REN-07 | Test | `tb_tilemap_banks`, `tb_arcade_video` | The reset clear sets both banks to tile 0 within one frame (≤ 16.7 ms); `fpga_ready` rises when it ends |

Inspection only: SR-REN-01, SR-REN-04, SR-REN-05, SR-REN-08 (also exercised by `tb_tilemap_banks`, `tb_tile_renderer_pipeline` and `tb_tilemap_crop`). SR-REN-02 is retired.

### Sound: SR-AUD (`srs-huzaifa-rev3.md` §3.2)

| ID | SRS verification | Testbench | What it checks |
|---|---|---|---|
| SR-AUD-01 | Test (measure AUD_XCK) | none: a PLL output frequency is measured, not simulated | Board: #50; PLL settings: #116 |
| SR-AUD-02 | Test (QuestaSim counts AUD_DACLRCK periods per simulated second) | `tb_i2s_tx` | 48,000 DACLRCK periods in one simulated second (12,288,000 clocks); 256 clocks per frame, 64 BCLK per frame |
| SR-AUD-03 | Test (decode the serial stream, compare with expected samples) | `tb_i2s_tx` | Decodes `AUD_DACDAT` like the codec in slave I²S mode: 48,002 frames, left = right = the sample handed over |
| SR-AUD-04 | Test (100, 440, 4,000 Hz; measure the period) | `tb_tone_generator` | Every whole period within ±1 %: 100.00, 440.08, 4,000.00 Hz |
| SR-AUD-05 | Test (10, 100, 2,000 ms) | `tb_tone_generator` | 10.000, 100.000, 2,000.000 ms (±1 ms allowed) |
| SR-AUD-06 | Test (simulation) | `tb_tone_generator` | A new tone during an old one reaches `sample` in 5 `aud_clk` (0.41 µs), well inside 1 sample (20.8 µs); the old one is dropped |
| SR-AUD-07 | Inspection and Test (no sample exceeds ±8,192) | `tb_tone_generator` | `sample` is only 0 or ±8,192, checked on every clock |
| SR-AUD-08 | Test (simulation, then board) | `tb_tone_generator` | Muted: every sample 0; sound again after unmute. Board part: #50 |
| SR-AUD-09 | Test (power on; no sound) | `tb_tone_generator` | Audio enable 0 (reset value): only 0; clearing it silences a playing tone. Board part: #50 |

### Notes

- **Measured on the board, not simulated:** SR-VGA-01 and SR-AUD-01 are PLL output frequencies (#46, #50); the monitor check of SR-VGA-10 is #46. The RTL testbenches count clock cycles, so they do not depend on the clock period.
- **SR-VGA-09 power-on check:** its SRS verification is "power on without starting the game; monitor shows black, no 'no signal' message". The simulation part is above; no board-test issue lists this check yet (suggested for #40 or a new board test).
- Testbench name to log: `tb_<module>.sv` → `hw/sim/logs/<module>.log` (`arcade_video`, `bridge_regs`, `i2s_tx`, `tile_renderer_pipeline`, `tilemap_banks`, `tilemap_crop`, `tone_generator`, `vga_controller`, `vga_timing`).

Rules: see [`AGENTS.md`](/AGENTS.md).
