# `sw/app/`

**Owner:** Muhammad  
**Issues:** #16, #15

`main.cpp`: chooses the backend and starts the program. `startup.*`: SR-FW-01 order and the SF-02 failure path, testable with a fake backend. `options.*`: `--backend=sdl|fpga`, `--profile`. `binding_lookup.hpp`: TBL-KeyBinding for the SDL2 backend. Signal handling arrives with #15. SRS: SR-FW-01, SR-FW-05, SR-FW-14, SF-02.

Rules: see [`AGENTS.md`](/AGENTS.md).
