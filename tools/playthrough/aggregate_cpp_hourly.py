#!/usr/bin/env python3
import glob, json, os, sys

INITIAL_STATES = {
    "SCHMITT": {
        "funds": 93560900,
        "population": 15401,
        "countT": [12210, 64, 348, 28, 480, 1961, 140, 50, 120, 0],
        "time": {"hour": 22.74, "day": 1, "quarter": 2, "year": 13, "absolute": 148.951538}
    },
    "BARKLE4D": {
        "funds": 278104600,
        "population": 5807,
        "countT": [3798, 0, 0, 0, 1170, 430, 105, 0, 160, 144],
        "time": {"hour": 15.385, "day": 2, "quarter": 4, "year": 22, "absolute": 2675.69762}
    }
}

def aggregate(dumps_dir, out_json, tower_name=None):
    if not tower_name:
        if "schmitt" in dumps_dir.lower() or "schmitt" in out_json.lower():
            tower_name = "SCHMITT"
        else:
            tower_name = "BARKLE4D"

    init = INITIAL_STATES.get(tower_name.upper(), INITIAL_STATES["BARKLE4D"])

    dumps = sorted(glob.glob(os.path.join(dumps_dir, "tick_*.json")))
    if not dumps:
        print(f"Error: no tick_*.json dumps in {dumps_dir}")
        return False

    records = []
    # Hour 0: initial state at load
    records.append({
        "hour_idx": 0,
        "tick": 0,
        "day": 0,
        "hour": 0,
        "funds": init["funds"],
        "population": init["population"],
        "countT": init["countT"],
        "time": init["time"]
    })

    # Hours 1..72:
    for idx, d_file in enumerate(dumps, start=1):
        if idx > 72:
            break
        d_data = json.load(open(d_file))
        records.append({
            "hour_idx": idx,
            "tick": d_data.get("ticks", idx * 40),
            "day": ((idx - 1) // 24) + 1,
            "hour": (idx - 1) % 24,
            "funds": d_data.get("funds", 0),
            "population": d_data.get("population", 0),
            "stars": d_data.get("rating", 0) + 1,
            "countT": d_data.get("countT", [0]*10),
            "retailPeriod": d_data.get("retailPeriod", 0),
            "time": d_data.get("time", {})
        })

    os.makedirs(os.path.dirname(out_json), exist_ok=True)
    with open(out_json, "w") as f:
        json.dump(records, f, indent=2)
    print(f"Aggregated {len(records)} records (hours 0..{len(records)-1}) into {out_json} for tower {tower_name}")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: aggregate_cpp_hourly.py <dumps_dir> <out_json> [tower_name]")
        sys.exit(1)
    dumps_dir = sys.argv[1]
    out_json = sys.argv[2]
    tower_name = sys.argv[3] if len(sys.argv) > 3 else None
    aggregate(dumps_dir, out_json, tower_name)
