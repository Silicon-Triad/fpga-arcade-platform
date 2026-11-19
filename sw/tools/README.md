# `sw/tools/`

**Owner:** Huzaifa (uinput), Muhammad (profiler, soak driver, save_loop)  
**Issues:** #23, #45, #51

Test tools: uinput injector, tick profiler, soak driver, high-score power-cut test. SRS: PR-04, PR-06, QA-05, SF-04.

- [`uinput/`](uinput/README.md): `uinput_inject` and `input_throughput_test` (PR-06, #23).
- [`soak/`](soak/README.md): `soak_driver`, the scripted player of the 2-hour soak (QA-05, #51).
- [`save_loop/`](save_loop/README.md): `save_loop`, the SF-04 power-cut test program (#45).
- `sdl_demo.cpp` (Muhammad, #17): manual demo of the SDL2 backend, built with the `pc-*` presets.

Rules: see [`AGENTS.md`](/AGENTS.md).
