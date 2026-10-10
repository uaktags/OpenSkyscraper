#!/usr/bin/env python3 -I
"""Renders out/metrics.json into markdown tables (out/tables.md). Mean over seeds 1-3, (min..max) in brackets."""
import json, os, statistics
HERE = os.path.dirname(os.path.abspath(__file__))
m = json.load(open(os.path.join(HERE, "out", "metrics.json")))
ORIG = {  # game-parity.md section numbers in the comments; None = not recorded
    "tokyo_g2_hotel_subway": {"popMax": [940, 1417, 2095], "checkins": [0, 0, 0], "note": "7.4 item 5 corrected map; 7.5 zero hotel guests in both games"},
    "tokyo_g4_blackout": {"popMax": [140, 79, 79], "dfunds": [36000, 0, -2505000], "blackout": [True, True, True], "note": "7.5/7.6.D: Power down 11:00 day 0, lasting; oracle day-3 funds delta -2,505,000 is the quarterly maintenance at 3.0 (inside the D3 row)"},
    "tokyo_g5_large_elevator": {"popMax": [144, None, None], "note": "7.6.A hkA-lift5, 1 day only: peak 144/144, riding 9/9; 2/2 funds within tolerance"},
    "hawaii_g2_resort": {"note": "no oracle run recorded for this fixture"},
    "tokyo_g3_venues": {"popMax": [710, 765, 1049], "note": "7.4 item 5 corrected map; 7.6.C Event Center audience 70 (d1) / 143 (d2)"},
}
HAWAII_G3 = "hawaii_g3_boatdeck (the only Hawaii oracle): popMax 540/632/977, hotel check-ins 9 (d1) / 11 (d2), hotel income $45,000 (d2) / $54,000 (d3), funds change 37,100 / 53,300 (7.4, 7.5, 7.6.B/G)"

def stat(vals):
    vals = [v for v in vals if v is not None]
    if not vals: return "-"
    mean = statistics.mean(vals)
    if len(vals) == 1 or min(vals) == max(vals): return f"{mean:,.0f}"
    return f"{mean:,.0f} ({min(vals):,}..{max(vals):,})"

def col(side, fx, key, day):
    return [x["rows"][day - 1][key] for x in m[fx][side].values() if "rows" in x]

L = []
def w(s=""): L.append(s)
METRICS = [("popMax", "pop peak (hourly samples)"), ("popMin", "pop min"), ("grade", "grade"), ("called", "visitors called (day)"), ("entered", "visitors entered (day)"),
           ("checkins", "hotel check-ins (day)"), ("hotelIncome", "hotel income $ (day)")]
for fx in m:
    w(f"### {fx}")
    ws = next(iter(m[fx]["web"].values()), None)
    if ws and "error" in ws: w(f"WEB FAILED: {ws['error']['message']}\n")
    elif ws:
        w(f"web variant: **{ws['variant']}**" + (f" (raw load failed: `{ws['raw_error']['message']}`; aliased {ws['aliased']}, dropped {ws['dropped']})" if ws["variant"] == "aliased" else "") + f"; web wall time {min(x['seconds'] for x in m[fx]['web'].values()):.2f}-{max(x['seconds'] for x in m[fx]['web'].values()):.2f} s per 3-day run; C++ real {', '.join(x['real'] or '?' for x in m[fx]['cpp'].values())}.")
    w(f"Original: {ORIG[fx]['note']}" + (f". Nearest: {HAWAII_G3}" if fx.startswith("hawaii") else ""))
    w()
    w("| metric | day | web | C++ | original |")
    w("|---|---|---|---|---|")
    for key, label in METRICS:
        for d in (1, 2, 3):
            o = (ORIG[fx].get(key) or [None] * 3)[d - 1] if key in ORIG[fx] else None
            w(f"| {label} | D{d} | {stat(col('web', fx, key, d))} | {stat(col('cpp', fx, key, d))} | {o if o is not None else 'n/r'} |")
    for d in (1, 2, 3):
        o = ORIG[fx].get("dfunds", [None] * 3)[d - 1] if "dfunds" in ORIG[fx] else None
        o = o if o is not None else "n/r"
        w(f"| funds change $ (web as loaded) | D{d} | {stat(col('web', fx, 'dfunds', d))} | {stat(col('cpp', fx, 'dfunds', d))} | {o} |")
    for d in (1, 2, 3):
        w(f"| funds change $ (web + accountingCycle=0) | D{d} | {stat(col('webcyc', fx, 'dfunds', d))} | {stat(col('cpp', fx, 'dfunds', d))} | |")
    bl = lambda side: " / ".join(("black" if any(x["rows"][d]["blackout"] for x in m[fx][side].values() if "rows" in x) else "no") if m[fx][side] and d < 2 or side == "web" else "-" for d in range(3))
    wb = [x for x in m[fx]["web"].values() if "rows" in x]
    cb = [x for x in m[fx]["cpp"].values() if "rows" in x]
    w(f"| blackout at D1 / D2 end | | {' / '.join(str(wb[0]['rows'][d]['blackout']) for d in (0,1)) if wb else '-'} | {' / '.join(str(cb[0]['rows'][d]['blackout']) for d in (0,1)) if cb else '-'} | {' / '.join(map(str, ORIG[fx]['blackout'][:2])) if 'blackout' in ORIG[fx] else 'n/r'} |")
    if wb and cb:
        wi, ci = wb[0]["rows"][2]["items"], cb[0]["rows"][2]["items"]
        diff = {k: (wi.get(k, 0), ci.get(k, 0)) for k in sorted(set(wi) | set(ci)) if wi.get(k, 0) != ci.get(k, 0)}
        w(f"| items D3 web vs C++ (differences only) | | {', '.join(f'{k}: {a}' for k,(a,b) in diff.items()) or 'identical'} | {', '.join(f'{k}: {b}' for k,(a,b) in diff.items()) or 'identical'} | |")
        w(f"| item types total D3 | | {sum(wi.values())} | {sum(ci.values())} | |")
    if wb: w(f"\nweb power at D3: supply {wb[0]['power']['supply']}, consumption {wb[0]['power']['consumption']}")
    if cb and cb[0].get("powerDownAt") is not None: w(f"C++ 'Power down' message at t={cb[0]['powerDownAt']} (day {int(cb[0]['powerDownAt'])}, hour {(cb[0]['powerDownAt'] % 1) * 24:.2f}); web blackoutsStarted={wb[0]['rows'][2]['blackoutsStarted'] if wb else '-'}")
    w()
open(os.path.join(HERE, "out", "tables.md"), "w").write("\n".join(L))
print("\n".join(L))
