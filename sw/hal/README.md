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
| `dev_mem_register_space.hpp/.cpp` | `DevMemRegisterSpace`: the `/dev/mem` mapping of the bridge window, the readiness check and its error codes | SR-HAL-01, 02, 07, 09, AS-01 |
| `bridge_sys.hpp`, `linux_bridge_sys.cpp` | `BridgeSys`: the Linux calls it makes (sysfs read, open, mmap, munmap, close), injectable for tests | SR-HAL-01, SR-HAL-09 |

## The register-access seam

Every bridge access goes through `RegisterSpace`, addressed by byte offset in the bridge window (the `kOffset*` constants). The HAL takes it by reference and never knows where the bytes go.

- **#34** adds one implementation, `DevMemRegisterSpace`: the `/dev/mem` mmap of the window (SR-HAL-01, 02, 09), with volatile loads and stores. Nothing in `TilemapHal` changes. See "Bridge mapping and readiness check" below.
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

## Bridge mapping and readiness check (#34)

`DevMemRegisterSpace::open()` runs these steps in order and stops at the first failure (SR-HAL-09, SR-HAL-01). Steps 1 and 2 only read sysfs files: **nothing opens `/dev/mem` or touches the bridge until both pass**, because an access while the fabric is unconfigured or the bridge is disabled can hang the HPS (SR-HAL-09 rationale).

1. **FPGA manager says configured** (SR-HAL-09): `/sys/class/fpga/fpga0/status` reads `user mode`. Any other contents give `kFpgaNotConfigured`; no "unconfigured" value is assumed (none was measured).
2. **Lightweight bridge enabled**: `/sys/class/fpga-bridge/lwhps2fpga/enable` reads `1`. This is our design choice supporting AS-02 ("the lightweight bridge is enabled before the game process starts"); SR-HAL-09 does not name it.
3. **Map the window** (SR-HAL-01, AS-01): `open("/dev/mem", O_RDWR | O_SYNC)`, then `mmap(BRIDGE_WINDOW_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, BRIDGE_BASE)`, both from `register_map.hpp`. `O_SYNC` gives an uncached mapping, as in the board-tested `bridge_benchmark` (#38). Needs root (`/dev/mem` is `crw-r----- root kmem`).
4. **`checkFabric()`** (SR-HAL-09): read ID/Version (`kOffsetIdVersion`) and require `HAL_ID_MAGIC`; only then read Status and require bit 1, FPGA ready. On this board a fresh boot loads U-Boot's default `soc_system.rbf`, and `/sys/class/fpga/fpga0/status` then reads `user mode`, **the same as with our design loaded**. So step 1 cannot tell them apart: the ID check is the only step that catches "configured, but not our design", and a fresh-boot run fails there. Requiring FPGA ready is **our choice** (§3.1.4 Status bit 1; #114 checked it too): it catches our design held in reset (for example the pixel PLL not locked), which SR-HAL-09 does not name. Nothing else is read, and the tilemap, pattern and palette regions are never touched by the check.

If step 4 fails, the window is released again (munmap and close) before `open()` returns. Sysfs values are compared after removing trailing white space (sysfs ends them with a newline); anything else must match exactly.

**Error codes** (`BridgeStatus`, SR-HAL-02), each with its own console message from `describeBridgeFailure()`:

| Code | Meaning | Extra in `BridgeResult` |
|---|---|---|
| `kFpgaStateUnreadable` | the FPGA-manager state file cannot be read | `sysError` (errno) |
| `kFpgaNotConfigured` | it does not read the configured value | `text` (what it read) |
| `kBridgeStateUnreadable` | the bridge enable file cannot be read | `sysError` |
| `kBridgeNotEnabled` | it does not read `1` | `text` |
| `kDevMemOpenFailed` | `open("/dev/mem")` failed, e.g. `EACCES` (not root) or `ENOENT` | `sysError` |
| `kMmapFailed` | `mmap()` of the window failed | `sysError` |
| `kIdMismatch` | ID/Version is not `0xA2CADE01`: not our design | `value` (what it read) |
| `kFpgaNotReady` | Status bit 1 is clear | `value` (Status) |

No step blocks, and nothing throws or crashes. `sw/app/` prints `graphics hardware not ready: <step>` and exits with status 1 (SF-02).

**Access and release.** Until `open()` succeeds, and after `release()`, `read8`/`read32` return 0 and `write8`/`write32` are dropped, so nothing ever goes through an unmapped pointer. Accesses outside the 2 MB window or not aligned to their width are dropped too. `release()` (SR-HAL-07) forgets the pointer, then munmaps the window and closes `/dev/mem`, once; a second call, or the destructor after it, does nothing. It writes no register, so Display Enable stays set. This is what keeps a late `TilemapHal::present()` after `HalLifecycle::shutdown()` off the bridge (review of #36).

**System calls.** `BridgeSys` wraps `open`, `close`, `mmap`, `munmap` and the sysfs read, as `EvdevSys` does in `sw/input/`; `LinuxBridgeSys` calls the kernel. Tests use `sw/tests/hal/fake_bridge_sys.hpp` (fake files and a 2 MB in-memory window that records every call), so every step and error is tested on the PC: `ctest --preset pc-debug -R "DevMem|CheckFabric|DescribeBridge|BridgeOpen" --output-on-failure`.

### Board environment (measured by the team on 12 Nov 2026, issue #34)

- Image: Intel FPGA University Program `DE1-SoC_UP_Linux.img`, kernel `3.18.0` (armv7l). SW10 MSEL = 01010.
- U-Boot configures the FPGA with `soc_system.rbf` from the FAT partition on every boot, before Linux starts. Our design is loaded afterwards with `load_fpga.sh arcade_top_1024.rbf` (`dd` to `/dev/fpga0` with the bridges disabled, then re-enabled). Loading our design at boot is #39.
- FPGA manager: Altera's own driver (`altera_fpga_manager ff706000.fpgamgr ... registered as minor 0`, `/dev/fpga0`), not the mainline FPGA manager, so the glossary's example path `/sys/class/fpga_manager/fpga0/state` does not exist. The state file is `/sys/class/fpga/fpga0/status` (measured 12 Nov 2026): `user mode` after a fresh boot (default `soc_system.rbf`) and after loading our design.
- Bridges: `/sys/class/fpga-bridge/` holds `fpga2hps`, `hps2fpga` and `lwhps2fpga`; each `enable` read `1` with our design loaded.
- Over `/dev/mem` at `0xFF200000` with our design loaded: ID/Version `0xA2CADE01`, Status `0x00000003`.

### Board build

The board's C library is older than the cross toolchain's, so build static, then check that `file` says `statically linked`:

```bash
cmake --preset de1soc-release -DCMAKE_EXE_LINKER_FLAGS=-static && cmake --build --preset de1soc-release
file build/de1soc-release/sw/app/arcade
```

Run as root on the board: `./arcade --backend=fpga`.

Rules: see [`AGENTS.md`](/AGENTS.md).
