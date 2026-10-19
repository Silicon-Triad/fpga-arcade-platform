## 2.1 Product Perspective

The FPGA Arcade Platform is a new, self-contained product, not an extension or follow-on to an existing system. It combines a Terasic DE1-SoC board's embedded Linux (HPS) side with its FPGA fabric side to form a dedicated arcade cabinet running two games, Snake and Breakout, with no PC required at run time ([DE1-SoC User Manual](https://people.ece.cornell.edu/land/courses/ece5760/DE1_SOC/DE1-SoC_User_manualv.1.2.2_revE.pdf)).

The division of responsibility follows the hardware split: the HPS owns *meaning* and the FPGA owns *pixels*. All game logic, rules, scoring, menus and input handling run as a C++ process on embedded Linux, which decides what the screen should show by writing tile indices into a shared tilemap. The FPGA fabric owns no game state at all; it only owns the Tile Renderer and VGA Controller, which read that tilemap and the tile pattern/palette RAM and turn them into a VGA signal, synchronised to the display's vertical refresh. This split keeps the FPGA design fixed and game-agnostic (see UR-16), while all gameplay changes happen entirely in software.

System context diagram: see `2.1-system-context.puml`.

## 2.7 Assumptions & Dependencies

- **AS-01** The Linux image running on the HPS permits root access to `/dev/mem`, which the HAL requires to `mmap()` the lightweight bridge window.
- **AS-02** The FPGA bitstream implementing the Tile Renderer, VGA Controller and bridge interface is loaded and the lightweight bridge is enabled before the game process starts.
- **AS-03** The lab's VGA monitors accept 1024×768 @ 60 Hz input over the DE1-SoC's VGA output, per the agreed design baseline.
- **AS-04** The microSD card has sufficient free space and write endurance for periodic, atomic rewrites of the high-score file over the project's demo lifetime.
- **AS-05** Only one game process accesses the lightweight bridge at a time; the system does not need to arbitrate between multiple concurrent HPS-side clients.
- **AS-06** The USB keyboard is HID-compliant and is recognised by the Linux kernel's standard input subsystem (evdev) without a custom driver.
- **AS-07** The on-chip M10K memory budget on the 5CSEMA5F31C6 is sufficient to hold two 12 KB tilemap banks plus the 8 KB tile pattern RAM concurrently; this is not re-verified in this SRS and is treated as a constraint handed down from the design baseline.
- **AS-08** The SDL2 simulation backend is available on the development PC used for off-board testing (UR-17) and is not required on the DE1-SoC itself.
- **AS-09** Powered speakers or headphones are available at the demo for the board's line-out jack; the DE1-SoC has no built-in speaker (UR-18). Without them the games run silently.

## 3.1.3 Software Interfaces

The game runs as a single C++17 process on embedded Linux (Terasic/Intel DE1-SoC image) on the HPS. At start-up, the process:

1. Opens `/dev/mem` and calls `mmap()` to map the 2 MB lightweight HPS-to-FPGA bridge window (base 0xFF20_0000) into its own virtual address space ([Cyclone V HPS Technical Reference Manual, "HPS-FPGA Bridges"](https://www.intel.com/content/www/us/en/docs/programmable/683126/); [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html)). This requires root privileges, since `/dev/mem` is owned by root ([mem(4)](https://man7.org/linux/man-pages/man4/mem.4.html)). *(Decision: `/dev/mem` is used, not UIO. UIO is noted only as a possible future improvement, not an open item.)*
2. Assumes the FPGA bitstream has already been loaded onto the fabric before the process starts, either by the Linux boot process (preloaded bitstream) or by a prior manual step; this SRS does not define the bitstream-loading mechanism itself (see AS-02).
3. Opens `/dev/i2c-0` to configure the WM8731 audio codec (SR-HAL-10, CON-12). Like `/dev/mem`, this needs root (or membership of the `i2c` group).
4. Links against: the C++ standard library, GoogleTest (unit testing only, not present in the shipped binary), and SDL2 (used exclusively by the PC-side simulation backend for development and debugging, per UR-17 — not linked into the FPGA-backend build).

No other software interfaces (databases, external APIs, other processes) are required; the game process is self-contained once started.

## 3.1.4 Communications Interfaces

The HPS and FPGA communicate exclusively through the **lightweight HPS-to-FPGA AXI bridge**, a memory-mapped interface exposing a 2 MB window at physical base address `0xFF20_0000` ([Cyclone V HPS Technical Reference Manual, "HPS-FPGA Bridges"](https://www.intel.com/content/www/us/en/docs/programmable/683126/)). The C++ game process maps this window with `mmap()` on `/dev/mem` and accesses it thereafter as ordinary memory reads and writes — no DMA and no interrupts are used.

The window exposes the tilemap (two banks), tile pattern RAM, a 16-entry colour palette, and a small set of control/status registers, detailed in the register map below.

**Synchronisation method:** all drawing operations (`setTile`, `fillRect`, `drawText`, `clear`) write only to a **shadow tilemap kept in HPS process RAM** — no bridge access occurs on these calls. On `present()`, the HAL, in order: copies every shadow cell that changed since the back bank was last written into the back bank over the bridge; requests a bank swap by writing the **Swap Request** register; then polls the Status register until the **swap pending** bit clears (or times out after 100 ms). present() does not wait for vsync before this sequence: waiting first would delay the swap by a full frame and break the 61 ms latency budget. Polling begins only after the Swap Request write, so the swap lands on the very next vblank. The actual swap is performed in hardware: the FPGA applies the swap only at the start of the next vertical blanking interval. This moves the race-prone part of the operation (deciding exactly when the front/back roles flip) into hardware, so software timing jitter can never cause a mid-frame swap.

**Data rate:** rewriting the entire 12,288-byte tilemap every frame at 60 Hz gives a worst-case bridge write load of approximately 737 kB/s (12,288 × 60). In practice, each tick only rewrites the cells that changed (e.g. the snake's head/tail, a moving ball and paddle), so typical load is expected to be well below this figure. *(Open item: a measured figure for LW-bridge write throughput from Linux userspace, sourced from the DE1-SoC manual or an on-board benchmark, is still needed to confirm this is well within the available bandwidth.)*

There is no network interface anywhere in this system; all communication is local, over the on-chip bridge only.

### Register map

These offsets are our own Platform Designer address assignment within the 2 MB lightweight-bridge window; only the bridge's base address (`0xFF20_0000`) and 2 MB window size come from the HPS Technical Reference Manual. The offsets below are frozen design, not something to verify against the TRM.

| Offset | Name | R/W | Width | Reset Value | Description |
|---|---|---|---|---|---|
| 0x0000–0x2FFF | Tilemap Bank 0 | R/W | 8 bits/cell | 0x00 | 128×96 tile-index grid, bank 0 (12,288 bytes) |
| 0x3000–0x5FFF | Tilemap Bank 1 | R/W | 8 bits/cell | 0x00 | 128×96 tile-index grid, bank 1 (12,288 bytes) |
| 0x6000–0x7FFF | Tile Pattern RAM | R/W | 4 bits/pixel | 0x0 | 256 tile patterns, 8×8 px, 4bpp (8 KB). Packed 2 pixels/byte; low nibble = left pixel, high nibble = right pixel. |
| 0x8000 | Palette Base | R/W | 32 bits ×16 | 0x00000000 | 16-entry colour palette; each entry packed as `0x00RRGGBB`, 8 bits per channel (final — matches the ADV7123 DAC's 8 wired bits/channel, CON-06) |
| 0x8040 | Control Register | R/W | 32 bits | 0x00000000 | Bit 0: reserved; Bit 1: reset tilemap; Bit 2: display enable (HAL sets this only after tile patterns and palette are loaded — SR-HAL-13, ties to SR-VGA-09); others reserved |
| 0x8044 | Status Register | R | 32 bits | 0x00000000 | Bit 0: vsync flag (cleared on read); Bit 1: FPGA ready; Bit 2: swap pending (set on Swap Request, cleared by hardware once the swap is applied) |
| 0x8048 | Frame Counter | R | 32 bits | 0x00000000 | Increments once per displayed frame since power-on |
| 0x804C | Swap Request / Front Bank | W: Swap Request (1 bit); R: Front Bank (1 bit) | 1 bit | 0x0 | Write 1 to request a bank swap at the next vertical blanking start. Read returns which bank (0/1) is currently front (displayed), so the HAL knows which is the writable back bank. |
| 0x8050 | ID / Version | R | 32 bits | 0xA2CADE01 | Fixed magic value confirming this design (not some other bitstream) is loaded on the fabric |
| 0x8054 | Tone Frequency | R/W | 16 bits | 0x0000 | Frequency in Hz for the Tone Generator, 100–4,000 (SR-AUD-04); written by the HAL on playSound() (SR-HAL-11) |
| 0x8058 | Tone Duration | R/W | 16 bits | 0x0000 | Duration in ms for the current tone, 10–2,000 (SR-AUD-05); written by the HAL on playSound() (SR-HAL-11) |
| 0x805C | Audio Control Register | R/W | 32 bits | 0x00000000 | Bit 0: audio enable (HAL sets after codec I2C setup succeeds, SR-HAL-10); Bit 1: mute (set by setMute(), SR-HAL-12); others reserved |

Register offsets, the tile-pattern nibble order and the palette packing format above are this team's own design decisions in Platform Designer, now frozen; they do not require a source citation to the TRM.

## 3.2 System Functional Requirements — SR-HAL (Hardware Access Layer)

**SR-HAL-01** The HAL shall map the lightweight HPS-to-FPGA bridge's 2 MB window at physical base address 0xFF20_0000 into the game process's virtual address space at start-up, using mmap() on /dev/mem.
- Parent: UR-01
- Rationale: the game process needs direct, call-free access to bridge registers and the tilemap region.
- Verification: Test

**SR-HAL-02** If the bridge mapping fails, the HAL shall return a distinct error code identifying the failure rather than causing the process to hang or crash.
- Parent: UR-15
- Rationale: lab staff must be able to diagnose an unconfigured or misconfigured FPGA without a silent failure.
- Verification: Test

**SR-HAL-03** The HAL shall write a single tile index to a specified (x, y) cell of the shadow tilemap in process RAM; no bridge access occurs on this call.
- Parent: UR-13
- Rationale: drawing calls must be cheap and frequent; only present() touches the bridge, keeping bridge traffic bounded and bank-swap timing predictable.
- Verification: Test

**SR-HAL-04** The HAL shall write a rectangular region of up to 128×96 cells to the shadow tilemap in process RAM in a single HAL call; no bridge access occurs on this call.
- Parent: UR-13
- Rationale: large-area redraws must be equally cheap and must not touch the bridge outside present(), for the same reason as SR-HAL-03.
- Verification: Test

**SR-HAL-05** After a Swap Request has been written, the HAL shall provide a blocking call that polls the Status register and returns as soon as the swap-pending bit clears (hardware has applied the swap), or with a timeout error if it has not cleared within 100 ms (6 frame periods).
- Parent: UR-13
- Rationale: Present() must confirm the requested swap actually completed, but must not block beforehand waiting for a separate vsync pulse — polling only begins after the Swap Request write, so the swap is applied at the very next vblank rather than one frame later. Waiting for vsync first, then requesting the swap, would delay the swap by a full frame and break the 61 ms keypress-to-screen latency budget (PR-02).
- Verification: Test

**SR-HAL-06** The HAL shall provide a non-blocking read of the Front Bank bit and the Frame Counter register at any time without waiting for vsync.
- Parent: UR-13
- Rationale: the renderer and diagnostics need current frame state without stalling the caller.
- Verification: Test

**SR-HAL-07** The HAL shall unmap the bridge region and release all associated resources on shutdown(), without clearing the Display Enable bit, so the last presented frame ("SAFE TO SWITCH OFF", SR-FW-13) stays on screen.
- Parent: UR-17
- Rationale: clean teardown is required for repeated start/stop during debugging; leaving display enable set keeps the shutdown message visible until power is removed (SF-07).
- Verification: Test

**SR-HAL-08** On present(), the HAL shall, in order: (1) copy to the back bank every cell that changed in the shadow tilemap since that bank was last written; (2) request a bank swap by writing the Swap Request register; (3) call the blocking poll of SR-HAL-05 and return once it confirms the swap or times out.
- Parent: UR-13
- Rationale: With double buffering the back bank is two frames old, so copying only the most recent frame's changes would leave stale cells from the frame before that. The copy-then-request-then-poll order (not waiting for vsync beforehand) keeps the swap on the very next vblank, which the 61 ms latency budget (PR-02) depends on.
- Verification: Test

**SR-HAL-09** Before any bridge access, the HAL shall confirm that the FPGA is configured (via the Linux FPGA manager state) and that the ID/Version register returns the expected value; if either check fails, it shall report "graphics hardware not ready" and exit without accessing the tilemap, pattern or palette regions of the bridge.
- Parent: UR-15
- Rationale: accessing the lightweight bridge while the FPGA fabric is unconfigured can hang or fault the HPS rather than return a clean error, so this must be checked before any other bridge access is attempted. Configuration state is read from the FPGA manager ([Linux FPGA Manager](https://docs.kernel.org/driver-api/fpga/fpga-mgr.html)).
- Verification: Test (boot with no bitstream loaded)

**SR-HAL-10** Before the first sound is played, the HAL should set HPS_I2C_CONTROL high and configure the WM8731 codec over `/dev/i2c-0`, then set the audio-enable bit in the Audio Control Register; if the codec does not acknowledge, the HAL should report a codec configuration error rather than hang.
- Parent: UR-18
- Rationale: "should" because sound is a "should" requirement (UR-18); mirrors SR-HAL-09's "report, don't hang" principle for the audio path.
- Verification: Test

**SR-HAL-11** On playSound(effectId), the HAL should look up effectId in TBL-SoundEffect and play the effect's notes in order: write the first note's frequency (Hz) and duration (ms) to the Tone Frequency and Tone Duration registers, then write each following note in the game tick in which the previous note ends. A new playSound() call replaces any effect still playing.
- Parent: UR-18
- Rationale: games call playSound() with only an effect ID; the HAL is what translates that into register writes, keeping Hz/ms values out of game code. TBL-SoundEffect holds one row per note (effect_id, note_no), every note a whole number of ticks, so tick-by-tick sequencing needs no extra hardware.
- Verification: Test

**SR-HAL-12** On setMute(on), the HAL should write the mute bit in the Audio Control Register to match the requested state.
- Parent: UR-18
- Rationale: the Arcade API's setMute() must have a concrete effect on the Tone Generator; one bit write is sufficient since the FPGA owns actually silencing output (SR-AUD-08).
- Verification: Test

**SR-HAL-13** After loading tile patterns and the palette at start-up, the HAL shall set the Display Enable bit (Control Register bit 2).
- Parent: UR-15
- Rationale: pairs with SR-VGA-09 — the VGA Controller outputs black until this bit is set, so the player never sees uninitialised tilemap/pattern/palette contents during boot.
- Verification: Test

## 3.2 System Functional Requirements — SR-REN (Tile Renderer)

**SR-REN-01** The Tile Renderer shall maintain two tilemap banks of 12,288 bytes each (128×96 cells, 1 byte/cell), addressed as bank 0 and bank 1.
- Parent: UR-13
- Rationale: double buffering exists specifically to prevent a torn or half-updated picture.
- Verification: Inspection

**SR-REN-03** The Tile Renderer shall apply a requested bank swap (Swap Request register) only at the start of vertical blanking, and shall clear the swap-pending status bit once the swap has been applied.
- Parent: UR-13
- Rationale: performing the swap in hardware, gated to vertical blanking, removes the software-timing race that could otherwise cause a swap to land mid-frame and tear the displayed image.
- Verification: Test

**SR-REN-04** The Tile Renderer shall store up to 256 tile patterns of 8×8 pixels at 4 bits per pixel in writable RAM, loaded at start-up rather than fixed at synthesis time.
- Parent: UR-16
- Rationale: a developer must be able to add a new game's graphics without resynthesising the FPGA bitstream.
- Verification: Inspection

**SR-REN-05** The Tile Renderer shall maintain a 16-entry colour palette, with each pixel in a tile pattern selecting one entry by a 4-bit index.
- Parent: UR-06
- Rationale: fixes a small, FPGA-resource-efficient colour model for all games.
- Verification: Inspection

**SR-REN-06** The Tile Renderer shall output one pixel per 65 MHz pixel-clock cycle, with a fixed pipeline latency of N = 4 cycles from tilemap read to colour output, and shall delay HSYNC/VSYNC by the same 4 cycles to keep sync aligned with pixel data.
- Parent: UR-06
- Rationale: the renderer is pipelined, not combinational, so a one-cycle fetch is not physically realistic. N = 4: tilemap read → pattern read → palette read → colour output, one pipeline stage per cycle, all four stages busy every cycle on different pixels.
- Verification: Analysis (testbench waveform)

**SR-REN-07** Setting Control Register bit 1 (reset) shall clear both tilemap banks to tile index 0 within one frame period (≤ 16.7 ms).
- Parent: UR-13
- Rationale: a defined, fully-cleared state on both banks is required so the system never displays stale or undefined tile data; clear() in the Arcade API is a software-side shadow-tilemap operation and is covered separately under SR-HAL.
- Verification: Test

**SR-REN-08** In 640×480 bring-up mode, the Tile Renderer shall display only tilemap columns 0–79 and rows 0–59 (80×60 visible tiles); the remainder of each tilemap bank shall still be written and held by the renderer but not displayed.
- Parent: UR-06
- Rationale: 640×480 is a smaller visible area than the 128×96 tilemap: this mode exists only to bring up the video path before switching to the full 1024×768 mode, so cropping rather than rescaling the tilemap keeps the renderer simple.
- Verification: Inspection
