# DE1-SoC board setup

Everything here comes from the SRS (sections 2.4, 2.6, 3.1.2, 4.2) unless marked **Open item**. Do not add hardware values that are not in the SRS or the DE1-SoC manual (R-10.10).

## 1. What you need (SRS 2.4)

- Terasic DE1-SoC board and its 12 V DC adapter
- microSD card with the Linux image, the `arcade` program, the FPGA configuration file and the high-score file
- Monitor with an analog VGA input that accepts 1024×768 at 60 Hz, and a VGA cable (15-pin D-SUB, male to male). The lab monitors may not have VGA; bring your own if needed.
- USB keyboard (HID class), in either HPS USB host port
- Optional: powered speakers or headphones in the **green line-out** jack (the board has no speaker)
- For development: a PC connected over USB for the serial console and for FPGA programming

## 2. Handling and power (SF-06, SF-07)

- Put the board on a **non-conductive surface** and handle it **only by its edges**.
- Connect or disconnect the 12 V supply **only with the power switch off**.
- Switch off **only after "SAFE TO SWITCH OFF"** is on screen. Switching off at any other time can corrupt the microSD card.

## 3. Configuration switch SW10 (MSEL[4:0])

Set SW10 for FPGA configuration by the HPS (SRS 2.4, DE1-SoC manual Table 3-2):

| Who loads the FPGA | SW10 MSEL[4:0] |
|---|---|
| Linux | `01010` |
| U-Boot, from the SD card | `00000` |

**Open item (owner: Muhammad, #39):** which of the two this project uses depends on the Linux image chosen in #39.

## 4. Linux image and microSD card

**Open item (owner: Muhammad, #39):** the Linux image, how the bitstream is loaded at boot (AS-02), and the card layout are decided in #39 and then documented here. Requirements the image must meet:

- root access to `/dev/mem` (AS-01), `/dev/input/event*` (AS-06) and `/dev/i2c-0` (SRS 3.1.3)
- the `arcade` user in the `input` group (SC-01)
- no network services listening (SC-02); a non-empty root password (SC-06)
- directory `/var/lib/arcade/` for `highscores.csv` (SR-FW-08); optional `/etc/arcade/keymap.conf` (SC-05)

## 5. Serial console

Connect the board's UART-to-USB port to your PC and open a terminal with the settings in the DE1-SoC manual's UART section. The console shows boot messages and errors such as "graphics hardware not ready" (SF-02).

## 6. Start and stop

1. Connect the monitor, keyboard and (optional) speakers, and insert the microSD card.
2. Switch on. The main menu appears within 60 s (PR-07).
3. To shut down: on the main menu hold **Esc** for 3 s, press **Enter** at "SHUT DOWN?", wait for **"SAFE TO SWITCH OFF"**, then switch off (SRS 2.6, SR-UI-14).
4. From the serial console during development, Ctrl+C stops the program cleanly (SR-FW-14).

Reset buttons (SRS 3.1.2.3): **KEY5** is a cold reset (restarts Linux and the game); **KEY7** is a warm reset (debugging). KEY0–KEY3 and SW0–SW9 are not used.

## 7. Deploying a new build

**Open item (owner: Muhammad, #39):** the copy-and-install steps are written here once `board/deploy/` exists.
