#!/bin/bash
# build_uinput.sh: build and load the uinput kernel module on the board (#70; PR-06, QA-05).
# Run as root on the board. The image's kernel (3.18.0, Intel UP image) has no uinput, but the
# image ships the kernel tree it was built from at /lib/modules/3.18.0/build (= /usr/src/3.18.0):
# same .config as /proc/config.gz, and the Module.symvers checksums of the running kernel
# (CONFIG_MODVERSIONS=y). The module is built against that tree with the board's own gcc, so
# its version string and symbol checksums match the running kernel.
#
# Usage: ./build_uinput.sh            build (if needed), install, load, and load at every boot
# Safe to run again: an installed module is not rebuilt.
# Tested on the board 20 Nov 2026; record in board/tests/uinput.md.

set -eu

[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }

KREL=$(uname -r)
KBUILD=/lib/modules/$KREL/build
SRC=$KBUILD/drivers/input/misc/uinput.c
DEST=/lib/modules/$KREL/kernel/drivers/input/misc
WORK=/home/root/uinput
# The running kernel's module version string (modinfo vermagic), as seen on the board for this
# image on 20 Nov 2026. The module must report exactly this before it is installed.
VERMAGIC_PREFIX="$KREL SMP mod_unload modversions ARMv7"

if [ ! -f "$DEST/uinput.ko" ]; then
    [ -f "$SRC" ] && [ -f "$KBUILD/Module.symvers" ] || {
        echo "no kernel tree with Module.symvers at $KBUILD: cannot build uinput" >&2
        exit 1
    }
    echo "== build uinput.ko against $KBUILD"
    mkdir -p "$WORK"
    cd "$WORK"
    cp "$SRC" .
    # uinput.c includes "../input-compat.h" (its parent directory in the kernel tree); copy the
    # header next to it and point the include there. The code itself is unchanged.
    cp "$KBUILD/drivers/input/input-compat.h" .
    sed -i 's|"../input-compat.h"|"input-compat.h"|' uinput.c
    echo 'obj-m := uinput.o' > Makefile
    make -C "$KBUILD" M="$WORK" modules

    VM=$(modinfo -F vermagic ./uinput.ko)
    case "$VM" in
        "$VERMAGIC_PREFIX"*) echo "vermagic: $VM" ;;
        *) echo "vermagic '$VM' does not start with '$VERMAGIC_PREFIX': not installed" >&2
           exit 1 ;;
    esac
    mkdir -p "$DEST"
    cp uinput.ko "$DEST/"
    depmod -a
fi

echo "== load uinput"
modprobe uinput
# /etc/modules: the image loads the modules listed there at every boot.
grep -qx uinput /etc/modules || echo uinput >> /etc/modules
ls -l /dev/uinput
echo "uinput ready; check with ./input_throughput_test (PR-06)"
