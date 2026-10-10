#!/usr/bin/env python3
"""Collect the hourly SimHarness dumps of a headless C++ run into hourly.json.

Input:  <dir>/tick_<n>.json written by `--sim 3d --sim-dump-every 40 <dir>`
Output: <dir>/../hourly.json  [{tick, absolute, hour, day, quarter, year, funds, population, rating(stars)}]
Metric method for the C++ headless source: values come straight from the summary JSON
(funds/population are exact sim state, rating is 0-based in the JSON so stars = rating+1).
Usage: cpp_hourly.py <dump_dir> <out_json>
"""
import sys, os, re, json, glob
rows = []
for f in glob.glob(os.path.join(sys.argv[1], "tick_*.json")):
    m = re.search(r"tick_(\d+)\.json$", f)
    s = json.load(open(f))
    t = s["time"]
    rows.append({"tick": int(m.group(1)), "absolute": t["absolute"], "hour": t["hour"], "day": t["day"],
                 "quarter": t["quarter"], "year": t["year"], "funds": s["funds"],
                 "population": s["population"], "stars": s["rating"] + 1})
rows.sort(key=lambda r: r["tick"])
json.dump(rows, open(sys.argv[2], "w"), indent=1)
print(len(rows), "hourly rows ->", sys.argv[2])
