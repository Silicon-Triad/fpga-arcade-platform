## Appendix A — Data Dictionary

*Merged from Mujtaba's entries (bridge/HAL/renderer/hardware tables), Muhammad's §4 (game-side, including the 16 Oct shutdown addition), and Huzaifa's `datadict-huzaifa.md` (VGA Controller, Input Manager, Tone Generator). Complete as of 16 Oct.*

### Constants

| Name | Type / range | Value | Description | Related IDs |
|---|---|---|---|---|
| TICK_RATE | — | 60 Hz | Game-loop ticks per second, locked to vsync | SR-FW-02 |
| GRID_COLS × GRID_ROWS | — | 128 × 96 tiles | Tilemap size | SR-FW-12, UR-06 |
| BOOT_LIMIT / BOOT_TO_MENU_MAX_S | UINT8, s | 60 s | Maximum power-on to main menu | SR-FW-01, PR-07 |
| TILE_SIZE | — | 8 × 8 px | Tile dimensions | SR-REN-04 |
| TILEMAP_BANK_SIZE | — | 12,288 bytes | Size of one tilemap bank (128×96×1 byte) | SR-REN-01, SR-HAL-03/04 |
| TILE_PATTERN_COUNT | — | 256 | Max distinct tile patterns | SR-REN-04 |
| TILE_PATTERN_RAM_SIZE | — | 8,192 bytes | 256 tiles × 64 px × 4 bit | SR-REN-04 |
| PALETTE_SIZE | — | 16 entries | Colour palette size | SR-REN-05 |
| BRIDGE_BASE | — | 0xFF20_0000 | LW HPS-to-FPGA bridge physical base address | SR-HAL-01 |
| BRIDGE_WINDOW_SIZE | — | 2 MB | LW bridge window size | SR-HAL-01 |
| VSYNC_TIMEOUT | — | 100 ms (6 frame periods) | present() swap-pending poll timeout before reporting error | SR-HAL-05 |
| HAL_ID_MAGIC | — | 0xA2CADE01 | Expected value of the ID/Version register, confirms correct bitstream | SR-HAL-09 |
| PIXEL_CLK_HZ | UINT32, Hz | 65,000,000 (± 0.5%) | Pixel clock, operating mode 1024×768 | SR-VGA-01, CON-02, CON-03 |
| H_ACTIVE | UINT11, px | 1024 | Visible pixels per line | SR-VGA-02 |
| H_FRONT_PORCH | UINT11, px | 24 | Blank pixels, active video → HSYNC | SR-VGA-02 |
| H_SYNC | UINT11, px | 136 | HSYNC pulse width (active low) | SR-VGA-02, SR-VGA-04 |
| H_BACK_PORCH | UINT11, px | 160 | Blank pixels, HSYNC → active video | SR-VGA-02 |
| H_TOTAL | UINT11, px | 1,344 | Pixel clocks per line | SR-VGA-02 |
| V_ACTIVE | UINT10, lines | 768 | Visible lines per frame | SR-VGA-03 |
| V_FRONT_PORCH | UINT10, lines | 3 | Blank lines, active video → VSYNC | SR-VGA-03 |
| V_SYNC | UINT10, lines | 6 | VSYNC pulse width (active low) | SR-VGA-03, SR-VGA-04 |
| V_BACK_PORCH | UINT10, lines | 29 | Blank lines, VSYNC → active video | SR-VGA-03 |
| V_TOTAL | UINT10, lines | 806 | Lines per frame | SR-VGA-03 |
| LINE_RATE_HZ | real, Hz | 48,363 | PIXEL_CLK_HZ ÷ H_TOTAL | SR-VGA-03 |
| FRAME_RATE_HZ | real, Hz | 60.004 | LINE_RATE_HZ ÷ V_TOTAL | PR-01 |
| FRAME_PERIOD_MS | real, ms | 16.67 | 1 ÷ FRAME_RATE_HZ; also one game tick | PR-02, PR-03 |
| BRINGUP_PIXEL_CLK_HZ | UINT32, Hz | 25,000,000–25,200,000 | Pixel clock, bring-up mode 640×480 | SR-VGA-10 |
| BRINGUP_H_TOTAL / V_TOTAL | UINT11 / UINT10 | 800 / 525 | Bring-up line and frame length | SR-VGA-10 |
| VIDEO_MODE | ENUM | XGA_1024x768 (default), VGA_640x480 | Synthesis-time parameter of the VGA Controller | SR-VGA-11 |
| BRINGUP_VISIBLE_TILES | — | 80 × 60 tiles (cols 0–79, rows 0–59) | Tilemap area shown in 640×480 bring-up mode | SR-REN-08 |
| AUD_MCLK_HZ | UINT32, Hz | 12,288,000 (± 0.5%) | Codec master clock on AUD_XCK (256 × 48 kHz) | SR-AUD-01 |
| AUD_SAMPLE_RATE_HZ | UINT32, Hz | 48,000 (± 0.5%) | Stereo frames per second sent to the codec | SR-AUD-02 |
| AUD_SAMPLE_BITS | UINT8, bits | 16 | Bits per channel, two's complement, I²S | SR-AUD-03 |
| TONE_FREQ_MIN / MAX | UINT16, Hz | 100 / 4,000 | Allowed tone-frequency register range | SR-AUD-04, SR-HAL-11 |
| TONE_DURATION_MIN / MAX | UINT16, ms | 10 / 2,000 | Allowed tone-duration register range | SR-AUD-05, SR-HAL-11 |
| TONE_AMPLITUDE_MAX | INT16 | 8,192 (25% of full scale) | Largest square-wave sample magnitude | SR-AUD-07, SF-05 |
| KEY_LATENCY_MAX_MS | real, ms | 67 (4 frames) | Worst-case keypress-to-screen time | PR-02 |
| RECONNECT_MAX_MS | UINT16, ms | 1,000 | Time to select a newly plugged-in keyboard | SR-INP-15 |
| SNAKE_BOARD | — | 42 × 30 cells | Snake playfield | SR-GAM-01 |
| SNAKE_CELL | — | 3 × 3 tiles | Size of one snake cell on screen | SR-GAM-01 |
| SNAKE_START_LEN | — | 4 cells | Initial snake length | SR-GAM-02 |
| SNAKE_STEP_START / _MIN | — | 6 / 3 ticks | Ticks per snake move, start and fastest | SR-GAM-03 |
| FOOD_POINTS | — | 10 | Points per food | SR-GAM-05 |
| SNAKE_DEATH_TICKS | — | 30 ticks | Still frame after the snake dies | Snake state machine |
| BRICK_COLS × BRICK_ROWS | — | 21 × 6 | Breakout wall | SR-GAM-07 |
| PADDLE_W | — | 16 tiles | Paddle width | SR-GAM-07 |
| BALL_SIZE | — | 2 × 2 tiles | Ball size | SR-GAM-07 |
| START_LIVES | — | 3 | Breakout lives at start | SR-GAM-13 |
| ROW_POINTS | — | 60, 50, 40, 30, 20, 10 | Brick points, top row to bottom row | SR-GAM-12 |
| LEVEL_CLEAR_TICKS | — | 60 ticks | Pause before a new wall | SR-GAM-14 |
| HISCORE_SLOTS | — | 10 per game | Size of the high-score table | SR-FW-10 |
| INITIALS_LEN | — | 3 letters (A–Z) | Initials length | SR-UI-10 |
| SCORE_MAX | — | 999,999 (6 digits) | Highest displayable score | SR-UI-04 |
| SHUTDOWN_HOLD | — | 3 s (180 ticks) | Back hold time to open the shutdown confirmation | SR-UI-14 |
| HISCORE_FILE | — | `/var/lib/arcade/highscores.csv` | High-score file path (decision D2) | SR-FW-08, SR-FW-09 |

### State variables

| Name | Type / range | Possible values or meaning | Related IDs |
|---|---|---|---|
| appState | enum | Boot, HardwareNotReady, Menu, ShutdownConfirm, ShuttingDown, Playing, Paused, KeyboardLost, GameOver, Initials, HighScores | App state machine |
| menuSelection | 0 … (games + 1) | Highlighted menu entry; the last entry is SCORES | SR-UI-01 |
| activeGame | UINT8 | game_id of the running game (TBL-GameRegistry) | SR-FW-04 |
| soundOn | bool | true at start-up; toggled by Mute | SR-FW-11 |
| score | 0 … 999,999 | Current game score | SR-UI-04 |
| shadowTilemap | 128×96 byte array, per bank role | Software-side copy of the tilemap; drawing calls write here | SR-HAL-03, SR-HAL-04 |
| swapPending | bool (hardware status bit) | Set on Swap Request, cleared by hardware at vblank | SR-HAL-08, SR-REN-03, Status Register bit 2 |
| frontBank | 0 or 1 | Which tilemap bank is currently displayed | SR-HAL-06, Swap Request/Front Bank register |
| h_state | ENUM | ACTIVE, FRONT_PORCH, SYNC, BACK_PORCH | Horizontal timing state of the VGA Controller | SR-VGA-02, VGA state machine |
| h_count | UINT11 | 0–1343 | Pixel clock within the current line | SR-VGA-02 |
| v_state | ENUM | ACTIVE, FRONT_PORCH, SYNC, BACK_PORCH | Vertical timing state of the VGA Controller | SR-VGA-03, VGA state machine |
| v_count | UINT10 | 0–805 | Line within the current frame | SR-VGA-03 |
| display_enable | BOOL | 0 (black, reset value), 1 (show tilemap) | Set by the HAL after tiles and palette are loaded (SR-HAL-13) | SR-VGA-09 |
| im_state | ENUM | NO_KEYBOARD, SCANNING, READY, DISCONNECTED | Input Manager state | SR-INP-12–16, Input Manager state machine |
| key_state[key_code] | ENUM per key | RELEASED (default), PRESSED | Held-key table, updated by KeyDown/KeyUp | SR-INP-08, SR-INP-13 |
| input_context | ENUM | MENU, SNAKE, BREAKOUT, PAUSED, GAMEOVER, INITIALS, HIGHSCORES, SHUTDOWN_CONFIRM | Current screen's key-binding context; GLOBAL is searched second and is never current | SR-INP-09, SR-INP-10, SR-UI-14 |
| keyboard_fd | INT | −1 (none) or ≥ 0 | File descriptor of the open evdev keyboard | SR-INP-01, SR-INP-12 |
| audio_enable | BOOL | 0 (silent, reset value), 1 | Set by the HAL after codec configuration (SR-HAL-10) | SR-AUD-09 |
| mute | BOOL | 0 (sound on, default), 1 (sound off) | Toggled by the Mute action; lasts until power-off | SR-AUD-08 |
| tone_active | BOOL | 0, 1 | 1 while a tone is playing | SR-AUD-04, SR-AUD-05 |
| tone_remaining_ms | UINT16, ms | 0–2,000 | Time left in the current tone | SR-AUD-05, SR-AUD-06 |
| snakeState | enum | Ready, Running, Dead, Over | Snake state machine |
| snakeDir / nextDir | enum | Up, Down, Left, Right | SR-GAM-04 |
| snakeBody | list of cells, length 4 … 1,260 | Head first; 1,260 = 42 × 30 | SR-GAM-03, SR-GAM-05 |
| foodPos | cell (0–41, 0–29) | Current food cell | SR-GAM-05 |
| stepTicks / tickCount | 3 … 6 / 0 … stepTicks − 1 | Snake speed and tick counter | SR-GAM-03 |
| breakoutState | enum | Serve, Running, LevelClear, Over | Breakout state machine |
| paddleX | tile column 1 … 111 | Paddle left edge | SR-GAM-09 |
| ballPos / ballVel | tile position / dx, dy ∈ {−1, +1} | Ball position and direction | SR-GAM-10, SR-GAM-11 |
| bricks | 21 × 6 bit array | true = brick present | SR-GAM-12, SR-GAM-14 |
| lives | 0 … 3 | Breakout lives left | SR-GAM-13 |
| level | 1 … | Breakout level | SR-GAM-10, SR-GAM-14 |
| initialsBuf | 0–3 letters | Initials typed so far | SR-UI-10 |
| highScores | TBL-HighScore rows | In-memory copy of the high-score table | SR-FW-08 … SR-FW-10 |

### Inputs and outputs

| Name | Direction | Type | Description | Related IDs |
|---|---|---|---|---|
| Action events | In | Up, Down, Left, Right, Select, Back, Pause, Launch, Char(A–Z), Delete, Mute | From the Input Manager via `pollInput()`, once per tick | SR-INP-09, SR-FW-02 |
| KeyboardLost / KeyboardFound | In | event | From the Input Manager | SR-INP-14, SR-UI-11 |
| Held-key state | In | per-key bool | Used for paddle movement | SR-INP-08, SR-GAM-09 |
| input_event | In | struct {time, type, code, value} | Raw keyboard event from `/dev/input/eventN`; only EV_KEY used | SR-INP-04–07 |
| read() result | In | errno | EAGAIN (queue empty), ENODEV (keyboard removed) | SR-INP-04, SR-INP-12 |
| inotify event | In | IN_CREATE on `/dev/input` | New input device node appeared | SR-INP-15 |
| Tilemap drawing | Out | `setTile`, `fillRect`, `drawText`, `clear` | To the shadow tilemap only — no bridge access | SR-HAL-03, SR-HAL-04 |
| Frame end | Out | `present()` | Copy changed shadow cells to back bank + request swap | SR-HAL-08, SR-FW-02 |
| Sound | Out | `playSound(effectId)`, `setMute(on)` | To the HAL / Tone Generator | SR-HAL-11 (playSound), SR-HAL-12 (setMute), SR-FW-11 |
| High-score file | Out | CSV file | Atomic save (decision D2: temp file, fsync, rename) | SR-FW-09 |
| FPGA-ready check | In/Out | boolean + magic value | HAL checks FPGA manager state + ID/Version register before any bridge access | SR-HAL-09 |
| tone-frequency / tone-duration registers | Out | UINT16 Hz / UINT16 ms | Written by HAL from TBL-SoundEffect on playSound() | SR-AUD-04, SR-AUD-05, SR-HAL-11 |
| audio control register | Out | bits {audio_enable, mute} | Written by HAL | SR-AUD-08, SR-AUD-09, SR-HAL-10, SR-HAL-12 |
| VGA_R/G/B, VGA_HS/VS, VGA_BLANK_N, VGA_SYNC_N, VGA_CLK | Out | per 3.1.2.1 | VGA signals to the ADV7123 DAC / monitor | SR-VGA-01, 04–07 |
| vblank_start | Out | 1-bit pulse | First pixel clock after last active pixel of line 767; bank-swap trigger | SR-VGA-08, SR-REN-03 |
| AUD_XCK, AUD_BCLK, AUD_DACLRCK, AUD_DACDAT | Out | per 3.1.2.4 | Audio signals to the WM8731 codec | SR-AUD-01–03 |
