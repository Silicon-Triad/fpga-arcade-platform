# `sw/hal/`

**Owner:** Mujtaba  
**Issues:** #4, #34–#36, #43, #44

Hardware Access Layer: bridge mapping, shadow tilemap, present(), audio registers, codec I2C. `register_map.hpp` is FROZEN and only this folder may include it (QA-01). SRS: SR-HAL, §3.1.4.

## Files

| File | What | SRS |
|---|---|---|
| `register_map.hpp` | FROZEN register map (offsets, bits, sizes) | §3.1.4 |
| `register_space.hpp` | `RegisterSpace`: the register-access seam (read8/write8/read32/write32, writeBlock8) | §3.1.4 |
| `tilemap_hal.hpp/.cpp` | `TilemapHal`: shadow tilemap, present(), swap poll, frame state | SR-HAL-03, 04, 05, 06, 08 |
| `bridge_benchmark.hpp/.cpp`, `bridge_benchmark_main.cpp` | `bridge_benchmark` (#38): LW-bridge write throughput, verdict against 737,280 B/s | §3.1.4 data rate, PR-04 |
| `hal_lifecycle.hpp/.cpp` | `HalLifecycle`: start-up load (patterns, palette, Display Enable) and shutdown() | SR-HAL-13, SR-HAL-07 |

## The register-access seam

Every bridge access goes through `RegisterSpace`, addressed by byte offset in the bridge window (the `kOffset*` constants). The HAL takes it by reference and never knows where the bytes go.

- **#34** adds one implementation: the `/dev/mem` mmap of the window (SR-HAL-01, 02, 09), with volatile loads and stores. Nothing in `TilemapHal` changes.
- **#36** (tile patterns, palette, Display Enable, shutdown) writes through the same `write8`/`writeBlock8`/`write32`.
- **#44** (Tone Frequency, Tone Duration, Audio Control) uses `write32`/`read32`.
- Tests use the in-memory mock in `sw/tests/hal/mock_register_space.hpp`, which records every access in order.

The poll clock is injected as a `MonotonicClock` (`SteadyClock` on the board, a fake clock in tests).

## `bridge_benchmark` (#38)

Board-only test program. It maps the bridge window from `/dev/mem` (SR-HAL-01), checks ID / Version and Status FPGA ready first and writes nothing if either is wrong (the SR-HAL-09 rule), then writes the **back bank** (never the front bank, Control or Swap Request) `--passes` times with 8-bit and 32-bit stores. Each pass ends with a barrier and an ID read, so the time is for writes that reached the FPGA. It prints bytes/s and ms per full bank for each width, with a verdict against the §3.1.4 worst case 12,288 × 60 = 737,280 B/s. Exit status: 0 all widths pass, 1 bad arguments, 2 mapping failed, 3 hardware not ready (or not an ARM build), 4 below target.

Build it static for the board (the board image's C library is older than the cross toolchain's), then check that `file` says `statically linked` and a minimum kernel no newer than the board's:

```bash
cmake --preset de1soc-release -DCMAKE_EXE_LINKER_FLAGS=-static && cmake --build --preset de1soc-release
file build/de1soc-release/sw/hal/bridge_benchmark
```

On the board, as root, only after arcade_top is loaded and the bridge enabled: `./bridge_benchmark` (options: `--passes N`, `--width 8|32|both`, `--value N`, `--help`). The 8-bit figure is the path `RegisterSpace::writeBlock8()` takes today; the 32-bit figure shows what #34 gains if its mmap implementation overrides `writeBlock8()` with word stores.

## Design choices the SRS leaves open (#35, not SRS values)

- **Per-bank dirty sets.** One bitset per bank marks the cells changed since that bank was last written; a shadow change marks both. present() copies and clears only the back bank's set, so the two-frame-old bank still gets frame N's changes in frame N+1 (SR-HAL-08). Both sets start full, so the first present() to each bank writes all 12,288 cells, because the reset state is not known to match the shadow.
- **Writing the same value** to a cell does not mark it dirty.
- **Cell address** inside a bank is `row * GRID_COLS + col`, the same as `hw/rtl/tilemap_crop.sv`.
- **Timeout boundary.** The poll reads Status first, then times out only once *more than* `VSYNC_TIMEOUT` (App. A, 100 ms) has passed, so a bit that clears at exactly 100 ms counts as success. The poll busy-waits; it does not sleep between reads.
- **Out-of-range calls** return `HalStatus::kOutOfRange` and write nothing; `fillRect` with width or height 0 inside the grid is `kOk`.
- **A timed-out present()** still counts the back bank as written: its contents match the shadow. The next present() first checks Status swap pending; while that swap is still pending it waits (SR-HAL-05) and, if it times out again, writes no cell and returns `kSwapTimeout`, so a late swap can never show a half-written bank (review of #108).
- **The poll clears the vsync flag.** Status bit 0 clears on read (§3.1.4) and the poll reads Status many times per frame, so no code may rely on that bit; use the Frame Counter (SR-HAL-06) instead.

## Lifecycle (#36)

`HalLifecycle::loadAssets(patterns, palette)` (SR-HAL-13, SR-VGA-09), in this order:

1. Check every palette entry fits 0x00RRGGBB; otherwise return `kInvalidPalette` and write nothing.
2. Write the 256 patterns (32 bytes each, as given, never repacked) to Tile Pattern RAM, pattern *i* at `kOffsetTilePatternRam + 32*i`, through `writeBlock8`.
3. Write the 16 palette entries to `kOffsetPaletteBase + 4*i`, repacked from their channels with the `kPalette*Shift`/`kPaletteChannelMask` constants.
4. Write Control with only Display Enable (bit 2) set; reset tilemap (bit 1) and the reserved bits are written 0. No read-modify-write: §3.1.4 does not say whether bit 1 clears itself or what reserved bits read back as, so a write-back could carry a stale tilemap reset (review of #110). This is the only Control access.

`HalLifecycle::shutdown()` (SR-HAL-07) calls `RegisterSpace::release()` once and writes no register, so Display Enable stays set and "SAFE TO SWITCH OFF" (SR-FW-13) stays on screen. A second call does nothing; `loadAssets()` after it returns `kShutDown` without access.

**Release hook for #34.** `RegisterSpace::release()` is a virtual with an empty default. The `/dev/mem` implementation overrides it to `munmap` the window and close the file descriptor, and after it reads return 0 and writes are dropped, so nothing touches the bridge after shutdown. `HalLifecycle` does not change. The mock counts `release()` calls and any access made after it.

### Design choices for #36 (not SRS values)

- **A separate class, not methods on `TilemapHal`.** Load and shutdown run once and share no state with the shadow tilemap, so #35's class stays unchanged. #37 owns both objects on one `RegisterSpace`.
- **HAL-native input types** (`HalTileSet`, `HalPalette`) sized from `register_map.hpp`, so `sw/hal/` does not depend on `arcade/types.hpp`; #37 converts (the layouts are the same).
- **No tilemap reset (Control bit 1) during load.** The SRS only says to set Display Enable after the load; the first `present()` writes every cell of both banks anyway (#35).
- **Palette validated before any write**, so a rejected load leaves the bridge untouched.

Rules: see [`AGENTS.md`](/AGENTS.md).
