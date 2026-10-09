#!/usr/bin/env python3
"""Extract playthrough metrics from a .tower XML save and optional SimHarness summary JSON.

Usage:
    python3 parse_tower_metrics.py <tower_xml_path> [summary_json_path] [day_number]
"""

import sys
import os
import json
import xml.etree.ElementTree as ET

def parse_metrics(tower_path, summary_path=None, day=1):
    metrics = {
        "day": day,
        "funds": 0,
        "population": 0,
        "rating": 1,
        "stressed_people": 0,
        "elevator_avg_wait": 0.0,
        "items": {}
    }

    # Load summary JSON if available
    if summary_path and os.path.exists(summary_path):
        try:
            with open(summary_path, "r", encoding="utf-8") as f:
                s = json.load(f)
                metrics["funds"] = s.get("funds", 0)
                metrics["population"] = s.get("population", 0)
                metrics["rating"] = s.get("rating", 1)
                metrics["items"] = s.get("items", {})
        except Exception as e:
            pass

    # Parse .tower XML
    if os.path.exists(tower_path):
        try:
            tree = ET.parse(tower_path)
            root = tree.getroot()
            if root.tag == "tower":
                if not summary_path or not os.path.exists(summary_path):
                    metrics["funds"] = int(root.get("funds", metrics["funds"]))
                    metrics["population"] = int(root.get("population", metrics["population"]))
                    metrics["rating"] = int(root.get("rating", 0)) + 1

                # Count items if not in summary
                if not metrics["items"]:
                    items = {}
                    for item in root.findall(".//item"):
                        itype = item.get("type", "unknown")
                        items[itype] = items.get(itype, 0) + 1
                    metrics["items"] = items

                # Count stressed people
                # Look for all elements with a 'stress' attribute > 0
                stressed = 0
                for elem in root.iter():
                    if "stress" in elem.attrib:
                        try:
                            val = float(elem.attrib["stress"])
                            if val > 0:
                                stressed += 1
                        except ValueError:
                            pass
                metrics["stressed_people"] = stressed
        except Exception as e:
            pass

    return metrics

def main():
    if len(sys.argv) < 2:
        print("Usage: parse_tower_metrics.py <tower_xml_path> [summary_json_path] [day_number]")
        sys.exit(1)

    tower_path = sys.argv[1]
    summary_path = sys.argv[2] if len(sys.argv) > 2 else None
    day = int(sys.argv[3]) if len(sys.argv) > 3 else 1

    m = parse_metrics(tower_path, summary_path, day)
    print(json.dumps(m, indent=2))

if __name__ == "__main__":
    main()
