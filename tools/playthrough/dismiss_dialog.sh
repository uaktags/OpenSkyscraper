#!/usr/bin/env bash
# Dismiss the modal tgui dialogs of the C++ GUI (1280x800 Xvfb) when, and only when, their button is on
# screen (a blind click would inspect an elevator / place a tool). They are in-window tgui ChildWindows,
# not X windows, so xdotool has no title to find: the geometry is fixed (window centred on 1280x800) and
# the button face is sampled first (light grey >= #D0D0D0).
#   1. Financial Report: "Close" button, face sampled at (610,566), click (642,573).
#   2. LevelUpDialog "Congratulations - your tower reached N stars!" (source/LevelUpDialog.cpp: 260x150
#      client, centred): "OK" button, face sampled at (612,455), click (640,461). It sits under the
#      report and is revealed once that closes (and re-appears whenever the rating promotes).
light() { # light X Y -> 0 when the pixel is >= #D0D0D0
  local px r g b
  px=$(import -window root -crop 1x1+$1+$2 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-Fa-f]\{6\}' | head -1)
  [ -n "$px" ] || return 1
  r=$((16#${px:1:2})); g=$((16#${px:3:2})); b=$((16#${px:5:2}))
  [ "$r" -ge 208 ] && [ "$g" -ge 208 ] && [ "$b" -ge 208 ]
}
for _ in 1 2 3; do
  if light 610 566; then xdotool mousemove 642 573 click 1; sleep 0.4; continue; fi
  if light 612 455; then xdotool mousemove 640 461 click 1; sleep 0.4; continue; fi
  break
done
exit 0
