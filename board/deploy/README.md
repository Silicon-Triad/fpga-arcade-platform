# `board/deploy/`

**Owner:** Muhammad  
**Issues:** #39

Board start-up script, `arcade` user, poweroff wrapper, hardening. SRS: SC-01, SC-02, SC-06, CON-09.

## Exit status for the start-up script (#39)

After a confirmed shutdown, `arcade` exits with status **3** (`kExitShutdownRequested` in `sw/engine/shutdown.hpp`, SR-FW-13). Screen 10b is on the display and the high scores are saved. Under SC-01 the program has no root, so the root start-up script must run `poweroff` on this status, and only on this status. Status 0 (SIGINT, SIGTERM, SR-FW-14) must not power off.

## `install.sh`: one-time board set-up (#39)

Run once as root on the board, from a copy on the FAT partition. The partition must also hold `arcade` (static build), `arcade_top_1024.rbf` and `load_fpga.sh`:

```
bash /media/fat_partition/install.sh
```

It does the following, and is safe to run again:
- copies the three files to `/usr/local/share/arcade/`
- creates group `input` and user `arcade` (no login, in groups `input` and `i2c`) (SC-01)
- creates `/var/lib/arcade/` owned by `arcade` (SR-FW-08)
- adds a udev rule giving group `input` access to `/dev/input/event*` (SC-01, AS-06)
- disables the network services that exist under their usual names (SC-02)
- installs the Upstart job `/etc/init/arcade.conf`, which loads the bitstream, starts `arcade --backend=fpga` and runs `poweroff` only on status 3 (AS-02, SR-FW-13)

Options:
- `--no-autostart`: everything except the boot job, for test sessions.
- `--remove-autostart`: turns the boot job off (`/etc/init/arcade.override`).

Reboot afterwards and check `ss -tuln` (empty), `id arcade` and `ls -l /dev/input/event0`.

**Known limits:**
- `arcade` does not drop root yet (SC-01 needs a code change), so the boot job still runs it as root.
- The VNC/X listeners (5901, 6001) are started by a script, not by a named service. If `ss -tuln` still lists them, find that script (the command is in `install.sh`) and disable it.
- uinput (#70) needs a kernel rebuild, which is outside this script.

Rules: see [`AGENTS.md`](/AGENTS.md).
