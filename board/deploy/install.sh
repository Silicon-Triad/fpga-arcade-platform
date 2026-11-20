#!/bin/bash
# install.sh: one-time board set-up for release 1.0 (#39). Run as root on the board, from the
# serial console. SC-01 (arcade user, input group), SC-02 (no listening services), SR-FW-08
# (high-score directory), SR-FW-13 (poweroff on exit 3), AS-02 (bitstream loaded at boot).
#
# Usage: ./install.sh [SRC_DIR]          default SRC_DIR=/media/fat_partition
#        ./install.sh --no-autostart     everything except the boot job (for testing)
#        ./install.sh --remove-autostart turns the boot job off again
#        ./install.sh --yes ...          answer yes at every checkpoint (no prompts)
# SRC_DIR must hold: arcade and codec_setup (static builds), arcade_top_1024.rbf, load_fpga.sh.
# The script stops at checkpoints and asks before each step that is hard to undo.
# It is safe to run again. Reboot afterwards, then check: ss -tuln (empty).

set -eu

case "${1:-}" in -h|--help) sed -n '2,12p' "$0"; exit 0 ;; esac
[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }

PREFIX=/usr/local/share/arcade
JOB=/etc/init/arcade.conf          # Upstart job; checked below with initctl
RULE=/etc/udev/rules.d/99-arcade-input.rules
# Directory of HISCORE_FILE (/var/lib/arcade/highscores.csv, App. A, SR-FW-08). Keep in step
# with HISCORE_FILE in sw/include/arcade/tables.hpp.
HISCORE_DIR=/var/lib/arcade
HISCORE_NAME=highscores.csv
AUTOSTART=yes
ASSUME_YES=no
SRC=/media/fat_partition

for arg in "$@"; do
    case $arg in
        --no-autostart) AUTOSTART=no ;;
        --yes) ASSUME_YES=yes ;;
        --remove-autostart)
            echo manual > /etc/init/arcade.override
            echo "autostart off (/etc/init/arcade.override); remove that file to turn it on"
            exit 0 ;;
        -*) echo "unknown option $arg" >&2; exit 2 ;;
        *) SRC=$arg ;;
    esac
done

step() { echo; echo "== $*"; }

# Checkpoint: a human answers y to continue. Anything else stops the script here, with
# nothing after this point changed. --yes answers for them; with no terminal and no --yes the
# script stops rather than guess.
confirm() {
    if [ "$ASSUME_YES" = yes ]; then
        echo "-- $1 [--yes]"
        return 0
    fi
    if ! (: < /dev/tty) 2>/dev/null; then
        echo "-- $1: no terminal to ask on; rerun with --yes" >&2
        exit 1
    fi
    local answer
    printf -- '-- %s [y/N] ' "$1"
    read -r answer < /dev/tty
    case $answer in
        y|Y|yes) return 0 ;;
        *) echo "stopped by the user; nothing after this point was changed"; exit 1 ;;
    esac
}

for f in arcade codec_setup arcade_top_1024.rbf load_fpga.sh; do
    [ -f "$SRC/$f" ] || { echo "missing $SRC/$f" >&2; exit 1; }
done

step "checkpoint 1: before any change"
echo "This installs the arcade on this card: files, user arcade, udev rule, high-score"
echo "directory, network services OFF (ssh too) and, unless --no-autostart, the boot job."
echo "After it, the serial console is the only way in. Have a serial cable connected, and"
echo "keep an untouched card (or an image of this one) for recovery."
sha256sum "$SRC/arcade" "$SRC/codec_setup" "$SRC/arcade_top_1024.rbf" "$SRC/load_fpga.sh"
confirm "serial console connected and a recovery card kept; continue?"

step "files to $PREFIX"
install -d -m 0755 "$PREFIX"
install -m 0755 "$SRC/arcade" "$PREFIX/arcade"
install -m 0644 "$SRC/arcade_top_1024.rbf" "$PREFIX/arcade_top_1024.rbf"
install -m 0755 "$SRC/load_fpga.sh" "$PREFIX/load_fpga.sh"
install -m 0755 "$SRC/codec_setup" "$PREFIX/codec_setup"
# The start script the boot job runs; also usable by hand.
cat > "$PREFIX/arcade-run.sh" <<'EOF'
#!/bin/bash
# Started at boot by /etc/init/arcade.conf (#39). Loads our design, runs the arcade, and
# powers off only on exit status 3, "shutdown requested" (SR-FW-13, board/deploy/README.md).
cd /usr/local/share/arcade
# A failed load leaves a fabric that is not our design, perhaps with the bridges disabled;
# an access over the bridge could then hang the HPS. Stop without powering off (review #134).
if ! ./load_fpga.sh ./arcade_top_1024.rbf; then
    echo "FPGA load failed; not starting arcade" >&2
    exit 1
fi
# Interim (#39): arcade does not configure the codec itself yet, so do it here after every
# load (the load resets Audio Enable; output stays 0 until it is set, SR-AUD-09, SR-HAL-10).
# Sound is a "should": a codec failure is logged and the game still starts.
if ! ./codec_setup; then
    echo "codec set-up failed; starting arcade without sound" >&2
fi
./arcade --backend=fpga
rc=$?
echo "arcade exited with status $rc"
if [ "$rc" -eq 3 ]; then
    sync
    poweroff
fi
exit "$rc"
EOF
chmod 0755 "$PREFIX/arcade-run.sh"

step "group input, user arcade (SC-01)"
getent group input >/dev/null || groupadd --system input
NOLOGIN=/usr/sbin/nologin
[ -x "$NOLOGIN" ] || NOLOGIN=/bin/false
if ! id arcade >/dev/null 2>&1; then
    useradd --system --no-create-home --home-dir "$HISCORE_DIR" --shell "$NOLOGIN" arcade
fi
usermod -a -G input arcade
# /dev/i2c-0 is root:i2c 0660 on this image; the codec set-up needs it (SC-01, SR-HAL-10).
getent group i2c >/dev/null && usermod -a -G i2c arcade
id arcade

step "high-score directory $HISCORE_DIR (SR-FW-08)"
install -d -o arcade -g arcade -m 0755 "$HISCORE_DIR"
[ -f "$HISCORE_DIR/$HISCORE_NAME" ] && chown arcade:arcade "$HISCORE_DIR/$HISCORE_NAME"
ls -ld "$HISCORE_DIR"

step "keyboard devices readable by group input (SC-01, AS-06)"
install -d -m 0755 "$(dirname "$RULE")"
cat > "$RULE" <<'EOF'
# arcade (#39, SC-01): the arcade user reads keyboards through group input.
SUBSYSTEM=="input", KERNEL=="event*", GROUP="input", MODE="0660"
EOF
udevadm control --reload-rules 2>/dev/null || true
udevadm trigger --subsystem-match=input 2>/dev/null || true
ls -l /dev/input/ || true

step "checkpoint 2: network services (SC-02)"
# Names are the usual Ubuntu packages for the ports seen on 19 Nov (21, 22, 631, 5353, 5901,
# 6001). Only services that exist are touched; anything still listening is listed at the end.
FOUND=
for s in ssh vsftpd proftpd avahi-daemon cups cups-browsed lightdm vncserver xrdp; do
    if [ -f "/etc/init/$s.conf" ] || [ -x "/etc/init.d/$s" ]; then FOUND="$FOUND $s"; fi
done
echo "Listening now:"
ss -tuln 2>/dev/null || netstat -tuln 2>/dev/null || echo "(no ss or netstat)"
echo "Services found to disable:${FOUND:- none}"
if [ -n "$FOUND" ]; then
    confirm "disable these (ssh included: serial console only afterwards)?"
    for s in $FOUND; do
        [ -f "/etc/init/$s.conf" ] && echo manual > "/etc/init/$s.override"
        if [ -x "/etc/init.d/$s" ]; then
            update-rc.d "$s" disable >/dev/null 2>&1 || true
            service "$s" stop >/dev/null 2>&1 || true
        fi
        echo "disabled: $s"
    done
fi
# The VNC/X listeners (5901, 6001) are started by a script, not a service name. Find it with:
#   grep -rl -i vnc /etc/init /etc/init.d /etc/rc.local /etc/X11 2>/dev/null

if [ "$AUTOSTART" = yes ]; then
    step "checkpoint 3: boot job (AS-02, SR-FW-13)"
    # The boot job is an Upstart job (the 19 Nov boot log shows "init: ureadahead", an Upstart
    # job). If init is not Upstart, /etc/init/arcade.conf would do nothing, so check first.
    if ! initctl version 2>/dev/null | grep -qi upstart; then
        echo "init is not Upstart (initctl version did not say so); boot job NOT installed." >&2
        echo "Everything else is done. Start the game by hand: $PREFIX/arcade-run.sh" >&2
        AUTOSTART=no
    fi
fi

if [ "$AUTOSTART" = yes ]; then
    # Try the exact load the boot job will do, once, while a person watches. The boot job only
    # starts arcade when this load succeeds (review of #134), so check that it succeeds here.
    echo "Test load: $PREFIX/load_fpga.sh $PREFIX/arcade_top_1024.rbf"
    set +e
    (cd "$PREFIX" && ./load_fpga.sh ./arcade_top_1024.rbf)
    rc=$?
    set -e
    echo "load_fpga.sh exit status: $rc"
    if [ "$rc" -ne 0 ]; then
        echo "The load failed (or load_fpga.sh does not exit 0 on success): boot job NOT" >&2
        echo "installed. Fix load_fpga.sh, then rerun this script." >&2
        exit 1
    fi
    confirm "install the boot job (arcade starts at every power-on; undo: --remove-autostart)?"
    cat > "$JOB" <<EOF
# arcade (#39): start the game at boot. Turn off with: echo manual > /etc/init/arcade.override
description "FPGA arcade"
start on (local-filesystems and runlevel [2345])
stop on runlevel [016]
console output
task
exec $PREFIX/arcade-run.sh
EOF
    rm -f /etc/init/arcade.override
    echo "autostart on"
fi

step "done; reboot, then check:"
echo "  ss -tuln                    (SC-02: nothing listed)"
echo "  id arcade                   (SC-01: groups include input)"
echo "  ls -l /dev/input/event0     (group input)"
echo "Listening now (before reboot):"
ss -tuln 2>/dev/null || netstat -tuln 2>/dev/null || echo "(no ss or netstat)"
if [ "$ASSUME_YES" = yes ]; then
    echo "not rebooting (--yes); reboot by hand"
    exit 0
fi
printf -- '-- reboot now? [y/N] '
read -r answer < /dev/tty
case $answer in
    y|Y|yes) sync; reboot ;;
    *) echo "not rebooted; reboot by hand before the checks" ;;
esac
