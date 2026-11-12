# LW-bridge write throughput (#38)

**Requirement:** SRS §3.1.4 "Data rate" open item (a measured LW-bridge write figure from Linux
user space), PR-04. **Method:** Test, on the board.
**Verdict: PASS.** Full-tilemap writes reach 2,534,253 B/s with byte stores and 10,503,762 B/s
with 32-bit stores, 3.4× and 14.2× the worst-case load of 737,280 B/s.

## Target

§3.1.4: rewriting the whole 12,288-byte tilemap every frame at 60 Hz is a worst-case bridge write
load of 12,288 × 60 = **737,280 B/s** (`kTargetBytesPerSecond` in `sw/hal/bridge_benchmark.hpp`).

## Set-up (12 Nov 2026)

| Item | Value |
|---|---|
| Board | Terasic DE1-SoC, revision G0 (printed on the PCB) |
| SW10 MSEL[4:0] | `01010` (1 ON, 2 OFF, 3 ON, 4 OFF, 5 ON), DE1-SoC manual Table 3-1/3-2 |
| Linux image | Intel FPGA University Program `DE1-SoC_UP_Linux.img` (29 Mar 2019) |
| Kernel | `Linux de1soclinux 3.18.0 #9 SMP Mon Aug 8 17:11:41 EDT 2016 armv7l` |
| FPGA design | `arcade_top_1024.rbf` (VIDEO_MODE `XGA_1024x768`, compressed, PP x16), sha256 `377e428b602f909f65473d8b3b9f5cee0e9cec43ff57dabf91231dd29b1aec69`, built with Quartus Prime Lite 25.1std from commit `5ff4e31`, whose `hw/` is identical to `main` at `6c8c6d8` |
| Load | `dd` to `/dev/fpga0` with the bridges disabled, then all three re-enabled (`load_fpga.sh`, same sequence as the image's `/etc/init.d/programfpga`) |
| Program | `bridge_benchmark` from commit `839cbee`, static ARM build (`-DCMAKE_EXE_LINKER_FLAGS=-static`), sha256 `d3e8c8f36d0aaa659d6ad9ad46dfb957d1b28b833caec4b2fba54889e48f7d84` (stripped) |

Checks before the run, all passed: ID / Version `0xA2CADE01` (§3.1.4), Status `0x00000003`
(FPGA ready and vsync), Frame Counter +600 in 10 s, monitor OSD 1024×768 at 60 Hz.

## Method

`bridge_benchmark` maps the 2 MB window at `0xFF20_0000` from `/dev/mem` (SR-HAL-01), checks
ID / Version and Status FPGA ready, then writes the **back** bank (Front Bank 0, so bank 1 at
`0x3000`) N times, first with 8-bit stores, then with 32-bit stores. Each pass ends with a barrier
and an ID read, so the time counts writes that reached the FPGA. Timing uses `steady_clock`.

## Results (board console, verbatim)

```
root@de1soclinux:~# ./bridge_benchmark
bridge_benchmark (#38, SRS 3.1.4 data rate, PR-04)
ID 0xA2CADE01, Status 0x00000003, Front Bank 0; writing back bank at offset 0x00003000
bank size 12288 B, passes 600, tile value 0, target 737280 B/s
 8-bit stores: 7372800 B in 2.909 s = 2534253 B/s, 4.849 ms per full bank -> PASS
32-bit stores: 7372800 B in 0.702 s = 10503762 B/s, 1.170 ms per full bank -> PASS
root@de1soclinux:~# ./bridge_benchmark --passes 6000
bridge_benchmark (#38, SRS 3.1.4 data rate, PR-04)
ID 0xA2CADE01, Status 0x00000003, Front Bank 0; writing back bank at offset 0x00003000
bank size 12288 B, passes 6000, tile value 0, target 737280 B/s
 8-bit stores: 73728000 B in 29.090 s = 2534437 B/s, 4.848 ms per full bank -> PASS
32-bit stores: 73728000 B in 7.023 s = 10498561 B/s, 1.170 ms per full bank -> PASS
root@de1soclinux:~# ./bridge_benchmark
bridge_benchmark (#38, SRS 3.1.4 data rate, PR-04)
ID 0xA2CADE01, Status 0x00000003, Front Bank 0; writing back bank at offset 0x00003000
bank size 12288 B, passes 600, tile value 0, target 737280 B/s
 8-bit stores: 7372800 B in 2.909 s = 2534359 B/s, 4.849 ms per full bank -> PASS
32-bit stores: 7372800 B in 0.702 s = 10503396 B/s, 1.170 ms per full bank -> PASS
```

(A stray `^[[A^H` echoed during the second run, from keys pressed on the console, is removed
above; it did not affect the run.)

| Store width | Bytes/s (3 runs) | × target | One full bank |
|---|---|---|---|
| 8-bit | 2,534,253 / 2,534,437 / 2,534,359 | 3.4 | 4.849 ms |
| 32-bit | 10,503,762 / 10,498,561 / 10,503,396 | 14.2 | 1.170 ms |

The runs agree to better than 0.1 %.

## Notes

- PR-04 allows 8.3 ms for one tick. A full-bank rewrite takes 4.85 ms with byte stores (58 % of
  that) and 1.17 ms with 32-bit stores (14 %). The HAL copies only changed cells (SR-HAL-08), so
  a normal tick writes far less than a full bank.
- Suggested follow-up (not part of #38): the `/dev/mem` `RegisterSpace` of #34 could override
  `writeBlock8()` with aligned 32-bit stores; this board measurement shows about 4× the byte rate.
- The full board session (console log, photos, board snapshot) is kept by Mujtaba and Muhammad;
  the bring-up record sheet lists the other findings.
