# `sw/tools/uinput/`

**Owner:** Huzaifa  
**Issues:** #23

uinput test tools for PR-06 ("report every key press and release, with none lost, at 30 keys per second for 60 s"). Linux only. Both programs need write access to the uinput device, which normally means root.

| Program | What it does |
|---|---|
| `input_throughput_test` | The PR-06 test. Creates a virtual keyboard, runs the real Input Manager on it at 60 ticks/s (App. A TICK_RATE), injects 1,800 press/release pairs at 30 keys/s and checks that exactly 1,800 KeyDown and 1,800 KeyUp come back, in the order they were sent. Last line `PASS` or `FAIL`; exit code 0 or 1. |
| `uinput_inject` | Creates the same virtual keyboard and presses keys on it, for manual tests of a running program such as the arcade. Its keys go to whatever reads that keyboard. |

Options (both): `--pairs N` (default 1800), `--rate N` keys per second (default 30), `--device PATH` (default `/dev/uinput`), `--delay N` seconds before the first key (`uinput_inject` only, default 3), `--help`.

## Running the PR-06 test

On a Linux PC, from the repository root:

```
cmake --preset pc-debug && cmake --build --preset pc-debug
sudo ./build/pc-debug/sw/tools/uinput/input_throughput_test
```

On the board, cross-build with `cmake --preset de1soc-release && cmake --build --preset de1soc-release`, copy `build/de1soc-release/sw/tools/uinput/input_throughput_test` to the board and run it as root. The board's kernel needs uinput (`CONFIG_INPUT_UINPUT`); if `/dev/uinput` is missing, try `modprobe uinput`.

The run takes about 61 s. CI does not run it (no uinput access, and CI must stay under 5 minutes); it runs the unit tests in `sw/tests/uinput/` instead, which check the plan, the checker and the options without a device.

## Decisions (area owner, #23)

- **`/dev/uinput` as the default path.** The SRS names uinput (PR-06, glossary) but gives no path. This is the path in the kernel's uinput documentation (`Documentation/input/uinput.rst`). `--device` overrides it.
- **Only the virtual keyboard is used.** `input_throughput_test` accepts only the device named `arcade uinput inject` as a keyboard, so it never grabs the PC's own keyboard, which usually has a lower event number (SR-INP-01). The Input Manager code itself is unchanged; the test derives from `LinuxEvdevSys` for this.
- **Keys:** the letters A to Z in turn, so `uinput_inject` cannot run commands if its keys reach a terminal (no Enter). The virtual keyboard reports every key SR-INP-01 requires, so the Input Manager selects it.
- **Timing:** press `i` at `i / rate` seconds, release halfway to the next press, so a key is always released before the next one is pressed. 1,800 pairs at 30/s last just under 60 s.
- **Ordering is checked, not only counts.** Each KeyDown must be followed by the KeyUp of the same key, in plan order; any other event (for example KeyboardLost) fails the test.
- **Not SRS values:** waiting up to 5 s for the virtual keyboard's `/dev/input` node (retry every 100 ms), 100 ms before the first key, and 500 ms of ticks after the last key so nothing still queued is missed.

Rules: see [`AGENTS.md`](/AGENTS.md).
