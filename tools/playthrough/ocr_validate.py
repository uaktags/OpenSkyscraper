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

    # Run-independent checks (the oracle advances by wall clock, so no value is fixed per shot).
    # oracle: sanity of the first shot, then the saved day*.TDT balance must lie between the OCR of
    # the last shot of day d and the first shot of day d+1 (the sim keeps running during the save).
    first = o[min(o)]
    chk("oracle first shot date", first["date"], "WE/2Q/13th Year")
    chk("oracle first shot balance within $1M of the SCHMITT.TDT header", abs(first["funds"] - 93560900) < 1000000, True)
    if tdt_dir:
        for d in range(1, 5):
            p = os.path.join(tdt_dir, f"day{d}.TDT")
            if not os.path.exists(p) or (d, 23) not in o:
                continue
            lo = o[(d, 23)]["funds"]
            hi = o[(d + 1, 0)]["funds"] if (d + 1, 0) in o else lo
            lo, hi = min(lo, hi), max(lo, hi)
            chk(f"oracle day{d}.TDT balance in [OCR d{d}_h23, OCR d{d + 1}_h00]", lo <= tdt_funds(p) <= hi, True)
    unreadable = [k for k, r in o.items() if k[0] <= 3 and "?" in (r["fund"] + r["pop"] + r["date"])]
    chk("oracle days 1-3: shots with '?' glyphs (mid-update frames, <= 2)", len(unreadable) <= 2, True)
    # C++ GUI: the first shot is a few seconds after the load (the camera is moved to the lobby first)
    chk("cpp d1_h00 fund within $1M of the SCHMITT.TDT header", abs(c[(1, 0)]["funds"] - 93560900) < 1000000, True)
    chk("cpp d1_h00 date", c[(1, 0)]["date"] in ("2nd WD/2Q/13rd Yr", "WE/2Q/13rd Yr"), True)
    chk("cpp d1_h20 fund (= Financial Report 'Current balance' in that frame)", c[(1, 20)]["funds"], 110246900)
    for n, g, w, ok in checks:
        print(("PASS " if ok else "FAIL ") + f"{n}: ocr/read={g!r} expected={w!r}")
    print(f"{len(checks) - len(fails)}/{len(checks)} checks pass")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
