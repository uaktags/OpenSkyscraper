#!/usr/bin/env bash
# Scroll the C++ GUI camera down towards the lobby/cathedral and capture stills (run inside the
# opensky-play container: bash /lab/tools/cpp_view.sh <NAME> <TOWER> <OUT_DIR>).
# Used to compare lobby / cathedral / people rendering, which the hourly shots (camera parked at
# the top of the tower) do not show.
name="$1"; tower="$2"; out="$3"
mkdir -p "$out"
xvfb-run -a -s "-screen 0 1280x800x24" bash -c "
  /lab/src/$name/build/OpenSkyscraper --load-game '$tower' >'$out/gui.log' 2>&1 &
  PID=\$!
  sleep 6
  xdotool mousemove 640 450
  for i in 0 1 2 3 4 5 6; do
    import -window root '$out/view_'\$i'.png'
    for k in 1 2 3 4 5 6 7 8; do xdotool click 5; done
    sleep 1
  done
  kill \$PID
"
