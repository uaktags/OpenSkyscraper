#!/usr/bin/env bash
# High-level YootTower oracle flows on top of run_yoot.sh (all coordinates are
# derived from live window positions, so they survive window-placement changes).
#
#   drive.sh title [shot.png]          fresh start, skip intro, stop at NEW/OPEN/QUIT dialog
#   drive.sh new [shot.png]            from title: click NEW (new-game / world selection)
#   drive.sh open <save> [shot.png]    from title: OPEN -> type path in file dialog -> Return
#   drive.sh load <save> [shot.png]    restart and load via command line (fastest, preferred)
#   drive.sh pause | resume            toggle the Control palette run/pause button
#   drive.sh geom NAME                 print "X Y W H" of the window whose title matches NAME
#
# Add PAUSE=1 to `load`/`open` to pause the clock right after the save is loaded.
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
R="$HERE/run_yoot.sh"
SHOTS=${YOOT_SHOTS:-$HOME/yoottower-decomp/oracle-shots}

geom() {  # geom REGEX -> "X Y W H" of the first matching window
    "$R" windows | python3 -I -c '
import re, sys
pat = re.compile(sys.argv[1])
for line in sys.stdin:
    m = re.match(r"\S+\s+(\d+)x(\d+)\s+\+(-?\d+)\+(-?\d+) ?(.*)$", line.rstrip("\n"))
    if m and pat.search(m.group(5)):
        w, h, x, y = m.group(1, 2, 3, 4); print(x, y, w, h); sys.exit(0)
sys.exit(1)' "$1"
}

click_rel() {  # click_rel REGEX DX DY
    local g; g=$(geom "$1") || { echo "drive: window '$1' not found" >&2; return 1; }
    read -r x y _ _ <<<"$g"
    "$R" click $((x + $2)) $((y + $3))
}

skip_intro() {
    # The intro is an untitled 640x480 window at (-5,-5); any click skips it.
    local end=$((SECONDS + 60))
    while (( SECONDS < end )); do
        if "$R" windows | grep -Eq ' 640x480 '; then "$R" click 512 384; sleep 1; else
            # The title dialog may exist before the intro ends; give it a moment.
            sleep 1; "$R" windows | grep -Eq ' 640x480 ' || return 0
        fi
    done
}

shot() { [[ -n ${1:-} ]] && "$R" shot "$1" || true; }

maybe_pause() { if [[ ${PAUSE:-0} == 1 ]]; then do_pause; fi; }
do_pause() {  # run/pause button: Control palette client offset (25,30); red "||" = running, green ">" = paused
    click_rel '^Control$' 25 30; sleep 0.5
}

cmd=${1:-help}; shift || true
case $cmd in
    title)
        "$R" stop >/dev/null 2>&1 || true
        "$R" start
        sleep 3; skip_intro
        "$R" wait-window '^YOOT TOWER$' 30 >/dev/null
        sleep 1; shot "${1:-$SHOTS/title.png}" ;;
    new)
        click_rel '^YOOT TOWER$' 356 83     # NEW button (dialog at +279+212 -> screen 635,295)
        sleep 3; shot "${1:-$SHOTS/new.png}" ;;
    open)
        [[ $# -ge 1 ]] || { echo "usage: drive.sh open <save> [shot.png]" >&2; exit 2; }
        save=$1
        [[ $save == *\\* || $save == [A-Za-z]:* ]] || { [[ -f $HOME/yoottower-decomp/game/YOOT/$save ]] && save="C:\\YOOT\\$save"; }
        [[ $save == *\\* || $save == [A-Za-z]:* ]] || save=$(WINEPREFIX=$HOME/yoottower-decomp/wineprefix winepath -w "$(realpath "$save")")
        click_rel '^YOOT TOWER$' 356 201    # OPEN button (screen 635,413)
        "$R" wait-window '^Open$' 20 >/dev/null
        sleep 0.5
        "$R" type "$save"; "$R" key Return
        "$R" wait-window ' - T2$' 90 >/dev/null
        "$R" wait-gone '^Loading$' 90 >/dev/null || true
        sleep 1; maybe_pause; shot "${2:-$SHOTS/open.png}" ;;
    load)
        [[ $# -ge 1 ]] || { echo "usage: drive.sh load <save> [shot.png]" >&2; exit 2; }
        "$R" stop >/dev/null 2>&1 || true
        "$R" start "$1"
        maybe_pause; shot "${2:-$SHOTS/load.png}" ;;
    pause|resume) do_pause ;;
    geom) geom "$1" ;;
    *) sed -n '2,13p' "$0"; exit 2 ;;
esac
