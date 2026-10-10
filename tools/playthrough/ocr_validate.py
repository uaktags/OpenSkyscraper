#!/usr/bin/env python3
"""Checks of the OCR (ocr.py) against values known independently of the pixels.

oracle: values read by eye from full-resolution screenshots; balance of the saved day*.TDT
        (header int32 @4, units of $100) which must equal the OCR at the shot taken just before
        the save; every shot must read without '?' glyphs.
cpp:    the first shot is the freshly loaded SCHMITT.TDT, whose header says $93,560,900 /
        pop 15,401 / 2nd WD / 2Q; the Financial Report dialog in d1_h20 prints the balance.
Usage: ocr_validate.py <oracle_ocr.json> <cpp_ocr.json> [oracle_dir_with_day_tdt]  -> exit 1 on failure
"""
import sys, json, os, struct


def tdt_funds(p):
    with open(p, "rb") as f:
        b = f.read(8)
    return struct.unpack_from("<i", b, 4)[0] * 100


def main():
    o = {(r["day"], r["hour"]): r for r in json.load(open(sys.argv[1]))}
    c = {(r["day"], r["hour"]): r for r in json.load(open(sys.argv[2]))}
    tdt_dir = sys.argv[3] if len(sys.argv) > 3 else None
    fails, checks = [], []

    def chk(name, got, want):
        ok = got == want
        checks.append((name, got, want, ok))
        if not ok:
            fails.append(name)

    # by eye (oracle HUD, full-res shots)
    chk("oracle d1_h12 fund", o[(1, 12)]["funds"], 94369900)
    chk("oracle d1_h12 pop", o[(1, 12)]["population"], 15390)
    chk("oracle d1_h12 date", o[(1, 12)]["date"], "WE/2Q/13th Year")
    chk("oracle d3_h23 fund", o[(3, 23)]["funds"], 110445900)
    chk("oracle d3_h23 pop", o[(3, 23)]["population"], 15025)
    chk("oracle d3_h23 date", o[(3, 23)]["date"], "WE/3Q/13th Year")
    if tdt_dir and os.path.exists(os.path.join(tdt_dir, "day1.TDT")):
        chk("oracle day1.TDT balance == OCR d2_h00", tdt_funds(os.path.join(tdt_dir, "day1.TDT")), o[(2, 0)]["funds"])
    unreadable = [k for k, r in o.items() if "?" in (r["fund"] + r["pop"] + r["date"])]
    chk("oracle shots with '?' glyphs", len(unreadable), 0)
    # C++ GUI
    chk("cpp d1_h00 fund (= SCHMITT.TDT header)", c[(1, 0)]["funds"], 93560900)
    chk("cpp d1_h00 pop (= SCHMITT.TDT header)", c[(1, 0)]["population"], 15401)
    chk("cpp d1_h00 date", c[(1, 0)]["date"], "2nd WD/2Q/13rd Yr")
    chk("cpp d1_h20 fund (= Financial Report 'Current balance' in that frame)", c[(1, 20)]["funds"], 110246900)
    for n, g, w, ok in checks:
        print(("PASS " if ok else "FAIL ") + f"{n}: ocr/read={g!r} expected={w!r}")
    print(f"{len(checks) - len(fails)}/{len(checks)} checks pass")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
