# `sw/hal/`

**Owner:** Mujtaba  
**Issues:** #4, #34–#36, #38, #43, #44

Hardware Access Layer: bridge mapping, shadow tilemap, present(), audio registers, codec I2C. `register_map.hpp` is FROZEN and only this folder may include it (QA-01). SRS: SR-HAL, §3.1.4.

## Files

| File | What | SRS |
|---|---|---|
| `register_map.hpp` | FROZEN register map (offsets, bits, sizes) | §3.1.4 |
| `register_space.hpp` | `RegisterSpace`: the register-access seam (read8/write8/read32/write32, writeBlock8) | §3.1.4 |
| `tilemap_hal.hpp/.cpp` | `TilemapHal`: shadow tilemap, present(), swap poll, frame state | SR-HAL-03, 04, 05, 06, 08 |
| `bridge_benchmark.hpp/.cpp` | `benchmarkTilemapWrites()`: timed full-bank writes to the back bank | §3.1.4 data rate, PR-04 (#38) |
| `bridge_benchmark_main.cpp` | `bridge_benchmark` board tool; its own `/dev/mem` mapping until #34 lands | §3.1.4, #38 |

## The register-access seam

Every bridge access goes through `RegisterSpace`, addressed by byte offset in the bridge window (the `kOffset*` constants). The HAL takes it by reference and never knows where the bytes go.

- **#34** adds one implementation: the `/dev/mem` mmap of the window (SR-HAL-01, 02, 09), with volatile loads and stores. Nothing in `TilemapHal` changes.
- **#36** (tile patterns, palette, Display Enable, shutdown) writes through the same `write8`/`writeBlock8`/`write32`.
- **#44** (Tone Frequency, Tone Duration, Audio Control) uses `write32`/`read32`.
- Tests use the in-memory mock in `sw/tests/hal/mock_register_space.hpp`, which records every access in order.

The poll clock is injected as a `MonotonicClock` (`SteadyClock` on the board, a fake clock in tests).

## Design choices the SRS leaves open (#35, not SRS values)

- **Per-bank dirty sets.** One bitset per bank marks the cells changed since that bank was last written; a shadow change marks both. present() copies and clears only the back bank's set, so the two-frame-old bank still gets frame N's changes in frame N+1 (SR-HAL-08). Both sets start full, so the first present() to each bank writes all 12,288 cells, because the reset state is not known to match the shadow.
- **Writing the same value** to a cell does not mark it dirty.
- **Cell address** inside a bank is `row * GRID_COLS + col`, the same as `hw/rtl/tilemap_crop.sv`.
- **Timeout boundary.** The poll reads Status first, then times out only once *more than* `VSYNC_TIMEOUT` (App. A, 100 ms) has passed, so a bit that clears at exactly 100 ms counts as success. The poll busy-waits; it does not sleep between reads.
- **Out-of-range calls** return `HalStatus::kOutOfRange` and write nothing; `fillRect` with width or height 0 inside the grid is `kOk`.
- **A timed-out present()** still counts the back bank as written: its contents match the shadow. The next present() first checks Status swap pending; while that swap is still pending it waits (SR-HAL-05) and, if it times out again, writes no cell and returns `kSwapTimeout`, so a late swap can never show a half-written bank (review of #108).
- **The poll clears the vsync flag.** Status bit 0 clears on read (§3.1.4) and the poll reads Status many times per frame, so no code may rely on that bit; use the Frame Counter (SR-HAL-06) instead.

Rules: see [`AGENTS.md`](/AGENTS.md).
