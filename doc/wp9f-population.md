WP9f — `art/wp9f-hotel-pop`. Proven category timing corrections implemented; **the requested oracle population-curve match is not achieved**. No commit, push, branch switch, or local compilation.

The supplied series do not compare the same game dates or clocks. SCHMITT oracle index 1 is `1st WD/3Q/13th Year` (`shot_d1_h19.png`), while C++ index 1 is still day 1 (2nd WD), Q2, year 13, 23:29. At index 31 the oracle is already 2nd WD/Q3 while C++ is 1st WD/Q3, 10:42. BARKLE4D oracle index 1 is 1st WD/Q1 while C++ is WE/Q4, 16:08; index 31 is 2nd WD/Q1 versus C++ 1st WD/Q1, 22:37. Oracle JSON contains HUD population and date, **no clock, frame or CountT categories**. Its OCR year also drops a digit relative to the TDT/C++ year. Consequently an index labelled 04:00 cannot establish a 04:00 population update. Clock/calendar-aligned original telemetry is needed to measure an hourly rule delta.

EXE update census (Ghidra segment addresses; subtract 8 for Concilia):

- HUD `1120:0381` reads `c13a` directly, all.c:17310. `1068:07f7` adds a signed delta to category `c112+4*i` and total `c13a`, setting dirty word `7f8a` (6422–6437). `1068:07b3` subtracts the whole category, zeros it and sets that dirty word (6404–6416). No delayed full population recount was found.
- Mapper `1068:08be`, 6485–6530: office/single/twin/suite/shop/FF/restaurant/party/cinema/condo = categories 0–9. Office let/vacate adds/subtracts 6 (`1180:0d2e/0db9`, 20678/20714); condo move-in adds 3 (`1180:105d–1101`, raw `/tmp/audit/allseg_1180.txt`, omitted from all.c), vacate subtracts 3 (`1180:1102`, 20748). These are tenant transitions, not scheduled recounts.
- Hotel first arrival `1180:0e72`, add at `0f16`, books 1/2/2. Last departure calls `1180:0f24`, subtract at `104f`, immediately debiting the same amount. Bodies are missing between all.c:20716–20719; recovered bytes and relocation targets are in `/tmp/audit/wp9b/hotel-recovered.asm.txt`. `1228:2c33–2c60` uses period 0 (07–12) for the 1/12 departure roll, waits in period 6, and otherwise permits departure; `30fc–311e` tests the final occupancy before checkout. Handler owners: all.c:31970–32058; positive member dispatch: 31407–31412. These paths contain no WE exception. Existing WP9b already removed blanket SimTower 05:00 checkout.
- At ft **240 / 10:00**, `1208:0196` calls `11b0:0184` and `1188:05af` (28876–28878). The former unconditionally clears FF and shop (23506–23507), then `11b0:02f2` rotates yesterday's walk-ins and adds them (23607–23614). The latter unconditionally clears **both** cinema and party (21102–21103), then adds the cinema quota sum or **0+50 per party hall** (21108–21123). No hall need be present for the reset itself to happen.
- At ft **1600 / 17:00**, `11b0:0250` runs (28906–28907), clears restaurant (23544) and rotates/adds its visitors through the same `02f2`. Door opening/closing and today's arrivals do not recount population. Shop re-let adds 10 (20770), with no vacate subtraction in `1180:1255` (20779–20800); retail initialization can seed 10 (23793–23802).
- Demolition `1200:369d` subtracts hosted hotel 1/2/2, office 6 and condo 3 (28199–28236); retail/entertainment persist until their table rebuild. This already follows from the C++ item contribution overrides. At ft **2533 / 04:59**, quarterly `1068:003a` clears income/expense columns, not population (6358–6376; scheduler 28825–28834). At ft **2300 / midnight**, the calendar increments (28942–28947); no year/quarter population clear appears there. Hypotheses (a), (b) and (d) are unsupported; (c) has a proven party-hall timing error and missing scheduler-owned empty-category resets.

Rule changes: `Game::advanceSimTick` now clears shop/FF/party/cinema at 10:00 and restaurant at 17:00 before item updates, even with empty categories. Per-item day markers prevent a second clear. `PartyHall::advance` books its 50 at 10:00; 13:00 opens doors without adding population. `CountT::reset` now marks the count dirty. Yoot behavior is preserved.

Synthetic remote engine tests hand-sum all ten categories: **333 → 192 at 10:00 → 192 at 13:00 → 175 at 17:00**, on WD, WE and Q1/year rollover. Empty/deleted categories give **333 → 44 → 14** at 10:00/17:00. Midnight/settlement preserve all ten counts. Hotel tests exercise WD and WE: no dawn debit, retain twin 2 until the last departure, debit/pay exactly once.

Replays used the requested `play.sh sync ... wp9f`, `build wp9f`, and `PLAY_CPP_DUMP_EVERY=40 play.sh cpp wp9f data/{SCHMITT,BARKLE4D}.TDT 3`, then `aggregate_cpp_hourly.py` on `day3_dumps`. Builds and simulations ran remotely on Unraid, with the production pathfinder unchanged. Both runs completed. All 72 population samples **and all ten category samples are identical to the supplied C++ baseline**. The proven reset corrections therefore do not explain the supplied gaps.

Raw index comparisons against the oracle, indices 1–72, before → after:

- SCHMITT: mean absolute delta **581.25 → 581.25** people (**3.7537% → 3.7537%**); worst index **20**, **−1,119 / −6.9344%**, unchanged. Three-day checkpoints: 15,537 / 16,375 / 15,502.
- BARKLE4D: mean absolute delta **368.3333 → 368.3333** people (**6.2693% → 6.2693%**); worst tied indices **31–35**, **+1,002 / +18.3819%**, unchanged. Checkpoints: 6,212 / 5,299 / 6,053.

These are reproducible index deltas, not clock-aligned hourly errors. A further concrete hotel limitation remains: `TdtLoader::walkTail` skips the saved people records (source/TdtLoader.cpp:342–345), so imported hosted rooms have no guest objects and settle at their first eligible morning visit. The EXE preserves those records (all.c:12158–12164). Restoring saved guest states/trips needs a separate faithful import change; inventing a WE retention rule or tuning against these misaligned HUD indices would not establish parity.

Web mirror, inspected read-only in `/home/tim/Projects/OpenSkyWeb-art-v2/src`:

- `game/game.js::advance`: its existing 10:00 reset of categories 4/5 must also reset **7/8**, before item updates, even when empty. Preserve the SimTower/emergency gate. `runHotelEveningPass` already resets 6 at 17:00 and opens restaurants; retain it.
- `game/items/partyhall.js::advance`: move category 7's reset/add-50 from 13:00 to 10:00 for SimTower. Keep door/visitor work at 13:00 and Yoot's behavior.
- `game/items/cinema.js::advance`, `{fastfood,restaurant}.js::openForDay`, `retail.js::advance`: keep reset-once followed by each rotated/quota contribution, at 10:00/17:00. They share the scheduler's marker; do not debit population on closing.
- `game/game.js::{addPopulation,resetCategory}` already update total incrementally and mark the UI dirty. Retain this; no dawn/global recount. `hotel.js::{advance,checkout}` must retain the debit only at last departure, with identical WD/WE rules; no new checkout exemption or delayed debit is supported. Saved guest import remains a separate issue in both ports.

Changed files: `source/Game.{cpp,h}`, `source/Item/PartyHall.cpp`, `source/TenantRules.h`, `tests/test_main.cpp`, `tools/playthrough/play.sh` (optional dump interval), this report. Modified files retain LF. Temporary TDT copies were used only for replay. Evidence, raw dumps, aggregates, comparison metrics, run/build logs and gate output are under `tools/playthrough/out/wp9f/`.

Gate: `RB_SRC=/home/tim/Projects/OpenSky-art-v2-wp9f bash /tmp/audit/remote/rb-cpp.sh 2>&1 | tail -8`

Exact final gate tail:

```text
[7/9] Linking CXX executable OpenSkyTransportTests
[9/9] Linking CXX executable OpenSkyTests
BUILD_EXIT=0
All tests passed
TEST_EXIT=0
TRANSPORT_TEST_EXIT=0
Batch 2 C++ scenarios: 2766 checks passed
DISASTER_TEST_EXIT=0
```

`git -c core.whitespace=cr-at-eol diff --check` and `bash -n tools/playthrough/play.sh` pass.
