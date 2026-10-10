# WP15 - YootTower: web port vs C++ port vs original (C++ parity fixtures)

Subject: `/home/tim/Projects/OpenSkyWeb-art-v2` @ `b277912` (art/cc0-remaster-v2) against
`/home/tim/Projects/OpenSky-art-v2` @ `db55693` (art/cc0-remaster-v2, YootTower campaign merged),
fixtures `tests/parity/{tokyo_g2_hotel_subway,tokyo_g4_blackout,tokyo_g5_large_elevator,hawaii_g2_resort,tokyo_g3_venues}.tower`,
3 game days each, seeds 1-3. Nothing in either repo was modified or committed.

## Files

| file | purpose |
|---|---|
| `web_yoot_sim.mjs` | web runner (bun). `bun web_yoot_sim.mjs [--seed S] [--days N] [--inject-cycle] [--out f.json] fx.tower...` |
| `run_cpp.sh` | C++ runner via the Unraid lab (`lab.sh sync/build/exec`): `sync-build`, `run`, `pull` |
| `collect.py`, `tables.py` | fold `out/web_seed*.json`, `out/webcyc_seed*.json`, `out/cpp/<fx>/s<N>/` into `out/metrics.json`, `out/tables.md` |
| `out/` | raw results (web JSON + console, C++ summaries, hourly dumps, logs) |

## Method

- **Web** (`web_yoot_sim.mjs`): the UI path of `src/main.js` (`adoptYootFiles` / `onLoad`):
  `buildYootCatalog` over all 156 original `.t2p` in `~/Downloads/Yoot-Tower_Win_EN/YOOT/Plugins` ->
  `game.yootCatalog = catalog` -> `game.decodeXML(<fixture xml>)`. `decodeXML` calls
  `game.yootActivator` (`activateYootWorld`, `src/game/yoot-setup.js:108`) which runs
  `setRuleset("yoottower", world)`, `applyYootPlugins` and `peopleSystem.setPoolDef(world plugin)` before the items decode. No XML
  pre-processing is needed (the C++ `.tower` loads as is, hash attribute included) except the id aliasing described under Failures.
  Stepping: `game.advance(0.2)` at speed 3, i.e. the C++ harness tick (dt 1/60), 2894 ticks per 3 days (the speed ramp is the same
  as C++'s `setSpeedMode`). `Math.random` is replaced by a seeded mulberry32 (seeds 1-3); the web RNG is therefore not bit-identical to C++'s `rand()`.
- **C++**: `OpenSkyscraper --ruleset yoottower --load-game <fx> --sim 3d --sim-seed S --sim-speed 3 --sim-summary ... --sim-dump-every 40 dumps/`
  (tree `/lab/src/wp15y`, built from the working tree on the Unraid `yoottower-lab` container with `lab.sh build`, which is the lab's default Debug build; 3.6-5.6 s per 3-day run). 40 ticks = about one game hour, so
  every dump gives an hourly `population` (also the source of "pop peak" and "pop min") plus the saved tower (blackout flag, hotel stay records).
- **Rows**: D1/D2/D3 = the first, second and third game day after load (the fixtures start at 00:30 of day 0, so D1 = oracle "day 0").
  C++ rows use the first hourly dump at or after start+N days (D3: the end of the run), web rows the first 40-tick sample at or after it.
  "Visitors called/entered" are the pool counters per day (C++ prints them at midnight, web `peopleSystem.calledToday/enteredToday` diffed at the midnights).
  "Hotel check-ins": web = new `peopleSystem.stays`; C++ = distinct rooms with a `yootStayStatus` in that day's hourly dumps (approximate, undercounts nothing longer than 1 h).
  "Hotel income" = the "Income from Hotel Facilities" messages. Funds change = funds at the row minus the previous row.
  Cells are the 3-seed mean with (min..max).
- **Original**: `doc/yoottower/game-parity.md` section 7 oracle numbers only; `n/r` = not recorded. The oracle numbers
  come from a `.bld` export of these towers (population peak per day, 7.4 item 5 corrected-map run), so they are indicative, not an identical-input run.
- A second web variant, **accountingCycle=0** (`--inject-cycle`), adds the attribute a C++-written save carries; see delta 2.

## Per-fixture tables (web vs C++ vs original)

### tokyo_g2_hotel_subway
web variant: **raw**; web wall time 0.33-0.47 s per 3-day run; C++ real 0m5.472s, 0m5.541s, 0m5.593s.
Original: 7.4 item 5 corrected map; 7.5 zero hotel guests in both games

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 282 | 1,753 (1,655..1,882) | 940 |
| pop peak (hourly samples) | D2 | 282 | 2,099 (2,063..2,160) | 1417 |
| pop peak (hourly samples) | D3 | 282 | 2,533 (2,439..2,592) | 2095 |
| pop min | D1 | 282 | 21 | n/r |
| pop min | D2 | 282 | 411 (406..417) | n/r |
| pop min | D3 | 282 | 412 (406..418) | n/r |
| grade | D1 | 2 | 2 | n/r |
| grade | D2 | 2 | 2 | n/r |
| grade | D3 | 2 | 2 | n/r |
| visitors called (day) | D1 | 1,574 (1,548..1,608) | 1,489 (1,389..1,624) | n/r |
| visitors called (day) | D2 | 1,907 (1,872..1,956) | 1,749 (1,698..1,811) | n/r |
| visitors called (day) | D3 | 2,636 (2,502..2,704) | 2,180 (2,107..2,232) | n/r |
| visitors entered (day) | D1 | 1,652 (1,623..1,687) | 1,716 (1,612..1,872) | n/r |
| visitors entered (day) | D2 | 1,988 (1,948..2,039) | 2,103 (2,081..2,146) | n/r |
| visitors entered (day) | D3 | 2,714 (2,567..2,794) | 2,557 (2,445..2,628) | n/r |
| hotel check-ins (day) | D1 | 8 (6..10) | 8 (7..10) | 0 |
| hotel check-ins (day) | D2 | 12 (10..14) | 24 (20..26) | 0 |
| hotel check-ins (day) | D3 | 6 (4..7) | 20 (19..22) | 0 |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 36,000 (27,000..45,000) | 37,500 (31,500..45,000) | n/r |
| hotel income $ (day) | D3 | 45,333 (37,500..53,000) | 59,667 (45,500..67,500) | n/r |
| funds change $ (web as loaded) | D1 | 326,933 (326,400..327,200) | 127,133 (125,800..128,100) | n/r |
| funds change $ (web as loaded) | D2 | 39,500 (30,800..46,100) | 40,033 (33,900..47,800) | n/r |
| funds change $ (web as loaded) | D3 | 60,600 (52,100..67,200) | -319,333 (-332,500..-310,400) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | -13,067 (-13,600..-12,800) | 127,133 (125,800..128,100) | |
| funds change $ (web + accountingCycle=0) | D2 | 39,500 (30,800..46,100) | 40,033 (33,900..47,800) | |
| funds change $ (web + accountingCycle=0) | D3 | 60,600 (52,100..67,200) | -319,333 (-332,500..-310,400) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 128 | 128 | |

web power at D3: supply 5000, consumption 415

### tokyo_g4_blackout
web variant: **aliased** (raw load failed: `undefined is not an object (evaluating 'p.make')`; aliased {}, dropped {'school': 45}); web wall time 0.09-0.11 s per 3-day run; C++ real 0m3.624s, 0m3.619s, 0m3.685s.
Original: 7.5/7.6.D: Power down 11:00 day 0, lasting; oracle day-3 funds delta -2,505,000 is the quarterly maintenance at 3.0 (inside the D3 row)

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 72 | 147 (130..160) | 140 |
| pop peak (hourly samples) | D2 | 72 | 79 | 79 |
| pop peak (hourly samples) | D3 | 72 | 79 | 79 |
| pop min | D1 | 72 | 72 | n/r |
| pop min | D2 | 72 | 79 | n/r |
| pop min | D3 | 72 | 79 | n/r |
| grade | D1 | 4 | 4 | n/r |
| grade | D2 | 4 | 4 | n/r |
| grade | D3 | 4 | 4 | n/r |
| visitors called (day) | D1 | 415 (367..443) | 73 (56..86) | n/r |
| visitors called (day) | D2 | 378 (369..395) | 0 | n/r |
| visitors called (day) | D3 | 531 (509..566) | 0 | n/r |
| visitors entered (day) | D1 | 437 (385..463) | 74 (52..95) | n/r |
| visitors entered (day) | D2 | 388 (371..406) | 0 | n/r |
| visitors entered (day) | D3 | 541 (524..569) | 0 | n/r |
| hotel check-ins (day) | D1 | 0 | 0 | n/r |
| hotel check-ins (day) | D2 | 0 | 0 | n/r |
| hotel check-ins (day) | D3 | 0 | 0 | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 0 | 0 | n/r |
| hotel income $ (day) | D3 | 0 | 0 | n/r |
| funds change $ (web as loaded) | D1 | 10,567 (10,300..10,900) | 36,000 | 36000 |
| funds change $ (web as loaded) | D2 | 15,200 (15,000..15,300) | 0 | 0 |
| funds change $ (web as loaded) | D3 | -8,300 (-8,600..-8,100) | -2,505,000 | -2505000 |
| funds change $ (web + accountingCycle=0) | D1 | 30,567 (30,300..30,900) | 36,000 | |
| funds change $ (web + accountingCycle=0) | D2 | 15,200 (15,000..15,300) | 0 | |
| funds change $ (web + accountingCycle=0) | D3 | -8,300 (-8,600..-8,100) | -2,505,000 | |
| blackout at D1 / D2 end | | False / False | True / True | True / True |
| items D3 web vs C++ (differences only) | | school: 0 | school: 45 | |
| item types total D3 | | 48 | 93 | |

web power at D3: supply 5000, consumption 69
C++ 'Power down' message at t=0.459078 (day 0, hour 11.02); web blackoutsStarted=0

### tokyo_g5_large_elevator
web variant: **raw**; web wall time 0.09-0.11 s per 3-day run; C++ real 0m3.820s, 0m3.810s, 0m3.769s.
Original: 7.6.A hkA-lift5, 1 day only: peak 144/144, riding 9/9; 2/2 funds within tolerance

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 144 | 887 (809..962) | 144 |
| pop peak (hourly samples) | D2 | 144 | 933 (897..963) | n/r |
| pop peak (hourly samples) | D3 | 144 | 1,257 (1,168..1,305) | n/r |
| pop min | D1 | 144 | 144 | n/r |
| pop min | D2 | 144 | 157 | n/r |
| pop min | D3 | 144 | 157 | n/r |
| grade | D1 | 4 | 4 | n/r |
| grade | D2 | 4 | 4 | n/r |
| grade | D3 | 4 | 4 | n/r |
| visitors called (day) | D1 | 875 (755..955) | 743 (665..818) | n/r |
| visitors called (day) | D2 | 864 (810..943) | 789 (753..819) | n/r |
| visitors called (day) | D3 | 1,259 (1,176..1,358) | 1,113 (1,024..1,161) | n/r |
| visitors entered (day) | D1 | 875 (755..955) | 799 (715..885) | n/r |
| visitors entered (day) | D2 | 864 (810..943) | 866 (821..919) | n/r |
| visitors entered (day) | D3 | 1,259 (1,176..1,358) | 1,224 (1,097..1,315) | n/r |
| hotel check-ins (day) | D1 | 0 | 0 | n/r |
| hotel check-ins (day) | D2 | 0 | 0 | n/r |
| hotel check-ins (day) | D3 | 0 | 0 | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 0 | 0 | n/r |
| hotel income $ (day) | D3 | 0 | 0 | n/r |
| funds change $ (web as loaded) | D1 | -38,967 (-40,100..-38,300) | 72,667 (72,000..73,200) | n/r |
| funds change $ (web as loaded) | D2 | 1,200 (800..1,800) | 967 (600..1,200) | n/r |
| funds change $ (web as loaded) | D3 | 3,600 (3,100..4,200) | -276,300 (-277,200..-275,600) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | 1,033 (-100..1,700) | 72,667 (72,000..73,200) | |
| funds change $ (web + accountingCycle=0) | D2 | 1,200 (800..1,800) | 967 (600..1,200) | |
| funds change $ (web + accountingCycle=0) | D3 | 3,600 (3,100..4,200) | -276,300 (-277,200..-275,600) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 66 | 66 | |

web power at D3: supply 5000, consumption 126

### hawaii_g2_resort
web variant: **raw**; web wall time 0.17-0.19 s per 3-day run; C++ real 0m5.003s, 0m5.154s, 0m4.985s.
Original: no oracle run recorded for this fixture. Nearest: hawaii_g3_boatdeck (the only Hawaii oracle): popMax 540/632/977, hotel check-ins 9 (d1) / 11 (d2), hotel income $45,000 (d2) / $54,000 (d3), funds change 37,100 / 53,300 (7.4, 7.5, 7.6.B/G)

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 0 | 771 (729..796) | n/r |
| pop peak (hourly samples) | D2 | 117 | 1,192 (1,182..1,197) | n/r |
| pop peak (hourly samples) | D3 | 126 | 1,679 (1,642..1,705) | n/r |
| pop min | D1 | 0 | 9 | n/r |
| pop min | D2 | 0 | 102 (98..111) | n/r |
| pop min | D3 | 117 | 101 (93..113) | n/r |
| grade | D1 | 2 | 2 | n/r |
| grade | D2 | 2 | 2 | n/r |
| grade | D3 | 2 | 2 (2..3) | n/r |
| visitors called (day) | D1 | 755 (741..775) | 779 (741..808) | n/r |
| visitors called (day) | D2 | 1,173 (1,131..1,215) | 1,146 (1,137..1,154) | n/r |
| visitors called (day) | D3 | 1,820 (1,714..1,920) | 1,635 (1,605..1,654) | n/r |
| visitors entered (day) | D1 | 781 (771..799) | 913 (876..942) | n/r |
| visitors entered (day) | D2 | 1,219 (1,176..1,264) | 1,397 (1,379..1,406) | n/r |
| visitors entered (day) | D3 | 1,845 (1,739..1,944) | 1,928 (1,890..1,970) | n/r |
| hotel check-ins (day) | D1 | 14 (11..15) | 13 (11..15) | n/r |
| hotel check-ins (day) | D2 | 9 (7..12) | 20 (19..21) | n/r |
| hotel check-ins (day) | D3 | 3 (2..4) | 13 (9..15) | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 66,000 (49,500..76,500) | 67,500 (58,500..76,500) | n/r |
| hotel income $ (day) | D3 | 48,000 (36,000..63,000) | 37,500 (31,500..49,500) | n/r |
| funds change $ (web as loaded) | D1 | -123,433 (-123,500..-123,400) | -12,833 (-13,100..-12,600) | n/r |
| funds change $ (web as loaded) | D2 | 5,913,400 (5,896,800..5,924,300) | 67,167 (58,200..77,200) | n/r |
| funds change $ (web as loaded) | D3 | 509,267 (500,200..522,700) | -241,200 (-247,800..-229,900) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | -13,433 (-13,500..-13,400) | -12,833 (-13,100..-12,600) | |
| funds change $ (web + accountingCycle=0) | D2 | 5,913,400 (5,896,800..5,924,300) | 67,167 (58,200..77,200) | |
| funds change $ (web + accountingCycle=0) | D3 | 509,267 (500,200..522,700) | -241,200 (-247,800..-229,900) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 114 | 114 | |

web power at D3: supply 5000, consumption 376

### tokyo_g3_venues
web variant: **aliased** (raw load failed: `undefined is not an object (evaluating 'p.make')`; aliased {'trashfacility': 1, 'doctor': 1}, dropped {}); web wall time 0.19-0.20 s per 3-day run; C++ real 0m5.012s, 0m4.879s, 0m4.795s.
Original: 7.4 item 5 corrected map; 7.6.C Event Center audience 70 (d1) / 143 (d2)

| metric | day | web | C++ | original |
|---|---|---|---|---|
| pop peak (hourly samples) | D1 | 402 | 1,424 (1,366..1,468) | 710 |
| pop peak (hourly samples) | D2 | 402 | 1,442 (1,417..1,476) | 765 |
| pop peak (hourly samples) | D3 | 402 | 1,658 (1,584..1,696) | 1049 |
| pop min | D1 | 402 | 4 | n/r |
| pop min | D2 | 402 | 426 | n/r |
| pop min | D3 | 402 | 426 | n/r |
| grade | D1 | 3 | 3 | n/r |
| grade | D2 | 3 | 3 | n/r |
| grade | D3 | 3 | 3 | n/r |
| visitors called (day) | D1 | 1,135 (1,102..1,157) | 1,022 (964..1,066) | n/r |
| visitors called (day) | D2 | 1,247 (1,151..1,316) | 1,036 (1,011..1,070) | n/r |
| visitors called (day) | D3 | 1,505 (1,463..1,544) | 1,252 (1,178..1,290) | n/r |
| visitors entered (day) | D1 | 1,115 (1,072..1,139) | 1,066 (1,007..1,104) | n/r |
| visitors entered (day) | D2 | 1,239 (1,146..1,306) | 1,067 (1,021..1,110) | n/r |
| visitors entered (day) | D3 | 1,500 (1,458..1,539) | 1,298 (1,207..1,360) | n/r |
| hotel check-ins (day) | D1 | 0 | 0 | n/r |
| hotel check-ins (day) | D2 | 0 | 0 | n/r |
| hotel check-ins (day) | D3 | 0 | 0 | n/r |
| hotel income $ (day) | D1 | 0 | 0 | n/r |
| hotel income $ (day) | D2 | 0 | 0 | n/r |
| hotel income $ (day) | D3 | 0 | 0 | n/r |
| funds change $ (web as loaded) | D1 | 568,933 (568,600..569,200) | 219,467 (218,700..219,900) | n/r |
| funds change $ (web as loaded) | D2 | 20,533 (19,700..21,400) | 18,533 (18,300..18,700) | n/r |
| funds change $ (web as loaded) | D3 | -700 (-900..-400) | -394,433 (-395,100..-393,900) | n/r |
| funds change $ (web + accountingCycle=0) | D1 | 34,933 (34,600..35,200) | 219,467 (218,700..219,900) | |
| funds change $ (web + accountingCycle=0) | D2 | 20,533 (19,700..21,400) | 18,533 (18,300..18,700) | |
| funds change $ (web + accountingCycle=0) | D3 | -700 (-900..-400) | -394,433 (-395,100..-393,900) | |
| blackout at D1 / D2 end | | False / False | False / False | n/r |
| items D3 web vs C++ (differences only) | | identical | identical | |
| item types total D3 | | 120 | 120 | |

web power at D3: supply 10000, consumption 372


## Ranked web deltas (web vs C++; original where recorded)

Ranked by effect on the measured numbers. File:line are in the web tree at `b277912`; C++ references in `/home/tim/Projects/OpenSky-art-v2`.

1. **The web tower population ignores the visitor pool** (largest; affects grade progression, rating, every population comparison).
   Web `population` is constant on every fixture (282 / 72 / 144 / 0->126 / 402 = sum of item populations, tokyo_g2: 282 for the whole run)
   whereas C++ peaks at 1,753 / 2,099 / 2,533 (hotel), 1,424 / 1,442 / 1,658 (venues), 887-1,257 (elevator), 771-1,679 (Hawaii); original 940 / 1,417 / 2,095 and 710 / 765 / 1,049.
   Cause: C++ `Game.cpp:1178-1187` (`yootEconomy`: sum of `item->population` + `eventSystem.people.visitorPopulation()`); the web
   `populationNeedsUpdate` block (`src/game/game.js:422-425`) only calls `_reactToPopulation()`, nothing recomputes `population`, so
   `PeopleSystem.advance` flagging `populationNeedsUpdate` (`src/game/systems/peoplesystem.js:165`) has no effect. Consequence: the population gate of
   `yootCheckGrade` (`game.js:1108-1114`) never sees the pool: C++ Hawaii seed 3 reaches grade 3 on D3, web stays at 2/2/3/4/3 (the starting grade) on all runs.
   Caveat for the C++ column: it exceeds the last recorded OpenSky peaks of the campaign (venues 756/884/1006, hotel 990/1379/1870) by roughly 1.5-2x on these .tower
   inputs (hourly series: `out/cpp/tokyo_g3_venues/s1/dumps`, the pool count 1,042 and shrink 1,042 at 23:00), so the C++ peak itself is outside the oracle's 20% tolerance on venues and hotel; not investigated here.

2. **No YootTower quarterly settlement in the web.** C++ `settleYootQuarter` (`source/Game.cpp:187-207`, plugin `outMoney` per item) and
   `collectYootOfficeRent` (`Game.cpp:214`, 3 x inMoney per let office) have no web counterpart; the web runs the SimTower
   `settleQuarterlyAccounting` / `quarterlyMaintenanceCost` (`game.js:882-905`, `:964`) with SimTower rents ("Quarterly rent income" x47 in the hotel tower).
   Measured effects (3-seed means):
   - Funds D1: hotel +326,900 web vs +127,100 C++; venues +568,900 vs +219,500; elevator -39,000 vs +72,700; blackout +10,600 vs +36,000 (oracle +36,000).
   - Funds on the quarter boundary (inside D3): C++ -319,300 (hotel), -394,400 (venues), -276,300 (elevator), -241,200 (Hawaii), -2,505,000 (blackout; oracle -2,505,000). Web shows no charge at 3.0 at all.
   - The web charge is made early: the fixtures carry no `accountingCycle`, so the loader (`game.js:3060-3064`, `currentCycle - (hour < 5 ? 1 : 0)`) thinks a quarter is due and charges -110k..-280k at 05:00 on day 0 (message at t=0.2098 in every run). With `accountingCycle="0"` injected (webcyc rows) that artifact disappears
     (D1 funds hotel -13,100, venues +34,900, elevator +1,000, Hawaii -13,400) but D1 still lacks the C++ office rent (+72k..+219k) and D3 still lacks the maintenance.

3. **Hawaii: the web sells the 42 condos for $150,000 each** ("Income from Condo sale" x42, +$5.9M funds on D2; C++ D2 +67,200, original +53,300 on its Hawaii tower).
   Web decodes the fixture's `type="condo"` as the SimTower `Condo` (`src/game/items/condo.js:240-258` random 07-17h sale roll via `openSkyQuarterlyRent("condo", ...)`); the C++ condos stay unsold (`armed="false"`, residents come from the pool hire, ~93-113 people at D3). Also web Hawaii population is 0 on D1 and 117-126 later.

4. **Web cannot load two of the five fixtures as saved** (fallback aliases used for all numbers above on those two): see Failures. Blackout fixture: with the 45 schools dropped the web power demand is 69 units against supply 5,000
   (C++/oracle consumption 5,833), so **no blackout happens in the web** (C++ "Power down" at 11:01 on day 0 and a lasting blackout, oracle 11:00). Consequently web visitors keep being called after 11:00
   (415 / 378 / 531 per day vs C++ 73 / 0 / 0), and web D2/D3 funds are not frozen. Web has no `school` prototype at all (`src/game/items/catalog.js`), although `yoot-plugins.js:101` links `school` to `School.t2p`; `YOOT_ITEM_POWER` (`systems/powersystem.js`) therefore cannot count it.

5. **Hotel check-ins fall off after day one in the web**: subway tower 8 / 12 / 6 vs C++ 8 / 24 / 20; Hawaii 14 / 9 / 3 vs C++ 13 / 20 / 13 (original, Hawaii g3: 9 / 11). Hotel income follows on D3 for subway (45,300 vs 59,700) but not Hawaii (48,000 vs 37,500).
   Most likely the maid/cleaning loop (C++ 7.6.H: dirty rooms are never made again, so eligible rooms dry up); suspects `src/game/systems/specialsystem.js` (`housekeepingTick`) and `src/game/items/hotel.js`; not traced.

6. **Visitor calls differ modestly**: called per day web vs C++: hotel 1,574 / 1,907 / 2,636 vs 1,489 / 1,749 / 2,180 (+6 / +9 / +21%), venues 1,135 / 1,247 / 1,505 vs 1,022 / 1,036 / 1,252 (+11 / +20 / +20%), elevator 875 / 864 / 1,259 vs 743 / 789 / 1,113 (+18 / +10 / +13%), Hawaii within +/-12%.
   Entered per day is closer (hotel -4 / -5 / +6%, venues +5 / +16 / +16%, Hawaii -14 / -13 / -4%). Web calls are consistently a little higher on D3; module `src/game/systems/peoplesystem.js` (`collectCustomers`/`callFromPool`) vs `source/PeopleSystem.cpp`; seed spread is 5-15%, so only the D3 +13..21% is clearly outside noise.

7. **Matches**: item sets after load are identical on hotel, elevator, Hawaii and venues (aliasing excepted); world/ruleset/pool definition load correctly on all five (`pool=true`, Tokyo/Hawaii); grade at start matches; the large-elevator tower is the closest overall (called/entered within 10-18%; no rides in the oracle either).
   Hotel income per day is the same magnitude (D2 36,000 vs 37,500 hotel; 66,000 vs 67,500 Hawaii).

## Failures and exceptions

Raw load of two fixtures throws in the web (stack identical, `out/web_seed1.txt`):

```
tokyo_g4_blackout.tower, tokyo_g3_venues.tower: RAW LOAD FAILED undefined is not an object (evaluating 'p.make')
TypeError: undefined is not an object (evaluating 'p.make')
    at make (src/game/items/factory.js:26:18)
    at makeFromXML (src/game/items/factory.js:74:23)
    at decodeXML (src/game/game.js:3088:37)
    at runOne (web_yoot_sim.mjs)
```

- `tokyo_g3_venues.tower`: the C++ ids `trashfacility` and `doctor` are `yoot_trashfacility` / `yoot_doctor` in the web catalog (`items/catalog.js:122,129`; `WEB_PROTOTYPE_ALIAS` in `yoot-plugins.js:112-116` only aliases the plugin links, not save decoding). The runner retries with those two ids rewritten; 1 + 1 items.
- `tokyo_g4_blackout.tower`: `school` (45 items) has no web prototype. The runner retries with the 45 schools removed, which changes the tower (see delta 4); the blackout row is therefore not a like-for-like comparison.
- `tokyo_g3_reggae` and `hawaii_g3_boatdeck` (not in this package) would fail the same way (`trashfacility`/`doctor`).
- No other exceptions, no C++ failures. The web runs once emitted no warnings.

## Timings

| | per 3-day run |
|---|---|
| web (bun, local) | 0.09-0.11 s blackout/elevator, 0.17-0.20 s Hawaii/venues, 0.33-0.47 s hotel (about 1,000x real time, well under one minute) |
| C++ (Unraid container, Debug build, xvfb) | 3.6-5.6 s |
| lab build (`lab.sh build wp15y`, incremental cmake) | a few minutes |

## Not covered / caveats

- Original numbers exist only for population peaks and a few funds/hotel figures; hawaii_g2_resort has no oracle record at all, the Hawaii references are from hawaii_g3_boatdeck.
- C++ check-ins are inferred from hourly dumps; web check-ins count stay creation (booking), so the two are close but not identical definitions.
- Power consumption of the C++ run is not in the summary; the 5,833 figure is from game-parity.md 7.5.
- Web seeds use a seeded `Math.random`; the C++ `rand()` sequence is different, so only distributions (3 seeds) are comparable, not single events.
