# Implementation Plan: FPGA Arcade Platform, release 1.0

Status: **PROPOSAL, awaiting approval.** No issues, labels, milestones or code exist yet.
Source of truth: `docs/srs/` (SRS rev 1.0, 19 Oct 2026). Every issue below cites the SRS IDs it implements. If this plan and the SRS disagree, the SRS wins.

Owners (from the SRS split):

| Person | Areas |
|---|---|
| **Muhammad** | Game Engine, Arcade API, game interface, Snake, Breakout, screens, high scores, SDL2 backend, integration and deployment |
| **Mujtaba** | LW bridge and register block, Tile Renderer, HAL (C++), FPGA backend, Quartus top level |
| **Huzaifa** | VGA Controller, Input Manager (C++), Tone Generator and I2S, board-level tests |

---

## 1. Repository layout

```
fpga-arcade-platform/
├── AGENTS.md                 # single rulebook for humans and AI agents
├── CLAUDE.md                 # contains only "@AGENTS.md"
├── CONTRIBUTING.md           # short onboarding, points to AGENTS.md
├── README.md
├── .github/
│   ├── workflows/ci.yml      # sw build+test+lint, hw lint, interface checks
│   ├── CODEOWNERS            # area → owner; frozen files need all three
│   ├── pull_request_template.md
│   └── ISSUE_TEMPLATE/       # feature.yml, bug.yml, interface-change.yml
├── docs/
│   ├── srs/                  # FROZEN: the submitted SRS, never edited
│   ├── plan.md               # this file
│   ├── setup/                # dev-pc.md, fpga-toolchain.md, board.md, sd-card.md
│   ├── user/                 # quick-start.md, player-manual.md
│   └── dev/                  # developer-guide.md (adding a game)
├── sw/                       # C++17, CMake
│   ├── CMakeLists.txt
│   ├── CMakePresets.json     # pc-debug, pc-coverage, de1soc-release
│   ├── cmake/arm-linux-gnueabihf.cmake
│   ├── include/arcade/       # FROZEN public headers
│   │   ├── arcade_api.hpp    # Arcade API (setTile, fillRect, drawText, clear, present,
│   │   │                     #   pollInput, playSound, setMute, loadTiles, frameCount)
│   │   ├── game.hpp          # game interface (init, update, render, isOver, score)
│   │   └── types.hpp         # Action, InputEvent, EffectId, TileIndex, grid constants
│   ├── engine/               # Muhammad: app state machine, menu, screens, high scores, registry
│   ├── games/snake/          # Muhammad
│   ├── games/breakout/       # Muhammad
│   ├── assets/               # Muhammad: font, mosaic tiles, game tiles, palette values
│   ├── backend_sdl/          # Muhammad: SDL2 backend (PC only)
│   ├── backend_fpga/         # Mujtaba: Arcade API implemented on the HAL
│   ├── hal/                  # Mujtaba: bridge, shadow tilemap, present, audio regs, codec I2C
│   │   └── register_map.hpp  # FROZEN; only hal/ may include it (QA-01)
│   ├── input/                # Huzaifa: evdev Input Manager, TBL-KeyBinding
│   ├── app/main.cpp          # Muhammad: entry point, backend option (SR-FW-05)
│   ├── tests/                # GoogleTest, one folder per module + mock HAL
│   └── tools/                # uinput injector, tick profiler, soak driver
├── hw/                       # SystemVerilog, Quartus Prime, Platform Designer
│   ├── rtl/
│   │   ├── arcade_regs_pkg.sv  # FROZEN register map (mirror of register_map.hpp)
│   │   ├── bridge_regs.sv      # Mujtaba
│   │   ├── tile_renderer.sv    # Mujtaba
│   │   ├── vga_controller.sv   # Huzaifa
│   │   ├── tone_generator.sv   # Huzaifa
│   │   ├── i2s_tx.sv           # Huzaifa
│   │   └── arcade_top.sv       # Mujtaba
│   ├── ip/                   # PLL and Platform Designer (.qsys) generated sources
│   ├── quartus/              # .qpf, .qsf (pins), .sdc
│   ├── tb/                   # QuestaSim testbenches, one per SR-VGA/REN/AUD group
│   └── sim/Makefile          # `make -C hw/sim <tb>` runs QuestaSim
├── board/                    # Muhammad (deploy) / Huzaifa (tests)
│   ├── deploy/               # init script, arcade user, poweroff wrapper, hardening
│   └── tests/                # board test procedures and result logs (PR, SF, QA)
└── tools/
    └── check_includes.py     # QA-01 layering check, run in CI
```

Tooling proposal (needs your OK, see section 5):
- C++: CMake ≥ 3.20, GCC (PC) and `arm-linux-gnueabihf-g++` (board), GoogleTest via `FetchContent`, SDL2, `clang-format` and `clang-tidy`, `gcovr` for coverage.
- SystemVerilog: Quartus Prime + Platform Designer, QuestaSim for testbenches (QA-04), **Verilator `--lint-only`** and **Verible** format and lint in CI, because QuestaSim cannot run on GitHub runners.

## 2. Milestones

Semester calendar: Week 10 starts Mon 19 Oct 2026; release is due at the end of Week 13 (Sun 15 Nov 2026). Hardware and software tracks run in parallel.

| # | Milestone | Due | Exit criterion |
|---|---|---|---|
| M0 | **Foundation** | Wed 21 Oct (W10) | Layout, CI green, frozen headers and register package merged and approved by all three |
| M1 | **PC playable** | Sun 1 Nov (end W11) | Both games, all screens and high scores playable on the SDL2 backend; Input Manager unit-tested |
| M2 | **Video on board** | Sun 1 Nov (end W11) | Test pattern, then a tilemap written by the HAL, shown at 1024×768 on a monitor |
| M3 | **Hardware integration** | Sun 8 Nov (end W12) | Both games run on the board with a USB keyboard and FPGA backend end to end |
| M4 | **Sound** | Sun 8 Nov (end W12) | Sound effects audible on line-out; mute works (all "should" requirements) |
| M5 | **Verification and release 1.0** | Sun 15 Nov (end W13) | Every PR/SF/SC/QA test passed and logged; user docs done; tag `v1.0` |

Schedule risk: four weeks is tight for 55 issues. If time runs short, M4 (all "should" requirements) is cut first, as the SRS priority allows.

## 3. Labels

| Group | Labels |
|---|---|
| Area | `area:engine`, `area:games`, `area:sdl`, `area:hal`, `area:bridge`, `area:renderer`, `area:vga`, `area:input`, `area:audio`, `area:board`, `area:infra`, `area:docs` |
| Type | `type:feature`, `type:test`, `type:bug`, `type:docs`, `type:chore` |
| Priority (from SRS wording) | `prio:shall`, `prio:should` |
| Owner (fallback if GitHub usernames are not mapped) | `owner:muhammad`, `owner:mujtaba`, `owner:huzaifa` |
| Process | `frozen-interface` (touches a frozen file; needs an approved interface-change issue), `blocked`, `needs-board` (cannot finish without the DE1-SoC), `agent-ok` (well-scoped enough for an AI agent) |

## 4. Issue list

Format: **ID. Title** (owner, milestone, labels). *SRS IDs.* Acceptance check.

### M0: Foundation

1. **Scaffold repo, CMake presets and CI** (Muhammad, M0, `area:infra`). *CON-08.* `cmake --preset pc-debug && ctest` passes on an empty test. CI runs sw build, test, clang-format check and Verilator lint on every PR, and is green.
2. **Freeze Arcade API, game interface and shared types** (Muhammad, M0, `area:engine`, `frozen-interface`). *SR-FW-03, SR-FW-05, Glossary (Arcade API), App. A Inputs/Outputs.* Headers compile. Doxygen comments cite SR IDs. All three owners approve the PR.
3. **Freeze register map in C++ and SystemVerilog** (Mujtaba, M0, `area:bridge`, `frozen-interface`). *§3.1.4, TBL-RegisterMap, TBL-Tilemap, TBL-TilePattern, TBL-Palette, HAL_ID_MAGIC.* `register_map.hpp` and `arcade_regs_pkg.sv` hold every offset, bit and reset value from §3.1.4. A CI test parses both and fails on any mismatch.
4. **Layering check: include rules** (Muhammad, M0, `area:infra`, `agent-ok`). *QA-01.* `tools/check_includes.py` fails CI if a file outside `sw/hal/` includes `register_map.hpp`, or a file under `sw/games/` includes anything but `arcade/*.hpp` and the standard library. A violating fixture proves it fails.
5. **Freeze software table definitions** (Muhammad, M0, `area:engine`, `frozen-interface`). *TBL-GameRegistry, TBL-HighScore, TBL-KeyBinding, TBL-SoundEffect (definition by Mujtaba, data by Huzaifa).* The C++ structs and constant tables match Chapter 5 field by field. The sound data equals the §5 contents table, checked by a unit test.

### M1: PC playable (software)

6. **Game loop and tick timing** (Muhammad, M1, `area:engine`). *SR-FW-02, PR-03, PR-04 (instrumentation).* The loop order is pollInput → update → render → present. A unit test with a mock clock shows one tick per frame. A `--profile` flag logs the tick time 99.9th percentile.
7. **Game registry and main menu** (Muhammad, M1, `area:engine`). *SR-FW-03, SR-FW-04, SR-UI-01, SR-UI-02, SR-UI-03, QA-02.* The menu lists the registry plus SCORES, and the selection wraps. Select starts a game within 1 tick. A test-only third game appears after adding one class and one registry row (QA-02 demo).
8. **Text rendering, assets, common layout and hint bar** (Muhammad, M1, `area:engine`). *SR-FW-12, SR-UI-13, §3.1.1 text, common layout and tile budget.* `drawText` scales 1 and 4 work. The font uses 95 tiles and the mosaic 16. Every screen draws a hint bar with the sound state. Screens match the §3.1.1 positions (golden-tilemap tests).
9. **Gameplay screens: HUD, pause, quit and game over** (Muhammad, M1, `area:engine`). *SR-UI-04, SR-UI-05, SR-UI-06, SR-UI-07, SR-UI-08, SR-UI-09.* Unit tests show: the state is identical across a 60 s pause; Back does not save; a qualifying score goes to Initials and others to Game Over; Select restarts the game.
10. **High-score store** (Muhammad, M1, `area:engine`). *SR-FW-08, SR-FW-09, SR-FW-10, SC-03, SC-04, TBL-HighScore.* Tests cover a missing, empty and corrupted file; bad lines are dropped; ties rank by `seq`; an 11th entry drops the lowest. Saving writes a temp file, calls `fsync`, then `rename` (verified with a fake filesystem layer).
11. **Initials entry and High Scores screen** (Muhammad, M1, `area:engine`). *SR-UI-10, SR-UI-12.* Select works only with 3 letters, Delete removes a letter, and the new entry is highlighted. Left and Right switch game.
12. **Keyboard-loss handling in the app** (Muhammad, M1, `area:engine`). *SR-UI-11, SR-UI-15.* During play, KeyboardLost leads to the paused screen 9 within 1 tick, and the game stays paused after reconnect until P. On other screens the state is kept and a notice is shown.
13. **Sound on/off and menu sound events** (Muhammad, M1, `area:engine`, `prio:should`). *SR-FW-11, SR-GAM-15, SR-GAM-16.* Sound is on at start. M toggles it via `setMute`. Each menu move or select calls `playSound` exactly once with the right effect.
14. **Shutdown and signal handling** (Muhammad, M1, `area:engine`). *SR-UI-14, SR-FW-13, SR-FW-14.* Holding Back 180 ticks opens the confirmation and 179 ticks does not. The shutdown steps run in the SRS order (mock HAL records the call order). SIGINT and SIGTERM do the same steps without poweroff.
15. **Boot sequence and hardware-not-ready** (Muhammad, M1, `area:engine`). *SR-FW-01, SF-02, §3.1.1 screen 1.* The splash shows a progress bar. On a HAL readiness failure, the message and failed step are printed and the program exits non-zero within 5 s (tested with the mock HAL).
16. **SDL2 backend** (Muhammad, M1, `area:sdl`). *SR-FW-05, SR-FW-06, SR-FW-07, QA-03.* `arcade --backend=sdl` opens a 1024×768 window drawing the 128×96 tilemap with PC keys mapped to the same actions. Every `playSound`/`setMute` call succeeds (unit test). The same game sources build for both backends.
17. **Snake** (Muhammad, M1, `area:games`). *SR-GAM-01 to SR-GAM-06, SR-GAM-17, SR-GAM-18, SF-01 (still death frame).* Unit tests cover the start state, the speed curve (6 → 3 ticks), reverse-key rejection, food placement and growth (+10), and wall and self death with a 30-tick still frame.
18. **Breakout** (Muhammad, M1, `area:games`). *SR-GAM-07 to SR-GAM-14, SR-GAM-19 to SR-GAM-22.* Unit tests cover the layout, serve and launch, paddle limits, ball speed by level, each bounce case including the paddle halves, row points 60 to 10, lives, and level clear after 60 ticks. Each sound event fires exactly once.
19. **Input Manager: device, reading and key state** (Huzaifa, M1, `area:input`). *SR-INP-01 to SR-INP-08.* Unit tests on recorded events: the keyboard is chosen over a mouse, 10 events in one tick all come back, value 2 is dropped, KeyDown and KeyUp are produced, and the held state is tracked. Code review confirms `O_NONBLOCK` and `EVIOCGRAB`.
20. **Key bindings and keymap override** (Huzaifa, M1, `area:input`). *SR-INP-09, SR-INP-10, SR-INP-11, SC-05, TBL-KeyBinding.* A test per context: M in Snake gives Mute, M in Initials gives Char('M'), all 26 letters map, and unbound keys are dropped. Invalid lines in `keymap.conf` are ignored and the defaults stay.
21. **Input hot-plug** (Huzaifa, M1, `area:input`). *SR-INP-12 to SR-INP-16.* With a fake evdev and inotify: ENODEV gives the Disconnected state, all keys released and KeyboardLost in the same tick. A new device is picked up. Start-up with no keyboard enters No-keyboard.
22. **uinput injector and input throughput test** (Huzaifa, M1, `area:input`, `type:test`). *PR-06.* `sw/tools/uinput_inject` sends 1,800 press/release pairs at 30/s. The test asserts 1,800 KeyDown and 1,800 KeyUp. It runs on a Linux PC and on the board.
23. **Unit-test coverage gate** (Muhammad, M1, `area:infra`, `agent-ok`). *QA-04 (software part).* The `pc-coverage` preset produces a gcovr report. CI fails if `sw/games/` and `sw/engine/` line coverage is below 80 %.

### M2: Video on board (hardware)

24. **PLL and XGA VGA timing** (Huzaifa, M2, `area:vga`). *SR-VGA-01 to SR-VGA-07, CON-02, CON-03.* A QuestaSim testbench counts 1344 clocks/line and 806 lines/frame, active-low HS/VS, BLANK_N low outside active, SYNC_N always 0, and RGB passed through.
25. **vblank_start and display-enable blanking** (Huzaifa, M2, `area:vga`). *SR-VGA-08, SR-VGA-09.* The testbench shows `vblank_start` on the first clock after the last active pixel of line 767, and black active pixels with correct sync while display enable = 0.
26. **640×480 bring-up mode and VIDEO_MODE parameter** (Huzaifa, M2, `area:vga`). *SR-VGA-10, SR-VGA-11.* The testbench checks the 800 × 525 frame. The default parameter is XGA (RTL review). The monitor reports 640×480 at 60 Hz.
27. **Bridge register block and Platform Designer system** (Mujtaba, M2, `area:bridge`). *§3.1.4, TBL-RegisterMap, SR-REN-03 (status bit), AS-02.* The testbench reads every register's reset value: ID = 0xA2CADE01, status bits as defined, frame counter increments, swap-pending set on write, and Front Bank readable.
28. **Double-buffered tilemap, swap at vblank and reset clear** (Mujtaba, M2, `area:renderer`). *SR-REN-01, SR-REN-03, SR-REN-07, TBL-Tilemap.* The testbench shows a swap applied only at `vblank_start` and swap-pending cleared. Reset clears both banks to 0 within 16.7 ms.
29. **Tile pattern RAM, palette and 4-stage pipeline** (Mujtaba, M2, `area:renderer`). *SR-REN-04, SR-REN-05, SR-REN-06, TBL-TilePattern, TBL-Palette, CON-05, CON-06.* The testbench checks that output colour equals palette[pattern[tile]] for known patterns, the latency is exactly 4 clocks, HS/VS are delayed by 4, and the nibble order is low = left.
30. **Bring-up crop 80×60** (Mujtaba, M2, `area:renderer`). *SR-REN-08.* In 640×480 mode only columns 0–79 and rows 0–59 are shown (RTL review plus a testbench frame dump).
31. **Frame-update latency testbench** (Mujtaba, M2, `area:renderer`, `type:test`). *PR-05.* Write the back bank, request a swap, and the next frame's pixels match the new tilemap.
32. **Quartus top level, pins, timing and memory budget** (Mujtaba, M2, `area:bridge`). *CON-01, CON-04.* A full compile with no timing violations. The fit report shows video memory at no more than about 33 M10K blocks. Pin assignments come from the DE1-SoC manual tables cited in §3.1.2.

### M3: Hardware integration

33. **HAL: bridge mapping and readiness check** (Mujtaba, M3, `area:hal`, `needs-board`). *SR-HAL-01, SR-HAL-02, SR-HAL-09, AS-01.* Maps 2 MB at 0xFF200000 through `/dev/mem`. A distinct error code is returned for each failure. With no bitstream it reports "graphics hardware not ready" before touching the tilemap, pattern or palette regions (board test).
34. **HAL: shadow tilemap, present and frame state** (Mujtaba, M3, `area:hal`). *SR-HAL-03, SR-HAL-04, SR-HAL-05, SR-HAL-06, SR-HAL-08.* Unit tests on a mock register space: drawing does no bridge access; present copies changed cells for that bank (2-frame-old bank case), writes Swap Request, then polls; there is a 100 ms timeout error. Front-bank and frame-counter reads do not block.
35. **HAL: start-up load and shutdown lifecycle** (Mujtaba, M3, `area:hal`). *SR-HAL-07, SR-HAL-13.* Display enable is set only after patterns and palette are loaded. `shutdown()` unmaps without clearing display enable (mock call order plus board check).
36. **FPGA backend of the Arcade API** (Mujtaba, M3, `area:hal`). *SR-FW-05, QA-03.* `arcade --backend=fpga` runs both games unchanged on the board.
37. **LW-bridge write throughput benchmark** (Mujtaba, M3, `area:hal`, `type:test`, `needs-board`). *§3.1.4 open item (data rate), PR-04.* Measured bytes/s for full-tilemap writes from user space is recorded in `board/tests/`, with a verdict against 737 kB/s.
38. **Board image, deployment and hardening** (Muhammad, M3, `area:board`, `needs-board`). *SC-01, SC-02, SC-06, CON-09, AS-01 to AS-06.* The init script loads the bitstream and starts `arcade` at boot. The program drops to user `arcade` (group `input`). The root script runs `poweroff` on a "shutdown requested" exit. `ss -tuln` shows nothing listening. Root has a password.
39. **End-to-end integration on board** (Muhammad, M3, `area:board`, `needs-board`). *UC-01 to UC-13 (demonstration).* Every use case is shown on the board, and a checklist is recorded in `board/tests/integration.md`.

### M4: Sound (should)

40. **Audio PLL and I2S transmitter** (Huzaifa, M4, `area:audio`, `prio:should`). *SR-AUD-01, SR-AUD-02, SR-AUD-03, CON-12.* The testbench gives 48,000 LRCK periods per simulated second ±0.5 %, decoded I²S samples match the input, and L = R.
41. **Tone Generator** (Huzaifa, M4, `area:audio`, `prio:should`). *SR-AUD-04 to SR-AUD-09, CON-13, SF-05 (RTL part).* The testbench checks frequency at 100, 440 and 4,000 Hz within ±1 %; duration at 10, 100 and 2,000 ms within ±1 ms; a new tone starts within 1 sample; |sample| ≤ 8,192; output is 0 while muted or not enabled.
42. **HAL: codec I2C configuration** (Mujtaba, M4, `area:hal`, `prio:should`, `needs-board`). *SR-HAL-10.* Sets HPS_I2C_CONTROL, writes the WM8731 registers listed in §3.1.2.4 at address 0x1A, then sets audio enable. A missing ACK gives an error with no hang.
43. **HAL: sound sequencer and mute** (Mujtaba, M4, `area:hal`, `prio:should`). *SR-HAL-11, SR-HAL-12, TBL-SoundEffect.* A unit test on the mock register space checks that each effect writes its notes in the right ticks, a new call replaces the old effect, and `setMute` writes bit 1.

### M5: Verification and release 1.0

44. **Board test: boot time and power-cut safety** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`). *PR-07, SR-FW-01, SR-FW-09, SF-04.* 5 power cycles each ≤ 60 s to menu. 20 power cuts during save loops each leave a loadable file.
45. **Board test: VGA clock and frame rate** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`). *PR-01, SR-VGA-01, SR-VGA-10.* Measured VGA_CLK and VGA_VS are within tolerance and recorded with photos.
46. **Board test: key-to-screen and sound latency** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`). *PR-02, PR-08.* With a 240 fps camera over 20 presses the maximum is ≤ 67 ms. Measured with the GPIO and oscilloscope, sound starts ≤ 16.7 ms after the call.
47. **Board test: tick timing over 10 minutes** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`). *PR-03, PR-04, SR-FW-02.* The tick count equals the change in frameCount over 10 minutes per game. The tick time 99.9th percentile is ≤ 8.3 ms.
48. **Board test: keyboard robustness** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`). *SF-03, SR-INP-03, SR-INP-15, SR-UI-11.* 20 unplug/replug cycles in each game and 5 on the menu with no crash and no score lost. Reconnect takes ≤ 1 s. Ctrl+Alt+Del does not reboot.
49. **Board test: audio levels and gating** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`, `prio:should`). *SR-AUD-01, SR-AUD-08, SR-AUD-09, SF-05.* AUD_XCK is 12.288 MHz ±0.5 %, line-out is ≤ 25 % of full scale, there is no sound before enable, and mute silences output.
50. **Board test: 2-hour soak** (Huzaifa, M5, `area:board`, `type:test`, `needs-board`). *QA-05.* A scripted uinput driver alternates the games for 2 h with no crash or hang and a valid high-score file.
51. **Usability and photosensitivity tests** (Huzaifa, M5, `area:board`, `type:test`). *QA-06, SF-01, SR-UI-02.* 5 first-time users each start a game in ≤ 30 s. Recorded effects show ≤ 3 flashes/s over a 341×256 px area.
52. **QuestaSim testbench index** (Huzaifa, M5, `area:vga`, `type:test`, `agent-ok`). *QA-04 (hardware part).* `hw/tb/README.md` maps every Test-verified SR-VGA, SR-REN and SR-AUD requirement to a passing testbench.
53. **Quick-start card** (Huzaifa, M5, `area:docs`). *SF-06, SF-07, §2.6.* `docs/user/quick-start.md` has the handling, power and shutdown warnings word for word from the SRS.
54. **Player manual and developer guide** (Muhammad, M5, `area:docs`). *§2.6, UC-12, QA-02.* `docs/user/player-manual.md` and `docs/dev/developer-guide.md`. A teammate adds a test game following the guide alone.
55. **Release 1.0 traceability check and tag** (Muhammad, M5, `area:infra`). *Appendix B.* Every one of the 134 derived requirements maps to a closed issue and a passing test or inspection record. Tag `v1.0`.

### Coverage check

All 107 SRs, 8 PRs, 7 SFs, 6 SCs and 6 QAs (134 in total) appear in at least one issue:
SR-FW 01–14 (#6, 7, 10, 13–16), SR-UI 01–15 (#7–9, 11, 12, 14), SR-GAM 01–22 (#13, 17, 18), SR-INP 01–16 (#19–21), SR-VGA 01–11 (#24–26), SR-AUD 01–09 (#40, 41), SR-HAL 01–13 (#33–35, 42, 43), SR-REN 01, 03–08 (#28–30), PR 01–08 (#6, 22, 31, 37, 44–47), SF 01–07 (#15, 17, 41, 44, 48, 49, 51, 53), SC 01–06 (#10, 20, 38), QA 01–06 (#4, 7, 16, 23, 50–52).

## 5. Decisions

| # | Decision | Answer |
|---|---|---|
| 1 | GitHub usernames | Muhammad = @Muhammad-Ahmed-Qazi, Mujtaba = @MujtabaJRao, Huzaifa = @huzaifa-2123 |
| 2 | Dates | Release by end of Week 13 (15 Nov 2026); milestone dates in section 2 |
| 3 | Quartus | No lab install; each member uses Quartus Prime 25.1 or later on their own machine, free edition only |
| 4 | Hardware checks in CI | **Open**, under discussion |
| 5 | Merge style | Squash-merge into `main`; 1 review from the area owner; all three for `frozen-interface` changes |
