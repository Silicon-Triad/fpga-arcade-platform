# `sw/input/`

**Owner:** Huzaifa  
**Issues:** #20–#23

Input Manager (evdev, inotify) and TBL-KeyBinding. SRS: SR-INP, SC-05.

## Behaviour the SRS leaves open

Decided by the area owner in #20 (PR #61):

- **Which device is "first" (SR-INP-01):** the lowest event number, so `event2` is tried before `event10`. The choice is the same on every boot.
- **A keyboard that cannot be grabbed is not used (SR-INP-03).** If `EVIOCGRAB` fails, for example because another process (such as a stale `arcade` instance) already holds the keyboard, it is closed and the scan moves on. If no other keyboard is found, there is no input. This is stricter than SR-INP-03, which only requires the request; it keeps keys from reaching the Linux console. Board tests (#49) and hot-plug (#22) should expect it.
- **Read errors other than `EAGAIN` end the tick.** Events already read are kept. `ENODEV` handling is #22.

Rules: see [`AGENTS.md`](/AGENTS.md).
