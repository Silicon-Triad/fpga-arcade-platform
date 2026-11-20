#!/bin/sh
# soak_monitor.sh: QA-05 soak monitor (#51).
# Every N s (default 60) prints one line:
#   t_s pid vmrss_kB fds frame_counter
# pid is DEAD when no arcade runs.
# Ctrl+C prints the first and last lines.
# Usage: ./soak_monitor.sh [N] | tee soak.log
# RW: path of the rw tool (default ./rw).

INT=${1:-60}
RW=${RW:-./rw}
# Frame Counter: kOffsetFrameCounter in
# sw/hal/register_map.hpp (SRS 3.1.4).
FC=0x8048

case $INT in
  ''|*[!0-9]*|0) echo "usage: $0 [seconds]" >&2
     exit 2 ;;
esac

# /proc/uptime: the board has no RTC.
up() { cut -d. -f1 /proc/uptime; }

T0=$(up)
FIRST=
LAST=
SPID=

sample() {
  t=$(( $(up) - T0 ))
  pid=$(pidof arcade | cut -d' ' -f1)
  rss=-
  fds=-
  if [ -n "$pid" ] && [ -r "/proc/$pid/status" ]; then
    rss=$(awk '/^VmRSS:/ {print $2}' \
      "/proc/$pid/status")
    fds=$(ls "/proc/$pid/fd" 2>/dev/null | wc -l)
  else
    pid=DEAD
  fi
  fc=$("$RW" "$FC" 2>/dev/null | awk 'END {print $NF}')
  [ -n "$fc" ] || fc=?
  LAST="$t $pid $rss $fds $fc"
  [ -n "$FIRST" ] || FIRST=$LAST
  echo "$LAST"
}

finish() {
  [ -n "$SPID" ] && kill "$SPID" 2>/dev/null
  echo "first: $FIRST"
  echo "last:  $LAST"
  exit 0
}
trap finish INT TERM

echo "# t_s pid vmrss_kB fds frame_counter"
while :; do
  sample
  sleep "$INT" &
  SPID=$!
  wait "$SPID"
  SPID=
done
