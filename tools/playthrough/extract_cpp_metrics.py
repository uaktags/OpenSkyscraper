#!/usr/bin/env python3
"""Metrics for the C++ GUI screenshot run: per-source digit-template OCR (see ocr.py).

Reads <out_dir>/shot_d<D>_h<HH>.png (1280x800 Xvfb frames of the OpenSkyscraper time window),
OCRs fund / population / date from every shot and the star count from the gold star strip,
and writes <out_dir>/ocr_hourly.json (every shot), metrics.json / metrics.csv (one row per
day, taken from the hour-23 shot) and crop evidence under <out_dir>/ocr/.
Usage: extract_cpp_metrics.py <out_dir> [days]
"""
import sys, os, json, csv
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ocr


def main():
    out_dir = sys.argv[1]
    days = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    rows = ocr.cmd_run("cpp", out_dir, os.path.join(out_dir, "ocr"), os.path.join(out_dir, "ocr_hourly.json"))
    by = {(r["day"], r["hour"]): r for r in rows}
    metrics = []
    for d in range(1, days + 1):
        r = by.get((d, 23))
        if r:
            metrics.append({"day": d, "funds": r["funds"], "population": r["population"], "rating": r["stars"],
                            "date": r["date"], "stressed_people": 0, "elevator_avg_wait": 0.0, "items": {}})
            print(f"[cpp-metrics] Day {d} (23:xx shot): funds={r['funds']} pop={r['population']} stars={r['stars']} date={r['date']}")
    json.dump(metrics, open(os.path.join(out_dir, "metrics.json"), "w"), indent=2)
    with open(os.path.join(out_dir, "metrics.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["day", "funds", "population", "stars", "date"])
        for m in metrics:
            w.writerow([m["day"], m["funds"], m["population"], m["rating"], m["date"]])


if __name__ == "__main__":
    main()
