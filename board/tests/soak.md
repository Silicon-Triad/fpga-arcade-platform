# 2-hour soak (QA-05, #51)

**Requirement:** QA-05: 2 hours of continuous
play, alternating both games, without a crash,
a hang or corruption of the high-score file.
**Method:** Test (SRS: "2-hour soak test with a
scripted input program").
**Verdict:** not run yet.

Commands are short so they can be typed
through minicom.

## Tools

- `soak_monitor.sh` (this folder): one line
  every 60 s: `t_s pid vmrss_kB fds frame`.
- `soak_driver` (`sw/tools/soak/`): plays both
  games through uinput. It needs `/dev/uinput`
  (#70). Until then, play by hand with the
  schedule below.
- `save_loop check` (`sw/tools/save_loop/`,
  #45): checks the high-score file. Its `check`
  only accepts its own A/B tables, so here use
  the manual check in "End checks".

## Set-up

1. Load the design, check the ID:
   ```
   load_fpga.sh arcade_top_1024.rbf
   ./rw 0x8050
   ```
   Expect `0xA2CADE01`.
2. Copy `arcade`, `rw`, `soak_monitor.sh`
   (and `soak_driver` once #70 is done) to
   `/root`. Static builds:
   `-DCMAKE_EXE_LINKER_FLAGS=-static`.
3. Note the high-score file before the run:
   ```
   wc -l /var/lib/arcade/highscores.csv
   ```
4. Console 1 (minicom), start the game:
   ```
   ./arcade --backend=fpga --profile
   ```
   With the driver instead: unplug the real
   keyboard, run `./soak_driver > drv.log &`,
   then start `arcade` within 15 s.
5. Console 2 (ssh), start the monitor:
   ```
   ./soak_monitor.sh | tee soak.log
   ```
6. Write down the start time (uptime).

## During the run (2 h)

- Switch game about every 10 min: Esc to the
  menu, select the other game.
- Every 20-30 min, reach the top 10 and enter
  initials, so the file is saved during play.
- Use Pause now and then.
- Never hold Esc 3 s in the menu (shutdown).

Checkpoints: tick a row when you pass it.

| Time | Game | Top-10 entry | Notes |
|---|---|---|---|
| 0:00 | | | |
| 0:10 | | | |
| 0:20 | | | |
| 0:30 | | | |
| 0:40 | | | |
| 0:50 | | | |
| 1:00 | | | |
| 1:10 | | | |
| 1:20 | | | |
| 1:30 | | | |
| 1:40 | | | |
| 1:50 | | | |
| 2:00 | | | |

## End checks

1. Monitor: `pid` never `DEAD`; `frame`
   rises every line (about 3600 per 60 s);
   VmRSS and fds flat from first to last.
   Ctrl+C prints the first and last lines.
2. Quit `arcade` (Esc held 3 s, Enter).
   The `--profile` line: `ticks` equals
   `frames`, `present_errors 0`.
3. High-score file (TBL-HighScore): every
   line is `game_id,rank,initials,score,seq`,
   at most 10 per game (HISCORE_SLOTS),
   scores falling within a game:
   ```
   cat /var/lib/arcade/highscores.csv
   ```
   Restart `arcade`: both tables show the
   entries made during the run (SC-04 would
   drop a bad line).
4. `dmesg | tail` shows no new errors.

Pass: all four hold, no crash and no hang.

## Results

Fill in after the run.

| Item | Value |
|---|---|
| Date / uptime at start | |
| Design (.rbf, sha256) | |
| `arcade` commit, sha256 | |
| Input | by hand / `soak_driver` |
| Duration | |
| First monitor line | |
| Last monitor line | |
| `--profile` line | |
| Entries in the file | |
| Verdict | |

Monitor log and console output (verbatim):

```
```
