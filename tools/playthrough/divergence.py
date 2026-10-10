import json

cat_names = [
    "Offices", "Hotel Single", "Hotel Twin", "Hotel Suite",
    "Shops", "Fast Food", "Restaurants", "Party Hall", "Cinema", "Condos"
]

def analyze_tower(tower):
    base = "/home/tim/Projects/OpenSky-art-v2/tools/playthrough/out/compare/hourly"
    o = json.load(open(f"{base}/{tower}_oracle_hourly.json"))
    c = json.load(open(f"{base}/{tower}_cpp_hourly.json"))
    w = json.load(open(f"{base}/{tower}_web_hourly.json"))

    print("\n=======================================================")
    print("               %s DIVERGENCE ANALYSIS" % tower)
    print("=======================================================")

    # First hour > 2% for C++
    cpp_div = None
    web_div = None
    for i in range(len(o)):
        op = o[i]["population"]
        cp = c[i]["population"]
        wp = w[i]["population"]
        d_cp = abs(cp - op) / op if op else 0
        d_wp = abs(wp - op) / op if op else 0

        if cpp_div is None and d_cp > 0.02:
            cpp_div = (i, o[i], c[i], d_cp)
        if web_div is None and d_wp > 0.02:
            web_div = (i, o[i], w[i], d_wp)

    print("--- C++ Divergence ---")
    if cpp_div:
        idx, od, cd, pct = cpp_div
        print("First hour |C++ - Oracle| > 2%%: Hour %d (Day %d, %02d:00)" % (idx, od["day"], od["hour"]))
        print("  Oracle Pop: %d | C++ Pop: %d (delta: %.2f%%)" % (od["population"], cd["population"], pct*100))
        print("  Oracle Date: %s (shot: %s)" % (od["date"], od["shot"]))
        print("  C++ countT: %s" % str(cd.get("countT")))

        # Check movement around that hour
        candidates = []
        if idx > 0:
            c_prev = c[idx-1].get("countT", [0]*10)
            c_curr = cd.get("countT", [0]*10)
            diffs = [abs(c_curr[k] - c_prev[k]) for k in range(10)]
            max_move = max(diffs)
            max_idx = diffs.index(max_move)
            candidates.append((diffs[max_idx], cat_names[max_idx], c_prev[max_idx], c_curr[max_idx], "h%d->h%d" % (idx-1, idx)))
        if idx + 1 < len(c):
            c_curr = cd.get("countT", [0]*10)
            c_next = c[idx+1].get("countT", [0]*10)
            diffs = [abs(c_next[k] - c_curr[k]) for k in range(10)]
            max_move = max(diffs)
            max_idx = diffs.index(max_move)
            candidates.append((diffs[max_idx], cat_names[max_idx], c_curr[max_idx], c_next[max_idx], "h%d->h%d" % (idx, idx+1)))

        print("  Movement around divergence hour:")
        for move, cat, v1, v2, step in candidates:
            print("    [%s] Most moving category: %s (diff: %+d, %d -> %d)" % (step, cat, move, v1, v2))

    # Also check post-hour-0 divergence if hour 0 was diverged
    if cpp_div and cpp_div[0] == 0:
        cpp_div_post0 = None
        for i in range(1, len(o)):
            op = o[i]["population"]
            cp = c[i]["population"]
            d_cp = abs(cp - op) / op if op else 0
            if cpp_div_post0 is None and d_cp > 0.02:
                cpp_div_post0 = (i, o[i], c[i], d_cp)
        if cpp_div_post0:
            idx, od, cd, pct = cpp_div_post0
            print("\n  [Post-Hour 0 Sim Divergence] First hour: Hour %d (Day %d, %02d:00)" % (idx, od["day"], od["hour"]))
            print("    Oracle Pop: %d | C++ Pop: %d (delta: %.2f%%)" % (od["population"], cd["population"], pct*100))
            c_prev = c[idx-1].get("countT", [0]*10)
            c_curr = cd.get("countT", [0]*10)
            diffs = [abs(c_curr[k] - c_prev[k]) for k in range(10)]
            max_move = max(diffs)
            max_idx = diffs.index(max_move)
            print("    Most moving category around h%d: %s (%+d pop, %d -> %d)" % (idx, cat_names[max_idx], c_curr[max_idx] - c_prev[max_idx], c_prev[max_idx], c_curr[max_idx]))
            for k in range(10):
                if c_curr[k] != c_prev[k]:
                    print("      %s: %d -> %d (diff: %+d)" % (cat_names[k], c_prev[k], c_curr[k], c_curr[k] - c_prev[k]))

    print("\n--- Web Divergence ---")
    if web_div:
        idx, od, wd, pct = web_div
        print("First hour |Web - Oracle| > 2%%: Hour %d (Day %d, %02d:00)" % (idx, od["day"], od["hour"]))
        print("  Oracle Pop: %d | Web Pop: %d (delta: %.2f%%)" % (od["population"], wd["population"], pct*100))
        if idx == 0:
            for i in range(1, len(o)):
                op = o[i]["population"]
                wp = w[i]["population"]
                d_wp = abs(wp - op) / op if op else 0
                if d_wp > 0.02:
                    print("  [Post-Hour 0 Sim Divergence] First hour: Hour %d (Day %d, %02d:00)" % (i, o[i]["day"], o[i]["hour"]))
                    print("    Oracle Pop: %d | Web Pop: %d (delta: %.2f%%)" % (o[i]["population"], w[i]["population"], d_wp*100))
                    break

analyze_tower("SCHMITT")
analyze_tower("BARKLE4D")
