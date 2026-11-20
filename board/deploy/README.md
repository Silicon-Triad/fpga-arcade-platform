# `board/deploy/`

**Owner:** Muhammad  
**Issues:** #39

Board start-up script, `arcade` user, poweroff wrapper, hardening. SRS: SC-01, SC-02, SC-06, CON-09.

## Exit status for the start-up script (#39)

After a confirmed shutdown, `arcade` exits with status **3** (`kExitShutdownRequested` in `sw/engine/shutdown.hpp`, SR-FW-13). Screen 10b is on the display and the high scores are saved. Under SC-01 the program has no root, so the root start-up script must run `poweroff` on this status, and only on this status. Status 0 (SIGINT, SIGTERM, SR-FW-14) must not power off.

## `install.sh`: one-time board set-up (#39)

Run once as root **on the serial console**, from a copy on the FAT partition. The partition must also hold `arcade` (static build), `arcade_top_1024.rbf` and `load_fpga.sh`:

```
bash /media/fat_partition/install.sh
```

**Recovery first:** keep a serial cable connected, and an untouched card (or an image of this one). The script turns ssh off (SC-02), so afterwards the serial console is the only way in. `--remove-autostart` also needs a shell.

The script stops at checkpoints and waits for `y` before each step that is hard to undo. Any other answer stops it there, with nothing after that point changed.

| Checkpoint | Shows | Then |
|---|---|---|
| 1 | what will change, sha256 of the three files | copies them to `/usr/local/share/arcade/` (with `arcade-run.sh`); creates group `input` and user `arcade` (groups `input`, `i2c`, no login) (SC-01); creates `/var/lib/arcade/` owned by `arcade` (SR-FW-08); adds a udev rule giving group `input` `/dev/input/event*` |
| 2 | `ss -tuln` and the services found | disables them (SC-02) |
| 3 | `initctl version` must say Upstart, and **a test load** of the bitstream with its exit status | installs the Upstart job `/etc/init/arcade.conf` (AS-02, SR-FW-13) |
| end | `ss -tuln` | asks whether to reboot |

The boot job runs `arcade-run.sh`, which:
- loads the bitstream;
- **stops without starting `arcade` if the load fails** (a fabric that is not our design could hang the HPS on a bridge access);
- otherwise runs `arcade --backend=fpga`;
- runs `poweroff` only on exit status 3.

The test load at checkpoint 3 makes sure `load_fpga.sh` exits 0 on success. If it doesn't, the boot job is not installed.

**Options:**
- `--no-autostart`: everything except the boot job, for test sessions.
- `--remove-autostart`: turns the boot job off (`/etc/init/arcade.override`).
- `--yes`: answers every checkpoint, for re-running on a known card. Without a terminal and without `--yes`, the script stops at checkpoint 1.

Reboot afterwards and check `ss -tuln` (empty), `id arcade` and `ls -l /dev/input/event0`.

**Why Upstart:** the image's boot log of 19 Nov shows `init: ureadahead main process (639) terminated with status 5`. ureadahead is an Upstart job, and the shutdown log shows Ubuntu's Upstart/SysV scripts. The script checks `initctl version` too, and skips the boot job if init is not Upstart.

**Known limits:**
- `arcade` does not drop root yet (SC-01 needs a code change), so the boot job still runs it as root.
- The VNC/X listeners (5901, 6001) are started by a script, not by a named service. If `ss -tuln` still lists them, find that script (the command is in `install.sh`) and disable it.
- uinput (#70) needs a kernel module or rebuild, which is outside this script.

Rules: see [`AGENTS.md`](/AGENTS.md).
