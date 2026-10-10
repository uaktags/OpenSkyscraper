#!/usr/bin/env python3
"""Build out/compare/REPORT.md tables from the three runs.

Inputs (all under out/): oracle OCR (compare/ocr/oracle_ocr_hourly.json), web hourly.json (game state
+ HUD DOM) and its OCR, C++ headless hourly.json (SimHarness dumps, true state) and C++ GUI OCR.
Writes out/compare/tables.md (the REPORT.md hand-written text includes it).
"""
import json, os
B = os.path.dirname(os.path.abspath(__file__)) + "/out"
oracle = {(r["day"], r["hour"]): r for r in json.load(open(B + "/compare/ocr/oracle_ocr_hourly.json"))}
webh = json.load(open(B + "/web/hourly.json"))
webo = {(r["day"], r["hour"]): r for r in json.load(open(B + "/compare/ocr/web_ocr_hourly.json"))}
cppg = {(r["day"], r["hour"]): r for r in json.load(open(B + "/compare/ocr/cpp_gui_ocr_hourly.json"))}
cpph = json.load(open(B + "/art_hourly/hourly.json"))
WD = {0: "1st WD", 1: "2nd WD", 2: "WE"}
WEBD = {"Hol": "WE", "D0": "1st WD", "D1": "2nd WD"}


def web_date(t):
    p = t.split()
    return f"{WEBD.get(p[0], p[0])}/{p[1]}"


def o_date(t):
    return "/".join(t.split("/")[:2])


def cpp_state(k):
    if k == 0:
        return None
    r = cpph[k - 1]
    return r


def m(v):
    return f"{v:,}" if isinstance(v, int) else str(v)


def d(a, b):
    return "" if a is None or b is None else f"{a - b:+,}"


L = []
L.append("### Per-day table (shot index = hours since load; 07 and 23 of each run day)\n")
L.append("Oracle values are OCR of the Wine HUD (wall-clock shots); web = game state at the same elapsed game hour "
         "(HUD OCR agrees unless noted); C++ = headless SimHarness state at the same elapsed game hour (tick 40*k). "
         "Dates are in-game dates (WD = weekday, WE = weekend). Deltas are port minus oracle.\n")
L.append("| Day/hr | Oracle funds | Oracle pop | Oracle date | Web funds | Web pop | Web date | C++ funds | C++ pop | C++ date | Web-Orc funds | C++-Orc funds | Web-Orc pop | C++-Orc pop |")
L.append("|---|---:|---:|---|---:|---:|---|---:|---:|---|---:|---:|---:|---:|")
for day in (1, 2, 3):
    for hr in (7, 23):
        k = (day - 1) * 24 + hr
        o = oracle[(day, hr)]
        w = webh[k]
        c = cpp_state(k)
        wd = web_date(w["hud"]["date"]["text"]) if False else f"{WEBD[('Hol' if w['day']==2 else 'D%d' % w['day'])]}/Q{w['quarter']}"
        cd = f"{WD[c['day']]}/Q{c['quarter']}"
        L.append(f"| d{day} {hr:02d}:00 | {m(o['funds'])} | {m(o['population'])} | {o_date(o['date'])} | {m(w['funds'])} | {m(w['population'])} | {wd} | "
                 f"{m(c['funds'])} | {m(c['population'])} | {cd} | {d(w['funds'], o['funds'])} | {d(c['funds'], o['funds'])} | {d(w['population'], o['population'])} | {d(c['population'], o['population'])} |")
L.append("\nRating: oracle 5 stars throughout (the Tower promotion dialog fires in day 3), web 5, C++ 5 (all runs).\n")

L.append("### Date-aligned comparison (the three runs do not tick at the same speed, so compare by in-game date)\n")
L.append("First and last sample of every in-game date per source: funds / population.\n")
L.append("| In-game date | Oracle first | Oracle last | Web first | Web last | C++ headless first | C++ headless last |")
L.append("|---|---|---|---|---|---|---|")
order = ["WE/Q2", "1st WD/Q3", "2nd WD/Q3", "WE/Q3"]
def collect():
    S = {"o": {}, "w": {}, "c": {}}
    for (dd, hh), r in oracle.items():
        key = o_date(r["date"]).replace("/3Q", "/Q3").replace("/2Q", "/Q2")
        S["o"].setdefault(key, []).append((r["funds"], r["population"]))
    for i, w in enumerate(webh):
        key = f"{WEBD['Hol' if w['day']==2 else 'D%d' % w['day']]}/Q{w['quarter']}"
        S["w"].setdefault(key, []).append((w["funds"], w["population"]))
    for c in cpph:
        key = f"{WD[c['day']]}/Q{c['quarter']}"
        S["c"].setdefault(key, []).append((c["funds"], c["population"]))
    return S
S = collect()
f = lambda t: "-" if t is None else f"{t[0]:,} / {t[1]:,}"
for key in order + ["2nd WD/Q2"]:
    row = [key]
    for s in "owc":
        v = S[s].get(key)
        row += [f(v[0] if v else None), f(v[-1] if v else None)]
    L.append("| " + " | ".join(row) + " |")
L.append("\n### C++ GUI run (OCR of the 1280x800 time window) at the same shot indices\n")
L.append("| Day/hr | funds | pop | date | stars |\n|---|---:|---:|---|---:|")
for day in (1, 2, 3):
    for hr in (7, 23):
        r = cppg[(day, hr)]
        L.append(f"| d{day} {hr:02d} | {m(r['funds'])} | {m(r['population'])} | {r['date']} | {r['stars']} |")
open(B + "/compare/tables.md", "w").write("\n".join(L) + "\n")
print("\n".join(L))
