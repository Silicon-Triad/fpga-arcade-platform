# uinput on the board, and PR-06 key throughput (#70)

**Requirements:** PR-06 (input throughput), QA-05 (scripted soak through uinput). **Method:** Test, on the board.
**Verdict: PASS.** `uinput` builds against the image's own kernel tree, loads, and survives a reboot; `input_throughput_test` reports 1,800 of 1,800 KeyDown and KeyUp events with 0 errors.

## Problem

The image's kernel (`3.18.0 #9 SMP Mon Aug 8 17:11:41 EDT 2016`, Intel UP image) has no uinput: `/dev/uinput` was missing, `modprobe uinput` failed ("Module uinput not found"), and `/proc/config.gz` has no `CONFIG_INPUT_UINPUT` line (`CONFIG_INPUT_MISC` is not set). `input_throughput_test` (PR-06) and `soak_driver` (#131, QA-05) need it.

## Why a module built on the board matches the kernel

Checked on the card's Linux partition on 20 Nov 2026:

| Check | Result |
|---|---|
| `CONFIG_MODULES` / `CONFIG_MODVERSIONS` | `y` / `y`: modules load, and every kernel symbol a module uses must match the running kernel's checksum |
| `/lib/modules/3.18.0/build` | link to `/usr/src/3.18.0`, a full kernel tree with `Module.symvers`, `.config`, `drivers/input/misc/uinput.c` |
| tree `.config` vs `/proc/config.gz` (board snapshot of 12 Nov) | identical (`diff` prints nothing) |
| `include/generated/utsrelease.h` | `"3.18.0"`, same as `uname -r` |
| `scripts/mod/modpost`, `fixdep`, `genksyms` | ARM binaries (built for the board) |
| `Module.symvers` | has the input core, e.g. `0xd53ab190 input_register_device vmlinux EXPORT_SYMBOL` |
| compiler on the board | `gcc-4.6`, `make` |

So the module is built on the board with `make -C /lib/modules/3.18.0/build M=$PWD modules` against the same tree and checksums as the running kernel. The script is `board/deploy/build_uinput.sh`.

## Board run (20 Nov 2026)

Board: DE1-SoC rev G0, Intel UP image, kernel `3.18.0 #9 SMP`. `input_throughput_test` from the static build of `main` at `23c1765` (sha256 `4090f70a6dfdf3b4bfb1c1cd01aa3795507735babad0cb2de7395279d7101995`).

First build: `uinput.c` includes `"../input-compat.h"`, which is in its parent directory in the kernel tree. Copied next to it with the include changed to `"input-compat.h"` (code unchanged). Then:

```
root@de1soclinux:~/uinput# make -C /lib/modules/$(uname -r)/build M=$PWD modules
make: Entering directory `/usr/src/3.18.0'
  CC [M]  /home/root/uinput/uinput.o
  Building modules, stage 2.
  MODPOST 1 modules
  CC      /home/root/uinput/uinput.mod.o
  LD [M]  /home/root/uinput/uinput.ko
make: Leaving directory `/usr/src/3.18.0'
root@de1soclinux:~/uinput# modinfo ./uinput.ko
filename:       ./uinput.ko
version:        0.3
license:        GPL
description:    User level driver support for input subsystem
author:         Aristeu Sergio Rozanski Filho
alias:          devname:uinput
alias:          char-major-10-223
srcversion:     7A7EB0625C1EAC9B20E83D7
depends:
vermagic:       3.18.0 SMP mod_unload modversions ARMv7 p2v8
```

Installed to `/lib/modules/3.18.0/kernel/drivers/input/misc/`, `depmod -a`, `modprobe uinput`:

```
root@de1soclinux:~/uinput# ls -l /dev/uinput; lsmod | grep uinput
crw-r----- 1 root root 10, 223 Jan  1 00:29 /dev/uinput
uinput                  7098  0
root@de1soclinux:~/uinput# grep uinput /proc/misc
223 uinput
```

`uinput` added to `/etc/modules` (loaded at every boot). PR-06:

```
root@de1soclinux:~# ./input_throughput_test; echo "exit=$?"
keyboard: /dev/input/event2
injecting 1800 pairs at 30 keys/s (60.0 s), polling at 60 ticks/s
KeyDown: 1800 of 1800
KeyUp:   1800 of 1800
errors:  0
ticks:   3637, most events in one tick: 1
PASS
exit=0
```

`soak_driver` creates its virtual keyboard (`--minutes 1` plans 0 rounds, since one round is 3 min play + 40 s; this only checks the device):

```
root@de1soclinux:~# ./soak_driver --minutes 1 --delay 0
soak_driver (QA-05): 1 min, seed 1, 0 rounds, 0 key events
virtual keyboard 'arcade-soak-keyboard' created; start arcade now (0 s)
soak_driver: plan finished
```

After `reboot`, with no manual `modprobe`:

```
root@de1soclinux:~# ls -l /dev/uinput; lsmod | grep uinput
crw-r----T 1 root root 10, 223 Jan  1 00:00 /dev/uinput
uinput                  7098  0
```

## Notes

- `/dev/uinput` is root-only (`crw-r-----`), so `input_throughput_test` and `soak_driver` run as root, as their READMEs say.
- A card written fresh from the image needs the module again: run `board/deploy/build_uinput.sh` on the board, or `install.sh`, which calls it.
- The full scripted soak with `soak_driver` (#51) is now possible; it was not run here.
