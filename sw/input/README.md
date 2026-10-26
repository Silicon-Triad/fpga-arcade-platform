# `sw/input/`

**Owner:** Huzaifa  
**Issues:** #20–#23

Input Manager (evdev, inotify) and TBL-KeyBinding. SRS: SR-INP, SC-05.

## Behaviour the SRS leaves open

Decided by the area owner in #20 (PR #61):

- **Which device is "first" (SR-INP-01):** the lowest event number, so `event2` is tried before `event10`. The choice is the same on every boot.
- **A keyboard that cannot be grabbed is not used (SR-INP-03).** If `EVIOCGRAB` fails, for example because another process (such as a stale `arcade` instance) already holds the keyboard, it is closed and the scan moves on. If no other keyboard is found, there is no input. This is stricter than SR-INP-03, which only requires the request; it keeps keys from reaching the Linux console. Board tests (#49) and hot-plug (#22) should expect it.
- **Read errors other than `EAGAIN` and `ENODEV` end the tick.** Events already read are kept, and so is the keyboard.

Decided by the area owner in #22:

- **Events read before `ENODEV` are kept (SR-INP-14).** In the tick the keyboard is lost, the events read before `read()` returned `ENODEV` are delivered first, then KeyboardLost. Keys are set to released without KeyUp events (SR-INP-13 asks for the state, not events).
- **inotify watches `IN_CREATE` and `IN_ATTRIB` (SR-INP-15).** The SRS names `IN_CREATE` (App. A). A new `eventN` node can appear before its group and mode let the `arcade` user (SC-01) open it, so the check after `IN_CREATE` may fail; the attribute change that follows reports the node again and it is checked again. Seen on a CachyOS laptop as the `arcade`-like user: `open ... Permission denied`, then the keyboard found one change later.
- **After start-up, only the devices inotify reports are checked (SR-INP-15).** A full scan opens every `/dev/input/event*` device, and opening some of them is slow: on a test laptop, one device took about 650 ms per `open()`, which froze the tick for 0.7 s on every change and made a reconnect take about 1.4 s. So inotify's entry names are kept (`eventN` only, in a fixed-size set, no allocation) and, in Disconnected or NoKeyboard, only those devices are checked, lowest number first. The start-up scan is still a full scan (SR-INP-01).
- **Devices reported while Ready are kept for later.** A second keyboard plugged in during play, or the same one plugged in again before `ENODEV` was read, is checked straight after KeyboardLost and can be found in the same tick. A device that fails the check is dropped until inotify reports it again. A keyboard that was already present at start-up but not selected is not found after an unplug, because SR-INP-15 is about newly plugged-in keyboards.
- **Full scan as a fallback.** If more than 16 devices change before the next check, or the kernel reports lost inotify events (`IN_Q_OVERFLOW`), the next check is one full scan.
- **KeyboardFound is reported whenever a keyboard is selected after start-up**, from Disconnected or NoKeyboard. Its keys are read from the next tick.
- **If the inotify watch cannot be set up, it is retried every tick**, with one full scan when it starts working, so a keyboard plugged in meanwhile is not missed.
- **Checking devices allocates (R-4.1.6).** Building device paths and a full scan's directory listing allocate. That happens only at start-up and when a device is checked after a hot-plug change, which SR-INP-15 requires, never while a keyboard is Ready. Reading inotify itself does not allocate.

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
