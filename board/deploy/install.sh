#!/bin/bash
# install.sh: one-time board set-up for release 1.0 (#39). Run as root on the board.
# SC-01 (arcade user, input group), SC-02 (no listening services), SR-FW-08 (high-score
# directory), SR-FW-13 (poweroff on exit 3), AS-02 (bitstream loaded at boot).
#
# Usage: ./install.sh [SRC_DIR]          default SRC_DIR=/media/fat_partition
#        ./install.sh --no-autostart     everything except the boot job (for testing)
#        ./install.sh --remove-autostart turns the boot job off again
# SRC_DIR must hold: arcade (static build), arcade_top_1024.rbf, load_fpga.sh.
# The script is safe to run again. Reboot afterwards, then check: ss -tuln (empty).

set -eu

PREFIX=/usr/local/share/arcade
JOB=/etc/init/arcade.conf          # Upstart: the image boots with Upstart (init: ureadahead)
RULE=/etc/udev/rules.d/99-arcade-input.rules
HISCORE_DIR=/var/lib/arcade        # HISCORE_FILE directory (SR-FW-08, App. A)
AUTOSTART=yes
SRC=/media/fat_partition

for arg in "$@"; do
    case $arg in
        --no-autostart) AUTOSTART=no ;;
        --remove-autostart)
            echo manual > /etc/init/arcade.override
            echo "autostart off (/etc/init/arcade.override); remove that file to turn it on"
            exit 0 ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        -*) echo "unknown option $arg" >&2; exit 2 ;;
        *) SRC=$arg ;;
    esac
done

[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }
for f in arcade arcade_top_1024.rbf load_fpga.sh; do
    [ -f "$SRC/$f" ] || { echo "missing $SRC/$f" >&2; exit 1; }
done

step() { echo "== $*"; }

step "files to $PREFIX"
install -d -m 0755 "$PREFIX"
install -m 0755 "$SRC/arcade" "$PREFIX/arcade"
install -m 0644 "$SRC/arcade_top_1024.rbf" "$PREFIX/arcade_top_1024.rbf"
install -m 0755 "$SRC/load_fpga.sh" "$PREFIX/load_fpga.sh"
sha256sum "$PREFIX/arcade" "$PREFIX/arcade_top_1024.rbf"

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
[ -f "$HISCORE_DIR/highscores.csv" ] && chown arcade:arcade "$HISCORE_DIR/highscores.csv"
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

step "network services off (SC-02)"
# Names are the usual Ubuntu packages for the ports seen on 19 Nov (21, 22, 631, 5353, 5901,
# 6001). Only services that exist are touched; anything still listening is listed at the end.
for s in ssh vsftpd proftpd avahi-daemon cups cups-browsed lightdm vncserver xrdp; do
    done_any=no
    if [ -f "/etc/init/$s.conf" ]; then
        echo manual > "/etc/init/$s.override"; done_any=yes
    fi
    if [ -x "/etc/init.d/$s" ]; then
        update-rc.d "$s" disable >/dev/null 2>&1 || true
        service "$s" stop >/dev/null 2>&1 || true; done_any=yes
    fi
    [ "$done_any" = yes ] && echo "disabled: $s"
done
# The VNC/X listeners (5901, 6001) are started by a script, not a service name. Find it with:
#   grep -rl -i vnc /etc/init /etc/init.d /etc/rc.local /etc/X11 2>/dev/null

if [ "$AUTOSTART" = yes ]; then
    step "boot job $JOB (AS-02, SR-FW-13)"
    cat > "$PREFIX/arcade-run.sh" <<'EOF'
#!/bin/bash
# Started at boot by /etc/init/arcade.conf (#39). Loads our design, runs the arcade, and
# powers off only on exit status 3, "shutdown requested" (SR-FW-13, board/deploy/README.md).
cd /usr/local/share/arcade
./load_fpga.sh ./arcade_top_1024.rbf
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
