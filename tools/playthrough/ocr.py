#!/usr/bin/env python3
"""Per-source digit-template OCR for the SimTower / OpenSkyscraper / OpenSkyWeb HUDs.

The three HUDs all render with fixed bitmap/TTF fonts at fixed positions, so a glyph
always produces the same pixel pattern.  Method:
  1. crop a fixed HUD region per source (REGIONS below),
  2. binarize it with the source's foreground predicate,
  3. split it into glyphs on empty pixel columns (wide gaps become spaces),
  4. look every glyph up in ocr_templates.json (exact bitmap, else nearest bitmap of
     the same size within a small Hamming budget; otherwise '?').
Templates are NOT hand-drawn: `collect` clusters every glyph found in a set of
screenshots and writes a contact sheet; the clusters are labelled once (from screenshots
with known values) and stored in ocr_templates.json.  `run` OCRs a whole shot directory
and saves the region crops it read to <out>/ocr/ as evidence.

Usage:
  ocr.py collect <source> <shot_dir> <sheet.png> [candidates.json]
  ocr.py run     <source> <shot_dir> <evidence_dir> <metrics_out.json> [web hourly.json]
  ocr.py train   web <shot_dir> <hourly.json> <train_days e.g. 1,2>   (supervised: web only; truth = HUD DOM text)
  ocr.py validate web <shot_dir> <hourly.json> <days e.g. 3> <report.json>

`train` builds glyph templates for sources whose true state is logged next to the shots
(the web driver's hourly.json): every shot whose glyph count equals the length of the true
string contributes (bitmap -> char) votes; ambiguous bitmaps are dropped.  `validate` OCRs
held-out shots and compares with the logged truth.
"""
import sys, os, re, json, glob
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE_FILE = os.path.join(HERE, "ocr_templates.json")


def lum(p):
    return (p[0] * 299 + p[1] * 587 + p[2] * 114) // 1000


# foreground predicates -------------------------------------------------------
def fg_dark(p):      # oracle: dark text on grey; red (weekend) also counts
    return lum(p) < 110 or (p[0] > 150 and p[1] < 90 and p[2] < 90)

class Adaptive:
    """Light text on a dark panel whose tint drifts (C++ and web HUDs): threshold at the
    midpoint between the crop's background (median luminance) and its brightest pixel."""
    def __init__(self, frac=0.5):
        self.frac = frac
    def prepare(self, crop):
        ls = sorted(lum(crop.getpixel((x, y))) for y in range(crop.height) for x in range(crop.width))
        lo, hi = ls[len(ls) // 2], ls[-1]
        thr = lo + self.frac * (hi - lo)
        return lambda p: lum(p) > thr

fg_light = Adaptive()   # C++ HUD
fg_web = Adaptive()     # web HUD

# region = (box, fg predicate, space gap in px)
REGIONS = {
    "oracle": {
        "fund": ((550, 18, 632, 34), fg_dark, 99),
        "pop":  ((545, 36, 632, 52), fg_dark, 99),
        "date": ((360, 19, 500, 38), fg_dark, 4),
    },
    "cpp": {
        "fund": ((528, 29, 640, 47), fg_light, 99),
        "pop":  ((528, 55, 640, 73), fg_light, 99),
        "date": ((318, 30, 480, 48), fg_light, 4),
    },
    "web": {
        "fund": ((592, 50, 712, 70), fg_web, 99),
        "pop":  ((716, 58, 790, 76), fg_web, 99),
        "date": ((292, 52, 380, 70), fg_web, 99),
        "clock": ((160, 70, 212, 90), fg_web, 99),
    },
}


def load_templates():
    if os.path.exists(TEMPLATE_FILE):
        return json.load(open(TEMPLATE_FILE))
    return {}


def segment(crop, fg, gap):
    """Return list of (x0, y0, rows[list[str]], space_before) glyphs."""
    w, h = crop.size
    px = crop.load()
    if hasattr(fg, "prepare"):
        fg = fg.prepare(crop)
    mask = [[fg(px[x, y]) for x in range(w)] for y in range(h)]
    cols = [any(mask[y][x] for y in range(h)) for x in range(w)]
    glyphs, x, last_end = [], 0, None
    while x < w:
        if cols[x]:
            s = x
            while x < w and cols[x]:
                x += 1
            ys = [y for y in range(h) if any(mask[y][xx] for xx in range(s, x))]
            y0, y1 = ys[0], ys[-1]
            rows = ["".join("#" if mask[y][xx] else "." for xx in range(s, x)) for y in range(y0, y1 + 1)]
            glyphs.append((s, y0, rows, last_end is not None and s - last_end >= gap))
            last_end = x
        else:
            x += 1
    return glyphs


WHOLE = {"date"}  # (the web clock is read glyph-wise)   # regions matched as one bitmap (kerned / sub-pixel positioned text)


def segment_region(source, name, crop, fg, gap):
    gl = segment(crop, fg, gap)
    if name not in WHOLE or not gl:
        return gl
    # merge every glyph of the region into one bitmap spanning x0..x1, y0..y1
    x0 = gl[0][0]
    x1 = max(g[0] + len(g[2][0]) for g in gl)
    y0 = min(g[1] for g in gl)
    y1 = max(g[1] + len(g[2]) for g in gl)
    grid = [["."] * (x1 - x0) for _ in range(y1 - y0)]
    for (x, y, rows, _sp) in gl:
        for dy, r in enumerate(rows):
            for dx, ch in enumerate(r):
                if ch == "#":
                    grid[y + dy - y0][x + dx - x0] = "#"
    return [(x0, y0, ["".join(r) for r in grid], False)]


def gkey(y0, rows):
    return f"{y0}|" + "/".join(rows)


def hamming(a, b):
    return sum(ca != cb for ra, rb in zip(a, b) for ca, cb in zip(ra, rb))


def _pad(rows, w, h):
    return [r.ljust(w, ".") for r in rows] + ["." * w] * (h - len(rows))


def _shift_dist(a, b):
    """Min Hamming distance of two bitmaps over +/-1 px translations (canvas = union size + 2)."""
    w = max(len(a[0]), len(b[0])) + 2
    h = max(len(a), len(b)) + 2
    best = 10 ** 9
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ga = [["."] * w for _ in range(h)]
            gb = [["."] * w for _ in range(h)]
            for y, r in enumerate(a):
                for x, c in enumerate(r):
                    if c == "#":
                        ga[y + 1][x + 1] = "#"
            for y, r in enumerate(b):
                for x, c in enumerate(r):
                    if c == "#" and 0 <= y + 1 + dy < h and 0 <= x + 1 + dx < w:
                        gb[y + 1 + dy][x + 1 + dx] = "#"
            d = sum(ga[y][x] != gb[y][x] for y in range(h) for x in range(w))
            best = min(best, d)
    return best


def classify(templates, y0, rows):
    """Exact bitmap match first; otherwise nearest template over +/-1 px shifts, accepted only
    inside a Hamming budget (about 12 % of the glyph area) and with a clear margin."""
    k = gkey(y0, rows)
    if k in templates:
        return templates[k], 0
    area = len(rows) * len(rows[0])
    best = {}
    for tk, ch in templates.items():
        ty, trows = tk.split("|", 1)
        trows = trows.split("/")
        if abs(len(trows) - len(rows)) > 1 or abs(len(trows[0]) - len(rows[0])) > 1:
            continue
        d = _shift_dist(rows, trows)
        if ch not in best or d < best[ch]:
            best[ch] = d
    if not best:
        return "?", 10 ** 9
    ranked = sorted(best.items(), key=lambda kv: kv[1])
    ch, d = ranked[0]
    budget = 3 if len(rows[0]) > 40 else max(3, area // 8)   # whole-string regions: near-exact only
    if d > budget or (len(ranked) > 1 and ranked[1][1] - d < 2 and ranked[1][1] <= budget):
        return "?", d
    return ch, d


def read_region(source, name, img, templates, save_crop=None, box=None):
    _box, fg, gap = REGIONS[source][name]
    crop = img.crop(box or _box)
    if save_crop:
        crop.resize((crop.width * 4, crop.height * 4), Image.NEAREST).save(save_crop)
    text, worst = "", 0
    for (_, y0, rows, sp) in segment_region(source, name, crop, fg, gap):
        ch, d = classify(templates.get(source, {}), y0, rows)
        worst = max(worst, d)
        text += (" " if sp else "") + ch
    return text, worst


def count_stars(source, img):
    """Count gold stars (yellow clusters) in the HUD star strip; oracle/C++ only."""
    box = {"oracle": (245, 14, 355, 40), "cpp": (185, 29, 260, 47)}.get(source)
    if not box:
        return None
    c = img.crop(box); px = c.load(); occ = []
    for x in range(c.width):
        occ.append(any(px[x, y][0] > 190 and px[x, y][1] > 170 and px[x, y][2] < 110 for y in range(c.height)))
    n, prev, gapn = 0, False, 0
    for o in occ:
        if o and not prev and gapn >= 2 or (o and not prev and n == 0):
            n += 1
        gapn = 0 if o else gapn + 1
        prev = o
    return n


def shot_files(shot_dir):
    out = {}
    for f in glob.glob(os.path.join(shot_dir, "shot_d*_h*.png")):
        m = re.search(r"shot_d(\d+)_h(\d+)\.png$", f)
        if m:
            out[(int(m.group(1)), int(m.group(2)))] = f
    return dict(sorted(out.items()))


def cmd_collect(source, shot_dir, sheet, cand_json=None):
    clusters = {}
    for (d, h), f in shot_files(shot_dir).items():
        img = Image.open(f).convert("RGB")
        for name, (box, fg, gap) in REGIONS[source].items():
            for (_, y0, rows, _sp) in segment_region(source, name, img.crop(box), fg, gap):
                k = gkey(y0, rows)
                c = clusters.setdefault(k, {"n": 0, "region": name, "eg": f"{d}/{h}"})
                c["n"] += 1
    items = sorted(clusters.items(), key=lambda kv: (kv[1]["region"], -kv[1]["n"]))
    cell, per_row = 64, 12
    sheet_img = Image.new("RGB", (per_row * cell, ((len(items) + per_row - 1) // per_row) * (cell + 14)), (200, 200, 255))
    from PIL import ImageDraw
    dr = ImageDraw.Draw(sheet_img)
    for i, (k, c) in enumerate(items):
        y0, rows = k.split("|", 1)
        rows = rows.split("/")
        g = Image.new("RGB", (len(rows[0]), len(rows)), (255, 255, 255))
        for yy, r in enumerate(rows):
            for xx, ch in enumerate(r):
                if ch == "#":
                    g.putpixel((xx, yy), (0, 0, 0))
        s = max(1, min(4, (cell - 4) // max(len(rows[0]), len(rows))))
        g = g.resize((g.width * s, g.height * s), Image.NEAREST)
        x, y = (i % per_row) * cell, (i // per_row) * (cell + 14)
        sheet_img.paste(g, (x + 2, y + 12))
        dr.text((x + 2, y), f"{i}{c['region'][0]}", fill=(180, 0, 0))
    sheet_img.save(sheet)
    cands = [{"id": i, "key": k, "n": c["n"], "region": c["region"], "char": ""} for i, (k, c) in enumerate(items)]
    json.dump(cands, open(cand_json or sheet + ".json", "w"), indent=0)
    print(f"{len(items)} distinct glyphs -> {sheet}")


def parse_num(s):
    d = re.sub(r"[^0-9]", "", s)
    return int(d) if d else None


WEB_REGIONS = ("fund", "pop", "clock", "date")


def web_rec(hourly, d, h):
    i = (d - 1) * 24 + h
    return hourly[i] if i < len(hourly) else None


def web_box(rec, name):
    """Crop box of a web HUD element for this shot (the HUD reflows, so the web driver logs
    every element's bounding box in hourly.json), padded by 2 px."""
    x0, y0, x1, y1 = rec["hud"][name]["box"]
    return (x0 - 2, y0 - 2, x1 + 2, y1 + 2)


def web_truth(rec, name):
    """What the web HUD DOM displayed for this shot (independent of the pixels)."""
    return rec["hud"][name]["text"]


def cmd_train(source, shot_dir, hourly_json, train_days):
    """Supervised template building from shots with a known displayed string (web DOM text)."""
    from collections import Counter, defaultdict
    hourly = json.load(open(hourly_json))
    days = {int(x) for x in train_days.split(",")}
    votes = defaultdict(Counter)
    used = skipped = 0
    for (d, h), f in shot_files(shot_dir).items():
        if d not in days:
            continue
        rec = web_rec(hourly, d, h)
        img = Image.open(f).convert("RGB")
        for name in WEB_REGIONS:
            want = web_truth(rec, name)
            _b, fg, gap = REGIONS[source][name]
            gl = segment_region(source, name, img.crop(web_box(rec, name)), fg, gap)
            if name in WHOLE:
                want = [want]
            if len(gl) != len(want):
                skipped += 1
                continue
            used += 1
            for (_, y0, rows, _sp), ch in zip(gl, want):
                votes[gkey(y0, rows)][ch] += 1
    T = load_templates()
    T[source] = {}
    kept = 0
    for k, c in votes.items():
        (ch, n), = c.most_common(1)
        if len(c) == 1 or n / sum(c.values()) >= 0.9:
            T[source][k] = ch
            kept += 1
    json.dump(T, open(TEMPLATE_FILE, "w"), indent=0)
    print(f"{source}: {used} strings used, {skipped} skipped (glyph count mismatch), {kept} templates kept of {len(votes)}")


def cmd_validate(source, shot_dir, hourly_json, days, report):
    hourly = json.load(open(hourly_json))
    want_days = {int(x) for x in days.split(",")}
    templates = load_templates()
    tot = ok = 0
    bad = []
    per = {}
    for (d, h), f in shot_files(shot_dir).items():
        if d not in want_days:
            continue
        rec = web_rec(hourly, d, h)
        img = Image.open(f).convert("RGB")
        for name in WEB_REGIONS:
            want = web_truth(rec, name)
            got, _ = read_region(source, name, img, templates, box=web_box(rec, name))
            if name not in WHOLE:
                got = got.replace(" ", "")
            tot += 1
            per.setdefault(name, [0, 0])[1] += 1
            if got == want:
                ok += 1
                per[name][0] += 1
            else:
                bad.append({"day": d, "hour": h, "region": name, "ocr": got, "dom_text": want})
    json.dump({"days": sorted(want_days), "strings": tot, "exact": ok, "per_region_exact_total": per,
               "mismatches": bad}, open(report, "w"), indent=1)
    print(f"{source} validation on days {sorted(want_days)}: {ok}/{tot} strings exact {per}; {len(bad)} mismatches -> {report}")


def cmd_run(source, shot_dir, ev_dir, out_json, hourly_json=None):
    templates = load_templates()
    hourly = json.load(open(hourly_json)) if hourly_json else None
    os.makedirs(ev_dir, exist_ok=True)
    rows = []
    for (d, h), f in shot_files(shot_dir).items():
        img = Image.open(f).convert("RGB")
        rec = {"day": d, "hour": h, "file": os.path.basename(f)}
        wr = web_rec(hourly, d, h) if hourly else None
        for name in REGIONS[source]:
            if source == "web" and name not in WEB_REGIONS:
                continue
            keep = h in (7, 12, 23) or d == 1 and h in (8, 18)
            txt, worst = read_region(source, name, img, templates,
                                     os.path.join(ev_dir, f"{source}_d{d}_h{h:02d}_{name}.png") if keep else None,
                                     box=web_box(wr, name) if wr else None)
            rec[name] = txt
            if worst:
                rec.setdefault("fuzzy", {})[name] = worst
        rec["funds"] = parse_num(rec["fund"])
        rec["population"] = parse_num(rec["pop"])
        rec["stars"] = count_stars(source, img)
        rows.append(rec)
    json.dump(rows, open(out_json, "w"), indent=1)
    print(f"{len(rows)} shots OCR'd -> {out_json}")
    return rows


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "collect":
        cmd_collect(*a[1:])
    elif a and a[0] == "train":
        cmd_train(*a[1:])
    elif a and a[0] == "validate":
        cmd_validate(*a[1:])
    elif a and a[0] == "run":
        cmd_run(*a[1:])
    else:
        print(__doc__)
