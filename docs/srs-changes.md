# SRS changes

Changes agreed after SRS rev 1.0. `docs/srs/` itself is never edited (R-1.3).

## Game::lives() added to the game interface (#92)

SR-FW-03 lists five game functions. A sixth, optional one is added: `lives()`, which returns the lives left or nullopt for a game without lives. The engine needs it to show Breakout's lives in the HUD (SR-UI-04).

Status: approved by all three members (#92, merged in #94).

## Breakout HUD: SR-UI-04 and section 3.1.1 screen 4 disagree (#10)

SR-UI-04 asks the HUD to show the lives (Breakout only) and the game's current high score. Section 3.1.1 screen 4 shows `LIVES 3` on the right and no HI. Release code follows screen 4, so Breakout's HUD shows no high score. The Game Over box shows HI for both games.

Status: open; where to show Breakout's high score is still to be decided.
## "No keyboard" notice layout (#13, PR #100)

SR-UI-15 names a "no keyboard" notice but gives no text or position, so this is a design choice, not a requirement change.

- **Text and position:** `NO KEYBOARD - PLUG IN A USB KEYBOARD`, scale 1, centred on row 90.
- **Why row 90:** it is free on the menu, initials and high-score screens. On Game Over it overlays the frozen picture; in Breakout that is the paddle row (section 3.1.1 screen 4).
- **Extra state:** the notice also shows on ShutdownConfirm, beyond the four states SR-UI-15 lists, because the menu stays under that box.

## Text colour: white-only text for release 1.0 (#81)

Section 3.1.1 shows yellow, red and green text, a red shadow under the logo, and coloured bands behind text (menu highlight, hint band). The frozen `drawText` (SR-FW-12) takes no colour, so all text is drawn in white (palette entry 1) on black.

- **Decision:** option B on #81. Accept white-only text for release 1.0; `drawText` is not changed.
- **Why:** the screens read well in white, and coloured text would need a copy of the 95-tile font per colour, out of the 145 tiles left for games (section 3.1.1 tile budget).
- **Effect on verification:** the section 3.1.1 text colours, logo shadow and text bands are accepted deviations when the screens are inspected against the mockups.

Status: decided by the area owner (Muhammad) on #81. No frozen interface changes.

## Palette RGB values (#83)

TBL-Palette has 16 entries packed as `0x00RRGGBB`. The SRS fixes only entry 0 = black (section 3.1.1 screen 10b). The other values are a design choice made by the area owner (issues #9 and #75), copied here from `kPalette` in `sw/assets/palette.hpp`.

| Index | Name | 0x00RRGGBB | Source |
|---|---|---|---|
| 0 | Black | 0x000000 | SRS, section 3.1.1 screen 10b |
| 1 | White | 0xFFFFFF | design choice (all text, see #81) |
| 2 | Red | 0xFF0000 | design choice |
| 3 | Green | 0x00FF00 | design choice |
| 4 | DarkGreen | 0x008000 | design choice |
| 5 | Blue | 0x0000FF | design choice |
| 6 | Yellow | 0xFFFF00 | design choice |
| 7 | Orange | 0xFF8000 | design choice |
| 8 | Cyan | 0x00FFFF | design choice |
| 9 | Grey | 0x808080 | design choice |
| 10 | DarkGrey | 0x404040 | design choice |
| 11 | (unused) | 0x000000 | spare |
| 12 | (unused) | 0x000000 | spare |
| 13 | (unused) | 0x000000 | spare |
| 14 | (unused) | 0x000000 | spare |
| 15 | (unused) | 0x000000 | spare |

Status: agreed on #75; recorded 4 Nov 2026. If `kPalette` changes, update this table in the same PR.

## FPGA-manager state file on the board kernel (#34, PR #115)

The glossary entry "FPGA manager" gives `/sys/class/fpga_manager/fpga0/state` as an example of where SR-HAL-09 reads the configuration state. The board image (Intel UP Linux, kernel 3.18.0) uses Altera's own FPGA manager driver (`altera_fpga_manager ff706000.fpgamgr`), and the file it provides is `/sys/class/fpga/fpga0/status`. It reads `user mode` both after a fresh boot (U-Boot's default `soc_system.rbf`) and with our design loaded, so the ID/Version check of SR-HAL-09 is what tells the two apart.

- **Used by the HAL:** `/sys/class/fpga/fpga0/status` = `user mode` (`kFpgaManagerStatePath`, `kFpgaManagerConfiguredValue` in `sw/hal/dev_mem_register_space.hpp`).
- **Evidence:** board console log of 12 Nov 2026 quoted in PR #115.
- **Not a requirement change:** SR-HAL-09 is unchanged; only the glossary's example path differs from this kernel's.

Status: recorded 12 Nov 2026 by the HAL C++ owner (Muhammad); review on PR #115.

## GPIO1 base address for HPS_I2C_CONTROL: 0xFF709000, not 0xFF708000 (#119)

`srs-huzaifa-rev3.md` §3.1.2.4, "Switching the bus to the HPS", says: "HPS_I2C_CONTROL is HPS_GPIO48, bit 19 of the GPIO1 controller, whose registers are at 0xFF708000". The address was copied from the DE1-SoC manual §6.2, which contradicts its own bit numbers and the board.

- **Corrected value:** the GPIO1 controller's registers are at **0xFF709000**. HPS_GPIO48 is still bit 19 of GPIO1, and HPS_I2C_CONTROL is still HPS_GPIO48; only the base address changes.
- **Why (manual):** HPS_GPIO48 is bit 19 of GPIO1 (§6.4) and HPS_GPIO53 (HPS_LED) is bit 24 of GPIO1 (§6.2), so GPIO1's first line is GPIO 29.
- **Why (board):** the board's device tree has three GPIO controllers, `gpio@ff708000` (29 lines), `gpio@ff709000` (29 lines) and `gpio@ff70a000` (27 lines); lines 29 to 57 are the one at 0xFF709000. On 19 Nov 2026, with only `gpio@ff709000` enabled, Linux showed `gpiochip483 base=483 ngpio=29 label=ff709000.gpio`; driving GPIO 483 + 19 = 502 high gave the I2C bus to the HPS and the WM8731 acknowledged its writes, and with it low the same write failed (details on #43, #39 and PR #120).
- **Effect on code:** none. The HAL (#43, PR #120) finds HPS_I2C_CONTROL by the gpiochip label `ff709000.gpio` and never uses the register address. A future `/dev/mem` implementation would use 0xFF709000.
- **Requirement IDs:** SR-HAL-10 is unchanged; only the hardware fact in §3.1.2.4 is corrected.

Status: approved by all three members on #119 (19 Nov 2026).

## PR-02 verification: software timing instead of a 240 frames/s camera (#47, PR #127)

PR-02 (key press to the first displayed frame showing its effect, at most 67 ms) is verified in the SRS by Analysis (latency budget) and Test: "film the key and the screen with a 240 frames/s phone camera, 4.2 ms per frame; 20 presses; maximum ≤ 67 ms". The team has no camera faster than normal video, so the Test part is replaced by a software measurement on the board. The requirement and its 67 ms limit are unchanged.

- **New method:** Analysis plus a software Test, using `arcade --backend=fpga --latency --usb-us=N` (`sw/app/latency_probe.*`, procedure in `sw/app/README.md`). The latency is split into three parts:
  - **a, USB polling (Analysis):** at most the keyboard's polling interval, from `bInterval` in `lsusb -v` on the board, converted for the device speed shown by `lsusb -t` (low/full speed: bInterval × 1 ms; high speed: 2^(bInterval−1) × 125 µs, USB 2.0 spec 9.6.6).
  - **b, kernel event time to the bank swap (Test, measured):** the evdev timestamp of each press (switched to CLOCK_MONOTONIC with `EVIOCSCLOCKID`) against CLOCK_MONOTONIC when `present()` of the tick that read the press returns, i.e. after the swap.
  - **c, scan-out after the swap (Analysis):** at most one frame, FRAME_PERIOD_MS 16.67 ms (App. A), as PR-05 requires and its QuestaSim test verifies.
- **Pass:** a + max(b) + 16.67 ms ≤ 67 ms, over at least 20 presses (the SRS's count). No verdict is given if the clock check fails or any press was dropped.
- **Why it is at least as strict:** a camera at 240 frames/s resolves 4.2 ms per frame; b is timed to the microsecond, and a and c are taken at their worst case rather than measured.
- **Not covered:** a game that stores a key and acts on it at its own next step (Snake's step timer) shows the move later; that is game design, not input latency, as in the camera method's "first frame showing its effect".
- **Requirement IDs:** PR-02 is unchanged; only its verification method changes. PR-05 is used as stated.

Status: decided by the integration owner (Muhammad), 20 Nov 2026; recorded in PR #127.
