# 1 Introduction

The FPGA Arcade Platform is a self-contained arcade machine built on a single Terasic DE1-SoC board. Game logic runs as C++ software under embedded Linux on the board's ARM processor, while a custom graphics and sound subsystem in the board's FPGA fabric draws the picture on a VGA monitor and plays sound effects. This chapter states the purpose and scope of this document, who it is written for, the terms and conventions it uses, and the sources it draws on.

## 1.1 Document Purpose

This Software Requirements Specification (SRS) defines the requirements for the **FPGA-Accelerated Arcade Gaming Platform, release 1.0** (the *FPGA Arcade Platform* in the rest of this document), the CS-326 Software Engineering complex engineering problem of practical group G2. It is revision 1.0 of the SRS, issued on 19 October 2026.

The SRS covers the **whole system**: the C++ program on the Hard Processor System (game framework, Arcade API, two games, Input Manager and Hardware Access Layer) and the FPGA design (Tile Renderer, VGA Controller and Tone Generator), together with the interfaces between them. It is the agreed basis for design, implementation and testing: every requirement has an identifier, a parent user requirement and a verification method, so each one can be traced from stakeholder need to test.

## 1.2 Product Scope

The product is a dedicated arcade cabinet on one board. A player connects a VGA monitor, a USB keyboard and, optionally, speakers or headphones, powers the board on and reaches a menu of games without any PC. Release 1.0 provides two games, **Snake** and **Breakout**, a persistent top-10 high-score table for each game, simple sound effects, and a safe shutdown.

The design separates **meaning from pixels**. The processor owns all game state, rules, scoring and menus; the FPGA owns only the picture and the sound. The two halves meet at a small, frozen interface: a memory-mapped tilemap, a set of registers and the Arcade API. The benefits are:

- **Reuse (product line):** a new game is a new C++ class and one registry entry. The FPGA design, the HAL and the framework do not change (UR-16).
- **Testability:** the same games run on a PC through an SDL2 simulation backend, so game logic can be developed and unit-tested before the hardware is ready (UR-17).
- **Real-time quality:** the FPGA produces a stable 1024 × 768 picture at 60 Hz in hardware, with double-buffered tilemaps so the display never tears (UR-06, UR-13).

**Out of scope for release 1.0:** Space Invaders and any other sprite-based game (future scope, because the renderer is tile-only), multiplayer, networking and recorded audio.

**Change from the approved proposal:** the proposal named a PS/2 keyboard as the input device. On the DE1-SoC the PS/2 port connects only to the FPGA, while the two USB host ports connect to the processor. Release 1.0 therefore uses a **USB keyboard read through Linux**, which removes a custom FPGA input decoder and lowers project risk (decision of 13 Oct, CON-07). The instructor was informed of this change.

## 1.3 Intended Audience and Document Overview

| Reader | Uses this SRS to | Suggested reading order |
|---|---|---|
| Course instructor (client and assessor) | Check that scope, stakeholders, requirements, models and data design are complete and traceable | 1 → 2 → 3.2 → 3.3 → 5 → Appendix B |
| Team developers (C++ and SystemVerilog) | Build each component to its requirements and interfaces | 2.1 → 3.1 → 3.2 (own subsystem) → 3.3 → Appendix A |
| Testers | Derive a test for every requirement from its verification method | 3.2 → 4 → Appendix B |
| Future game developers | Add a game without changing hardware | 1.2 → 2.2 → 3.1.3 → SR-FW → 5 |
| Lab staff | Set up, power and safely shut down the board | 2.4 → 2.6 → 4.2 |

**Organisation.** Chapter 1 introduces the document. Chapter 2 describes the product as a whole: its context, functions and user requirements, stakeholders and users, environment, constraints and assumptions. Chapter 3 gives the specific requirements: external interfaces, system functional requirements grouped by subsystem, and the behavioural models (use cases, state machines and sequence diagrams). Chapter 4 gives the non-functional requirements: performance, safety and security, and quality attributes. Chapter 5 specifies the database tables. Appendix A is the data dictionary, Appendix B the traceability matrix and Appendix C the gridded UI mockups.

## 1.5 Document Conventions

**Formatting.** The document follows the IEEE SRS layout of the course template: Arial-style sans-serif text (Helvetica in LaTeX), 11 pt, single spacing and 1-inch margins. Code, file paths, register names and API calls are set in `monospace`. Diagrams follow UML 2 notation as used in the course labs (StarUML style).

**Requirement identifiers.**

| Prefix | Meaning |
|---|---|
| UR-nn | User requirement (stakeholder view, section 2.2) |
| SR-FW-nn, SR-UI-nn, SR-GAM-nn | System requirements: framework, screens and navigation, game rules |
| SR-INP-nn, SR-VGA-nn, SR-AUD-nn | System requirements: keyboard input, VGA output, sound |
| SR-HAL-nn, SR-REN-nn | System requirements: Hardware Access Layer, Tile Renderer |
| PR-nn, SF-nn, SC-nn, QA-nn | Performance, safety, security and quality-attribute requirements |
| CON-nn, AS-nn | Design constraints, assumptions and dependencies |
| UC-nn, TBL-name | Use cases, database tables |

Identifiers are never reused. A number that was retired during review (SR-REN-02) is left unused so that cross-references stay valid.

**Requirement wording.** *Shall* marks a mandatory requirement, *should* a desirable one (all sound requirements derived from UR-18), and *will* a statement of fact. Each system requirement is one testable statement with a single parent user requirement, a rationale and a verification method: Test, Inspection, Analysis or Demonstration. Quantities always carry units.

**Component names.** The same names are used throughout: Game Engine, Arcade API, HAL, Input Manager, Tilemap RAM, Tile Renderer, VGA Controller and Tone Generator.

## 1.6 References and Acknowledgments

[1] ISO/IEC/IEEE, *Systems and software engineering — Life cycle processes — Requirements engineering*, ISO/IEC/IEEE 29148:2018.

[2] IEEE, *IEEE Recommended Practice for Software Requirements Specifications*, IEEE Std 830-1998.

[3] I. Sommerville, *Software Engineering*, 10th ed. Boston, MA, USA: Pearson, 2016.

[4] Terasic Inc., *DE1-SoC User Manual*, rev. E, 2015. [Online]. Available: https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf

[5] Intel Corp., *Cyclone V Hard Processor System Technical Reference Manual*. [Online]. Available: https://www.intel.com/content/www/us/en/docs/programmable/683126/

[6] Intel Corp., *Cyclone V Device Overview*. [Online]. Available: https://www.intel.com/content/www/us/en/docs/programmable/683694/current/embedded-memory-capacity-in-cyclone.html

[7] Analog Devices, *ADV7123 CMOS 330 MHz Triple 10-Bit High Speed Video DAC*, data sheet, rev. D. [Online]. Available: https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7123.pdf

[8] Wolfson Microelectronics, *WM8731 Portable Internet Audio CODEC with Headphone Driver*, data sheet, rev. 3.4. [Online]. Available: https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/Wolfson-WM8731-audio-CODEC.pdf

[9] VESA, *VESA and Industry Standards and Guidelines for Computer Display Monitor Timing (DMT)*, ver. 1.0, rev. 13, 2013.

[10] The Linux Kernel Documentation, "Input subsystem" and "Input event codes." [Online]. Available: https://www.kernel.org/doc/html/latest/input/input.html

[11] The Linux Kernel Documentation, "FPGA Manager." [Online]. Available: https://docs.kernel.org/driver-api/fpga/fpga-mgr.html

[12] Linux man-pages project, *mmap(2)*, *mem(4)*, *rename(2)*, *fsync(2)*, *inotify(7)*. [Online]. Available: https://man7.org/linux/man-pages/

[13] W3C, *Web Content Accessibility Guidelines (WCAG) 2.1*, "Understanding SC 2.3.1: Three Flashes or Below Threshold." [Online]. Available: https://www.w3.org/WAI/WCAG21/Understanding/three-flashes-or-below-threshold.html

[14] SDL Project, *Simple DirectMedia Layer 2.0*. [Online]. Available: https://www.libsdl.org/

[15] Google, *GoogleTest User's Guide*. [Online]. Available: https://google.github.io/googletest/

[16] NED University of Engineering & Technology, *CS-326 Software Engineering: Complex Engineering Problem brief and rubric*, Fall 2026; and *Lab Session 05: Behavioral view UML diagrams*.

[17] M. A. Qazi, H. Hanif and M. J. Rao, *FPGA-Accelerated Arcade Gaming Platform: Project Proposal*, CS-326, NED University, 10 Sep 2026.

**Acknowledgments.** The team thanks Dr. Fakhra Aftab, course instructor, for approving the project and guiding the requirements process.

---

# 2 Overall Description

## 2.2 Product Functionality

The system performs the following major functions:

- **Start-up and hardware check:** boot from the microSD card, check that the FPGA design is loaded, and show the main menu within 60 s.
- **Game selection:** list the registered games and the high-score viewer in a menu with a live preview.
- **Gameplay:** run Snake and Breakout at a fixed 60 ticks per second, with pause, resume and quit.
- **Keyboard input:** read a USB keyboard, map keys to actions per screen, and survive the keyboard being unplugged.
- **Display:** draw every screen on a 128 × 96 tile grid and output 1024 × 768 VGA at 60 Hz without tearing.
- **Sound effects:** play short tones for game events, with mute (desirable).
- **High scores:** keep a top-10 table per game on the microSD card, entered with three initials, safe against power loss.
- **Safe shutdown:** save, show "SAFE TO SWITCH OFF" and halt Linux before power is removed.
- **Development support:** run the same games on a PC through an SDL2 backend, and add new games without changing the FPGA design.

The data flow diagrams below use Yourdon–DeMarco notation: circles are processes, squares are external entities, open-ended boxes are data stores, and labelled arrows are data flows.

- **Context diagram (DFD level 0):** the whole system as one process, exchanging data with the Player, Lab Staff and Game Developer. The USB keyboard, VGA monitor and speakers are the physical media for the Player's flows (section 2.4).
- **DFD level 1:** the system broken into six processes: 1.0 Read Input, 2.0 Run Game Logic, 3.0 Manage High Scores, 4.0 Render Display, 5.0 Generate Sound and 6.0 Control Lifecycle. It also shows four data stores: D1 high-score file, D2 tilemap banks, D3 tile patterns and palette, and D4 key bindings, game registry and sound effects.
- **DFD level 2 for process 4.0:** rendering split across HPS software (4.1 update the shadow tilemap, 4.2 copy cells and request a swap) and FPGA hardware (4.3 swap banks at vblank, 4.4 fetch tile and colour, 4.5 generate VGA timing). This is where data crosses from software into hardware.

*Figure 2.2a: Context diagram (`2.2-dfd-0-context`).*

*Figure 2.2b: DFD level 1 (`2.2-dfd-1`).*

*Figure 2.2c: DFD level 2, process 4.0 Render Display (`2.2-dfd-2-render`).*

### 2.2.1 User Requirements

The user requirements below state what each stakeholder needs, in natural language. Every system requirement in section 3.2 and every non-functional requirement in chapter 4 traces to exactly one of them (Appendix B).

| ID | User requirement | Stakeholder | Priority |
|---|---|---|---|
| UR-01 | The system shall start from power-on to the main menu without a PC connected. | Player | Shall |
| UR-02 | The player shall be able to choose a game from a main menu using the keyboard. | Player | Shall |
| UR-03 | The player shall be able to play Snake. | Player | Shall |
| UR-04 | The player shall be able to play Breakout. | Player | Shall |
| UR-05 | The player shall control the active game with a USB keyboard, and each keypress shall take effect on screen without noticeable delay. | Player | Shall |
| UR-06 | The system shall display the game on a standard VGA monitor at 1024 × 768. | Player | Shall |
| UR-07 | The system shall show the current score, and lives where the game has them, during play. | Player | Shall |
| UR-08 | The player shall be able to pause and resume a game. | Player | Shall |
| UR-09 | The player shall be able to leave a game and return to the main menu at any time. | Player | Shall |
| UR-10 | When a game ends, the system shall show the final score and let the player play again or return to the menu. | Player | Shall |
| UR-11 | When a score qualifies for the high-score table, the player shall be able to enter three initials. | Player | Shall |
| UR-12 | The player shall be able to view the top 10 scores for each game, and these shall be kept after power-off. | Player | Shall |
| UR-13 | The display shall never show a torn or half-updated picture. | Player | Shall |
| UR-14 | If the keyboard is unplugged, the system shall keep running and accept input again when it is reconnected. | Player | Shall |
| UR-15 | If the graphics hardware is not ready, the system shall report it instead of hanging. | Player, lab staff | Shall |
| UR-16 | A developer shall be able to add a new game without changing the FPGA design or the HAL. | Game developer | Shall |
| UR-17 | A developer shall be able to run and debug the games on a PC without the DE1-SoC board. | Game developer | Shall |
| UR-18 | The system should play short sound effects for game events (food eaten, brick hit, game over, menu move), and the player should be able to mute them. | Player | Should |
| UR-19 | The player shall be able to shut the system down safely from the main menu before switching off power. | Player, lab staff | Shall |

## 2.3 Users and Characteristics

### 2.3.1 Stakeholders and Viewpoints

Stakeholders are grouped by viewpoint, following the viewpoint-oriented approach of Sommerville [3]: **interactor** viewpoints interact with the system directly, **indirect** viewpoints have an interest in the system without using it during play, and **domain** viewpoints are standards and platform characteristics that constrain it.

| Viewpoint | Stakeholder | Interest in the system | Key user requirements |
|---|---|---|---|
| Interactor | Player | Plays the games, reads the screen, uses the keyboard and speakers | UR-01 to UR-14, UR-18, UR-19 |
| Interactor | USB keyboard, VGA monitor, speakers (external devices) | Exchange signals with the system at its physical interfaces | UR-05, UR-06, UR-18 |
| Indirect | Course instructor (client and assessor) | Defines the problem, approves scope, grades the SRS and the final demonstration | All; 1.2 scope |
| Indirect | Game developer (team now, future students) | Adds and tests games through the Arcade API without touching hardware | UR-16, UR-17 |
| Indirect | Lab staff | Provides boards and monitors, powers the system on and off, diagnoses faults | UR-15, UR-19 |
| Indirect | Development team | Builds, integrates and tests the HPS software and FPGA design within the semester | CON-08, CON-11 |
| Domain | VESA display timing and VGA analog levels | Fix the 1024 × 768 at 60 Hz timing and the signal levels | UR-06; SR-VGA |
| Domain | USB HID class and the Linux input subsystem | Fix how key events reach the program | UR-05, UR-14; SR-INP |
| Domain | DE1-SoC board and Cyclone V SoC | Fix the bridge, memory, clocks, audio codec and pin assignments | CON-01 to CON-04 |
| Domain | Photosensitivity guidance (WCAG 2.3.1) | Limits flashing content | SF-01 |

### 2.3.2 User Classes

| User class | Frequency of use | Technical expertise | Functions used | Importance |
|---|---|---|---|---|
| Player | Every session | None assumed; reads English text on screen | Menu, both games, pause, high scores, sound, shutdown | **Primary**: the system succeeds only if a first-time player can start a game unaided (QA-06) |
| Game developer | During development | C++17 and the Arcade API; no FPGA knowledge required | SDL2 backend, Arcade API, game interface, TBL-GameRegistry | **Secondary**: extensibility is a core design goal (UR-16, UR-17) |
| Lab staff | At setup and shutdown | Basic lab equipment handling | Power on, safe shutdown, serial-console error messages | **Secondary** |
| Course instructor | At assessment | Software engineering expert | Observes all functions during the demonstration | **Primary** for acceptance, not for daily use |

The system has a single privilege level for players; there are no logins or accounts. Developers and lab staff use the serial console, which is outside the player interface (4.2.1).

---

# 3 Specific Requirements

## 3.2 System Functional Requirements — Muhammad's subsystems

### SR-FW — Game framework, Arcade API and game loop

**SR-FW-01** At start-up, the Game Engine shall initialise the HAL, load the tile patterns and palette, set display enable and show the main menu within 60 s of power being applied.
- Parent: UR-01
- Rationale: decision D1 fixes the boot-to-menu time; PR-07 measures it. Display enable is set only after tiles and palette are loaded, so no undefined picture is shown (SR-HAL-13, SR-VGA-09).
- Verification: Test (stopwatch from power switch to menu, 5 power cycles)

**SR-FW-02** The Game Engine shall run exactly one game tick per displayed frame, 60 ticks per second, each tick performing in order: `pollInput()`, the active screen's `update()`, `render()` and `present()`; `present()` shall lock the loop to the display refresh.
- Parent: UR-05
- Rationale: all game speeds are counted in ticks, so gameplay speed is independent of processor load, and input is read once per frame, which bounds input latency (PR-02, PR-03).
- Verification: Test (over 10 minutes, the tick count equals the change in `frameCount()`)

**SR-FW-03** Every game shall implement the game interface `init()`, `update(input)`, `render(api)`, `isOver()` and `score()`, and the Game Engine shall call games only through this interface.
- Parent: UR-16
- Rationale: one fixed interface makes games interchangeable parts of a product line; the engine never depends on a specific game.
- Verification: Inspection (code review: no game-specific calls in the Game Engine)

**SR-FW-04** The main menu shall be built from TBL-GameRegistry at start-up, so that adding a game requires only a new class and one registry row, with no change to the Game Engine, the HAL or the FPGA design.
- Parent: UR-16
- Rationale: makes extensibility a property of the design rather than of developer discipline (QA-02).
- Verification: Demonstration (add a test game; it appears in the menu after rebuilding the C++ program only)

**SR-FW-05** The rendering backend, FPGA or SDL2, shall be selected by a start-up option, and the game code shall be identical for both backends.
- Parent: UR-17
- Rationale: proves the Arcade API is a real abstraction and allows development before the hardware is complete.
- Verification: Test (the same game build runs on both backends)

**SR-FW-06** The SDL2 backend shall display the 128 × 96 tilemap in a single 1024 × 768 window and take keyboard input from the PC keyboard, mapped to the same actions as on the board.
- Parent: UR-17
- Rationale: the PC simulation must look and behave like the board so that game behaviour seen on the PC is the behaviour on the hardware.
- Verification: Demonstration (play both games on a PC)

**SR-FW-07** The SDL2 backend shall accept every `playSound()` and `setMute()` call without error; producing audible sound on the PC is optional.
- Parent: UR-17
- Rationale: games must not need backend-specific code paths for sound (SR-FW-05).
- Verification: Test (unit test calls every effect on the SDL2 backend)

**SR-FW-08** At start-up, the Game Engine shall load TBL-HighScore from `/var/lib/arcade/highscores.csv`; if the file is missing or unreadable it shall start with an empty table, and it shall discard invalid lines (SC-04).
- Parent: UR-12
- Rationale: a missing or damaged file must never stop the arcade from starting.
- Verification: Test (start with no file, an empty file and a corrupted file)

**SR-FW-09** The Game Engine shall save TBL-HighScore by writing a temporary file, calling `fsync()`, and then calling `rename()` over the existing file.
- Parent: UR-12
- Rationale: `rename()` replaces the file atomically, so a power loss leaves either the complete old or the complete new table (decision D2, SF-04).
- Verification: Test (power cut during repeated saves, 20 times)

**SR-FW-10** The Game Engine shall keep the top 10 scores per game, sorted by score in descending order; equal scores shall be ranked by entry order, the older entry first.
- Parent: UR-12
- Rationale: defines a single, deterministic ranking for the high-score screen.
- Verification: Test (unit test with ties and an 11th entry)

**SR-FW-11** Sound should be on at start-up, and each Mute action should toggle it through `setMute()`; the setting should last until power-off.
- Parent: UR-18
- Rationale: decision D3; the amplitude cap (SR-AUD-07, SF-05), not muting, protects headphone users.
- Verification: Test

**SR-FW-12** `drawText()` shall support scale 1, one font tile per character, and scale 4, built from the 16 quadrant-mosaic tiles in which each tile shows 2 × 2 font pixels.
- Parent: UR-06
- Rationale: readable titles at 1024 × 768 while using only 16 of the 256 tile patterns for large text.
- Verification: Inspection (screens match the 3.1.1 mockups)

**SR-FW-13** On a confirmed shutdown, the Game Engine shall, in order: mute sound; complete any pending high-score save; draw "SAFE TO SWITCH OFF" and `present()` it; call the HAL's `shutdown()` without clearing display enable; and run `poweroff`.
- Parent: UR-19
- Rationale: halting Linux before power is removed protects the microSD card; keeping display enable set leaves the message on screen until power-off (SR-HAL-07, SF-07).
- Verification: Test (shutdown, then check the message stays and the card's file system is clean on the next boot)

**SR-FW-14** On receiving SIGTERM or SIGINT, the Game Engine shall perform the steps of SR-FW-13 except `poweroff`, and then exit.
- Parent: UR-19
- Rationale: stopping the program from the serial console during development must also leave the hardware released and the scores saved.
- Verification: Test (Ctrl+C on the serial console during play)

### SR-UI — Screens and navigation

**SR-UI-01** The main menu shall list every game in TBL-GameRegistry followed by SCORES; Up and Down shall move the selection, wrapping at both ends.
- Parent: UR-02
- Rationale: one consistent selection model for any number of games.
- Verification: Test

**SR-UI-02** The main menu shall show an animated preview of the selected game in its preview panel.
- Parent: UR-02
- Rationale: lets a first-time player recognise each game before starting it (QA-06).
- Verification: Inspection (against 3.1.1 screen 2)

**SR-UI-03** Select on a game entry shall start that game within 1 tick.
- Parent: UR-02
- Rationale: immediate response to the start key.
- Verification: Test

**SR-UI-04** During play, the HUD shall show the game name, the score as 6 digits, the lives (Breakout only) and the game's current high score, updated every tick.
- Parent: UR-07
- Rationale: the player always sees progress; 6 digits cover SCORE_MAX = 999,999.
- Verification: Inspection (against 3.1.1 screens 3 and 4)

**SR-UI-05** Pause during play shall stop all game updates and show the Paused box over the frozen game picture.
- Parent: UR-08
- Rationale: the game state must not change while paused.
- Verification: Test (game state identical before and after a 60 s pause)

**SR-UI-06** Pause in the Paused state shall resume the game from exactly the state in which it was paused.
- Parent: UR-08
- Rationale: completes the pause cycle without loss.
- Verification: Test

**SR-UI-07** Back in the Playing or Paused state shall return to the main menu without saving the score.
- Parent: UR-09
- Rationale: quitting is the player's choice to abandon the game.
- Verification: Test

**SR-UI-08** When a game ends, the system shall go to Initials entry if the score qualifies for that game's top 10, and to Game Over otherwise.
- Parent: UR-10
- Rationale: decision D4; a qualifying player is asked for initials straight away.
- Verification: Test (one qualifying and one non-qualifying score)

**SR-UI-09** The Game Over screen shall show the final score; Select shall restart the same game and Back shall return to the main menu.
- Parent: UR-10
- Rationale: offers both choices required by UR-10.
- Verification: Test

**SR-UI-10** Initials entry shall accept letters A to Z up to three letters, Delete shall remove the last letter, and Select shall be accepted only when three letters have been entered; the entry shall then be saved and the High Scores screen shown with the new entry highlighted.
- Parent: UR-11
- Rationale: initials are exactly three letters (TBL-HighScore); highlighting confirms the save.
- Verification: Test

**SR-UI-11** On a KeyboardLost event in the Playing or Paused state, the system shall pause the game and show the Keyboard Disconnected screen within 1 tick; when a keyboard is found again, the game shall stay paused until the player presses Pause.
- Parent: UR-14
- Rationale: the player must never lose a game to a pulled cable (decision D5, SF-03).
- Verification: Test (unplug during play)

**SR-UI-12** The High Scores screen shall show the rank, initials and score of the top 10 entries for one game; Left and Right shall switch game and Back shall return to the main menu.
- Parent: UR-12
- Rationale: shows the table required by UR-12 one game at a time.
- Verification: Inspection (against 3.1.1 screen 8)

**SR-UI-13** Every screen shall show a hint bar listing that screen's keys and the current sound state.
- Parent: UR-02
- Rationale: the controls are always visible, so no manual is needed to play (QA-06).
- Verification: Inspection (all 11 screens)

**SR-UI-14** Holding Back for 3 s (180 ticks, timed from KeyDown using the held-key state) on the main menu shall show the shutdown confirmation; Select shall confirm the shutdown and Back shall cancel it.
- Parent: UR-19
- Rationale: a held key with confirmation prevents an accidental shutdown; timing from KeyDown works because auto-repeat is discarded (SR-INP-07).
- Verification: Test (hold 2 s: no box; hold 3 s: box appears)

**SR-UI-15** On a KeyboardLost event in the Menu, Game Over, Initials or High Scores state, the system shall remain in that state and show the "no keyboard" notice until a keyboard is found.
- Parent: UR-14
- Rationale: decision D5; outside gameplay there is nothing to pause, so only a notice is needed.
- Verification: Test (unplug on the main menu)

### SR-GAM — Game rules

**Snake**

**SR-GAM-01** The Snake board shall be 42 × 30 cells, each drawn as 3 × 3 tiles, occupying tile columns 1–126 and rows 3–92, with walls on its border.
- Parent: UR-03
- Rationale: 24-pixel cells are clearly visible at 1024 × 768 and the board fills the play field exactly.
- Verification: Inspection (against 3.1.1 screen 3)

**SR-GAM-02** A Snake game shall start in the Ready state with a snake of length 4 in the centre of the board heading right; the first arrow key shall start movement.
- Parent: UR-03
- Rationale: the player starts the game when ready.
- Verification: Test

**SR-GAM-03** The snake shall move one cell every `stepTicks` ticks; `stepTicks` shall start at 6 and decrease by 1 after every 5 food items eaten, to a minimum of 3.
- Parent: UR-03
- Rationale: speed rises from 10 to 20 cells per second as the game progresses.
- Verification: Test (unit test of speed against food count)

**SR-GAM-04** An arrow key shall set the direction for the snake's next move; a key that reverses the current direction shall be ignored, and at most one direction change shall apply per move.
- Parent: UR-05
- Rationale: prevents the snake from turning into itself through two quick key presses.
- Verification: Test (unit test: Up then Left within one move)

**SR-GAM-05** Food shall appear at a random cell not occupied by the snake; eating it shall grow the snake by one cell and add 10 points.
- Parent: UR-03
- Rationale: core Snake rule.
- Verification: Test

**SR-GAM-06** The game shall end when the snake's head moves into a wall or into the snake's body; the final frame shall then be held, without flashing, for 30 ticks.
- Parent: UR-03
- Rationale: core Snake rule; the still frame satisfies SF-01.
- Verification: Test

**Breakout**

**SR-GAM-07** The Breakout wall shall be 21 × 6 bricks, each 5 × 2 tiles in a 6 × 2-tile slot, on tile rows 8–19; the paddle shall be 16 × 1 tiles on row 90 and the ball 2 × 2 tiles.
- Parent: UR-04
- Rationale: fills the screen width (columns 1–126) and keeps the ball clearly visible.
- Verification: Inspection (against 3.1.1 screen 4)

**SR-GAM-08** In the Serve state the ball shall rest on the paddle and move with it; Launch shall start the ball moving upward diagonally.
- Parent: UR-04
- Rationale: the player chooses when each ball starts.
- Verification: Test

**SR-GAM-09** The paddle shall move one tile per tick while Left or Right is held, and shall stop at the side walls.
- Parent: UR-05
- Rationale: held-key movement independent of the keyboard's auto-repeat rate (SR-INP-07, SR-INP-08).
- Verification: Test

**SR-GAM-10** The ball shall move one tile diagonally every 2 ticks on level 1 and every tick from level 2 onwards.
- Parent: UR-04
- Rationale: difficulty increases after the first wall is cleared.
- Verification: Test

**SR-GAM-11** The ball shall bounce off the side walls, the top wall, the paddle and bricks; after a paddle hit its horizontal direction shall be set by the half of the paddle it hit.
- Parent: UR-04
- Rationale: lets the player aim the ball.
- Verification: Test (unit test of each bounce case)

**SR-GAM-12** A ball hitting a brick shall remove the brick and add points by row: 60 for the top row down to 10 for the bottom row.
- Parent: UR-07
- Rationale: higher bricks are harder to reach and are worth more.
- Verification: Test

**SR-GAM-13** A Breakout game shall start with 3 lives; when the ball passes below the paddle one life shall be lost and the game shall return to Serve, and when no lives remain the game shall end.
- Parent: UR-04
- Rationale: core Breakout rule.
- Verification: Test

**SR-GAM-14** When the last brick is removed, the game shall pause for 60 ticks, draw a new full wall and increase the level by 1.
- Parent: UR-04
- Rationale: the game continues with rising difficulty instead of ending.
- Verification: Test

**Sound events** (desirable; all with parent UR-18 and verification by Test)

**SR-GAM-15** Moving the menu selection should play the MenuMove effect.

**SR-GAM-16** Starting a game should play the MenuSelect effect.

**SR-GAM-17** The snake eating food should play the Eat effect.

**SR-GAM-18** The snake dying should play the GameOver effect.

**SR-GAM-19** The Breakout ball hitting a brick should play the BrickHit effect.

**SR-GAM-20** The Breakout ball hitting the paddle should play the PaddleHit effect.

**SR-GAM-21** Losing a Breakout life with lives remaining should play the LifeLost effect.

**SR-GAM-22** Losing the last Breakout life should play the GameOver effect.

- Parent (SR-GAM-15 to 22): UR-18
- Rationale: audible feedback for each event in UR-18; effects are defined once in TBL-SoundEffect.
- Verification: Test (each event triggers exactly one `playSound()` call with the listed effect)

## 3.3 Behaviour Requirements

### 3.3.1 Use Case View

Figure 3.1 shows the use case diagram for the whole system. The **Player** is the primary actor. The **Game Developer** uses the system through the SDL2 backend and the Arcade API. **Lab Staff** set up and shut down the board and read fault reports. The **USB Keyboard** and **VGA Monitor** are secondary actors that the system reads from and writes to.

*Figure 3.1: Use case diagram (`3.3-usecase`).*

| ID | Use case | Actors | Description | URs |
|---|---|---|---|---|
| UC-01 | Start System | Player, Lab Staff | Power on; the system boots, checks the FPGA design and shows the main menu, or reports "graphics hardware not ready". | UR-01, UR-15 |
| UC-02 | Select Game | Player | Move through the menu and start a game. | UR-02 |
| UC-03 | Play Snake | Player | Steer the snake to eat food without hitting a wall or itself. | UR-03, UR-05, UR-07 |
| UC-04 | Play Breakout | Player | Move the paddle to keep the ball in play and clear the wall. | UR-04, UR-05, UR-07 |
| UC-05 | Pause and Resume | Player | Extends Play Game: freeze and continue the game. | UR-08 |
| UC-06 | Quit to Menu | Player | Extends Play Game: abandon the game. | UR-09 |
| UC-07 | Enter Initials | Player | Extends Play Game when the score is in the top 10: type three initials. | UR-10, UR-11 |
| UC-08 | View High Scores | Player | Browse the top 10 of each game. | UR-12 |
| UC-09 | Toggle Sound | Player | Mute or unmute sound effects from any screen except Initials entry. | UR-18 |
| UC-10 | Reconnect Keyboard | Player | Plug the keyboard back in after it was unplugged; the game waits paused. | UR-14 |
| UC-11 | Shut Down System | Player, Lab Staff | Hold Esc on the menu, confirm, wait for "SAFE TO SWITCH OFF", switch off. | UR-19 |
| UC-12 | Add New Game | Game Developer | Implement the game interface, register the game and rebuild the C++ program only. | UR-16 |
| UC-13 | Run Games on PC | Game Developer | Run and debug games in an SDL2 window without the board. | UR-17 |

UC-03 and UC-04 specialise the abstract use case *Play Game*; UC-05, UC-06 and UC-07 extend Play Game, so they apply to every game, including games added later (UC-12). Play Game exchanges key events with the USB Keyboard and pictures with the VGA Monitor. The detailed flows of these use cases are given by the application state machines and the sequence diagrams below; full use case descriptions belong to the test plan.

### 3.3.2 State Machines and Interaction Models

The system's behaviour is specified by the following models, each in its own figure:

| Figure | Model | Owner |
|---|---|---|
| `3.3-sm-app-1-menu`, `3.3-sm-app-2-play` | Application state machine: start-up, menu and shutdown; gameplay | Muhammad |
| `3.3-sm-snake`, `3.3-sm-breakout` | Game state machines | Muhammad |
| `3.3-sm-input-manager`, `3.3-seq-keypress` | Input Manager state machine; keypress sequence | Huzaifa |
| `3.3-sm-vga-1-horizontal`, `3.3-sm-vga-2-vertical` | VGA Controller timing state machine | Huzaifa |
| `3.3-sm-hal-lifecycle`, `3.3-sm-renderer` | HAL lifecycle; Tile Renderer | Mujtaba |
| `3.3-seq-frame-1-request`, `-2-confirm`, `-3-display` | Frame-update sequence | Mujtaba |

**Application state machine.** The Game Engine is always in exactly one state: Boot, HardwareNotReady, Menu, HighScores, ShutdownConfirm, ShuttingDown, Playing, Paused, KeyboardLost, GameOver or Initials. Part 1 covers start-up, the menu and shutdown; part 2 covers gameplay. The transitions implement SR-FW-01, SR-FW-13, SR-UI-01 to SR-UI-15.

**Snake and Breakout state machines.** Each game has its own internal states below the application's Playing state: Snake moves through Ready, Running, Dead and Over; Breakout through Serve, Running, LevelClear and Over. A game reports the end of play through `isOver()`, which the Game Engine uses to leave Playing (SR-UI-08).

---

# 4 Other Non-functional Requirements

## 4.3 Software Quality Attributes

The quality attributes below are listed in priority order. Where attributes conflict, correctness of the real-time display and reliability come before extensibility, and extensibility before ease of learning.

### 4.3.1 Maintainability

**QA-01** The software shall be layered as Game → Arcade API → HAL → FPGA registers, and no layer shall call a layer above it or bypass the layer below it.
- Parent: UR-16
- How achieved: games include only the Arcade API header; only the HAL includes the register map; a build check fails if any other module includes it. Each layer has one owner and its own unit tests.
- Verification: Inspection (include-dependency report)

### 4.3.2 Reusability (product line)

**QA-02** Adding a new game shall require exactly one new class implementing the game interface and one TBL-GameRegistry row, with zero changes to the Game Engine, the HAL or the FPGA design.
- Parent: UR-16
- How achieved: the frozen game interface (SR-FW-03), the registry-built menu (SR-FW-04) and tile graphics loaded at start-up rather than fixed at synthesis (SR-REN-04). Design for change: the register map, tile size and palette are frozen design decisions recorded in Chapter 5.
- Verification: Demonstration (a third, minimal test game added by changing only those two items)

### 4.3.3 Portability

**QA-03** The same game source code shall build and run unchanged on the FPGA backend on the DE1-SoC and on the SDL2 backend on a Linux or Windows PC.
- Parent: UR-17
- How achieved: all hardware access sits behind the Arcade API (SR-FW-05); the game code uses only standard C++17.
- Verification: Test (both games built for both backends from one source tree)

### 4.3.4 Testability

**QA-04** Game logic shall be unit-tested on a PC with GoogleTest to at least 80 % line coverage, and every SR-VGA, SR-REN and SR-AUD requirement verified by Test shall have a QuestaSim testbench.
- Parent: UR-17
- How achieved: games are pure state machines driven by `update(input)` with no hardware calls, so tests feed recorded inputs and check state; the SDL2 backend and a mock HAL replace the hardware.
- Verification: Inspection (coverage report and testbench list)

### 4.3.5 Reliability

**QA-05** The system shall run 2 hours of continuous play, alternating both games, without a crash, a hang or corruption of the high-score file.
- Parent: UR-12
- How achieved: atomic high-score saves (SR-FW-09), validated loading (SC-04), the HAL's bounded timeouts (SR-HAL-05) and the keyboard-loss handling of SR-INP and SR-UI-11.
- Verification: Test (2-hour soak test with a scripted input program)

### 4.3.6 Usability

**QA-06** A first-time player shall be able to start a game within 30 s of seeing the main menu, using only the on-screen hint bar.
- Parent: UR-02
- How achieved: a hint bar on every screen (SR-UI-13), a live game preview (SR-UI-02) and the same Select/Back keys on every screen.
- Verification: Test (5 first-time users, timed)

---

# 5 Other Requirements — Database Tables (Software)

The system has no database engine. Its persistent and configuration data are specified as tables so that each field, key and constraint can be traced to requirements. Section 5 has two parts: the hardware tables held in FPGA memory (TBL-Tilemap, TBL-TilePattern, TBL-Palette, TBL-RegisterMap) and the software tables below. Figure 5.1 shows how the software tables relate.

*Figure 5.1: Software tables entity-relationship diagram (`5-er-software-tables`).*

### TBL-GameRegistry

Compiled into the program, read-only. One row per game; the main menu is built from it (SR-FW-04).

| Field | Type | Constraint |
|---|---|---|
| game_id | UINT8 | PK |
| name | VARCHAR(16) | Unique; shown in the menu and HUD |
| menu_order | UINT8 | Unique; position in the menu |
| factory | function reference | Creates an object implementing the game interface (SR-FW-03) |

- **Rows (release 1.0):** (0, "SNAKE", 0), (1, "BREAKOUT", 1)
- **Used by:** SR-FW-03, SR-FW-04, SR-UI-01, QA-02

### TBL-HighScore

Stored in `/var/lib/arcade/highscores.csv` on the microSD card, one CSV line per row: `game_id,rank,initials,score,seq`. Rewritten atomically (SR-FW-09).

| Field | Type | Constraint |
|---|---|---|
| game_id | UINT8 | PK (with rank); FK → TBL-GameRegistry.game_id |
| rank | UINT8 | PK (with game_id); 1–10 |
| initials | CHAR(3) | Exactly three letters A–Z |
| score | UINT32 | 0–999,999 (SCORE_MAX); ranks sorted descending |
| seq | UINT32 | Entry order; breaks ties, older entry ranks higher |

- **Rows:** at most 10 per game (20 in release 1.0)
- **No date field:** the board has no battery-backed clock and no network time, so a date would be wrong after every power cycle.
- **Used by:** SR-FW-08 to SR-FW-10, SR-UI-08, SR-UI-10, SR-UI-12, SF-04, SC-03, SC-04

### TBL-KeyBinding

Default rows compiled in; an optional override file `/etc/arcade/keymap.conf` may replace individual rows (SC-05).

| Field | Type | Constraint |
|---|---|---|
| context | ENUM (Global, Menu, Snake, Breakout, Paused, GameOver, Initials, HighScores, ShutdownConfirm) | PK (with key_code) |
| key_code | UINT16 | PK (with context); Linux evdev code `KEY_*` |
| action | ENUM (Up, Down, Left, Right, Select, Back, Pause, Launch, Char, Delete, Mute) | One action per key per context |

A key is looked up in the current context first and then in Global, so a screen can override a global key (SR-INP-09).

**Default bindings**

| Context | Keys → action |
|---|---|
| Global | M → Mute |
| Menu | ↑ → Up, ↓ → Down, Enter → Select, H → Select (SCORES), Esc → Back (held 3 s: shutdown) |
| Snake | ↑ ↓ ← → → Up, Down, Left, Right; P → Pause; Esc → Back |
| Breakout | ← → → Left, Right; Space → Launch; P → Pause; Esc → Back |
| Paused | P → Pause; Esc → Back |
| GameOver | Enter → Select; Esc → Back |
| Initials | A–Z → Char (overrides Global M); Backspace → Delete; Enter → Select |
| HighScores | ← → → Left, Right; Esc → Back |
| ShutdownConfirm | Enter → Select; Esc → Back |

- **Used by:** SR-INP-09 to SR-INP-11, SR-UI-01 to SR-UI-14, SC-05

### TBL-SoundEffect

Compiled into the program, read-only. One row per note; the key is effect_id with note_no.

| Field | Type | Constraint |
|---|---|---|
| effect_id | UINT8 | PK (with note_no); 0–6 |
| name | VARCHAR(12) | Same for all notes of one effect |
| note_no | UINT8 | PK (with effect_id); 1–4, play order |
| freq_hz | UINT16 | 100–4,000 (SR-AUD-04) |
| dur_ms | UINT16 | 10–2,000 (SR-AUD-05); a whole number of game ticks |

**Contents**

| effect_id | Effect | Notes in order: pitch, Hz, ms (ticks) | Total |
|---|---|---|---|
| 0 | Eat | G5 784 Hz, 33 ms (2) → C6 1,047 Hz, 50 ms (3) | 83 ms |
| 1 | BrickHit | E6 1,319 Hz, 33 ms (2) | 33 ms |
| 2 | PaddleHit | C5 523 Hz, 33 ms (2) | 33 ms |
| 3 | LifeLost | G4 392 Hz, 100 ms (6) → E4 330 Hz, 100 ms (6) → C4 262 Hz, 150 ms (9) | 350 ms |
| 4 | GameOver | C5 523 Hz, 117 ms (7) → G4 392 Hz, 117 ms (7) → E4 330 Hz, 117 ms (7) → C4 262 Hz, 133 ms (8) | 484 ms |
| 5 | MenuMove | A5 880 Hz, 33 ms (2) | 33 ms |
| 6 | MenuSelect | E5 659 Hz, 50 ms (3) → C6 1,047 Hz, 83 ms (5) | 133 ms |

Every note lies within 262–1,319 Hz and every effect lasts 500 ms or less. The HAL plays an effect's notes in order, writing the next note in the tick in which the previous note ends (SR-HAL-11); a new `playSound()` call replaces any effect still playing.

- **Used by:** SR-HAL-11, SR-GAM-15 to SR-GAM-22, SR-AUD-04, SR-AUD-05, PR-08

### Future scope

Release 1.0 deliberately leaves out: Space Invaders and other sprite-based games (needs a sprite renderer in the FPGA); multiplayer; recorded or mixed audio; and network features. The frozen Arcade API and register map are designed so that a sprite layer can be added later as new registers without changing existing ones.

---

# Appendix B — Traceability Matrix

Each user requirement (UR) is listed with every system requirement (SR) and non-functional requirement (PR, SF, SC, QA) whose parent it is. Every UR has at least one child, and every one of the 134 SR, PR, SF, SC and QA requirements has exactly one parent UR, so there are no orphan requirements and no unrealised user needs. Constraints (CON) and assumptions (AS) limit the design rather than derive from a single UR and are not listed.

| UR | Count | Derived requirements |
|---|---|---|
| UR-01 | 7 | SR-FW-01, SR-HAL-01, PR-07, SF-06, SC-01, SC-02, SC-06 |
| UR-02 | 7 | SR-UI-01, SR-UI-02, SR-UI-03, SR-UI-13, SR-INP-09, SC-05, QA-06 |
| UR-03 | 5 | SR-GAM-01, SR-GAM-02, SR-GAM-03, SR-GAM-05, SR-GAM-06 |
| UR-04 | 6 | SR-GAM-07, SR-GAM-08, SR-GAM-10, SR-GAM-11, SR-GAM-13, SR-GAM-14 |
| UR-05 | 16 | SR-FW-02, SR-GAM-04, SR-GAM-09, SR-INP-01, SR-INP-02, SR-INP-03, SR-INP-04, SR-INP-05, SR-INP-06, SR-INP-07, SR-INP-08, SR-INP-10, PR-02, PR-03, PR-04, PR-06 |
| UR-06 | 15 | SR-FW-12, SR-VGA-01, SR-VGA-02, SR-VGA-03, SR-VGA-04, SR-VGA-05, SR-VGA-06, SR-VGA-07, SR-VGA-10, SR-VGA-11, SR-REN-05, SR-REN-06, SR-REN-08, PR-01, SF-01 |
| UR-07 | 2 | SR-UI-04, SR-GAM-12 |
| UR-08 | 2 | SR-UI-05, SR-UI-06 |
| UR-09 | 1 | SR-UI-07 |
| UR-10 | 2 | SR-UI-08, SR-UI-09 |
| UR-11 | 2 | SR-UI-10, SR-INP-11 |
| UR-12 | 8 | SR-FW-08, SR-FW-09, SR-FW-10, SR-UI-12, SF-04, SC-03, SC-04, QA-05 |
| UR-13 | 10 | SR-VGA-08, SR-HAL-03, SR-HAL-04, SR-HAL-05, SR-HAL-06, SR-HAL-08, SR-REN-01, SR-REN-03, SR-REN-07, PR-05 |
| UR-14 | 8 | SR-UI-11, SR-UI-15, SR-INP-12, SR-INP-13, SR-INP-14, SR-INP-15, SR-INP-16, SF-03 |
| UR-15 | 5 | SR-VGA-09, SR-HAL-02, SR-HAL-09, SR-HAL-13, SF-02 |
| UR-16 | 5 | SR-FW-03, SR-FW-04, SR-REN-04, QA-01, QA-02 |
| UR-17 | 6 | SR-FW-05, SR-FW-06, SR-FW-07, SR-HAL-07, QA-03, QA-04 |
| UR-18 | 23 | SR-FW-11, SR-GAM-15, SR-GAM-16, SR-GAM-17, SR-GAM-18, SR-GAM-19, SR-GAM-20, SR-GAM-21, SR-GAM-22, SR-AUD-01, SR-AUD-02, SR-AUD-03, SR-AUD-04, SR-AUD-05, SR-AUD-06, SR-AUD-07, SR-AUD-08, SR-AUD-09, SR-HAL-10, SR-HAL-11, SR-HAL-12, PR-08, SF-05 |
| UR-19 | 4 | SR-FW-13, SR-FW-14, SR-UI-14, SF-07 |

**Coverage summary:** 19 of 19 user requirements are realised; 134 derived requirements (107 SR, 8 PR, 7 SF, 6 SC and 6 QA), of which 24 are desirable "should" requirements (the 23 children of UR-18, plus SC-01). Section 3.3 maps the same URs to use cases UC-01 to UC-13.
