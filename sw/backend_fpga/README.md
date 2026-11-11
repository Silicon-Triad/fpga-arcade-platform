# `sw/backend_fpga/`

**Owner:** Mujtaba  
**Issues:** #37

FPGA backend of the Arcade API, built on the HAL. SRS: SR-FW-05, QA-03.

## What it does

`fpga_backend.hpp/.cpp`: `FpgaBackend`, an `EngineApi` for the board. It binds to one `hal::RegisterSpace&` and one `hal::MonotonicClock&` (both injected) and owns a `TilemapHal` (#35) and a `HalLifecycle` (#36) on them.

| Call | Goes to | SRS |
|---|---|---|
| `setTile`, `fillRect`, `clear` | `TilemapHal` shadow tilemap, no bridge access | SR-HAL-03, SR-HAL-04 |
| `drawText` | `assets::textToTiles()`, the layout `SdlBackend` uses, into the shadow tilemap | SR-FW-12 |
| `present` | `TilemapHal::present()`: changed cells to the back bank, Swap Request, Status poll | SR-HAL-08, SR-HAL-05, SR-FW-02 |
| `loadTiles` | `HalLifecycle::loadAssets()`: patterns, palette, then Display Enable | SR-HAL-13, SR-FW-01 |
| `frameCount` | `TilemapHal::frameCounter()` | SR-HAL-06 |
| `pollInput`, `setInputContext` | `InputManager::poll()` and `KeyBindings` (sw/input/, unchanged) | SR-INP-02, SR-INP-09 to SR-INP-16 |
| `playSound`, `setMute` | the `FpgaSound` hook, if set; otherwise accepted and silent | SR-FW-07 |
| `shutdown()` (and the destructor) | `HalLifecycle::shutdown()`: release once, no register write | SR-HAL-07 |

Bridge access happens only in `present()`, `loadTiles()`, `frameCount()` and `shutdown()`. After `shutdown()`, `present()` and `loadTiles()` return `BackendError` and `frameCount()` returns the last value read, without touching the bridge. The backend includes HAL headers but never `register_map.hpp` (QA-01), and holds no register offset or other hardware value.

## How #34 and #44 plug in

- **#34 (bridge mapping, SR-HAL-01, 02, 09):** implement the `/dev/mem` `RegisterSpace` and the readiness check, then replace the body of `openBridge()` in `sw/app/main.cpp` to return it in a `BridgeOpen`, or the failed step. `startFpgaBackend()` (`sw/app/fpga_start.cpp`) already builds the backend on it. Until then, `arcade --backend=fpga` exits with `graphics hardware not ready: map the lightweight bridge (/dev/mem): not implemented (#34)` (SF-02).
- **#44 (sound, SR-HAL-10 to 12):** implement `FpgaSound` (`play`, `setMute`, `tick`) over the Tone and Audio registers and pass it as the last constructor argument in `startFpgaBackend()`. `tick()` is called once per `present()`, before the tilemap is written, so the driver can start the next note in the tick in which the previous one ends (SR-HAL-11). `FpgaBackend` itself does not change.

## Design choices (made in #37; not SRS values)

- **Same return codes as `SdlBackend`.** `fillRect` with a width or height ≤ 0 is `OutOfRange` (the HAL alone would accept an empty rectangle). A palette entry above `0x00RRGGBB` gives `InvalidArgument` and Display Enable stays clear. `SdlBackend` masks such entries and returns `Ok`, because it has no hardware register to protect. A swap timeout is `BackendError`.
- **Text layout not moved.** `FontTextLayout` in `sw/backend_sdl/` is a thin wrapper over `assets::textToTiles()` in `sw/assets/`, which has no SDL dependency. This backend calls `textToTiles()` directly, so nothing moved and backend_fpga does not depend on `sw/backend_sdl/`.
- **Frozen types passed as they are.** `TileSet` and `Palette` are the same types as `HalTileSet` and `HalPalette` (checked by `static_assert`), so `loadTiles()` passes them without copying.
- **Input queue.** Events wait in a fixed queue of 256 (8 × `kMaxEventsPerTick`, as in `SdlBackend`) and are delivered at most `kMaxEventsPerTick` per tick, oldest first. The Input Manager drains the kernel queue in one call (SR-INP-04), so the backend calls it only once its own queue is empty, and a backlog waits in the kernel queue. If one drain brings more than the queue holds, the extra key events are dropped and counted (`droppedEvents()`). The last 2 slots are kept for KeyboardLost and KeyboardFound, so neither is ever dropped. That needs more than 254 key events between two ticks.
- **KeyUp keeps its KeyDown's action** even if the context changed while the key was held, as in `SdlBackend`. On KeyboardLost, keys are released without KeyUp events (SR-INP-13, `sw/input/README.md`). The held state is the Input Manager's `isPressed()` mapped through the current context, then Global.

Tests: `ctest --preset pc-debug -R "FpgaBackend|SdlParity|FpgaStart" --output-on-failure`, on the mock register space (`sw/tests/hal/`) and a fake evdev (`sw/tests/backend_fpga/fake_evdev_sys.hpp`). With SDL2 in the build, `SdlParityTest` runs the same calls on `SdlBackend` and checks that the return codes and key mapping match.

Rules: see [`AGENTS.md`](/AGENTS.md).
