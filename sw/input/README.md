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
- **inotify watches `IN_CREATE` and `IN_ATTRIB` (SR-INP-15).** The SRS names `IN_CREATE` (App. A). A new `eventN` node can appear before its group and mode let the `arcade` user (SC-01) open it, so the scan after `IN_CREATE` may fail; the attribute change that follows starts another scan. Any change to `/dev/input` starts a scan, which skips devices that are not keyboards.
- **One scan straight after `ENODEV` (SR-INP-15).** A second keyboard already plugged in, or the same one plugged in again before `ENODEV` was read, is picked up in the same tick, after KeyboardLost. Changes seen while Ready do not start a scan.
- **KeyboardFound is reported whenever a keyboard is selected after start-up**, from Disconnected or NoKeyboard. Its keys are read from the next tick.
- **If the inotify watch cannot be set up, it is retried every tick**, with one scan when it starts working, so a keyboard plugged in meanwhile is not missed.
- **Scanning allocates (R-4.1.6).** A scan lists `/dev/input`, which allocates. It runs only at start-up and after a hot-plug change, which SR-INP-15 requires, never while a keyboard is Ready.

Rules: see [`AGENTS.md`](/AGENTS.md).
