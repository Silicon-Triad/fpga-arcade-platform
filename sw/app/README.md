# `sw/app/`

**Owner:** Muhammad  
**Issues:** #16, #15, #13, #37

`main.cpp`: chooses the backend and starts the program. `startup.*`: SR-FW-01 order and the SF-02 failure path, testable with a fake backend. `options.*`: `--backend=sdl|fpga`, `--profile`. `binding_lookup.hpp`: TBL-KeyBinding for the SDL2 backend. `fpga_start.*`: starts the FPGA backend (#37). `main.cpp` also installs the SIGINT and SIGTERM handlers (SR-FW-14).

## Backends (SR-FW-05)

- `--backend=sdl` (the default on the PC): `SdlBackend` in one 1024 × 768 window.
- `--backend=fpga` (the default in the board build, which has no SDL2): `startFpgaBackend()` takes the bridge from `openBridge()`, starts the Input Manager and builds `FpgaBackend` (`sw/backend_fpga/`). Destroying it at the end releases the bridge without clearing Display Enable (SR-HAL-07).
- **Until #34 lands**, `openBridge()` always reports the missing step, so `--backend=fpga` prints `graphics hardware not ready: map the lightweight bridge (/dev/mem): not implemented (#34)` and exits with status 1 (SF-02). #34 only replaces the body of `openBridge()`.
- Both backends use the same `KeyBindings`, with `/etc/arcade/keymap.conf` applied once (SC-05).

Exit status: `0` normal exit, including a signal or a closed window (SR-FW-14); `1` hardware not ready (SF-02); `2` bad command line; `3` shutdown requested (`kExitShutdownRequested`, SR-FW-13): the program never runs `poweroff` itself, see `board/deploy/README.md`. SRS: SR-FW-01, SR-FW-05, SR-FW-13, SR-FW-14, SF-02, SC-01.

Rules: see [`AGENTS.md`](/AGENTS.md).
