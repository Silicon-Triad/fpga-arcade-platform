# `sw/app/`

**Owner:** Muhammad  
**Issues:** #16, #15, #13, #37, #34

`main.cpp`: chooses the backend and starts the program. `startup.*`: SR-FW-01 order and the SF-02 failure path, testable with a fake backend. `options.*`: `--backend=sdl|fpga`, `--profile`. `binding_lookup.hpp`: TBL-KeyBinding for the SDL2 backend. `fpga_start.*`: starts the FPGA backend (#37). `bridge_open.*`: hands the checked bridge window to it (#34). `main.cpp` also installs the SIGINT and SIGTERM handlers (SR-FW-14).

## Backends (SR-FW-05)

- `--backend=sdl` (the default on the PC): `SdlBackend` in one 1024 × 768 window.
- `--backend=fpga` (the default in the board build, which has no SDL2): `startFpgaBackend()` takes the bridge from `openBridge()`, starts the Input Manager and builds `FpgaBackend` (`sw/backend_fpga/`). Destroying it at the end releases the bridge without clearing Display Enable (SR-HAL-07).
- `openBridge()` (#34) opens a `hal::DevMemRegisterSpace` on the board's readiness files through `openCheckedBridge()` (`bridge_open.*`): FPGA manager configured, lightweight bridge enabled, then the `/dev/mem` mapping and the ID/Version and FPGA-ready checks (`sw/hal/README.md`, SR-HAL-01, SR-HAL-09). If a step fails, `--backend=fpga` prints `graphics hardware not ready: <step>` and exits with status 1 at once (SF-02), for example `graphics hardware not ready: check ID/Version register: reads 0x00000000, expected 0xA2CADE01 (our FPGA design is not loaded)`. Run it as root (AS-01).
- Both backends use the same `KeyBindings`, with `/etc/arcade/keymap.conf` applied once (SC-05).

Exit status: `0` normal exit, including a signal or a closed window (SR-FW-14); `1` hardware not ready (SF-02); `2` bad command line; `3` shutdown requested (`kExitShutdownRequested`, SR-FW-13): the program never runs `poweroff` itself, see `board/deploy/README.md`. SRS: SR-FW-01, SR-FW-05, SR-FW-13, SR-FW-14, SF-02, SC-01.

Rules: see [`AGENTS.md`](/AGENTS.md).

## `--latency`: key-to-screen latency by software (PR-02, #47)

PR-02 (key press to the first frame showing its effect, at most 67 ms) is verified with a high-speed camera in the SRS; the team has none. It is split into: (a) USB polling, at most the keyboard's polling interval, read from `bInterval` in `lsusb -v` on the board; (b) kernel event time to the bank swap of the tick that read the press, **measured** here; (c) scan-out after the swap, at most one frame (FRAME_PERIOD_MS 16.67 ms, App. A; PR-05 by QuestaSim). Verdict: a + max(b) + one frame ≤ 67 ms.

`arcade --backend=fpga --latency --usb-ms=N` (N = the polling interval in ms; both options are required together, FPGA backend only):

- `main.cpp` gives the Input Manager a `LatencyEvdevSys` (`latency_probe.*`) wrapping `LinuxEvdevSys`. It switches each opened event device to CLOCK_MONOTONIC timestamps with `EVIOCSCLOCKID` (evdev defaults to CLOCK_REALTIME; the ioctl is defined in `include/uapi/linux/input.h` and handled by `evdev_set_clk_type()` in `drivers/input/evdev.c`, present since Linux 3.4), and copies the timestamp of every key press it reads. Events are passed on unchanged.
- After each tick's `present()` (which returns once the swap is done), `runArcade()` reads CLOCK_MONOTONIC and turns the presses read in that tick into one b each. One line per press: `latency: press 7 b=12.345 ms`. At exit: `latency: n=… max(b)=… p99(b)=…` and the bound with `PASS`/`FAIL`.
- Clock check: if `EVIOCSCLOCKID` fails, or a press is later than its swap or more than 1 s before it, the run prints `clock check FAILED` and gives no verdict.
- Without `--latency` the wrapper is not used: no cost and no output.
- Not measured: a game that stores a key and acts on it at its own next step (Snake's step timer) shows the move later; that is game design, not input latency.

Board procedure: `lsusb -v` and note the keyboard's interrupt endpoint `bInterval`; run `./arcade --backend=fpga --latency --usb-ms=N 2> latency.log`; press keys for about a minute in the menu and in Breakout; quit; copy the summary lines.
