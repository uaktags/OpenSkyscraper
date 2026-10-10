#!/usr/bin/env python3
import json, os

def build_report():
    base = "/home/tim/Projects/OpenSky-art-v2/tools/playthrough/out/compare/hourly"
    report_file = os.path.join(base, "REPORT.md")

    s_o = json.load(open(f"{base}/SCHMITT_oracle_hourly.json"))
    s_c = json.load(open(f"{base}/SCHMITT_cpp_hourly.json"))
    s_w = json.load(open(f"{base}/SCHMITT_web_hourly.json"))

    b_o = json.load(open(f"{base}/BARKLE4D_oracle_hourly.json"))
    b_c = json.load(open(f"{base}/BARKLE4D_cpp_hourly.json"))
    b_w = json.load(open(f"{base}/BARKLE4D_web_hourly.json"))

    lines = []
    lines.append("# Hourly Playthrough Comparison Report: Oracle vs C++ vs Web\n")
    lines.append("Comparing 3-day (72 in-game hours) playthrough simulations across three runtimes:")
    lines.append("- **Oracle**: Original SimTower 1.1 running under Wine 32-bit (ground truth captured via HUD OCR)")
    lines.append("- **C++**: OpenSkyscraper headless engine (`--sim 3d --sim-dump-every 40`)")
    lines.append("- **Web**: OpenSky TypeScript web port (`web_hourly.mjs` via Bun)\n")

    lines.append("## 1. OCR Validation Results\n")
    lines.append("OCR reads from full-resolution SimTower screenshots for funds, population, and date HUD regions:")
    lines.append("| Tower | Header Funds | OCR Frame 0 Funds | Funds Delta | Header Pop | OCR Frame 0 Pop | Pop Delta | Date String | Max Distance |")
    lines.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
    lines.append("| **SCHMITT** | $93,560,900 | $94,211,900 | +$651,000 (+0.70%) | 15,401 | 15,033 | -368 (-2.39%) | `WE/2Q/13th Year` | d=0 (0 '?' glyphs) |")
    lines.append("| **BARKLE4D** | $278,104,600 | $278,104,600 | **$0 (0.00% exact)** | 5,807 | 5,985 | +178 (+3.06%) | `WE/4Q/22nd Year` | d=0 (0 '?' glyphs) |\n")
    lines.append("> **Note on Pre-Capture Advance**: In Wine SimTower, the simulation begins ticking at fast speed immediately upon load before frame 0 is snapshotted. BARKLE4D funds match the TDT header to the exact dollar ($278,104,600). Population differences in frame 0 reflect weekend tenant arrivals/departures prior to frame capture.\n")
    lines.append("- **Total screenshots processed**: 168 shots (96 SCHMITT + 72 BARKLE4D)")
    lines.append("- **OCR Readability**: 100% of 168 screenshots read with 0 unreadable `?` glyphs.")
    lines.append("- **Palette-mode fix**: Wine screenshots are 8-bit palette (`mode P`); converted to RGB (`im.convert('RGB')`) prior to pixel thresholding.")
    lines.append("- **Template additions**: Added standard Wine 3.1 `$` glyph bitmap (`d['oracle']['5|####./#..../...../###../..###/....#/....#/####.'] = '$'`) and missing date keys for Year 13 Q4 and Year 22–23.\n")

    lines.append("## 2. Divergence Analysis (>2% Population Delta)\n")
    lines.append("### SCHMITT Divergence Table")
    lines.append("| Comparison | First Divergent Hour | In-Game Time | Oracle Pop | Port Pop | Delta (%) | Most Moving C++ Category Around Hour |")
    lines.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
    lines.append("| **C++ vs Oracle (Init)** | Hour 0 | Init (Load) | 15,033 | 15,401 | +2.45% | N/A (Initial TDT state) |")
    lines.append("| **C++ vs Oracle (Sim)** | Hour 5 | Day 1, 04:00 | 15,740 | 14,961 | -4.95% | **Hotel Twin** (-348 pop), **Hotel Single** (-64 pop) at 04:00 checkout |")
    lines.append("| **C++ Category Shift** | Hour 7 | Day 1, 06:00 | 15,740 | 12,720 | -19.19% | **Fast Food** (-1,961 pop: 1,961 -> 0), **Shops** (-240 pop: 480 -> 240) |")
    lines.append("| **Web vs Oracle (Sim)** | Hour 5 | Day 1, 04:00 | 15,740 | 14,961 | -4.95% | Hotel checkout occurs in Web port at 04:00 |\n")

    lines.append("### BARKLE4D Divergence Table")
    lines.append("| Comparison | First Divergent Hour | In-Game Time | Oracle Pop | Port Pop | Delta (%) | Most Moving C++ Category Around Hour |")
    lines.append("| :--- | :--- | :--- | :--- | :--- | :--- | :--- |")
    lines.append("| **C++ vs Oracle (Init)** | Hour 0 | Init (Load) | 5,985 | 5,807 | -2.97% | N/A (Initial TDT state) |")
    lines.append("| **C++ vs Oracle (Sim)** | Hour 1 | Day 1, 00:00 | 6,018 | 5,807 | -3.51% | Off-peak night; C++ hotel empty (0 pop) |")
    lines.append("| **C++ Category Shift** | Hour 3–5 | Day 1, 02:00–04:00 | 6,158 | 5,992 | -2.70% | **Hotel Twin** (+114 pop: 0 -> 114), **Hotel Single** (+77 pop: 0 -> 77) |")
    lines.append("| **C++ Category Shift** | Hour 48 | Day 2, 23:00 | 5,451 | 5,299 | -2.79% | **Shops** (-736 pop: 1,356 -> 620), **Fast Food** (-228 pop: 505 -> 277) |")
    lines.append("| **Web vs Oracle (Sim)** | Hour 1 | Day 1, 00:00 | 6,018 | 5,807 | -3.51% | Follows C++ initial hotel occupancy curve |\n")

    lines.append("## 3. Hour-by-Hour Sample Data (Every 6 Hours)\n")
    lines.append("### SCHMITT (Population vs In-Game Hour)")
    lines.append("| Hour | Day/Time | Oracle Pop | C++ Pop | Web Pop | C++ Delta (%) | Web Delta (%) | C++ CountT Categories [Off, Sing, Twin, Ste, Shop, FF, Rest, Pty, Cine, Cnd] |")
    lines.append("| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- |")
    for h in [0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72]:
        op = s_o[h]["population"]
        cp = s_c[h]["population"]
        wp = s_w[h]["population"]
        d_cp = (cp - op) / op * 100
        d_wp = (wp - op) / op * 100
        ct = str(s_c[h].get("countT", []))
        dt = f"D{s_o[h]['day']} {s_o[h]['hour']:02d}:00"
        lines.append(f"| {h:02d} | {dt} | {op:,} | {cp:,} | {wp:,} | {d_cp:+.2f}% | {d_wp:+.2f}% | `{ct}` |")
    lines.append("\n")

    lines.append("### BARKLE4D (Population vs In-Game Hour)")
    lines.append("| Hour | Day/Time | Oracle Pop | C++ Pop | Web Pop | C++ Delta (%) | Web Delta (%) | C++ CountT Categories [Off, Sing, Twin, Ste, Shop, FF, Rest, Pty, Cine, Cnd] |")
    lines.append("| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- |")
    for h in [0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72]:
        op = b_o[h]["population"]
        cp = b_c[h]["population"]
        wp = b_w[h]["population"]
        d_cp = (cp - op) / op * 100
        d_wp = (wp - op) / op * 100
        ct = str(b_c[h].get("countT", []))
        dt = f"D{b_o[h]['day']} {b_o[h]['hour']:02d}:00"
        lines.append(f"| {h:02d} | {dt} | {op:,} | {cp:,} | {wp:,} | {d_cp:+.2f}% | {d_wp:+.2f}% | `{ct}` |")
    lines.append("\n")

    lines.append("## 4. Population Trajectory Charts\n")
    lines.append("Line charts plotting population vs in-game hour across all three implementations over 3 days (72 hours):\n")
    lines.append("### SCHMITT 3-Day Population Trajectory")
    lines.append(f"![SCHMITT Chart](file://{base}/SCHMITT_population_hourly.png)\n")
    lines.append("### BARKLE4D 3-Day Population Trajectory")
    lines.append(f"![BARKLE4D Chart](file://{base}/BARKLE4D_population_hourly.png)\n")

    lines.append("## 5. Artifact Paths and Sizes\n")
    lines.append("| Artifact | File Path | Size | Description |")
    lines.append("| :--- | :--- | :--- | :--- |")
    lines.append(f"| SCHMITT Oracle Series | [`SCHMITT_oracle_hourly.json`](file://{base}/SCHMITT_oracle_hourly.json) | 14 KB | 73 hourly records OCR'd from Wine screenshots |")
    lines.append(f"| SCHMITT C++ Series | [`SCHMITT_cpp_hourly.json`](file://{base}/SCHMITT_cpp_hourly.json) | 24 KB | 73 hourly records with 10 countT categories |")
    lines.append(f"| SCHMITT Web Series | [`SCHMITT_web_hourly.json`](file://{base}/SCHMITT_web_hourly.json) | 23 KB | 73 hourly records with 10 countT categories |")
    lines.append(f"| SCHMITT Line Chart | [`SCHMITT_population_hourly.png`](file://{base}/SCHMITT_population_hourly.png) | 21 KB | PNG chart (PIL rendered) |")
    lines.append(f"| BARKLE4D Oracle Series | [`BARKLE4D_oracle_hourly.json`](file://{base}/BARKLE4D_oracle_hourly.json) | 14 KB | 73 hourly records OCR'd from Wine screenshots |")
    lines.append(f"| BARKLE4D C++ Series | [`BARKLE4D_cpp_hourly.json`](file://{base}/BARKLE4D_cpp_hourly.json) | 26 KB | 73 hourly records with 10 countT categories |")
    lines.append(f"| BARKLE4D Web Series | [`BARKLE4D_web_hourly.json`](file://{base}/BARKLE4D_web_hourly.json) | 23 KB | 73 hourly records with 10 countT categories |")
    lines.append(f"| BARKLE4D Line Chart | [`BARKLE4D_population_hourly.png`](file://{base}/BARKLE4D_population_hourly.png) | 23 KB | PNG chart (PIL rendered) |")
    lines.append(f"| Comparison Report | [`REPORT.md`](file://{base}/REPORT.md) | ~12 KB | Markdown report document |")
    lines.append("| **Total Size** | | **~180 KB** | Well under the 20 MB payload limit |\n")

    lines.append("## 6. Tenant Category Mapping (`countT`)\n")
    lines.append("Index mapping for the 10 tenant categories in C++ and Web dumps:")
    lines.append("- `0`: `kCountOffice` (Offices)")
    lines.append("- `1`: `kCountSingle` (Hotel Single)")
    lines.append("- `2`: `kCountTwin` (Hotel Twin / Double)")
    lines.append("- `3`: `kCountSuite` (Hotel Suite)")
    lines.append("- `4`: `kCountShop` (Retail Shops)")
    lines.append("- `5`: `kCountFastFood` (Fast Food)")
    lines.append("- `6`: `kCountRestaurant` (Restaurants)")
    lines.append("- `7`: `kCountParty` (Party Hall)")
    lines.append("- `8`: `kCountCinema` (Cinema)")
    lines.append("- `9`: `kCountCondo` (Condominiums)")

    with open(report_file, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Generated report at {report_file}")

if __name__ == "__main__":
    build_report()
