# `sw/backend_sdl/`

**Owner:** Muhammad  
**Issues:** #17

SDL2 backend of the Arcade API, PC only. SRS: SR-FW-05 to 07, QA-03.

- `sdl_backend.hpp/.cpp`: `SdlBackend`, an `EngineApi` drawing the 128 × 96 tilemap in one
  1024 × 768 window (SR-FW-06). `present()` is paced by a 1/`TICK_RATE` CPU timer, not vsync.
  Sound calls are accepted and silent (SR-FW-07).
- `key_codes.hpp/.cpp`: SDL scancode → Linux `KEY_*` code, the same codes the Input Manager
  reads on the board.
- `key_action_lookup.hpp`: the injected key → action lookup. The real TBL-KeyBinding lookup is
  #21; until then a caller supplies its own.
- `text_layout.hpp`: the hook `drawText()` uses; without one, `drawText()` returns
  `BackendError` and draws nothing.
- `font_text_layout.hpp`: the real hook, over `assets::textToTiles()` (#9).

Built only when the CMake option `ARCADE_BACKEND_SDL` is ON: the `pc-*` presets turn it on,
`de1soc-release` leaves it off, so the board build never needs or links SDL2
(`srs-mujtaba-drop1.md` §3.1.3). Manual demo: `./build/pc-debug/sw/tools/sdl_demo`.
Tests: `ctest --preset pc-debug -R Sdl --output-on-failure` (headless, SDL dummy video driver).

Rules: see [`AGENTS.md`](/AGENTS.md).
