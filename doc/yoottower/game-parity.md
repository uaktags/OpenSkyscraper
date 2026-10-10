# YootTower game-rules parity audit (`--ruleset yoottower`)

**Status:** research snapshot from 2026-10-08. Plan steps 0-13 (§4) are implemented
(2026-10-08); the rows they touch carry an **Implemented:** note in the OpenSky column.
**Scope:** the gameplay rules of the original YootTower (1999 Win32: `T2.exe` + `T2DLL.dll` +
`Plugins/*.t2p`), compared with OpenSky's `--ruleset yoottower` mode.

**Headline (before steps 0-6):** in OpenSky, `--ruleset yoottower` played exactly like `--ruleset opensky`.
The only ruleset branch was the horizontal lot check in `source/BuildRules.cpp:53-66`, and that
check applied to SimTower only. Since steps 0-6, YootTower mode reads a `RulesProfile`
(`source/BuildRules.h`) built from the world plugin (`source/YootWorld.{h,cpp}`, `--world
tokyo|hawaii|kegon`); the text below describes the pre-implementation state. YootTower mode differs from OpenSky mode only in which skins it allows
(`source/Skin.cpp:35-77`). No rule in LevelUp, JudgeSystem, EventSystem, VipSystem, the economy,
Items, PathFinder or the toolbox branches on the ruleset. Plugin resources are parsed
(`source/PluginResources.*`), but only the editor and export path reads them (`OspPlugin.cpp:616-620`);
no gameplay code does. So every YootTower rule below that differs from SimTower is
DIVERGENT or MISSING in OpenSky.

Related documents. This audit cites them and does not repeat them:
- `world-and-sp-formats.md`: the WORLDDEF, WATR, T2EV, `*EV`, PODF and PGDF byte layouts (from the world-format agent). The
  grade table, starting funds and event-header semantics used here agree with it.
- `resource-formats.md` §9–10: CFilePlugin/ATTR offsets and the `FDnT`/`FDvM` template-loading path.
- `bld-format.md`: the save format. `oracle.md` / `tools/yoottower/oracle/run_yoot.sh`: a headless Wine
  harness for checking rules against the running game. It has not been used for this audit yet; see the plan step "V".

## How to read the evidence

- **Addresses:** `0x1xxxxxxx` is `T2DLL.dll` (image base 0x10000000, Ghidra names from its mangled
  exports). `0x004xxxxx` is `T2.exe`. `Plugin.t2p 0x1000xxxx` is plugin `.text`.
- **Decompiled bodies** are saved under `~/yoottower-decomp/rules-agent/`:
  - `funcs/events.c`, `funcs/tenant.c`, `funcs/people.c`: T2DLL.
  - `funcs/plugin_overrides.c`: the plugins' own virtuals.
  - `funcs/exe_doc*.c` and `out/exe3/T2.exe.decomp.c`: T2.exe, including the T2TowerDoc functions Ghidra missed. They were recovered by a prologue scan.
  - `out/{dll,plg,plg2}/*.decomp.c`: full dumps.
  - `JOURNAL.md`: the research log.
- **Data tables** come from reproducible scripts in `tools/yoottower/rules/`:
  - `world_rules.py`: WORLDDEF, WATR and the event list.
  - `item_rules.py`: the TNDF/MVDF template defs of all 100 PE item plugins. Every resource is read to its last
    byte, which validates the constructor field order.
  - `attr_table.py`: ATTR.
  - `vtable.py` and `overrides.py`: which T2DLL virtuals each plugin replaces.
  - `dump_rsrc.py`: resources and string tables.
- **Money:** the engine stores money in units of $100. `T2WorldDef::ExchangeMoney` 0x1000ab50 multiplies by
  `+0x250`/`+0x254` = 100/1 in all three worlds. This is confirmed by StdElev price 1500, whose MVDF string is
  "$150000", and by the world description strings ("$10,000,000" for Tokyo's 100000).
- **Confidence:**
  - **H**: read directly from decompiled code or from data whose consumer is decompiled.
  - **M**: data decoded and the consumer partly traced, or the semantics inferred from consistent data across the 3 worlds.
  - **L**: hypothesis.
- **Status:** MATCH, PARTIAL, MISSING (OpenSky has no equivalent), or DIVERGENT (OpenSky implements a
  different, usually SimTower, rule).

---

## 1. Parity matrix

### 1.1 Time and calendar

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| T1 | **The clock is minute-linear.** `T2DateTime` holds year, "month" (0–11), minute (0–1439) and sub-tick (0–3). There are 4 sub-ticks per game minute, so a day is 5760 linear ticks. | `IsIncDateTime` 0x1001ca90; `CalcTotalCount` 0x10001d60 (`((m+12y)*1440+min)*4+sub`) | Uses SimTower's non-linear 2600-frame day (`Time.cpp:19-33`). **Implemented:** YootTower mode switches `Time::absoluteToHour/hourToAbsolute` to a linear 24-hour day (`Time::setLinearClock`, set by `Game::setRules`). The real-time day length and the 4-sub-tick/speed mapping are unchanged; `Item/Metro.cpp` caches SimTower durations at static init. | PARTIAL | H |
| T2 | **Calendar.** Each internal "month" is one game day. A quarter is 3 days and a year is 4 quarters (12 days). Quarter = m/3+1, day-of-quarter = m%3+1, and years start at 1. | `GetQuarter`/`GetDay` 0x10001c30/0x10001c50, `CalcTotalDays` 0x10001cc0 | Uses the same 3/4/12 structure (`Time.cpp:93-95`). **Implemented (gap closers):** the YootTower date line is T2WorldDef::DrawCalendar's "YEAR %d %s." with the month name of the day (Jan..Dec) and "WD" / red "HD" on holidays (`yootCalendarText`); the oracle shows "YEAR 1 Mar. HD" on day index 2. | MATCH | H |
| T3 | **Daytime and time zones.** "Day" runs 06:00–18:00 (`IsDay`: 360 ≤ min < 1080). There are 10 people time zones with boundaries at 2, 6, 9, 10, 12, 13, 15, 18 and 22 h, and they drive the PZDF demand tables. | `IsDay` 0x10001cf0, `GetTimeZone` 0x1001c9a0 | Has no time-zone model; spawns are hardcoded per item. | MISSING | H |
| T4 | **Holidays (weekends) are the same rule in all three worlds.** Let `d` = month index 0–11 and `Y` = year. A day is a holiday if `d==7` or `d==11`, or if `n=12Y+d-23` satisfies `n≥2 && (n-2)%5==0` (unsigned arithmetic, so it wraps in year 1). That gives fixed days 8 and 12 of each year plus every 5th day. | Tokyo.t2p 0x10014c10; Hawai.t2p 0x10017770; Kegon.t2p `WorldDef::IsHoliday` 0x10014e70. Dispatch: `T2DateTime::IsHoliday` 0x1001cc50 → world vtable +0x24. | Weekend is `day%3==2` (`Time.cpp`, `LevelUp.cpp:104-105`). **Implemented:** `Time::yootIsHoliday` is a verbatim unsigned port (year 1-based = `Time::year`, month = `Time::dayOfYear()`); all item weekend checks go through `Time::isHoliday()`, which keeps `day == 2` for SimTower/OpenSky. Test: years 1-3 vs a direct port. | MATCH | H |
| T5 | **Rain is deterministic and per world.** Tokyo rains when `(d+1)%4==0 && d/3 != 3`, which is days 4 and 8 of each year. Hawaii and Kegon never rain: they do not override `IsRainyDay`, and the base returns 0. | Tokyo.t2p 0x10014c50; base `T2WorldDef::IsRainyDay` 0x1000a6a0; override list in `overrides.py` output | Rolls `rand()%3==0` at 05:00 (`Sky.cpp:32`). **Implemented:** `Sky.cpp` uses `RulesProfile::rain`: Random (SimTower/OpenSky, unchanged), Tokyo days 4 and 8 (`Time::yootTokyoIsRainy`), never in Hawaii/Kegon. | MATCH | H |
| T6 | **When the day changes** (midnight, inside the hour-change branch), the game calls the world's `DayChanged`, `T2PeopleArrayList::DayChanged` and the cold-table refresh. On day 1 of a quarter it also calls `T2Settlement::Update` (opening the new quarter's books) and charges quarterly maintenance. | T2.exe FUN_0041dc40 (doc sim step, vtable +0xc4) | Settles the quarter at the 4:59/5:00 boundary (`Game.cpp:878-924`). **Implemented:** YootTower mode settles the quarter at midnight of day 1 (`settleYootQuarter`, plan step 7); the world `DayChanged`, people `DayChanged` and cold table are not modelled. | PARTIAL | H |

### 1.2 Tower rating (grades / stars)

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| R1 | **The grade table comes from WORLDDEF**, one row per step of `{vipMask, population, anim…}`. Steps (start grade, then star→star):<br>• **Tokyo** starts at grade 1: 1→2 needs **1000**, 2→3 **2000**, 3→4 **5000**, 4→5 **12000**.<br>• **Hawaii** starts at grade 2: 2→3 needs **500**, 3→4 **2000**, 4→5 **8000**; its row 0 is a dummy (pop 1).<br>• **Kegon** starts at grade 3: 3→4 needs **200**, 4→5 **1000**.<br>Start grades come from WORLDDEF +0x70 = 1/2/3 (`world-and-sp-formats.md` §2.2). | `T2WorldDef` ctor 0x1006e4e0; `world_rules.py` output | Uses the SimTower table 300 / 1000 / 5000 / 10000 / 15000 (`LevelUp.cpp:37-44`). **Implemented:** grade rows come from WORLDDEF via `RulesProfile::grades`; the new-tower star is `startGrade` (Tokyo 1, Hawaii 2, Kegon 3; also confirmed by T2.exe doc init, which copies worldDef+0x70 to doc+0x8c). | MATCH | H (pop), M (start grade) |
| R2 | **The gate is population plus one VIP flag per grade, and nothing else.** The test is `(doc+0xa0 & row.vipMask)==row.vipMask && pool.population ≥ row.population`, which returns 0x2715. T2.exe then does `grade++`. It checks every idle tick, with no time-of-day window. There is no security, medical, recycling or metro requirement. | `CheckGameLevelProc` 0x10072410 (identical to `T2WorldPlugin::CheckGameLevel` 0x10073290); T2.exe FUN_00421e90 (0x2715 = +1, 0x2716 = −1); caller FUN_0041da20 | Requires Security, Suite, Recycling, Medical, VIP and Metro on a weekday-evening window (`LevelUp.cpp:37-44,104-155`). **Implemented:** `Game::yootCheckGrade` / `LevelUp::yootPromotionReady` run every sim tick in YootTower mode: `(yootVipFlags & row.vipFlag) == row.vipFlag && population >= row.population`. No facility scan, no time window. | MATCH | H |
| R3 | **VIP visits** (VIEV, one VIP per grade).<br>• **Mask:** a VIP's flag is the first free bit of `1<<(4(g-1))`, i.e. 1, 16 or 256.<br>• **Start conditions:** the current grade equals the VIP's grade, its flag is still unset, and population > row.population/2.<br>• **Schedule:** at most once per day; cycleDay 9, begins 10:00.<br>• **Destination:** each VIP needs its destination tenant type built, or the game shows "To invite a VIP, a/an X needs to be built". If that plugin is not installed, the flag is granted outright.<br>• **Success:** the VIP's `CalcCurrEstimate > 200`.<br>• **Destinations:** Tokyo Record store / Electronic store / Suite; Hawaii Burger / Suite / Suite; Kegon Old Time Cafe / Observatory / Kegon Spa. | `T2VisitVIP::Init` 0x1006d360, `Start` 0x1006cd90, `GetDestinationTennant` 0x1006d8c0, `T2People::VIPEstimation` 0x10049990; VIEV data | Has one VIP, at 3★ only, scheduled on `day%9==3`, arriving 17:00, favourable if stress ≤ 150 (`VipSystem.cpp:23-30,127-150`). **Implemented:** `VipSystem::advanceYoot`: VIEV entries with T2VisitVIP::Init flags; one attempt per day at beginMin (Init forces cycleDay to 1 and IsBeginDay admits the first call per day, so the 9 is not a day cadence); start gate as `T2VisitVIP::Start` (`LevelUp::yootVipMayStart`). The destination plugin maps to OpenSky prototypes by plugin id; no counterpart = flag granted outright, none built = "To invite a VIP, a/an X needs to be built". Verdict at the VIP arrival minute: the destination's 0-100 evaluation x3 must exceed 200 (proxy until per-person estimates, P2, exist). No VIP person is spawned. | PARTIAL | H |
| R4 | **The top grade (4→5) needs the world's final-item event.** There is no grade-4 VIP. Instead the final item calls doc vtable +0x188 (T2.exe FUN_00422670), which ORs the current grade's mask (0x1000) into `doc+0xa0`, but only when grade == numGrades. The triggers are:<br>• **Tokyo:** a baseball game at the **Stadium**. It needs ≥ 200 visitors, runs at grade 4 only, and lets you name the team (BBEv; `T2Baseball::Exec` Tokyo.t2p 0x10011c40).<br>• **Hawaii:** a wedding at the **Cathedral**. It runs on a holiday and needs ≥ 30 visitors and a couple (WdEv 15:00–17:00; `T2Wedding::Exec` Hawai.t2p 0x10015120).<br>• **Kegon:** the "Attraction will be renovated" event (Attraction.t2p FUN_0x1000c290).<br>After that, population ≥ 12000 / 8000 / 1000 promotes. | as cited | TOWER is granted at 15000 + Security + Medical + Metro (`LevelUp.cpp:42`). `Cathedral::weddingCompleted()` exists but nothing calls it. **Implemented:** `Game::grantFinalGrade()` (FUN_00422670 semantics: OR the top grade's flag only at grade == numGrades, then re-check). Nothing calls it yet: the Stadium/Cathedral/Attraction events come with the specials. **Implemented (step 14 B):** the three triggers call `grantFinalGrade()`: Tokyo `VenueSystem::beginBaseball` (BBEV at 20:00, April-October, `Yoot::baseballBeginDay`; the Stadium must have 200 visitors today, then the team "Towers" is announced; no naming dialog, no fireworks), Hawaii `beginWedding` (WDEV 15:00 on a holiday: Cathedral open with 30 visitors and two Hawaiian-condo residents with estimate > 70 or unevaluated; the STRTBL 1 announcement verbatim, "Mr." quirk included), Kegon Attraction renovation at 200 visitors a day (FUN_1000c290 text, bitmap 9998; the flag is saved, unlike the original). Done flags are saved (`yootBaseballDone`, `yootWeddingDone`). | MATCH (abstracted visitors) | H |
| R5 | **Unlocking.** The toolbox lists a plugin when its ATTR `mGameLevel` (CFilePlugin+0x54) ≤ the current grade. The ATTR levels are the source of truth (e.g. Coin/BCD-Mart 3, Front Desk 2, Power Room 3, Fire Dept 4, Large Elevator 3). | T2.exe FUN_00422030 and FUN_0041ca80 (refilter on grade change); `attr_table.py` | Uses a hardcoded SimTower mapping (`LevelUp.cpp:52-79`). The ATTR level is parsed (`PluginResources.h:67`) but never used. **Implemented:** `BuildRules::minRatingToBuild` uses ATTR mGameLevel, the world mask and the secret flag (FUN_0041ca80) for OpenSky prototypes linked to YootTower plugins (`yootPrototypeLinks()` in YootWorld.cpp; a prototype is offered when any linked plugin is available, at the lowest level). Prototypes without a counterpart (secom) keep the SimTower lock table. Items absent from a world are hidden and refused. | PARTIAL | H |
| R6 | **Each grade change** calls `T2Pool::GameLevelChanged` (loads that grade's PGDF visitor pool), `IsDustOn(grade)` (trash system on from grade 3), `GetToiletDenomi(grade)` (restroom demand 1/10 from grade 1) and `GetColdDenomi(grade, month)` (illness rate, from CODF). | T2.exe FUN_0041da20; `T2Pool::GameLevelChanged` 0x10050390; `T2WorldDef::IsDustOn` 0x10072100, `GetToiletDenomi` 0x100720c0 | None of these systems exist in OpenSky. **Implemented (step 12):** `RulesProfile::facilities` carries WORLDDEF dustOnAt/toiletAt/toiletDenomi and the CODF table (now parsed, `parseYootColdTable`); `Yoot::isDustOn`, `toiletDenomi`, `coldDenomi` (YootFacilities.h) are read live from the current grade and day, which is what FUN_00419460 caches on each grade change (and FUN_0041dc40 refreshes daily for colds). The PGDF visitor pool (`GameLevelChanged`) is still not modelled (P4). | PARTIAL | H |
| R7 | **Grade-up message and animation** are per world (Tokyo and Kegon override `GetGradeUpMessage` for grades 3, 4 and 5). The animation bitmaps are 1207–1210. | Tokyo.t2p 0x10015500, Kegon `WorldDef::GetGradeUpMessage` 0x100153d0, `SetupGradeUpAnimation` 0x10072e70 | Shows "Your tower has been given a N-Star Award" and the LevelUpDialog. **Implemented:** YootTower promotions show "Your tower has been given a N-Star Award!" and the LevelUpDialog; per-world messages/animations are not loaded. **Implemented (gap closers):** the message is the world's GetGradeUpMessage (`yootGradeUpMessage`): Tokyo "...to a two/three/four star level." / "The Tower Grade, the highest level.", Kegon "Bamboo" / "Pine", Hawaii the T2DLL "Your building has been upgraded to a %d star level" (strings read from the binaries). The animation bitmaps are not loaded. **Implemented (walking agent):** the YootTower promotion dialog plays the world animation of the new grade's WORLDDEF row (T2.exe FUN_0041ca80, dialog 9101 item 0x68 -> T2WorldDef::SetupGradeUpAnimation 0x10072e70): bitmap 1207..1210 (Tokyo), 144x96 frames stacked vertically, 13 / 11 frames, one per 100 ms, looping back to frame 8 / 6 (row fields 3 and 5; T2DlgItemAnimation::OnT2Timer 0x10022470). Deviation: the dialog stays until OK instead of closing after the 3-second pump and plugin load. | MATCH | H |

### 1.3 Worlds, scenarios and unlock attributes

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| W1 | **Three worlds with distinct parameters:**<br>• **Tokyo:** $10,000,000 start; 100 floors above / 10 below; 354 m wide; lobby 3 rows; sort key 0.<br>• **Hawaii:** $5,000,000; 45 / 6; 400 m; lobby 2 rows.<br>• **Kegon:** $2,000,000; 2 / 32 (you build down the cliff); 36 m; lobby 1 row. | WATR `initialFunds` +0x24c via ctor 0x1006e4e0; WATR's 6 "discarded" ints = [.., above, below, width]; world STRTABLE 20000 text | Has no world concept. Start is always $2M (`Game.cpp:358`), which happens to equal Kegon's. **Implemented:** `YootWorld` parses WORLDDEF/WATR/T2EV/FINF; new towers take WATR funds ($10M/$5M/$2M), start grade, the WORLDDEF+0x74 start day at 07:00 (T2.exe doc init `T2DateTime(1, worldDef+0x74, 7, 0, 0)`) and the lobby height. `world=` is saved; older YootTower saves load as Tokyo. | MATCH | H |
| W2 | **The build area is a WORLDDEF RECT** (+0x200; Tokyo cols 36–332 × rows 6–120; Hawaii 56–392 × 3–62; Kegon 81–186 × 3–43). Drag-building clamps x to `[left, right]`. | `GetBuildArea` 0x1000a660; T2.exe FUN_0041e8f0 | YootTower mode is unbounded (`BuildRules.cpp:53-66`). The comment in TODO.md "MUST NOT become numeric limits" is now answered: the consumer is decoded. **Implemented:** build-area columns and rows are enforced at every footprint call site (`BuildRules::allowsFootprint`). OpenSky x maps to units as `unit = x + width/2` (an OpenSky convention: the world centre sits at x = 0 where the camera starts; Tokyo x in [-148, 148)). | PARTIAL | H (x clamp) / M (y) |
| W3 | **ATTR `mAttrib` looks like a world bitmask.** Worlds: Tokyo 0x7, Kegon 0xf0, Hawaii 0xf01. Items: 0x1 = Tokyo+Hawaii, 0x2 / 0x4 = Tokyo only, 0x10 = Kegon, 0x100 = Hawaii.<br>This is consistent with every world-specific item: final items Stadium 0x4, Cathedral 0x100, Attraction 0x10; the K-variants 0x10; all VIP destinations. The exact consumer (T2PluginSpecifier+0x1c) has not been located. | `T2PluginSpecifier::Initialize` 0x1004fd00 (ATTR→specifier +0x1c); `attr_table.py` | Has no world filter. **Implemented:** the consumer is T2.exe FUN_0041ca80 (`world.mAttrib & spec+0x1c`, `spec+0x20 <= doc+0x8c`, `spec+0x24` secret needs doc+0x9c); applied as above (R5). The world mask comes from the world plugin's own ATTR. | MATCH | M |
| W4 | **ATTR `mDependID`** (+9 4cc) = "NORF" (Front Desk) on Single/Twin/Suite rooms and Housekeeping. Rooms must be linked to a Front Desk (string 1049 "Room is not connected to the front desk"; `T2GuestroomTable`). | ATTR sweep; `T2GuestroomTable::*` 0x100377a0.. | Hotel rooms stand alone. **Implemented (step 14):** native Front Desk (`Item::FrontDesk`, Front.t2p / RFront.t2p). Rooms whose plugin carries mDependID NORF (Single/Twin/Suite; not the Kegon Small/Large Room) get no guests until a desk links them; desks link every unlinked room in route range (FUN_10013b70, run hourly and on build here instead of at staff arrival); the inspector shows comment 1049. **Oracle:** a Front Desk + 4 Single + 1 Twin built through the Tools palette on Sample Tokyo Map (grade 3) had all five rooms at T2Tenant +0x94 = the desk id (1001) once the desk opened (status 30). Front.t2p/Parking.t2p's "A Front is required..." / parking request messages sit in vtable slot 50, past T2TenantDef's 50 virtuals: dead code, not shown (oracle: no parking request at grade 3). Artifacts: `~/yoottower-decomp/hotel-agent/`. | MATCH | M |
| W5 | **`mMustItem` / `mSecretItem`** are 1 on Kegon (world) and Spa, and on Floor, SkyBridge, Stadium, Cathedral, the K-elevators and the worlds, respectively. Their engine semantics are not traced. | `T2PluginSpecifier::Initialize` (+0x24 = mustItem) | n/a **Resolved:** ATTR field 5 (spec+0x24, OpenSky `secretItem`) is 1 on Spa and Kegon; the toolbox and doc-init loaders skip it until doc+0x9c bit 0 is set, which T2HotSpring does through FUN_00422080(1). Field 7 (Floor, SkyBridge, Stadium, ...) is not a toolbox gate. **Implemented:** `YootUnlock::secret` / `BuildRules::setSecretUnlocked`; the Kegon Spa is locked until the hot spring (V6). doc+0x9c lives in the .bld header (`f9c`), so the original does keep the unlock through a save even though T2HotSpring::Write saves nothing; OpenSky saves it as `yootSecretItems` and maps `f9c` both ways. | MATCH | L |
| W6 | **"Can only be placed once":** TenantDef `maxBuildCount` (+0x104) = 1 for Stadium, Cathedral, Attraction, Observatory, Subway, Theater, ClassicTheater, Power Room and Boat Deck. | `T2TenantDef` ctor 0x100623c0, `GetMaxBuildCount` 0x10004ef0; STRTABLE 9999 msg | Only Metro and SECOM are single-instance (`Game.cpp:2140-2153,2285`). **Implemented:** `TenantDef maxBuildCount` comes from the catalog (`prototype->yoot->maxBuild`); a second Theater/Cathedral/Subway/Power Room is refused with "can only be placed once" at both placement checks (plan step 4). | MATCH | H |
| W7 | **Corner-click doubler cheat:** clicking unit (1,1)–(2,2) while funds == the initial funds pays +initial (`DoPay(-f,4)`). | T2.exe FUN_0041e6f0 | Has the SimTower variant at (-187,-10) / $2M (`Game.cpp:2037`). | PARTIAL | H |
| W8 | **Buried treasure** (world `CheckTreasure`, e.g. Hawaii). It is a random roll whose odds depend on grade and funds: 1/12, 1/30 or 1/70. The outcome is a 1/3 "bone" (no money), otherwise $1,000,000 below grade 3 or $3,000,000 at grade 3+. | Hawai.t2p 0x10017890; world RT_STRING 4000–4002 | Deterministic B(s+2) award of $200k/300k/500k (`EventSystem.cpp:60-61`). | DIVERGENT | H |

### 1.4 Economy

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| E1 | **Prices are data.** Each ToolDef variation carries `price` (+0xd8), quarterly `outMoney` (+0xa8), `power` (+0xf0) and names. A full table is in §3. Examples:<br>• Office $100k; Condo $100k; Single room $30k; Suite $100k<br>• Simple Elevator $150k + $50k/car; Large Elevator $500k + $150k/car; Service $100k + $50k/car<br>• Stairs $5k; Escalator $20k; Floor $500/unit; Lobby $5k/unit<br>• Parking $50k/gate + $5k/lot; Subway $1M (+$500k/upgrade); Power Room $500k (+$500k per transformer)<br>• Stadium $5M; Cathedral $3M; Attraction $1M | `T2ToolDef` ctor 0x10065540, `CalcPayment` 0x10004e70; per-car price = MoverDef +0x194 | Uses SimTower prices (Office $40k, Condo $80k, Std elevator $200k + $80k/car …; `Factory`/item headers). Floor, Lobby, Stairs and Escalator happen to match. **Implemented:** `YootCatalog` (`resolveYootCatalog`) reads TNDF/MVDF for every linked prototype; `Game::applyItemCatalog` overlays price and car price (MoverDef +0x194 tier x $100) and sets `prototype->yoot` (power, capacity, inMoney, hours, maxBuild, mover limits, score, speeds). Floor/Lobby and Parking use ToolDef variation 1 where the plugin has two. Test: StdElev $150,000 + $50,000/car, 15 floors, 6 cars of 17. Prototypes without an available plugin (secom) keep OpenSky prices. | MATCH | H |
| E2 | **Floor-type items charge per empty cell covered** (attr 0x2000). | `T2TenantDef::CalcPayment` 0x10062b10 | A brand-new slab costs a flat $500 (`Game.cpp:1412,1425`). **Implemented:** Not changed by plan step 7: the per-empty-cell floor charge is still OpenSky's flat slab price. **Implemented (gap closers):** YootTower floors charge price x cells that had no floor (`Floor::yootCover`, `Yoot::floorCharge`), for new slabs and extensions; lobbies charge their price per covered cell and lay their slab free. Escalator maintenance re-checked against T2MoverDef::CalcMentenanceCostProc 0x10040520 (module count / 2 x outMoney, one span = $5,000/Q; not overridden by Escalator.t2p). **Oracle:** floor dragged 10 units into a gap on 8 rows cost $40,000 (80 cells x $500); cells already under a Sky Bridge cost nothing. | MATCH | H |
| E3 | **Quarterly maintenance** is charged at midnight on day 1 of each quarter, for every item whose attr has 0x100:<br>• **ordinary items:** `outMoney[variant]`<br>• **attr 0x4000 (lobby, sky bridge):** `outMoney × width` (per unit)<br>• **movers:** `outMoney × cars`, halved if attr 0x2000<br>Paid through doc vtable +0x278 (DoPayTool). | T2.exe FUN_0041dc40 → `T2FloorInfo::CalcMentenanceCost` 0x100358e0 → `T2HaveOutViewObject::CalcMentenanceCost` 0x10039140; `T2TenantDef/T2MoverDef/T2ToolDef::CalcMentenanceCostProc` 0x10062df0/0x10040520/0x10065bc0 | Quarterly at 5:00, using SimTower categories and values (`Game.cpp:60-99`). **Implemented:** Plan step 7: `settleYootQuarter` runs when the financial cycle changes (midnight, day 1) and charges `Yoot::maintenanceCost`: attr 0x100 only; attr 0x4000 x width; movers outMoney x used cars (stairs/escalators: 2 modules halved by attr 0x2000). The Power Room's $50,000 comes through the same path. **Oracle** (T2.exe under Wine, Sample Tokyo Map.bld, Y1 Mar 20:48 -> Apr 00:10): -$77,000 = lobby 46 units x $1,000 + 7 restrooms x $1,000 + Simple Elevator 2 used cars (of 6 slots) x $12,000; settlement totals reset at the same moment. Artifacts: `~/yoottower-decomp/econ-agent/`. | MATCH | H |
| E4 | **Office rent:**<br>• **Quarterly charge:** on day 1 of the quarter, between 04:51 and 05:00, the office pays `3 × inMoney[priceIndex]` (inMoney = [10,20,30,40], i.e. $1,000–$4,000 per day).<br>• **On occupancy:** it also pays once, via SetKind, with the message "Income from office rentals just came in".<br>• **Who works:** workers are called at the open time on non-holidays only. | Office.t2p FUN_0x1000c0c0 (ChangeStatusOnIdle) | Charges 15k/10k/5k/2k per quarter plus a $10k deposit (`Game.cpp:31-58`, `Office.cpp:66,149`). **Implemented:** Plan step 7: `collectYootOfficeRent` pays 3 x inMoney[selection] for every let office once per quarter from 04:52 on day 1 (`yootRentCycle`, saved); occupancy pays inMoney once. Default selection = inMoneyIndex 2. **Oracle:** by 05:24 on Apr 1 the 9 offices (inMoneyIndex 2) had paid 9 x 3 x $3,000 = $81,000 (settlement cost list 1200: -810 units); the message showed at 04:58 and nothing had been paid at 04:11. During the first quarter each office paid $3,000 once on occupancy. Apartments pay 3 x inMoney the same way (step 14: native `Item::Apartment`, Apartment.t2p, one resident, paid with the office rent; Sample Tokyo Map imports its apartment). | MATCH | H |
| E5 | **Price levels:** priced tenants have ≤ 4 `inMoney` levels (`GetPriceIndex` = selected + 1). The residential estimate gets `+(3 - priceIndex)*30`. | `T2EquipDef::GetInMoney` 0x1002f8a0, `T2Tenant::GetPriceIndex` 0x1005f880, `UpdateResidencialEstimate` 0x10060e20 | Uses 4 rent classes, but they do not affect evaluation that way. **Implemented:** `Yoot::priceIndex` / `selectionFromRentClass` (OpenSky rent class 0 High..3 Very low -> selection 3..0) feed rent, hotel income and the `(3 - priceIndex) x 30` estimate term. There is still no per-item YootTower price picker except the shop rent class. | PARTIAL | H |
| E6 | **Commercial income and evaluation:** income = `inMoney × customers/100 − maintenance`; the estimate = `70 × income / maintenance`, clamped to 1–300, so 70 is break-even. | `T2Tenant::CalcInMoney` 0x1005f620, `CalcProfitEstimate` 0x10061410 | Pays fixed amounts at closing time by customer-count threshold (`FastFood.cpp:195`, `Restaurant.cpp:183`). **Implemented:** Plan step 7: fast food, restaurants and shops pay `CalcInMoney` at close (customers x inMoney / 100 - outMoney, in $100 units) instead of the SimTower thresholds/quarterly shop rent, and store `CalcProfitEstimate` (70 x gross / outMoney, 1..300) as the tenant estimate. Customer volume is still OpenSky's (tens per day), so most shops run at a loss under these values. | MATCH | H |
| E7 | **Hotel income** sums `inMoney` for rooms in occupied states 31–34, per Front Desk table. Rooms: Single [8,12,20,32], Twin [15,30,45,60], Suite [45,60,90,120], i.e. $800–$12,000. | `T2GuestroomTable::CalcInMoney` 0x10037a00 | Pays the class-0 rate at checkout (`Hotel.cpp:335`). **Implemented:** Plan step 7: checkout pays the room's inMoney[selection] (`Yoot::hotelRoomIncome`); there is no Front Desk table, and the payment timing is still OpenSky's checkout (YootTower timing unknown). **Implemented (step 14):** checkout no longer pays. When a stay's first guest arrives, the room's inMoney[selection] is booked on its desk per room type (FUN_10013ce0 -> FUN_10015420); the desk pays the ledger at 12:00 as "Income from Hotel Facilities just came in." (FUN_10015540, one payment per room type) and clears it at 17:00 (FUN_10015450); the ledger is saved. Kegon rooms without a desk pay at check-in (untraced). **Oracle:** prices ($100k desk, $30k single, $60k twin, $50k housekeeping) and quarterly maintenance (desk 100 units, housekeeping 200) confirmed; no guest arrived in 4 game days at population 12, so the 12:00 payment itself is still from code only (H). Guests walk straight to their room instead of queueing at the desk. **Implemented (gap closers):** the Front Desk rate popups of T2DLG 1000 (7154/7157/7158: Single $800-$3,200, Twin $1,500-$6,000, Suite $4,500-$12,000; RFront Small/Big) are offered in the inspector and stored in the desk ledger (+0x16, default 3); linked rooms are priced from their desk. **Not done:** the desk queue (guests walk to the desk, 2 check-ins per desk idle, +15 stress per idle while queued, give up above 249 with -100 estimate; FUN_10014000/FUN_10013ce0) needs walking guests (people agent). **Step 15:** the guests of a room are called out of the visitor pool with the room plugin's customer table (they return to the pool at once; the OpenSky guests are not pool people). **Implemented (walking agent):** the desk queue. A clean room with estimate > 200 calls its guests at 17:00..22:00 with one chance in 30 an idle and an open linked desk (FUN_10012d50 -> CollectEmployee with the room's TMDF 1: Single 1, Twin 2, Suite 3; econo + priceIndex - 3). They walk to the desk and queue; each desk idle stresses the waiting by 15 (the party gives up above 249 with -100) and runs two check-ins / check-outs (FUN_10013ce0). Check-in sends the party to the room with a return to the desk and books the inMoney on the desk ledger; statuses 32 (in), 33 (asleep from 23:00 at 1 in 5, forced 01:00), 34 (up 06:00-10:00 at 1 in 20, forced 10:00), PushOutAll at 11:00 (or 1 in 20): the guests walk back to the desk, check out and leave into the pool, and the room waits for a maid. From a room's second stay the guests get special flag 1 and a return to the room at 22:00 between 17:00 and 21:00 (FUN_10012e80), so restaurants and shops can call them out. Kegon rooms also need a linked desk (the Small/Large Room call checks it). Deviations: the room estimate is JudgeSystem's, not the empty-room night bonus; stays are not saved (a reloaded let room is checked out). | MATCH | M |
| E8 | **Event payments** use doc vtable +0x270 (`DoPay`, category 2): the fire helicopter, terrorist ransom and Power Room transformers. | `T2FireBurning::Start` 0x10030d50, Terrorist 0x10063a60, PowerRoom.t2p 0x1000c4a0 | `transferFunds` with toasts. | MATCH (mechanism) | H |
| E9 | **No loans:** there is no loan class or string in T2DLL or T2.exe. The funds dialog has 8 fixed cash steps 0…$20M (`GetCashAmount`). | string and class inventory; `T2WorldDef::GetCashAmount` 0x1000a790 | No loans. | MATCH | M |

### 1.5 Tenant evaluation, complaints and departures

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| J1 | **The estimate scale is 1–300.** The colour is red below 70, yellow 70–200 and blue above 200. | `T2Tenant::SetEstimate` 0x1005ffb0, `CalcEstimateColor` 0x10060a20 | Uses 0–100 with critical below 25 (`JudgeSystem.cpp:94-186`). **Implemented:** Plan step 8: `JudgeSystem::yootEstimate` produces the 1..300 value (`Item::yootEstimate`, saved); `evaluation` mirrors it /3 for overlays. `Yoot::estimateColor` ports CalcEstimateColor; critical = below 70. | MATCH | H |
| J2 | **Residential and office estimate** = mean of the residents' people-estimates − `senseOfNoise × trafficRange(entrance)` + `(3-priceIndex)*30` − years since built. `senseOfNoise` is Condo/Hotel 150 and Office/Apartment 100. | `UpdateResidencialEstimate` 0x10060e20, `T2TenantDef::GetSenseOfNoise` 0x10005540, `T2TrafficInfo` | Uses a same-floor 20-cell noise rule for hotels only (`JudgeSystem.cpp:31-62`). **Implemented:** Plan step 8: offices, condos and hotel rooms use `Yoot::residentialEstimate` with the residents' people-estimates, TenantDef senseOfNoise, price index and age. Deviations: no T2TrafficInfo exists, so the range (0..3) is a stand-in (people on movers serving the floor within 8 cells, one step per 5); OpenSky items have no build date, so age = 0. Formula tests cover noise, price and age. **Implemented (gap closers):** the traffic range comes from a port of T2TrafficInfo (`Yoot::TrafficMap`, owned by YootExtrasSystem): cells of 4 units over the floor area (Tokyo 74 x 109 = 8066 cells, the .bld count), +1 per walk leg of every route a person sets out on (FindRoute books legs as they are taken), hourly accum = accum x 31/32 + count capped at 0xffff, ranges at 25/125/250, cell 0 never counts. Items carry the T2Equip build day (`buildDay`, set at placement in YootTower; .bld T2Equip +0x4c both ways) and the kept resident mean (+0x8c, `yootPeopleEstimate`). The traffic cells are saved (`yootTraffic`) and mapped to/from the .bld (u16 count, u16 accum, not traffic/range as bld-format.md said). Deviation: the entrance is the tenant centre on its bottom row. | MATCH | H |
| J3 | **Departure and growth:** a tenant whose estimate falls below 70 loses its residents (they move out). Above 200, residents duplicate (growth); between the two, they duplicate every 5th call. | `DecideResidencialEstimation` 0x10061380 | Move-out at < 30 or no route (`Office.cpp:364`); no growth. **Implemented:** Plan step 8: Office/Condo/YootCondo stay let while the estimate is >= 70 and a vacant one only fills above 200 (`Item::yootKeepsTenants`); offices decide at the 5:00 check after the 04:52 rent. `Yoot::decideResidential` (every-5th-call growth counter) is ported and tested, but OpenSky tenants have fixed occupant counts, so "growth" only means move-in. | PARTIAL | H |
| J4 | **The complaint/status catalogue** is a fixed set of 26 messages: no demands, route, far elevator, power down, Reggae noise, no restaurants, needs parking, needs doctor, bad atmosphere, no trash facility, not connected to trash, full of garbage, forced to close, customers few/many/weekend/rainy, opens tomorrow, beds not made, room dirty, not connected to front desk. | T2.exe STRINGTABLE 1024–1049 (`dump_rsrc.py --strings`) | About 46 generic toasts. | PARTIAL | H |

### 1.6 People simulation and routing

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| P1 | **Stress (0–300):** while waiting for a mover, stress rises +5 per idle tick. At 300 the person gives up (teleports) and loses 50 estimate. With no route, stress rises +30 per attempt; at 300 the game shows "No route to destination." and the person loses 150 estimate. | `T2People::IdleWaitMover` 0x10048880, `SolveNoRoute` 0x10048c40, `IncStress` 0x100497e0 | Stress is also capped at 300, but it comes from per-floor stairs (+35) and escalators (+16) plus walking; there is no per-tick wait stress (`Person.cpp:36-57`). **Implemented:** Plan step 9: queued people get +5 stress per people-idle tick (2 game minutes; T2PeopleArrayList round-robin of 8 steps at 4 steps/minute) and give up at 300 (60 ticks): they jump to the destination with -50 estimate (`Person::yootWaitTick`, `Journey::jumpToDestination`). A failed route adds +30; at 300 "No route to destination." and -150 estimate. The SimTower stair/escalator journey stress and queue watchdog are off in this mode. | MATCH | H |
| P2 | **The people-estimate** is a running average over up to 4 trips, `(est×n − stress + 300)/(n+1)`. The VIP and tenant estimates are built from it. | `CalcCurrEstimate` 0x100496f0, `UpdateEstimation` 0x10049790 | No per-person estimate. **Implemented:** Plan step 8/9: `Person::yootEstimate`/`yootTrips` (start 300/0, saved when non-default); `Yoot::calcCurrEstimate` and `updateEstimation` at every trip end. The VIP verdict (R3) now uses the mean people-estimate at the destination, then its tenant estimate. | MATCH | H |
| P3 | **Route cost** per mover = `MoverDef.score × floors` (Stairs 50, Escalator 30, Simple 15, Service 8, Large 5, K-elev 15). | `T2Mover::CalcScore` 0x1003ef20; MVDF `score` | Uses A* constants (`PathFinder/MapSearchNode.h:47-58`). **Implemented:** Plan step 9: `MapSearchNode::GetCost` charges catalogued movers score x floors (`Yoot::moverRouteScore`); walking stays free. The OpenSky stair/escalator leg caps remain. | MATCH | H |
| P4 | **Population is data-driven:**<br>• per-grade visitor pools (PGDF via PODF)<br>• hourly demand lists by time zone (PZDF)<br>• tenant member tables (FDmT: demand / silhouette / count / rate / percent)<br>• customer search scores (DPDF EPlace scoring) | `T2Pool` 0x100501e0/0x10050390, `T2PeopleDemandList`, `T2TenantMemberTableDef` 0x100631e0 | Spawning is hardcoded per item class. **Implemented (step 15):** `Yoot::Pool` (`YootPool.*`) is a verbatim port of T2Pool with T2PeopleDemandList / T2PeopleTimeZoneList / T2PeopleTypeArray: PODF per-grade PGDF pools added on every grade change (GameLevelChanged 0x10050390), PZDF types (count = raw x 20 / 100, inserted in front, so the PZDF order is reversed), the hourly zone roll with AdjustLife 0x1004e930 and InitSearchLimit 0x1004e9d0 (no world ships SPDF id 1: a new tower opens no window until each zone first rolls, then factor 1.0), Call 0x10050580 / Find 0x10050500 with T2PeopleType::Check 0x1004e4c0 (transport bit, silhouette range or age band, option, econo in [e, e+2]), the demand -1 quirk (list 0 is never tried), Enter 0x10050610 (restore in front of the window; > 200 duplicate, 71..200 every 5th, <= 70 only immortals), RemovePeople, BornVIP and the midnight population drop (DayChanged 0x100504a0). The tower population adds the pool's (T2Pool::GetPopulation). CheckRouteFromPool 0x10060bf0 is the transport mask (3 lobby, 4 parking, 8 subway while a train is in, 0x10 ship while the cruise is docked; routes within the PLDF pool score). Plugin tenants run CollectCustomer 0x1005f440 with all three sources (pool 0x1005ef70, tenant 0x1005f0c0 = CalledInTenant, floor 0x1005f250 = StopOnHisWay), gated by Float2Int of the PLDF and DPDF holiday rates. The pool is saved (`yootPool`) and imported from .bld saves. Deviation (before the walking agent): routes are OpenSky's PathFinder (mover score + walk against the PLDF score). **Walking agent:** the visitors have T2People floor positions (curH/curV/dstH) and walk to each mover and to the end of the route at 16 units per people idle (IsReachDestination 0x10049310, IncStress(step/4) off the ground floor; lobby entrants start at its left edge for transport 0, else right edge - 2). StopOnHisWay now takes the people walking on the tenant's floor within the score (not those queued at a mover) and SelectNearTenant follows T2RoutingTable 0x10055db0 / SelectNearFloor 0x100428d0 (a random mover of the entrance floor, the walk off the score, a random floor it serves within the rest, one tenant with people inside within x +- score); CalledInTenant/StopOnHisWay compare the kind of the last tenant left (+0x6c is an equip type). Shop and restaurant staff, office workers (Office.t2p TMDF 1: six of demand 1, ages 24-55) and the School's 60 students are drawn with CollectEmployeeFromPool (T2Pool::Find of the whole table, then the calls); their types stay with the tenant and they are counted in its population (not in the pool's). Remaining deviation: routes are OpenSky's PathFinder. | MATCH | H |
| P5 | **Restrooms:** people need a toilet (the rate is set by `toiletDenomi`). `SearchToilet` looks in registry 7 within a route score. On failure it shows "No restroom nearby" / "Restroom is crowded" and costs 30 estimate. | `T2People::SearchToilet` 0x100499c0, `T2RegistedTenantDB::SearchToilet` 0x10051f30 | None. **Implemented (step 12):** entering at the lobby rolls `rand() % toiletDenomi` (T2People::Idle case 1 + T2.exe FUN_00421e60); arriving at a tenant whose TNDF attr has 0x2000000 runs `Yoot::searchToilet` over the restrooms (reachable by route, users < capacity 5). Found: the restroom counts the user for its stay (15 min) and shows its pattern; else "No restroom nearby." / "Restroom is crowded." and -30 estimate (`FacilitySystem::onArrive`). Deviation: the person does not walk to the restroom and back; the route score cap of CheckRoute is not applied. | PARTIAL | H |
| P6 | **Illness and doctors:** the CODF cold table, per grade and month, makes people sick. Tenants complain "Requires a Doctor's office nearby". | `GetColdDenomi` 0x100720e0; string 1034; Doctor's.t2p overrides | Medical Center only gates promotion. **Implemented (step 12):** Office.t2p's cycle: at 00:00 sick workers lose 50 estimate (FUN_1000b8b0), then on working days each worker catches a cold with `rand() % GetColdDenomi(grade, day+1)` (FUN_1000b850; CODF 20/30, from grade 3); 08:00-18:00 at the top of each hour one sick worker in the office goes to a reachable, open Doctor's Office with room (FUN_1000b900/FUN_1000ba80) and is treated after the 30-minute stay. The plugin's failure balloon loads strings 11001/11002, which do not exist in the shipped game (no text); the inspector shows comment 1034 "Requires a Doctor's office nearby." instead. **Implemented (walking agent):** the School's colds (School.t2p 0x1000b8b0 / 0x1000b850 / 0x1000b900): sick students stay home at the call (CallEmployee skips flag 4), the flags are rolled at midnight on working days, and from 08:00 to 18:00 up to five sick students inside are treated at a reachable, open doctor every hour. Deviation: students keep no estimate while home, so the -50 for the still-sick is not applied to them. | PARTIAL | M |

### 1.7 Movers

| # | YootTower rule (MVDF via `T2MoverDef` 0x1003ffb0) | OpenSky | Status | Conf |
|---|---|---|---|---|
| M1 | **Simple Elevator** (EdtS): ≤ **15 floors** ("can be extended up to 15 floors"), ≤ 6 cars, 17 per car, speed [1,3,0], grade 1, $150k + $50k/car, $12k/Q. | Standard: span 29, 8 cars, 21 per car, $200k + $80k (`Elevator.h:27-35`, `Elevator.cpp:111`). **Implemented:** Plan step 4: `Elevator::maxSpan/maxCars` from MVDF: 15 floors, 6 cars; car capacity 17; price $150,000 + $50,000/car; maintenance $12,000 per car. | MATCH | H |
| M2 | **Large Elevator** (EgiB): any length ("to the top floor"), ≤ **8 stops** (`maxStop`), 8 cars, 40 per car, speed [1,3,12], grade 3, $500k + $150k/car, $20k/Q. | Express: stops only at `floor%15==0` plus basements, 42 per car (`Express.h:28-33`). **Implemented:** Plan step 4: no 15-floor rule (`Express::connectsFloor` defers to the stop set), any height, 8 cars of 40, $500,000 + $150,000/car, at most 8 stops: floors added by a drag beyond the cap stay unserviced and stop toggles refuse past it. Car physics: top speed scaled from MVDF speed[2] relative to the Simple Elevator's 3. | PARTIAL | H |
| M3 | **Service Elevator** (EreS): any length, 8 cars, 21 per car, grade 2, $100k + $50k/car, $10k/Q, staff only. | Service: span 29, 21 per car. **Implemented:** Plan step 4: any length, 8 cars of 21, $100,000 + $50,000/car. | MATCH | H |
| M4 | **Kegon variants** (VLEK / RESK): length 50 / 1000, 20 per car, $50k + $20k/car, $5k/Q ("extended 34 floors"). | none **Implemented:** Not covered: the K-variants have no OpenSky prototype of their own; in Kegon the standard/service elevator prototypes take KElev/KServ data. **Implemented (step 14):** confirmed by test: in Kegon the standard/service prototypes resolve to KElev (50 floors, 20 per car, $50,000 + $20,000/car, $5,000/Q) and KServ (1000). YootTower-only prototypes now take their per-world footprint from the plugin (KRestroom 10x1, KTrashFacility 17x2, RMaintenance 12x1). KDrink/KIcecream/kCigar are left to the plugin-backed shop family. | MATCH | H |
| M5 | **Stairs and Escalator** connect adjacent floors; capacity 100; Stairs $5k with no maintenance, Escalator $20k at $5k/Q, grade 1 (Escalator is grade 1 in YootTower). | Prices match (`Stairs.h:14`, `Escalator.h:14`), but the escalator unlocks at 3★ (`LevelUp.cpp`). **Implemented:** Plan step 4: Stairs $5,000 / Escalator $20,000 and route scores 50 / 30 from MVDF; escalator maintenance $5,000/Q (2 modules halved, L confidence). Unlock grades come from ATTR (step 5). | PARTIAL | H |
| M6 | **Lobby is world-dependent (3/2/1 rows).** Elevators cannot stop on the upper lobby rows (`IsStoppableFloor` 0x1000a6b0). A floor may be stopped at only on its bottom row (`T2Mover::AddStop` 0x1003e780). There is no 15-floor sky-lobby rule; the only lobby rule is "Only the lobby item can be placed on the first floor". | Lobbies are allowed only on `y%15==0`, sky lobbies and transfers apply (`Game.cpp:2160`). **Implemented:** the default (no-modifier) lobby height is the world's 3/2/1 rows; the stop rule and the 15-floor rule are unchanged. **Implemented (step 13):** lobbies are placed on 1F only (`Yoot::lobbyRule`); the 15-floor sky-lobby placement no longer applies in YootTower. The express-shaft anchoring and transfer behaviour of sky lobbies are unchanged. | PARTIAL | M |
| M7 | **Elevator/escalator behaviour** (adding cars, shaft drag, stop selection, car AI) is plugin code: StdElev, BigElev and ServElev each override about 20 MoverDef virtuals. | Native SimTower elevator. **Implemented (walking agent):** car timing. T2.exe's sim step (0x41dc40) idles every mover after each IsIncDateTime, 4 times a game minute; StdElev crawls 6 idles a floor (speed [1,3,0] x 6 of 36 sub-steps; 2n + 8 idles for n >= 2 floors, 0->9 in about 6.5 minutes) and loads in a 6-idle pattern. YootTower cars now advance 5760 frames a game day instead of SimTower's 192 (one frame was 7.5 minutes, so trips took about 200 minutes and most visitors gave up waiting); OpenSky's 1 + 5 frame crawl matches the 6 idles a floor. People left behind by a full car call again at once, and a moving car redirected to its own floor opens its doors (it waited forever). Not ported: the StdElev hold of defaultWait 3 idles, the Large Elevator top gear (OpenSky's express stride is 3 floors a frame, the original 2). | PARTIAL | H |
| M8 | **Sky Bridge** connects two towers. It needs ≥ 3 free floors above and below, must be above the 5th floor, must sit between the towers within a min/max distance, and allows no transit on it. $5k/unit, $1k/unit/Q. | none (OpenSky has one tower). | MISSING **Step 14 B:** the AreaCheck is ported as `Yoot::skyBridgeRule` (FUN_1000bcc0: at/above 5F, a building on both sides of the gap inside the build area, gap <= 72 units with no minimum, 3 empty floors above and below) with the STRTABLE 9999 texts 59/60/62/63. The bridge is decorative in YootTower (attr 0x45980 lacks IsFloor, so nobody walks on it). There is no "tower" entity: a tower is any occupied run of cells on the row, so no multi-tower support is needed. **Deferred:** the item itself, because OpenSky's tool has no snap-to-gap variable-width placement for tenants. Correction: the original has a maximum distance only; the "too short", "nothing above" and "no transit" strings have no caller. **Implemented (gap closers):** native `Item::SkyBridge`. The tool click runs `Yoot::skyBridgeAreaCheck` (FUN_1000bcc0 verbatim, including the snap to the gap) and builds one bridge of the gap's width, laying Floor in the gap first. YootTower floors keep `Floor::holes` between detached runs, so two towers can share a row (a tenant cannot stand on a hole: "Floor below is not wide enough"; T2.exe says "Can not place items beyond the edges of the floor below."). Floor laid over a bridge splits it into the remaining runs (FUN_1000c0b0). Cost $5,000 + floor $500 per empty cell, $1,000/unit/Q. **Oracle** (`~/yoottower-decomp/misc-agent/`, exported two-tower Tokyo save): the bridge snapped to the 40-unit gap on 9F and cost $25,000; after a 10-unit floor extension, a bridge on 12F snapped to the 30-unit gap and cost $20,000; the saved .bld holds a Floor record under the bridge (no hole on that row); a floor dragged over the 9F bridge charged nothing for the bridge cells and left the rest standing. OpenSky reproduces all four (`--qa-build`). Deviation: the floor under the bridge is walkable in OpenSky routing (untraced in YootTower). Status: MATCH. | H (msgs) / M (rules) |

### 1.8 Placement rules

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| L1 | **75 placement and removal error rules** in T2.exe STRTABLE 9999. Examples: floors can only be removed top-down or bottom-up; entrances (parking gates) must be stacked from the first floor; parking lots must be adjacent to a gate or lot and removed from the outside; the subway goes only on the lowest floor; boat deck at the left edge of B2; observatory at the left edge of B32; nothing above the sky bridge. | `dump_rsrc.py --type STRTABLE --id 9999` | Has its own subset of rules. **Implemented (step 13):** `YootPlacement.h` ports, with the STRTABLE 9999 texts: floor removal order (Floor.t2p FUN_1000ce30: transit #21, people #72, tenants #20, then top floor #18 / lowest floor #19) — YootTower floors become bulldozable under these rules; parking removal from the outside and gates bottom-up (Parking.t2p FUN_1000bf00: #54, #55, #57); gate stacking from 1F and lot adjacency (#52, #56), whose placement check switches on when the Parking Gate item lands (step 14); subway (#63), lobby (#15), final item (#65, #67). Not done: sky bridge, boat deck, observatory (items missing), Kinsoku (not in YootTower: no world or plugin data). **Step 14:** the gate (#52) and lot adjacency (#56) checks are live with the Parking Gate item; adjacency compares shared rows of the two-row gate. | PARTIAL | H (rules exist) / M (each check) |
| L2 | **Final item only on the top floor** ("Only Final item can be placed on the top floor of building"): Stadium above the 100th floor, Cathedral above the 45th, Attraction above the 2nd (MVDF/TNDF descriptions). | as above + TNDF desc | Cathedral is a 5★ item placed anywhere. **Implemented (step 13):** `Yoot::finalItemRule` at both placement checks: a TNDF category-100 item must stand with its bottom on the top floor (Stadium/Cathedral/Attraction AreaCheck: rect.bottom == topFloorLine + 1) and rises above the floor area inside the build area (`BuildRules::allowsSpecialFootprint`); every other non-mover item must stay below the top floor (T2TenantDef::AreaCheck 0x10062a90, "Only Final item can be placed on the top floor of building."). | MATCH | H |
| L3 | **Floor bounds per world** (100/10, 45/6, 2/32). | W1/W2 | Fixed B9 floor, about 120-floor camera, no height check (`Game.cpp:2134,960`). **Implemented:** floor limits come from the WORLDDEF floor area `[topFloorLine, bottomFloorLine)`: Tokyo 1F-100F / B9, Hawaii 45F / B6, Kegon 2F / B32. Kegon's FINF mask is enforced per cell. Note: Tokyo's WATR says 10 basements; the floor area gives 9 (row 119 is inside the build area only). | MATCH | H |

### 1.9 Infrastructure systems that are new in YootTower

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| I1 | **Power** (BOEV = [defSupply 5000, perTransformer 5000, transformerPrice 5000 = $500k]).<br>• **Supply:** `def + 5000 × transformers` (≤ 5 per Power Room; buying one costs the price).<br>• **Demand:** every item consumes `power[variant]` (e.g. Theater 200, Stadium 300, Subway 300, elevators 20–30).<br>• **Warning:** "Shortage of power" when supply − demand < 50.<br>• **Blackout:** a daily check at 11:00–14:00. If demand > supply, there is a blackout: +30 stress to everyone; at the end, "residents escaped through the emergency stairs" and +300 stress, until fixed.<br>• **Power Room:** grade 3, $500k, $50k/Q, once. | `T2BlackOut` 0x1001b580–0x1001ba90; PowerRoom.t2p 0x1000c4a0 | **Implemented (step 11):** `PowerSystem` (owned by EventSystem) feeds the pure `YootPowerModel` (`YootEvents.h`): supply = BOEV def + add × transformers (Power Room `Item/PowerRoom.*`, 1–5 transformers, the first included, $500k each from the inspector), demand = Σ ToolDef `consumptionPower` of each item's linked plugin. "Shortage of power." + suffix when a build leaves < 50 spare (AddConsumptionPower 0x1001b8b0); at 11:00 a blackout when supply < demand: "Power down." message, +30 stress, tenants drawn dark (T2Tenant::Draw DrawColorMesh); at 14:00 the escape (+1 min): message and +300 stress; ends when demand < supply or a transformer is added (BreakerOn). Not modelled: per-transformer breaker switches, the tenant-idle freeze while an exclusive event runs (T2.exe FUN_0041dc40 skips tenant Idle when `Idle` returns 1), the once-per-tower limit, maintenance $50k/Q (economy overlay). Tests: supply/demand arithmetic, the 11:00–14:00 window, escape and recovery. Oracle: see V7. | PARTIAL | H |
| I2 | **Trash** (from grade 3).<br>• **Collection:** restaurants (category 50) within route range of a Trash Facility deliver garbage, which fills it in 5 stages.<br>• **Emptying:** the facility empties between its close and open times (07:00→07:30).<br>• **Warning:** "Building requires a Trash facility" at 06:50 and 21:50 if there is none.<br>• **Cost:** $500k, $50k/Q (Kegon variant $200k).<br>• **Complaints:** "There's no Trash Facility", "not connected", "Full of garbage". | KTrashFacility.t2p 0x1000bba0/0x1000bcc0/0x1000bde0/0x1000bf00; `IsDustOn` | Recycling Center only gates promotion. **Implemented (step 12):** native `Item::TrashFacility` (TrashFacility.t2p / KTrashFacility.t2p art and TNDF data, YootTower only). `FacilitySystem`: the 06:50-06:59 / 21:50-21:59 warning (DefIdleProc FUN_1000bde0) once the item is unlocked (grade 3) and none is built; the facility fills in 5 patterns at 1, cap/4, cap/2, 3cap/4, cap (FUN_1000bcc0) and empties 07:00-07:30 (FUN_1000bba0). Restaurants (category 50) deliver one load per hour to a reachable facility with room; otherwise the inspector shows comments 1036-1038. **Oracle:** `Sample Tokyo Map.bld` patched to grade 3 at 06:40 shows "Building requires a Trash facility." from 06:52 to about 07:16; the same save at grade 2 shows nothing (artifacts in ~/yoottower-decomp/facilities-agent/oracle/). Not modelled: per-customer garbage trips (CollectCustomer from SearchRange) and the IsDustOn consumer, which is untraced (used here to gate deliveries). | PARTIAL | H |
| I3 | **Restroom** ($10k, $1k/Q, grade 1, registry 7). | P5 | none **Implemented (step 12):** native `Item::Restroom` (Restroom.t2p / KRestroom.t2p art and data, patterns 0/1/2 by occupancy as in FUN_1000cd50/FUN_1000ce20), used by P5. | MATCH | H |
| I4 | **Security Room** ($100k, $20k/Q, grade 2, registry 5). It supplies up to 6 firefighters or bomb searchers. Fire-fighting by staff is refused without one ("no security room"). | `T2FireBurning::DialogHook` 0x10031570 (`GetTenantByPID('IBEK')`), `CallFireman` 0x10031ed0 | The Security office exists, with 6 guards. **Implemented (step 14):** YootTower foot response uses CallFireman: six people in total from Security Rooms then Fire Departments (`SpecialSystem::firemen`). | PARTIAL | H |
| I5 | **Doctor's Office** ($500k, $50k/Q, grade 3, registry 4); **Fire Department** (Kegon, grade 4); **Housekeeping** + **Front Desk** (hotel back office). | ATTR, TNDF, overrides | Medical Center only; no front desk. **Implemented (step 12):** native `Item::DoctorsOffice` (Doctor's.t2p art and data: open 08:00-20:00, capacity 20, patterns 0 closed .. 3 busy), used by P6. The SimTower Medical Center and Recycling Center are unavailable in YootTower. **Deferred to step 14:** Front Desk and Housekeeping with `mDependID` NORF room linking (W4) and the Fire Department. **Implemented (step 14):** Front Desk (W4/E7), Housekeeping (`Item::Housekeeping`, Maintenance.t2p / RMaintenance.t2p 12x1 in Kegon): dirty rooms wait for its maids, sent 07:00-14:00 (open .. close - 120, RMaintenance FUN_1000bd00) with up to three rooms per maid trip (FUN_1000bf70, T2Matter return stack of 3); rooms still dirty at 17:00 get no guests; inspector comments 1047/1048. Fire Department (`Item::FireDepartment`, Kegon, grade 4, 8 firemen, registry 5) joins CallFireman, but Kegon's T2EV list has no FBEv and the staff option needs a Security Room (DialogHook checks IBEK), so in the shipped game it never fights a fire. | PARTIAL | M |
| I6 | **Parking:** a surface gate plus lots; registry 3; `FindEmptyParking`. Office visitors and hotel guests park. | Parking.t2p (14 overrides), `T2RegistedTenantDB::FindEmptyParking` 0x10052040 | Parking ramps are not registered, so parking is unusable (`Factory.cpp:41-67`). **Implemented (step 14):** native Parking Gate (Parking.t2p variation 1, $50k, $10k/Q, 16x2, patterns by stack position FUN_1000bd20) and Parking Lot (variation 0, $5k, 4x1, 5 drivers, car pattern id % 13 + 1); the SimTower parking is not offered in YootTower. Office workers and hotel guests take a slot in a reachable lot on arrival and free it when they leave; once lots exist, a driver who finds none loses 100 estimate (MoveToTowerEntrance). **Oracle:** Parking appears under Bldg. Facilities at grade 3 and not at grade 2. Not modelled: per-person entrance kind (CheckRouteFromPool), cars driving through the gate. | PARTIAL | M |
| I7 | **Subway** (Tokyo, lowest floor B10, $1M, upgrades $500k, $100k/Q, grade 2). The MREV train timetable has 12 arrivals 06:50–23:40 per day. | MREV via FUN_0x1003d690 (`T2Transport`) | Metro (1 per tower, underground, visitors 10–17h with a 1/36 roll). **Implemented (step 13):** placement only: the subway (OpenSky metro) must have its bottom row one below the lowest floor (Subway.t2p AreaCheck FUN_1000d5f0: rect.bottom == bottomFloorLine + 1; Tokyo B10) and uses the build-area footprint. The MREV timetable stays for step 14. **Implemented (step 14 B):** native `Item::Subway` (Subway.t2p art 40x3, open 06:30-23:59, capacity 100, patterns 1 empty / 2 occupied / 3 half full). VenueSystem runs the MREV timetable: level L (T2Transport::UpGrade 0x1006bb80) enables train types < L, so levels 1-4 run 4/8/10/12 trains (type 0 = 08:10, 10:50, 17:40, 21:10); a train stops 8 minutes and its riders (up to 20, 240 a day) walk to reachable tenants and ride a later train back. The inspector expands the schedule for MREV's $500,000 ("Not enough money. You cannot expand the schedule of the subway."). Simplified: no train sprite is drawn. **Step 15:** riders are pool people of transport 3, called by the tenants while a train is in (CheckRouteFromPool bit 8, MoveToSubway); the station no longer spawns its own visitors. **Implemented (walking agent):** riders of transport 3 board a train due within the hour at their start minute (MoveToSubway 0x10048a90 -> T2Transport::GetAvailable 0x1006bd80, at most 300; none: -100 and the lobby); while it is stopped (8 minutes) the station lands them with a return to the station (Subway.t2p FUN_1000d330: LeaveFirst + SetReturn), they walk to their tenant, walk back, wait inside and board the next stopped train; at departure the ones aboard go into the pool (T2Transport::Stop 0x1006bf70). The station's close pushes the waiting out. CheckRouteFromPool's subway bit follows GetAvailable. The same flow carries the Hawaii cruise passengers (transport 4, 30 minutes at the Boat Deck). | PARTIAL | M |

### 1.10 Events

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| V1 | **Events are scheduled from data.** The world's T2EV lists `(type, rsrcId)`. Each event header is `{levelBit (grade mask), exclusive, cycleDay, beginMin, endMin}`. An event runs when the grade bit is set and `(lapseDays+1) % cycleDay == 0` (overridable), starting at beginMin. | `T2TowerEvent` ctor 0x10068ca0, `Idle` 0x10069970, `T2EventItem` 0x10030050/0x10030280 | **Implemented (step 10):** `YootEventScheduler` (`YootEvents.*`) is fed the world's T2EV list through `RulesProfile::events` (built-in table without the plugin). Begin day = `(totalDays − 11) % cycleDay == 0` (`CalcLapseDays(12)` 0x1001cb10 = CalcTotalDays − 12, +1), grade bit `1 << (grade−1)`, begin minute crossed (Santa: 1440 − width/2, ctor 0x100560b0); a running event does not restart. `EventSystem` uses it in YootTower mode only (`EventSystemYoot.cpp`); SimTower/OpenSky keep the TimeT calendar. Test: 120-day schedules for every event, grade and world. | MATCH | H |
| V2 | **Fire** (FBEv: grades 3–4, every **60** days, 12:00–18:00). You pay for the helicopter at **$300k** (needs funds), send the security staff (needs a Security Room), or do nothing. | `T2FireBurning::Start/StartEvent/DialogHook` 0x10030d50/0x10031000/0x10031570; FBEV data | **Implemented:** YootTower fires begin at 12:00 every 60 days at grades 3–4 (FBEV), helicopter $300k (heliPrice × 100), no security/cathedral gate and no 16:00 clock jump; unfought fires stop at 18:00 (endMin). Reuses the SimTower fire model and its two-button dialog (helicopter / guards on foot); the "do nothing" choice and T2Fire spread rules are not separate. | PARTIAL | H |
| V3 | **Terrorist bomb** (TrEv: Tokyo grades 2–3, Hawaii grade 3; every **36** days at 09:00). The ransom is **$300k** with a deadline of 720 (12:00). The bomb goes into a random tenant that is not fireproof, and security guards search for it. | `T2Terrorist::Start` 0x10063a60; TREV [3000,720,5000] | **Implemented:** 09:00 every 36 days on TREV's levelBit (Tokyo 2–3, Hawaii 3), ransom $300k, detonation at the TREV deadline minute 720 (12:00), no security gate, no clock jump; the SimTower guard hunt and blast are reused. | PARTIAL | H |
| V4 | **Thief / noisy neighbour "Uncle Reggae"** (MaEv, Tokyo, grades 3+, every 20 or 25 days). He visits tenants and produces complaints ("Uncle Reggae is noisy and annoying") until the security staff catch him. | `T2Maru_Reggae` 0x1003a770–0x1003b8e0 | **Implemented (abstracted):** MAEV 129 (Tokyo, grades 3+, every 25 days, 11:00–23:00) picks a random reachable restaurant (category 50 → fastfood/restaurant), preferring occupied ones. Every 3 minutes a tick; every 10 ticks +10 stress to each occupant; every 60 ticks the claim "'Uncle Reggae is noisy.'. There was a complaint from … floor" and, if a Security office exists, a guard is sent (arrival approximated as 3 min/floor), then "Uncle Reggae was caught by the security staff.". He is not drawn as a walker. MAEV 128 (Mr. Maru, every 20 days, cosmetic) is a hook. **Step 14 B:** Mr. Maru (MAEV 128: one VIP walker, 30 minutes per tenant, sound 9003, no stress or income) stays a logged hook. **Implemented (gap closers):** Mr. Maru (MAEV 128) in YootExtrasSystem: a random lobby-connected tenant, 30 minutes standing, 8 minutes dancing with MR_Maru (world WAVE 9003, interactive runs only), 30 minutes, then the next tenant until 23:00. No stress, income or message. He is not drawn (the walker belongs to the people work). **Implemented (walking agent):** Mr. Maru and Uncle Reggae are pool VIPs who walk (T2Maru_Reggae::Start 0x1003a9f0: BornVIP of VIPL 223 / 224, special flag 0x60, in by the lobby) and stand at their tenant instead of entering. Skin::Yoot draws them: Maru with SLDF 353, at the tenant sheet 354 (ChangeStyle(1)) sitting in walk style 0 and howling in walk style 2 during the 8-minute dance; Reggae with SLDF 355, toggling walk style 0/1 every 3-minute tick; his stress, complaints and guard run only while he plays at the tenant. They leave through the lobby (DeadVIP). Deviations: Maru does not try the adjacent tenant when his target is empty (FindNextTenant), and a reloaded event without its walker ends. | PARTIAL | H |
| V5 | **Santa** (SaEv every 12 days, all grades, cosmetic). | `T2SantaClaus` 0x100560b0 | **Implemented:** YootTower Santa follows SAEV: last day of each year (`totalDays % 12 == 11`), all grades, starts at 1440 − width/2 (20:56 in Tokyo) and crosses the world right to left, one unit per minute, at the top floor line. Kegon has no SAEV. | MATCH | H |
| V6 | **World X-events:** Tokyo Baseball (BBEv); Hawaii Wedding (WdEv), Cruise (`T2Cruise`) and DoEv; Kegon Tonbi (ToEv) and HotSpring (HSEv); ClEv clouds; OdEv rooftop decorations. | `WorldDef::MakeXEvent` (Kegon 0x10015270), Tokyo/Hawai MakeXEvent overrides | **Partly implemented:** ODEV rooftop decorations are drawn natively (MakeLoofList/DoDecoration: ornaments 9101–9109 from the world plugin in list order on roofs wider than sprite + 64 px, night pattern after 18:00). Bodies decoded: MREV timetable, BBEV, HSEV, CLEV (`decodeYoot*`). Hooks (logged, TODO in `EventSystemYoot.cpp`): BBEV baseball (needs Stadium), WDEV wedding (needs YootTower Cathedral), MREV metro, CLEV clouds, DOEV dolphins, TOEV kite, HSEV hot spring, CREV cruise. **Step 14 B:** BBEV and WDEV are implemented (R4); CLEV clouds drift across Tokyo and Hawaii (10 clouds from bitmaps 9001-9004, height `G - floor(sqrt(rand % G^2))` rows above ground with G = groundLine - 25, rand%4+2 px right every 10 game minutes, refilled from the left; drawn in front of the tower); the CREV cruise (not in T2EV: Boatdeck.t2p BuildFinish makes it) docks at x 46 for 30 minutes at 07:00/11:00/15:00/19:00 from grade 3 and brings visitors to the Boat Deck; the ship (Hawai.t2p 8800 at 2x) sails in and out at one unit a minute. **Still hooks:** DOEV dolphins and TOEV black kite (cosmetic animation scripts), HSEV hot spring (Kegon, pop >= 100: the first fully underground build unlocks the Kegon Spa, which has no OpenSky item yet). **Implemented (gap closers):** TOEV black kite and DOEV dolphins run the T2Tonbi engine (`Yoot::TonbiFlight`: a flight tried every 512 ticks of 4 per minute, a step every 4 ticks, scripts {pict, count, dy, dx} with pict-0 loops, start points (row 5, unit 123) (6, 111) (7, 102) for the kite, (56, 0) for the dolphins; kite 206 steps, dolphins 260), weather-gated by IsReady (Tokyo rain only; T2Dolphin's cruise gate is dead code), 09:00/08:00 to 17:00, sprites 8600-8611 / 8901-8918 from the world plugin, the kite behind the tower. HSEV: at grade 3 with population >= 100 the first build whose top is at or below the ground line shows "Spa was found underground. Spa can be placed." and unlocks the Spa (W5). Baseball now asks for the team name (dialog 12006, default "Towers", an empty name is refused). **Walking agent:** cruise passengers are pool people of transport 4 who ride the ship in and out (see I7); the Boat Deck no longer counts pool draws at each sailing. | PARTIAL | H (list) / M (rules) |
| V7 | **Blackout** (BOEv, daily) — see I1. | T2BlackOut::Start/Exec 0x1001b620/0x1001b750 | **Implemented** with I1 (BOEV, daily 11:00 check, exclusive). **Oracle:** a save edited to BOEV consumption 8192 (`~/yoottower-decomp/events-agent/blackout.bld`) came back as 94 after a reload and a day run: T2.exe rebuilds consumption from the tenants on load. That supports recomputing demand from items, but no blackout was triggered, so the 11:00–14:00 sequence is still unobserved; it needs a tower that really draws more than 5000. | PARTIAL | H |

### 1.11 Specials, toolbox and messages

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| S1 | **Movie theaters:** Theater ($500k, 500 seats, power 200) and Classic Theater show MV_* movie plugins (SP format). The **Event Center** hosts Pl* hall events and settles `funds += value − cost` at close. | Theater/EventCenter overrides; `world-and-sp-formats.md` §SP | SimTower Cinema / Party Hall. **Implemented (step 14 B):** native Theater (31x3, 500 seats, 10:00-22:00) and Classic Theater (24x2, 40 seats, 12:00-22:00). The player picks the film in the inspector: the Theater offers roadshow films in phase 1 of their cycle (GetMovieLifePhase 0x10043320, cumulative p1/p2/p3 from the tower's first theater day), the Classic offers famous films; the change costs MODF +0xfc (`unkFC`, $20k-$50k). Four shows a day (runningTime + 30), the first door opening sets the target (record, then MODF +0x11c, then capacity; afterwards 95% of yesterday's best, `unk104`), customers are collected until close - bindTime - 60, income `CalcInMoney` at close, and the day's best house becomes the pending R/F that ChkReMake judges at midnight (famous / sequel "PART II" / retired). The clip's PCM sound track plays during shows when Movie/*.MOV is installed (no Cinepak; the still is not shown). Event Center (31x2, capacity 80): one Pl* booking for bookingDays from today (before its doors) or tomorrow within its day mask; visitors are collected 09:00-17:00 only (so the evening Rock Live / Fashion Show draw nobody, as in FUN_1000bec0), funds += cost - outMoney at close (an idle hall pays its outMoney), attendance < unkE0 is a flop with a 36-day cooldown, else the next target is 2 x attendance. Billboard (OODF 20x2): a sponsor ad (the 7 lPdA plugins incl. Aquazone and GAMEWORKSAD) shows for 12 days and pays $50,000 up front. **Oracle:** a Classic Movie Theatre built in T2.exe listed the seven filmKind-2 films, charged $20,000 for Bodyguard Chops, scheduled 12:30/15:00/17:30/20:00 and paid $1,400 at 22:00 from 3 full houses of 40 (`~/yoottower-decomp/venues-agent/`). Visitors are counted, not walked. **Implemented (walking agent):** the audiences walk. Every venue collects with T2Tenant::CollectCustomer from its plugin idle (every 8 minutes, no idle skipped): the pool (PLDF and DPDF holiday rates), tenant and floor sources, with the venue's TMDF 2 or the movie's / hall event's audience table (SetCustomerTable 0x10062c00: bind 1, units = capacity). Theater / Classic Theater (FUN_1000e770): when the doors open and on every idle of a show, target - walking - inside; stay running time + 30 + bind; the show's end pushes everyone out. Event Center (FUN_1000dc70): every idle while open, hall bind + bind. Arrivals count TotalCustomer and closed venues send them back with -100. Deviation: the Event Center counts every leaver as satisfied for the next target (the original weighs them by estimate against world +0x64 / +0x68). | MATCH (walking visitors) | M |
| S2 | **Observatory** (Kegon, $1M, open 08:00–18:00), **Boat Deck** (Hawaii, B2 left edge), **Spa / Gathering Area / School / Ticket booth**. | TNDF + overrides | none **Implemented (step 14 B):** native Observatory (Kegon, 17x2, 08:00-18:00, only at unit 81 / row 40 = B32 left edge, error 75; visitors only through a Ticket Booth, "A Ticket Booth is required ..." otherwise), Ticket Booth (12x1, 09:00-16:00, earns `CalcInMoney` of the tickets it sells), Gathering Area (35x2, 10:00-22:00, no income) and Boat Deck (Hawaii, 20x2, 06:00-20:00, only at unit 56 / row 55 = B2 left edge, error 74; visitors come by the cruise). Visitors use `Yoot::venueArrivals` (capacity x DPDF percent[holiday][rain] per stay period); **step 15:** each arrival is a person called out of the pool with the venue plugin's TMDF 2 table and CheckRouteFromPool mask (cruise passengers only while the ship is docked), who returns at once (not walked). Spa and School are not implemented. **Implemented (gap closers):** native `Item::School` (School.t2p, Tokyo grade 4, 38x2, $1,000,000, 60 students, 08:00-18:00): ChangeStatusOnIdle 0x1000bf30 and Enter/LeaveTenant ported as `Yoot::SchoolModel` (called at open on working days, arrivals over 50 minutes, class at 08:30 and 13:00, lunch at 12:00, 3 sent home per idle from 15:00, everyone out at 18:00; patterns 0-3). Students are counted from the pool, not walked; the colds/doctor part shared with Office.t2p is not applied. The Kegon Spa stays the plugin-backed shop, now locked until the hot spring (W5). **Implemented (walking agent):** Stadium (collects below status 33, until 18:00; people stay until the close; 21:00 halving and quarter push-outs), Cathedral (ten collects on the opening idle; leaving over the last 50 minutes), Attraction and Gathering Area (every idle, TNDF bind) and Observatory (collects for the open booth with the fewest people; the booth queues them, forwards 2 an idle, +15 stress to the rest, home above 229; Ticket.t2p FUN_1000d200 / 0x1000d320) walk their visitors. The School's 60 students are pool people (TMDF 1) who walk in at their CallEmployee minutes and home at the push-outs, and its colds are applied (P6). | PARTIAL | M |
| S3 | **AquaZone** (Aquazone.t2p, about 10 KB) is not a PE item plugin. Its format belongs to the SP agent. | — | none **Resolved (step 14 B):** Aquazone.t2p and GAMEWORKSAD.t2p are lPdA advertise SP plugins (Billboard sponsors, see S1). Gameworks.t2p is an ordinary shop tenant. | MATCH | L |
| S4 | **Toolbox:** categories and order come from data (`GetSortKey` = category×65536 + toolNo, 0x10065a50). The palette chrome is `DefaultToolPalette.t2p`; icons are each plugin's bitmap 100. | `T2ToolDef::GetSortKey` 0x10065a50, `T2ToolWindow` | Hardcoded toolbox tree (`ToolboxWindow.cpp:34`). **Implemented:** YootTower mode shows category buttons (OBJMAP 100 `Category` icons) over the selected category's items in GetSortKey order, with each plugin's own item icon (`ToolboxWindowYoot.cpp`). The DefaultToolPalette scroll buttons are not used (the window grows instead). | PARTIAL | H |
| S5 | **Messages:** an info bar (timed), balloons over people and equipment, alert dialogs, and per-category pay-message rate limiting. | `T2TowerMessage::*`, `T2Settlement::EmitPayToolMessage` 0x10059600 | Single 3 s toast plus modals (`TimeWindow.cpp:298-304`). | PARTIAL | H |
| S6 | **Open/close hours are data** (TNDF +0x174 / +0x178 minutes, e.g. Burger 08:00–22:00, Office 08:00–17:00, hotel check-in 17:00 / out 11:00). | `item_rules.py` (f174/f178) cross-checked with KTrash 0x1000bba0 | Hours are hardcoded per class. **Implemented:** Plan step 4: open/close/stay minutes are in the catalog (`prototype->yoot->openMinute/closeMinute/bindMinutes`) but the item schedules still use their OpenSky hours. **Implemented (gap closers):** offices call their workers at the TNDF open minute (+ (rand % 6) x 10, CallEmployee) and release them from the close minute; hotel arrivals and checkouts follow the room's open/close minutes (same windows as before for the shipped rooms). Condos and Security have no hours (0/0, 06:00/06:00). | PARTIAL | M |

### 1.12 Status counts

| Status | Count |
|---|---|
| MATCH | 4 (T2 calendar shape, E8 payment mechanism, E9 no loans, V5 Santa cadence) |
| PARTIAL | 21 |
| DIVERGENT | 24 |
| MISSING | 22 |

(71 rows; rows with a split verdict are counted under the first status listed. Counts are from the
original audit; steps 0-6 moved T4, T5, R1, R2, W1, W3 and L3 to MATCH and T1, R3, R4, R5, R7, W2 and
M6 to PARTIAL, see the **Implemented:** notes.)

---|---|
| MATCH | 6 (T2, E8, E9, V5, and the 3/4/12 calendar and Santa cadence counted within them) |
| PARTIAL | 25 |
| DIVERGENT | 25 |
| MISSING | 29 |

(85 rows in total. "MATCH" is mostly structural; no YootTower-specific numeric rule is implemented today.)

---

## 2. Generic engine code vs plugin-native code

What can come from data and what needs native C++. The source for each is `overrides.py` (`~/yoottower-decomp/rules-agent/out/overrides.txt`).

| Layer | Lives in | What it means for OpenSky |
|---|---|---|
| Calendar, grade check, VIP, event scheduler, fire, terrorist, Reggae, Santa, blackout, people stress and estimate, tenant estimate formulas, settlement, routing score | **T2DLL generic** | Implement once, natively, parameterised by the world and plugin data. |
| Grade table, funds, bounds, lobby height, dust/toilet/cold thresholds, cash steps, event list and parameters, pools and demand | **World data** (WORLDDEF, WATR, T2EV, `*EV`, PODF/PGDF/PZDF/CODF) | Load from the world `.t2p` (formats: `world-and-sp-formats.md`). |
| Holiday and rain rules, treasure odds, grade messages, X-events (Baseball, Wedding, Cruise, Tonbi, HotSpring) | **World plugin code** | Port as small native functions per world (they are 10–40 lines each). |
| Price, maintenance, power, capacity, in-money levels, hours, noise sense, registry id, max-build, unlock level, world mask, mover span, cars, stops, speed, score | **Item data** (ATTR, TNDF/MVDF) | Data-driven prototypes. All 100 PE plugins decode cleanly with `item_rules.py`. |
| Tenant state machine (`ChangeStatusOnIdle`, `EnterTenant`, `LeaveTenant`), `AreaCheck`, `BuildFinish`, `SearchRange` | **Every item plugin overrides these** | Behaviour classes stay native C++. Generic archetypes can cover most shops and restaurants (they share a template). The specials (PowerRoom, Trash, Restroom, Security, Doctor, Front, Housekeeping, Parking, Subway, SkyBridge, Theater, EventCenter, Stadium, Cathedral, Attraction, Observatory, and the movers) need their own classes. |

---

## 3. Item rule table (excerpt; full output in `out/item_rules.txt`)

Prices and maintenance are in dollars (internal value × 100). "Lvl" is the ATTR `mGameLevel`. "World" is the ATTR `mAttrib` mask: T = Tokyo, H = Hawaii, K = Kegon.

| Item | Lvl | World | Price | Maint/Q | Power | Cap | inMoney ($) | Notes |
|---|---|---|---|---|---|---|---|---|
| Floor / Lobby | 1 | T,H | 500 / 5,000 per unit | 0 / 1,000 per unit | 0 | – | – | attr 0x7980 |
| Office | 1 | T | 100,000 | 0 | 4 | 6 | 1k/2k/3k/4k | open 08:00–17:00, noise 100 |
| Condo | 1 | T | 100,000 | 0 | 4 | 3 | 100k/120k/150k/200k | noise 150 |
| Single / Twin / Suite | 2 | T / T,H / T,H | 30k / 60k / 100k | 0 | 3 / 5 / 8 | 1 / 2 / 4 | 800–3,200 / 1.5k–6k / 4.5k–12k | depend NORF |
| Front Desk / Housekeeping | 2 | T,H | 100k / 50k | 10k / 20k | 8 / 12 | – | – | |
| Burger land | 1 | T,H | 200,000 | 800 | 8 | 20 | 1,000 | 08:00–22:00 |
| Simple / Large / Service elevator | 1 / 3 / 2 | T,H | 150k + 50k / 500k + 150k / 100k + 50k per car | 12k / 20k / 10k | 20 / 30 / 20 | 17 / 40 / 21 | – | max length 15 / ∞ / ∞; cars 6 / 8 / 8; stops – / 8 / – |
| Stairs / Escalator | 1 | T,H | 5k / 20k | 0 / 5k | 0 / 8 | 100 | – | score 50 / 30 |
| Security Room | 2 | T,H | 100k | 20k | 20 | 40 | – | registry 5 |
| Power Room | 3 | T,H | 500k (+500k per transformer) | 50k | 0 | – | – | once |
| Trash Facility | 3 | T,H | 500k | 50k | 20 | 50 | – | registry 2; 07:00 / 07:30 |
| Restroom | 1 | T,H | 10k | 1k | 1 | 5 | – | registry 7 |
| Doctor's Office | 3 | T,H | 500k | 50k | 30 | 20 | – | registry 4 |
| Parking gate / lot | 3 | T,H | 50k / 5k | 10k / 0 | 8 / 0 | – | – | registry 3 |
| Subway | 2 | T | 1M (+500k upgrade) | 100k | 300 | 100 | – | once; lowest floor |
| Movie Theater | 3 | T,H | 500k | 2k/day | 200 | 500 | 3,000 | once |
| Stadium (Tokyo final) | 4 | T | 5M | 20k/day | 300 | 400 | 10,000 | once; top floor |
| Cathedral (Hawaii final) | 4 | H | 3M | 0 | 100 | 75 | – | once; top floor |
| Attraction (Kegon final) | 4 | K | 1M | 1k/day | 50 | 70 | 1,000 | once; top floor |

---

## 4. Implementation plan (dependency-ordered, each step lands on its own)

Every step is gated on `activeRules.ruleset() == BuildRules::Ruleset::YootTower` (or on a rules-profile
object), so SimTower and OpenSky behaviour stays bit-identical. Every step needs a unit test in
`tests/test_main.cpp`.

0. **[done]** **Rules profile seam.** Add `struct RulesProfile` (funds, bounds, grade table, calendar hooks), owned by
   `BuildRules`, and make `Game` read the start funds and bounds through it.
   - Files: `BuildRules.{h,cpp}`, `Game.cpp`.
   - Test: the SimTower and OpenSky profiles return the current constants; the YootTower profile returns a stub Tokyo profile.
1. **[done]** **World loader** (depends on `world-and-sp-formats.md` and 0). Parse WORLDDEF, WATR and T2EV in C++
   (`PluginResources`), port `world_rules.py`, and add a world selector (default Tokyo) with save attribute `world=`.
   - Files: `PluginResources.*`, `Application.cpp` (`--world tokyo|hawaii|kegon`), `Game.cpp` save and load.
   - Test: Tokyo gives funds 10,000,000, grades `[1000,2000,5000,12000]` and lobby height 3; Kegon gives start grade 3.
2. **[done]** **Start funds, bounds and floor limits** (W1, W2, L3): apply the build-area RECT and the above/below limits; use the world's lobby height as the default.
   - Files: `BuildRules.cpp`, `Game.cpp` (placement checks at the `allowsHorizontalFootprint` call sites).
   - Test: the placement predicate rejects x < 36 in Tokyo, and rejects floor 46 in Hawaii.
3. **[done]** **Calendar hooks** (T1, T4, T5): implement the holiday formula and per-world rain. Keep the SimTower clock, but add a minute-linear `Time` mode for YootTower.
   - Files: `Time.{h,cpp}`, `Sky.cpp`, `LevelUp.cpp`.
   - Test: a table of holidays and rainy days for years 1–3, compared with a direct port of the decompiled functions.
4. **[done]** **Data-driven item catalog** (E1, M1–M5, S6, W6): port `item_rules.py` to `parsePluginTemplateDef()` and overlay price, maintenance, power, capacity, inMoney, hours, maxBuild, span, cars, stops and score onto the prototypes in YootTower mode.
   - Files: `PluginResources.*`, `Item/Factory.cpp`, `Item/Prototype.h`, the elevator headers.
   - Test: the parsed StdElev gives price 1500, car price 500, max length 15 and 6 cars; all 100 plugins consume their resource fully.
5. **[done]** **Unlocks** (R5, W3): drive the toolbox lock from ATTR `mGameLevel` and the world mask.
   - Files: `LevelUp.cpp` (`minRatingToBuild` profile branch), `ToolboxWindow.cpp`.
   - Test: in Tokyo, Coin (Kegon-only) is hidden; BCD-Mart (Hawaii-only, mask 0x100) unlocks at grade 3 in Hawaii.
6. **[done]** **Grades and VIPs** (R1–R4, R7): add a grade table with per-grade VIP flags, the VIEV-driven VIP (estimate > 200), checking every tick, and a `grantFinalGrade()` hook.
   - Files: `LevelUp.cpp`, `VipSystem.cpp`, `Game.cpp:1808-1874`.
   - Test: a mocked population of 999 vs 1000 with the flag set or unset; the final hook works only at grade == numGrades.
7. **[done]** **Economy timing and values** (E2–E7): quarterly maintenance at midnight on day 1 from `outMoney`; office rent `3×inMoney` at 04:51–05:00 on day 1; commercial income from customers × inMoney; hotel per-room income.
   - Files: `Game.cpp:60-156`, `Office.cpp`, `Shop.cpp`, `Hotel.cpp`.
   - Test: a ledger fixture over one quarter.
8. **[done]** **Evaluation scale and formulas** (J1–J3, P2): a YootTower JudgeSystem path on a 1–300 scale, with the residential formula and the 70/200 thresholds.
   - Files: `JudgeSystem.cpp`, `Person.cpp`.
   - Test: formula unit tests with noise, price and age inputs.
9. **[done]** **Stress and routing score** (P1, P3): +5 per tick while waiting, the 300 give-up rule, the no-route rule, and mover score × floors.
   - Files: `Person.cpp`, `PathFinder/MapSearchNode.h`.
   - Test: a waiting person reaches 300 after 60 ticks.
10. **[done]** **Data-driven event scheduler** (V1–V3, V5): a T2EV-driven scheduler, plus the fire and terrorist parameters from FBEV and TREV.
    - Files: `EventSystem.cpp`.
    - Test: the schedule over 120 days matches the levelBit and cycleDay rules.
11. **[done]** **Power** (I1, V7): consumption from item data, the Power Room with transformers, warnings, the blackout and its stress.
    - Files: new `PowerSystem.{h,cpp}`, `Item/PowerRoom.*`.
    - Test: supply and demand arithmetic, and the blackout window.
12. **[done]** **Trash, restrooms and doctors** (I2, I3, P5, P6, R6). Front Desk / Housekeeping linking (W4) moves to step 14.
    - Files: new items plus `Person.cpp` needs.
    - Test: no trash facility at grade 3 → warning at 06:50.
13. **[done]** **Placement rules** (L1, L2, M6, I7): final item only on the top floor, subway on the lowest floor, parking gate and lot adjacency, floor removal order, no sky-lobby rule.
    - Files: `Game.cpp` placement.
    - Test: one placement-predicate test per rule.
14. **Native special items** (I4–I6, M8, S1–S2, V4, V6), one PR each. **Part A [done]:** Front Desk, Housekeeping, Parking, Fire Department, Apartment and the Kegon variants (W4, E4, E7, I4-I6, M4). **Part B [done]:** venues, final items, subway, clouds and cruise (S1–S3, R4, V6, I7; M8 rule only): Front Desk with linked rooms, Housekeeping, Parking, Subway timetable, Sky Bridge (needs multi-tower support), Theater and Event Center hall events (needs SP plugins), Stadium/Baseball, Cathedral/Wedding, Attraction, Observatory, Uncle Reggae.
15. **[done]** **People** (P4, I7, S1, S2): the T2Pool visitor pool, CheckRouteFromPool, walking customers of the plugin tenants (pool, tenant and floor sources), pool-drawn venue/cruise/hotel visitors, silhouette and PADF people drawing.
    - Files: `YootPool.*`, `PeopleSystem.*`, `YootTenants.*`, `Item/YootTenant.*`, `YootPeopleArt.*`.
    - Test: pool selection per grade, Check/CheckRouteFromPool rules, Enter, save/load; a two-day tenant run against the Tokyo pool; the `sim_people_yoottower` golden.
V. **Oracle validation** (cross-cutting): for each step, script `tools/yoottower/oracle/run_yoot.sh` on a
   `.bld` (read before and after with `tools/yoottower/bld.py`) to confirm the dynamic behaviour, especially
   E4 rent timing, T4 holidays, R3 VIP arrival, and I1 blackout.

**Ambiguities to settle with the oracle:** the W3 world-mask consumer, the W5 must/secret flags, Y-axis
build-area enforcement, the meaning of the TNDF f170 field, the exact sub-tick rate per speed setting, and
hotel income timing.

---

## 5. Plugin coverage (2026-10-08)

Every one of the 105 PE plugins and what YootTower mode builds from it. Status:
**native** = a YootTower-only OpenSky item written for the plugin; **plugin-backed
family** = one prototype per plugin (`yt_<stem>`, `source/YootTenants.*`,
`source/Item/YootTenant.*`) whose name, footprint, price, unlock (ATTR mGameLevel and
world mask) and art come from the plugin and whose behaviour is the family's ported
ChangeStatusOnIdle / EnterTenant / LeaveTenant; **mapped OpenSky** = an OpenSky prototype
overlaid with the plugin's catalog data (`yootPrototypeLinks()`); **missing** = no item yet.

| Status | Plugins |
|---|---|
| plugin-backed family | 56 (shop 29 incl. Spa, restaurant 22, vending 5) |
| native | 31 (27 items, 3 worlds, the tool palette) |
| mapped OpenSky | 18 |
| missing | 0 |

Plugin-backed families (decompiled with Ghidra, `~/yoottower-decomp/{plugins,tenants}-agent/pout/`):

* **Shop** (Barber.t2p FUN_1000bc20 / 1000bfa0 / 1000c0a0; the Aloha and Orange Julius
  variants only drop the sound): built (9) -> hiring (10, CollectEmployee at the top of an
  hour) -> closed (11/12/13) -> staff called at open-60 (14) -> open (30) once all staff are
  in; customers from the TMDF 2 table every 8-minute idle until close - bindTime - 60;
  patterns 0 closed, 1 open, 2 from one customer, 3 from half the capacity; at close
  CalcInMoney is paid ("Income from shop just came in.") and everyone leaves. Spa runs the
  same machine and pays silently.
* **Restaurant** (Burger.t2p; French drops the sound): the shop machine, but only the
  customers leave at close and the staff carry the garbage out (status 15) before going
  home. Garbage itself is FacilitySystem's hourly delivery (I2), not the plugin's counter.
* **Vending** (Drink.t2p, byte-identical in Drink, KDrink, Cigar, kCigar, Coin): no staff,
  open/close by hours (pattern 1 while open; round-the-clock machines keep pattern 0),
  pays CalcInMoney silently at close.

Customers walk (step 15): every customer is a person called out of the visitor pool
(P4) or from a nearby tenant / the floor. It waits for its start minute (the call minute
+ 2 per person of the member), enters at the entrance of its transport type (lobby;
parking lot; a train or ship due within the hour, which lands it at the station; a
missing entrance costs 100 estimate and uses the lobby), walks along the floors at 16
units a people idle and rides the movers with the YootTower queue stress (P1), stays the
TNDF bind time, then leaves for the next tenant that called it, for its return stack, or
out through the lobby back into the pool (T2Pool::Enter with its estimate). Staff are
hired out of the pool (TMDF 1, CollectEmployee) but commute instantly: the shop opens when
the hire is done. The staff are drawn inside the tenant from OBJMAP 1100 `SHIL` parts
posed by the PADF (SetPeopleAnime). Calibration: see §6.

Oracle check (`~/yoottower-decomp/tenants-agent/`): a Tokyo tower exported by BldTool with
Barber, Flower, Juice, Book Store, Burger, coffee.com, Ramen, Cigarette and Ice Cream
loaded in T2.exe; all opened on day 1 and showed the same View patterns OpenSky draws
(`sbs_shops.png`). It also showed that a tenant exported past hiring without staff people
never opens, so the exporter now writes plugin tenants just built (status 9) with one empty
staff place per TNDF staff member.

| Plugin | Code family | Status | OpenSky prototype / owner |
|---|---|---|---|
| Aloha | Shop | plugin-backed family (shop) | yt_aloha |
| Apartment | Residential | native | apartment (Item::Apartment) |
| Attraction | Venue | native | attraction |
| Barber | Shop | plugin-backed family (shop) | yt_barber |
| BCDMart | Shop | plugin-backed family (shop) | yt_bcdmart |
| BigElev | Mover | mapped OpenSky | elevator-express |
| Billboard | OuterObj | native | billboard |
| Boatdeck | Facility/structure | native | boatdeck |
| Bookstore | Shop | plugin-backed family (shop) | yt_bookstore |
| Boutique | Shop | plugin-backed family (shop) | yt_boutique |
| Burger | Restaurant | plugin-backed family (restaurant) | yt_burger |
| Cathedral | Facility/structure | native | yootcathedral |
| Chinese | Restaurant | plugin-backed family (restaurant) | yt_chinese |
| Cigar | Facility/structure | plugin-backed family (vending) | yt_cigar |
| ClassicTheater | Facility/structure | native | classictheater |
| Cocktail | Restaurant | plugin-backed family (restaurant) | yt_cocktail |
| Coffee | Restaurant | plugin-backed family (restaurant) | yt_coffee |
| Coin | Facility/structure | plugin-backed family (vending) | yt_coin |
| Condo | Residential | mapped OpenSky | condo / yoot_condo |
| Convenience | Shop | plugin-backed family (shop) | yt_convenience |
| DefaultToolPalette | ToolPalette | native | toolbox (ToolboxWindowYoot) |
| Doctor's | Office | native | doctor |
| Drink | Facility/structure | plugin-backed family (vending) | yt_drink |
| Drug | Shop | plugin-backed family (shop) | yt_drug |
| DutyFree | Shop | plugin-backed family (shop) | yt_dutyfree |
| Electric | Shop | plugin-backed family (shop) | yt_electric |
| Escalator | Mover | mapped OpenSky | escalator |
| EventCenter | Facility/structure | native | eventcenter |
| FireDepartment | Facility/structure | native | firedept (Item::FireDepartment) |
| Floor | Facility/structure | mapped OpenSky | floor / lobby |
| Flower | Shop | plugin-backed family (shop) | yt_flower |
| FoodCoat | Restaurant | plugin-backed family (restaurant) | yt_foodcoat |
| French | Restaurant | plugin-backed family (restaurant) | yt_french |
| Front | Hotel | native | frontdesk (Item::FrontDesk) |
| Fruits | Shop | plugin-backed family (shop) | yt_fruits |
| Gameworks | Shop | plugin-backed family (shop) | yt_gameworks |
| GatheringArea | Facility/structure | native | gatheringarea |
| Hawai | World | native | world (YootWorld, YootWorldArt) |
| HawaianCondo | Venue | mapped OpenSky | condo / yoot_condo |
| Icecream | Shop | plugin-backed family (shop) | yt_icecream |
| Indian | Restaurant | plugin-backed family (restaurant) | yt_indian |
| Itarian | Restaurant | plugin-backed family (restaurant) | yt_itarian |
| J-Pub | Restaurant | plugin-backed family (restaurant) | yt_jpub |
| J-Snack | Restaurant | plugin-backed family (restaurant) | yt_jsnack |
| Japanese | Restaurant | plugin-backed family (restaurant) | yt_japanese |
| Jenny's | Restaurant | plugin-backed family (restaurant) | yt_jennys |
| Juicebar | Shop | plugin-backed family (shop) | yt_juicebar |
| kCigar | Facility/structure | plugin-backed family (vending) | yt_kcigar |
| KDrink | Facility/structure | plugin-backed family (vending) | yt_kdrink |
| Kegon | World | native | world (YootWorld, YootWorldArt) |
| KElev | Mover | mapped OpenSky | elevator-standard |
| KFloor | Facility/structure | mapped OpenSky | floor / lobby |
| KIcecream | Shop | plugin-backed family (shop) | yt_kicecream |
| Kids | Shop | plugin-backed family (shop) | yt_kids |
| KoreanBBQ | Restaurant | plugin-backed family (restaurant) | yt_koreanbbq |
| KRestroom | Facility/structure | native | restroom |
| KServ | Mover | mapped OpenSky | elevator-service |
| KTrashFacility | Facility/structure | native | trashfacility |
| Largeroom | Facility/structure | mapped OpenSky | hotel_suite |
| LocalPickle | Shop | plugin-backed family (shop) | yt_localpickle |
| Maintenance | Facility/structure | native | housekeeping (Item::Housekeeping) |
| Men'sClothing | Shop | plugin-backed family (shop) | yt_mensclothing |
| Mexcan | Restaurant | plugin-backed family (restaurant) | yt_mexcan |
| Observatory | Facility/structure | native | observatory |
| Office | Office | mapped OpenSky | office |
| OldTimeCafe | Restaurant | plugin-backed family (restaurant) | yt_oldtimecafe |
| OrangeJulius | Shop | plugin-backed family (shop) | yt_orangejulius |
| Parking | Facility/structure | native | parkinglot / parkinggate (Item::ParkingLot, Item::ParkingGate) |
| Petshop | Shop | plugin-backed family (shop) | yt_petshop |
| Photo | Shop | plugin-backed family (shop) | yt_photo |
| PowerRoom | Facility/structure | native | powerroom |
| Ramen | Restaurant | plugin-backed family (restaurant) | yt_ramen |
| Record | Shop | plugin-backed family (shop) | yt_record |
| Restroom | Facility/structure | native | restroom |
| RFront | Hotel | native | frontdesk (Item::FrontDesk) |
| RMaintenance | Facility/structure | native | housekeeping (Item::Housekeeping) |
| School | Facility/structure | native | school (Item::School, YootExtrasSystem) |
| SeaFood | Restaurant | plugin-backed family (restaurant) | yt_seafood |
| Securityroom | Facility/structure | mapped OpenSky | security |
| ServElev | Mover | mapped OpenSky | elevator-service |
| Singleroom | Facility/structure | mapped OpenSky | hotel_single |
| SkyBrdge | Facility/structure | native | skybridge (Item::SkyBridge, Yoot::constructSkyBridge) |
| Smallroom | Facility/structure | mapped OpenSky | hotel_single / hotel_double |
| SobaNoodle | Restaurant | plugin-backed family (restaurant) | yt_sobanoodle |
| Souvenirs | Shop | plugin-backed family (shop) | yt_souvenirs |
| Spa | Facility/structure | plugin-backed family (shop) | yt_spa (locked until the Kegon hot spring, W5) |
| Sportsshop | Shop | plugin-backed family (shop) | yt_sportsshop |
| Stadium | Venue | native | stadium |
| Stair | Mover | mapped OpenSky | stairs |
| StdElev | Mover | mapped OpenSky | elevator-standard |
| Steak | Restaurant | plugin-backed family (restaurant) | yt_steak |
| Subway | Facility/structure | native | subway |
| Suiteroom | Facility/structure | mapped OpenSky | hotel_suite |
| SuperMarket | Shop | plugin-backed family (shop) | yt_supermarket |
| SurfShop | Shop | plugin-backed family (shop) | yt_surfshop |
| Sushibar | Restaurant | plugin-backed family (restaurant) | yt_sushibar |
| Theater | Facility/structure | native | theater |
| Ticket | Venue | native | ticketbooth |
| Tokyo | World | native | world (YootWorld, YootWorldArt) |
| Toyworld | Shop | plugin-backed family (shop) | yt_toyworld |
| Trad-Japanese | Restaurant | plugin-backed family (restaurant) | yt_tradjapanese |
| TrashFacility | Facility/structure | native | trashfacility |
| Twinroom | Facility/structure | mapped OpenSky | hotel_double |
| Video&DVD | Shop | plugin-backed family (shop) | yt_videodvd |
| Weinstein'sToffee | Restaurant | plugin-backed family (restaurant) | yt_weinsteinstoffee |

The other 51 `.t2p` files in `data/Plugins` are SP-format sub-plugins, not items, so they
have no row above: all are **native (sub-plugin list)**, i.e. read at run time by
`VenueSystem` and offered as choices on a native item: the 38 `MV_*` movies (MoPl) on the
Theater / Classic Theater, the 6 `Pl*` hall events (HePl) on the Event Center, and the 7
ads (AdPl: AirJamaica, Aquazone, GAMEWORKSAD, Ob9003, SandalBeachResort, Segapc, Yoot) on
the Billboard (filtered by `Yoot::advertiseFits`).

## 6. People calibration (oracle, 2026-10-08)

Tower: `tests/sim/yoot_people.tower` (Tokyo, grade 1: a 3-row lobby, 9 offices, one Simple
Elevator, Barber, 1-800 FLOWERS, Juice and Cigarette vending, Book Store, Burger land,
coffee.com, Ramen, Ice Cream), exported with `BldTool export` against Sample Tokyo Map.
The exporter used to copy the template's T2Pool, whose zone clock is about 9 hours ahead
of the save's 00:30 ("template pool"); it now writes the tower's own pool, or the
template's put on the save's clock with fresh search windows (T2PeopleDemandList::Init,
InitSearchLimit 1.0). Its export of this tower has the same pool, byte for byte, as the
hand-aligned save (`people-agent/scripts/align_pool.py`, "aligned pool"), and the original
replays it identically (`~/yoottower-decomp/walk-agent/oracle/`: days 1 and 2 matched `oracle2` customer for customer). OpenSky loads
each .bld's pool through `BldTool import` (`yootPool`). The original ran in the lab
(`tools/yoottower/remote/lab.sh`, `people-agent/lab/oracle_loop.py`, one save per game
hour, read with `bld.py`); OpenSky ran `--sim` (`walk-agent/in/calib.sh`). Customers per
day = TotalCustomer (+0xa8) before the close. "Before" is the branch before the walking
work, "after" the walking work.

Aligned pool, customers per day (days 1-4):

| Tenant | Original | OpenSky before | OpenSky after |
|---|---|---|---|
| coffee.com | 82, 132, 139, 102 | 119, 120, 125, 119 | 128, 78, 157, 130 |
| Burger land | 110, 117, 119, 105 | 75, 76, 85, 72 | 101, 144, 177, 136 |
| Book Store | 86, 107, 111, 87 | 61, 65, 70, 61 | 109, 96, 130, 139 |
| Ice Cream Stand | 52, 55, 67, 57 | 35, 32, 39, 39 | 46, 66, 97, 72 |
| 1-800 FLOWERS | 21, 18, 38, 22 | 21, 26, 26, 24 | 31, 38, 46, 38 |
| Barber Shop | 14, 16, 16, 15 | 15, 15, 14, 16 | 22, 15, 25, 22 |
| Ramen Noodle House | 0, 6, 0, 3 | 9, 4, 17, 26 | 0, 1, 2, 5 |
| Cigarette / Juice vending | 0 (never opened, status 10) | 65, 79, 64, 77 / 35, 39, 35 | 6, 5, 20, 19 / 25, 15, 21 |
| Population (daily min..max) | 0..434, 69..509, 69..559, 69..439 | 0..280, 47..289, 54..338, 59..311 | 0..478, 69..460, 69..663, 69..567 |
| Pool calls a day | 365, 440, 490, 370 | 247, 242, 284, 252 | 409, 391, 594, 498 |

Template pool (the old export): the original had 16-27 coffee.com and 0-20 Burger
customers a day and almost none elsewhere (population 39..55 up to 69..89 when five offices
let on day 4). OpenSky before: coffee.com 1-42, Burger 11-24, Book Store 5-14 (population
0..60 up to 36..76); after: coffee.com 0-30, Burger 0-20, the others 0-2 (population 0..74
up to 45..87).

Why the peak population was about 60% of the original's (each fixed, with evidence):
- **Elevator cars ran about 30 times too slowly in game time** (M7). T2.exe idles every
  mover 4 times a game minute; OpenSky's car advanced 192 frames a game day, one per 7.5
  YootTower minutes, so a 9-floor trip took about 200 minutes instead of 6.5. Hourly status
  counts showed 60-118 people waiting at the elevator (the original 20-72), and almost every
  visitor gave up waiting (stress 300, people-estimate 1): all returns had estimates of
  71..200, so T2Pool::Enter never duplicated them and the pool was called about 250 times a
  day instead of 370-490. With the cars on the sub-tick, most returns are above 200 again.
- **The floor and tenant sources over-fired.** They took people queued at a mover and any
  tenant with visitors; about 190 customers a day came without a pool call (the original:
  0-65, all of it office workers on working days). With positions, only people walking on
  the floor turn in, and SelectNearTenant searches one floor near one mover.
- **A full car could stall forever** (moving to the floor it stood on), keeping 17 leaving
  visitors overnight.
- **Six imported offices never hired.** A .bld office of status 11 with a kind but no
  belong people imported as let with population 0; the night population was 33, not 69
  (54 office workers + 15 shop staff). They now hire at the next Office.t2p window.

Open points: OpenSky's holiday (day 3) is busier than the original's (peak 663 vs 559;
Burger 177 vs 119); exported vending machines stay in status 10 in the original (Drink.t2p
has no case for it), so their counts are not compared. Venues, hotel guests, riders and
students are not part of this tower; `tests/sim/yoot_visitors.tower` (golden
`sim_visitors_yoottower`) covers them in OpenSky only.

## 7. Parity campaign

**Status summary:**
- **Matched / within tolerance:** people tower peaks (510/480/608 vs 434/509/559 after the appointed-queue elevator dispatch), venues/hotel/Hawaii peaks within tolerance after the BldExport waiting-map fix (the old 288 oracle value was an invalid export, see §7.4 item 5), Uncle Reggae text/time, blackout start message 11:01.
- **Open items:** Event Center evening events unevidenced, blackout 11:00-14:00 end and hotel income 12:00 unobserved, Large Elevator top gear unrun, Maru walk-back, hotel guests and venue audiences inactive in the fixtures, Hawaii condo residents and facility staff counts, per-car rider counts not re-measured, rubble/clouds/kite/dolphins/Billboard visuals not compared, Tokyo visitors Gathering Area export rejected by the original, hotel stays not saved across reload.

Harness: `tools/yoottower/remote/parity/` (`parity.sh run TAG DAYS TOWER...`, then
`report` / `recompare`). Each tower is exported once to a .bld save and both games start from
it: the original through the Wine oracle (one save per game hour), OpenSky through `--sim`.
"Before" is OpenSky at `64f110f` (the branch before this campaign), "after" is the branch
head. The original's series is the same in both tables. Population peak = the daily maximum
of the tower population (the people counted in the tower); each cell is original / OpenSky,
and `x` marks a difference beyond the tolerance (`compare.py` TOL).

### 7.1 Population peak per day (3 game days; day 2 is a holiday)

| Tower (run) | Day | Original | OpenSky before | OpenSky after |
|---|---|---|---|---|
| yoot_people (s7) | 0 | 434 | 467 | 336 x |
| | 1 | 509 | 478 | 317 x |
| | 2 (holiday) | 559 | 596 | 419 x |
| tokyo_g4_blackout (s7) | 0 | 308 | 350 | 294 |
| | 1 | 313 | 386 | 268 |
| | 2 | 373 | 457 | 466 |
| tokyo_g3_reggae (s7, days 11-13) | 11 | 302 | 827 x | 814 x |
| | 12 | 428 | 793 x | 871 x |
| | 13 | 428 | 1054 x | 967 x |
| tokyo_g3_venues (s8) | 0 | 288 | 823 x | 726 x |
| | 1 | 288 | 860 x | 902 x |
| | 2 | 288 | 1041 x | 1065 x |
| tokyo_g2_hotel_subway (s8) | 0 | 645 | 1008 x | 936 x |
| | 1 | 703 | 1426 x | 1380 x |
| | 2 | 935 | 2165 x | 1826 x |
| hawaii_g3_boatdeck (s8) | 0 | 540 | 645 | 630 |
| | 1 | 632 | 724 | 754 |
| | 2 | 977 | 923 | 819 |

The venues tower's original peak reads 288 on every day. That is suspicious (the same value
each day in the oracle's snapshots), so its row is not evidence yet (superseded by the
corrected-map run in §7.4 item 5, where all peaks are within tolerance).

### 7.2 Within-tolerance counts (report.md, before / after)

| Tower | Funds | Pop max | Pop min | Estimate | Customers |
|---|---|---|---|---|---|
| yoot_people | 4/4 / 4/4 | 4/4 / 1/4 | 4/4 / 4/4 | 37/69 / 38/69 | 18/26 / 13/25 |
| tokyo_g4_blackout | 4/4 / 4/4 | 4/4 / 4/4 | 4/4 / 4/4 | 47/60 / 46/60 | 6/10 / 8/10 |
| tokyo_g3_reggae | 2/4 / 2/4 | 0/4 / 0/4 | 1/4 / 1/4 | 190/304 / 194/304 | 7/30 / 10/32 |
| tokyo_g3_venues | 4/4 / 4/4 | 1/4 / 1/4 | 4/4 / 4/4 | 197/312 / 200/312 | 12/32 / 8/29 |
| tokyo_g2_hotel_subway | 4/4 / 4/4 | 0/4 / 0/4 | 0/4 / 0/4 | 177/376 / 186/376 | 21/67 / 19/64 |
| hawaii_g3_boatdeck | 1/4 / 1/4 | 3/4 / 3/4 | 1/4 / 1/4 | 180/190 / 182/190 | 45/58 / 44/59 |

### 7.3 Changes in this campaign

- **Hall events (Event Center):** the collect window is [open, close - 60) (WithinMinutes is
  half open, `T2Tenant::CollectCustomer` 0x1005f440), and the next event value is
  2 x good + medium leavers (LeaveTenant hook FUN_1000bcb0), not 2 x visitors.
- **Plugin restaurants** match the find-target category (+0x38 = 50).
- **Simple elevators** hold `defaultWait` idles at each stop and drive in gears
  (StdElev FUN_1000bf30 / c1b0 / c200 / c3b0 / c590, FUN_1000cad0). The value is evidenced:
  T2MoverDef +0x174 is copied to T2Mover +0x68 (InitMover) and is the stop counter. The
  StdElev.t2p value is 3 idles, speeds 1 / 3 / 0.
- **Called staff** walk from the visitor pool before opening (CallBelongPeople 0x1005fb80,
  start (rand x 10) % 6 minutes on) instead of appearing at once.
- **Mr. Maru** walks on to the next tenant on his row (FindNextTenant 0x1003b760, DoReception
  0x1003abe0). The original turns round once at the row end; OpenSky turns without the walk back.

### 7.4 Findings and open mismatches

1. **The elevator change lowered the people tower's population [superseded]:**
   *(Superseded by appointed-queue dispatch; see update below).*
   The elevator change lowered the people tower's population (yoot_people: peak 596 -> 419
   on the holiday). The mover means show 16 / 13 / 24 riders against the original's 38 / 53 /
   39, and the pool calls drop from 398 / 409 / 527 (before) to 282 / 263 / 365 after (original
   365 / 440 / 490; the elevator change alone gives 302 / 267 / 380).
   Experiment on the same save (3 days, `--sim`, not committed): the gear drive with the old
   stop hold (`schedulePatienceTicks`) gives pool calls 329 / 431 / 456 and peaks 398 / 500 /
   525, all within tolerance; the gear drive with the 3-idle hold gives 371 / 336 / 449. So the
   3-idle stop hold is evidenced, but the boarding cycle that surrounds it is not yet matched:
   the original's cars carry about twice the riders. The next step is the door and boarding
   timing (Car::advance door states against FUN_1000bf30's arrival branch), checked with the
   people timeline, not with the peak alone.

   **Update (superseded by appointed-queue dispatch):** appointed-queue dispatch
   (StdElev FUN_1000cf90 / cda0 / d070: pickup sets, sweep cost, end-of-window unappoint)
   restores the people tower's peaks to 510 / 480 / 608 against the original's 434 / 509 / 559
   (all four days within tolerance, estimate 42/69 against 37/69 before the campaign).
   Rider counts per car were not re-measured in this population campaign.
2. **Uncle Reggae matches in text and time.** Both games print "Uncle Reggae is noisy" at
   14:32 and "Uncle Reggae was caught by the security staff" at 14:47 on day 11. The Maru walk
   and reception are not compared step by step.
3. **Blackout:** both games print "Power down ... please add a power room" at 11:01 on the
   5213-power tower (tokyo_g4_blackout). The 11:00-14:00 sequence was not run to its end in the
   3-day oracle run, and the series' blackout field is not compared yet.
4. **Event Center evening events (item 2):** not evidenced. The Event Center stays in status 10
   (never opened) in both games in the venues run, so no evening event was drawn in either.
   The collect window change is evidenced by the decompile only.
5. **Shop staff, Event Center leavers, venues and hotel [superseded]:**
   *(Superseded by BldExport waiting-map fix; corrected three-day peaks are within tolerance, see below).*
   The s8 venues/hotel population comparison used an invalid exported elevator waiting map (the old 288 oracle value was an invalid export). The original's
   constant 288 is real: every saved snapshot contains 264 office people, 24 staff,
   zero general visitors and zero deferred departures. People/tenant states still change;
   this is not a stale counter. The second shaft's queue overwrote the first shaft and
   boarding cells (lobby row 109, x176 resolves request 1026 instead of the first shaft).
   T2People::IsRegistRequest (T2DLL 0x10049380) looks up that cell; T2Request::Regist
   (0x10052fe0) rejects the wrong boarding coordinate, leaving staff in SolveNoRoute.
   BldExport now preserves shafts and partitions opposing waiting areas at the midpoint,
   following T2FloorInfo::FillRequestID (0x10034ab0) and CalcRequestLeftEnd/RightEnd
   (0x10034ef0 / 0x10034e50), including the odd cell on the right shaft's side.

   **Corrected three-day comparison (days 0 / 1 / 2):** these rows supersede the s8
   population rows above. OpenSky's imported geometry already routes both shafts;
   its peaks are unchanged before/after the export fix. Hawaii's single-shaft input
   is byte-identical, so its stored oracle was reused. All nine daily peaks are within
   the existing harness tolerance; no population tuning or golden regeneration was needed.

   | Tower | Original, old input | Original, corrected input | OpenSky before = after |
   |---|---|---|---|
   | tokyo_g3_venues | 288 / 288 / 288 | 710 / 765 / 1049 | 756 / 884 / 1006 |
   | tokyo_g2_hotel_subway | 645 / 703 / 935 | 940 / 1417 / 2095 | 990 / 1379 / 1870 |
   | hawaii_g3_boatdeck | 540 / 632 / 977 | 540 / 632 / 977 | 627 / 731 / 920 |

   **What the peak counts:** T2Pool::RecoverPeople (0x10050790) adds population;
   ShrinkPeople (0x10050870) defers subtraction until DayChanged (0x100504a0).
   Thus departures accumulated since midnight are a large part of the peak, not live
   venue attendance. The harness now extracts saved pool/shrink counts, live person
   origins, demand and transport types; OpenSky dumps expose the corresponding counts
   only when requested, with unknown types for legacy item populations. At each game's
   sampled day-2 peak, the additive breakdown is:

   | Tower / game | Office or condo | Staff | General visitors | Deferred departures | Total |
   |---|---|---|---|---|---|
   | venues / original | 306 | 24 | 44 | 675 | 1049 |
   | venues / OpenSky | 294 | 20 | 75 | 617 | 1006 |
   | hotel / original | 282 | 74 | 216 | 1523 | 2095 |
   | hotel / OpenSky | 276 | 53 | 218 | 1323 | 1870 |
   | Hawaii / original | 60 | 43 | 18 | 856 | 977 |
   | Hawaii / OpenSky | 0 | 30 | 48 | 842 | 920 |

   The old hotel oracle had only 250 deferred departures on day 2, versus OpenSky's
   1323: blocked routes suppressed shop turnover, rather than excessive hotel guests.
   Corrected-map general visitors are 216 versus 218. No hotel guests or venue audiences
   are exercised: hotel rooms and the Event Center/theaters remain status 10; selected
   films/hall events are not serialized by the exporter. Customer units, arrival rates,
   bind/stay times and CheckRouteFromPool masks were checked against the existing
   decompile-backed formulas, without changing unsettled numbers. Active venue/hotel
   fixtures are still needed before their demand can be calibrated.

   Residual composition gaps remain: OpenSky omits facility staff (doctor 4 in venues;
   security/frontdesk/housekeeping 21 in Tokyo hotel), and Hawaii condo residents fall
   from 115 on day 0 to zero versus the original's steady 60. Visitor recycling and
   route scoring are likely further contributors: original queue scores include
   +10 per waiting person (T2DLL 0x100427b0), absent from OpenSky's pool route check.
   These are separate unresolved causes, not evidence for a capacity/rate multiplier.
   Lab artifacts: /lab/out/parity/cxpop-before/<tower>/report.md and
   /lab/out/parity/cxpop-fixed-map/<tower>/report.md, with series and saved snapshots.
   The paired-elevator export regression and all 9 ctest checks pass; SimTower/OpenSky
   and YootTower goldens are unchanged.

   **Earlier elevator update:** (see item 1 above) appointed-queue dispatch (StdElev FUN_1000cf90 / cda0 /
   d070: pickup sets, sweep cost, end-of-window unappoint) restores the people tower's
   peaks to 510 / 480 / 608 against 434 / 509 / 559 (all four days within tolerance,
   estimate 42/69 against 37/69 before the campaign). Rider counts per car were not
   re-measured in this population campaign.
6. **Holiday population (item 3) [superseded]:** *(Superseded: appointed-queue dispatch restores
   the holiday peak to 608 vs original 559, within tolerance; see item 1).* The original's day-2 peak is 559, OpenSky's is 596 before
   and 419 after the elevator change. The 663-versus-559 gap of the earlier calibration is gone
   from the peak, but the regression above now puts the holiday under the original.

### 7.5 Second parity campaign

Hotel rooms must be exported at status 30 (`bdac8e0`): at status 10 the original
never opened them. With the corrected export, Hawaii hotel revenue is $45,000 on
day 1 and $54,000 on day 2, versus OpenSky's $49,500 and $72,000. Room rates match;
room-night supply/recycling remains unresolved. `tokyo_g2_hotel_subway` has zero
hotel guests in both games.

The blackout export fix (`9f5da51`) rewrites the `vEOB` consumption counter, which
is saved state, to 5,833 against supply 5,000. **Correction to §7.4 item 3:** the
original did not print "Power down" or black out on the old export. Only the
consumption rewrite makes the original start the blackout (11:00; OpenSky 11:01).

The 14:01 "Note" is an emergency escape, not restored power. `T2BlackOut::Start`
(0x1001b620) adds 30 stress; `Exec` (0x1001b750) advances status 1 to 2 at 14:00,
then runs the emergency and adds 300 stress. Status 3 persists until consumption
is strictly below supply, or `BreakerOn` requests recovery. `Write` (0x1001ba50)
saves status, consumption and supply. In `gp2-blackout4/oracle/snaps`, `vEOB#128`
contains `0100c9168813` at 11:00 and `0300c9168813` after 14:01 and throughout day 2:
status 1/3, consumption 5,833, supply 5,000. This is a lasting blackout, not merely
a stranded pool after power recovered. Day 2 has 74 people in the pool, five
inside, and none walking. The purported blackout check near `T2Tenant::Idle`
is actually in `T2Tenant::Draw` (0x1005e4d0), where it darkens the tenant.

OpenSky now gates route queries (including cached pool reachability) and elevator
service on the YootTower blackout state. The emergency stairs clear active trips,
retain staff/resident reservations and remove general visitors without ordinary
pool recycling, following `T2People::BreakoutEmergency` (0x1004a770). This is an
OpenSky adaptation of the emergency path, rather than a full per-person port.
Simulation ticks that skip precisely 14:00 still schedule the escape. Recovery
remains driven by supply/breakers, with regression checks for persistence and
recovery; SimTower/OpenSky rulesets are unaffected.

Reusing the corrected three-day oracle with `pipeline.py --skip-oracle`, daily
population peaks change from **402 / 362 / 464** to **136 / 79 / 79**, against
**140 / 79 / 79** in the original (pop max 4/4 within tolerance, previously 1/4).
OpenSky stops gaining population during the blackout and matches the later
resident/staff-only population. Funds remain within tolerance (4/4), but tenant
charges still diverge: day-1/day-2 deltas are -$12,300/-$10,000 versus zero.
Tenant estimates remain mismatched (24/60 within tolerance); matching population
does not establish full tenant-state parity.
Artifacts: `/lab/out/parity/gp2-blackout4/report-codex-escape.md` and
`codex-escape/series.jsonl`, paired with the retained `oracle/` snapshots.

Remaining open items: Large Elevator top gear; Event Center evening events
(the fixture never opens); per-car rider counts (original 38 / 53 / 39);
hotel room-night counts; and blackout tenant-idle/financial effects. Schools
remain status 10 in the original because their opening route is unreachable.
No goldens were regenerated.

### 7.6 Close-out campaign

#### 7.6.D Blackout tenant effects

**Mechanism (decompile).** The lasting blackout is the exclusive BOEV event.
`T2BlackOut::Exec` (0x1001b750) returns the event's exclusive flag (1) in states 1-3,
and state 3 persists while consumption is not below supply (`tokyo_g4_blackout`:
5,833 against 5,000). `T2TowerEvent::Idle` (0x10069970) returns 1 while such an
exclusive event is executing; `T2WorldDef::IdleProc` (0x100700d0) is that call. The
main idle `FUN_0041dc40` (T2.exe, rules-agent/out/exe3/T2.exe.decomp.c) runs
`T2TenantArrayList::DispatchIdle` (0x10061c70) and the OutObj and template dispatch
only when that world idle returns 0. People, movers and requests are dispatched
regardless, and the day-change `T2Settlement::Update` and `CalcMentenanceCost` run
before the block, so quarterly maintenance is still charged. `T2Tenant::Draw`
(0x1005e4d0) only darkens tenants (`DrawColorMesh` when `T2WorldDef::IsBlackOut`);
it does not change status or money. The earlier note that the blackout check sits
in `T2Tenant::Idle` was wrong: `T2Tenant::Idle` (0x1005e670) has no blackout test.

**Oracle evidence (`gp2-blackout4/oracle/series.jsonl`).** From 11:00 on day 0 to the
end of day 3, no tenant changes status, customers, estimate or staff. Office status 13
and estimate 300 (one 270 snapshot at 11:00), shop status 30 with estimate 17, and the
Event Center, theaters and schools stay status 10. Funds change only by the day-3
quarterly maintenance. Day 0 income ends at 36,000 (the last change is at 10:08); days 1
and 2 have 0.

**OpenSky before (`gp2-blackout4/codex-escape`).** The extra charges are all tenant
settlements after 11:00, and they are the only differences in the money:

| Day | Entry (message, time) | Source |
|---|---|---|
| 0 | Event Center expenses $-10,000 @18:01 | `VenueSystem::eventCenterTick`, `pay(e, dollars)` on an unopened hall (outMoney) |
| 0 | shop $-600 @20:01; restaurants $-500 @22:01, $-800 @22:05 | `commercialIdle` close `pay` (`YootTenant::advance`) |
| 1 | Event Center -10,000 @18:01; shop -600 @20:01; restaurants -800 @22:01, -900 @22:05 | the same paths; total -12,300 |
| 2 | Event Center -10,000 @18:01 | the Event Center path only; total -10,000 |

Estimates also fell during the dark: offices from 300 to 1 at 00:10 on day 1, and the
"tenants are unhappy" and "has been unhappy" messages on days 1-3. These come from
`JudgeSystem::evaluateAll`, called by `Game::settleDailyAccounting`. Coffee also dropped to 1
at 00:01 on day 1 (the original keeps 7 after its 10:08 change).

**Change.** `EventSystem::tenantIdleFrozen()` (yoot and `power.isBlackOut()`) gates the
tenant idles. `YootTenant::advance` (and `Office`, `YootCondo` advance) return early,
and `VenueSystem::advance` and `FacilitySystem::advance` skip their minute ticks while
still advancing `lastMinute`, so skipped minutes are not replayed after recovery.
`settleDailyAccounting` skips `evaluateAll` while frozen. Commit `c57e564`. SimTower and
OpenSky are unaffected (`yoot` gate). The elevator freeze already present in `Game.cpp`
is unchanged. Tests: 9/9.

**Measured (tokyo_g4_blackout, 3 days, reused oracle, `--skip-oracle`).**

| | Before (codex-escape) | After (`hkD-bo2`) | Original |
|---|---|---|---|
| Funds delta day 0 / 1 / 2 / 3 | 24,100 / -12,300 / -10,000 / -2,505,000 | 36,000 / 0 / 0 / -2,505,000 | 36,000 / 0 / 0 / -2,505,000 |
| Tenant estimates within tolerance | 24 / 60 | 56 / 56 | |
| Customers within tolerance | 3 / 3 | 2 / 3 | |
| Population peaks | 136 / 79 / 79 | 136 / 79 / 79 | 140 / 79 / 79 |
| Day-1/2 events | 2 shop/restaurant + EC charges, unhappy messages | none beyond weather | none |

The customers column drops from 3/3 to 2/3 because the coffee count stops at 20 when
the tenants freeze at 11:00. Before, the same tenant still gained three arrivals during
11:00-11:10 (23), which is why it looked close. The original's count stops at 31 at 10:08.

Report: `/lab/out/parity/hkD-bo2/tokyo_g4_blackout/report.md` (`opensky/series.jsonl`
alongside). Run with the same `input.bld` and oracle as `gp2-blackout4`.

**Schools (item 3).** All 45 schools stay status 10 with zero customers, staff and
income on every day in both games (`report.md`). OpenSky's school is a YootVenue that
never reaches an opening route, so no change was needed.

**Residuals (not blackout effects).** Office staff: the original reports 0 workers
through the dark period while OpenSky keeps 6 per office (a pre-blackout staffing
difference; the freeze holds it, it does not create it). Coffee: OpenSky's estimate is
1 from 07:10 on day 0 and never reaches the original's 7 (the original changes 1 to 7
at 10:08), and its customers are 20 at 11:00 against the original's 31 at 10:08, so the
pre-blackout demand is still short (`customers 2/3`). The original's customer count stops
rising at 10:08, before the blackout, which OpenSky does not reproduce. The Event Center evening events are still unobserved.

#### 7.6.F Visuals

Scope: Mr. Maru's walk back, rubble, clouds, the black kite and dolphins, and the
Billboard. Logic-level comparison only. No pixel comparison was made and no oracle
run was made for this workstream. The lab tree is `hkF`.

**Mr. Maru (MAEV 128) walk back.** Original: `T2Maru_Reggae::DoReception` 0x1003abe0,
`FindNextTenant` 0x1003b760, `FindTargetTenant` 0x1003b660, `MaruReception` 0x1003aec0.
- Arrival at a tenant is not gated by the clock. The clock gate
  (`time <= this+0x50 && status unchanged`) applies to the stand phases only.
  So a walk still under way at the end time arrives first.
- At state 5, `FindTargetTenant` is chosen only while `time < endTime`. Otherwise
  the state becomes 0xb with destination tenant ID 1 (the lobby). He walks there as
  an ordinary people walk, then the state-1/8 exit path reaches `DeadVIP`. So the walk
  back starts from the tenant where he stands and ends at the lobby, where he is removed.
- Fixed in `YootExtrasSystem::maruMinute` (commit 758e109). A walk still under way
  at the end time is finished, and then he walks home from that tenant. Before the
  fix, `vipLeave` called `goOutTower` on a walking person, so he vanished mid-floor.
- Not matched: `FindNextTenant` flips direction at the row end and returns the
  current tenant as a zero-length destination. The original therefore repeats the
  visit at the edge tenant before walking back. OpenSky flips and walks back at once.
  Whether the repeat is a full reception depends on the tenant flag at `+0x60`, which
  is not settled. The walk speed is the generic people walk in both games, and it
  is not compared numerically.

**Clouds (V6, CLEV, Tokyo and Hawaii).** Original: `T2Cloud::Exec` 0x100130b0,
`CheckInView` 0x10013260, `MakeCloud` 0x100134d0, `GetUnuseCloud` 0x100133d0,
`T2CloudObject::Tick` 0x100137a0, ctor 0x100135d0.
- Ten clouds at all times. Each ticks once per 10 game minutes by its own speed
  (`rand()%4+2` px, set once). Out-of-view clouds are reused via `GetUnuseCloud`.
  The reused object keeps its bitmap and speed. Its entry x is `1 - width`, and its
  height is `G - floor(sqrt(rand % G^2))` with G = groundLine - 25.
- The OpenSky `VenueSystem::cloudTick` matched this except that it re-rolled bitmap
  and speed on each recycle. Fixed (commit 758e109). That change consumes fewer
  `rand()` calls, which shifted the shared random stream, so the sim goldens
  `sim_people_yoottower` and `sim_visitors_yoottower` were regenerated (see the
  commit message).
- Not checked: whether the original's width comes from the "Cloud" OBJMAP object
  (`GetObjectSize`) rather than the full bitmap, and the world origin offset.

**Rubble (demolition and fire).** Original: `T2Tenant::BurntDown` 0x10061020 sets the
tenant ID to 10000. `T2Tenant::Draw` 0x1005e4d0 then draws only
`T2WorldDef::DrawRubble` 0x10072290 → `T2TowerEvent::DrawRubble` 0x1006a1e0, which
draws the world 1007 "Rubble" object. The tenant graphic is not drawn. The object is
clipped to the tenant's width from its left edge, one 36 px row per storey.
OpenSky draws rubble per fire or bomb cell (`EventSystem::destroyFireCell`,
`detonateBomb`) with an 8 px column of the 640 px strip at phase `x*8 % 640`. It
draws on top of everything (`Game.cpp`, `win.draw(eventSystem)`), and it does not
hide the items under the rubble. Differences to check on screen: the phase is global
in OpenSky and per tenant in the original, and the original hides the burnt item's
graphic. Not changed: the `EnumParts` tiling semantics of `T2ImageObj::DrawObject`
0x1003a390 are not settled.

**Black kite (TOEV, Kegon) and dolphins (DOEV, Hawaii).** Original: `T2Tonbi::Exec`
0x100149f0 (Hawaii; Kegon 0x100134b0). It runs every 512 ticks while idle (mask 0x1ff)
and every 4 ticks while flying (mask 3). Each start point is random from the list.
`T2Tonbi::IsReady` tests the weather state at `+0x18` against {3, 4, 9, 10}.
OpenSky `YootExtrasSystem::flightMinute` uses 4 ticks per game minute, the same
kite and dolphin scripts, and the same start points.
- Sprite sheets match the stacked-strip assumption: Kegon 8600 is 44x528 (12 frames
  of 44 px) and Hawaii 8901 is 64x648 (18 frames of 36 px). The OBJMAP rects are not
  decoded, so the frame cut is inferred rather than read.
- Not matched: OpenSky gates on `rainyDay` in Tokyo only. The original applies the
  weather test in every world, Kegon included, so the Kegon gate is unevidenced.
- The dolphin `IsReady` (`T2Dolphin::IsReady` 0x10013e50) tests a cruise transport
  arrival, and the OpenSky comment calls the cruise gate dead code. That is
  consistent with the decompile but not confirmed in play.

**Billboard (BILL, OuterObj 2100).** Original: the Billboard.dll day counter
`FUN_1000bd20`, the campaign end `FUN_1000bba0`, and the ad draw `FUN_1000c1a0`.
OpenSky `VenueSystem::newDay` counts one day per new day and clears the ad at zero,
and `Billboard::showAd` centres the 128 DIB on a 160x60 board.
- Unresolved: as decompiled, `FUN_1000bd20` decrements only when
  `now < stored`, but `stored` is overwritten with `now` on every call, so the
  literal reading never counts down. The Ghidra output or the unsigned comparison
  probably needs a second look before OpenSky's countdown can be confirmed. The
  board size of 160x60 is taken from a comment and is not measured.

**Verification status.**
- Done: build clean on `hkF`; `lab.sh test hkF` 9/9 after the golden regeneration.
- Not done: no oracle or OpenSky screenshots, and no state dump of Maru, clouds
  or the flights. No sim run logs a Maru visit, so the Maru change has no
  measured effect. Pixel and motion checks of the clouds, rubble, kite, dolphins and
  Billboard need a human look or a lab screenshot pass, at these times: Tokyo cloud
  drift at 10-minute steps, a Hawaii dolphin run from 09:00, a Kegon kite run, a
  burnt tenant on Tokyo, and a Billboard after its campaign ends.
- Gotcha: a fresh worktree has empty `thirdparty/astar-search` and
  `thirdparty/tinyxml2` submodules, so the lab build fails at `PathFinder.h` until
  they are copied into the lab tree. `lab.sh sync` deletes them again on the next sync.

#### 7.6.E Gathering Area export

Problem: a `.bld` exported from a three-star tower with a Gathering Area (`tests/sim/yoot_visitors.tower`, rating 2) made the original show "Plugin Check Error" at load. The dialog lists "Gathering Area" under "Plugin files below were installed at saving, but they cannot be installed" (screenshot from oracle run hkE-repro, load, before fix).

Cause (T2.exe, plugins-agent/out/t2exe.c): the load builds the installed list with FUN_00421f70, which keeps only plugins whose CFilePlugin+0x54 (ATTR lvl, game level) is at most the header level (+0x8c). The save table lists the Gathering Area (IOKI, itemType 1741, lvl 4), and the list check at 0x419d62 reports every save plugin missing from that installed list in the non-fatal 0x457 dialog (Continue). FUN_0041ca80 gates availability the same way (spec.level <= doc grade; mirrored by YootWorld.cpp yootPluginAvailable). The exporter wrote header level 3 (rating 2 + 1), so the plugin was refused. The plugin record itself already matched a game-written save (tk1.bld, vis3-agent/towers): lPnT IOKI, 1741, mustItem 0, name, C:\YOOT\PLUGINS\GATHERINGAREA.T2P, and the tenant record matched too; the level was the only difference.

Fix (source/BldExport.cpp): after the plugin table is assembled, the star level is raised to the highest ATTR lvl of the plugins the export adds (when above the level it would otherwise write, and within the world grades), with a report note. BldLoader maps the level back to rating (level - 1), so OpenSky imports the same star level. Tokyo towers stay at their star level when every added plugin is grade 3 or lower.

Test (tests/test_main.cpp, testBldExport section): a Hawaii rating-2 tower with a gatheringarea exports level 4 with the note "Star level 3 raised to 4"; lab test hkE: 9/9 passed.

Measured on the lab (tree hkE, TAGs hkE-repro and hkE-fix):
- before: export level 3, the original shows the Plugin Check Error dialog listing only Gathering Area at load.
- after: export level 4, the original loads with no dialog and the status panel shows four stars (oracle dialogs/ empty, load.png).
- one-day run (hkE-fix, 1 day, TAG hkE-fix, output /lab/out/parity/hkE-fix/yoot_visitors): finished, oracle exit 0, no dialogs during the run. report.md: funds change day 0 25,700 orig / 27,300 OpenSky, day 1 -500 / -500; population peak day 0 511 / 627 and day 1 78 / 78; grade 4 / 4 both days; within tolerance funds 2/2, pop_max 2/2, pop_min 2/2, grade 2/2, vip 2/2, customers 6/10, estimate 33/38.
- Gathering Area survives in the original: gatheringarea@-36,13 status 13 (orig) / 13 (OpenSky) on day 0 and 177 customers (orig) vs 258 (OpenSky) on day 0 with an x mismatch flag on customers.
- Not measured: the pre-fix run (hkE-repro) was stopped before its report, so the before numbers for funds, population and the Gathering Area's customers are not available; the before evidence is the Plugin Check Error dialog only.

Open: the star level change is a deliberate departure from the source rating (the tower is three stars in OpenSky); the alternative, dropping the Gathering Area, loses the building. The lead should choose. Lab note: an earlier oracle process for hkE-repro did not exit on `parity.sh stop`; it was left running and should be checked.

#### 7.6.A Elevators

The worktree was fast-forwarded to `feature/yoottower-parity` (f660c88) before work started. Lab tree: `hkA`.

**1. Large Elevator top gear: no discrepancy in OpenSky. The "3 floors" premise is stale.**

- Original gear table: BigElev.t2p MVDF speed `[1, 3, 12]` (`rules-agent/out/item_rules.txt`; ServElev is the same, StdElev is `[1, 3, 0]`). `T2MoverDef::GetSpeed` (0x10004990) reads `+0x178 + 4*gear`. A car position is in units of 36 per floor (0x24). Each mover idle adds `speed[gear] * 6` (StdElev FUN_1000c200 / c3b0 / c590, with `speed[0..2]` at offsets 0x178 / 0x17c / 0x180).
- Gear 0 takes 6 idles a floor. Gear 1 takes 2 idles a floor. Gear 2 adds 72 units an idle, so it moves 2 floors an idle (`pos / 0x24` in FUN_1000c590). Shift thresholds are `T(n) = sum over i<n of (speed[i] + 5) / 6` (FUN_1000cad0), giving T(2) = 2 and T(3) = 4 floors.
- OpenSky (`Car::yootDrive` in `source/Item/Elevator/Car.cpp`, reached from `Car::advance` when `prototype->yoot` is set) implements exactly this: `yootOffset += max(1, speed[gear]) * 6`, `floors = min(left, yootOffset / 36)`, and the same threshold function for gear shifts (commit 524d97b). The 3-floor stride exists only in the non-YootTower `CalcMoveSpeed` express turbo (`stride = 3`), which runs only when `prototype->yoot == nullptr`. YootTower prototypes never reach it.
- So at top gear OpenSky steps 2 floors per mover idle, as the decomp does. No constant was changed.
- Not measured against the original. `BldExport` exports only the Simple Elevator (`elevPlugin = StdE`, `source/BldExport.cpp:207`, `:728`), so a tower with a Large Elevator cannot be loaded into the original. The oracle comparison needs a BigElev mover export (EgiB equipType, 8 cars of 40, its own mover data) first. That export is not done here.

**2. Per-car rider counts: the 38 / 53 / 39 figure was a mislabel.**

- Source: the `yoot_people` run `s7` (`~/yoottower-decomp/parity-agent/runs/s7/yoot_people/report.md`). Its "Mean people at the movers" line reads "waiting orig / OpenSky, riding orig / OpenSky", but `compare.py` `mover_load` returns `(orig waiting, orig riding, OpenSky waiting, OpenSky riding)` and the print put the slots in the wrong order. The 38 / 53 / 39 values were OpenSky **waiting** means. The original's riders were 13 / 16 / 12. Fixed in `tools/yoottower/remote/parity/compare.py` (report print only; the numbers are unchanged).
- Fixture: `tests/sim/yoot_people.tower` (Tokyo grade 1, 2 Simple Elevator cars, offices and shops). The original's snapshots from `s7/yoot_people/oracle` were copied to `hkA/yoot_people` and OpenSky was rerun with `pipeline.py --skip-oracle --exe-tree /lab/src/hkA` (current branch code, 3 days). Output: `/lab/out/parity/hkA/yoot_people`.

| Day | Original waiting | OpenSky waiting | Original riding | OpenSky riding | Riders per car (orig / OpenSky) |
|---|---|---|---|---|---|
| 0 | 9 | 12 | 13 | 14 | 6.5 / 7.0 |
| 1 | 34 | 16 | 16 | 14 | 8.0 / 7.0 |
| 2 (holiday) | 34 | 20 | 12 | 19 | 6.0 / 9.5 |

- Days 0 and 1 are within about 12 percent. Day 2 is 58 percent high on OpenSky (19 against 12 total riders, 9.5 against 6.0 per car). Each mean uses about 16 samples, so the riding noise is a few riders.
- Waiting diverges more (34 against 16 on day 1; 34 against 20 on day 2). That looks like queue size rather than car loading.
- The day 2 gap is above the 15 percent threshold. I did not find its cause in this pass and did not change the dispatch or boarding code. Candidates: the holiday population and appointed-queue sweeps (StdElev FUN_1000cf90 / cda0 / d070). A fix needs a per-trip boarding timeline from the original, which the snapshots do not record.

**Build and goldens:** no goldens touched. The worktree lacked `thirdparty/astar-search` (an uninitialised submodule); copying it from `/lab/src/glmp2` made `hkA` build. `lab.sh build` exits 0 even when `OpenSkyscraper` fails on the missing `stlastar.h`, so check the log for `fatal error`. `lab.sh test hkA`: 9/9 pass.

**Open:** Large Elevator oracle measurement (needs BigElev export); per-car rider gap on day 2 of `yoot_people`.

#### 7.6.B Hotels

**Status:** the maid-reach divergence behind the room-night gap is found, fixed and matches the
original's cleaning pattern. The headline room-night count is **not** closed (see the residual
below). Stays now survive save and reload. Lab: `hkB`, TAG prefix `hkB-`. Analysis scripts live in
`~/yoottower-decomp/closeout/hkB/` (roomtl.py, checkins.py, cleans.py, elig.py, osrooms.py,
checkins_os.py, maidpl.py).

**Setup.** The original output `gp2-hotel-hawaii` was copied to `hkB-hotel-hawaii` (not modified) and
OpenSky was rerun with `pipeline.py --skip-oracle` (3 days; extra seeds 2-4 in `hkB-seed{2,3,4}`).

**1. What the original does, per room (oracle snaps).** Check-ins are booked on the evening of day N
and paid at 12:00 on day N+1 in both games. Original check-ins are 9 on day 0 (8 Twin, 1 Suite; paid
$45,000 on day 1) and 11 on day 1 (10 Twin, 1 Suite; paid $54,000 on day 2). A room cycles 30 (clean)
→ 31 (called) → 32 (in) → 33 (asleep) → 34 (up) → 35 (checked out, dirty) → 36/37 → 30 (cleaned by a
Housekeeping maid). Only the y=5 rooms, which share the Housekeeping's floor, are ever cleaned: 4 on
day 1 and 5 on day 2. The 10 y=6 rooms that took guests stay at 35 for the rest of the run, so each is
used at most once. Mechanism: the maid's route check is RMaintenance FUN_1000c010, which calls
`T2RouteNavigator::CheckRoute(..., 1)` (routing table 1). `T2RoutingTable` 0x10055350 gives table k the
mover mask `0x1000000 << k`, so table 1 is `0x2000000`, the service movers. Only the service movers carry
that ATTR bit (ServElev 0x2008902, KServ 0x2008902, Stair 0x3002902). The Simple and Standard elevators
(StdElev / KElev 0x1008902) do not. The Hawaii fixture has one `elevator-standard` and no service elevator
or stairs, so maids cannot reach floor 6. Same-floor rooms need only horizontal distance within the
Maintenance customer score (PLDF EPlace 1 = 300, not binding here).

**2. What OpenSky did.** `Hotel::yootSendHousekeeper` and the candidate check in
`SpecialSystem::housekeepingTick` tried the passenger route first. Maids rode the Simple Elevator, so
every dirty room was cleaned: 6 y=6 rooms on day 1, 4 on day 2. Rooms were reusable the next evening,
so OpenSky booked 14 (12 Twin, 2 Suite) on day 1 and 7 on day 2, paid $49,500 and $72,000.

**3. Fix.** Maids route with the service flag only (`Hotel::yootSendHousekeeper`,
`SpecialSystem::housekeepingTick`; YootTower only, the housekeeping item is a YootFacility). Result on
`hkB-hotel-hawaii`:

| | Original | OpenSky before | OpenSky after (seed 1; seeds 2 / 3 / 4) |
|---|---|---|---|
| Check-ins day 0 | 9 | 10 | 10 (11 / 13 / 11) |
| Check-ins day 1 | 11 | 14 | 7 (8 / 5 / 7) |
| y=6 rooms cleaned d1 / d2 | 0 / 0 | 6 / 4 | 0 / 0 (seed 1) |
| y=5 rooms cleaned d1 / d2 | 4 / 5 | 4 / 2 | 4 / 3 (seed 1) |
| Hotel income paid day 1 | $45,000 | $49,500 | $49,500 (seed 1) |
| Hotel income paid day 2 | $54,000 | $72,000 | $36,000 (seed 1; $36,000 / $22,500 / $31,500 for seeds 2-4) |

The cleaning pattern now matches the original exactly (no y=6 room is ever cleaned). The room-call
counts do not. After the fix, OpenSky under-fills day 1 (5-8 check-ins against 11) and day 2 income
falls below the original. Before the fix it was above. The fix is kept because it reproduces the
original's maid reach; the headline gap is therefore not closed by it.

**Residual (unresolved).** The count is set at the call stage, not by housekeeping. A clean room is
called with probability 1/30 per idle tick while its estimate is above 200 (`YootHotelGuests.cpp`,
`roomIdle`). The estimate gate separates the games: the original's room estimates on day 1 are 248 to
276 for several rooms and 150 for `hotel_double@14,5`, which is therefore ineligible in the original and
eligible in OpenSky (287). Day 0 has no OpenSky estimate yet (`yootEstimate` 0 is treated as
unevaluated). The next step is tenant-estimate parity (J2) and a multi-seed oracle comparison.
The original's day-0 fill is 9 of 17 rooms that were eligible at some sample in 17:00-22:00 (sampling
is coarse, about 40 minutes). The single-run counts are noisy, so this is not a fully explained
room-night difference.

**4. Stays across save and reload.** A room whose guests are in (status 32..34) saves
`yootStayStatus` and `yootStayParty` (a `PeopleType` per member: demand, time zone, silhouette, econo,
life, transport). Written in the YootTower branch of `Hotel::encodeXML` only. `Hotel::decodeXML` reads
them into `pendingStayStatus` / `pendingStayParty`. `PeopleSystem::roomIdle` consumes them on the first
idle tick (`PeopleSystem::restoreStay`): the party is spawned inside the occupied room and the stay
runs on to the normal checkout (11:00 PushOutAll, then dirty). Statuses 31 (queued at the desk) and 35
(checked out, dirty) are not restored. Old saves, with no attributes, keep the old check-out on reload.
Party members are spawned again, so the pool save (which already excludes visitors) is not duplicated.
Codec and gating: `Yoot::encodeStayParty`, `decodeStayParty`, `decodeStay` (YootPool.cpp).

**Test.** `testYootHotelStaySaveReload` (tests/test_main.cpp) round-trips the room element through
tinyxml2: a status-33 stay with two members restores both, status 31/35/0 and an empty party are
refused, and an old save checks out. `test_main.cpp` has no Game-level harness, so `restoreStay` is
covered by build only, not by a reload test. This is the main limit of the save check.

**Goldens.** `tests/sim/sim_visitors_yoottower.json` was regenerated (`UPDATE_GOLDEN=ON`, TICKS 2000).
Its fixture has hotel rooms on floors 12 and 14 with one `elevator-standard`, so maids no longer ride
to the upper rooms. The other goldens are unchanged.

**Tests.** Lab `test hkB`: 9/9. The `thirdparty/astar-search` submodule is not checked out in this
worktree; it was copied into the lab tree `hkB` only (`lab.sh sync` does not carry it).

**5. Call stage (second pass, on the merged code).** The per-room table is in
`~/yoottower-decomp/closeout/hkB/perroom.py` (original snapshots vs OpenSky series). Hawaii, day 1, seed
1, clean rooms with estimate above 200: the original fills 9 of about 11; OpenSky fills 8 of about 15. Day
1 estimates are 248-276 for several original rooms and 280-300 in OpenSky. `hotel_double@14,5` and
`hotel_suite@-10,5` are 150 in the original on day 1 (and day 2 for 14,5; -10,5 is 150 on day 2 in both)
and 284-288 in OpenSky on day 1. So OpenSky has one or two extra eligible rooms, and it fills a smaller
share of its eligible rooms.

Guest-call path, traced in T2DLL (same path as belong people): room idle, FUN_10012d50 (Singleroom
plugin, desk link and floor status 30..9999 checks) -> `T2Tenant::CollectEmployee` 0x1005f5a0 ->
`T2TenantMemberTableDef::IsCollectFromPool` 0x10063430 (deterministic: a PLDF percent or DPDF holiday
rate of 0 refuses) -> `CollectEmployeeFromPool` 0x10060a50 -> `T2Pool::Find` 0x10050500 (relaxes econo
from member 1 on, onlyFirst) -> `T2Pool::Call` 0x10050580 (one econo for every member; the last argument
is the special flag, not onlyFirst) -> `T2PeopleDemandList::Call`. The econo adjustment is `econo - 3 +
priceIndex`, clamped to 1..5 (same as `callGuests`).

**Fix (committed).** `callGuests` passed -1 to `Call` for members after the first. It now passes the
room's econo to every member, as `callBelong` does. Five seeds, Hawaii, OpenSky only (oracle copy):

| | Original | seed 1 | seed 2 | seed 3 | seed 4 | seed 5 |
|---|---|---|---|---|---|---|
| Funds change day 1 | 37,100 | 40,400 | 47,800 | 36,800 | 37,300 | 27,100 |
| Funds change day 2 | 53,300 | 51,300 | 53,500 | 47,600 | 44,900 | 70,800 |
| Check-ins day 0 | 9 | 9 | 12 | 9 | 9 | 7 |
| Check-ins day 1 | 11 | 8 | 10 | 8 | 8 | 12 |

`funds_delta` is 4/4 within tolerance on every seed (before this fix, seed 1 was 26,800 / 27,600 and
day 2 was out of tolerance). The regenerated golden is `tests/sim/sim_visitors_yoottower.json`.

**Not verified (open).** Two OpenSky gates have no counterpart I could find in the decompile I traced:
the 1-in-30 call roll per idle tick and the estimate above 200 gate. `FUN_10012d50` has no estimate
test and no random roll, and the Singleroom room's status function (vtable slot 0x38, ChangeStatusOnIdle)
was not recoverable from the dumps (the vtable scan of `Singleroom.dll` found no pointers). These are the
prime suspects for the remaining fill gap (about 53% of eligible rooms filled in OpenSky against about 80%
in the original on day 1). The room estimate (J2) also differs: the original's day-1 values are 150 for
14,5, while OpenSky's are 284-288. I did not change either gate, since no evidence was found for a
replacement. Check-in counts are noisy across seeds (day 1: 8 to 12) and the original is a single run.
#### 7.6.G Hawaii night population

Tower `hawaii_g3_boatdeck`, 3 game days, `pipeline.py --skip-oracle` on the original oracle
copied from `/lab/out/parity/gp2-hotel-hawaii` (never modified). Code: `feature/yoottower-parity`
(`fea6248`, which includes workstream B's maid and hotel changes) plus this branch's change.
"Before" is the same tip without the change (lab tree `hkGb`); "after" is with it (`hkG`).
Night means the 00:00-06:00 samples; OpenSky dumps every 30 game minutes.

**Change (yoottower only).**
1. `YootPool::callBelong` (`source/YootPool.cpp`) passes the same econo to every member's call.
   `T2Pool::Find` (0x10050500) relaxes econo from member 1 on, but `CollectEmployeeFromPool`
   (0x10060a50) calls `T2Pool::Call` with one econo for all members. OpenSky relaxed it in the
   calls too, so every call succeeded. This is the reason for 60 residents against 54.
2. `YootCondo::advance` / `createOccupants` (`source/Item/YootCondo.cpp`) hires residents
   through `PeopleSystem::hireBelong` at the top of the hour, 07:00-20:59, as the Condo.dll
   `ChangeStatusOnIdle` CollectEmployee branch does (`rules-agent/out/plg2/Condo.dll.decomp.c`,
   about line 27330). The occupant count comes from the hired list. Move-out calls
   `releaseBelong`. Keeps `tenantIdleFrozen` from workstream B.

**Combined before / after (original in the first column):**

| Metric | Original | Before | After |
|---|---|---|---|
| Residents | 54 | 60 | **55** |
| Night pop d1 / d2 / d3 | 113 / 119 / 103 | 124 / 113 / 116 | **112 / 113 / 120** |
| Night unhomed visitors (hotel guests, inferred) d1 / d2 / d3 | 16 / 22 / 6 | 21 / 10 / 13 | 14 / 15 / 22 |
| Daily pop peak d0 / d1 / d2 / d3 | 580 / 648 / 910 / 103 | 594 / 798 / 983 / 116 | **560 / 685 / 1,009 / 120** |
| Funds change d0 / d1 / d2 (original 2,990,800 / 37,100 / 53,300) | | 2,993,300 / 47,200 / 24,100 | 2,992,600 / 26,800 / 27,600 |

All daily peaks are within the harness tolerance (`pop_max`, 20%). The combined baseline
already has day 1 at 798 against 648; the change moves day 1 down to 685 and day 2 up by 26
(983 to 1,009, original 910). Funds day 2 is outside tolerance in both runs (24,100 and
27,600 against 53,300). That gap comes from B's hotel under-booking, not this change, which
moves it slightly toward the original. Day 0 and day 1 funds are within tolerance in both.

**Where the daytime people come from (day 2, 13:30 peak).** `T2Pool::RecoverPeople` counts
the original's residents and staff in the pool counter (its population is 910 including 98
belong people). OpenSky's `callBelong` does not increment the pool counter, and the tower
population adds the belong people on top. So compare visitors only: the original has
910 - 98 = 812 pool-counted visitors, while OpenSky has 880 before and 911 after. Deferred departures (shrink) are 788 in the original, 845 before, and 857 after.

- *Release on move-out.* Not active in these runs: there are no condo sale-outs or
  "Repurchased" events in three days. It cannot explain the peak.
- *Trigger timing alone* (variant with the condo change but the original econo): day 2 pool
  866, peaks 710 / 969. The trigger lowers the daytime count slightly.
- *Econo fix alone* (variant with the fix but the original condo): reproduces the before
  numbers exactly (983). The econo fix is therefore the source of the extra daytime visitors
  (about +40 on day 2 and -25 on day 1 against the trigger-only variant). The mechanism is
  likely that the econo-matched residents consume a different subset of the callable window
  types, so visitors with other econo types stay callable. This is not proven.

The change is kept: it fixes residents and night population on day 1, and the peaks stay
within tolerance. The residual daytime excess (about 100 pool-called visitors on day 2
against the original's 812) is not caused by the release or the hire timing, and it is
present in B's baseline too.

**Hotel guests (re-measured after the B merge).** The night unhomed-visitor count is now
14 / 15 / 22 against the original's 16 / 22 / 6, versus 21 / 30 / 16 before B's merge. B's
maid routing reduced guest bookings, so the night surplus of guests is gone. The guest
counts remain the hotel workstream's to decide.

**Not changed.** `callGuests` (`source/YootHotelGuests.cpp:97`) has the same econo relaxation
(`onlyFirst && i > 0 ? -1 : econo`) in its pool calls. B found that OpenSky now under-books
rooms, so tightening it needs measurement first. Left alone, as asked.

**Open.** One resident (the original has 54, OpenSky 55): the members' silhouette and option
checks (`T2PeopleType::Check`) are not yet compared with the pool types. The day-2 funds gap
(see above) and the daytime visitor excess come from the hotel and window-consumption side.

**Tests.** `lab.sh test hkG` 9/9 (100%). No goldens changed.

**Lab outputs.** `hkG-cmb-before/` (combined baseline), `hkG-cmb-after/` (combined fix),
`hkG-cmb-c/` (trigger only), `hkG-cmb-d/` (econo only). Older runs: `hkG-hawaii/` (pre-B
baseline), `hkG-hawaii-econo/` (pre-rebase fix).


#### 7.6.C Event Center

**Status: not closed.** The export now writes the booked hall event, which is evidenced by
the decompile, but the original still never opens the hall in the fixture, so the evening
events remain unmeasured. No OpenSky change was made.

**Decompile (EventCenter.dll).** `FUN_1000bec0` (the hall's status machine, called from
`FUN_1000bd90` on every idle) moves the hall to status 30 only while the tenant's sub-plugin
(`T2HaveOutViewObject::GetSubPlugin`, set by `T2Tenant::SetHallEventPlugin`) is non-null:
- from open - 60 (`CallBelongPeople`, status 30);
- at open (status 30 while open, `UpdateCommercialEstimate` each hour);
- at close, `[close, close + 8)`: `DoPayTool(out - cost)`, which subtracts from funds, so
  funds += cost - outMoney (matches `hallEventSettlement`), then status 10.
Without a sub-plugin the status is 10 at every time of day, which is the fixture's state.
Open and close come from the sub-plugin (+0xb8 / +0xbc) when booked, and from the tenant def
(+0x174 / +0x178, `T2Tenant::GetOpenTime`) otherwise. OpenSky's `eventCenterTick` already
uses those rules.

**Save record.** `LoadExtraData` `FUN_1000e1f0` (save layout in `FUN_1000e4f0`) reads a u32
booked id first, resolves it with `FUN_1000e680` against the tenant def's sub-plugin list
(+0x10c, excluding tie-ups whose `IsTieupFinish` is set), and calls `SetHallEventPlugin`.
The record is 64 + 9k bytes. A game-written snapshot (`c1/tokyo_g3_venues/oracle/snaps/snap_010`)
has the record as 118 bytes (k = 6): a zero booked id, then six entries (u32 id, s16, s16,
s8) for Art Exhibition (`strA` = 0x41727473), Car Show, Rock Live, Fashon Show, Plant Show and
Tennis Match, then the 12 month slots. PlArt's id, read from its ATTR, is the same 0x41727473.

**Export change (`BldExport.cpp`, `eventCenterRecord`).** An Event Center with an `event` attribute
gets the record above with its booked id (k = 0, zero slots). Before this change the exporter
wrote n = 0 for every Event Center.

**Plugin table and gating.** The exported save's plugin table already lists the six `lPeH`
hall events (`strA` among them, `fileName C:\YOOT\T2.exe`), the same as the game's samples.
PlArt.t2p's ATTR: mAttrib 0x1 (Tokyo), mGameLevel 1, mSecretItem 0, mDependID `****`, so the
level and world gates pass at grade 3. `IsTieupFinish` only applies to tie-ups (+0xb0 set).
Neither the table nor the ATTR gate explains the null lookup.

**Measured.** Lab tree `hkC`, `tokyo_g3_venues` (`parity.sh run hkC-ec1 3`), oracle with the export
change. The original's hourly series has the Event Center at status 10 in every sample, with no
audience and no status 30 at any time over 3 days. Funds and population peaks are unchanged
from §7.4 item 5 (original 710 / 765 / 1049; OpenSky 770 / 921 / 1050). The day-end status
column reads 10 by construction, since the hall closes before the day ends.

**Ground truth.** No game-written save in the corpus has a booked Event Center: of 1,253
Event Center records with extra data, only the exported input has a non-zero booked id. A
booking needs the GUI (the hall dialog's event list, then save), which the oracle driver does not
do yet (it only closes dialogs and saves).

**Open.** The record is loaded, but the original does not resolve the booked id to a sub-plugin
(`FUN_1000e680` against the def's list at +0x10c). Next steps: (1) GUI-book PlArt on an exported
tower, save, and diff the record bytes against the export; (2) if the booked id stays zero, trace
how the def's sub-plugin list (+0x10c) is built at plugin load and why PlArt is absent from it;
(3) only then compare the evening window, audience and income against `eventCenterTick`.

**Tests.** Lab `test hkC` after the rebase onto `fea6248`: 9/9. The `thirdparty/astar-search`
submodule was copied into the lab tree `hkC` only.

**Large Elevator export (follow-up).** BldExport wrote only StdElev movers. `elevator-express` (EgiB, Large Elevator) now exports as a BigElev mover: equipType 1812, 8 car slots (`LMlE` f20 = 8, `maxModule` 8), and the 2 x 10 three-s16 extra table (n = 120; bld-format §4.3). The plugin record comes from the catalog only when the tower has a Large Elevator. The vEOB demand counts each mover's own plugin. No game-written save in the corpus places a Large Elevator (only plugin-table entries), so the layout is from the decompile; the BigElev extra table's fresh values (0, 0, 6) are assumed from KElev and are unverified (the reader accepts any n). Unit test: `testBldShippedSaves` export block "Large Elevator export" (EgiB record, 8 cars, n = 120, round trip).

- **Load failure found and fixed.** With a 4-wide shaft the original fails at load for any car count: `E:\T2\T2DLL\T2BitImage.cpp (190)`, `Src(826,862), Dest(0,36)`. The Express item is 6 units wide, so the exporter now uses a 6-wide shaft for Large Elevators. Then the tower loads (`hkA-vE6`, 8 cars, 10 floors, no dialog).
- **Fixture:** `tests/parity/tokyo_g5_large_elevator.tower` (`towers.py`, `tokyo_g5_large_elevator`): Tokyo, 4 stars, one 34-floor Large Elevator (8 cars) on the lobby, slabs and four offices on floors 3, 9, 15, 21, 27, 33. Run `hkA-lift2` (1 day, oracle + OpenSky).
- **Result: no rides in either game.** All 144 people stay in pool status 1 in the original's snapshots (day 0, 08:00-20:00), and OpenSky reports zero waiting and zero riding. Population peaks match (144 / 144). Car travel time between distant floors cannot be measured on this tower because nobody boards. I did not find the cause (the exporter's request/stop side or the lobby route to the Large Elevator); the same tower shape works with a Simple Elevator in `yoot_people`, so the difference is the Large Elevator's request set or its route from the lobby.
- `oracle_run.py --step MIN` (new) sets the snapshot cadence. A 5-minute fine run was started (`hkA-fine`) but ended with a failed click on the harness after the first snapshots, so it has no usable data and was stopped.
- Not done: car travel-time comparison; per-car rider comparison for the Large Elevator. Both need riders first.

**Open:** why the Large Elevator gets no riders in the original or in OpenSky on `tokyo_g5_large_elevator` (route or request set); per-car rider gap on day 2 of `yoot_people` (skipped per the lead).

**C2 (hkC2-; booking survives the first midnight, original opens the hall).** The 7.6.C
"not closed" state is resolved on the original side. The exported booking reaches the
original, but it is dropped at the first midnight, not on load. Evidence: (1) the save
record's booked id is read by `FUN_1000e680`, and game-written snap_002 of run `hkC2-ec3`
still has booked id 0 on day 0 while all 12 month slots already hold PlArt (0x41727473).
So the id lookup against the tenant def's list (+0x10c) failed at load, and the month slots
(resolved against the tower plugin list via `FUN_1000cd60`) did load. (2) `FUN_1000dcb0`
runs in the first 8 minutes of each day (`WithinMinutes(now, 0, 8)` in `FUN_1000bec0`) and
sets the booked plugin (this+0x18) from the month slot of the coming month. With empty
slots (our old export) the booking was cleared at 00:00, which is why the hall stayed at
status 10 for three days. Snap_026 (day 1 00:06) shows booked id 0x41727473 and all 12
slots set, so the midnight rollover now keeps the booking. (3) PlArt's HEDF fields (read
via `decodeHallEventDef`, `SubPluginDefs.cpp:191`): bookingDays 3, openTime 600 (10:00),
closeTime 1020 (17:00), monthMask 0x0f1f, cost 250. The original's hall went to status 30
at minute 564 on day 1 (open - 60 = 540), then 31 with customers and an estimate of 300.
Visitors peaked at 143 by day 2 close. **Fix (committed f0f241a):** `BldExport.cpp`
`eventCenterRecord` writes the booked id in all 12 month slots, not zeros.

**Measured (run `hkC2-ec3`, `parity.sh run hkC2-ec3 3 tests/parity/tokyo_g3_venues.tower`,
oracle plus OpenSky, both finished).** Original Event Center: status 30 on day 1 from 564 to
about 1000 min, 31 with customers 3 to 63, estimate 300; funds 7,989,600 at minute 564 and
8,031,600 at minute 1034 on day 1 (the settlement and the other tenants included). The
hall's own expenses (-10000 at 18:01) show each day in both runs. OpenSky: the hall stays at status 10 with zero
customers on all three days (its `eventCenterTick` needs `e->today`, which comes from
`VenueSystem.cpp:207` `runsOn(day)` and `hallCooldown`; the hall never gets a `today` event
on this import). So OpenSky does not open the hall for this fixture, even though the
original does with the corrected export. Not fixed in this pass: the open question is the
OpenSky import of the booked event (`e->event`, `startDay`, `span`) against the original's
bookingDays / monthMask rule. Income and audience parity for the Event Center therefore
remain unmeasured against OpenSky. Goldens: none touched. Oracle instances stopped
(`parity.sh status hkC2-ec3`: done).

**Large Elevator stops and riders (follow-up 2).**

- **The extra table is not the stop list.** BigElev's `LoadExtraData` (the same shape as StdElev's FUN_1000f4e0) reads `s32 n` and builds a 0xf4-byte object kept as the mover's RefCon (`T2HaveOutViewObject::SetRefCon`). Its reader FUN_10011280 fills 2 x 10 records of 12 bytes, each read by FUN_100110c0 as two s16 values (6 bytes for BigElev's three-s16 variant). That is a 2 x 10 schedule table, not stops. Stops are the mover's request-ID array (`ADIR`, two IDs per floor, one per side): `T2Mover::IsStopPosition` (0x1003ebc0) is `T2RequestIDArray::IsStopPosition`, a nonzero ID on either side of the floor. The cap is `T2MoverDef::GetMaxStop` (+0x170 of the def, 8 for BigElev). The player chooses stop floors by toggling request IDs on the shaft; that toggle path in the original was not decompiled here, so the "refuse past 8" rule is taken from OpenSky's `Elevator.cpp` (drag/resize cap). The exporter already writes these request IDs for every served floor, so the lead's "no stops written" hypothesis does not hold for this export.
- **Evidence in the original.** The oracle snapshot of `hkA-lift5` (after the first walk-in) has the exported mover with request IDs on floors 0, 3, 9, 15, 21, 27 and 33 (both sides): 7 stops, under the cap of 8. Riding and waiting then work in the original.
- **Why nobody boarded before.** The fixture had no visitor demand above the lobby. Lobby shops need no mover, and both games left all 144 people in pool status 1 with the Large Elevator, and also with a Simple Elevator in the same layout (controls `c2S_std_gap`, `c2L_large_contig`, `hkA-ctl`). Adding one shop each on floors 9, 21 and 33 gives riders in both games.
- **Fixture now** (`tokyo_g5_large_elevator`): lobby shops, one shop on floors 9, 21 and 33, offices on 3, 9, 15, 21, 27, 33. Seven stops (lobby plus six office floors), under the cap of 8. Run `hkA-lift5`, 1 day.
- **Comparison (`hkA-lift5`, 08:00-20:00 means):** riding 9 (original) / 9 (OpenSky); waiting 1 / 4. Funds 2/2, population peak 2/2, customers 5/6, estimate 51/60 within tolerance.
- **Stops, OpenSky:** `out.tower` serves exactly floors 0, 3, 9, 15, 21, 27, 33 (27 unserviced). This matches the original's request IDs, so no OpenSky change was needed.
- **Car movement (original, 3-minute snapshots, `hkA-fine2`, 71 samples, stopped at 07:28 by a harness dialog):** car 0 goes 6 -> 9 -> 21 at 436, 442 and 448 min. That is about 0.5 floor per game minute, then 2 floors per game minute (gear 0, then gear 1 of the decompile, 2 idles a floor at 4 idles per minute). No car reached gear 2 in the window. OpenSky's car 1 moves at 460 min, car 0 at 490 min, in 30-minute dumps. Those samples are too coarse for a travel-time test, so travel times are **not compared**. The gear model itself (`Car::yootDrive`) was not changed.
- Not done: a full-day fine-cadence original run (the harness stopped at 448 min), so a complete trip comparison is still missing.

**C3 (hkC3-; the .bld import reads the Event Center booking, OpenSky opens the hall).** The
importer ignored the EventCenter record, so OpenSky never had an event on an imported
.bld. Confirmed by reading `BldLoader.cpp` (the tenant loop wrote no event for
`eventcenter`) and `BldImport.cpp` (no record read). **Import (b8f2b1c):** the coming
month's slot (`(month0 + 1) % 12` of the 12 u32 slots, the one FUN_1000dcb0 restores at
00:00) gives the hall-event plugin through `hallEventLookup` (the exporter's id mapping in
reverse, plus the HEDF bookingDays). The import writes `event`, `startDay = loadDay + 1`,
`span = bookingDays - 1`. **Decision on the day-0 loss: mirrored.** FUN_1000e680 does not
resolve the booked id at load, so the original has no event on the load day, and
FUN_1000dcb0 restores it from the slot at the next 00:00. The original's observed
run (days 1 and 2 open, day 0 closed, with PlArt's bookingDays of 3) fits "the booking
starts on the load day and loses that day", so OpenSky closes the load day and runs
bookingDays - 1 days after it. Day 3 is not observed (the series ends at minute 120), so
the end of the run is an inference. Unit test: `testBldShippedSaves`, block "Event Center
booking" (export of a PlArt booking on the tokyo floor slab, 12 slots filled, import gives
`event="PlArt.t2p" startDay=loadDay+1 span="2"`; no lookup gives no event). 9/9 on `hkC3`.
**Rerun (hkC3-ec, OpenSky only, against the hkC2-ec3 oracle):** the hall now opens on days 1
and 2 and stays closed on day 0, as the original does. Day 1: status 30 from 540 (the samples
are 520 at 10 and 550 at 30; the original is 517 at 10 and 564 at 30). Close at 1020 (17:01 message)
with +15,000 both days, matching the original's +15,000 settlement on days 1 and 2. Day 0's
idle hall takes -9,600 of funds at 18:00 in both games. Customers: original 70 (day 1) and 143
(day 2); OpenSky 78 and 157 (+11% and +10%). First customers: original 3 at 611 with status
30; OpenSky 6 at 580. Status 31 is never shown by OpenSky (30 throughout) and the original
shows 31 from about 658 (day 1) and 574 (day 2); the status 30/31 switch is set outside
EventCenter.dll (FUN_1000dbb0 treats 30, 31 and 32 alike and collects in each), so it is not
modelled. Estimate: the original's hall reads 300 from day 1 and OpenSky leaves yootEstimate
at 0, because `eventCenterTick` never sets it (the decompile has `T2Tenant::UpdateCommercialEstimate`
hourly while open and at close; its formula is not in EventCenter.dll and was not traced).
**eventCenterTick change (57fe556):** the per-idle collect amount is now
min(cap, target) - inside - walking (FUN_1000dc70 calls CollectCustomer with target - walking -
inside). It changed nothing on this fixture (78 and 157 both before and after), so the
customer overshoot comes from the target value itself: OpenSky's target is 2 x good + medium
(or the def's unkDC on day 1), which is at least 78 here, while the original stops at 70.
Not resolved: where the original's target (`uVar1 = *(sub-plugin + 4)` in FUN_1000dbb0's
10 -> 30 branch) differs from OpenSky's `hallValue` / `unkDC`. Goldens: none touched.
Runs: `hkC3-ec` (import fix, before the collect change) and `hkC3-ec2` (after). Both
OpenSky-only (`--skip-oracle`) on the hkC2-ec3 oracle; no oracle instances were started.

#### 7.6.H WP8 close-out of the remaining measurable gaps

Lab tree `wp8`, runs `wp8-*` under `/lab/out/parity/` (OpenSky only, on copies of the existing
oracle outputs `gp2-hotel-hawaii`, `hkC2-ec3`, `gp2-blackout4`; the originals are untouched).

**Hotels: the gap was a regression, not the call path.** The 1-in-30 roll and the estimate gate
that 7.6.B could not find in `FUN_10012d50` are in the room's status function itself.
Singleroom.t2p `0x100136b0`, case status 30 (disassembled with objdump, Plugins/Singleroom.t2p):
the first minutes of the day add +5 (+20 from estimate 70) to the people estimate
(`IncPeopleEstimate`), the first 8 minutes of each hour run `UpdateResidencialEstimate`, then
`estimate > 200` (`cmp [esi+0x90],0xc8`, 0x10013777), `WithinMinutes(open, open + 0x12c)`
(0x10013794) and `rand() % 30 == 0` (0x100137a3..af) gate the call (`thunk 0x1000129e` =
`FUN_10012d50`). `roomIdle` already has exactly these gates, so the call path matches.
With an idle every 8 minutes a room has 37 or 38 rolls in 17:00-22:00, so an eligible clean
room is called with 71.5% to 72.4% (`Yoot::hotelCallChance`). Measured on
`hawaii_g3_boatdeck` (6 seeds, 3 days) the share of eligible rooms OpenSky calls is 57% on
day 0 (original 9 of 18 = 50%), 77% on day 1 (56 of 73; original 9 of 11) and 73% on day 2
(40 of 55). The "half of the eligible rooms" figure came from the number of rooms that were
eligible, not from the roll:
- `git show b60fa72:source/Item/Hotel.cpp` still has the maid's arrival and cleaning step
  (`Housekeeper` reaches the room, 30 minutes, `roomState = kClean`). The merge `ca55195`
  dropped it from `Hotel::addPerson` / `Hotel::advance` and kept only `yootSendHousekeeper`. A
  dispatched maid walked to the room and did nothing, `SpecialSystem::housekeepingTick` never
  saw the trip finish (`maidsOut` stuck at 1-2 of 6) and no dirty room was ever cleaned.
  Debug counters on 6 seeds: eligible clean rooms at 16:40 on day 1 / day 2 were 7.7 / 1.7 on
  average (rooms checked out dirty at 11:00 stayed dirty for the rest of the run); the original
  cleans its same-floor rooms every day (4 on day 1, 5 on day 2).
- **Fix:** `Hotel::addPerson` starts the cleaning for a `Housekeeper`; `Hotel::advance` makes
  the bed after 0.5 h (`kClean`), removes and deletes the maid and clears `housekeeper` so the
  next stay can be cleaned too (YootTower items only, `prototype->yoot`).

| 6 seeds, 3 days | Original | Before | After |
|---|---|---|---|
| eligible clean rooms at 16:40, day 1 / day 2 (mean) | about 11 / 6 | 7.7 / 1.7 | 12.2 / 9.2 |
| rooms called, day 1 / day 2 (mean) | 9 / 2-3 | 6.0 / 1.3 | 9.3 / 6.7 |
| funds change day 1 (original 37,100) | | 41.5k-55.6k, mean 47.7k | 40.5k-55.4k, mean 47.7k |
| funds change day 2 (original 53,300) | | 23.7k-49.8k, mean 33.7k, 3 of 6 outside tolerance | 39.3k-60.4k, mean 49.2k, 6 of 6 inside |
| funds_delta within tolerance | | 4/4 on 3 seeds, 3/4 on 3 seeds | 4/4 on all |

`tests/sim/sim_visitors_yoottower.json` was regenerated (its hotel rooms share a floor with the
Housekeeping, so the maid now cleans them). Test `testYootHotelCallOdds` pins the window,
the 37/38 rolls and the 71.5% / 72.4% chance. Room estimates (280-300 against 248-276, and
`hotel_double@14,5` 287 against 150) are not changed: the original recomputes the estimate
hourly from the guests' current estimates and keeps +0x8c between stays
(`UpdateResidencialEstimate` 0x10060e20, `IncPeopleEstimate` 0x609e0, initial 300 at
`InitTenant` 0x5e700), while OpenSky recomputes once a day in `judgeDaily`; the traffic range
of the entrance (150 per step) also differs for `14,5`. Open.

**Event Center.** The README figures were stale: on the current tree (five seeds,
`tokyo_g3_venues`, 3 days, hkC2-ec3 oracle) the audience is 65 / 71 / 79 / 53 / 84 on day 1
(original 70) and 128 / 139 / 146 / 128 / 152 on day 2 (original 143), identical with and
without this change (baseline tree `wp8base`). The status and the estimate were real gaps and
are fixed from the plugin disassembly (EventCenter.t2p, vtable at 0x10013b18):
- `EnterTenant` hook `0x1000bba0`: with a booking and status != 10 the visitor is counted;
  status 30 becomes 31, and status 31 becomes 32 once `inside >= capacity / 2` (signed halving,
  capacity 80). `LeaveTenant` hook `0x1000bcb0`: inside 0 and status != 10 gives 30; status
  32 with `inside <= capacity / 2` gives 31. So the "full" state is 32, and 31 means "some
  inside", which the oracle shows from the first visitors (hkC2-ec3 day 1: 30 at 611 with 3
  inside, 31 at 658 with 10 inside).
- `FUN_1000bec0` resets the booked hall to status 30 in `[open - 60, open - 52)` and again in
  `[open, open + 8)` even with visitors inside; the oracle reads 30 with 3 inside at 10:11 on day 1 and
  with 7 inside at 10:21 on day 2. `Yoot::hallStatusReset`.
- `UpdateCommercialEstimate` runs on the plugin idle at or after the open minute in the first 8
  minutes of every hour, in `[open, open + 8)` and at close. `CalcProfitEstimate`
  (0x10061410) is `(inMoney x customers / 100) x 70 / out`, with inMoney 200 units and out 1
  unit for the Event Center: 140 per visitor, 300 from three. The oracle reads 0 before 09:00,
  1 with no visitors and 300 after the first three. Fix: `VenueSystem::hallEntered`,
  `PeopleSystem::pushOut`, `eventCenterTick` and the pure `Yoot::hallStatusOnEnter`,
  `hallStatusOnLeave`, `hallStatusReset`, `hallEstimateIdle` (tests in the hall block of the
  yoot section). OpenSky run (seed 1, day 2): 30 -> 31 at 670 with 24 visitors, 32 from 730
  with 51 inside, back to 31 at 850, estimate 300 from 10:10, 1 on an empty day.
- Still open: the first-run target (`*(slot + 4)` of the month slot in `FUN_1000dbb0`) and the
  evening events.

**Population detail (not closed).**
- *Hawaii condo residents, 55 against 54.* Both games hire the 20 condos at 20:00 on day 0 and
  both end with partial tables, because `T2Pool::Find` only checks that each member has one
  match while two of the three Condo members share silhouette 34-50. The original's snapshot
  `gp2-hotel-hawaii` snap_023 (21:47) has 14 condos with 3 residents and 6 with 2 (= 54). Two
  fresh oracle runs of the same tower (`wp8-or1`, `wp8-or2`, 0.97 day) end with the same 14 x 3
  + 6 x 2 = 54 (the 20:59 snapshots differ: 13/4/3 and 14/3/3 condos with 3/2/1 residents, and
  the single-resident condos are filled by 21:46), so the original is deterministic here.
  OpenSky's hires are 15 with 3 and 5 with 2 (= 55). The silhouette and option checks were
  compared with the disassembly (`CheckSilhouetteType` 0x1004e590, `CheckSilhouetteOptionType`
  0x1004e630, `Check` 0x1004e4c0) and match; so does `Find`/`Call` of the type array and the
  season factors (absent in the shipped worlds). The econo adjustment of
  `CollectEmployeeFromPool` (`econo - 3 + priceIndex`) is applied in `callGuests` but not in
  `hireBelong`; applying it left the Hawaii run unchanged to the dollar, so it was not kept.
  The remaining difference is in the pool contents at 20:00 (how many silhouette 8-25 and
  34-50 types of demand 3 are in the zone), not in a rule I could find.
- *Coffee shop before the blackout (`tokyo_g4_blackout`).* The estimate now reaches 7 at
  10:10 as the original's does (10:08). Customers are 10 at 09:40 and 20 at 10:10 against 15 at
  09:20 and 31 at 10:08: OpenSky's first customers arrive about 40 minutes later. Not traced.
