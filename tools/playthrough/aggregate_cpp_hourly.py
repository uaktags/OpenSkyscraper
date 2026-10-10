#!/usr/bin/env python3
import glob, json, os, sys

def aggregate(dumps_dir, out_json, init_funds=278104600, init_pop=5807, init_countt=None):
    dumps = sorted(glob.glob(os.path.join(dumps_dir, "tick_*.json")))
    if not dumps:
        print(f"Error: no tick_*.json dumps in {dumps_dir}")
        return False

    records = []
    # Hour 0: initial state at load
    records.append({
        "hour_idx": 0,
        "day": 0,
        "hour": 0,
        "funds": init_funds,
        "population": init_pop,
        "countT": init_countt or [3798, 0, 0, 0, 1170, 430, 105, 0, 160, 144],
        "time": {"hour": 15.385, "day": 2, "quarter": 4, "year": 22}
    })

    # Hours 1..72:
    for idx, d_file in enumerate(dumps, start=1):
        if idx > 72:
            break
        d_data = json.load(open(d_file))
        records.append({
            "hour_idx": idx,
            "day": ((idx - 1) // 24) + 1,
            "hour": (idx - 1) % 24,
            "funds": d_data.get("funds", 0),
            "population": d_data.get("population", 0),
            "countT": d_data.get("countT", [0]*10),
            "time": d_data.get("time", {})
        })

    os.makedirs(os.path.dirname(out_json), exist_ok=True)
    with open(out_json, "w") as f:
        json.dump(records, f, indent=2)
    print(f"Aggregated {len(records)} records (hours 0..{len(records)-1}) into {out_json}")
    return True

if __name__ == "__main__":
    dumps_dir = sys.argv[1]
    out_json = sys.argv[2]
    aggregate(dumps_dir, out_json)
