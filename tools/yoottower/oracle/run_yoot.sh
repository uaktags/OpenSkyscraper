#!/usr/bin/env bash
# YootTower (1999, Win32) dynamic oracle: original T2.exe under Wine on a headless Xvfb.
#
#   run_yoot.sh setup                 idempotent: prefix, game copy, C:\YOOT, MFC42, registry
#   run_yoot.sh start [save.bld]      start Xvfb + T2.exe (optionally loading a save), wait for UI
#   run_yoot.sh shot <out.png>        screenshot of the whole 1024x768 screen
#   run_yoot.sh click X Y [button]    left click (button 3 = right)
#   run_yoot.sh dclick X Y | rclick X Y | move X Y | drag X1 Y1 X2 Y2
#   run_yoot.sh key KEY...            e.g. Return Escape alt+f ctrl+o F1
#   run_yoot.sh type "text"
#   run_yoot.sh windows               list visible windows: id WxH +X+Y title
#   run_yoot.sh wait-window REGEX [timeout_s]   wait until a window title matches
#   run_yoot.sh wait-gone REGEX [timeout_s]     wait until no window title matches
#   run_yoot.sh status | log | stop
#
# A save argument may be: a bare name in the game dir ("Yooooot.bld"), a Windows
# path ("C:\YOOT\x.bld"), or any host path (converted with winepath -> Z:\...).
#
# Environment overrides: YOOT_ROOT (default ~/yoottower-decomp), YOOT_SRC (original
# game dir, only read), YOOT_DISPLAY (:87), YOOT_SCREEN (1024x768x24), MFC42_DLL.
#
# Concurrent use: set YOOT_INSTANCE=<name> to get a private prefix, game copy,
# state/log dir and X display under $YOOT_ROOT/instances/<name>/. Without it,
# every caller shares one prefix and `stop` (wineserver -k) kills all of them.
set -euo pipefail

ROOT=${YOOT_ROOT:-$HOME/yoottower-decomp}
SRC=${YOOT_SRC:-$HOME/Downloads/Yoot-Tower_Win_EN/YOOT}
if [[ -n ${YOOT_INSTANCE:-} ]]; then
    [[ $YOOT_INSTANCE =~ ^[A-Za-z0-9_-]+$ ]] || { echo "run_yoot: bad YOOT_INSTANCE" >&2; exit 1; }
    ROOT=$ROOT/instances/$YOOT_INSTANCE
    # Stable per-instance display in :100-:599 unless YOOT_DISPLAY is given.
    DEFAULT_DISPLAY=:$(( 100 + $(printf '%s' "$YOOT_INSTANCE" | cksum | cut -d' ' -f1) % 500 ))
fi
GAME=$ROOT/game/YOOT
STATE=$ROOT/oracle-state
LOGS=$ROOT/logs
export WINEPREFIX=$ROOT/wineprefix
export DISPLAY=${YOOT_DISPLAY:-${DEFAULT_DISPLAY:-:87}}
SCREEN=${YOOT_SCREEN:-1024x768x24}
export WINEDEBUG=${WINEDEBUG:--all}
export WINEDLLOVERRIDES=${WINEDLLOVERRIDES:-"mscoree,mshtml="}
unset WAYLAND_DISPLAY
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
XIN=(python3 -P "$HERE/xinput.py")   # -P: never import from cwd/script dir; keeps user site (python-xlib)

mkdir -p "$STATE" "$LOGS"

die() { echo "run_yoot: $*" >&2; exit 1; }
alive() { [[ -f $1 ]] && kill -0 "$(cat "$1")" 2>/dev/null; }

find_mfc42() {
    local c
    for c in "${MFC42_DLL:-}" \
        "$HOME/.var/app/com.usebottles.bottles/data/bottles/bottles/YootTower/drive_c/windows/system32/mfc42.dll" \
        "$HOME/.wine/drive_c/windows/syswow64/mfc42.dll" \
        "$HOME/.wine/drive_c/windows/system32/mfc42.dll"; do
        [[ -n $c && -f $c ]] && { echo "$c"; return 0; }
    done
    return 1
}

cmd_setup() {
    command -v wine >/dev/null || die "wine not installed"
    command -v Xvfb >/dev/null || die "Xvfb not installed"
    python3 -P -c 'import Xlib.ext.xtest' 2>/dev/null || die "python-xlib missing (pip install --user python-xlib)"
    if [[ ! -f $GAME/T2.exe ]]; then
        [[ -f $SRC/T2.exe ]] || die "original game not found at $SRC"
        mkdir -p "$GAME"
        cp -a "$SRC/." "$GAME/"
        echo "copied game -> $GAME"
    fi
    if [[ ! -f $WINEPREFIX/system.reg ]]; then
        # Fedora wine 11 is pure wow64: WINEARCH=win32 is rejected, so use the default prefix.
        local started=0
        if ! xdpyinfo >/dev/null 2>&1 && ! "${XIN[@]}" windows >/dev/null 2>&1; then
            Xvfb "$DISPLAY" -screen 0 "$SCREEN" -nolisten tcp >"$LOGS/xvfb-setup.log" 2>&1 &
            echo $! >"$STATE/xvfb-setup.pid"; started=1; sleep 1
        fi
        wineboot -i >"$LOGS/wineboot.log" 2>&1 || die "wineboot failed (see $LOGS/wineboot.log)"
        wineserver -w
        if (( started )); then kill "$(cat "$STATE/xvfb-setup.pid")" 2>/dev/null || true; rm -f "$STATE/xvfb-setup.pid"; fi
        echo "created prefix $WINEPREFIX"
    fi
    ln -sfn "$GAME" "$WINEPREFIX/drive_c/YOOT"
    local sys=$WINEPREFIX/drive_c/windows/syswow64
    [[ -d $sys ]] || sys=$WINEPREFIX/drive_c/windows/system32
    if [[ ! -f $sys/mfc42.dll ]]; then
        local m; m=$(find_mfc42) || die "MFC42.DLL (VC6 redist) not found locally; set MFC42_DLL=/path/mfc42.dll or run: WINEPREFIX=$WINEPREFIX winetricks mfc42"
        cp "$m" "$sys/mfc42.dll"; echo "installed mfc42.dll from $m"
    fi
    # Suppress the "Please set the display mode to 256 colors" dialog.
    wine reg add 'HKCU\Software\OPeNBooK9003\YOOT Tower\Settings' /v CheckDisplay /t REG_DWORD /d 0 /f >/dev/null 2>&1
    wineserver -w
    echo "setup ok: C:\\YOOT -> $GAME"
}

to_winpath() {
    local s=$1
    if [[ $s == *\\* || $s == [A-Za-z]:* ]]; then echo "$s"
    elif [[ -f $GAME/$s ]]; then echo "C:\\YOOT\\$s"
    elif [[ -f $s ]]; then winepath -w "$(realpath "$s")" 2>/dev/null
    else die "save not found: $s"; fi
}

cmd_start() {
    [[ -f $WINEPREFIX/system.reg && -f $GAME/T2.exe ]] || cmd_setup
    alive "$STATE/wine.pid" && die "already running (run_yoot.sh stop first)"
    if ! alive "$STATE/xvfb.pid"; then
        Xvfb "$DISPLAY" -screen 0 "$SCREEN" -nolisten tcp >"$LOGS/xvfb.log" 2>&1 &
        echo $! >"$STATE/xvfb.pid"
        for _ in $(seq 50); do "${XIN[@]}" windows >/dev/null 2>&1 && break; sleep 0.1; done
    fi
    local args=() save=""
    if [[ $# -ge 1 && -n $1 ]]; then save=$(to_winpath "$1"); args=("$save"); fi
    # NOTE: no `explorer /desktop=` virtual desktop: in Wine 11 the title dialog never paints there.
    (cd "$GAME" && exec wine 'C:\YOOT\T2.exe' "${args[@]}") >"$LOGS/game.log" 2>&1 &
    echo $! >"$STATE/wine.pid"
    echo "$save" >"$STATE/save"
    if [[ -n $save ]]; then
        local name=${save##*\\}
        cmd_wait_window "^${name//./\\.} - T2\$" 90 >/dev/null || die "save window did not appear (see $LOGS/game.log)"
        cmd_wait_window '^Control$' 30 >/dev/null || true
        cmd_wait_gone '^Loading$' 90 >/dev/null || true
        sleep 1
        echo "loaded $save"
    else
        cmd_wait_window '^YOOT TOWER$' 60 >/dev/null || die "title dialog did not appear"
        echo "started (title screen; intro may still be playing - click to skip)"
    fi
}

cmd_wait_window() {
    local re=$1 t=${2:-60} end=$((SECONDS + ${2:-60}))
    while (( SECONDS < end )); do
        if "${XIN[@]}" windows 2>/dev/null | sed -E 's/^\S+\s+\S+\s+\S+ ?//' | grep -Eq -- "$re"; then
            echo "found: $re"; return 0
        fi
        sleep 0.5
    done
    echo "timeout (${t}s) waiting for window: $re" >&2; return 1
}

cmd_wait_gone() {
    local re=$1 end=$((SECONDS + ${2:-60}))
    while (( SECONDS < end )); do
        "${XIN[@]}" windows 2>/dev/null | sed -E 's/^\S+\s+\S+\s+\S+ ?//' | grep -Eq -- "$re" || { echo "gone: $re"; return 0; }
        sleep 0.5
    done
    echo "timeout waiting for window to close: $re" >&2; return 1
}

cmd_stop() {
    if [[ -f $STATE/wine.pid ]]; then kill "$(cat "$STATE/wine.pid")" 2>/dev/null || true; fi
    wineserver -k 2>/dev/null || true      # only kills this WINEPREFIX's wineserver + its processes
    sleep 1
    if [[ -f $STATE/xvfb.pid ]]; then kill "$(cat "$STATE/xvfb.pid")" 2>/dev/null || true; fi
    rm -f "$STATE/wine.pid" "$STATE/xvfb.pid" "$STATE/save"
    echo "stopped"
}

cmd_status() {
    alive "$STATE/xvfb.pid" && echo "xvfb: running pid $(cat "$STATE/xvfb.pid") on $DISPLAY" || echo "xvfb: stopped"
    alive "$STATE/wine.pid" && echo "wine: running pid $(cat "$STATE/wine.pid") save=$(cat "$STATE/save" 2>/dev/null)" || echo "wine: stopped"
    alive "$STATE/xvfb.pid" && "${XIN[@]}" windows || true
}

need_x() { alive "$STATE/xvfb.pid" || die "not running (run_yoot.sh start)"; }

cmd=${1:-help}; shift || true
case $cmd in
    setup) cmd_setup ;;
    start) cmd_start "$@" ;;
    stop) cmd_stop ;;
    status) cmd_status ;;
    log) cat "$LOGS/game.log" ;;
    wait-window) need_x; cmd_wait_window "$@" ;;
    wait-gone) need_x; cmd_wait_gone "$@" ;;
    shot) need_x; [[ $# -ge 1 ]] || die "usage: shot <out.png>"; mkdir -p "$(dirname "$1")"; "${XIN[@]}" shot "$1"; echo "$1" ;;
    click|dclick|rclick|move|drag|key|type|windows) need_x; "${XIN[@]}" "$cmd" "$@" ;;
    *) sed -n '2,22p' "$0"; exit 2 ;;
esac
