# `sw/tools/save_loop/`

**Owner:** Muhammad  
**Issues:** #45

`save_loop` is the test program SF-04 asks for: *"a test program rewrites the table in a loop; cut power 20 times; after each boot the file loads with 10 valid entries per game."* It checks the atomic save of SR-FW-09 (temporary file, `fsync()`, `rename()`) by calling the real `HighScoreStore::save()` from `sw/engine/`.

| Command | What it does |
|---|---|
| `save_loop write [--file PATH] [--count N]` | Saves two complete reference tables in turn, A, B, A, B, ... and prints a line every 100 saves. Runs until killed or until power is cut; `--count N` stops after N saves (for PC checks). Exit 1 if a save fails. |
| `save_loop check [--file PATH]` | Loads the file and prints for example `table=A game0=10 game1=10 rejected=0 -> PASS`. **PASS** only if the file exists, no line is rejected (SC-04), each game has 10 entries, and the whole table equals A or B: the complete old or the complete new table (SF-04). Exit 0 on PASS, 1 on FAIL. |

`--file` defaults to `HISCORE_FILE` (`/var/lib/arcade/highscores.csv`, SR-FW-08). Table A has initials `AAA` and table B `BBB`, with different scores, so a file mixing the two is caught.

## Board procedure (SF-04, 20 power cuts)

1. **Use a cloned spare microSD card.** Cutting power on purpose can damage the card's file system.
2. Build static: `cmake --preset de1soc-release -DCMAKE_EXE_LINKER_FLAGS=-static && cmake --build --preset de1soc-release`. Copy `build/de1soc-release/sw/tools/save_loop/save_loop` to the board and note its sha256.
3. As root on the serial console, make sure the directory exists (`mkdir -p /var/lib/arcade`). Do not run `arcade` at the same time. The file must be on the Linux root file system, as `arcade` uses it; not on the FAT partition.
4. Run `./save_loop write`. Wait a different time each cycle (5 to 60 s), then switch the power off while it is still printing.
5. Power on, log in and run `./save_loop check`. Record the line it prints.
6. Repeat steps 4 and 5 until 20 cuts are done. **Pass:** 20 of 20 checks print `PASS`.
7. Restore a real high-score file afterwards (or delete the file: a missing file gives empty tables, SR-FW-08).

Record the run in `board/tests/` like `lw-bridge-throughput.md`: set-up, binary sha256, the 20 check lines verbatim, the verdict.

The waiting time, the progress interval and the reference scores are test choices, not SRS values.

Rules: see [`AGENTS.md`](/AGENTS.md).
