#!/usr/bin/env python3
"""Minimal xdotool replacement built on python-xlib + XTEST.

Usage (DISPLAY must point at the oracle Xvfb, e.g. :87):
  xinput.py click X Y [button]       move + click (button 1=left, 3=right)
  xinput.py dclick X Y               double left click
  xinput.py move X Y
  xinput.py drag X1 Y1 X2 Y2         left-button drag
  xinput.py key KEYSPEC...           e.g. Return, Escape, alt+f, ctrl+o, F1
  xinput.py type "text"              type literal ASCII text
  xinput.py windows                  list top-level windows (id, geometry, name)
  xinput.py shot OUT.png             screenshot of the root window (needs Pillow)

Run with `python3 -I` so nothing is imported from the cwd.
"""
import sys
import time

from Xlib import X, XK, display
from Xlib.ext import xtest

D = display.Display()
ROOT = D.screen().root

MODS = {
    "ctrl": "Control_L", "control": "Control_L", "alt": "Alt_L",
    "shift": "Shift_L", "super": "Super_L",
}
ALIASES = {
    "enter": "Return", "esc": "Escape", "tab": "Tab", "space": "space",
    "backspace": "BackSpace", "del": "Delete", "delete": "Delete",
    "up": "Up", "down": "Down", "left": "Left", "right": "Right",
    "pageup": "Prior", "pagedown": "Next", "home": "Home", "end": "End",
}


def sync(delay=0.03):
    D.sync()
    time.sleep(delay)


def keycode(name):
    name = ALIASES.get(name.lower(), name)
    sym = XK.string_to_keysym(name)
    if sym == 0 and len(name) == 1:
        sym = ord(name)
    if sym == 0:
        raise SystemExit("unknown key: %s" % name)
    code = D.keysym_to_keycode(sym)
    if code == 0:
        raise SystemExit("no keycode for: %s" % name)
    return code


def move(x, y):
    xtest.fake_input(D, X.MotionNotify, x=int(x), y=int(y))
    sync()


def click(x, y, button=1, count=1):
    move(x, y)
    for _ in range(count):
        xtest.fake_input(D, X.ButtonPress, int(button))
        sync(0.04)
        xtest.fake_input(D, X.ButtonRelease, int(button))
        sync(0.06)


def drag(x1, y1, x2, y2):
    move(x1, y1)
    xtest.fake_input(D, X.ButtonPress, 1)
    sync(0.1)
    steps = 10
    for i in range(1, steps + 1):
        move(int(x1) + (int(x2) - int(x1)) * i // steps,
             int(y1) + (int(y2) - int(y1)) * i // steps)
    xtest.fake_input(D, X.ButtonRelease, 1)
    sync(0.1)


def key(spec):
    parts = spec.split("+")
    codes = [keycode(MODS.get(p.lower(), p)) for p in parts]
    for c in codes:
        xtest.fake_input(D, X.KeyPress, c)
        sync(0.02)
    for c in reversed(codes):
        xtest.fake_input(D, X.KeyRelease, c)
        sync(0.02)
    sync(0.05)


def type_text(text):
    shift = keycode("Shift_L")
    for ch in text:
        sym = XK.string_to_keysym(ch) or ord(ch)
        if ch == " ":
            sym = XK.string_to_keysym("space")
        elif ch == "\n":
            sym = XK.string_to_keysym("Return")
        code = D.keysym_to_keycode(sym)
        if code == 0:
            raise SystemExit("cannot type %r" % ch)
        needs_shift = D.keycode_to_keysym(code, 0) != sym
        if needs_shift:
            xtest.fake_input(D, X.KeyPress, shift)
        xtest.fake_input(D, X.KeyPress, code)
        xtest.fake_input(D, X.KeyRelease, code)
        if needs_shift:
            xtest.fake_input(D, X.KeyRelease, shift)
        sync(0.02)


def windows():
    def walk(w, depth):
        try:
            name = w.get_wm_name()
            attrs = w.get_attributes()
            g = w.get_geometry()
        except Exception:
            return
        if attrs.map_state == X.IsViewable and (name or depth <= 1):
            t = w.translate_coords(ROOT, 0, 0)
            print("0x%08x %4dx%-4d +%d+%d %s" % (w.id, g.width, g.height,
                                                -t.x, -t.y, name or ""))
        if depth < 3:
            for c in w.query_tree().children:
                walk(c, depth + 1)
    for c in ROOT.query_tree().children:
        walk(c, 1)


def shot(path):
    from PIL import Image
    g = ROOT.get_geometry()
    raw = ROOT.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
    img = Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
    img.save(path, format="PNG")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd, args = argv[1], argv[2:]
    if cmd == "click":
        click(args[0], args[1], int(args[2]) if len(args) > 2 else 1)
    elif cmd == "dclick":
        click(args[0], args[1], 1, 2)
    elif cmd == "rclick":
        click(args[0], args[1], 3)
    elif cmd == "move":
        move(args[0], args[1])
    elif cmd == "drag":
        drag(*args[:4])
    elif cmd == "key":
        for k in args:
            key(k)
    elif cmd == "type":
        type_text(" ".join(args))
    elif cmd == "windows":
        windows()
    elif cmd == "shot":
        shot(args[0])
    else:
        print(__doc__)
        return 2
    D.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
