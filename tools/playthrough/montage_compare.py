#!/usr/bin/env python3
"""Create side-by-side visual montages of Oracle, Web, and C++ playthrough screenshots."""

import os
import sys
from PIL import Image, ImageDraw, ImageFont

def make_montage(oracle_path, web_path, cpp_path, out_path, title):
    shots = [
        ("SimTower original under Wine (oracle)", oracle_path),
        ("OpenSkyWeb art branch", web_path),
        ("OpenSkyscraper C++ art branch", cpp_path)
    ]
    
    target_w = 640
    target_h = 480
    header_h = 40
    
    total_w = target_w * 3 + 40
    total_h = target_h + header_h + 30
    
    canvas = Image.new("RGB", (total_w, total_h), (24, 26, 30))
    draw = ImageDraw.Draw(canvas)
    
    for i, (label, p) in enumerate(shots):
        x_off = 10 + i * (target_w + 10)
        y_off = header_h + 10
        
        # Draw header label
        draw.text((x_off + 10, 12), label, fill=(240, 240, 240))
        
        if os.path.exists(p):
            try:
                img = Image.open(p).convert("RGB")
                img_resized = img.resize((target_w, round(target_w * img.height / img.width)), Image.Resampling.LANCZOS)
                canvas.paste(img_resized, (x_off, y_off))
            except Exception as e:
                draw.text((x_off + 20, y_off + 100), f"Error loading image:\n{e}", fill=(255, 80, 80))
        else:
            draw.rectangle([x_off, y_off, x_off + target_w, y_off + target_h], fill=(40, 44, 52))
            draw.text((x_off + 50, y_off + 200), f"Image not found:\n{os.path.basename(p)}", fill=(180, 180, 180))
            
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    canvas.save(out_path, optimize=True)
    print(f"[montage] Created {out_path} ({total_w}x{total_h})")

def main():
    """usage: montage_compare.py [oracle_dir web_dir cpp_dir]  (defaults: the pulled out/ dirs;
    pass the full-resolution dirs for sharper montages)."""
    base_dir = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(base_dir, "out", "compare")
    os.makedirs(out_dir, exist_ok=True)
    o_dir, w_dir, c_dir = (sys.argv[1:4] if len(sys.argv) >= 4 else
                           [os.path.join(base_dir, "out", p) for p in ("oracle32/SCHMITT", "web", "art-shots")])
    
    hours = ["08", "12", "18", "23"]
    for hr in hours:
        oracle_shot = os.path.join(o_dir, f"shot_d1_h{hr}.png")
        web_shot = os.path.join(w_dir, f"shot_d1_h{hr}.png")
        cpp_shot = os.path.join(c_dir, f"shot_d1_h{hr}.png")
        out_shot = os.path.join(out_dir, f"compare_d1_h{hr}.png")
        make_montage(oracle_shot, web_shot, cpp_shot, out_shot, f"Day 1 Hour {hr} Parity Comparison")

if __name__ == "__main__":
    main()
