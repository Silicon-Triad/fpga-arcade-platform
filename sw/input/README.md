# `sw/input/`

**Owner:** Huzaifa  
**Issues:** #20–#23

Input Manager (evdev, inotify) and TBL-KeyBinding. SRS: SR-INP, SC-05.

## Behaviour the SRS leaves open

Decided by the area owner in #20 (PR #61):

- **Which device is "first" (SR-INP-01):** the lowest event number, so `event2` is tried before `event10`. The choice is the same on every boot.
- **A keyboard that cannot be grabbed is not used (SR-INP-03).** If `EVIOCGRAB` fails, for example because another process (such as a stale `arcade` instance) already holds the keyboard, it is closed and the scan moves on. If no other keyboard is found, there is no input. This is stricter than SR-INP-03, which only requires the request; it keeps keys from reaching the Linux console. Board tests (#49) and hot-plug (#22) should expect it.
- **Read errors other than `EAGAIN` end the tick.** Events already read are kept. `ENODEV` handling is #22.

## keymap.conf (SC-05)

`/etc/arcade/keymap.conf` is optional and read once at start-up. The SRS names the file but not its line format; the area owner chose this one in #21:

```
# <Context> <KEY_NAME> <Action>     fields separated by spaces or tabs; '#' starts a comment
Snake     KEY_UP    Up
Breakout  KEY_SPACE Launch
```

- Context and action names are exactly as in Chapter 5 (TBL-KeyBinding), case-sensitive.
- Key names are the Linux `KEY_*` names of keys that have a default binding: `KEY_A` to `KEY_Z`, the four arrows, `KEY_ENTER`, `KEY_ESC`, `KEY_SPACE` and `KEY_BACKSPACE`.
- **Replace-only:** a line can change the action of an existing (context, key) row, never add a new key (Chapter 5: "may replace individual rows"; agreed with Muhammad on #21).
- `Char` is accepted only for `KEY_A` to `KEY_Z` in `Initials` (SR-INP-11).
- Any other line is ignored and that key keeps its binding (SC-05). If the same row appears twice, the later line wins.

The default rows are in `sw/include/arcade/default_keymap.hpp` (frozen). Only `sw/input/` includes it, because it needs `<linux/input.h>`; games never do (QA-03).

Rules: see [`AGENTS.md`](/AGENTS.md).
