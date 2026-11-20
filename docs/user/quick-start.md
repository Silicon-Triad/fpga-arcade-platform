# FPGA Arcade: quick-start card

**Snake and Breakout on one DE1-SoC board.** Keep this card next to the board.

## Before you touch the board

> **Place the board on a non-conductive surface, handle it only by its edges, and connect or disconnect the 12 V supply only with the power switch off.**

## 1. Connect

1. **microSD card** into the board's microSD socket.
2. **Monitor:** VGA cable from the board's VGA port to a monitor with a VGA input that shows 1024×768 at 60 Hz.
3. **Keyboard:** USB keyboard into either of the board's two USB ports.
4. **Speakers (optional):** powered speakers or headphones into the **green line-out** jack. The board has no speaker; without one the games run silently.
5. **Power:** with the power switch **off**, plug the 12 V adapter into the board.

## 2. Set SW10

Set the configuration switch **SW10** to `01010`: positions **1 ON, 2 OFF, 3 ON, 4 OFF, 5 ON**.

## 3. Start

Switch the power on. The 12 V power LED (D14) lights, and the main menu appears within **60 seconds**.

- If the screen says to plug in a USB keyboard, plug one in.
- **Sound is on at start.** Press **M** to switch it off or on.
- Main menu: **↑ / ↓** choose a game, **Enter** starts it, **H** shows high scores.

## 4. Shut down

> **On the main menu, hold Esc for 3 s, press Enter at "SHUT DOWN?", wait for "SAFE TO SWITCH OFF", then switch off the power.**

> **Switch off power only after "SAFE TO SWITCH OFF" appears. Switching off at any other time can corrupt the microSD card.**

Then, with the power off, you may unplug the 12 V adapter.

---
<sub>Sources: SRS §2.4 (connections, monitor, keyboard, speakers), §2.6 and SF-06, SF-07 (warnings and shutdown steps, word for word), §3.1.1 screens 1, 2 and 10 (keys), §3.1.2.3 (power LED D14), PR-07 (60 s), SF-05 and decision D3 (sound on at start). SW10 `01010` and its switch positions: as measured on the board (`sw/hal/README.md`, `board/tests/lw-bridge-throughput.md`). Issue #54.</sub>
