#!/usr/bin/env python3
import glob, json, os, sys, re
from PIL import Image

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from ocr import REGIONS, read_region

def clean_int(s):
    # remove $, commas, non-digits
    digits = re.sub(r"[^\d]", "", s)
    return int(digits) if digits else 0

def process_tower(tower, shots_dir, tmpl, out_json):
    shots = sorted(glob.glob(os.path.join(shots_dir, "shot_*.png")))
    if not shots:
        print(f"Error: no shots found in {shots_dir}")
        return False

    records = []
    for s in shots:
        im = Image.open(s).convert("RGB")
        f_txt, f_dist = read_region("oracle", "fund", im, tmpl)
        p_txt, p_dist = read_region("oracle", "pop", im, tmpl)
        d_txt, d_dist = read_region("oracle", "date", im, tmpl)
        records.append({
            "file": os.path.basename(s),
            "fund_raw": f_txt,
            "pop_raw": p_txt,
            "date": d_txt,
            "funds": clean_int(f_txt),
            "population": clean_int(p_txt),
            "worst_dist": max(f_dist, p_dist, d_dist)
        })

    # Group into date clusters
    clusters = []
    for r in records:
        if not clusters or clusters[-1]["date"] != r["date"]:
            clusters.append({"date": r["date"], "shots": [r]})
        else:
            clusters[-1]["shots"].append(r)

    print(f"[{tower}] Total shots: {len(records)}, Clusters: {len(clusters)}")
    for idx, c in enumerate(clusters):
        print(f"  Cluster {idx} ({c['date']}): {len(c['shots'])} shots")

    # Tower specific days mapping:
    # In SCHMITT:
    #   Cluster 0: WE/2Q/13th Year (partial initial day, 19 shots)
    #   Cluster 1: 1st WD/3Q/13th Year (Day 1, 27 shots)
    #   Cluster 2: 2nd WD/3Q/13th Year (Day 2, 28 shots)
    #   Cluster 3: WE/3Q/13th Year (Day 3, 11 shots)
    # In BARKLE4D:
    #   Cluster 0: WE/4Q/22nd Year (partial initial day, 2 shots)
    #   Cluster 1: 1st WD/1Q/23rd Year (Day 1, 11 shots)
    #   Cluster 2: 2nd WD/1Q/23rd Year (Day 2, 26 shots)
    #   Cluster 3: WE/1Q/23rd Year (Day 3, 21 shots)

    hourly_series = []

    # Hour 0: initial frame
    first_shot = records[0]
    hourly_series.append({
        "hour_idx": 0,
        "day": 0,
        "hour": 0,
        "date": first_shot["date"],
        "funds": first_shot["funds"],
        "population": first_shot["population"],
        "shot": first_shot["file"],
        "worst_dist": first_shot["worst_dist"]
    })

    # Days 1, 2, 3:
    day_clusters = clusters[1:4]
    global_hour = 1
    for day_idx, c in enumerate(day_clusters, start=1):
        c_shots = c["shots"]
        n = len(c_shots)
        for h in range(24):
            # map in-game hour h (0..23) to shot index
            if n == 1:
                shot_idx = 0
            else:
                shot_idx = int(round(h * (n - 1) / 23.0))
            shot = c_shots[shot_idx]
            hourly_series.append({
                "hour_idx": global_hour,
                "day": day_idx,
                "hour": h,
                "date": shot["date"],
                "funds": shot["funds"],
                "population": shot["population"],
                "shot": shot["file"],
                "worst_dist": shot["worst_dist"]
            })
            global_hour += 1

    os.makedirs(os.path.dirname(out_json), exist_ok=True)
    with open(out_json, "w") as f:
        json.dump(hourly_series, f, indent=2)
    print(f"[{tower}] Wrote {len(hourly_series)} hourly records to {out_json}")
    return True

if __name__ == "__main__":
    tmpl_path = sys.argv[1]
    tmpl = json.load(open(tmpl_path))
    tower = sys.argv[2]
    shots_dir = sys.argv[3]
    out_json = sys.argv[4]
    process_tower(tower, shots_dir, tmpl, out_json)
