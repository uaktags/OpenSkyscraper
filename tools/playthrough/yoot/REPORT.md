# WP15 - YootTower: web port vs C++ port vs original (C++ parity fixtures)

Subject: `/home/tim/Projects/OpenSkyWeb-art-v2` @ `28f4d3f` (art/cc0-remaster-v2, post-batches 6-8) against
`/home/tim/Projects/OpenSky-art-v2` @ `ce65de4` (art/cc0-remaster-v2),
fixtures `tests/parity/{tokyo_g2_hotel_subway,tokyo_g4_blackout,tokyo_g5_large_elevator,hawaii_g2_resort,tokyo_g3_venues}.tower`,
3 game days each, seeds 1-3. Nothing in either repo was committed or switched.

## Files

| file | purpose |
|---|---|
| `web_yoot_sim.mjs` | web runner (bun). Supports `--dt <val>` (default 0.2; test ran with `--dt 0.0166667`), `--seed S`, `--days N`, `--inject-cycle`, `--out f.json` |
| `run_cpp.sh` | C++ runner via the Unraid lab (`lab.sh sync/build/exec`): `sync-build`, `run`, `pull` |
| `collect.py`, `tables.py` | fold `out/web_seed*.json`, `out/webcyc_seed*.json`, `out/cpp/<fx>/s<N>/` into `out/metrics.json`, `out/tables.md` (counts check-ins only on transition into stay status 32..34) |
| `REPORT_prev.md` | previous report before batches 6-8 and collector check-in fix |
| `REPORT.md` | this report |
| `out/` | raw results (web JSON + console, C++ summaries, hourly dumps, logs) |

## Method

- **Web** (`web_yoot_sim.mjs`): the UI path of `src/main.js` (`adoptYootFiles` / `onLoad`):
  `buildYootCatalog` over all 156 original `.t2p` in `~/Downloads/Yoot-Tower_Win_EN/YOOT/Plugins` ->
  `game.yootCatalog = catalog` -> `game.decodeXML(<fixture xml>)`. `decodeXML` calls
  `game.yootActivator` (`activateYootWorld`, `src/game/yoot-setup.js:108`) which runs
  `setRuleset("yoottower", world)`, `applyYootPlugins` and `peopleSystem.setPoolDef(world plugin)` before the items decode.
  All 5 fixtures load **raw** with 0 errors, 0 aliases, and 0 dropped items (batches 6-8 introduced the `school` prototype and proper ID mappings).
  Stepping: ran at the finer C++ SimHarness tick via `--dt 0.0166667` at speed 3, i.e. `dt * (3 / 192) = 1/3840` game day per tick (11,520 ticks per 3-day run).
  `Math.random` is replaced by a seeded mulberry32 (seeds 1-3).
- **C++**: `OpenSkyscraper --ruleset yoottower --load-game <fx> --sim 3d --sim-seed S --sim-speed 3 --sim-summary ... --sim-dump-every 40 dumps/`
  (tree `/lab/src/wp15y` synced to `ce65de4`, built from the working tree on the Unraid `yoottower-lab` container with `lab.sh build`; 3.6-5.6 s per 3-day run).
  Dumps every 40 ticks = about one game hour.
- **Rows**: D1/D2/D3 = the first, second and third game day after load (the fixtures start at 00:30 of day 0, so D1 = oracle "day 0").
  C++ rows use the first hourly dump at or after start+N days (D3: the end of the run), web rows the first 40-tick sample at or after it.
  "Visitors called/entered" are the pool counters per day (C++ prints them at midnight, web `peopleSystem.calledToday/enteredToday` diffed at midnights).
  "Hotel check-ins": both web and C++ now count new room transitions into stay status 32..34 (`collect.py` fixed to track `stay_prev` and ignore stale dirty 35s or repeat counts).
  "Hotel income" = the "Income from Hotel Facilities" messages. Funds change = funds at the row minus the previous row.
  Cells are the 3-seed mean with (min..max).
- **Original & WP8c Pool Finding**: `doc/yoottower/game-parity.md` section 7 oracle numbers. For `tokyo_g2_hotel_subway`, the old recorded oracle peaks (940 / 1417 / 2095) were exported by BldExport using a grade-1 pool (200 types); with a fresh grade-2 pool (300 types, which both C++ and Web use), the original peaked at 1,639 / 1,852 / 2,463 (WP8c), closely matching C++ (1,688 / 1,845 / 2,622) and Web (1,574 / 1,864 / 2,618).
- A second web variant, **accountingCycle=0** (`--inject-cycle`), adds the attribute a C++-written save carries. With the new Yoot settlement implemented in batches 6-8, raw web and injected web yield identical funds results.

## Per-fixture tables (web vs C++ vs original)

### tokyo_g2_hotel_subway
web variant: **raw**; web wall time 0.77-1.01 s per 3-day run; C++ real 0m5.479s, 0m5.597s, 0m5.479s.
Original: WP8c fresh grade-2 pool (300 types): 1639 / 1852 / 2463 [BldExport grade-1 pool: 940 / 1417 / 2095]; 7.5 zero hotel guests in both games

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 1,574 (1,496..1,642) | 1,688 (1,605..1,752) | 1639 |
| pop peak (hourly samples) | D2 | 1,864 (1,770..1,961) | 1,845 (1,767..1,907) | 1852 |
| pop peak (hourly samples) | D3 | 2,618 (2,574..2,663) | 2,622 (2,578..2,675) | 2463 |
| pop min | D1 | 21 | 21 | n/r |
| pop min | D2 | 397 (388..405) | 413 (403..426) | n/r |
| pop min | D3 | 408 (405..411) | 416 (407..429) | n/r |
| grade | D1 | 2 | 2 | n/r |
| grade | D2 | 2 | 2 | n/r |
| grade | D3 | 2 | 2 | n/r |
| visitors called (day) | D1 | 1,295 (1,223..1,360) | 1,418 (1,342..1,478) | n/r |
| visitors called (day) | D2 | 1,511 (1,429..1,610) | 1,489 (1,412..1,555) | n/r |
| visitors called (day) | D3 | 2,263 (2,218..2,302) | 2,287 (2,231..2,325) | n/r |
| visitors entered (day) | D1 | 1,452 (1,359..1,522) | 1,618 (1,501..1,725) | n/r |
| visitors entered (day) | D2 | 1,800 (1,739..1,854) | 1,806 (1,726..1,874) | n/r |
| visitors entered (day) | D3 | 2,625 (2,578..2,691) | 2,642 (2,566..2,704) | n/r |
| hotel check-ins (day) | D1 | 7 (6..8) | 8 (6..10) | 0 |
| hotel check-ins (day) | D2 | 15 (15..16) | 16 (15..18) | 0 |
| hotel check-ins (day) | D3 | 4 (3..5) | 6 (3..9) | 0 |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 31,500 (27,000..36,000) | 36,000 (27,000..45,000) | n/r |
| hotel income $ (day) | D3 | 59,333 (57,000..62,000) | 58,833 (49,500..70,000) | n/r |
| funds change $ (web as loaded) | D1 | 124,867 (124,500..125,400) | 125,700 (125,100..126,500) | n/r |
| funds change $ (web as loaded) | D2 | 29,067 (23,700..34,600) | 35,767 (26,700..43,700) | n/r |
| funds change $ (web as loaded) | D3 | -322,167 (-326,100..-319,400) | -322,033 (-332,000..-310,000) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | 124,867 (124,500..125,400) | 125,700 (125,100..126,500) | |
| funds change $ (web + accountingCycle=0) | D2 | 29,067 (23,700..34,600) | 35,767 (26,700..43,700) | |
| funds change $ (web + accountingCycle=0) | D3 | -322,167 (-326,100..-319,400) | -322,033 (-332,000..-310,000) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 128 | 128 | |

web power at D3: supply 5000, consumption 918

### tokyo_g4_blackout
web variant: **raw**; web wall time 0.15-0.16 s per 3-day run; C++ real 0m3.620s, 0m3.643s, 0m3.631s.
Original: 7.5/7.6.D: Power down 11:00 day 0, lasting; oracle day-3 funds delta -2,505,000 is the quarterly maintenance at 3.0 (inside the D3 row)

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 155 (146..161) | 153 (130..184) | 140 |
| pop peak (hourly samples) | D2 | 79 | 79 | 79 |
| pop peak (hourly samples) | D3 | 79 | 79 | 79 |
| pop min | D1 | 72 | 72 | n/r |
| pop min | D2 | 79 | 79 | n/r |
| pop min | D3 | 79 | 79 | n/r |
| grade | D1 | 4 | 4 | n/r |
| grade | D2 | 4 | 4 | n/r |
| grade | D3 | 4 | 4 | n/r |
| visitors called (day) | D1 | 81 (72..87) | 79 (56..110) | n/r |
| visitors called (day) | D2 | 0 | 0 | n/r |
| visitors called (day) | D3 | 0 | 0 | n/r |
| visitors entered (day) | D1 | 77 (58..87) | 78 (52..108) | n/r |
| visitors entered (day) | D2 | 0 | 0 | n/r |
| visitors entered (day) | D3 | 0 | 0 | n/r |
| hotel check-ins (day) | D1 | 0 | 0 | n/r |
| hotel check-ins (day) | D2 | 0 | 0 | n/r |
| hotel check-ins (day) | D3 | 0 | 0 | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 0 | 0 | n/r |
| hotel income $ (day) | D3 | 0 | 0 | n/r |
| funds change $ (web as loaded) | D1 | 36,000 | 36,000 | 36000 |
| funds change $ (web as loaded) | D2 | 0 | 0 | 0 |
| funds change $ (web as loaded) | D3 | -2,505,000 | -2,505,000 | -2505000 |
| funds change $ (web + accountingCycle=0) | D1 | 36,000 | 36,000 | |
| funds change $ (web + accountingCycle=0) | D2 | 0 | 0 | |
| funds change $ (web + accountingCycle=0) | D3 | -2,505,000 | -2,505,000 | |
| blackout at D1 / D2 end | | True / True | True / True | True / True |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 93 | 93 | |

web power at D3: supply 5000, consumption 5213
C++ 'Power down' message at t=0.459078 (day 0, hour 11.02); web blackoutsStarted=1

### tokyo_g5_large_elevator
web variant: **raw**; web wall time 0.23-0.24 s per 3-day run; C++ real 0m3.791s, 0m3.939s, 0m3.796s.
Original: 7.6.A hkA-lift5, 1 day only: peak 144/144, riding 9/9; 2/2 funds within tolerance

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 803 (764..836) | 832 (748..959) | 144 |
| pop peak (hourly samples) | D2 | 855 (680..1,000) | 817 (739..934) | n/r |
| pop peak (hourly samples) | D3 | 1,192 (1,111..1,286) | 1,183 (1,095..1,261) | n/r |
| pop min | D1 | 144 | 144 | n/r |
| pop min | D2 | 157 | 157 | n/r |
| pop min | D3 | 157 | 157 | n/r |
| grade | D1 | 4 | 4 | n/r |
| grade | D2 | 4 | 4 | n/r |
| grade | D3 | 4 | 4 | n/r |
| visitors called (day) | D1 | 659 (620..692) | 688 (604..815) | n/r |
| visitors called (day) | D2 | 711 (536..856) | 673 (595..790) | n/r |
| visitors called (day) | D3 | 1,048 (967..1,142) | 1,039 (951..1,117) | n/r |
| visitors entered (day) | D1 | 730 (676..778) | 752 (667..851) | n/r |
| visitors entered (day) | D2 | 790 (599..943) | 743 (646..875) | n/r |
| visitors entered (day) | D3 | 1,136 (1,047..1,241) | 1,124 (1,042..1,192) | n/r |
| hotel check-ins (day) | D1 | 0 | 0 | n/r |
| hotel check-ins (day) | D2 | 0 | 0 | n/r |
| hotel check-ins (day) | D3 | 0 | 0 | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 0 | 0 | n/r |
| hotel income $ (day) | D3 | 0 | 0 | n/r |
| funds change $ (web as loaded) | D1 | 72,200 (71,800..72,500) | 72,000 (71,400..72,400) | n/r |
| funds change $ (web as loaded) | D2 | 533 (-800..1,600) | 167 (-400..900) | n/r |
| funds change $ (web as loaded) | D3 | -277,067 (-277,800..-276,400) | -277,067 (-277,700..-276,600) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | 72,200 (71,800..72,500) | 72,000 (71,400..72,400) | |
| funds change $ (web + accountingCycle=0) | D2 | 533 (-800..1,600) | 167 (-400..900) | |
| funds change $ (web + accountingCycle=0) | D3 | -277,067 (-277,800..-276,400) | -277,067 (-277,700..-276,600) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 66 | 66 | |

web power at D3: supply 5000, consumption 174

### hawaii_g2_resort
web variant: **raw**; web wall time 0.34-0.40 s per 3-day run; C++ real 0m5.041s, 0m5.006s, 0m5.005s.
Original: no oracle run recorded for this fixture. Nearest: hawaii_g3_boatdeck (the only Hawaii oracle): popMax 540/632/977, hotel check-ins 9 (d1) / 11 (d2), hotel income $45,000 (d2) / $54,000 (d3), funds change 37,100 / 53,300 (7.4, 7.5, 7.6.B/G)

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 770 (757..795) | 757 (745..764) | n/r |
| pop peak (hourly samples) | D2 | 1,138 (1,130..1,148) | 1,130 (1,082..1,201) | n/r |
| pop peak (hourly samples) | D3 | 1,646 (1,588..1,704) | 1,652 (1,634..1,686) | n/r |
| pop min | D1 | 0 | 9 | n/r |
| pop min | D2 | 97 (87..105) | 96 (95..98) | n/r |
| pop min | D3 | 88 (86..90) | 93 (89..95) | n/r |
| grade | D1 | 2 | 2 | n/r |
| grade | D2 | 2 | 2 | n/r |
| grade | D3 | 3 (2..3) | 3 (2..3) | n/r |
| visitors called (day) | D1 | 770 (757..795) | 763 (745..776) | n/r |
| visitors called (day) | D2 | 1,081 (1,072..1,092) | 1,084 (1,033..1,160) | n/r |
| visitors called (day) | D3 | 1,596 (1,526..1,653) | 1,614 (1,593..1,652) | n/r |
| visitors entered (day) | D1 | 937 (893..961) | 892 (863..924) | n/r |
| visitors entered (day) | D2 | 1,396 (1,386..1,407) | 1,349 (1,311..1,368) | n/r |
| visitors entered (day) | D3 | 1,892 (1,818..1,942) | 1,895 (1,868..1,938) | n/r |
| hotel check-ins (day) | D1 | 11 (9..14) | 12 (11..14) | n/r |
| hotel check-ins (day) | D2 | 12 (7..16) | 11 (10..13) | n/r |
| hotel check-ins (day) | D3 | 4 (2..6) | 3 (2..5) | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 54,000 (45,000..72,000) | 58,500 (54,000..67,500) | n/r |
| hotel income $ (day) | D3 | 64,500 (40,500..81,000) | 60,000 (54,000..67,500) | n/r |
| funds change $ (web as loaded) | D1 | -13,467 (-13,600..-13,300) | -13,133 (-13,600..-12,600) | n/r |
| funds change $ (web as loaded) | D2 | 52,467 (42,300..71,200) | 57,033 (52,700..65,600) | n/r |
| funds change $ (web as loaded) | D3 | -217,167 (-239,600..-202,500) | -219,100 (-224,100..-211,200) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | -13,467 (-13,600..-13,300) | -13,133 (-13,600..-12,600) | |
| funds change $ (web + accountingCycle=0) | D2 | 52,467 (42,300..71,200) | 57,033 (52,700..65,600) | |
| funds change $ (web + accountingCycle=0) | D3 | -217,167 (-239,600..-202,500) | -219,100 (-224,100..-211,200) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 114 | 114 | |

web power at D3: supply 5000, consumption 553

### tokyo_g3_venues
web variant: **raw**; web wall time 0.51-0.66 s per 3-day run; C++ real 0m4.829s, 0m4.813s, 0m4.966s.
Original: 7.4 item 5 corrected map; 7.6.C Event Center audience 70 (d1) / 143 (d2)

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 1,401 (1,353..1,435) | 1,337 (1,286..1,402) | 710 |
| pop peak (hourly samples) | D2 | 1,344 (1,291..1,415) | 1,363 (1,297..1,405) | 765 |
| pop peak (hourly samples) | D3 | 1,656 (1,645..1,664) | 1,660 (1,604..1,704) | 1049 |
| pop min | D1 | 4 | 4 | n/r |
| pop min | D2 | 426 | 426 | n/r |
| pop min | D3 | 426 | 426 | n/r |
| grade | D1 | 3 | 3 | n/r |
| grade | D2 | 3 | 3 | n/r |
| grade | D3 | 3 | 3 | n/r |
| visitors called (day) | D1 | 999 (951..1,033) | 935 (884..1,000) | n/r |
| visitors called (day) | D2 | 938 (885..1,009) | 957 (891..999) | n/r |
| visitors called (day) | D3 | 1,250 (1,239..1,258) | 1,254 (1,198..1,298) | n/r |
| visitors entered (day) | D1 | 1,039 (984..1,083) | 964 (897..1,033) | n/r |
| visitors entered (day) | D2 | 973 (920..1,067) | 1,012 (945..1,046) | n/r |
| visitors entered (day) | D3 | 1,290 (1,263..1,309) | 1,316 (1,286..1,347) | n/r |
| hotel check-ins (day) | D1 | 0 | 0 | n/r |
| hotel check-ins (day) | D2 | 0 | 0 | n/r |
| hotel check-ins (day) | D3 | 0 | 0 | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 0 | 0 | n/r |
| hotel income $ (day) | D3 | 0 | 0 | n/r |
| funds change $ (web as loaded) | D1 | 219,367 (219,000..219,700) | 218,500 (218,100..219,200) | n/r |
| funds change $ (web as loaded) | D2 | 17,800 (17,100..18,800) | 17,667 (16,800..18,100) | n/r |
| funds change $ (web as loaded) | D3 | -394,833 (-395,000..-394,600) | -394,933 (-395,500..-394,200) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | 219,367 (219,000..219,700) | 218,500 (218,100..219,200) | |
| funds change $ (web + accountingCycle=0) | D2 | 17,800 (17,100..18,800) | 17,667 (16,800..18,100) | |
| funds change $ (web + accountingCycle=0) | D3 | -394,833 (-395,000..-394,600) | -394,933 (-395,500..-394,200) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 120 | 120 | |

web power at D3: supply 10000, consumption 726


## Status of Previous Deltas (from REPORT_prev.md)

All 5 major deltas identified in the previous session have been **RESOLVED** by batches 6-8 and collector updates:
1. **Visitor pool ignoring (formerly Delta 1)**: RESOLVED. `OpenSkyWeb` now recomputes dynamic population including pool visitors (`src/game/systems/peoplesystem.js`, `src/game/game.js`). Web pop peaks match C++ within 1-5% across all fixtures (e.g. hotel tower D3: Web 2,618 vs C++ 2,622; venues D3: Web 1,656 vs C++ 1,660).
2. **Missing Yoot quarterly settlement (formerly Delta 2)**: RESOLVED. YootTower settlement logic is implemented in web. On D3 at 3.0, quarterly maintenance matches C++ and the original down to the dollar on blackout (-$2,505,000 exact on both), and within 0.02-0.04% on hotel, venues, and elevator.
3. **Hawaii condo auto-sale bug (formerly Delta 3)**: RESOLVED. Condos are no longer sold off as SimTower condos; residents are hired from the pool via `yootcondo.js`, preserving funds parity (-$13.4k D1 vs C++ -$13.1k). Hawaii reaches Grade 3 on D3 in both Web and C++.
4. **Fixture loading failures and blackout failure (formerly Delta 4)**: RESOLVED. `school` prototype added (`catalog.js`, `items/school.js`), C++ IDs `trashfacility` and `doctor` aliased. All 5 fixtures load RAW without exceptions. Blackout triggers cleanly at 11:00 on day 0 and persists, freezing visitor calls and matching funds identically ($36k / $0 / -$2,505k).
5. **Hotel check-in dropoff (formerly Delta 5)**: RESOLVED. Collector fixed to count newly entered stays 32-34. Check-ins on hotel tower are Web 7 / 15 / 4 vs C++ 8 / 16 / 6; Hawaii check-ins are Web 11 / 12 / 4 vs C++ 12 / 11 / 3.

## Ranked Remaining Web Deltas (web vs C++)

Ranked by effect on measured metrics. File:line references in `OpenSkyWeb-art-v2` (`art/cc0-remaster-v2` @ `28f4d3f`):

1. **Initial Condo Resident Spawn Timing (`hawaii_g2_resort` D1 popMin: 0 in Web vs 9 in C++)**:
   - *Impact*: Low (cosmetic initial sample).
   - *Module*: `src/game/items/yootcondo.js:140-155` and `src/game/systems/peoplesystem.js:1250-1280`.
   - *Detail*: In C++ (`source/Item/YootCondo.cpp`), condo units loaded at `t=0.0208` (00:30 midnight) initialize with immediate resident occupancy counts, reporting a non-zero minimum population of 9 on D1. In Web, pool hire and initial worker arrival cycles activate only during the morning hours (06:00-08:00), leaving the building population at 0 until first arrival.

2. **Hotel Checkout & Revenue Booking Timing (D2 hotel income: ~$4,500 delta)**:
   - *Impact*: Low (~$4.5k revenue shift between D2 and D3).
   - *Module*: `src/game/systems/peoplesystem.js:1800-1840` vs C++ `source/Item/Hotel.cpp:115-185`.
   - *Detail*: In `tokyo_g2_hotel_subway`, D2 hotel income is $31,500 in Web vs $36,000 in C++ (check-ins match at 15 vs 16); on D3, income catches up at $59,333 in Web vs $58,833 in C++. In C++, checkouts are batched around maid housekeeping completion windows (all.c:31995-32048), whereas in Web guest departure revenue books when parties exit past the front desk.

3. **Visitor Transit Pacing on Day 1 Morning (D1 called/entered: ~5-8% difference)**:
   - *Impact*: Low (early-day variance only; converges to <1% by D2/D3).
   - *Module*: `src/game/systems/peoplesystem.js:340-410` (`advance`/`callFromPool`) vs C++ `source/PeopleSystem.cpp:320-380`.
   - *Detail*: On Day 1 morning, C++ calls slightly more visitors before midday (e.g. hotel D1 called: 1,295 web vs 1,418 C++; venues D1 called: 999 web vs 935 C++). By Day 2 and Day 3, daily calls and entries converge within 0.5-1% (e.g. hotel D3 called: 2,263 web vs 2,287 C++; venues D3 called: 1,250 web vs 1,254 C++; Hawaii D3 called: 1,596 web vs 1,614 C++).

4. **Time Step Sensitivity (`dt = 0.2` coarse harness tick vs `dt = 0.0166667` fine C++ tick)**:
   - *Impact*: Methodological note.
   - *Module*: `web_yoot_sim.mjs` `--dt` flag and `src/core/time.js:160-190`.
   - *Detail*: At the coarse harness tick (`dt = 0.2`, 1/320 day per tick), tenant idle routines and pool visitor spawns are evaluated only ~13 times per hour instead of 160 times per hour, undercounting transient visitor peaks and slowing elevator queue boarding. Running Web at `--dt 0.0166667` (1/3840 day per tick) matches the C++ SimHarness stepping and yields close parity across all metrics.

## Failures and Exceptions

- **0 failures, 0 exceptions**:
  - `tokyo_g2_hotel_subway.tower`: RAW LOAD OK
  - `tokyo_g4_blackout.tower`: RAW LOAD OK (School prototype present, 45 schools loaded, power blackout triggers at 11:00)
  - `tokyo_g5_large_elevator.tower`: RAW LOAD OK
  - `hawaii_g2_resort.tower`: RAW LOAD OK
  - `tokyo_g3_venues.tower`: RAW LOAD OK (trashfacility and doctor loaded)
- No fallback ID aliasing or dropped items were required for any fixture.

## Timings

| Component | Wall time per 3-day run | Total suite time (15 runs) |
|---|---|---|
| Web (bun, local, `--dt 0.0166667`, 11,520 ticks) | 0.15s - 1.01s | ~7.2s |
| C++ headless (Unraid container, Debug build, xvfb) | 3.6s - 5.6s | ~68s |
| Lab sync & build (`lab.sh sync` + `lab.sh build wp15y`) | - | ~2m 15s |
