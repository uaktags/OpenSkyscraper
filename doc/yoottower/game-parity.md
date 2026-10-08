# YootTower game-rules parity audit (`--ruleset yoottower`)

**Status:** research snapshot from 2026-10-08. Nothing has been implemented from it yet.
**Scope:** the gameplay rules of the original YootTower (1999 Win32: `T2.exe` + `T2DLL.dll` +
`Plugins/*.t2p`), compared with OpenSky's `--ruleset yoottower` mode.

**Headline:** in OpenSky today, `--ruleset yoottower` plays exactly like `--ruleset opensky`.
The only ruleset branch is the horizontal lot check in `source/BuildRules.cpp:53-66`, and that
check applies to SimTower only. YootTower mode differs from OpenSky mode only in which skins it allows
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
| T1 | **The clock is minute-linear.** `T2DateTime` holds year, "month" (0–11), minute (0–1439) and sub-tick (0–3). There are 4 sub-ticks per game minute, so a day is 5760 linear ticks. | `IsIncDateTime` 0x1001ca90; `CalcTotalCount` 0x10001d60 (`((m+12y)*1440+min)*4+sub`) | Uses SimTower's non-linear 2600-frame day (`Time.cpp:19-33`). | DIVERGENT | H |
| T2 | **Calendar.** Each internal "month" is one game day. A quarter is 3 days and a year is 4 quarters (12 days). Quarter = m/3+1, day-of-quarter = m%3+1, and years start at 1. | `GetQuarter`/`GetDay` 0x10001c30/0x10001c50, `CalcTotalDays` 0x10001cc0 | Uses the same 3/4/12 structure (`Time.cpp:93-95`). | MATCH | H |
| T3 | **Daytime and time zones.** "Day" runs 06:00–18:00 (`IsDay`: 360 ≤ min < 1080). There are 10 people time zones with boundaries at 2, 6, 9, 10, 12, 13, 15, 18 and 22 h, and they drive the PZDF demand tables. | `IsDay` 0x10001cf0, `GetTimeZone` 0x1001c9a0 | Has no time-zone model; spawns are hardcoded per item. | MISSING | H |
| T4 | **Holidays (weekends) are the same rule in all three worlds.** Let `d` = month index 0–11 and `Y` = year. A day is a holiday if `d==7` or `d==11`, or if `n=12Y+d-23` satisfies `n≥2 && (n-2)%5==0` (unsigned arithmetic, so it wraps in year 1). That gives fixed days 8 and 12 of each year plus every 5th day. | Tokyo.t2p 0x10014c10; Hawai.t2p 0x10017770; Kegon.t2p `WorldDef::IsHoliday` 0x10014e70. Dispatch: `T2DateTime::IsHoliday` 0x1001cc50 → world vtable +0x24. | Weekend is `day%3==2` (`Time.cpp`, `LevelUp.cpp:104-105`). | DIVERGENT | H |
| T5 | **Rain is deterministic and per world.** Tokyo rains when `(d+1)%4==0 && d/3 != 3`, which is days 4 and 8 of each year. Hawaii and Kegon never rain: they do not override `IsRainyDay`, and the base returns 0. | Tokyo.t2p 0x10014c50; base `T2WorldDef::IsRainyDay` 0x1000a6a0; override list in `overrides.py` output | Rolls `rand()%3==0` at 05:00 (`Sky.cpp:32`). | DIVERGENT | H |
| T6 | **When the day changes** (midnight, inside the hour-change branch), the game calls the world's `DayChanged`, `T2PeopleArrayList::DayChanged` and the cold-table refresh. On day 1 of a quarter it also calls `T2Settlement::Update` (opening the new quarter's books) and charges quarterly maintenance. | T2.exe FUN_0041dc40 (doc sim step, vtable +0xc4) | Settles the quarter at the 4:59/5:00 boundary (`Game.cpp:878-924`). | PARTIAL | H |

### 1.2 Tower rating (grades / stars)

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| R1 | **The grade table comes from WORLDDEF**, one row per step of `{vipMask, population, anim…}`. Steps (start grade, then star→star):<br>• **Tokyo** starts at grade 1: 1→2 needs **1000**, 2→3 **2000**, 3→4 **5000**, 4→5 **12000**.<br>• **Hawaii** starts at grade 2: 2→3 needs **500**, 3→4 **2000**, 4→5 **8000**; its row 0 is a dummy (pop 1).<br>• **Kegon** starts at grade 3: 3→4 needs **200**, 4→5 **1000**.<br>Start grades come from WORLDDEF +0x70 = 1/2/3 (`world-and-sp-formats.md` §2.2). | `T2WorldDef` ctor 0x1006e4e0; `world_rules.py` output | Uses the SimTower table 300 / 1000 / 5000 / 10000 / 15000 (`LevelUp.cpp:37-44`). | DIVERGENT | H (pop), M (start grade) |
| R2 | **The gate is population plus one VIP flag per grade, and nothing else.** The test is `(doc+0xa0 & row.vipMask)==row.vipMask && pool.population ≥ row.population`, which returns 0x2715. T2.exe then does `grade++`. It checks every idle tick, with no time-of-day window. There is no security, medical, recycling or metro requirement. | `CheckGameLevelProc` 0x10072410 (identical to `T2WorldPlugin::CheckGameLevel` 0x10073290); T2.exe FUN_00421e90 (0x2715 = +1, 0x2716 = −1); caller FUN_0041da20 | Requires Security, Suite, Recycling, Medical, VIP and Metro on a weekday-evening window (`LevelUp.cpp:37-44,104-155`). | DIVERGENT | H |
| R3 | **VIP visits** (VIEV, one VIP per grade).<br>• **Mask:** a VIP's flag is the first free bit of `1<<(4(g-1))`, i.e. 1, 16 or 256.<br>• **Start conditions:** the current grade equals the VIP's grade, its flag is still unset, and population > row.population/2.<br>• **Schedule:** at most once per day; cycleDay 9, begins 10:00.<br>• **Destination:** each VIP needs its destination tenant type built, or the game shows "To invite a VIP, a/an X needs to be built". If that plugin is not installed, the flag is granted outright.<br>• **Success:** the VIP's `CalcCurrEstimate > 200`.<br>• **Destinations:** Tokyo Record store / Electronic store / Suite; Hawaii Burger / Suite / Suite; Kegon Old Time Cafe / Observatory / Kegon Spa. | `T2VisitVIP::Init` 0x1006d360, `Start` 0x1006cd90, `GetDestinationTennant` 0x1006d8c0, `T2People::VIPEstimation` 0x10049990; VIEV data | Has one VIP, at 3★ only, scheduled on `day%9==3`, arriving 17:00, favourable if stress ≤ 150 (`VipSystem.cpp:23-30,127-150`). | PARTIAL (the 9-day cycle matches) | H |
| R4 | **The top grade (4→5) needs the world's final-item event.** There is no grade-4 VIP. Instead the final item calls doc vtable +0x188 (T2.exe FUN_00422670), which ORs the current grade's mask (0x1000) into `doc+0xa0`, but only when grade == numGrades. The triggers are:<br>• **Tokyo:** a baseball game at the **Stadium**. It needs ≥ 200 visitors, runs at grade 4 only, and lets you name the team (BBEv; `T2Baseball::Exec` Tokyo.t2p 0x10011c40).<br>• **Hawaii:** a wedding at the **Cathedral**. It runs on a holiday and needs ≥ 30 visitors and a couple (WdEv 15:00–17:00; `T2Wedding::Exec` Hawai.t2p 0x10015120).<br>• **Kegon:** the "Attraction will be renovated" event (Attraction.t2p FUN_0x1000c290).<br>After that, population ≥ 12000 / 8000 / 1000 promotes. | as cited | TOWER is granted at 15000 + Security + Medical + Metro (`LevelUp.cpp:42`). `Cathedral::weddingCompleted()` exists but nothing calls it. | DIVERGENT | H |
| R5 | **Unlocking.** The toolbox lists a plugin when its ATTR `mGameLevel` (CFilePlugin+0x54) ≤ the current grade. The ATTR levels are the source of truth (e.g. Coin/BCD-Mart 3, Front Desk 2, Power Room 3, Fire Dept 4, Large Elevator 3). | T2.exe FUN_00422030 and FUN_0041ca80 (refilter on grade change); `attr_table.py` | Uses a hardcoded SimTower mapping (`LevelUp.cpp:52-79`). The ATTR level is parsed (`PluginResources.h:67`) but never used. | DIVERGENT | H |
| R6 | **Each grade change** calls `T2Pool::GameLevelChanged` (loads that grade's PGDF visitor pool), `IsDustOn(grade)` (trash system on from grade 3), `GetToiletDenomi(grade)` (restroom demand 1/10 from grade 1) and `GetColdDenomi(grade, month)` (illness rate, from CODF). | T2.exe FUN_0041da20; `T2Pool::GameLevelChanged` 0x10050390; `T2WorldDef::IsDustOn` 0x10072100, `GetToiletDenomi` 0x100720c0 | None of these systems exist in OpenSky. | MISSING | H |
| R7 | **Grade-up message and animation** are per world (Tokyo and Kegon override `GetGradeUpMessage` for grades 3, 4 and 5). The animation bitmaps are 1207–1210. | Tokyo.t2p 0x10015500, Kegon `WorldDef::GetGradeUpMessage` 0x100153d0, `SetupGradeUpAnimation` 0x10072e70 | Shows "Your tower has been given a N-Star Award" and the LevelUpDialog. | PARTIAL | H |

### 1.3 Worlds, scenarios and unlock attributes

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| W1 | **Three worlds with distinct parameters:**<br>• **Tokyo:** $10,000,000 start; 100 floors above / 10 below; 354 m wide; lobby 3 rows; sort key 0.<br>• **Hawaii:** $5,000,000; 45 / 6; 400 m; lobby 2 rows.<br>• **Kegon:** $2,000,000; 2 / 32 (you build down the cliff); 36 m; lobby 1 row. | WATR `initialFunds` +0x24c via ctor 0x1006e4e0; WATR's 6 "discarded" ints = [.., above, below, width]; world STRTABLE 20000 text | Has no world concept. Start is always $2M (`Game.cpp:358`), which happens to equal Kegon's. | MISSING | H |
| W2 | **The build area is a WORLDDEF RECT** (+0x200; Tokyo cols 36–332 × rows 6–120; Hawaii 56–392 × 3–62; Kegon 81–186 × 3–43). Drag-building clamps x to `[left, right]`. | `GetBuildArea` 0x1000a660; T2.exe FUN_0041e8f0 | YootTower mode is unbounded (`BuildRules.cpp:53-66`). The comment in TODO.md "MUST NOT become numeric limits" is now answered: the consumer is decoded. | MISSING | H (x clamp) / M (y) |
| W3 | **ATTR `mAttrib` looks like a world bitmask.** Worlds: Tokyo 0x7, Kegon 0xf0, Hawaii 0xf01. Items: 0x1 = Tokyo+Hawaii, 0x2 / 0x4 = Tokyo only, 0x10 = Kegon, 0x100 = Hawaii.<br>This is consistent with every world-specific item: final items Stadium 0x4, Cathedral 0x100, Attraction 0x10; the K-variants 0x10; all VIP destinations. The exact consumer (T2PluginSpecifier+0x1c) has not been located. | `T2PluginSpecifier::Initialize` 0x1004fd00 (ATTR→specifier +0x1c); `attr_table.py` | Has no world filter. | MISSING | M |
| W4 | **ATTR `mDependID`** (+9 4cc) = "NORF" (Front Desk) on Single/Twin/Suite rooms and Housekeeping. Rooms must be linked to a Front Desk (string 1049 "Room is not connected to the front desk"; `T2GuestroomTable`). | ATTR sweep; `T2GuestroomTable::*` 0x100377a0.. | Hotel rooms stand alone. | MISSING | M |
| W5 | **`mMustItem` / `mSecretItem`** are 1 on Kegon (world) and Spa, and on Floor, SkyBridge, Stadium, Cathedral, the K-elevators and the worlds, respectively. Their engine semantics are not traced. | `T2PluginSpecifier::Initialize` (+0x24 = mustItem) | n/a | MISSING | L |
| W6 | **"Can only be placed once":** TenantDef `maxBuildCount` (+0x104) = 1 for Stadium, Cathedral, Attraction, Observatory, Subway, Theater, ClassicTheater, Power Room and Boat Deck. | `T2TenantDef` ctor 0x100623c0, `GetMaxBuildCount` 0x10004ef0; STRTABLE 9999 msg | Only Metro and SECOM are single-instance (`Game.cpp:2140-2153,2285`). | PARTIAL | H |
| W7 | **Corner-click doubler cheat:** clicking unit (1,1)–(2,2) while funds == the initial funds pays +initial (`DoPay(-f,4)`). | T2.exe FUN_0041e6f0 | Has the SimTower variant at (-187,-10) / $2M (`Game.cpp:2037`). | PARTIAL | H |
| W8 | **Buried treasure** (world `CheckTreasure`, e.g. Hawaii). It is a random roll whose odds depend on grade and funds: 1/12, 1/30 or 1/70. The outcome is a 1/3 "bone" (no money), otherwise $1,000,000 below grade 3 or $3,000,000 at grade 3+. | Hawai.t2p 0x10017890; world RT_STRING 4000–4002 | Deterministic B(s+2) award of $200k/300k/500k (`EventSystem.cpp:60-61`). | DIVERGENT | H |

### 1.4 Economy

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| E1 | **Prices are data.** Each ToolDef variation carries `price` (+0xd8), quarterly `outMoney` (+0xa8), `power` (+0xf0) and names. A full table is in §3. Examples:<br>• Office $100k; Condo $100k; Single room $30k; Suite $100k<br>• Simple Elevator $150k + $50k/car; Large Elevator $500k + $150k/car; Service $100k + $50k/car<br>• Stairs $5k; Escalator $20k; Floor $500/unit; Lobby $5k/unit<br>• Parking $50k/gate + $5k/lot; Subway $1M (+$500k/upgrade); Power Room $500k (+$500k per transformer)<br>• Stadium $5M; Cathedral $3M; Attraction $1M | `T2ToolDef` ctor 0x10065540, `CalcPayment` 0x10004e70; per-car price = MoverDef +0x194 | Uses SimTower prices (Office $40k, Condo $80k, Std elevator $200k + $80k/car …; `Factory`/item headers). Floor, Lobby, Stairs and Escalator happen to match. | DIVERGENT | H |
| E2 | **Floor-type items charge per empty cell covered** (attr 0x2000). | `T2TenantDef::CalcPayment` 0x10062b10 | A brand-new slab costs a flat $500 (`Game.cpp:1412,1425`). | PARTIAL | H |
| E3 | **Quarterly maintenance** is charged at midnight on day 1 of each quarter, for every item whose attr has 0x100:<br>• **ordinary items:** `outMoney[variant]`<br>• **attr 0x4000 (lobby, sky bridge):** `outMoney × width` (per unit)<br>• **movers:** `outMoney × cars`, halved if attr 0x2000<br>Paid through doc vtable +0x278 (DoPayTool). | T2.exe FUN_0041dc40 → `T2FloorInfo::CalcMentenanceCost` 0x100358e0 → `T2HaveOutViewObject::CalcMentenanceCost` 0x10039140; `T2TenantDef/T2MoverDef/T2ToolDef::CalcMentenanceCostProc` 0x10062df0/0x10040520/0x10065bc0 | Quarterly at 5:00, using SimTower categories and values (`Game.cpp:60-99`). | PARTIAL | H |
| E4 | **Office rent:**<br>• **Quarterly charge:** on day 1 of the quarter, between 04:51 and 05:00, the office pays `3 × inMoney[priceIndex]` (inMoney = [10,20,30,40], i.e. $1,000–$4,000 per day).<br>• **On occupancy:** it also pays once, via SetKind, with the message "Income from office rentals just came in".<br>• **Who works:** workers are called at the open time on non-holidays only. | Office.t2p FUN_0x1000c0c0 (ChangeStatusOnIdle) | Charges 15k/10k/5k/2k per quarter plus a $10k deposit (`Game.cpp:31-58`, `Office.cpp:66,149`). | PARTIAL (timing) / DIVERGENT (values) | H |
| E5 | **Price levels:** priced tenants have ≤ 4 `inMoney` levels (`GetPriceIndex` = selected + 1). The residential estimate gets `+(3 - priceIndex)*30`. | `T2EquipDef::GetInMoney` 0x1002f8a0, `T2Tenant::GetPriceIndex` 0x1005f880, `UpdateResidencialEstimate` 0x10060e20 | Uses 4 rent classes, but they do not affect evaluation that way. | PARTIAL | H |
| E6 | **Commercial income and evaluation:** income = `inMoney × customers/100 − maintenance`; the estimate = `70 × income / maintenance`, clamped to 1–300, so 70 is break-even. | `T2Tenant::CalcInMoney` 0x1005f620, `CalcProfitEstimate` 0x10061410 | Pays fixed amounts at closing time by customer-count threshold (`FastFood.cpp:195`, `Restaurant.cpp:183`). | DIVERGENT | H |
| E7 | **Hotel income** sums `inMoney` for rooms in occupied states 31–34, per Front Desk table. Rooms: Single [8,12,20,32], Twin [15,30,45,60], Suite [45,60,90,120], i.e. $800–$12,000. | `T2GuestroomTable::CalcInMoney` 0x10037a00 | Pays the class-0 rate at checkout (`Hotel.cpp:335`). | PARTIAL | M |
| E8 | **Event payments** use doc vtable +0x270 (`DoPay`, category 2): the fire helicopter, terrorist ransom and Power Room transformers. | `T2FireBurning::Start` 0x10030d50, Terrorist 0x10063a60, PowerRoom.t2p 0x1000c4a0 | `transferFunds` with toasts. | MATCH (mechanism) | H |
| E9 | **No loans:** there is no loan class or string in T2DLL or T2.exe. The funds dialog has 8 fixed cash steps 0…$20M (`GetCashAmount`). | string and class inventory; `T2WorldDef::GetCashAmount` 0x1000a790 | No loans. | MATCH | M |

### 1.5 Tenant evaluation, complaints and departures

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| J1 | **The estimate scale is 1–300.** The colour is red below 70, yellow 70–200 and blue above 200. | `T2Tenant::SetEstimate` 0x1005ffb0, `CalcEstimateColor` 0x10060a20 | Uses 0–100 with critical below 25 (`JudgeSystem.cpp:94-186`). | DIVERGENT | H |
| J2 | **Residential and office estimate** = mean of the residents' people-estimates − `senseOfNoise × trafficRange(entrance)` + `(3-priceIndex)*30` − years since built. `senseOfNoise` is Condo/Hotel 150 and Office/Apartment 100. | `UpdateResidencialEstimate` 0x10060e20, `T2TenantDef::GetSenseOfNoise` 0x10005540, `T2TrafficInfo` | Uses a same-floor 20-cell noise rule for hotels only (`JudgeSystem.cpp:31-62`). | DIVERGENT | H |
| J3 | **Departure and growth:** a tenant whose estimate falls below 70 loses its residents (they move out). Above 200, residents duplicate (growth); between the two, they duplicate every 5th call. | `DecideResidencialEstimation` 0x10061380 | Move-out at < 30 or no route (`Office.cpp:364`); no growth. | DIVERGENT | H |
| J4 | **The complaint/status catalogue** is a fixed set of 26 messages: no demands, route, far elevator, power down, Reggae noise, no restaurants, needs parking, needs doctor, bad atmosphere, no trash facility, not connected to trash, full of garbage, forced to close, customers few/many/weekend/rainy, opens tomorrow, beds not made, room dirty, not connected to front desk. | T2.exe STRINGTABLE 1024–1049 (`dump_rsrc.py --strings`) | About 46 generic toasts. | PARTIAL | H |

### 1.6 People simulation and routing

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| P1 | **Stress (0–300):** while waiting for a mover, stress rises +5 per idle tick. At 300 the person gives up (teleports) and loses 50 estimate. With no route, stress rises +30 per attempt; at 300 the game shows "No route to destination." and the person loses 150 estimate. | `T2People::IdleWaitMover` 0x10048880, `SolveNoRoute` 0x10048c40, `IncStress` 0x100497e0 | Stress is also capped at 300, but it comes from per-floor stairs (+35) and escalators (+16) plus walking; there is no per-tick wait stress (`Person.cpp:36-57`). | PARTIAL | H |
| P2 | **The people-estimate** is a running average over up to 4 trips, `(est×n − stress + 300)/(n+1)`. The VIP and tenant estimates are built from it. | `CalcCurrEstimate` 0x100496f0, `UpdateEstimation` 0x10049790 | No per-person estimate. | MISSING | H |
| P3 | **Route cost** per mover = `MoverDef.score × floors` (Stairs 50, Escalator 30, Simple 15, Service 8, Large 5, K-elev 15). | `T2Mover::CalcScore` 0x1003ef20; MVDF `score` | Uses A* constants (`PathFinder/MapSearchNode.h:47-58`). | DIVERGENT | H |
| P4 | **Population is data-driven:**<br>• per-grade visitor pools (PGDF via PODF)<br>• hourly demand lists by time zone (PZDF)<br>• tenant member tables (FDmT: demand / silhouette / count / rate / percent)<br>• customer search scores (DPDF EPlace scoring) | `T2Pool` 0x100501e0/0x10050390, `T2PeopleDemandList`, `T2TenantMemberTableDef` 0x100631e0 | Spawning is hardcoded per item class. | MISSING | M |
| P5 | **Restrooms:** people need a toilet (the rate is set by `toiletDenomi`). `SearchToilet` looks in registry 7 within a route score. On failure it shows "No restroom nearby" / "Restroom is crowded" and costs 30 estimate. | `T2People::SearchToilet` 0x100499c0, `T2RegistedTenantDB::SearchToilet` 0x10051f30 | None. | MISSING | H |
| P6 | **Illness and doctors:** the CODF cold table, per grade and month, makes people sick. Tenants complain "Requires a Doctor's office nearby". | `GetColdDenomi` 0x100720e0; string 1034; Doctor's.t2p overrides | Medical Center only gates promotion. | MISSING | M |

### 1.7 Movers

| # | YootTower rule (MVDF via `T2MoverDef` 0x1003ffb0) | OpenSky | Status | Conf |
|---|---|---|---|---|
| M1 | **Simple Elevator** (EdtS): ≤ **15 floors** ("can be extended up to 15 floors"), ≤ 6 cars, 17 per car, speed [1,3,0], grade 1, $150k + $50k/car, $12k/Q. | Standard: span 29, 8 cars, 21 per car, $200k + $80k (`Elevator.h:27-35`, `Elevator.cpp:111`). | DIVERGENT | H |
| M2 | **Large Elevator** (EgiB): any length ("to the top floor"), ≤ **8 stops** (`maxStop`), 8 cars, 40 per car, speed [1,3,12], grade 3, $500k + $150k/car, $20k/Q. | Express: stops only at `floor%15==0` plus basements, 42 per car (`Express.h:28-33`). | DIVERGENT | H |
| M3 | **Service Elevator** (EreS): any length, 8 cars, 21 per car, grade 2, $100k + $50k/car, $10k/Q, staff only. | Service: span 29, 21 per car. | PARTIAL | H |
| M4 | **Kegon variants** (VLEK / RESK): length 50 / 1000, 20 per car, $50k + $20k/car, $5k/Q ("extended 34 floors"). | none | MISSING | H |
| M5 | **Stairs and Escalator** connect adjacent floors; capacity 100; Stairs $5k with no maintenance, Escalator $20k at $5k/Q, grade 1 (Escalator is grade 1 in YootTower). | Prices match (`Stairs.h:14`, `Escalator.h:14`), but the escalator unlocks at 3★ (`LevelUp.cpp`). | PARTIAL | H |
| M6 | **Lobby is world-dependent (3/2/1 rows).** Elevators cannot stop on the upper lobby rows (`IsStoppableFloor` 0x1000a6b0). A floor may be stopped at only on its bottom row (`T2Mover::AddStop` 0x1003e780). There is no 15-floor sky-lobby rule; the only lobby rule is "Only the lobby item can be placed on the first floor". | Lobbies are allowed only on `y%15==0`, sky lobbies and transfers apply (`Game.cpp:2160`). | DIVERGENT | M |
| M7 | **Elevator/escalator behaviour** (adding cars, shaft drag, stop selection, car AI) is plugin code: StdElev, BigElev and ServElev each override about 20 MoverDef virtuals. | Native SimTower elevator. | PARTIAL | H |
| M8 | **Sky Bridge** connects two towers. It needs ≥ 3 free floors above and below, must be above the 5th floor, must sit between the towers within a min/max distance, and allows no transit on it. $5k/unit, $1k/unit/Q. | none (OpenSky has one tower). | MISSING | H (msgs) / M (rules) |

### 1.8 Placement rules

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| L1 | **75 placement and removal error rules** in T2.exe STRTABLE 9999. Examples: floors can only be removed top-down or bottom-up; entrances (parking gates) must be stacked from the first floor; parking lots must be adjacent to a gate or lot and removed from the outside; the subway goes only on the lowest floor; boat deck at the left edge of B2; observatory at the left edge of B32; nothing above the sky bridge. | `dump_rsrc.py --type STRTABLE --id 9999` | Has its own subset of rules. | PARTIAL | H (rules exist) / M (each check) |
| L2 | **Final item only on the top floor** ("Only Final item can be placed on the top floor of building"): Stadium above the 100th floor, Cathedral above the 45th, Attraction above the 2nd (MVDF/TNDF descriptions). | as above + TNDF desc | Cathedral is a 5★ item placed anywhere. | DIVERGENT | H |
| L3 | **Floor bounds per world** (100/10, 45/6, 2/32). | W1/W2 | Fixed B9 floor, about 120-floor camera, no height check (`Game.cpp:2134,960`). | DIVERGENT | H |

### 1.9 Infrastructure systems that are new in YootTower

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| I1 | **Power** (BOEV = [defSupply 5000, perTransformer 5000, transformerPrice 5000 = $500k]).<br>• **Supply:** `def + 5000 × transformers` (≤ 5 per Power Room; buying one costs the price).<br>• **Demand:** every item consumes `power[variant]` (e.g. Theater 200, Stadium 300, Subway 300, elevators 20–30).<br>• **Warning:** "Shortage of power" when supply − demand < 50.<br>• **Blackout:** a daily check at 11:00–14:00. If demand > supply, there is a blackout: +30 stress to everyone; at the end, "residents escaped through the emergency stairs" and +300 stress, until fixed.<br>• **Power Room:** grade 3, $500k, $50k/Q, once. | `T2BlackOut` 0x1001b580–0x1001ba90; PowerRoom.t2p 0x1000c4a0 | none | MISSING | H |
| I2 | **Trash** (from grade 3).<br>• **Collection:** restaurants (category 50) within route range of a Trash Facility deliver garbage, which fills it in 5 stages.<br>• **Emptying:** the facility empties between its close and open times (07:00→07:30).<br>• **Warning:** "Building requires a Trash facility" at 06:50 and 21:50 if there is none.<br>• **Cost:** $500k, $50k/Q (Kegon variant $200k).<br>• **Complaints:** "There's no Trash Facility", "not connected", "Full of garbage". | KTrashFacility.t2p 0x1000bba0/0x1000bcc0/0x1000bde0/0x1000bf00; `IsDustOn` | Recycling Center only gates promotion. | MISSING | H |
| I3 | **Restroom** ($10k, $1k/Q, grade 1, registry 7). | P5 | none | MISSING | H |
| I4 | **Security Room** ($100k, $20k/Q, grade 2, registry 5). It supplies up to 6 firefighters or bomb searchers. Fire-fighting by staff is refused without one ("no security room"). | `T2FireBurning::DialogHook` 0x10031570 (`GetTenantByPID('IBEK')`), `CallFireman` 0x10031ed0 | The Security office exists, with 6 guards. | PARTIAL | H |
| I5 | **Doctor's Office** ($500k, $50k/Q, grade 3, registry 4); **Fire Department** (Kegon, grade 4); **Housekeeping** + **Front Desk** (hotel back office). | ATTR, TNDF, overrides | Medical Center only; no front desk. | MISSING | M |
| I6 | **Parking:** a surface gate plus lots; registry 3; `FindEmptyParking`. Office visitors and hotel guests park. | Parking.t2p (14 overrides), `T2RegistedTenantDB::FindEmptyParking` 0x10052040 | Parking ramps are not registered, so parking is unusable (`Factory.cpp:41-67`). | DIVERGENT | M |
| I7 | **Subway** (Tokyo, lowest floor B10, $1M, upgrades $500k, $100k/Q, grade 2). The MREV train timetable has 12 arrivals 06:50–23:40 per day. | MREV via FUN_0x1003d690 (`T2Transport`) | Metro (1 per tower, underground, visitors 10–17h with a 1/36 roll). | PARTIAL | M |

### 1.10 Events

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| V1 | **Events are scheduled from data.** The world's T2EV lists `(type, rsrcId)`. Each event header is `{levelBit (grade mask), exclusive, cycleDay, beginMin, endMin}`. An event runs when the grade bit is set and `(lapseDays+1) % cycleDay == 0` (overridable), starting at beginMin. | `T2TowerEvent` ctor 0x10068ca0, `Idle` 0x10069970, `T2EventItem` 0x10030050/0x10030280 | Hardcoded `day%N` cadences (`EventSystem.cpp:32-34`). | PARTIAL | H |
| V2 | **Fire** (FBEv: grades 3–4, every **60** days, 12:00–18:00). You pay for the helicopter at **$300k** (needs funds), send the security staff (needs a Security Room), or do nothing. | `T2FireBurning::Start/StartEvent/DialogHook` 0x10030d50/0x10031000/0x10031570; FBEV data | Every 84 days, 3★+, chopper $500k, clock jump (`EventSystem.cpp:32-44`). | DIVERGENT | H |
| V3 | **Terrorist bomb** (TrEv: Tokyo grades 2–3, Hawaii grade 3; every **36** days at 09:00). The ransom is **$300k** with a deadline of 720 (12:00). The bomb goes into a random tenant that is not fireproof, and security guards search for it. | `T2Terrorist::Start` 0x10063a60; TREV [3000,720,5000] | Every 60 days at 2–4★, ransom $200k/$300k/$1M, detonates 13:00 (`EventSystem.cpp:47-51`). | DIVERGENT | H |
| V4 | **Thief / noisy neighbour "Uncle Reggae"** (MaEv, Tokyo, grades 3+, every 20 or 25 days). He visits tenants and produces complaints ("Uncle Reggae is noisy and annoying") until the security staff catch him. | `T2Maru_Reggae` 0x1003a770–0x1003b8e0 | Stub only (`JudgeSystem.cpp:148`). | MISSING | H |
| V5 | **Santa** (SaEv every 12 days, all grades, cosmetic). | `T2SantaClaus` 0x100560b0 | Every 12 days at 21:00 (`EventSystem.cpp:34`). | MATCH | H |
| V6 | **World X-events:** Tokyo Baseball (BBEv); Hawaii Wedding (WdEv), Cruise (`T2Cruise`) and DoEv; Kegon Tonbi (ToEv) and HotSpring (HSEv); ClEv clouds; OdEv rooftop decorations. | `WorldDef::MakeXEvent` (Kegon 0x10015270), Tokyo/Hawai MakeXEvent overrides | Only the unused Cathedral wedding exists. | MISSING | H (list) / M (rules) |
| V7 | **Blackout** (BOEv, daily) — see I1. | | none | MISSING | H |

### 1.11 Specials, toolbox and messages

| # | YootTower rule | Evidence | OpenSky | Status | Conf |
|---|---|---|---|---|---|
| S1 | **Movie theaters:** Theater ($500k, 500 seats, power 200) and Classic Theater show MV_* movie plugins (SP format). The **Event Center** hosts Pl* hall events and settles `funds += value − cost` at close. | Theater/EventCenter overrides; `world-and-sp-formats.md` §SP | SimTower Cinema / Party Hall. | PARTIAL | M |
| S2 | **Observatory** (Kegon, $1M, open 08:00–18:00), **Boat Deck** (Hawaii, B2 left edge), **Spa / Gathering Area / School / Ticket booth**. | TNDF + overrides | none | MISSING | M |
| S3 | **AquaZone** (Aquazone.t2p, about 10 KB) is not a PE item plugin. Its format belongs to the SP agent. | — | none | MISSING | L |
| S4 | **Toolbox:** categories and order come from data (`GetSortKey` = category×65536 + toolNo, 0x10065a50). The palette chrome is `DefaultToolPalette.t2p`; icons are each plugin's bitmap 100. | `T2ToolDef::GetSortKey` 0x10065a50, `T2ToolWindow` | Hardcoded toolbox tree (`ToolboxWindow.cpp:34`). | DIVERGENT | H |
| S5 | **Messages:** an info bar (timed), balloons over people and equipment, alert dialogs, and per-category pay-message rate limiting. | `T2TowerMessage::*`, `T2Settlement::EmitPayToolMessage` 0x10059600 | Single 3 s toast plus modals (`TimeWindow.cpp:298-304`). | PARTIAL | H |
| S6 | **Open/close hours are data** (TNDF +0x174 / +0x178 minutes, e.g. Burger 08:00–22:00, Office 08:00–17:00, hotel check-in 17:00 / out 11:00). | `item_rules.py` (f174/f178) cross-checked with KTrash 0x1000bba0 | Hours are hardcoded per class. | DIVERGENT | M |

### 1.12 Status counts

| Status | Count |
|---|---|
| MATCH | 4 (T2 calendar shape, E8 payment mechanism, E9 no loans, V5 Santa cadence) |
| PARTIAL | 21 |
| DIVERGENT | 24 |
| MISSING | 22 |

(71 rows; rows with a split verdict are counted under the first status listed. No YootTower-specific
numeric rule is implemented in OpenSky today.)

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

0. **Rules profile seam.** Add `struct RulesProfile` (funds, bounds, grade table, calendar hooks), owned by
   `BuildRules`, and make `Game` read the start funds and bounds through it.
   - Files: `BuildRules.{h,cpp}`, `Game.cpp`.
   - Test: the SimTower and OpenSky profiles return the current constants; the YootTower profile returns a stub Tokyo profile.
1. **World loader** (depends on `world-and-sp-formats.md` and 0). Parse WORLDDEF, WATR and T2EV in C++
   (`PluginResources`), port `world_rules.py`, and add a world selector (default Tokyo) with save attribute `world=`.
   - Files: `PluginResources.*`, `Application.cpp` (`--world tokyo|hawaii|kegon`), `Game.cpp` save and load.
   - Test: Tokyo gives funds 10,000,000, grades `[1000,2000,5000,12000]` and lobby height 3; Kegon gives start grade 3.
2. **Start funds, bounds and floor limits** (W1, W2, L3): apply the build-area RECT and the above/below limits; use the world's lobby height as the default.
   - Files: `BuildRules.cpp`, `Game.cpp` (placement checks at the `allowsHorizontalFootprint` call sites).
   - Test: the placement predicate rejects x < 36 in Tokyo, and rejects floor 46 in Hawaii.
3. **Calendar hooks** (T1, T4, T5): implement the holiday formula and per-world rain. Keep the SimTower clock, but add a minute-linear `Time` mode for YootTower.
   - Files: `Time.{h,cpp}`, `Sky.cpp`, `LevelUp.cpp`.
   - Test: a table of holidays and rainy days for years 1–3, compared with a direct port of the decompiled functions.
4. **Data-driven item catalog** (E1, M1–M5, S6, W6): port `item_rules.py` to `parsePluginTemplateDef()` and overlay price, maintenance, power, capacity, inMoney, hours, maxBuild, span, cars, stops and score onto the prototypes in YootTower mode.
   - Files: `PluginResources.*`, `Item/Factory.cpp`, `Item/Prototype.h`, the elevator headers.
   - Test: the parsed StdElev gives price 1500, car price 500, max length 15 and 6 cars; all 100 plugins consume their resource fully.
5. **Unlocks** (R5, W3): drive the toolbox lock from ATTR `mGameLevel` and the world mask.
   - Files: `LevelUp.cpp` (`minRatingToBuild` profile branch), `ToolboxWindow.cpp`.
   - Test: in Tokyo, Coin (Kegon-only) is hidden and BCD-Mart unlocks at grade 3.
6. **Grades and VIPs** (R1–R4, R7): add a grade table with per-grade VIP flags, the VIEV-driven VIP (estimate > 200), checking every tick, and a `grantFinalGrade()` hook.
   - Files: `LevelUp.cpp`, `VipSystem.cpp`, `Game.cpp:1808-1874`.
   - Test: a mocked population of 999 vs 1000 with the flag set or unset; the final hook works only at grade == numGrades.
7. **Economy timing and values** (E2–E7): quarterly maintenance at midnight on day 1 from `outMoney`; office rent `3×inMoney` at 04:51–05:00 on day 1; commercial income from customers × inMoney; hotel per-room income.
   - Files: `Game.cpp:60-156`, `Office.cpp`, `Shop.cpp`, `Hotel.cpp`.
   - Test: a ledger fixture over one quarter.
8. **Evaluation scale and formulas** (J1–J3, P2): a YootTower JudgeSystem path on a 1–300 scale, with the residential formula and the 70/200 thresholds.
   - Files: `JudgeSystem.cpp`, `Person.cpp`.
   - Test: formula unit tests with noise, price and age inputs.
9. **Stress and routing score** (P1, P3): +5 per tick while waiting, the 300 give-up rule, the no-route rule, and mover score × floors.
   - Files: `Person.cpp`, `PathFinder/MapSearchNode.h`.
   - Test: a waiting person reaches 300 after 60 ticks.
10. **Data-driven event scheduler** (V1–V3, V5): a T2EV-driven scheduler, plus the fire and terrorist parameters from FBEV and TREV.
    - Files: `EventSystem.cpp`.
    - Test: the schedule over 120 days matches the levelBit and cycleDay rules.
11. **Power** (I1, V7): consumption from item data, the Power Room with transformers, warnings, the blackout and its stress.
    - Files: new `PowerSystem.{h,cpp}`, `Item/PowerRoom.*`.
    - Test: supply and demand arithmetic, and the blackout window.
12. **Trash, restrooms and doctors** (I2, I3, P5, P6, R6).
    - Files: new items plus `Person.cpp` needs.
    - Test: no trash facility at grade 3 → warning at 06:50.
13. **Placement rules** (L1, L2, M6, I7): final item only on the top floor, subway on the lowest floor, parking gate and lot adjacency, floor removal order, no sky-lobby rule.
    - Files: `Game.cpp` placement.
    - Test: one placement-predicate test per rule.
14. **Native special items** (I4–I6, M8, S1–S2, V4, V6), one PR each: Front Desk with linked rooms, Housekeeping, Parking, Subway timetable, Sky Bridge (needs multi-tower support), Theater and Event Center hall events (needs SP plugins), Stadium/Baseball, Cathedral/Wedding, Attraction, Observatory, Uncle Reggae.
V. **Oracle validation** (cross-cutting): for each step, script `tools/yoottower/oracle/run_yoot.sh` on a
   `.bld` (read before and after with `tools/yoottower/bld.py`) to confirm the dynamic behaviour, especially
   E4 rent timing, T4 holidays, R3 VIP arrival, and I1 blackout.

**Ambiguities to settle with the oracle:** the W3 world-mask consumer, the W5 must/secret flags, Y-axis
build-area enforcement, the meaning of the TNDF f170 field, the exact sub-tick rate per speed setting, and
hotel income timing.
