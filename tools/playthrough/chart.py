#!/usr/bin/env python3
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

def render_chart(title, out_png, series_dict, max_hours=73):
    width, height = 1000, 520
    margin_l, margin_r = 90, 50
    margin_t, margin_b = 60, 60
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b

    im = Image.new("RGB", (width, height), (255, 255, 255))
    draw = ImageDraw.Draw(im)

    # Determine Y bounds across all series
    all_vals = []
    for s in series_dict.values():
        for r in s[:max_hours]:
            all_vals.append(r["population"])

    if not all_vals:
        return

    min_y = min(all_vals)
    max_y = max(all_vals)
    # Add 5% padding
    y_range = max(1, max_y - min_y)
    y_min_plot = max(0, min_y - int(y_range * 0.08))
    y_max_plot = max_y + int(y_range * 0.08)

    # Grid lines & day separators
    # Horizontal grid lines (5 steps)
    for i in range(6):
        val = y_min_plot + i * (y_max_plot - y_min_plot) / 5.0
        y = margin_t + plot_h - int(i * plot_h / 5.0)
        draw.line([(margin_l, y), (margin_l + plot_w, y)], fill=(230, 230, 230), width=1)
        draw.text((margin_l - 70, y - 7), f"{int(val):,}", fill=(100, 100, 100))

    # Day vertical lines (hours 0, 24, 48, 72)
    day_ticks = [0, 24, 48, 72]
    day_names = ["Day 1 Start", "Day 2 Start", "Day 3 Start", "Day 3 End"]
    for dt, dn in zip(day_ticks, day_names):
        x = margin_l + int(dt * plot_w / (max_hours - 1))
        draw.line([(x, margin_t), (x, margin_t + plot_h)], fill=(200, 200, 200), width=2)
        draw.text((x - 25, margin_t + plot_h + 10), f"h{dt}", fill=(100, 100, 100))
        draw.text((x - 30, margin_t + plot_h + 25), dn, fill=(70, 70, 70))

    # Additional hour ticks every 6 hours
    for h in range(0, max_hours, 6):
        if h in day_ticks: continue
        x = margin_l + int(h * plot_w / (max_hours - 1))
        draw.line([(x, margin_t), (x, margin_t + plot_h)], fill=(245, 245, 245), width=1)
        draw.text((x - 10, margin_t + plot_h + 10), f"{h%24}h", fill=(160, 160, 160))

    # Plot lines
    colors = {
        "Oracle (Wine SimTower)": (37, 99, 235),      # Blue
        "C++ Port (OpenSkyscraper)": (5, 150, 105),   # Emerald green
        "Web Port (OpenSky)": (217, 119, 6),          # Amber
    }

    def to_coords(h_idx, pop):
        x = margin_l + int(h_idx * plot_w / (max_hours - 1))
        norm_y = (pop - y_min_plot) / float(y_max_plot - y_min_plot)
        y = margin_t + plot_h - int(norm_y * plot_h)
        return (x, y)

    for name, series in series_dict.items():
        color = colors.get(name, (0, 0, 0))
        pts = []
        for r in series[:max_hours]:
            h_idx = r["hour_idx"]
            pop = r["population"]
            pts.append(to_coords(h_idx, pop))

        for j in range(len(pts) - 1):
            draw.line([pts[j], pts[j+1]], fill=color, width=3)
        for pt in pts[::6]: # dots every 6 hours
            draw.ellipse([pt[0]-3, pt[1]-3, pt[0]+3, pt[1]+3], fill=color)

    # Title & Legend
    draw.text((margin_l, 20), title, fill=(20, 20, 20))

    leg_x = width - margin_r - 280
    leg_y = 20
    for name, color in colors.items():
        if name in series_dict:
            draw.line([(leg_x, leg_y + 6), (leg_x + 25, leg_y + 6)], fill=color, width=3)
            draw.ellipse([leg_x + 9, leg_y + 3, leg_x + 15, leg_y + 9], fill=color)
            draw.text((leg_x + 35, leg_y), name, fill=(50, 50, 50))
            leg_y += 18

    # Border
    draw.rectangle([margin_l, margin_t, margin_l + plot_w, margin_t + plot_h], outline=(180, 180, 180), width=1)

    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    im.save(out_png)
    print(f"Saved chart to {out_png}")

if __name__ == "__main__":
    tower = sys.argv[1]
    base_dir = "/home/tim/Projects/OpenSky-art-v2/tools/playthrough/out/compare/hourly"
    o = json.load(open(f"{base_dir}/{tower}_oracle_hourly.json"))
    c = json.load(open(f"{base_dir}/{tower}_cpp_hourly.json"))
    w = json.load(open(f"{base_dir}/{tower}_web_hourly.json"))
    title = f"{tower} — Hourly Population Over 3 Days (Oracle vs C++ vs Web)"
    render_chart(title, f"{base_dir}/{tower}_population_hourly.png", {
        "Oracle (Wine SimTower)": o,
        "C++ Port (OpenSkyscraper)": c,
        "Web Port (OpenSky)": w,
    })
