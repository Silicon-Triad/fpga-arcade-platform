# `sw/tools/soak/`: soak driver

**Owner:** Muhammad  
**Issue:** #51 · **SRS:** QA-05 (2 hours of continuous play, alternating both games, no crash, hang or high-score file corruption; verified by Test with a scripted input program).

`soak_driver` plays Snake and Breakout in turn by sending keys through a uinput virtual keyboard. It reuses `UinputDevice` from [`../uinput/`](../uinput/README.md) (#23, Huzaifa) unchanged, by linking `arcade_uinput`. The board run waits for #70 (the board kernel has no `/dev/uinput`). The monitor and the 2-hour procedure are in `board/tests/` (`soak_monitor.sh`, `soak.md`).

## Files

| File | What |
|---|---|
| `soak_plan.hpp/.cpp` | The key plan: a pure, seeded function, unit-tested in `sw/tests/soak/` |
| `soak_main.cpp` | Creates the virtual keyboard, waits `--delay` s, sends the plan, logs every key |

## The plan

Rounds alternate Snake and Breakout until the next round would not fit in `--minutes` (default 120, QA-05). One round (default about 4 minutes):

1. **Menu:** Down until the round's game is selected (SNAKE, BREAKOUT, SCORES, wrapping, SR-UI-01), then Enter (SR-UI-03). The driver cannot see the screen, so it tracks the selection itself, starting on SNAKE.
2. **Play** `--play` s (default 180) with pseudo-random keys about twice a second: Snake arrows; Breakout Left/Right holds and Space. P twice (pause, resume) every 60 s.
3. **End** (40 s): no steering. Snake hits a wall; Breakout launches each ball and then runs the paddle to one edge (alternating sides), so the ball comes back where the paddle is not and the lives run out. Check this in the live run: a ball that still hits the paddle only means that round ends on Esc from Playing.
4. **Initials** in every round: S, K, A, Enter. The Initials screen has no Back key (TBL-KeyBinding); three letters and Enter are the only way out (SR-UI-10), and they save the high-score file (SR-FW-09).
5. **Esc** (held 100 ms) back to the menu.

Every key means something harmless on the screens a round can end on, so the sequence always returns to the menu: on Playing and Paused the letters and Enter are unbound and Esc leaves; on GameOver the letters are unbound and Enter starts a new game that Esc then leaves; on Initials the letters and Enter save the entry and open High Scores, which Esc leaves. The test `QA_05_EveryRoundEndReturnsToTheMenu` checks this for every round against the real key bindings. M (Mute) is never pressed, and Esc is never held near SHUTDOWN_HOLD (3 s, SR-UI-14). A lost key could leave the menu selection out of step; the log shows it.

Delays and rates are test choices, not SRS values.

## Use

```
soak_driver [--minutes N] [--seed N] [--play N] [--device PATH] [--delay N] [--dry-run]
```

- `--dry-run` prints the whole plan without opening a device (works anywhere).
- Otherwise: as root, with the real keyboard **unplugged** (the Input Manager uses the first keyboard by event number and grabs it, SR-INP-01, SR-INP-03; an ungrabbed virtual keyboard would type into the console), run `soak_driver`, then start `arcade` within `--delay` s (default 15). The arcade must be on its menu with SNAKE selected when play starts.
- Ctrl+C releases any held key and stops. Each key is logged: `t=  12.345 s press   KEY_UP     snake play`.

Exit status: 0 plan finished (or `--dry-run`, `--help`), 1 device error or stopped, 2 bad arguments.

Short PC check: `sudo ./build/pc-debug/sw/tools/soak/soak_driver --minutes 3 --play 15` (two rounds: Snake, then Breakout), then start `./build/pc-debug/sw/app/arcade --backend=sdl` and keep its window focused (SDL gets the keys through the desktop, so they go to whatever window has focus).
