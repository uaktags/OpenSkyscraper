#!/usr/bin/env python3
"""Compare two OpenSky metrics runs day-by-day and display deltas.

Usage:
    python3 compare.py <run_a_or_file> <run_b_or_file>
"""

import sys
import os
import json
import csv

def load_metrics(path):
    if not os.path.exists(path):
        out_cand = os.path.join(os.path.dirname(__file__), "out", path)
        if os.path.exists(out_cand):
            path = out_cand
    if os.path.isdir(path):
        json_path = os.path.join(path, "metrics.json")
        csv_path = os.path.join(path, "metrics.csv")
        if os.path.exists(json_path):
            path = json_path
        elif os.path.exists(csv_path):
            path = csv_path
        else:
            raise FileNotFoundError(f"No metrics.json or metrics.csv found in directory {path}")

    if not os.path.isfile(path):
        raise FileNotFoundError(f"Metrics file not found: {path}")

    if path.endswith(".json"):
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
            if isinstance(data, dict):
                # single day summary or object
                if "days" in data:
                    data = data["days"]
                elif "day" in data:
                    data = [data]
                else:
                    data = [data]
            return {int(row.get("day", i + 1)): row for i, row in enumerate(data)}
    elif path.endswith(".csv"):
        days = {}
        with open(path, "r", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for i, row in enumerate(reader):
                day = int(row.get("day", i + 1))
                days[day] = row
        return days
    else:
        raise ValueError(f"Unsupported file format: {path}")

def format_delta(v1, v2):
    try:
        n1 = float(v1)
        n2 = float(v2)
        diff = n2 - n1
        sign = "+" if diff > 0 else ""
        if isinstance(v1, int) and isinstance(v2, int):
            return f"{int(diff):+d}"
        return f"{sign}{diff:.2f}"
    except (ValueError, TypeError):
        return "=" if str(v1) == str(v2) else "diff"


CANONICAL_ITEMS = {
    "double hotel room": "hotel_double",
    "single hotel room": "hotel_single",
    "hotel suite": "hotel_suite",
    "english pub": "restaurant",
    "coffee shop": "fastfood",
    "chinese cafe": "fastfood",
    "hamburger stand": "fastfood",
    "ice cream": "fastfood",
    "japanese stall": "fastfood",
    "movie theatre": "cinema",
    "metro station": "metro",
    "party hall": "partyhall",
    "recycling center": "recycling",
    "medical center": "medicalcenter",
}

def canonicalize_item(name):
    norm = name.strip().lower().replace("_", " ")
    norm = CANONICAL_ITEMS.get(norm, norm)
    return norm.replace(" ", "_")

def compare_runs(path_a, path_b):
    name_a = os.path.basename(path_a.rstrip("/"))
    name_b = os.path.basename(path_b.rstrip("/"))
    metrics_a = load_metrics(path_a)
    metrics_b = load_metrics(path_b)

    all_days = sorted(set(metrics_a.keys()).union(metrics_b.keys()))
    if not all_days:
        print("No days found to compare.")
        return

    print("=" * 80)
    print(f"OpenSky Parity Run Comparison: {name_a} (A) vs {name_b} (B)")
    print("=" * 80)

    for day in all_days:
        print(f"\n--- Day {day} ---")
        a = metrics_a.get(day)
        b = metrics_b.get(day)
        if a is None:
            print(f"  Only present in B ({name_b})")
            continue
        if b is None:
            print(f"  Only present in A ({name_a})")
            continue

        core_fields = ["funds", "population", "rating", "stressed_people", "elevator_avg_wait"]
        print(f"  {'Metric':<22} | {'A (' + name_a + ')':<16} | {'B (' + name_b + ')':<16} | {'Delta (B - A)':<16}")
        print("  " + "-" * 76)

        for field in core_fields:
            va = a.get(field, "N/A")
            vb = b.get(field, "N/A")
            if va != "N/A" and vb != "N/A":
                delta = format_delta(va, vb)
            else:
                delta = "N/A"
            print(f"  {field:<22} | {str(va):<16} | {str(vb):<16} | {delta:<16}")

        # Item counts comparison
        items_a = a.get("items", {})
        items_b = b.get("items", {})
        if isinstance(items_a, str):
            try: items_a = json.loads(items_a)
            except Exception: items_a = {}
        if isinstance(items_b, str):
            try: items_b = json.loads(items_b)
            except Exception: items_b = {}

        canon_a = {}
        for k, v in items_a.items():
            ck = canonicalize_item(k)
            canon_a[ck] = canon_a.get(ck, 0) + int(v)
        canon_b = {}
        for k, v in items_b.items():
            ck = canonicalize_item(k)
            canon_b[ck] = canon_b.get(ck, 0) + int(v)
        items_a, items_b = canon_a, canon_b
        all_items = sorted(set(items_a.keys()).union(items_b.keys()))
        if all_items:
            print(f"\n  Item Breakdown (Day {day}):")
            print(f"    {'Item Type':<20} | {'A':<10} | {'B':<10} | {'Delta':<10}")
            print("    " + "-" * 56)
            for it in all_items:
                cnt_a = int(items_a.get(it, 0))
                cnt_b = int(items_b.get(it, 0))
                diff = cnt_b - cnt_a
                sign = "+" if diff > 0 else ""
                diff_str = f"{sign}{diff}" if diff != 0 else "="
                print(f"    {it:<20} | {cnt_a:<10} | {cnt_b:<10} | {diff_str:<10}")

    print("\n" + "=" * 80)

def main():
    if len(sys.argv) < 3:
        print("Usage: compare.py <run_a_or_metrics.json> <run_b_or_metrics.json>")
        sys.exit(1)
    compare_runs(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    main()
