#!/usr/bin/env bash
# WP15: C++ headless SimHarness on the parity fixtures, via the Unraid lab (never compiles locally).
#   run_cpp.sh sync-build   rsync this tree to /lab/src/wp15y and build (lab.sh sync/build)
#   run_cpp.sh run          3 days x seeds 1..3 per fixture, hourly-ish dumps (every 40 ticks)
#   run_cpp.sh pull         copy /lab/out/wp15y -> ./out/cpp
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)
LAB="bash $REPO/tools/yoottower/remote/lab.sh"
FIX="tokyo_g2_hotel_subway tokyo_g4_blackout tokyo_g5_large_elevator hawaii_g2_resort tokyo_g3_venues"
case "${1:-}" in
sync-build) $LAB sync "$REPO" wp15y; $LAB build wp15y ;;
run)
  for s in ${SEEDS:-1 2 3}; do for f in $FIX; do
    $LAB exec "cd /lab/src/wp15y && d=/lab/out/wp15y/$f/s$s && rm -rf \$d && mkdir -p \$d/dumps && ( time xvfb-run -a -s '-screen 0 1280x800x24' build/OpenSkyscraper --ruleset yoottower --load-game tests/parity/$f.tower --sim 3d --sim-seed $s --sim-speed 3 --sim-out \$d/out.tower --sim-summary \$d/summary.json --sim-dump-every 40 \$d/dumps ) > \$d/log.txt 2>&1; grep -E 'real|ERROR|WARN' \$d/log.txt | head -3"
  done; done ;;
pull) mkdir -p "$HERE/out/cpp"; $LAB exec "cd /lab/out/wp15y && tar cf - ." | tar xf - -C "$HERE/out/cpp" ;;
*) sed -n 2,6p "$0" ;;
esac
