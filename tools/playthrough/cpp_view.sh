#!/usr/bin/env bash
# C++ GUI camera helper (OpenSkyscraper, 1280x800 Xvfb). Uses the game's own navigation instead of
# wheel scrolling (56 wheel clicks = 56*40*zoom px never reached the lobby):
#   * Game::handleEvent: Up / Down move poi.y by 20 px per key press; the constraint at the end of
#     Game::update clamps poi.y at the world bottom (-360 + half view height, i.e. basement B10),
#   * MapWindow click-to-jump (Game::centerViewportOnTile) centres poi.x on the tower; the Map window
#     sits at top right (tower silhouette centre ~ (1170, 322)).
# so "bottom clamp, then N Up presses" reaches a repeatable row: lobby = 13 presses above the clamp
# (ground lobby row + hotel floors above it), hotel floors = 172, roof / cathedral = 193 (SCHMITT.TDT).
#
# Usage (inside the opensky-play container):
#   cpp_view.sh goto <lobby|floors|cathedral>        # needs $DISPLAY with OpenSkyscraper running
#   cpp_view.sh <NAME> <TOWER> <OUT_DIR>              # launch the GUI, capture the three views as view_*.png
UP_LOBBY=${UP_LOBBY:-13}; UP_FLOORS=${UP_FLOORS:-172}; UP_CATHEDRAL=${UP_CATHEDRAL:-193}

goto_view() {
  case "$1" in
    lobby) n=$UP_LOBBY ;; floors) n=$UP_FLOORS ;; cathedral) n=$UP_CATHEDRAL ;;
    *) echo "unknown view $1" >&2; return 1 ;;
  esac
  xdotool mousemove 1170 322 click 1        # minimap: centre the camera horizontally on the tower
  xdotool mousemove 640 785                # park the pointer on the black strip below the window (no hover box)
  xdotool key --repeat 300 --delay 3 Down   # clamp at the bottom of the world
  sleep 0.5
  xdotool key --repeat "$n" --delay 12 Up
  sleep 1
}

if [ "$1" = "goto" ]; then goto_view "$2"; exit $?; fi

name="$1"; tower="$2"; out="$3"
mkdir -p "$out"
export -f goto_view
export UP_LOBBY UP_FLOORS UP_CATHEDRAL
xvfb-run -a -s "-screen 0 1280x800x24" bash -c "
  /lab/src/$name/build/OpenSkyscraper --load-game '$tower' >'$out/gui.log' 2>&1 &
  PID=\$!
  sleep 7
  for v in lobby floors cathedral; do
    goto_view \$v
    import -window root '$out/view_'\$v'.png'
  done
  kill \$PID
"
