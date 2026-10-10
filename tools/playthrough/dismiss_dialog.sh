#!/usr/bin/env bash
# Click the modal dialog's "Close" button of the C++ GUI (Financial Report / rating dialogs
# pause the sim) when, and only when, the button is on screen: sample the pixel at (610,566);
# the light-grey button face is >= #D0D0D0, whereas a blind click would inspect an elevator.
px=$(import -window root -crop 1x1+610+566 txt:- 2>/dev/null | tail -1 | grep -o '#[0-9A-Fa-f]\{6\}' | head -1)
r=$((16#${px:1:2})); g=$((16#${px:3:2})); b=$((16#${px:5:2}))
if [ -n "$px" ] && [ "$r" -ge 208 ] && [ "$g" -ge 208 ] && [ "$b" -ge 208 ]; then
  xdotool mousemove 642 573 click 1
fi
exit 0
