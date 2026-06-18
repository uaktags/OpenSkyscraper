OpenSky TODO
============

Status legend: `[DONE]` complete · `[PARTIAL]` started but incomplete · `[ ]` not started · `[STUB]` placeholder exists · `[DEFERRED]` intentionally postponed

Branch: `feature/simtower-gap-impl` — see `.omo/plans/simtower-gap-implementation.md` for full design notes.

Sources of truth for status were verified against `source/` (not just the plan doc) on 2026-06-17.


-------------------------------------------------------------------------------
General / Cross-cutting
-------------------------------------------------------------------------------

- [DONE] `mapWindow` is now its own class `MapWindow` (Phase 4.2).
- [ ] Cache the result of the KWAJ decompression so the game doesn't decompress `SIMTOWER.EX_` every launch.

### Pausing doesn't affect elevators
When pausing the game, elevators keep moving as if the game was unpaused. They react to speedup by moving faster — investigate why they don't react to the speeddown.

### Game Speed
Game speed seems rather fast compared to elevator movements. Cinema customers hardly arrive at the theatre in time. Slow down time, or speed up elevators?

### Different Access Floors for Items
People enter all items on floor 0 (relative to the item). Certain items (Metro station, Cinema) are accessed via floor 1. The item prototype should carry a field that enables a relative shift of the access floor.

### Clean up CMakeLists.txt
[DONE] Removed old Lua/ObjectiveLua/CEGUI cruft.


-------------------------------------------------------------------------------
SimTower Gap Implementation — Phase Tracking
-------------------------------------------------------------------------------

Phase 0: Foundation Repairs                                  [DONE]
-----------------------------------------------------------------------

All four foundation repairs landed on `feature/simtower-gap-impl`.

- [DONE] **0.1 Fix priority-queue comparator bugs** — `Office.h` and `Condo.h` comparators were self-comparing (`a > a`); corrected to `a > b`. Condo time helpers straightened out. Commit `836ef8a`.
- [DONE] **0.2 Fix elevator queue cleanup** — `Elevator::cleanQueues()` iterator invalidation / double-delete repaired. Commit `563ea44`.
- [DONE] **0.3 Empty-route guard in Person::Journey** — `Person::Journey::set()` / `next()` now guard against empty `nodes`. Commit `108e5a2`.
- [DONE] **0.4 Restore HUD updates** — Commented-out `TimeWindow.updateFunds/updateRating/updatePopulation` etc. re-enabled. Commit `e58e125`.


Phase 1: People & Simulation Foundation                      [DONE — Metro pending]
-----------------------------------------------------------------------

- [DONE] **1.1 Person state machine + identity fields** — `Person::State` enum (`kWandering, kHome, kCommuting, kWorking, kLunch, kShopping, kReturning, kResting, kIdle`) added to `source/Person.h`. `name`, `from`, `goingTo`, `eval` fields added. Commit `a4be685`.
- [DONE] **1.2 Stress system** — `Person::addStress(amount)` clamps to `[0,100]`; existing stress mutations routed through it. Commits `4d1c5e5`, `6097f22`.
- [DONE] **1.3 NameManager** — `source/NameManager.{h,cpp}` generates default display names per person type; covered by test. Commits `98bc941`, `b66ebfb`.
- [DONE] **1.x Person::advance() wired into Game loop** — Commit `75102e3`.
- [DONE] **1.x Condo state transitions** — `CondoOccupant` driven by new State enum. Commit `ca424bc`.
- [DONE] **1.x Office state transitions** — `Office::Worker` driven by new State enum. Commit `ae13bd4`.
- [DONE] **1.x FastFood / Restaurant / Cinema subclass state** — Guests now transition through `kLunch` / `kShopping` / `kReturning` on arrival and departure. (Plan listed this as pending; verified present in `FastFood.cpp:195`, `Restaurant.cpp:192`, `Cinema.cpp:193`.)
- [DONE] **1.x Metro subclass state** — `Metro::Visitor` cycles `kCommuting` → `kShopping` → `kReturning` → `kIdle` (commit for Phase 2.4).


Phase 2: Missing Items & Commercial Depth                   [Hotel DONE, rest in progress]
-----------------------------------------------------------------------

- [DONE] **2.1 Hotel system** — `source/Item/Hotel.{h,cpp}` implements single/double/suite variants, room state (clean/occupied/dirty), full guest lifecycle (arrival → dinner → sleep → wake → checkout), housekeeper dispatch, persistence, and Factory registration (`hotel_single`, `hotel_double`, `hotel_suite`, with legacy `hotel` migration).
    - [ ] Verify VIP check-in → rating boost (original: VIP arrivals affect tower rating).
    - [ ] Verify guest actually pathfinds to a Restaurant for dinner (currently tagged `goingTo = "Restaurant"` but no route lookup).
    - [ ] Validate sprite frame rects against `single` / `double` / `suite` sheet.

- [PARTIAL] **2.2 Parking system** — Basic item landed.
    - [DONE] `source/Item/Parking.{h,cpp}` with `totalSpaces()` (derived from width × 2), `usedSpaces()`, `hasSpace()` / `assignSpace()` / `freeSpace()`.
    - [DONE] Registered in `source/Item/Factory.cpp` as prototype `"parking"`; added `ICON_PARKING` to `Factory.h`.
    - [DONE] Tower-wide coverage metric in `JudgeSystem::computeParkingCoverage()` (1 space / 4 offices + 1 / hotel room); Office and Hotel scoring penalised when coverage < 50% / 100%.
    - [DONE] `used` counter persisted in XML.
    - [ ] **Gate integration** (`SetupAllGate()` equivalent) — connect parking to road access.
    - [ ] **Cars visually appear/disappear** on tiles (per-cell occupancy sprites).
    - [ ] Hook `assignSpace()` / `freeSpace()` into actual office-worker / hotel-guest arrival (currently the coverage check uses total capacity only).

- [PARTIAL] **2.3.1 Office: rent, evaluation, lunch** (`source/Item/Office.{h,cpp}`)
    - [DONE] Rent + deposit collection on Monday 05:00 (`Office.cpp:105`-`122`).
    - [DONE] `findLunchRoute()` + `kLunch` dispatch at 12:00 (`Office.cpp:180`-`183`).
    - [DONE] `isAttractive()` route-based gate for move-in.
    - [ ] Extend `isAttractive()` to consider amenity coverage (security, food reachability) not just route score.
    - [ ] Salesman behaviour: leave for sales, return, no lunch.
    - [ ] Stress recovery when worker returns from successful lunch; stress gain when no food reachable.

- [PARTIAL] **2.3.2 FastFood / Restaurant: schedules & pricing** (`source/Item/{FastFood,Restaurant}.{h,cpp}`)
    - [DONE] Person state transitions (`kLunch` / `kShopping` / `kReturning`).
    - [ ] Weekday-dependent hours (offices only populate Mon–Fri).
    - [ ] Customer count scaled by reachable office / hotel population.
    - [ ] Pricing model: `income = customers * pricePerMeal - dailyMaintenance`.
    - [ ] Restaurants should serve hotel guests at dinner; FastFood should serve office workers at lunch.
    - [ ] Stop admitting customers after 19:00 so venues empty toward end of day (existing `TODO.md` note).
    - [ ] Persist customers to XML (existing `TODO.md` note — F2 save/reload leaves empty venues).

- [DONE] **2.3.3 Cinema: movie scheduling & revenue** (`source/Item/Cinema.cpp`)
    - [DONE] Person state transitions on arrival/departure.
    - [DONE] Two screenings/day (13:00 doors / 15:00 start / 17:00 close, and 19:00 / 21:00 / 23:00).
    - [DONE] Attendance-based income: `attendees * ticketPrice - screeningFee` using `people.size()` at close.
    - [ ] Halfway-through-movie break (per original; verify).
    - [ ] Permit underground construction (verify original behaviour).

- [DONE] **2.3.4 PartyHall: event visitors** (`source/Item/PartyHall.cpp`)
    - [DONE] `PartyHall::Visitor` subclass; spawned on each open.
    - [DONE] Two parties/day (afternoon 13:00-17:00 and evening 19:00-23:00).
    - [DONE] Attendance-based income: `attendees * visitorFee - eventCost`.
    - [DONE] Visitors arrive, get stress relief, depart on close.

- [PARTIAL] **2.4 Metro train cycles** (`source/Item/Metro.cpp`)
    - [DONE] Train arrival / departure intervals driven by `Time::absolute` (default: 30 min gap, 10 min dwell; `kTrainDwellAbs` / `kTrainGapAbs` constants).
    - [DONE] `spawnVisitors()` emits 2–5 visitors per arrival, each routed to a reachable commercial venue.
    - [DONE] Visitors return to the platform after a random dwell window; departing train boards them (`boardReturnedVisitors()`) and yields fare revenue (`metro_fare`).
    - [DONE] Visitor state machine (`kCommuting` → `kShopping` → `kReturning` → `kIdle`) closes the last Phase 1.x subclass-state item.
    - [DONE] `Visitor` and `nextTrainTime` persisted across save/reload.
    - [ ] Tune `kTrainDwellAbs` / `kTrainGapAbs` against the original SimTower feel.
    - [ ] Target underground commercial specifically (currently any reachable venue).
    - [ ] Metro tracks as a decoration (parallel to fire stairs / crane — see existing Decorations work).
    - [ ] Enforce: only one Metro instance per game.
    - [ ] Forbid building any item below the Metro.


Phase 3: Game Systems                                        [STUB — major work pending]
-----------------------------------------------------------------------

- [PARTIAL] **3.1 Judge / Evaluation engine** — `source/JudgeSystem.{h,cpp}` landed.
    - [DONE] `JudgeSystem::evaluateAll(Game*)` called daily from `Game::settleDailyAccounting()`; runs after maintenance so scores reflect yesterday's state.
    - [DONE] Per-tenant scoring (`scoreOffice` / `scoreCondo` / `scoreHotel` / `scoreCommercial`) using route quality, occupant stress, amenity coverage (FastFood lunch, Restaurant dinner, Security).
    - [DONE] Scores cached on `Item::evaluation` (persisted in XML) and mirrored onto `Person::eval` for inspector / overlay use.
    - [DONE] `Office::isAttractive()` and `Condo::isAttractive()` consult evaluation (≥30 required, default 50 keeps fresh tenants attractive).
    - [DONE] `Counts` struct (offices / condos / hotels / hotelsDirty / foodOutlets / security / medical / population) approximates `CountT.h/c`; ready to feed star progression (3.2) and overlays (4.3).
    - [DONE] Tower-wide parking coverage metric feeds Office / Hotel scoring (see 2.2).
    - [ ] `JudgeAllHotel()` daily hotel performance review (currently each hotel is scored individually; aggregate review still TODO).
    - [ ] `ExpandoBadHotel()` shrink/grow under-performing hotels.
    - [ ] Per-type counters `CountPC` / `CountVC` / `CountIn` / `CountDay` (more granular than current `Counts`).

- [DONE] **3.2 Level / star-rating expansion** (`source/LevelUp.{h,cpp}`, `source/Game.cpp:ratingMayIncrease`)
    - [DONE] Star thresholds (1★ start; 2★ at 300 pop; 3★ at 1000 pop + Security; 4★ at 2000 pop + Medical; 5★ at 3000 pop + Metro).
    - [DONE] `ratingMayIncrease()` checks population **and** required facilities via `JudgeSystem::Counts`; loops to multi-promote if eligible.
    - [DONE] Construction handler rejects placement below `LevelUp::minRatingToBuild()` with a clear reason.
    - [DONE] `ToolboxWindow` greys out locked items with "Unlocks at N stars" tooltip; reloads on promotion.
    - [DONE] Promotion message via `TimeWindow.showMessage()`.
    - [ ] **VIP visits** and **Cathedral "Tower of the Year"** ending (codemap.md:2653, needs VIP system).
    - [ ] Level-up dialog window (currently a transient message, not a modal).

- [ ] **3.3 Fire / Emergency / Thief events** — No files exist. Priority LOW.
    - [ ] `EventScheduler` randomly triggers events based on time + rating.
    - [ ] Fire: floor-to-floor spreading, helicopter extinguish, water spray.
    - [ ] Terror: bomb threats, player defusal minigame.
    - [ ] Thief: targets tenants, calls police, existing `Security` item becomes functional (currently STUB).
    - [ ] Emergency mode: reset all person states, evacuate.

- [ ] **3.4 Weather / Environment (CLUT equivalent)** — No files exist.
    - [ ] `Lighting` manager class (SFML has no indexed palettes; use shader / vertex tint).
    - [ ] Time-of-day tint applied to all items in `Item::render()`.
    - [ ] Rain reduces brightness + blue tint (extend existing `Sky.cpp`).
    - [ ] Sunrise/sunset orange-red tint; night dark blue with lit-window sprite variants.


Phase 4: UI & Visualization                                   [PENDING]
-----------------------------------------------------------------------

- [PARTIAL] **4.1 Info / inspector dialogs** — `source/InspectorDialog.{h,cpp}` popup.
    - [DONE] TGUI popup opens on inspect-click; closes on Close button, title-bar X, or Escape.
    - [DONE] Item content: name, floor, daily maintenance, cached evaluation, occupant count, route score.
    - [DONE] Hotel content: room state (clean/occupied/dirty), capacity.
    - [DONE] Elevator content: cars count, queues count, serviced floors, unserviced count, total waiting passengers.
    - [DONE] Occupant list (up to 8): name, state, stress, eval for each person at the item.
    - [DONE] Refreshes every frame so values stay live; tears down on world clear.
    - [ ] **Per-person sprite hit-testing** — clicking an individual walking person should open their info directly (currently we list all occupants of the clicked item).
    - [ ] Wire tenant complaint messages (stress > threshold) to `TimeWindow.showMessage()`.
    - [ ] Tenant messaging: route-based complaints ("can't reach lobby", "no lunch nearby").

- [PARTIAL] **4.2 Minimap** (`source/MapWindow.{h,cpp}`)
    - [DONE] TGUI child window + SFML canvas overview, anchored to right edge.
    - [DONE] Items drawn as filled rectangles; elevators as dark bars.
    - [DONE] StatusMode-aware tinting (matches main viewport Eval/Hotel overlays).
    - [DONE] Toggle with `M` key; closes on Escape; hidden by default.
    - [DONE] Click-to-jump via `Game::centerViewportOnTile()`.
    - [DONE] Throttled refresh (~1 s of game time) plus reload on GUI rebuild / world clear.
    - [ ] Toolbar button to toggle (currently keyboard only).
    - [ ] Dedicated Pric palette (deferred with the rent-pricing model).
    - [ ] Separate grey-body + sky-background rendering mode per the original (`MapT.h/c`).

- [PARTIAL] **4.3 Status overlays (Eval / Pric / Hotel)** — `Game::StatusMode` enum + `O` key cycle.
    - [DONE] `enum StatusMode { kNormal, kEval, kPric, kHotel }` on Game; cycle via `O` key with mode-name message.
    - [DONE] Eval overlay: blue/yellow/red tint on each tenant item based on `Item::evaluation`.
    - [DONE] Hotel overlay: red on dirty rooms, yellow on occupied, green on clean.
    - [DONE] Per-item culling so offscreen overlays are skipped.
    - [ ] **Pric overlay** — needs rent pricing tier data per tenant (deferred until pricing model lands).
    - [ ] Toolbar button to cycle modes (currently keyboard only).
    - [ ] Minimap integration (Phase 4.2) so the overlays also render on the minimap.

- [DONE] **4.4 Construction animation** (also in original `TODO.md`)
    - [DONE] `bool underConstruction` + `double constructionEndTime` on `Item`; persisted in XML.
    - [DONE] `Item::render()` draws a hatched khaki overlay rectangle while building (real sprites hidden).
    - [DONE] `Game::advance()` skips the item's advance, maintenance, and construction timer check; clears the flag with a "normal" construction sound on completion.
    - [DONE] `Factory::make()` sets the flag on fresh placement; `decodeXML()` overrides with saved state.
    - [DONE] Inspector dialog shows "under construction" with remaining-time estimate.
    - [DONE] Duration scales with item width (`constructionDuration()` virtual, overridable per subclass).
    - [ ] Swap rectangle overlay for actual `construction/solid` and `construction/grid` bitmaps.
    - [ ] Per-prototype duration tuning (currently 2h + 0.2h/tile width).


Phase 5: Polish & Balance                                     [IN PROGRESS]
-----------------------------------------------------------------------

- [PARTIAL] **5.1 Tenant satisfaction & retention**
    - [DONE] Stress accumulation + `addStress()` plumbing.
    - [DONE] Office workers flee for the day when stress > 80 (state → `kReturning`, route to lobby, partial relief on the way home).
    - [DONE] Missed lunch now applies a meaningful 15-point stress hit (was 0.1).
    - [DONE] Daily complaint message from `JudgeSystem` when critical-tenant count > 0 (with hint to open the Eval view).
    - [DONE] Office/Condo weekly vacation via `isAttractive()` (evaluation ≥ 30) already wired.
    - [ ] Condo occupants vacate mid-week when stress is critical (currently only weekly via `isAttractive`).
    - [ ] Hotel guests rate stay based on cleanliness, noise, elevator wait (currently fixed `eval = 50` on arrival; JudgeSystem overwrites daily but in-stay feedback is shallow).
    - [ ] `population` correctly updates when tenants leave mid-day (verify path for stress-flee).

- [PARTIAL] **5.2 Noise / zoning system** (original `Kinsoku.h/c`)
    - [DONE] Noise profile per tenant type (loud: office/fastfood/restaurant/cinema/partyhall/metro; sensitive: condo/hotel).
    - [DONE] Isolation radii from SimTower Notes (30 tiles for condos/240px, 15 tiles for hotels/120px).
    - [DONE] `computeNoisePenalty()` scans same-floor and adjacent-floor neighbours within radius, linear falloff, cross-floor dampening.
    - [DONE] Penalty wired into `scoreCondo` (full) and `scoreHotel` (70%) inside JudgeSystem.
    - [ ] `noiseLevel` / `noiseSensitivity` as actual `AbstractPrototype` fields (currently a lookup table in JudgeSystem).
    - [ ] Stress penalty applied directly to occupants (currently only depresses the cached evaluation).
    - [ ] Per-prototype tuning (e.g. cinema louder than office).

- [PARTIAL] **5.3 Elevator control panel** (original `ElvDlogT.h/c`)
    - [DONE] Per-floor service toggle via finger tool.
    - [DONE] `ElevatorDialog` (TGUI child window + scrollable panel) with per-floor toggle buttons.
    - [DONE] `Game::toggleElevatorService()` helper encapsulates gameMap + cleanQueues + updateRoutes.
    - [DONE] Opened from InspectorDialog via "Floors..." button (visible only for elevators).
    - [DONE] Button labels refresh each frame so external changes (finger tool) stay in sync.
    - [DONE] Escape / Close button / title-bar X close the dialog.
    - [ ] WD/WE (weekday/weekend) service modes.
    - [ ] Express-to-top / express-to-bottom buttons.
    - [ ] Time-before-departing slider.


Phase 6: Long Tail / Deferred                                 [DEFERRED]
-----------------------------------------------------------------------

Low priority — revisit after Phases 2–5 land.

- [DEFERRED] **6.1 Outdoor objects** — antennas, ads, parks (original `OutObjT.h/c`).
- [DEFERRED] **6.2 Outside view toggle** (original `OutSideT.h/c`, `OutTV.h/c`).
- [DEFERRED] **6.3 Television / multimedia system** — TV placement, commercial revenue (`QTimeT.h/c`, `QCM.h/c`, `QTSelect.h/c`).
- [DEFERRED] **6.4 Santa / seasonal events** (original `SantaT.h/c`).
- [DEFERRED] **6.5 Church weddings** (original `ChurchT.h/c`).
- [DEFERRED] **6.6 Future simulation / what-if mode** (original `FutureT.h/c`).
- [DEFERRED] **6.7 AquaZone** (original `AquaT.h/c`).


-------------------------------------------------------------------------------
Item-specific notes (carried over from earlier TODO)
-------------------------------------------------------------------------------

Item::FastFood
--------------
- [DONE] Customer arrival loop iterates customers each frame.
- [ ] Stop admitting customers after 19:00 so venues empty toward end of day.
- [ ] Persist customers to disk — F2 save/reload currently leaves open but empty FastFoods.

Item::Cinema
------------
- [ ] Verify showtimes against original.
- [ ] Implement attendance-based income (replace TODO at `Cinema.cpp:144`).
- [ ] Confirm halfway-through-movie break.
- [ ] Verify underground construction support.

Item::Office
------------
- [DONE] Lunch scheduling logic exists.
- [DONE] Rent + deposit collection.
- [ ] Office workers should also arrive by car (currently lobby only).

Item::PartyHall
---------------
- [ ] Confirm party timing from original.
- [ ] Confirm income model (attendance-based? flat?).
- [ ] Make people arrive/leave.

Item::Metro
-----------
- [ ] Trains arrive/leave at regular absolute-time intervals.
- [ ] Each train brings visitors for underground commercial and hauls away waiting passengers.
- [ ] Metro tracks as decoration.

Item::Elevator
--------------
- [DONE] Step-out animation for people leaving a car (step-in was already animated).

Item::Floor
-----------
- [DONE] Auto-create / extend floor item when constructing buildings or resizing elevators.
- [DONE] Floor item exposes width, eliminating per-floor iteration.
- [DONE] Floor renders behind other items; ceilings no longer rendered separately.
- [DONE] Floor renders in segments (full when empty, ceiling-only when built upon).


-------------------------------------------------------------------------------
PathFinding
-------------------------------------------------------------------------------

- [DONE] Game map of transport access points + floor nodes; buildings accessible from a generic floor node.
- [DONE] `findRoute()` rewritten as A-Star over the transport graph.
- [ ] Balance path costs for each transport to better match the original.
- [ ] Check for sky-lobby availability before allowing elevator transfer.
- [ ] Handle route changes (accessible ↔ inaccessible) for people already travelling — stop them mid-journey to avoid crashes and transit them out of the flow cleanly.


-------------------------------------------------------------------------------
Reference Index (codemap.md → implementation)
-------------------------------------------------------------------------------

| Module                            | Doc Source            | Impl File                       | Status   |
|-----------------------------------|-----------------------|---------------------------------|----------|
| Animation / CLUT                  | codemap.md:72-107     | source/Sky.*                    | PARTIAL  |
| People animation                  | codemap.md:108-141    | —                               | MISSING  |
| Bitmap loading                    | codemap.md:207-237    | source/BitmapManager.*          | PORTED   |
| Blocks                            | codemap.md:238-272    | source/Item/Floor.*             | REIMPL   |
| Church                            | codemap.md:274-312    | —                               | MISSING  |
| Clouds/Sky                        | codemap.md:313-343    | source/Sky.*                    | PORTED   |
| Command buttons                   | codemap.md:344-378    | source/ToolboxWindow.*          | REIMPL   |
| Count/Statistics                  | codemap.md:414-447    | —                               | MISSING  |
| Draw utilities                    | codemap.md:518-550    | source/Sprite.*                 | REIMPL   |
| Dust/Cleaning                     | codemap.md:586-618    | —                               | MISSING  |
| Emergency                         | codemap.md:749-773    | —                               | MISSING  |
| End/Cleanup                       | codemap.md:775-801    | source/Game::clearWorld         | PORTED   |
| Event/Fire/Terror                 | codemap.md:833-862    | —                               | MISSING  |
| File operations                   | codemap.md:864-893    | source/Game encode/decode XML   | PARTIAL  |
| Find dialogs                      | codemap.md:895-956    | —                               | MISSING  |
| Fire                              | codemap.md:958-990    | —                               | MISSING  |
| Guard/Security                    | codemap.md:1020-1049  | source/Item/Security.*          | STUB     |
| Hotel                             | codemap.md:1051-1098  | source/Item/Hotel.*             | PORTED   |
| Info destination                  | codemap.md:1124-1146  | —                               | MISSING  |
| Info/HUD                          | codemap.md:1148-1193  | source/TimeWindow.*             | PARTIAL  |
| Info messages                     | codemap.md:1100-1122  | —                               | MISSING  |
| Initialize                        | codemap.md:1195-1223  | source/Application.*            | PORTED   |
| Judge/Evaluation                  | codemap.md:1225-1251  | source/JudgeSystem.*            | PORTED   |
| Kinsoku/Placement                 | codemap.md:1253-1277  | source/JudgeSystem.cpp (noise)  | PARTIAL  |
| Level/Progression                 | codemap.md:1279-1343  | source/Game.cpp ratingMayIncrease | STUB   |
| Maintenance                       | codemap.md:1344-1377  | —                               | MISSING  |
| Main window                       | codemap.md:1378-1408  | source/Application.*            | PORTED   |
| Map/minimap                       | codemap.md:1410-1460  | source/MapWindow.*              | PORTED   |
| Medical                           | codemap.md:1462-1489  | source/Item/MedicalCenter.*     | STUB     |
| Money                             | codemap.md:1491-1517  | source/Money.h                  | PORTED   |
| Movie/Cinema                      | codemap.md:1519-1546  | source/Item/Cinema.*            | PARTIAL  |
| Name system                       | codemap.md:1548-1575  | source/NameManager.*            | PORTED   |
| Outdoor objects                   | codemap.md:1577-1603  | —                               | MISSING  |
| Outside view                      | codemap.md:1605-1641  | —                               | MISSING  |
| OutTV                             | codemap.md:1643-1676  | —                               | MISSING  |
| Parameters                        | codemap.md:1678-1710  | —                               | MISSING  |
| Parking                           | codemap.md:1746-1776  | source/Item/Parking.*           | PARTIAL  |
| People (UniPeple)                 | codemap.md:2495-2522  | source/Person.*                 | PORTED   |
| Quick drawing                     | codemap.md:1864-1891  | (SFML-native)                   | REIMPL   |
| Restaurant                        | codemap.md:1893-1923  | source/Item/Restaurant.*        | PARTIAL  |
| Route/Pathfind                    | codemap.md:1925-1955  | source/PathFinder/*             | GOOD     |
| Santa                             | codemap.md:1957-1985  | —                               | MISSING  |
| Side rendering                    | codemap.md:1987-2015  | —                               | MISSING  |
| Sound                             | codemap.md:2017-2049  | source/Sound.*                  | PORTED   |
| Status overlays                   | codemap.md:2051-2079  | —                               | MISSING  |
| Stress                            | codemap.md:2111-2137  | source/Person.*                 | PORTED   |
| Subway                            | codemap.md:2168-2195  | source/Item/Metro.*             | STUB     |
| Tenants                           | codemap.md:2221-2275  | (spread)                        | PARTIAL  |
| Thief                             | codemap.md:2277-2305  | —                               | MISSING  |
| Time                              | codemap.md:2307-2334  | source/Time.*                   | PORTED   |
| Toolbox                           | codemap.md:2336-2366  | source/ToolboxWindow.*          | PORTED   |
| VIP                               | codemap.md:2653-2690  | —                               | MISSING  |
