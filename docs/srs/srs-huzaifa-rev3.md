## 2.4 Operating Environment

The FPGA Arcade Platform runs on one Terasic DE1-SoC board connected to a VGA monitor and a USB keyboard. The game software runs as a user-space process on Embedded Linux on the board's ARM Cortex-A9 Hard Processor System (HPS), booted from a microSD card. The FPGA fabric on the same chip generates the 1024×768 @ 60 Hz VGA picture and the sound-effect tones, which the board's audio codec plays through powered speakers or headphones. No PC, network connection or other device is needed once the board is powered on (UR-01). During development, a PC is connected over USB for the serial console and for FPGA programming, but it is not part of the operating environment.

**Minimum platform**

| Item | Requirement | Source |
| --- | --- | --- |
| Board | Terasic DE1-SoC with Cyclone V SE 5CSEMA5F31C6. HPS: 800 MHz dual-core ARM Cortex-A9, 1 GB DDR3 SDRAM (32-bit bus) | [Manual §2.1, §3.7.4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |
| Power | 12 V DC adapter, supplied in the DE1-SoC kit | [Manual §1.1, §2.2](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |
| Storage | microSD card holding the Linux image, the game program, the FPGA configuration file and the high-score file | [Manual §3.7.5](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |
| Display | Monitor with an analog VGA (15-pin D-SUB) input that accepts 1024×768 @ 60 Hz | [Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |
| Cable | VGA cable, 15-pin D-SUB male to male | — |
| Input | USB keyboard (HID class), plugged into either of the board's two USB 2.0 Type-A host ports | [Manual §3.7.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |
| Audio (for UR-18) | Powered (active) speakers or headphones with a 3.5 mm stereo plug, in the green line-out jack. The board has no built-in speaker. Without them the games still run, silently | [Manual §2.1, §3.6.4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |
| Configuration switch | SW10 (MSEL[4:0]) set for FPGA configuration by the HPS: `01010` when Linux loads the FPGA, `00000` when U-Boot loads it from the SD card | [Manual §3.1, Table 3-2](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) |

The Linux interfaces the software relies on are specified in 3.1.3 Software Interfaces; the Linux image and FPGA configuration method are those of the lab's DE1-SoC set-up (2.7).

The physical setup is shown in `2.4-physical-setup.puml`: mains → 12 V adapter → DE1-SoC; DE1-SoC VGA port → VGA cable → monitor; USB keyboard → HPS USB host port; line-out jack → speakers; microSD card in the HPS socket.

> **Open item:** lab monitor support for 1024×768 @ 60 Hz over VGA is being checked with lab staff. If the lab monitors have no VGA input, the team brings or arranges a VGA monitor.

---

## 2.5 Design and Implementation Constraints

**CON-01 Single hardware platform.** The system shall run on one Terasic DE1-SoC board with the Cyclone V SE 5CSEMA5F31C6 device and no other processing hardware.
- Rationale: the course provides the DE1-SoC, and the product is defined as a self-contained arcade on one board.
- Source: [Manual §2.1](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-02 Hard real-time video timing.** VGA timing shall be generated entirely in FPGA logic at a 65.000 MHz pixel clock (15.38 ns per pixel), with no part of the timing depending on software.
- Rationale: Linux on the HPS is not a real-time OS; one late pixel or sync pulse makes the monitor lose lock.
- Source: [Manual §3.6.6, Tables 3-14, 3-15](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-03 Pixel clock source.** The 65.000 MHz pixel clock shall be derived by an FPGA PLL from a 50 MHz on-board clock (50 MHz × 13 ÷ 10).
- Rationale: the board provides four 50 MHz clocks to the FPGA and no 65 MHz oscillator; 13/10 gives 65 MHz exactly.
- Source: [Manual §3.5, Table 3-5](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-04 On-chip memory budget.** All video memory (two tilemap banks, tile patterns and palette) shall fit in the 5CSEMA5's on-chip memory, which has 397 M10K blocks (3,970 Kb) plus 480 Kb of MLAB.
- Budget (estimate, to be confirmed by the Quartus fit report):

| Memory | Size | M10K blocks (estimate) |
| --- | --- | --- |
| Tilemap bank A | 12,288 B (128 × 96 × 8 bit) | 12 (1K × 8 each) |
| Tilemap bank B | 12,288 B | 12 |
| Tile-pattern RAM | 8,192 B (256 tiles × 64 px × 4 bit) | 8 (256 × 32 each) |
| Palette | 64 B (16 entries × 32 bit, `0x00RRGGBB`) | 1 (or MLAB) |
| Tone Generator | 3 registers, no sample memory | 0 |
| Total | 32,816 B | ≈ 33 of 397 (≈ 8 %) |

- Rationale: external SDRAM adds latency and a memory controller; the budget leaves over 90 % of block RAM free. Sound adds no block RAM because tones are generated, not stored (CON-13).
- Source: [Intel Cyclone V Device Overview, embedded memory table](https://www.intel.com/content/www/us/en/docs/programmable/683694/current/embedded-memory-capacity-in-cyclone.html)

**CON-05 Tile-only graphics.** The display shall be built only from 8×8 px tiles on a 128 × 96 grid, with 16 on-screen colours. There are no hardware sprites, so every object moves in whole-tile (8 px) steps.
- Rationale: design baseline decision (tile size 8×8, 16-entry palette); keeps the Tile Renderer and memory small.

**CON-06 Colour depth.** Colour output shall use the board's ADV7123 DAC at 8 bits per channel; the 16 palette entries are chosen from this 24-bit range.
- Rationale: the board wires only the upper 8 of the DAC's 10 bits per channel.
- Source: [Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-07 Keyboard path.** Keyboard input shall come only from a USB HID keyboard on an HPS USB host port, read through the Linux input subsystem. The board's PS/2 port shall not be used.
- Rationale: design baseline decision (13 Oct). This changes Objective 5 of the approved proposal; 1.2 Scope records the change.
- Source: [Manual §3.7.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-08 Languages and toolchain.** Hardware shall be written in SystemVerilog and built with Quartus Prime and Platform Designer, then simulated with QuestaSim. Software shall be written in C++17 with GoogleTest for unit tests and SDL2 for the PC simulation backend.
- Rationale: course requirement and team decision.
- Open: Quartus Prime version installed in the lab (to be confirmed).

**CON-09 Offline operation.** The system shall provide no network services. The board's Gigabit Ethernet port shall not be used at run time.
- Rationale: a single-player arcade needs no network; this also removes remote attack paths (see 4.2 Security).

**CON-10 Analog VGA only.** Video output shall be analog VGA through the board's 15-pin D-SUB connector, so the monitor must have a VGA input (or a VGA-to-HDMI active converter).
- Rationale: the DE1-SoC has no digital video output.
- Source: [Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-11 Schedule.** The SRS shall be submitted by 19 Oct 2026, and the implementation shall be completed within the CS-326 semester by a team of three.
- Rationale: course deadline; limits scope to two games (Space Invaders is future scope).

**CON-12 Audio path.** Sound samples shall be generated in FPGA logic and sent to the WM8731 codec over its digital audio interface. The codec shall be configured by the HPS over I2C.
- Rationale: the codec's audio data pins connect only to the FPGA, while its I2C control bus can be switched to the HPS. Configuring it from Linux needs no version-specific Quartus IP and no FPGA rebuild to change settings.
- Source: [Manual §3.6.4, Table 3-12; §3.6.5, Table 3-13](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)

**CON-13 Tone-only sound.** Sound shall be one square-wave tone at a time, with no recorded sound samples.
- Rationale: tones need no sample memory and are enough for game-event sounds; sound is a "should" feature (UR-18), so its cost is kept small.

---

## 2.6 User Documentation

The system will be delivered with four documents. A one-page **quick-start card** shows how to connect power, monitor, keyboard and speakers, set SW10, and start the arcade. It also tells users to place the board on a non-conductive surface, handle it only by its edges, and connect or disconnect the 12 V supply only with the power switch off (SF-06). It gives the shutdown steps: on the main menu, hold Esc for 3 s, press Enter at "SHUT DOWN?", wait for "SAFE TO SWITCH OFF", then switch off the power. It warns that switching off at any other time can corrupt the microSD card (SF-07). A **player manual** describes each screen, the controls for Snake and Breakout (the key table in 3.1.1), scoring, sound on/off, and how high scores are saved. A **developer guide** explains how to add a new game: implement the game interface (`init`, `update`, `render`, `isOver`, `score`), register it in TBL-GameRegistry, load its tiles with `loadTiles`, and test it on a PC with the SDL2 backend before running it on the board. A **README** in the GitHub repository covers building the FPGA design and the C++ program, preparing the microSD card, and running the unit tests. All documents are written in English, in Markdown, and kept in the repository.

---

## 3.1 External Interface Requirements

### 3.1.1 User Interfaces

All screens are drawn on the 128 × 96 tile grid (columns 0–127, rows 0–95, 8 × 8 px per tile). Positions are given as (column, row) in tiles. Each screen has a clean mockup for this section and a gridded copy for the appendix. Both are drawn to scale by `3.1.1-mockups.py`, which extends the reference menu script. The mockups use the reference palette (entries 0–14); the final colour values are set in TBL-Palette.

**Text.** `drawText` takes a scale of 1 or 4:

- **Scale 1:** one font tile per character (ASCII 0x20–0x7E, 5 × 7 glyphs). Used for HUDs, captions and hint bars.
- **Scale 4:** each character covers 4 × 4 tiles and is built from 16 "quadrant mosaic" tiles. Each mosaic tile shows 2 × 2 font pixels, so one font pixel is 4 × 4 screen pixels. Used for logos, menu items and screen titles.
- **Tile budget:** the font (95 tiles) and the mosaic set (16 tiles) use 111 of the 256 tile patterns, leaving 145 for game graphics.

**Common layout.** Every screen has a border frame on rows 0 and 95 and columns 0 and 127, with a blue accent tile every 8 columns on rows 0 and 95.

| Region | During play (screens 3–6, 9) | Other screens (1, 2, 7, 8) |
| --- | --- | --- |
| Top | Row 0 frame; HUD text on row 1 | Row 0 frame |
| Play field / content | Rows 2–93 (walls on rows 2 and 93 where the game has them) | Rows 1–92 |
| Hint bar | Text on row 94; frame on row 95 | Text on row 93 (band over columns 1–126); frame on row 95 |

**Screen list and key mapping**

| # | Screen | Shown when | Keys | Mockup |
| --- | --- | --- | --- | --- |
| 1 | Boot / splash | From the first frame after the HAL is ready until the menu loads | — | `3.1.1-mockup-1-boot.png` |
| 2 | Main menu | After boot, after quitting a game, or after Esc on Game over | ↑/↓ choose item · Enter start game or open scores · H high scores · M sound on/off · hold Esc 3 s shut down | `3.1.1-mockup-2-menu.png` |
| 3 | Snake | A Snake game is running | Arrow keys turn · P pause · Esc quit to menu · M sound on/off | `3.1.1-mockup-3-snake.png` |
| 4 | Breakout | A Breakout game is running | ←/→ move paddle · Space launch ball · P pause · Esc quit to menu · M sound on/off | `3.1.1-mockup-4-breakout.png` |
| 5 | Paused | P pressed during a game | P resume · Esc quit to menu · M sound on/off | `3.1.1-mockup-5-paused.png` |
| 6 | Game over | A game ends and the score is not in the top 10 (decision D4) | Enter play again · Esc main menu · M sound on/off | `3.1.1-mockup-6-gameover.png` |
| 7 | Initials entry | A game ends and the score is in the top 10 (decision D4) | A–Z type · Backspace delete · Enter confirm (3 letters) | `3.1.1-mockup-7-initials.png` |
| 8 | High scores | H on the main menu, Enter on SCORES, or after initials entry | ←/→ switch game · Esc back · M sound on/off | `3.1.1-mockup-8-highscores.png` |
| 9 | Keyboard disconnected | The keyboard is unplugged during play (UR-14, SR-UI-11, decision D5) | (plug in a keyboard) | `3.1.1-mockup-9-nokeyboard.png` |
| 10 | Shut down (10a confirm, 10b safe to switch off) | Esc held for 3 s on the main menu (UR-19, SR-UI-14) | 10a: Enter confirm · Esc cancel; 10b: none | `3.1.1-mockup-10a-shutdown-confirm.png`, `3.1.1-mockup-10b-safe-off.png` |

The key defaults are stored in TBL-KeyBinding, so remapping a key needs no code change. M (sound on/off, UR-18) is bound in the Global context of TBL-KeyBinding, with the action Mute, so it works on every screen except Initials entry, where M types a letter (SR-INP-09). The on/off setting lasts until the system is powered off.

**Screen layouts**

1. **Boot / splash.** "FPGA ARCADE" at scale 4 on rows 34–37, centred (columns 42–85), yellow with a red shadow. Subtitle "SILICON TRIAD - DE1-SOC - 1024X768" at scale 1 on row 44. Progress bar on row 50, columns 32–95, filling in green as start-up proceeds. "STARTING..." on row 53. Only if no keyboard is found, row 60 shows "NO KEYBOARD FOUND - PLUG IN A USB KEYBOARD" in red. Before the HAL has mapped the hardware, the VGA Controller outputs a black screen (SR-VGA-09), so this screen is the first picture the player sees.
2. **Main menu (reference screen).**
    - Logo "FPGA ARCADE" at scale 4 on rows 6–9, subtitle on row 16, divider on row 19 (columns 16–111).
    - Menu items at scale 4 from column 13: "SNAKE" on row 28, "BREAKOUT" on row 38, "SCORES" on row 48. The selected item has a highlight band (columns 6–55, from 2 rows above to 5 rows below the text), a green bar in column 6 and a scale-4 ">" at column 8.
    - Preview panel on the right: a green frame over columns 60–121 and rows 24–77, showing the selected game in 3 × 3-tile cells. Row 74 inside the panel shows the best score and the game's own caption. A tagline sits under the panel on row 80.
    - "PRESS ENTER TO PLAY" on row 62 (column 13), status line "SOUND ON - 2 GAMES - TOP 10 SAVED" on row 86.
    - Hint bar on row 93: "^/v CHOOSE  ENTER PLAY  H HIGH SCORES  M SOUND: ON  HOLD ESC: SHUT DOWN".
3. **Snake.** HUD on row 1: "SNAKE" at column 2, "SCORE 000120" centred, "HI 000560" right-aligned to column 125. The board is 42 × 30 Snake cells, each drawn as 3 × 3 tiles (24 × 24 px), covering columns 1–126 and rows 3–92. The walls are column 0, column 127, row 2 and row 93. Snake body in dark green, head in bright green, food in red. Hint bar on row 94: "ARROWS TURN  P PAUSE  ESC MENU  M SOUND: ON".
4. **Breakout.** HUD on row 1: "BREAKOUT" at column 2, "SCORE 000340" centred, "LIVES 3" right-aligned to column 125. Walls are column 0, column 127 and row 2; the bottom is open. Each brick sits in a 6 × 2-tile slot (5 tiles of brick plus a 1-tile gap): 21 bricks per row across columns 1–126, 6 rows on rows 8–19, one colour per row (red, orange, yellow, green, cyan, blue). The paddle is 16 × 1 tiles on row 90 (columns 56–71 when centred); the ball is 2 × 2 tiles (16 × 16 px), drawn round with 4 ball tiles. Hint bar on row 94: "</> MOVE  SPACE LAUNCH  P PAUSE  ESC MENU  M SOUND: ON".
5. **Paused.** The game picture stays in place. A box with a blue frame covers columns 40–87 and rows 38–57: "PAUSED" at scale 4 on rows 41–44, then "P RESUME" on row 48, "ESC QUIT TO MENU" on row 50 and "M SOUND ON/OFF" on row 52. Hint bar on row 94: "P RESUME  ESC MENU  M SOUND: ON".
6. **Game over.** The final game picture stays in place. A box with a red frame covers columns 36–91 and rows 34–61: "GAME OVER" at scale 4 on rows 37–40, "SCORE 000340" on row 45, "HI 000560" on row 47, "ENTER PLAY AGAIN" on row 52 and "ESC MAIN MENU" on row 54. Hint bar on row 94.
7. **Initials entry.** "NEW HIGH SCORE!" at scale 4 on rows 18–21. Game, score and rank ("SNAKE - SCORE 000560 - RANK 2") on row 28; divider on row 32. Three letter slots at scale 4, starting at columns 50, 60 and 70, with text on rows 40–43, each on a highlight background (rows 38–45) with an underline on row 46. The current slot's underline is green, the others grey. "ENTER YOUR INITIALS" on row 52. Hint bar on row 93: "A-Z TYPE  BACKSPACE DELETE  ENTER CONFIRM".
8. **High scores.** "HIGH SCORES" at scale 4 on rows 6–9, divider on row 16. Game selector on rows 20–23: the game name at scale 4, centred, with green "<" and ">" at columns 40 and 84. Column headings on row 30 ("RANK" column 36, "NAME" column 56, "SCORE" column 80), divider on row 32. Ten entries on rows 34–52, one every 2 rows; rank 1 is shown in yellow. Hint bar on row 93: "</> SWITCH GAME  ESC BACK  M SOUND: ON".
9. **Keyboard disconnected.** The game is paused and its picture stays in place. A box with a red frame covers columns 30–97 and rows 38–57: "NO KEYBOARD" at scale 4 on rows 41–44, "PLUG IN A USB KEYBOARD TO CONTINUE" on row 49 and "GAME PAUSED" on row 52. The hint bar on row 94 is empty because no keys work. When a keyboard is detected, the box is replaced by the Paused screen (5), and the game continues only when the player presses P.
10. **Shut down (UR-19, SR-UI-14).**
    - **10a, confirm:** the main menu stays in place. A box with a blue frame covers columns 40–87 and rows 38–57, the same as the Paused box: "SHUT DOWN?" at scale 4 on rows 41–44, "ENTER YES" on row 48 and "ESC NO" on row 50. The hint bar on row 93 reads "ENTER CONFIRM  ESC CANCEL". Esc closes the box and returns to the menu.
    - **10b, safe to switch off:** after Enter, the program mutes, saves and releases the hardware (SR-FW-13/14). The whole screen is black (palette entry 0, no frame), with "SAFE TO SWITCH OFF" at scale 4 on rows 46–49, centred (columns 28–99) in white. No keys work. The power may be switched off only after this screen appears (SF-07).

### 3.1.2 Hardware Interfaces

#### 3.1.2.1 VGA output

The VGA Controller in the FPGA drives the board's Analog Devices ADV7123 video DAC, which drives the 15-pin D-SUB VGA connector ([Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).

- **DAC:** ADV7123 triple 10-bit video DAC; the board connects only the upper 8 bits per channel, so colour is 24-bit RGB. The board is rated up to 1280×1024 at a 100 MHz pixel clock ([Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Data latching:** pixel data, BLANK and SYNC are latched on the rising edge of the DAC clock, with a pipeline delay of 1 clock cycle ([ADV7123 datasheet, Tables 6 and 8](https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf)).
- **Analog levels:** the R, G and B outputs drive RS-343A video levels into a doubly terminated 75 Ω load: 0 V for black and about 0.7 V for full white ([ADV7123 datasheet, Figure 23, Analog Outputs](https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf)).
- **Sync signals:** HSYNC and VSYNC go directly from the FPGA to the connector as 3.3 V digital signals ([Manual Table 3-16](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).

**Signals** ([Manual Table 3-16](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf))

| Signal | Direction (FPGA) | Width | Function |
| --- | --- | --- | --- |
| VGA_R[7:0] | Out | 8 | Red pixel value to DAC |
| VGA_G[7:0] | Out | 8 | Green pixel value to DAC |
| VGA_B[7:0] | Out | 8 | Blue pixel value to DAC |
| VGA_CLK | Out | 1 | DAC clock = pixel clock (65 MHz) |
| VGA_BLANK_N | Out | 1 | Logic 0 forces all three DAC outputs to the blanking level and ignores pixel data ([ADV7123 Table 8](https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf)) |
| VGA_SYNC_N | Out | 1 | DAC composite-sync input; tied to logic 0 when no sync on green is needed ([ADV7123 Table 8](https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf)) |
| VGA_HS | Out | 1 | Horizontal sync to monitor, active low |
| VGA_VS | Out | 1 | Vertical sync to monitor, active low |

All signals use the 3.3 V I/O standard.

**Timing — operating mode 1024×768 @ 60 Hz (XGA)**

| Parameter | Horizontal | Vertical |
| --- | --- | --- |
| Active video | 1024 px (15.8 µs) | 768 lines |
| Front porch | 24 px (0.4 µs) | 3 lines |
| Sync pulse | 136 px (2.1 µs) | 6 lines |
| Back porch | 160 px (2.5 µs) | 29 lines |
| Total | 1344 px | 806 lines |

Pixel clock 65.000 MHz. Line rate = 65 MHz ÷ 1344 = 48.36 kHz. Frame rate = 48.36 kHz ÷ 806 = 60.004 Hz. Source: [Manual Tables 3-14 and 3-15](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf) (µs values; pixel and line counts follow from the 65 MHz clock).

**Timing — bring-up mode 640×480 @ 60 Hz**

| Parameter | Horizontal | Vertical |
| --- | --- | --- |
| Active video | 640 px | 480 lines |
| Front porch | 16 px | 10 lines |
| Sync pulse | 96 px | 2 lines |
| Back porch | 48 px | 33 lines |
| Total | 800 px | 525 lines |

Pixel clock 25.175 MHz nominal (the manual lists 25 MHz). Source: [Manual Tables 3-14 and 3-15](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf).

#### 3.1.2.2 USB keyboard

- **Ports:** the board has two USB 2.0 Type-A host ports. An SMSC USB3300 PHY connects to the HPS USB 2.0 controller over ULPI, through an SMSC USB2512B 2-port hub; the PHY runs in host mode, powering the two ports ([Manual §3.7.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Keyboard type:** any USB keyboard of the HID class.
- **Protocol handling:** USB enumeration and the HID protocol are handled by the Linux kernel's USB host and HID drivers, which present the keyboard as an event device `/dev/input/eventN` ([Linux input subsystem](https://www.kernel.org/doc/html/latest/input/input.html)).
- **Events:** each key change arrives as a `struct input_event` of type `EV_KEY` with a key code (`KEY_*`) and a value of 1 for key press, 0 for key release and 2 for auto-repeat ([input event codes](https://www.kernel.org/doc/html/latest/input/event-codes.html)).
- **No FPGA involvement:** the FPGA has no part in keyboard input, and there is no PS/2 or USB logic in the FPGA design.

#### 3.1.2.3 Board power and reset

- **Power:** 12 V DC input; the 12 V power LED (D14) lights when power is on ([Manual §2.2, Table 3-3](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Cold reset — KEY5 (HPS_RESET_N):** resets the HPS, the Ethernet PHY and the USB host. This restarts Linux and the game ([Manual §3.4, Table 3-4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Warm reset — KEY7 (HPS_WARM_RST_N):** resets the HPS system domain, for debugging ([Manual Table 3-4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **FPGA push-buttons KEY0–KEY3 and switches SW0–SW9:** not used by the games.
- **Power loss:** removing power at any time must not corrupt the high-score file. The file is written atomically (see SR-FW and 4.2 Safety).

#### 3.1.2.4 Audio output

The Tone Generator in the FPGA sends sound to the board's Wolfson WM8731 audio codec, which drives the 3.5 mm line-out jack ([Manual §3.6.4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).

- **Codec:** WM8731, 24-bit, with microphone-in, line-in and line-out jacks and a sample rate adjustable from 8 kHz to 96 kHz. Only the DAC and line-out are used; line-in and microphone-in are not used ([Manual §3.6.4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Speaker:** the board has no built-in speaker. Powered speakers or headphones plug into the green line-out jack (2.4).
- **Output level:** the line output has a fixed 0 dB gain; full scale is 1.0 × AVDD / 3.3 Vrms. The codec's analog volume control applies only to its headphone output, so on line-out the volume is set by the sample amplitude (SR-AUD-07) ([WM8731 datasheet, Electrical Characteristics, Table 9](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)).
- **Clock and format:** the codec runs in slave mode, so the FPGA supplies the bit clock and the left/right clock. Samples are sent in I²S format, 16 bits per channel, at 48 kHz with a 12.288 MHz master clock (256 × 48 kHz) ([WM8731 datasheet, Digital Audio Interfaces; Table 18](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)).

**Audio data signals** ([Manual Table 3-12](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf))

| Signal | FPGA pin | Direction (FPGA) | Function |
| --- | --- | --- | --- |
| AUD_XCK | PIN_G7 | Out | Codec master clock, 12.288 MHz |
| AUD_BCLK | PIN_H7 | Out | Bit clock (codec in slave mode) |
| AUD_DACLRCK | PIN_H8 | Out | DAC left/right clock, 48 kHz |
| AUD_DACDAT | PIN_J7 | Out | DAC serial data (the sound) |
| AUD_ADCLRCK | PIN_K8 | — | ADC left/right clock; not used |
| AUD_ADCDAT | PIN_K7 | — | ADC serial data (recording); not used |

All signals use the 3.3 V I/O standard.

**Control interface (codec set-up)**

- **Bus:** the codec's registers are written over I2C. An I2C multiplexer connects this bus to the FPGA (PIN_J12/PIN_K12) or to the HPS. The HPS gets the bus only when HPS_I2C_CONTROL is high ([Manual §3.6.5, Table 3-13](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Switching the bus to the HPS:** HPS_I2C_CONTROL is HPS_GPIO48, bit 19 of the GPIO1 controller, whose registers are at 0xFF708000 ([Manual §6.2, §6.4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Linux access:** the HAL writes the codec registers through the Linux I2C driver at `/dev/i2c-0`, as in the manual's I2C multiplexer demo ([Manual §6.4](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- **Registers used:** Reset (R15), Analogue Audio Path Control (R4), Digital Audio Path Control (R5), Digital Audio Interface Format (R7) and Sampling Control (R8), plus the power-down and activation registers (R6, R9; Power Down Control and Active Control). Each write carries a 7-bit register address and 9 bits of data ([WM8731 datasheet, register descriptions](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)).
- **Device address:** 0x1A (7-bit), set by the CSB pin being low ([WM8731 datasheet, software control interface](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)).

---

## 3.2 System Functional Requirements

### SR-VGA — VGA timing and display output (VGA Controller)

**SR-VGA-01** The VGA Controller shall generate video with a pixel clock of 65.000 MHz ± 0.5 %.
- Parent: UR-06
- Rationale: 65 MHz is the XGA 1024×768 @ 60 Hz pixel clock; the PLL ratio 13/10 from 50 MHz is exact (CON-03).
- Verification: Test (measure VGA_CLK with an oscilloscope or frequency counter)

**SR-VGA-02** The VGA Controller shall produce each video line as 1024 active pixels, 24 front-porch pixels, 136 HSYNC pixels and 160 back-porch pixels, for 1344 pixel clocks per line.
- Parent: UR-06
- Rationale: XGA horizontal timing ([Manual Table 3-14](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)); a monitor loses lock if the line length is wrong.
- Verification: Test (QuestaSim testbench counts pixel clocks per region)

**SR-VGA-03** The VGA Controller shall produce each frame as 768 active lines, 3 front-porch lines, 6 VSYNC lines and 29 back-porch lines, for 806 lines per frame.
- Parent: UR-06
- Rationale: XGA vertical timing ([Manual Table 3-15](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)); 806 lines at 48.36 kHz gives 60.004 Hz.
- Verification: Test (QuestaSim testbench counts lines per region)

**SR-VGA-04** The VGA Controller shall drive VGA_HS and VGA_VS low during their sync pulses and high at all other times.
- Parent: UR-06
- Rationale: XGA uses active-low (negative) sync on both signals; the manual describes the sync as an active-low pulse ([Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- Verification: Test (simulation waveform check)

**SR-VGA-05** The VGA Controller shall drive VGA_BLANK_N low for every pixel clock outside the 1024 × 768 active region.
- Parent: UR-06
- Rationale: BLANK low forces the DAC outputs to the blanking level during the porches and sync, as the monitor requires ([ADV7123 Table 8](https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf)).
- Verification: Test (simulation waveform check)

**SR-VGA-06** The VGA Controller shall hold VGA_SYNC_N at logic 0 at all times.
- Parent: UR-06
- Rationale: sync is sent on the separate HSYNC and VSYNC wires; the datasheet says to tie SYNC to logic 0 when no sync on green is needed ([ADV7123 Table 8](https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf)).
- Verification: Inspection (RTL review)

**SR-VGA-07** During the active region, the VGA Controller shall output on VGA_R[7:0], VGA_G[7:0] and VGA_B[7:0] the 24-bit colour supplied by the Tile Renderer for that pixel.
- Parent: UR-06
- Rationale: the DAC takes 8 bits per channel on this board ([Manual §3.6.6](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)); the Tile Renderer owns the palette lookup (SR-REN).
- Verification: Test (simulation compares output colour with expected palette value)

**SR-VGA-08** The VGA Controller shall signal the start of vertical blanking to the Tile Renderer on the first pixel clock after the last active pixel of line 767.
- Parent: UR-13
- Rationale: the tilemap bank swap and the vsync flag read by the HAL happen at this point, so a new frame is never shown half-updated.
- Verification: Test (simulation)

**SR-VGA-09** From FPGA configuration until the HAL sets the display-enable bit in the control register, the VGA Controller shall output correct sync timing with every active pixel black (R = G = B = 0).
- Parent: UR-15
- Rationale: the monitor stays locked and shows a defined black screen instead of random tilemap contents while Linux boots.
- Verification: Test (power on without starting the game; monitor shows black, no "no signal" message)

**SR-VGA-10** The VGA Controller shall provide a 640×480 @ 60 Hz bring-up mode with an 800-pixel × 525-line frame and a pixel clock between 25.0 MHz and 25.2 MHz.
- Parent: UR-06
- Rationale: 640×480 is accepted by almost every VGA monitor, so it is used to bring up the video path before XGA ([Manual Tables 3-14, 3-15](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).
- Verification: Test (monitor reports 640×480 @ 60 Hz)

**SR-VGA-11** The video mode shall be selected by a synthesis-time parameter of the VGA Controller, with 1024×768 as the default value.
- Parent: UR-06
- Rationale: a fixed PLL per build keeps timing simple; the bring-up mode is a development build only.
- Verification: Inspection (RTL review)

### SR-INP — USB keyboard input (Input Manager)

**SR-INP-01** At start-up, the Input Manager shall select as the keyboard the first `/dev/input/event*` device that reports `EV_KEY` support for KEY_A to KEY_Z, KEY_ENTER, KEY_ESC and the four arrow keys.
- Parent: UR-05
- Rationale: other input devices (for example a mouse) also appear as event devices; these keys are the ones the games need ([input event codes](https://www.kernel.org/doc/html/latest/input/event-codes.html)).
- Verification: Test (start with a keyboard and a mouse connected; the keyboard is chosen)

**SR-INP-02** The Input Manager shall open the keyboard device in non-blocking mode (`O_NONBLOCK`).
- Parent: UR-05
- Rationale: the game loop must never wait for a key (baseline: `pollInput()` never blocks).
- Verification: Inspection (code review)

**SR-INP-03** The Input Manager shall request exclusive access to the keyboard device with the `EVIOCGRAB` ioctl.
- Parent: UR-05
- Rationale: stops keystrokes from also reaching the Linux console, where a key combination such as Ctrl+Alt+Del could reboot the board ([linux/input.h](https://github.com/torvalds/linux/blob/master/include/uapi/linux/input.h)).
- Verification: Test (press Ctrl+Alt+Del while playing; the board does not reboot)

**SR-INP-04** Once per game tick, the Input Manager shall read every queued `input_event` from the keyboard device until `read()` returns `EAGAIN`.
- Parent: UR-05
- Rationale: draining the queue every tick (60 per second) keeps input latency within one tick and loses no events.
- Verification: Test (inject 10 events in one tick; all 10 are returned)

**SR-INP-05** The Input Manager shall convert each `EV_KEY` event with value 1 into a KeyDown event carrying the key code.
- Parent: UR-05
- Rationale: value 1 means key press ([input event codes](https://www.kernel.org/doc/html/latest/input/event-codes.html)).
- Verification: Test (unit test with recorded events)

**SR-INP-06** The Input Manager shall convert each `EV_KEY` event with value 0 into a KeyUp event carrying the key code.
- Parent: UR-05
- Rationale: value 0 means key release; Breakout needs releases to stop the paddle.
- Verification: Test (unit test)

**SR-INP-07** The Input Manager shall discard `EV_KEY` events with value 2 (auto-repeat).
- Parent: UR-05
- Rationale: baseline decision: held keys are tracked from key-down and key-up events, not from the OS auto-repeat, so speed does not depend on the repeat rate.
- Verification: Test (hold a key for 2 s; exactly one KeyDown is produced)

**SR-INP-08** The Input Manager shall keep a pressed/released state for every key code, updated by each KeyDown and KeyUp event.
- Parent: UR-05
- Rationale: the Breakout paddle moves 1 tile per tick while its key is held.
- Verification: Test (unit test)

**SR-INP-09** The Input Manager shall translate each KeyDown event into an action using the TBL-KeyBinding entry for the current context (Menu, Snake, Breakout, Paused, GameOver, Initials, HighScores, ShutdownConfirm) or, if the key has no entry there, the entry for the Global context.
- Parent: UR-02
- Rationale: one mapping table gives every screen its keys (3.1.1) and allows remapping without code changes. Global holds keys that work on every screen, such as M (sound on/off). Checking the current context first lets a screen override a Global key: in Initials entry, M types the letter M (SR-INP-11) instead of muting.
- Verification: Test (unit test per context, including M in Snake → Mute and M in Initials → Char 'M')

**SR-INP-10** The Input Manager shall discard a KeyDown event whose key code has no binding in either the current context or the Global context.
- Parent: UR-05
- Rationale: unbound keys must have no effect on the game.
- Verification: Test (unit test)

**SR-INP-11** In the Initials context, the Input Manager shall translate a KeyDown on KEY_A to KEY_Z into a Char action carrying the letter A to Z.
- Parent: UR-11
- Rationale: initials are three letters A–Z (TBL-HighScore constraint).
- Verification: Test (unit test for all 26 letters)

**SR-INP-12** When `read()` on the keyboard device returns `ENODEV`, the Input Manager shall enter the Disconnected state.
- Parent: UR-14
- Rationale: the kernel's evdev driver returns `ENODEV` once the device has been removed ([evdev.c](https://github.com/torvalds/linux/blob/master/drivers/input/evdev.c)); the stale file descriptor is closed on entry to this state.
- Verification: Test (unplug the keyboard during play)

**SR-INP-13** On entering the Disconnected state, the Input Manager shall set every key to released.
- Parent: UR-14
- Rationale: a key held at the moment of unplugging would otherwise stay "pressed" (for example, a paddle that keeps moving).
- Verification: Test (unplug while holding →; the paddle stops)

**SR-INP-14** On entering the Disconnected state, the Input Manager shall report a KeyboardLost event to the Game Engine within the same game tick.
- Parent: UR-14
- Rationale: the Game Engine handles KeyboardLost as in SR-UI-11: during play it pauses the game and shows screen 9 (3.1.1); on other screens nothing changes (decision D5).
- Verification: Test

**SR-INP-15** In the Disconnected and No-keyboard states, the Input Manager shall select a newly plugged-in keyboard (as in SR-INP-01) within 1 s of it being plugged in.
- Parent: UR-14
- Rationale: the Input Manager watches `/dev/input` with inotify, which reports new device files without polling the directory ([inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html)).
- Verification: Test (unplug, wait 5 s, plug in; input works within 1 s)

**SR-INP-16** If no keyboard is found at start-up, the Input Manager shall enter the No-keyboard state.
- Parent: UR-14
- Rationale: start-up continues without a keyboard instead of hanging or exiting; screen 1 tells the player to plug one in.
- Verification: Test (boot with no keyboard)

### SR-AUD — Sound effects (Tone Generator)

UR-18 is a "should" requirement, so every SR-AUD requirement uses "should". The Tone Generator is a new FPGA block alongside the Tile Renderer and VGA Controller.

**SR-AUD-01** The Tone Generator should drive AUD_XCK at 12.288 MHz ± 0.5 %.
- Parent: UR-18
- Rationale: 12.288 MHz is the codec master clock for a 48 kHz sample rate (256 × 48 kHz) ([WM8731 datasheet, Table 18](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)); it comes from an FPGA PLL fed by a 50 MHz board clock. A ±0.5 % error only shifts pitch by an amount players cannot hear in a beep.
- Verification: Test (measure AUD_XCK with an oscilloscope or frequency counter)

**SR-AUD-02** The Tone Generator should drive AUD_BCLK and AUD_DACLRCK so that the codec receives 48,000 stereo frames per second ± 0.5 %.
- Parent: UR-18
- Rationale: in slave mode the codec takes its bit clock and left/right clock from the FPGA ([WM8731 datasheet, pin descriptions](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)).
- Verification: Test (QuestaSim testbench counts AUD_DACLRCK periods per second of simulated time)

**SR-AUD-03** The Tone Generator should send each sample on AUD_DACDAT as a 16-bit two's-complement value in I²S format, with the same value on the left and right channels.
- Parent: UR-18
- Rationale: matches the codec format set in R7 (I²S, 16-bit) ([WM8731 datasheet, Digital Audio Interfaces](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf)); one value on both channels gives mono sound from both speakers.
- Verification: Test (testbench decodes the serial stream and compares it with the expected samples)

**SR-AUD-04** While a tone is playing, the Tone Generator should output a square wave at the frequency in the tone-frequency register (written by the HAL from TBL-SoundEffect), for any value from 100 Hz to 4,000 Hz, within ± 1 %.
- Parent: UR-18
- Rationale: 100–4,000 Hz covers the beeps the games need; ±1 % is below the pitch difference most listeners notice.
- Verification: Test (simulation at 100, 440 and 4,000 Hz; measure the period of the output)

**SR-AUD-05** The Tone Generator should stop a tone (output value 0) when the time in the tone-duration register has passed, for any value from 10 ms to 2,000 ms, within ± 1 ms.
- Parent: UR-18
- Rationale: games call `playSound(effectId)`; the HAL looks the effect up in TBL-SoundEffect and writes its frequency and duration to the Tone Generator registers. The hardware then ends the tone by itself, so no software has to stop it and the game loop is never delayed.
- Verification: Test (simulation at 10, 100 and 2,000 ms)

**SR-AUD-06** When the HAL writes a new tone while another is playing, the Tone Generator should start the new tone within 1 sample period (20.8 µs) and drop the old one.
- Parent: UR-18
- Rationale: one voice only (CON-13); the newest game event is the one the player should hear.
- Verification: Test (simulation)

**SR-AUD-07** The square-wave amplitude should not exceed ± 8,192 (25 % of 16-bit full scale).
- Parent: UR-18
- Rationale: the line output has fixed gain, so the sample amplitude is the only volume control (3.1.2.4); the cap protects players using headphones (SF-05 in 4.2).
- Verification: Inspection (RTL review of the amplitude constant) and Test (simulation checks no sample exceeds ±8,192)

**SR-AUD-08** While the mute bit in the audio control register is 1, the Tone Generator should output the value 0.
- Parent: UR-18
- Rationale: the player turns sound off with M (3.1.1); muting in hardware means the games need no mute logic of their own.
- Verification: Test (simulation, then on the board with speakers)

**SR-AUD-09** From FPGA configuration until the HAL sets the audio-enable bit in the audio control register, the Tone Generator should output the value 0.
- Parent: UR-18
- Rationale: no noise or click is played while Linux boots and the HAL configures the codec over I2C; this mirrors SR-VGA-09 for the display.
- Verification: Test (power on with speakers connected and the game not started; no sound)

---

---

## 3.3 Behaviour models (Huzaifa's part)

### Keypress sequence diagram

File: `3.3-seq-keypress.puml`.

The diagram follows one keypress (LEFT in Breakout) from the player to the Game Engine:

- **Path:** USB keyboard → Linux USB HID and evdev → Input Manager → Game Engine. It ends when `pollInput()` returns the KeyDown event and the Action to the Game Engine.
- **Hand-off:** from there the Game Engine runs `update` → `render` → `present`, which is Mujtaba's frame-update sequence diagram (Figures `3.3-seq-frame-1-request` to `3.3-seq-frame-3-display`). How the Game Engine uses each action is in SR-UI and SR-GAM.
- **Timing:** notes give the worst-case time of each stage up to the hand-off. Together with the frame-update path they add up to the latency budget in PR-02.

### VGA Controller state machine

Figures: `3.3-sm-vga-1-horizontal` and `3.3-sm-vga-2-vertical`.

Two concurrent regions:

- **Horizontal:** H_ACTIVE (1024 px) → H_FRONT_PORCH (24) → H_SYNC (136) → H_BACK_PORCH (160). Advances every pixel clock.
- **Vertical:** V_ACTIVE (768 lines) → V_FRONT_PORCH (3) → V_SYNC (6) → V_BACK_PORCH (29). Advances at the end of each line.

Each state lists its output levels (HS, VS, BLANK_N). The V_ACTIVE → V_FRONT_PORCH transition raises `vblank_start`, which triggers the tilemap bank swap, the vsync flag and the frame counter (SR-VGA-08).

### Input Manager state machine

File: `3.3-sm-input-manager.puml`.

- **Start-up:** the Input Manager scans for a keyboard, then enters READY or NO_KEYBOARD.
- **Unplug:** READY → DISCONNECTED when `read()` returns ENODEV.
- **Reconnect:** NO_KEYBOARD or DISCONNECTED → READY when inotify reports a new keyboard.
- **Hand-off:** on entering DISCONNECTED, the Input Manager reports KeyboardLost to the Game Engine; what the game does with it is SR-UI-11. Per decision D5, the app enters its KeyboardLost state (pause and screen 9) only during play.

The entry and do actions trace to SR-INP-01 to SR-INP-16.

---

## 4.1 Performance Requirements

**Latency budget (used by PR-02).** The worst case is a key pressed just after the game loop has read input:

| Stage | Worst case | Why |
| --- | --- | --- |
| USB poll | 10 ms | The keyboard reports at its next poll; USB keyboards are typically polled every 8–10 ms |
| Kernel HID → evdev | < 1 ms | Driver work on an 800 MHz Cortex-A9 |
| Wait for the next game tick | 16.7 ms | `pollInput()` runs once per tick, just after vsync (SR-INP-04) |
| Tick work and bank swap | 16.7 ms | `update`, `render` and `present()` (copy changed cells, Swap Request, poll swap-pending); the banks swap at the next vblank (PR-03, PR-04; frame-update sequence diagram) |
| Scan-out to the changed row | 16.7 ms | The new frame is drawn top to bottom; the bottom row appears last |
| **Total** | **61.1 ms** | Below 67 ms (4 frames at 60 Hz) with 5.6 ms of margin |

**PR-01** The VGA Controller shall produce a frame rate of 60 Hz ± 0.5 % (59.7 Hz to 60.3 Hz).
- Parent: UR-06
- Rationale: 65 MHz ÷ (1344 × 806) = 60.004 Hz (SR-VGA-01 to 03), well inside the ±0.5 % that monitors accept for this mode.
- Verification: Test (measure VGA_VS with an oscilloscope or frequency counter)

**PR-02** The time from a key press to the first displayed frame showing its effect shall be 67 ms (4 frames) or less.
- Parent: UR-05
- Rationale: the latency budget above gives 61.1 ms in the worst case. This turns UR-05's "without noticeable delay" into a number.
- Verification: Analysis (latency budget) and Test (film the key and the screen with a 240 frames/s phone camera, 4.2 ms per frame; 20 presses; maximum ≤ 67 ms)

**PR-03** The Game Engine shall run exactly one game tick per displayed frame, with no missed frames over 10 minutes of play.
- Parent: UR-05
- Rationale: game speeds are counted in ticks (baseline, SR-FW-02), so a missed frame would slow the game and add 16.7 ms of input latency.
- Verification: Test (after 10 minutes of Snake and of Breakout, the tick counter equals the change in `frameCount()`)

**PR-04** One game tick of work (`pollInput`, `update`, `render` and copying changed cells to the back bank) shall take 8.3 ms or less in 99.9 % of ticks on the HPS.
- Parent: UR-05
- Rationale: 8.3 ms is half of a 16.7 ms frame. The other half is margin for Linux scheduling delays, so `present()` issues its Swap Request before the next vblank and PR-03 holds. The game loop itself is SR-FW-02.
- Verification: Test (time each tick with `clock_gettime(CLOCK_MONOTONIC)` over 10 minutes of each game; report the 99.9th percentile)

**PR-05** A tilemap change written before `present()` shall appear on screen in the first frame scanned out after the bank swap, within 16.7 ms of the swap.
- Parent: UR-13
- Rationale: double-buffered banks swap at vertical blanking (SR-VGA-08), so a frame is never half-updated and is never more than one frame late.
- Verification: Test (QuestaSim: write the back bank, swap, check that the next frame's pixels match)

**PR-06** The Input Manager shall report every key press and release, with none lost, when keys are pressed at 30 keys per second for 60 s.
- Parent: UR-05
- Rationale: 30 keys/s is above fast typing speed; draining the event queue every tick (SR-INP-04) means at most a few events wait per tick.
- Verification: Test (inject 1,800 press/release pairs at 30/s with a Linux `uinput` test program; compare the KeyDown and KeyUp counts with 1,800 each)

**PR-07** The system shall show the main menu 60 s or less after power is applied.
- Parent: UR-01
- Rationale: decision D1 sets boot-to-menu at 60 s or less (SR-FW-01). The time is mostly Linux boot from the microSD card and FPGA configuration.
- Verification: Test (stopwatch from power switch to menu, 5 power cycles, maximum ≤ 60 s)

**PR-08** A sound effect should start playing within 16.7 ms (one frame) of the `playSound(effectId)` call made for a sound event (SR-GAM-15 to SR-GAM-22).
- Parent: UR-18
- Rationale: the HAL writes the tone registers straight away and the Tone Generator starts within one sample (SR-AUD-06), so the sound comes at most one frame before the picture that shows the event.
- Verification: Test (toggle a test GPIO in `playSound()`; on an oscilloscope, measure from the GPIO edge to the first non-zero sample on line-out)

---

## 4.2 Safety and Security Requirements

**SF-01** No area of 341 × 256 px or larger shall flash more than 3 times in any 1 s period.
- Parent: UR-06
- Rationale:
    - WCAG 2.3.1 limits content to three flashes in any one-second period.
    - For a 1024 × 768 screen, it estimates the flash area that matters as a 341 × 256 pixel rectangle anywhere on screen ([WCAG 2.1 Understanding 2.3.1](https://www.w3.org/WAI/WCAG21/Understanding/three-flashes-or-below-threshold.html)).
    - This protects players with photosensitive epilepsy. It limits effects such as a flashing "GAME OVER" or a whole-screen flash when a life is lost. The Snake death screen holds a still frame instead of flashing, so it meets the limit by design.
- Verification: Inspection (review each game's effects against the limit) and Test (record each effect and count luminance changes per second)

**SF-02** If the HAL reports "graphics hardware not ready", the Game Engine shall print that message and the failed step to the serial console and exit with a non-zero status within 5 s.
- Parent: UR-15
- Rationale: UR-15 says the system must report the problem instead of hanging. Until the HAL sets display-enable, the monitor shows a stable black screen (SR-VGA-09), so the player never sees random video.
- Verification: Test (start the program without loading the FPGA bitstream; check the message and exit status)

**SF-03** Unplugging and reconnecting the keyboard 20 times, during play and on other screens, shall cause no crash, no hang and no loss of the current game or score.
- Parent: UR-14
- Rationale: covers the whole unplug path together (SR-INP-12 to 16 and SR-UI-11). Per decision D5, losing the keyboard pauses the game only during play; on other screens the screen simply stays. A pulled cable is the most likely fault during a lab demo.
- Verification: Test (20 unplug/replug cycles in each game and 5 on the main menu; the game resumes with the same score after P, and the menu stays usable)

**SF-04** After a power loss at any moment, the high-score file shall contain either the complete old table or the complete new table.
- Parent: UR-12
- Rationale:
    - Decision D2 and SR-FW-09: the table is a CSV file, written to a temporary file, flushed with `fsync()`, then renamed over the old file.
    - `rename()` replaces an existing file atomically, so no process ever finds it missing ([rename(2)](https://man7.org/linux/man-pages/man2/rename.2.html)).
    - `fsync()` makes sure the data is on the card before the rename ([fsync(2)](https://man7.org/linux/man-pages/man2/fsync.2.html)).
- Verification: Test (a test program rewrites the table in a loop; cut power 20 times; after each boot the file loads with 10 valid entries per game)

**SF-05** The sound output should not exceed 25 % of the codec's full-scale level.
- Parent: UR-18
- Rationale: per decision D3, sound is on at boot, so the first tone can reach a player's headphones without warning. The line output has fixed gain, so the amplitude cap in the Tone Generator (SR-AUD-07) is the safety measure, and no game can exceed it.
- Verification: Test (measure line-out amplitude with an oscilloscope at the highest-amplitude effect; ≤ 25 % of full scale)

**SF-06** The quick-start card shall tell users to place the board on a non-conductive surface, handle it only by its edges, and connect or disconnect the 12 V supply only with the power switch off.
- Parent: UR-01
- Rationale: the DE1-SoC is a bare board with exposed components and pins, so static discharge or a short on a metal desk can damage it.
- Verification: Inspection (review the quick-start card, 2.6)

**SF-07** The quick-start card shall tell users to switch off power only after "SAFE TO SWITCH OFF" appears.
- Parent: UR-19
- Rationale: pulling power while Linux is running can corrupt the microSD card, which holds the Linux image, the game and the high scores. The shutdown sequence (SR-UI-14, SR-FW-13/14) saves and powers Linux off, then shows "SAFE TO SWITCH OFF" (3.1.1 screen 10b). The card sits next to the SF-06 handling instructions.
- Verification: Inspection (review the quick-start card, 2.7)

---

## 4.2.1 Security

**Expected level.** The arcade runs offline in a teaching lab. It has no network services (CON-09), stores no personal data beyond three-letter initials, and can only be used by someone standing at the board. The realistic threats are:

- a malformed or tampered file on the microSD card (the high-score file or the keymap override);
- someone using the serial console or keyboard to get a Linux shell.

Remote attack is out of scope because no network interface is in use.

**Privileges.** The program needs privileged access to three device files:

- `/dev/mem`, to map the HPS-to-FPGA bridge (owned by root, group kmem, mode 660 ([mem(4)](https://man7.org/linux/man-pages/man4/mem.4.html)));
- `/dev/input/event*`, for the keyboard;
- `/dev/i2c-0`, for the codec.

The requirements below keep that privilege as small as possible.

**SC-01** After mapping the bridge and opening `/dev/i2c-0`, the game program should drop root privileges and run as a dedicated user `arcade` that belongs to the `input` group.
- Parent: UR-01
- Rationale: only start-up needs root. Membership of `input` is still needed to open a reconnected keyboard (SR-INP-15). If the program is attacked through a malformed file, it then runs without root. Running `poweroff` for the shutdown (SR-FW-13/14) also needs root; with SC-01, the program can exit with a "shutdown requested" status and let its root start-up script run `poweroff`.
- Verification: Inspection (`ps -o user` shows `arcade` during play)

**SC-02** No process on the board shall listen on any TCP or UDP port while the arcade is running.
- Parent: UR-01
- Rationale: matches CON-09. Some DE1-SoC Linux images start network services by default, and these must be disabled.
- Verification: Test (`ss -tuln` on the serial console lists no listening sockets)

**SC-03** The high-score CSV file (decision D2) shall store only game ID, rank, three-letter initials, score and entry order for each entry.
- Parent: UR-12
- Rationale: TBL-HighScore holds no names, dates or other personal data, so the file reveals nothing about players beyond their chosen initials.
- Verification: Inspection (file format against TBL-HighScore)

**SC-04** On start-up, the Game Engine shall discard any high-score CSV line that fails validation and load the rest. A valid entry has initials of exactly three letters A–Z, a score from 0 to 999,999 (SCORE_MAX) and a rank from 1 to 10.
- Parent: UR-12
- Rationale: a corrupted or hand-edited file must not crash the program or show invalid text on screen. Loading the table is SR-FW-08; SCORE_MAX also fits the 6-digit score shown in the HUD.
- Verification: Test (start with a file containing bad initials, out-of-range scores and binary junk; valid entries load and the program runs)

**SC-05** On start-up, the Input Manager shall ignore any line of `/etc/arcade/keymap.conf` that names an unknown context, key code or action, and use the default binding for that key.
- Parent: UR-02
- Rationale: a broken override file must not leave a screen without working keys.
- Verification: Test (keymap file with invalid lines; all default keys still work)

**SC-06** The Linux image shall have a non-empty root password.
- Parent: UR-01
- Rationale: the serial console and any local login are the remaining ways into the board. Some board images ship with an empty root password.
- Verification: Inspection (log in on the serial console; a password is required)

---

---

## 5 TBL-SoundEffect — note data (Huzaifa's input to Section 5)

The table gives the notes that `playSound(effectId)` plays for each of the 7 sound effects (UR-18; events in SR-GAM-15 to SR-GAM-22). Mujtaba owns the table definition in Section 5; this is its content.

**Rules the data follows:**

- **Frequency:** every note is 100–4,000 Hz (SR-AUD-04). Pitches are equal-tempered notes, rounded to 1 Hz.
- **Effect length:** every effect lasts 500 ms or less in total.
- **Note length:** every note is 33–150 ms, inside the Tone Generator's 10–2,000 ms range (SR-AUD-05).
- **Note timing:** each note's duration is a whole number of game ticks (1 tick = 16.7 ms). The HAL writes one note to the Tone Generator registers and writes the next note in the tick when the previous one ends, so no extra hardware is needed. The first note starts in the same frame as the call (PR-08).
- **Replacing an effect:** a new `playSound()` call replaces any effect still playing (one voice, CON-13 and SR-AUD-06).
- **Pitch order:** short high blips mark small positive events, falling phrases mark losses, and the paddle sounds lower than a brick so the two can be told apart.

**Table layout:** one row per note; the key is effect_id + note_no.

| Field | Type | Constraint |
| --- | --- | --- |
| effect_id | UINT8 | 0–6; part of the PK |
| name | VARCHAR(12) | Unique per effect_id |
| note_no | UINT8 | 1–4, play order; part of the PK |
| freq_hz | UINT16 | 100–4,000 (SR-AUD-04) |
| dur_ms | UINT16 | 10–2,000 (SR-AUD-05); a whole number of ticks |

**Contents**

| effect_id | Effect | Notes in order: pitch, Hz, ms (ticks) | Total |
| --- | --- | --- | --- |
| 0 | Eat | G5 784 Hz, 33 ms (2) → C6 1,047 Hz, 50 ms (3) | 83 ms |
| 1 | BrickHit | E6 1,319 Hz, 33 ms (2) | 33 ms |
| 2 | PaddleHit | C5 523 Hz, 33 ms (2) | 33 ms |
| 3 | LifeLost | G4 392 Hz, 100 ms (6) → E4 330 Hz, 100 ms (6) → C4 262 Hz, 150 ms (9) | 350 ms |
| 4 | GameOver | C5 523 Hz, 117 ms (7) → G4 392 Hz, 117 ms (7) → E4 330 Hz, 117 ms (7) → C4 262 Hz, 133 ms (8) | 484 ms |
| 5 | MenuMove | A5 880 Hz, 33 ms (2) | 33 ms |
| 6 | MenuSelect | E5 659 Hz, 50 ms (3) → C6 1,047 Hz, 83 ms (5) | 133 ms |

Range check: the lowest note is 262 Hz and the highest 1,319 Hz, both inside 100–4,000 Hz. The longest effect is GameOver at 484 ms, below the 500 ms limit.

---

## Glossary (Huzaifa's terms, for 1.4)

| Term | Definition |
| --- | --- |
| Active region | The part of each line and frame in which pixels are visible: 1024 × 768 in the operating mode. |
| ADV7123 | Analog Devices triple 10-bit video DAC on the DE1-SoC that converts digital RGB to analog VGA. |
| Atomic replace | Replacing a file so that readers see either the whole old file or the whole new one, never a partial file. |
| Auto-repeat | Repeated key-press events that the OS generates while a key is held. |
| Back porch | Blank interval between the end of a sync pulse and the start of active video. |
| BCLK | Bit clock of the codec's digital audio interface; one pulse per data bit. |
| Blanking | Intervals (front porch, sync, back porch) in which no picture is sent. |
| Bring-up mode | 640×480 @ 60 Hz video mode used during development to test the video path. |
| Codec | Coder/decoder chip that converts between digital audio samples and analog sound (here the WM8731). |
| D-SUB | 15-pin connector used for analog VGA. |
| DAC | Digital-to-analog converter. |
| EV_KEY | Linux input-event type for key and button changes. |
| evdev | Linux generic input-event interface; exposes each input device as `/dev/input/eventN`. |
| EVIOCGRAB | ioctl that gives one process exclusive access to an input device. |
| Flash (WCAG) | A pair of opposite changes in relative luminance of 10 % or more, where the darker image is below 0.80 relative luminance. |
| Front porch | Blank interval between the end of active video and the start of a sync pulse. |
| fsync | Linux call that writes a file's data to the storage device before returning. |
| Game tick | One iteration of the game loop; 60 ticks per second, locked to the display frame. |
| HID | Human Interface Device; the USB device class for keyboards and mice. |
| HPS | Hard Processor System; the dual-core ARM Cortex-A9 part of the Cyclone V SoC. |
| HSYNC / VSYNC | Horizontal and vertical synchronisation pulses that tell the monitor where a line or frame starts. |
| I2C | Two-wire serial bus used to write the codec's control registers. |
| I²S | Serial format for digital audio samples, with a bit clock and a left/right clock. |
| inotify | Linux mechanism that reports file-system changes, used to detect a keyboard being plugged in. |
| Key code | Linux number identifying a key (`KEY_*`, for example KEY_A). |
| Latency budget | Worst-case time of each stage of a path, added up to check a timing requirement. |
| Line-out | 3.5 mm analog audio output at line level, for powered speakers or headphones. |
| LRCK (DACLRCK) | Left/right clock of the codec; one period per stereo sample frame (48 kHz here). |
| M10K | 10-kilobit embedded memory block in the Cyclone V FPGA. |
| MCLK (AUD_XCK) | Codec master clock; 12.288 MHz for a 48 kHz sample rate. |
| MLAB | Memory logic array block; small distributed RAM in the Cyclone V FPGA. |
| MSEL | FPGA configuration-mode pins, set by switch SW10 on the DE1-SoC. |
| Mute | Setting that silences all sound; toggled with M. |
| Photosensitive epilepsy | Seizures triggered by flashing light or patterns. |
| Pixel clock | Clock at which pixels are sent to the display: 65 MHz in the operating mode. |
| PLL | Phase-locked loop; FPGA block that generates the pixel clock from the 50 MHz board clock. |
| RS-343A | Analog video level standard (0 V black to 0.7 V white) used by VGA. |
| Sample rate | Number of audio samples per second per channel: 48 kHz here. |
| Scan-out | Sending one frame to the monitor, line by line from top to bottom. |
| Square wave | Waveform that switches between two levels; gives a simple beep. |
| Tile | 8 × 8 pixel graphic; the screen is a 128 × 96 grid of tiles. |
| Tone Generator | FPGA block that makes square-wave tones and sends them to the codec. |
| uinput | Linux interface that lets a program create a virtual input device and inject key events, used for testing. |
| ULPI | UTMI+ Low Pin Interface; links the HPS USB controller to the USB PHY. |
| USB | Universal Serial Bus. |
| VESA DMT | VESA Display Monitor Timing standard, which defines XGA and other modes. |
| VGA | Video Graphics Array; analog RGB video interface with separate sync signals. |
| WCAG | Web Content Accessibility Guidelines (W3C); 2.3.1 limits flashing content. |
| WM8731 | Wolfson 24-bit audio codec on the DE1-SoC board. |
| XGA | 1024 × 768 display mode; used here at 60 Hz with a 65 MHz pixel clock. |
