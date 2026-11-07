# LW-bridge tilemap write throughput (issue #38)

**SRS:** §3.1.4 "Data rate" open item, PR-04
**Owner of the measurement:** Mujtaba
**Tool:** `bridge_benchmark` (`sw/hal/bridge_benchmark_main.cpp`, core in `sw/hal/bridge_benchmark.cpp`)

## What is measured

§3.1.4 gives the worst-case bridge write load as 12,288 bytes (one full tilemap bank) x 60
frames per second = **737,280 bytes/s** ("approximately 737 kB/s"), and leaves a measured
LW-bridge write figure from Linux user space as an open item.

`bridge_benchmark` maps the bridge window from `/dev/mem` (§3.1.4, the same mapping the game uses),
reads Front Bank once, then writes the whole **back** bank (the bank not on screen) byte by
byte, as `present()` does (SR-HAL-08), for N passes, timed with `steady_clock`. It requests
no swap and writes no other register, so the screen does not change.

**Verdict:** PASS if measured bytes/s >= 737,280; otherwise FAIL. The program prints the
verdict and exits 0 on PASS, 3 on FAIL.

## Procedure

1. Program the board with the current `arcade` bitstream (`hw/quartus/`), boot Linux.
2. Cross-build: `cmake --preset de1soc-release && cmake --build --preset de1soc-release`.
3. Copy `build/de1soc-release/sw/hal/bridge_benchmark` to the board.
4. As root, stop the `arcade` service if it is running, then run three times:
   `./bridge_benchmark 600` (600 passes, about 7.4 MB).
5. Record every run below, with the date, the git commit and the kernel version (`uname -r`).

## Results

Not measured yet: needs the board. Do not fill this in by hand without running the tool.

| Date | Commit | Kernel | Passes | Bytes/s | Verdict vs 737,280 B/s |
|---|---|---|---|---|---|
| | | | | | |

**Conclusion:**
