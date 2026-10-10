#!/usr/bin/env python3 -I
"""WP15 collector: reads out/web_seed*.json (web_yoot_sim.mjs) and out/cpp/<fx>/s<N>/ (run_cpp.sh pull)
and writes out/metrics.json: per fixture, per seed, per day (D1..D3 = the first three game days after load)."""
import glob, json, os, re, statistics, sys
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")
FIX = ["tokyo_g2_hotel_subway", "tokyo_g4_blackout", "tokyo_g5_large_elevator", "hawaii_g2_resort", "tokyo_g3_venues"]
TOWERS = os.path.join(HERE, "../../../tests/parity")
AGG = {"canHaulPeople", "elevator"}  # C++ aggregate counters, not items
ALIAS_BACK = {"yoot_trashfacility": "trashfacility", "yoot_doctor": "doctor"}

def start_of(fx):
    h = open(os.path.join(TOWERS, fx + ".tower")).read(600)
    return float(re.search(r' time="([\d.]+)"', h).group(1)), int(re.search(r' funds="(-?\d+)"', h).group(1))

def hotel_income(events, lo, hi):
    tot = 0
    for t, m in events:
        if lo <= t < hi:
            x = re.search(r"Income from Hotel Facilities just came in\.?: \$(-?\d+)", m)
            if x: tot += int(x.group(1))
    return tot

def web_rows(fx, seed, pre="web"):
    p = os.path.join(OUT, f"{pre}_seed{seed}.json")
    if not os.path.exists(p): return None
    recs = [r for r in json.load(open(p)) if r["fixture"] == fx + ".tower"]
    ok = [r for r in recs if not r["error"]]
    raw_err = next((r["error"] for r in recs if r["variant"] == "raw" and r["error"]), None)
    if not ok: return {"error": recs[-1]["error"], "raw_error": raw_err}
    r = ok[-1]
    t0, f0 = start_of(fx)
    rows, prev = [], r["start"]
    ev = [(t, m) for t, m in r["messages"]]
    mids = r["midnights"]; pm = {"called": 0, "entered": 0, "checkins": 0}
    for k, d in enumerate(r["days"], 1):
        mid = mids[k - 1] if k - 1 < len(mids) else None
        row = {"day": k, "funds": d["funds"], "dfunds": d["funds"] - prev["funds"], "popMax": d["popMax"], "popMin": d["popMin"],
               "pop": d["population"], "grade": d["grade"],
               "called": (mid["called"] - pm["called"]) if mid else None, "entered": (mid["entered"] - pm["entered"]) if mid else None,
               "checkins": (mid["checkins"] - pm["checkins"]) if mid else None,
               "hotelIncome": hotel_income(ev, k - 1, k), "blackout": d["blackout"], "blackoutsStarted": d["blackoutsStarted"],
               "items": {ALIAS_BACK.get(i, i): n for i, n in d["items"].items()}}
        if mid: pm = {a: mid[a] for a in pm}
        prev = d; rows.append(row)
    return {"variant": r["variant"], "aliased": r.get("aliased"), "dropped": r.get("dropped"), "seconds": r["seconds"], "rows": rows,
            "raw_error": raw_err, "startItems": r["start"]["items"], "startPop": r["start"]["population"], "power": r["days"][-1].get("power"), "messages": r["messages"]}

def cpp_rows(fx, seed):
    d = os.path.join(OUT, "cpp", fx, f"s{seed}")
    if not os.path.exists(os.path.join(d, "summary.json")): return None
    t0, f0 = start_of(fx)
    final = json.load(open(os.path.join(d, "summary.json")))
    dumps = []
    for jp in sorted(glob.glob(os.path.join(d, "dumps", "tick_*.json"))):
        j = json.load(open(jp)); j["_tower"] = jp[:-5] + ".tower"; dumps.append(j)
    ev = []
    for e in final["events"]:
        m = re.match(r"([\d.]+) (.*)", e); ev.append((float(m.group(1)), m.group(2)))
    pools = [m for t, m in ev if m.startswith("pool: called")]
    log = open(os.path.join(d, "log.txt")).read()
    real = re.search(r"real\s+(\S+)", log); real = real.group(1) if real else None
    rows, prev_f, lo_abs = [], f0, t0
    for k in (1, 2, 3):
        cand = [x for x in dumps if x["time"]["absolute"] - t0 >= k] if k < 3 else []
        src = cand[0] if cand else final
        in_win = [x for x in dumps if lo_abs <= x["time"]["absolute"] <= src["time"]["absolute"]]
        pops = [x["population"] for x in in_win] or [src["population"]]
        # hotel stays / blackout from the tower dumps
        seen, black = set(), 0
        for x in in_win:
            try: t = open(x["_tower"]).read()
            except OSError: continue
            for m in re.finditer(r'<item type="(hotel_\w+)" x="(-?\d+)" y="(-?\d+)"[^>]*?yootStayStatus="(\d+)"', t): seen.add((m.group(2), m.group(3)))
            if re.search(r'powerBlackout="[1-9]', t): black += 1
        pl = re.match(r"pool: called (\d+) entered (\d+)", pools[k - 1]) if k - 1 < len(pools) else None
        items = {i: n for i, n in src["items"].items() if i not in AGG}
        rows.append({"day": k, "funds": src["funds"], "dfunds": src["funds"] - prev_f, "popMax": max(pops), "popMin": min(pops), "pop": src["population"],
                     "grade": src["rating"] + 1, "called": int(pl.group(1)) if pl else None, "entered": int(pl.group(2)) if pl else None,
                     "checkins": len(seen), "hotelIncome": hotel_income(ev, k - 1, k), "blackout": bool(re.search(r'powerBlackout="[1-9]', open(src["_tower"]).read())) if "_tower" in src else None,
                     "blackoutHours": black, "items": items})
        prev_f = src["funds"]; lo_abs = src["time"]["absolute"]
    pd = next((m for t, m in ev if m.startswith("Power down")), None)
    pt = next((t for t, m in ev if m.startswith("Power down")), None)
    return {"rows": rows, "real": real, "powerDownAt": pt, "ticks": final["ticks"]}

out = {}
for fx in FIX:
    out[fx] = {"web": {}, "webcyc": {}, "cpp": {}}
    for s in (1, 2, 3):
        w = web_rows(fx, s); c = cpp_rows(fx, s)
        if w: out[fx]["web"][s] = w
        wc = web_rows(fx, s, "webcyc")
        if wc: out[fx]["webcyc"][s] = wc
        if c: out[fx]["cpp"][s] = c
json.dump(out, open(os.path.join(OUT, "metrics.json"), "w"), indent=1)
print("wrote metrics.json", {fx: (len(v["web"]), len(v["cpp"])) for fx, v in out.items()})
