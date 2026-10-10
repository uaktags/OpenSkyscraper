#!/usr/bin/env python3
"""Final parity montages: oracle | web | C++ per tower/view/hour -> out/compare/final/<TOWER>/<view>_hHH.png
usage: montage_final.py   (reads out/compare/final/raw/<TOWER>/{oracle,web,cpp})"""
import os
from PIL import Image, ImageDraw
B = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out", "compare", "final")
W, HEAD = 600, 24
def cell(canvas, d, label, p, x, y, crop=None):
    d.text((x + 4, y + 6), label, fill=(240, 240, 240))
    if os.path.exists(p):
        im = Image.open(p).convert("RGB")
        if crop: im = im.crop(crop)
        h = round(W * im.height / im.width)
        canvas.paste(im.resize((W, h), Image.Resampling.LANCZOS), (x, y + HEAD)); return h
    d.rectangle([x, y + HEAD, x + W, y + HEAD + 300], fill=(40, 44, 52))
    d.text((x + 20, y + HEAD + 140), "not found: " + os.path.basename(p), fill=(180, 180, 180)); return 300
def montage(paths, labels, out, crops=(None, None, None)):
    ims = []
    c = Image.new("RGB", (W * len(paths) + 10 * (len(paths) + 1), 900), (24, 26, 30)); d = ImageDraw.Draw(c); hmax = 0
    for i, (p, l, cr) in enumerate(zip(paths, labels, crops)):
        hmax = max(hmax, cell(c, d, l, p, 10 + i * (W + 10), 5, cr))
    c.crop((0, 0, c.width, hmax + HEAD + 10)).save(out, optimize=True); print(out)
for T in ("SCHMITT", "BARKLE4D"):
    r = os.path.join(B, "raw", T); o = os.path.join(B, T); os.makedirs(o, exist_ok=True)
    for hh in ("08", "12", "18", "23"):
        for v in ("lobby", "floors", "cathedral"):
            f = f"view_d1_h{hh}_{v}.png"
            montage([os.path.join(r, s, f) for s in ("oracle", "web", "cpp")],
                    [f"{T} oracle: {v} {hh}:00 (d1)", f"{T} web: {v} {hh}:00", f"{T} C++: {v} {hh}:00"], os.path.join(o, f"{v}_h{hh}.png"))
    # minimap at night (top-right of the 1280x800 window; oracle has none in this crop, whole HUD)
    f = "shot_d1_h23.png"
    montage([os.path.join(r, "web", f), os.path.join(r, "cpp", f)], [f"{T} web: minimap 23:00", f"{T} C++: minimap 23:00"],
            os.path.join(o, "minimap_night.png"), crops=((880, 0, 1280, 400), (880, 0, 1280, 400)))
    montage([os.path.join(r, "web", "menu_web.png"), os.path.join(r, "cpp", "menu_cpp.png")], [f"{T} web: main menu", f"{T} C++: main menu"], os.path.join(o, "menu.png"))
