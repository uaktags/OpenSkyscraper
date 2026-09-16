OpenSky TODO
============

Status legend: `[DONE]` complete · `[PARTIAL]` started but incomplete · `[ ]` not started · `[STUB]` placeholder exists · `[DEFERRED]` intentionally postponed

Branch: `feature/simtower-gap-impl` — see `.omo/plans/simtower-gap-implementation.md` for full design notes.

Sources of truth for status were verified against `source/` (not just the plan doc) on 2026-06-18.


-------------------------------------------------------------------------------
General / Cross-cutting
-------------------------------------------------------------------------------

- [DONE] `mapWindow` is now its own class `MapWindow` (Phase 4.2).
- [DONE] Cache the result of the KWAJ decompression so the game doesn't decompress `SIMTOWER.EX_` every launch.

### [DONE] Pausing doesn't affect elevators
When pausing the game, elevators keep moving as if the game was unpaused. They react to speedup by moving faster — investigate why they don't react to the speeddown.

### [DONE] Game Speed
Game speed seems rather fast compared to elevator movements. Cinema customers hardly arrive at the theatre in time. Slow down time, or speed up elevators?

**[DONE] Speed rubberbanding fixed** — the old `speed_animated` smoothing `(speed*dt + sa*0.1)/(dt+0.1)` coupled convergence to frame delta, and a binary `kNightSpeedMultiplier=2.0` caused a jarring 2× speed jump at 19:00 / 07:00. Fixed: smoothing is now frame-rate-independent exponential `(1-exp(-dt/tau))`; the night multiplier was removed entirely (the non-linear `absoluteToHour` mapping already makes night hours pass faster without a hard binary switch). Tests updated accordingly.

### [DONE] Different Access Floors for Items
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
    - [DONE] Verify guest actually pathfinds to a Restaurant for dinner (currently tagged `goingTo = "Restaurant"` but no route lookup). — Wired in `Hotel.cpp:281-296`: iterates `itemsByType["restaurant"]`, picks the lowest-score reachable route via `game->findRoute(this, *i)`, sets the guest's journey; if none reachable they leave the tower.
    - [ ] Validate sprite frame rects against `single` / `double` / `suite` sheet.

- [PARTIAL] **2.2 Parking system** — Basic item + arrival hooks landed; gate integration blocked on missing road item.
    - [DONE] `source/Item/Parking.{h,cpp}` with `totalSpaces()` (derived from width × 2), `usedSpaces()`, `hasSpace()` / `assignSpace()` / `freeSpace()`.
    - [DONE] Registered in `source/Item/Factory.cpp` as prototype `"parking"`; added `ICON_PARKING` to `Factory.h`.
    - [DONE] Tower-wide coverage metric in `JudgeSystem::computeParkingCoverage()` (1 space / 4 offices + 1 / hotel room); Office and Hotel scoring penalised when coverage < 50% / 100%.
    - [DONE] `used` counter persisted in XML.
    - [PARTIAL] **Gate integration** (`SetupAllGate()` equivalent) — `Parking::setupGate()` hook exists and is a no-op; blocked on road/gate item addition.
    - [DONE] **Cars visually appear/disappear** on tiles — `Parking::render()` overrides to overlay one `sf::RectangleShape` per `used` slot.
    - [DONE] Hook `assignSpace()` / `freeSpace()` into office-worker / hotel-guest arrival.
        - Known limitation: per-guest `parkingUsed` pointer is **not** persisted across save/reload.

    - [DONE] **2.5 Multi-story Lobby Staircases (Spiral Stairs)**
    - **Goal**: Implement proper spiral staircase rendering and sizing when placing stairs in a 2-story or 3-story lobby, and fix pathfinding for multi-floor stairs.
    - **Primary Source of Bitmaps (SIMTOWER.EXE)**: The two `0xFF02`-type (per-cell arrangement) resources in `SIMTOWER.EXE` are the canonical animated source — neither is consumed by `SimTowerLoader` today.
        - **`0x8fe9`** (dumped as `data/bitmaps/unhandled/8fe9.bmp`, 4416×36): **2-story spiral staircase**. Laid out as 69 slots of 64×36 (one floor of one animation frame each):
            - slots 0–10   – toolbox thumbnails
            - slots 11–21  – **2-story stair BOTTOM floor, 11 animation frames**
            - slots 22–32  – **2-story stair TOP floor, 11 animation frames** (stack on top of slots 11–21 to form a 64×72 frame)
            - slots 33–44  – people on 2-story stair (12 sparse frames)
            - slots 45–68  – likely 4-story variant or alternate facings (unconfirmed)
        - **`0x8fea`** (dumped as `data/bitmaps/unhandled/8fea.bmp`, 5888×36): **3-story spiral staircase**. Laid out as 92 slots of 64×36:
            - slots 0–10   – toolbox thumbnails
            - slots 11–21  – **3-story stair TOP floor, 11 animation frames**
            - slots 22–32  – **3-story stair MIDDLE floor, 11 animation frames**
            - slots 33–43  – **3-story stair BOTTOM floor, 11 animation frames** (stack 11+22+33 for a 64×108 frame, top-down)
            - slots 44–55  – people on 3-story stair (12 sparse frames)
            - slots 56–91  – likely 4-story variant (12 frames × 3 visible floors, unconfirmed)
        - **Frame composition** (per-strip slot order, verified visually against the original game):
            - 2-story: `frame N = slot[22+N]` (top) over `slot[11+N]` (bottom) — slot index decreases going up.
            - 3-story: `frame N = slot[11+N]` (top) + `slot[22+N]` (middle) + `slot[33+N]` (bottom) — slot index increases going up.
    - **Fallback Source (YootTower `Stair.t2p`)**: Win32 PE DLL `data/Plugins/Stair.t2p` should be used only when SIMTOWER resources are unavailable. Its bitmaps are **static** (no animation), per the embedded `OBJMAP 0x3e8`:
        - `RT_BITMAP 0x3e8` (1000) — `stair_t2p_1000.bmp`, 64×288. **Contains 3 static views, NOT 11 animation frames** (the earlier description here was wrong):
            - Span 2 (2-story): Rect (0, 0, 64, 60)
            - Span 3 (3-story): Rect (0, 60, 64, 156)  → 64×96
            - Span 4 (4-story): Rect (0, 156, 64, 288) → 64×132
        - `RT_BITMAP 0xc8` (200) — `stair_t2p_200.bmp`, 32×768. SHIL ("shadow") tiles — 8×24 people-walking frames for the spiral stairs.
        - `RT_BITMAP 0x64` (100) — `stair_t2p_100.bmp`, 26×130. Toolbox icon + 5 sub-frames.
    - **What's been built**:
        - `SimTowerLoader::loadSpiralStairs()` slices the SIMTOWER strips into `simtower/stairs/spiral_2` (704×72, 11 frames) and `simtower/stairs/spiral_3` (704×108, 11 frames).
        - `Stairs::init()` / `configureForLobby()` detect a multi-story lobby at the stair's position via `game->itemsByFloor[position.y]` and select `spiral_2`/`spiral_3` with `frameCount=11`. `size.y` is set to `lobbyHeight + 1` (3 or 4) so the stairs span all lobby floors and connect directly to the floor above the lobby.
        - `Stairs::encodeXML/decodeXML` persists `spiral=2/3` with the correct `size.y` values (3 or 4) so save games restore the correct texture, size, and frame count.
        - `scratch/stair_analysis.py` re-extracts the spiral sheets from the dumped BMPs as a verifier. `scratch/extract_t2p_stairs.py` extracts the static YootTower views to PNGs that BitmapManager auto-loads as the t2p fallback path.
        - **`PathFinder/GameMap::addNode/removeNode`** — fixed a bug where the stair's upper transport node was hard-coded to `p.y + 1`. For 3-story stairs (`size.y = 3` originally, now `size.y = 4`) this left the top floor disconnected. Now uses `p.y + item->size.y - 1`, matching `Stairlike::connectsFloor()`.
        - **`Game::handleEvent`** — auto-extends multi-story lobbies horizontally when a wider item is placed on top. Extended manual/automatic lobby resizing to correctly call `extendFloor` and update the floor `interval` multisets on all lobby floors.
    - **Remaining limitations**:
        - Multi-story lobbies don't yet provide automatic internal stairs; people inside the lobby can only change floors via the spiral stair they placed inside (or an elevator). Original SimTower's sky lobbies had automatic interior stairs; adding them would require overriding `Lobby::isStairlike()` and wiring FloorNodes in `GameMap::addNode` — noted as a future enhancement.
        - No 4-story lobby or 4-story stair exists in SIMTOWER (confirmed by the YootTower `Stair.t2p` resource going up to Span 4 but no matching lobby variant in `SIMTOWER.EXE`).

- [PARTIAL] **2.3.1 Office: rent, evaluation, lunch** (`source/Item/Office.{h,cpp}`)
    - [DONE] Rent + deposit collection on Monday 05:00 (`Office.cpp:105`-`122`).
    - [DONE] `findLunchRoute()` + `kLunch` dispatch at 12:00 (`Office.cpp:180`-`183`).
    - [DONE] `isAttractive()` route-based gate for move-in.
    - [ ] Extend `isAttractive()` to consider amenity coverage (security, food reachability) not just route score.
    - [DONE] Salesman behaviour: leave for sales, return, no lunch. — `salesLeaveQueue`/`salesReturnQueue` dispatch in `Office.cpp:271-313`; salesmen are excluded from `lunchQueue` at `Office.cpp:429`.
    - [ ] Stress recovery when worker returns from successful lunch; stress gain when no food reachable.

- [DONE] **2.3.2 FastFood / Restaurant: schedules & pricing** (`source/Item/{FastFood,Restaurant}.{h,cpp}`) — verified DONE in source on 2026-06-18 (Wave 1.1 commit `d512208`); TODO previously lagged behind code.
    - [DONE] Person state transitions (`kLunch` / `kShopping` / `kReturning`).
    - [DONE] Weekday-dependent hours — FastFood opens 10:00 Mon–Sat (skips `day == 2`, the SimTower weekend); offices only populate on weekdays via the same `day != 2` filter used by `Office`/`Condo`.
    - [DONE] Customer count scaled by reachable office / hotel population (`FastFood.cpp:134`-`143` iterates Office items; `Restaurant.cpp:129`-`143` iterates Hotel items, filtered by `findRoute`).
    - [DONE] Pricing model: `game->transferFunds(population * pricePerMeal - dailyMaintenanceCost(), "retail_income", ...)` runs at close (FastFood 21:00, Restaurant 23:00).
    - [DONE] Restaurants serve hotel guests at dinner (iterate Hotels, dispatch at 17:00); FastFood serves office workers at lunch (iterate Offices, dispatch at 10:00).
    - [DONE] Stop admitting customers after 19:00 — `if (game->time.hour < 19.0)` guard in the arrival loop (`FastFood.cpp:174`, `Restaurant.cpp:173`); late arrivals are dropped.
    - [DONE] Persist customers to XML — both `arriving` and `eating` phases serialised with stress/eval/name/state (`FastFood.cpp:33`-`73`, `Restaurant.cpp:33`-`70`).

- [DONE] **2.3.3 Cinema: movie scheduling & revenue** (`source/Item/Cinema.cpp`)
    - [DONE] Person state transitions on arrival/departure.
    - [DONE] Two screenings/day (13:00 doors / 15:00 start / 17:00 close, and 19:00 / 21:00 / 23:00).
    - [DONE] Attendance-based income: `attendees * ticketPrice - screeningFee` using `people.size()` at close.
    - [DONE] Halfway-through-movie break (per original; verify). — `Cinema.cpp:151` fires `intermission = true` at 16:00 and 22:00 (1 h after each 2 h screening start at 15:00 / 21:00).
    - [DONE] Permit underground construction (verify original behaviour). — `Game.cpp:309-312` blocks cinema placement above ground (`toolPosition.y > 0 && toolPrototype->id == "cinema"`).

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
    - [DONE] Tune `kTrainDwellAbs` / `kTrainGapAbs` against the original SimTower feel. — Replaced the literal `0.02` / `0.08` values (which were ~3× too long — a full train cycle took most of the afternoon) with `Time::hourToAbsolute(10.0/60.0)` / `Time::hourToAbsolute(0.5)` for a ~10 min dwell and ~30 min gap (`Metro.cpp:17-18`).
    - [DONE] Target underground commercial specifically (currently any reachable venue). — `Metro::pickDestinationFor` at `Metro.cpp:129-149` filters by `item->position.y < 0` over `{fastfood, restaurant, cinema, partyhall}`.
    - [DONE] Metro tracks as a decoration (parallel to fire stairs / crane — see existing Decorations work). — `Decorations.cpp:16-19` (`track.SetImage` from `simtower/metro/tracks`); `updateTracks()` at `Decorations.cpp:101` wired from `Game.cpp:1074`.
    - [DONE] Enforce: only one Metro instance per game. — Construction flow already rejects a second Metro with "Only one Metro Station allowed" (`Game.cpp:424-427`); the `init()` defensive branch now logs a warning and keeps the existing station instead of asserting (`Metro.cpp:51-57`).
    - [DONE] Forbid building any item below the Metro. — `Game.cpp:299-302` rejects placement when `metroStation != NULL && toolPosition.y < metroStation->position.y`.

    - [DONE] Elevator id mismatch (latent bug surfaced by the overlay rewrite). — `CATEGORIES` in `ToolboxWindow.cpp` and `kItemLocks` in `LevelUp.cpp` used `standard_elevator` / `express_elevator` / `service_elevator` (underscores) but the prototype ids in `Item/Elevator/{Standard,Express,Service}.h` use `elevator-standard` / `elevator-express` / `elevator-service` (dashes). Effects: (1) press-and-hold on Standard Elevator did nothing — `isCategoryParent` always returned false; (2) Express/Service elevators were never locked despite the table claiming a 3★ minimum. Both strings now match the prototype ids.


Phase 3: Game Systems                                        [PARTIAL — 3.1/3.2/3.4 in progress, 3.3 pending]
-----------------------------------------------------------------------

- [PARTIAL] **3.1 Judge / Evaluation engine** — `source/JudgeSystem.{h,cpp}` landed.
    - [DONE] `JudgeSystem::evaluateAll(Game*)` called daily from `Game::settleDailyAccounting()`; runs after maintenance so scores reflect yesterday's state.
    - [DONE] Per-tenant scoring (`scoreOffice` / `scoreCondo` / `scoreHotel` / `scoreCommercial`) using route quality, occupant stress, amenity coverage (FastFood lunch, Restaurant dinner, Security).
    - [DONE] Scores cached on `Item::evaluation` (persisted in XML) and mirrored onto `Person::eval` for inspector / overlay use.
    - [DONE] `Office::isAttractive()` and `Condo::isAttractive()` consult evaluation (≥30 required, default 50 keeps fresh tenants attractive).
    - [DONE] `Counts` struct (offices / condos / hotels / hotelsDirty / foodOutlets / security / medical / population) approximates `CountT.h/c`; ready to feed star progression (3.2) and overlays (4.3).
    - [DONE] Tower-wide parking coverage metric feeds Office / Hotel scoring (see 2.2).
    - [DONE] **`JudgeAllHotel()` aggregate review** — `JudgeSystem::reviewHotels()` computes tower-wide hotel occupancy / cleanliness / average evaluation and surfaces a daily complaint when hotels are persistently underperforming (low avg eval, or dirty ratio > 50% on 3+ room towers). Also notes high-occupancy towers.
    - [DONE] **`ExpandoBadHotel()` underperformer tracking** — `JudgeSystem::reviewUnderperformers()` keeps a per-tenant "bad-day streak" map and only complains once a tenant has been below the critical-evaluation threshold for 3+ days (with reminders every 5 days). Original SimTower dynamically shrunk/grew hotels; we surface complaints instead since items have fixed footprints.
    - [DONE] **Per-type counters `CountPC` / `CountVC` / `CountIn`** — `Counts` extended with `populationCapacity`, `visitorCapacity`, `currentOccupants`, `hotelsOccupied`, and `hotelAvgEval`. (`CountDay` deferred until daily-arrival tracking lands.)
    - [ ] `CountDay` (total arrivals per day) — needs person-spawn bookkeeping; deferred.

- [PARTIAL] **3.2 Level / star-rating expansion** (`source/LevelUp.{h,cpp}`, `source/Game.cpp:ratingMayIncrease`)
    - [DONE] Star thresholds (1★ start; 2★ at 300 pop; 3★ at 1000 pop + Security; 4★ at 2000 pop + Medical; 5★ at 3000 pop + Metro).
    - [DONE] `ratingMayIncrease()` checks population **and** required facilities via `JudgeSystem::Counts`; loops to multi-promote if eligible.
    - [DONE] Construction handler rejects placement below `LevelUp::minRatingToBuild()` with a clear reason.
    - [DONE] `ToolboxWindow` greys out locked items with "Unlocks at N stars" tooltip; reloads on promotion.
    - [DONE] Promotion message via `TimeWindow.showMessage()`.
    - [DONE] **VIP visits** — `source/VipSystem.{h,cpp}` schedules random VIP visits once the tower hits 2★ / 150+ population (21–50 day gaps, 14 extra days after a bad review — tuned down from the original 3–8 days which was far too frequent). Each ~2h visit evaluates the latest judge pass and grants a cash bonus (full $50k for excellent, half for content) or a complaint. State persists across save/reload via `vipNextVisit` / `vipVisiting` attributes; `positiveReviews()` is intended to gate the Cathedral ending.
    - [DONE] **Level-up dialog window** — `source/LevelUpDialog.{h,cpp}` TGUI modal pops up on every promotion showing the new star count and the comma-separated list of freshly-unlocked prototypes (driven by `LevelUp::minRatingToBuild`). Closes on OK button, title-bar X, or Escape; auto-closed on `clearWorld()`.
    - [ ] **Cathedral "Tower of the Year"** ending — needs a Cathedral item (none exists yet); the VIP `positiveReviews` counter is plumbed for future gating.
    - [ ] Spawn an actual walking VIP sprite (currently the visit is abstract).

- [PARTIAL] **3.3 Fire / Emergency / Terror events** — `source/EventSystem.{h,cpp}` landed. Art (fire/large, fire/small, fire/chopper, fire/destroyed, alerts/fire, alerts/terrorist, alerts/chopper) was already loaded by SimTowerLoader but unused; now wired up.
    - [DONE] `EventSystem` scheduler — randomly triggers fire (70%) or terror (30%) based on time + 2★ rating. Events spaced 30–90 in-game days apart (first at 15 days).
    - [DONE] Fire: ignites at a random tenant, spreads floor-to-floor every ~15 game-min, applies stress + eval penalty to occupants. Click the blaze to dispatch helicopter ($10k), arrives after ~30 game-min and extinguishes.
    - [DONE] Terror: bomb threat with ~2 game-hour countdown. Click the blinking alert to defuse. Detonation damages nearby tenants ($80k + heavy stress).
    - [DONE] Rendering — fire animation (4-frame cycle), helicopter fly-in sweep, alert icons. Drawn on top of all items.
    - [DONE] Save/load — `nextEventTime` persisted; active events cleared on reload (intentional).
    - [ ] Thief: targets tenants, calls police, existing `Security` item becomes functional (currently STUB).
    - [ ] Emergency mode: reset all person states, evacuate.
    - [ ] Water spray effect (player extinguishing with fire extinguisher).

- [PARTIAL] **3.4 Weather / Environment (CLUT equivalent)** — `source/Lighting.{h,cpp}` landed.
    - [DONE] `Lighting` manager class — extends `GameObject`, refreshed once per frame from `Game::advance()` after `Sky::advance()`.
    - [DONE] Time-of-day tint applied to all items — `Item::render()` and `Floor::render()` multiply each sprite's color by `Lighting::compose()` (save/restore around the draw so the tint doesn't compound across frames).
    - [DONE] Construction overlay also composes with the global tint so building sites respect day/night.
    - [DONE] Rain reduces brightness + cool blue tint (fades in/out via smoothed `rainIntensity` driven by `Sky::rainyDay`, 07:00–17:00 window matches the rain sound).
    - [DONE] Sunrise/sunset orange-red tint (warm anchor at Sky state 1); night dark blue (Sky state 2); cloudy/rain gray-blue (Sky states 3–5).
    - [DONE] Tint math covered by `testLightingColorMath` in `tests/test_main.cpp` (white*white identity, halving, alpha preservation, black multiplicative zero, associativity).
    - [PARTIAL] **Lit-window sprite variants at night** — blocked on missing infrastructure. The codebase has no per-sprite "is window" metadata: `AbstractPrototype` (`source/Item/Prototype.h`) only carries `id/name/price/size/icon/variant`, and most items (Office, Condo, Hotel) pack walls+windows into a single sprite-sheet cell, so even a prototype-level flag wouldn't suffice. The closest existing concept is per-instance state (`Office::lit`, `Condo::LightingConditions` LIT/NIGHT/DARK) which selects between pre-baked sprite frames but is item-type-specific and not a global window classification. To finish this, pick ONE of: (a) split each item's texture into separate wall/window sub-sprites and add a `SpriteSet Item::windowSprites`; (b) ship per-item window-mask bitmaps and classify at load time; (c) add `AbstractPrototype::isWindow` plus a full sprite-sheet audit. Then add a Lighting override pass at Sky state 2 that replaces `compose()` on window sprites with a warm glow (e.g. `sf::Color(255, 220, 140)`).
    - [DONE] Wire the weather message back in — `Sky.cpp` now calls `game->timeWindow.showMessage(...)` at 05:00 alongside the `rainyDay` roll (`Sky.cpp:34`).
    - [DONE] Apply the tint to the `Elevator`-drawn sprites — `Elevator::render` (shaft + digits, with home-floor pinkish-red preserved by composing *after* the per-floor color selection), `Car::render` (car sprite via local-copy save/restore since `render` is const), and `Queue::render` (stress color composed after the red/pink selection) all route through `game->lighting.compose(...)`. The person-step-out sprite in `Car::render` is always `sf::Color::Black` so compose is a mathematical no-op and was deliberately skipped.


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
    - [DONE] Wire tenant complaint messages (stress > threshold) to `TimeWindow.showMessage()`. — `JudgeSystem.cpp:361-369` emits a daily "N tenants unhappy" message when `lastCounts.criticalTenants > 0`; hotel and per-tenant complaints also surface via `showMessage` at lines 406, 415, 422, 467.
    - [ ] Tenant messaging: route-based complaints ("can't reach lobby", "no lunch nearby").

- [PARTIAL] **4.2 Minimap** (`source/MapWindow.{h,cpp}`)
    - [DONE] TGUI child window + SFML canvas overview, anchored to right edge.
    - [DONE] Items drawn as filled rectangles; elevators as dark bars.
    - [DONE] StatusMode-aware tinting (matches main viewport Eval/Hotel overlays).
    - [DONE] Toggle with `M` key; closes on Escape; hidden by default.
    - [DONE] Click-to-jump via `Game::centerViewportOnTile()`.
    - [DONE] Throttled refresh (~1 s of game time) plus reload on GUI rebuild / world clear.
    - [DONE] Toolbar button to toggle (currently keyboard only). — Added "Map" button in the new view-toggle row of `ToolboxWindow::reload` (Step 4), calls `mapWindow.setVisible(!mapWindow.isVisible())` mirroring the M keybind.
    - [ ] Dedicated Pric palette (deferred with the rent-pricing model).
    - [ ] Separate grey-body + sky-background rendering mode per the original (`MapT.h/c`).

- [DONE] **4.3 Status overlays (Eval / Pric / Hotel)** — `Game::StatusMode` enum + `O` key cycle.
    - [DONE] `enum StatusMode { kNormal, kEval, kPric, kHotel }` on Game; cycle via `O` key with mode-name message.
    - [DONE] Eval overlay: blue/yellow/red tint on each tenant item based on `Item::evaluation`.
    - [DONE] Hotel overlay: red on dirty rooms, yellow on occupied, green on clean.
    - [DONE] Per-item culling so offscreen overlays are skipped.
    - [DONE] **Pric overlay** — draws a yellow overlay on unoccupied condos and offices in the For Sale / For Rent state, turning off once purchased/occupied.
    - [DONE] Toolbar button to cycle modes (currently keyboard only). — Added "View" button in the new view-toggle row of `ToolboxWindow::reload` (Step 4), calls `game->cycleStatusMode()` mirroring the O keybind; the active mode is announced via `TimeWindow.showMessage` as before.
    - [DONE] Minimap integration (Phase 4.2) so the overlays also render on the minimap. — `MapWindow.cpp:113-131` honours `Game::kEval` (blue/yellow/red by `item->evaluation`), `Game::kHotel` (red/yellow/green by `roomState`), and `Game::kPric` (yellow for unoccupied condos/offices).

- [DONE] **4.4 Construction animation** (also in original `TODO.md`)
    - [DONE] `bool underConstruction` + `double constructionEndTime` on `Item`; persisted in XML.
    - [DONE] `Item::render()` draws a hatched khaki overlay rectangle while building (real sprites hidden).
    - [DONE] `Game::advance()` skips the item's advance, maintenance, and construction timer check; clears the flag with a "normal" construction sound on completion.
    - [DONE] `Factory::make()` sets the flag on fresh placement; `decodeXML()` overrides with saved state.
    - [DONE] `InspectorDialog` shows "under construction" with remaining-time estimate.
    - [DONE] `Duration` scales with item width (`constructionDuration()` virtual, overridable per subclass).
    - [ ] Swap rectangle overlay for actual `construction/solid` and `construction/grid` bitmaps.
    - [ ] Per-prototype duration tuning (currently 2h + 0.2h/tile width).

- [DONE] **4.5 Toolbox categories (click-and-hold)**
    - [DONE] Press and hold a category parent tool (e.g. Lobby, Standard Elevator, Hotel Single, Condo) and its children pop up as an overlay over the neighbouring grid slots — `ToolboxWindow::showOverlayFor()` builds the overlay on top of `window` so the grid layout never shifts.
    - [DONE] While held, drag to a child and release to select it; release over the parent or off-toolbox to select the parent. Release is detected by polling `sf::Mouse::isButtonPressed` in `ToolboxWindow::update()` (called every frame from `Game::advance`) since TGUI's mouse capture is unreliable once overlay widgets stack over the pressed parent.
    - [DONE] Lobby reveals Floor.
    - [DONE] Stairs reveals Escalator (own category; pairs the basic vertical transport with its upgrade, mirroring SimTower's primary/secondary relationship).
    - [DONE] Standard Elevator reveals Express Elevator and Service Elevator.
    - [DONE] Hotel Single reveals Hotel Double and Hotel Suite.
    - [DONE] Condo reveals YootCondo.
    - [DONE] **Hide locked tools entirely** — prototypes whose `LevelUp::minRatingToBuild()` exceeds the current tower rating are filtered out of `visiblePrototypes` before layout (replaces the previous greyed-out disabled-button + tooltip approach; the level-up dialog is now the sole discovery mechanism).
    - [DONE] **Enlarged toolbox buttons** — item cells 32→36 px, tool buttons 21→24 px, window widened from 106→118 px to match; `makeButton` now takes optional `cellW`/`cellH` so a 32 px sheet cell can be rendered at 36 px. `updateTool()` highlights the parent button when a category child is the active tool.
    - [DONE] **Slot substitution on selection** — when a child is picked via the press-and-hold overlay, the parent slot's icon is replaced with the child's (e.g. selecting Floor replaces the Lobby icon with Floor's). The slot stays registered as the parent in `toolButtons` but `buttonStates[btn]` is swapped to the child's textures via `rebuildSlotTexture()`. Short-click then selects the currently-displayed tool. Substitutions are preserved across `reload()` (level-up) if the child is still buildable; otherwise the slot reverts to its parent.


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
    - [DONE] `population` correctly updates when tenants leave mid-day (verify path for stress-flee). — Verified: `Item::population` is a tenant-headcount field, not a current-presence count (`Office::population = workers.size()` at `Office.cpp:435`; a stress-fleeing worker stays in the workers set and returns next day, so headcount is unchanged). Same semantics as the Condo weekly vacate path which clears `population = 0` only when the tenant leaves permanently.

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
- [DONE] Stop admitting customers after 19:00 — `if (game->time.hour < 19.0)` guard in `FastFood.cpp:174`; late arrivals are dropped (Wave 1.1, commit `d512208`).
- [DONE] Persist customers to disk — `encodeXML`/`decodeXML` cover both `arriving` and `eating` phases with full person state (`FastFood.cpp:33`-`108`, Wave 1.1).

Item::Cinema
------------
- [DONE] Verify showtimes against original (two screenings/day at 13:00-17:00 and 19:00-23:00; `Cinema.cpp`).
- [DONE] Implement attendance-based income (`attendees * ticketPrice - screeningFee`, replaced the TODO at `Cinema.cpp:144`; commit `d2e9cbc` Wave 1.3).
- [DONE] Confirm halfway-through-movie break.
- [DONE] Verify underground construction support.

Item::Office
------------
- [DONE] Lunch scheduling logic exists.
- [DONE] Rent + deposit collection.
- [DONE] Office workers arrive by car — claim a reachable Parking slot on morning arrival / sales return, free it on departure / sales leave / stress-flee (Phase 2.2).

Item::PartyHall
---------------
- [DONE] Confirm party timing from original. — Two sessions/day: 13:00–17:00 and 19:00–23:00 (`PartyHall.cpp:55-72`).
- [DONE] Confirm income model (attendance-based? flat?). — `attendees * kVisitorFee - kEventCost` at `PartyHall.cpp:85-86` (attendance-based).
- [DONE] Make people arrive/leave. — `kVisitorsPerEvent` Visitors spawned at open (`PartyHall.cpp:67-72`); cleared at next open or destruction (`PartyHall.cpp:64, 91-106`).

Item::Metro
-----------
- [DONE] Trains arrive/leave at regular absolute-time intervals. — `kTrainDwellAbs` / `kTrainGapAbs` constants in `Metro.cpp:17-18` drive `nextTrainTime` scheduling.
- [DONE] Each train brings visitors for underground commercial and hauls away waiting passengers. — `spawnVisitors()` (2-5 visitors per arrival) and `boardReturnedVisitors()` in `Metro.cpp`; fare revenue via `metro_fare`.
- [DONE] Metro tracks as decoration. — See Phase 2.4 entry above (`Decorations.cpp`).

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
| Animation / CLUT                  | codemap.md:72-107     | source/Sky.*, source/Lighting.* | PARTIAL  |
| People animation                  | codemap.md:108-141    | source/Item/Item.cpp (AnimPeple)| PARTIAL  |
| Bitmap loading                    | codemap.md:207-237    | source/BitmapManager.*          | PORTED   |
| Blocks                            | codemap.md:238-272    | source/Item/Floor.*             | REIMPL   |
| Church                            | codemap.md:274-312    | source/Item/Cathedral.* + source/EventSystem.* | PARTIAL  |
| Clouds/Sky                        | codemap.md:313-343    | source/Sky.*                    | PORTED   |
| Command buttons                   | codemap.md:344-378    | source/ToolboxWindow.*          | REIMPL   |
| Count/Statistics                  | codemap.md:414-447    | source/JudgeSystem.*            | PORTED   |
| Draw utilities                    | codemap.md:518-550    | source/Sprite.*                 | REIMPL   |
| Dust/Cleaning                     | codemap.md:586-618    | —                               | MISSING  |
| Emergency                         | codemap.md:749-773    | source/EventSystem.*            | PARTIAL  |
| End/Cleanup                       | codemap.md:775-801    | source/Game::clearWorld         | PORTED   |
| Event/Fire/Terror                 | codemap.md:833-862    | source/EventSystem.*            | PARTIAL  |
| File operations                   | codemap.md:864-893    | source/Game encode/decode XML   | PARTIAL  |
| Find dialogs                      | codemap.md:895-956    | —                               | MISSING  |
| Fire                              | codemap.md:958-990    | source/EventSystem.*            | PARTIAL  |
| Guard/Security                    | codemap.md:1020-1049  | source/Item/Security.* + source/EventSystem.* | PARTIAL  |
| Hotel                             | codemap.md:1051-1098  | source/Item/Hotel.*             | PORTED   |
| Info destination                  | codemap.md:1124-1146  | —                               | MISSING  |
| Info/HUD                          | codemap.md:1148-1193  | source/TimeWindow.*             | PARTIAL  |
| Info messages                     | codemap.md:1100-1122  | —                               | MISSING  |
| Initialize                        | codemap.md:1195-1223  | source/Application.*            | PORTED   |
| Judge/Evaluation                  | codemap.md:1225-1251  | source/JudgeSystem.*            | PORTED   |
| Kinsoku/Placement                 | codemap.md:1253-1277  | source/JudgeSystem.cpp (noise)  | PARTIAL  |
| Level/Progression                 | codemap.md:1279-1343  | source/Game.cpp ratingMayIncrease, source/VipSystem.*, source/LevelUpDialog.* | PARTIAL  |
| Maintenance                       | codemap.md:1344-1377  | source/Game.cpp (quarterly settlement) | PARTIAL  |
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
| Parking                           | codemap.md:1746-1776  | source/Item/Parking.*           | GOOD    |
| People (UniPeple)                 | codemap.md:2495-2522  | source/Person.*                 | PORTED   |
| Quick drawing                     | codemap.md:1864-1891  | (SFML-native)                   | REIMPL   |
| Restaurant                        | codemap.md:1893-1923  | source/Item/Restaurant.*        | PARTIAL  |
| Route/Pathfind                    | codemap.md:1925-1955  | source/PathFinder/*             | GOOD     |
| Santa                             | codemap.md:1957-1985  | source/EventSystem.*              | PARTIAL  |
| Side rendering                    | codemap.md:1987-2015  | —                               | MISSING  |
| Sound                             | codemap.md:2017-2049  | source/Sound.*                  | PORTED   |
| Status overlays                   | codemap.md:2051-2079  | —                               | MISSING  |
| Stress                            | codemap.md:2111-2137  | source/Person.*                 | PORTED   |
| Subway                            | codemap.md:2168-2195  | source/Item/Metro.*              | PARTIAL  |
| Tenants                           | codemap.md:2221-2275  | (spread)                        | PARTIAL  |
| Thief                             | codemap.md:2277-2305  | —                               | RETIRED  |
| Time                              | codemap.md:2307-2334  | source/Time.*                   | PORTED   |
| Toolbox                           | codemap.md:2336-2366  | source/ToolboxWindow.*          | PORTED   |
| VIP                               | codemap.md:2653-2690  | source/VipSystem.*              | PARTIAL  |

-------------------------------------------------------------------------------
Ported from OpenSkyWeb (2026-08-28)
-------------------------------------------------------------------------------

Improvements brought back from the 1:1 JS conversion at ~/Projects/OpenSkyWeb
(each verified against that codebase before porting):

- [DONE] **Pathfinder: per-segment stairlike budget** — `MapSearchNode` now
  tracks `stairsSinceTransport` / `escalatorsSinceTransport` which refill at
  elevator/lobby contact instead of accumulating over the whole journey
  (web `pathfinder.js` PORT NOTE 1). Fixes tower tops becoming unreachable
  when stairs were also used below the elevator serving them.
- [DONE] **Pathfinder: stamina-aware A* state equivalence** — `IsSameState`
  includes the since-counters + `numElevators` so an exhausted-budget path no
  longer prunes a valid elevator route (web commit `9918897`).
- [DONE] **Authentic SIMTOWER.EXE catalog pricing** — lobby $5000, condo
  $80000, hotel $20000/$50000/$100000, medical/recycling $500000, party hall
  $100000, parking $3000 (web commit `eca25e7`). Elevators now price the
  shaft authentically (standard $200000, express $400000, service $100000)
  plus a per-car `carCost` ($80000/$150000/$50000) charged when adding cars
  (replaces the hardcoded $80000).
- [DONE] **Authentic star progression** — advancement table now 300 / 1000 +
  Security / 5000 + Medical & VIP / 10000 + Metro; toolbox locks moved to the
  authentic mapping (service elevator/hotel single/security at 2★,
  escalator/express/restaurant/cinema/party hall/hotel double & suite/
  medical/recycling/parking at 3★, metro at 4★) (web commit `46372a2`).
- [DONE] **Star award gift message** on promotion ("Your tower has been given
  a Two-Star Award!", EXE strings 0x79636-0x79c36; web ISSUE-034).
- [DONE] **Autosave** — daily snapshot throttled to 1/60s real time, written
  to `<save>.autosave.tower` with an "Autosaved." toast (web ISSUE-030).
- [DONE] **Floor-slab self-heal on load** — `Game::repairFloorCoverage()`
  mints free stub slabs under haulers/rooms missing coverage, repairing towers
  saved with the web's old elevator-drag off-by-one (web ISSUE-041).
- [DONE] **Save integrity hash** — FNV-1a over the serialized XML (dev salt,
  web-compatible) stored as the first `hash` attribute on `<tower>`;
  `Application::loadGameFromFile` refuses mismatched files, legacy saves load
  unchanged (web commit `7699b46`).
- [DONE] **Tenant name variants** — restaurants (English Pub, French
  Restaurant, Chinese Restaurant, Sushi Bar) and fast food (Japanese Stall,
  Chinese Cafe, Hamburger Stand, Ice Cream, Coffee Shop) get authentic
  cosmetic names from the existing sprite variant, persisted via `variant`
  (web ISSUE-035).

Verified: `OpenSkyTests` all green (run from `build/`); full game builds.
Not ported: web's multi-story-lobby auto-connections (dead code there —
`Lobby.canHaulPeople()` is false, so `gamemap.addNode` returns early), touch
UI/rendering/asset work, SECOM/thief + ransom-choice + buried-treasure +
parking-ramp + retail/housekeeping/restroom/cathedral items (candidates for a
follow-up pass).

### Second batch — gameplay cadence & events (2026-08-28, later)

- [DONE] **CRITICAL fix: event cadence was undefined behaviour** —
  `scheduleNextEvent()` passed *days* through `Time::hourToAbsolute()`, which
  has no branch above 24h (uninitialized result). Events now add gap days
  directly to `time.absolute` (web ISSUE-036 clamp-bug fix). This is the
  "fires happen way too often/weirdly" bug.
- [DONE] **Deterministic terrorist threat** (web ISSUE-036) — replaces the
  random 30% terror roll. Once per game: weekend day of Q4 in year 5 at
  10:00; detonation fixed at 15:00 the same day. If another event is active
  when the trigger hits, it fires when that ends (`pendingTerror`). Both
  flags persist in the save (`eventTerrorFired`/`eventPendingTerror`).
- [DONE] **Burglar events** (web ISSUE-011/036) — random pool is now
  fire 50% / burglar 50%. Security office within ±15 floors (`Security::
  isCoveringFloor`) catches the burglar (+$10,000); otherwise −$50,000,
  −20 evaluation and +40 stress for everyone on that floor.
- [DONE] **Buried treasure** (web ISSUE-034) — 8% daily roll while anything
  is under construction: instant $20k–$100k windfall with a blinking
  `alerts/treasure` icon at the site for 12 game-hours.
- [DONE] **VIP visits are promotion-gated, not random** (web ISSUE-036) —
  the VIP arrives when the next star requires a VIP rating and every other
  requirement is already met. Good review completes the promotion gate;
  bad review starts a 14-day retry cooldown. Scoring/rewards unchanged
  ($50k excellent / $25k content). Old saves transition cleanly (one pending
  visit then the new model takes over).
- Not ported (still follow-ups): SECOM center & its bomb-scan/fire-sense
  coupling, terrorist ransom pay-or-find dialog, fire-crew hire dialog,
  security quench/burnout flow, thief sprite system, cathedral ending.

### Third batch — remaining web features (2026-08-29)

- [DONE] **SECOM Center** (web ISSUE-031) — second security item ($100,000,
  16x1, one per tower, "Only one SECOM allowed" EXE guard). Registered as a
  `Security`-class prototype (`makeSecomPrototype`) sharing the security
  sprite; unlocks at 2 stars. Event coupling: fires are reported as
  "SECOM has sensed a fire..."; bomb threats get the auto-scan (+0.5h
  "Secom System Now Scanning....") and auto-defusal (+1h, EXE success text);
  SECOM substitutes for Security coverage in the burglar-arrest roll.
- [DONE] **Fire-crew choice dialog** (web ISSUE-036 flow, EXE fire dialog) —
  fires now open a modal "FIRE!" choice: Call Fire Crew ($10,000) vs Let it
  burn. Declining hands response to Security: coverage within ±15 floors
  quenches the blaze after ~1 game-hour; otherwise it burns out after ~12.
  Note: the web's `chooseFireResponse`/`chooseTerrorResponse` UI hooks are
  actually never wired in its main.js — the C++ implementation is the first
  real wiring of this flow. The dialog pauses the sim until answered.
- [DONE] **Terrorist ransom choice** (web ISSUE-033, EXE dialog 0x78c00) —
  bomb threats open "Blackmail from Terrorists!" — Pay Them ($10k-$90k) or
  Find the Bomb. Paying ends the event peacefully; finding keeps click-to-
  defuse with a wider radius when Security covers the bomb floor (6->9).
  Detonation with SECOM present shows the authentic SECOM failure text.
- [DONE] **Authentic fire outcome text** — end-of-fire message now depends
  on emergency stairs on the burning floor (EXE dialog 0x78800).
- [DONE] **Cathedral + TOWER rating** (web ISSUE-010) — new 48x4, $3M item
  unlockable at 5 stars (uses the party-hall art, matching the web's sprite
  reuse). Construction completion rings `simtower/bells`, grants the TOWER
  rating (internal 5) and shows the "Tower of the Year" award message; the
  ceremony flag persists so reloads don't replay it. Advancement table gains
  the authentic 6th tier (15,000 population + facilities -> TOWER).
- Game gains `askEventChoice()` (modal two-button dialog, pauses the sim,
  dropped safely on clearWorld).

Verified: full build green, `OpenSkyTests` all green, smoke launch OK.
Still open (no web counterpart worth porting / deferred): thief sprite
walking animation, wedding processions, fireworks particle effects.

### Fourth batch — occupant animation pacing (2026-08-29)

- [DONE] **Stairs/escalators no longer play full-tilt loops** — Stairlike
  animation now tracks the walking person's actual transition progress and
  direction (straight stairs: frames 1-6 ascending, 7-13 descending;
  escalator/spiral: progress-mapped through the moving frames) instead of a
  free-running loop that always strobed at full speed (web stairlike.js,
  commit 77a9feb).
- [DONE] **Occupant overlays suppressed where the art already has people** —
  elevators/stairs/escalators (walkers are pre-rendered in their animation
  frames), cinema/party hall (pre-rendered crowds), fast food/cafes
  (pre-rendered interior), and condos at night (dedicated night frames)
  (web commits 4145df7, 77a9feb, e144044).
- [DONE] **Item::renderPeople port** (web item.js renderPeople) —
  multi-activity AnimPeple sheets (office, hotel, restaurant, guard,
  housekeeper) now hold each activity for ~4-8 seconds with a ~1-2 Hz
  micro-animation toggle instead of rapid costume strobing; condo residents
  map to persona frames by Person type (men/kids/parents/women activity
  pairs); hotels only show awake guests in occupied rooms; restaurants show
  1-2 waitstaff (2 at >=10 diners) instead of every patron; offices show a
  few workers while lit but not yet populated; people clamp inside room
  margins with slight per-cycle positional wander.
- [DONE] Added the missing `animpeple/guard` bitmap mapping (0x8469) and
  public accessors (`Condo/YootCondo::isNightLighting`, `Office::isLit` /
  `getWorkerPeople`).

### Fifth batch — toolbox icons, elevator load frames, drag-to-build (2026-08-29)

- [DONE] **SECOM toolbox icon** — the authentic SIMTOWER items strip (resource
  0x812c, dumped and inspected cell by cell) has a dedicated cell labeled
  "SECOM" at index 25; SECOM now uses it instead of borrowing the Security
  guard icon (cell 21), which made it read as a duplicate Security entry.
- [DONE] **Parking toolbox icon fix** — Parking pointed at cell 20 (the
  church dome); it now uses the "P" sign cell (17).
- [DONE] **Elevator car load frames graded across capacity** — the car sprite
  has 5 baked load frames but the old thresholds (0 / <=1 / <=3 / rest /
  exactly-full) meant cars jumped from empty to the 4+-person frame. Frames
  now map to quarters of maxCarCapacity (21): 1-5 light, 6-10 half, 11-15
  busy, 16+ full — genuine empty/some/more/full states.
- [DONE] **ISSUE-039 drag-to-place batching (RTS-style)** — ported from
  OpenSkyWeb: hold an item tool and drag to lay out a whole row or column
  (Shift = full rows x columns grid). Ghost rectangles preview each cell
  (white = buildable & affordable, red = blocked); nothing builds or charges
  until mouse release, then every valid cell is constructed (funds stop the
  batch when exhausted), a single updateRoutes() runs, and Esc cancels.
  Construction was extracted from Game::handleEvent into
  `Game::constructToolAt(pos)` with a side-effect-free preview mirror
  (`Game::validateBatchCell`). Elevators, lobbies, floors, stairs,
  escalators, metro and the parking ramp keep instant placement.
- [CORRECTION 2026-08-29] SECOM unlock moved to **3 stars** (was 2) — the web's
  2★ default had no authentic evidence, and it plays as the premium tier above
  the manual Security Office (Tim's call).
- [DONE 2026-08-29] **Vertical drag-builds into unbuilt space** — batch drag
  preview simulates the floor slabs the batch itself lays (cells build
  bottom-to-top, each item's construction creates the slab the next cell
  stands on), so dragging e.g. an office upward past the current roof now
  previews/validates instead of going red. Obstructed cells still break the
  chain; downward drags into unsupported space stay blocked.
- [FIX 2026-09-13] **Spiral stairs now span the full lobby height** — the
  first-party `data/bitmaps/stairs/spiral_2.png` / `spiral_3.png` sheets were
  64x60 / 64x96 frames (YootTower t2p extraction proportions, then reproduced
  by the OpenSkyAssets Blender dioramas), while the engine anchors stair
  sprites bottom-edge to the entry floor line and needs one 36 px floor pitch
  per story (64x72 / 64x108, matching the EXE-built sheets from the 8fe9/8fea
  64x36 slots). Spirals visually stopped ~2/3 up their lobby. Rebuilt both
  Blender dioramas at 4.5 BU/story (landing surfaces flush with the sprite
  edges, full-height column), re-rendered, and reinstalled 704x72 / 704x108
  sheets. Files: data/bitmaps/stairs/spiral_2.png, spiral_3.png (regenerated
  from OpenSkyAssets assets/stairs; see its LATEST.md).
- [FIX 2026-09-13] **Debug assert when placing stairs inside a multi-story
  lobby** — the lobby atrium links (b15db18) claim every covered column's UP
  link, so GameMap::addNode's `neighbours[UP] == NULL` assert fired for a
  legitimate stair placement. A stair now supersedes the atrium hop at its own
  tile, mirroring OpenSkyWeb gamemap.js last-writer-wins. Known follow-up:
  on save load, a lobby encoded AFTER its stair still overwrites the stair's
  link silently (both repos), leaving that stair unhaukable until re-placed.

-------------------------------------------------------------------------------
Concilia-tower adoption review (2026-09-14)
-------------------------------------------------------------------------------

Review of Jonahss/concilia-tower (clean-room C reimplementation reverse-
engineered from a Ghidra + hand-disassembly of SIMTOWER.EXE v1.1) against our
tree. Every adopted value was verified against the EXE itself: resource bytes
dumped from SIMTOWER.EXE (types 0x7F05/0x7F0b ids 1000-1002) and in-program
disassembly of the imported binary (Ghidra; concilia segment = Ghidra selector
-8). Full evidence log: `scratch/artifacts/ground-truth.md` (repo-local
artifact; generated from the imported binary).

Phase 1a conflict verdicts (blocking, all byte-adjudicated):
- **No star-promotion bonus exists.** Errata §3c's "$200k/$300k/$500k on
  star-up" misread the buried-treasure payout: the ceremony pays nothing; the
  treasure is a one-shot BUILD-triggered dig payout (star 1/2/3, basement
  B(star+2) extent > star*25 cells), latch re-seeded at each promotion
  (NotifyFacilityBuilt -> condition -> AwardMoney, 1150:0194..026f in our
  Ghidra mapping; `IMUL DX,DX,0x19` is the x25).
- **Express elevators get NO full-queue +1000 route surcharge.** Direct ride:
  express = queue_len + 640; std/svc = 8*|dx| + 640 (+1000 when the 40-queue
  is full). Transfers: +3000 base, 6000 full; express transfer = queue + 3000.
- **Disaster-day income gate scope**: restaurant/fast-food CLOSE income is
  suppressed on fire days (day%84==83) and bomb days (day%60==59); shops have
  no close income to gate; cinema/party/hotel paths are not day-gated.

Disposition (R = render, S = sim, E = economy; U rows are sequenced follow-ups
below, NOT implemented this campaign):

1. [ADOPT] Venue crowd tiers: patrons >=10 packed / >=1 busy / 0 empty
   (replaces min(ceil(people/5),2) in FastFood/Restaurant).
2. [ADOPT] Occupant people animation: 6-slot occupant bands, re-pose every 16
   sim ticks, EXE frame tables per venue/variant (sheets 0x85E8-0x85EE); keep
   our overlay architecture.
3. [ADOPT] Shop tenant type: new Item + Factory registration, 11 variants,
   For RENT/shutters shared frames, walk-in quota economy.
4. [ADOPT] Parking visuals: 15-frame merged sheet 0x86A8+0x86A9 (empty bay /
   red X / 13 car variants), pick 2+id%13, fill by ordinal.
5. [ADOPT] Parking usability: B1-anchored ramp chain (same-x consecutive
   floors), quota 2x usable per category, suite-guest-can't-park cancels stay,
   $0 income, ramp $10k/qtr (res 0x3EA item 44).
6. [ADOPT] Hotel room frames: door + 8 frames (occupied/clean/dirty/infested
   x day/night) + 4AM lights-out clamp.
7. [ADOPT] Red shaft floor digits: plate lights when a car of the group is on
   that floor (IsCarOnFloor); replaces runtime tint.
8. [ADOPT] Security office 3-frame palette cycle (197<->198 etc.).
9. [ADOPT] Construction crane: left-edge park, >=7 cells, hidden above
   displayed floor 100 / under cathedral, x captured only on top-floor-number
   change (stuck crane is intended).
10. [ADOPT] Restaurant 5th variant ("Steak House") — fix the 4*24 loader
    buffer in SimTowerLoader.cpp.
11. [FIX] YootCondo double-draw (residentSprite + renderPeople both draw).
12. [ADOPT] Event scheduler calendar: fire day%84==83 @10AM (star>2 + security
    + no cathedral + extent>=32), bomb day%60==59 (stars 2/3/4), santa day%12;
    no RNG (TimeT 1208:0342 byte-verified in our Ghidra pass).
13. [RETIRE] Burglar event — does not exist in the EXE. Candidate future
    OpenSky mod (kept in git history).
14. [ADOPT] Fire model: per-floor L/R 12-cell fronts, 1 cell/7 frames,
    upward-only +80 frames/floor, die at extent, 9PM hard stop, $500k chopper
    sweeps right->left 1 cell/frame, foot guards (6/office, 5-tick douse)
    free path, rubble 0x8FA8, clock jump 4PM if <16:00, no cash damage charge.
    DIVERGENCE: fix original B7 (chopper stops at origin-floor left extent) —
    sweep to actual leftmost front.
15. [ADOPT] Bomb: ransom $200k/$300k/$1M by star (tuning 0xDE1C/1E/20),
    floor hidden, guard hunt 6/office deterministic sweep 1 cell/2 frames,
    catch on exact cell, detonate 13:00 (frame 0x4B0 verified in TimeT),
    blast t-2..t+3 x +-20 cells -> rubble.
16. [ADOPT] Disaster-day income: restaurant/FF close income $0 on bomb/fire
    days (scope per 1a verdict above).
17. [ADOPT] Buried treasure: star 1/2/3, B(star+2) extent > star*25 cells ->
    next build pays $200k/$300k/$500k (tuning +0xA8), one-shot latch re-seeded
    at each promotion, over-dig forfeits. Byte-exact (1150:01ac condition).
18. [ADOPT] Santa flyby day%12 @9PM, x-10/tick, y bob (art/sound ids exist).
19. [ADOPT] VIP: real suite guest arriving by car (voids on failed park),
    arms day%9==3 at star 3 while unsatisfied, judged on own banked stress
    <=150, gates 3->4 only, promotion ceremony clears flag, reservation
    dialog names first bookable suite floor, NO cash reward (drop our
    $50k/$25k); keep our persistence plumbing.
20. [KEEP] Star thresholds 300/1000/5000/10000/15000 — already authentic
    (tuning +0x44..0x50 verified: 300/1000/5000/10000).
21. [ADOPT] Promotion gates: 2->3 security; 3->4 suites + recycling adequate
    + medical adequate + VIP + weekday-evening window; 4->5 metro + recycling
    + medical + window.
22. [RESOLVED] Promotion bonus: none (see 1a verdict). Row folded into 17.
23. [ADOPT] Cathedral: stops fires forever (0xB3EC gate); 5->TOWER needs
    cathedral + 15000 pop + weekend wedding: 40 guests fill by 12:30 ->
    ceremony (star 6 mid-ceremony), couple animation 0x8828 + 6 ceremony
    strips 0x8CE9..0x8DEA (SimTowerLoader slicing needed).
24. [ADOPT] Starting funds $2,000,000 + corner-click doubler (virgin tower,
    B10 cell 0, exact-balance check; build dispatcher 11f8:0955-098c).
25. [ADOPT] Express capacity 42 / std+svc 21 (push literals 0x2a/0x15
    verified at 1200:0a4b-0a57 in our pass).
26. [ADOPT] Car speed = 4-gear distance table (CalcMoveSpeed verified
    byte-exact; see ground-truth.md); express {<=1: crawl, <=4: cruise, else
    3f/t}, std/svc {<=1: crawl, <=3: middle, else 1f/t}.
27. [ADOPT] Door choreography: 5-tick door timer (tick5 = ding + unboard all
    + patience*30 dwell; odd ticks one boarder; tick1 bulk; full car departs
    instantly; idle cars park open; door-open steals calls).
28. [ADOPT] Queue cap 40/dir/floor, +5 full penalty once/16 frames, 300-frame
    watchdog -> bank full stress + storm off + 1/16 retry.
29. [ADOPT] Service elevators never bank wait stress; grand-lobby forgiveness
    25 (2-story) / 50 (3-story) ticks per bank.
30. [ADOPT] Stress model: banked felt-wait 0-300 delivered to home-tenant
    pool at final arrival; daily category passes; demand categories with bars
    150/200; no-route = instant 300; span stress 16/esc-floor + 35/stair-
    floor; walk-to-transport >=80 +30 / >=125 +60 once per trip; queue tint
    pink >=80 / red >=150. Retire our 0-100 addStress scale and per-activity
    venue reliefs.
31. [ADOPT] Route costs (byte-verified this session): escalator 8*d (+640
    flagged variant); stairs 8*d+640; express queue+640 (NO full surcharge);
    std/svc 8*d+640 (+1000 full); transfer 3000/6000; express transfer
    queue+3000; early-accept escalator hop <640; walking-leg caps 6 floors
    all-esc / 3 with any stair; ONE transfer slot; 16 routing slots (6/4
    cells); staff stairs-only <=3.
32. [FIX+ADOPT] Service routing: pass serviceRoute (currently never passed ->
    service elevators unroutable) + staff/maid routing rules.
33. [ADOPT] Elevator schedules: per-group [WD/WE][6 editable periods] mode
    (Full/ExpressUp/ExpressDown) + threshold 5 + patience 0-3 (*30 ticks),
    night period hidden, cars consume at dispatch/dwell.
34. [ADOPT] Shaft/car caps: 24 shafts, 8 cars/shaft, 29-floor non-express
    span (express uncapped), metro-depth rule, express sky-lobby anchoring,
    shaft clearance x+-8 different type.
35. [ADOPT] Car economics remainder: add-car home = clicked floor; removal
    refunds nothing. (Add-car prices $80k/$150k/$50k already authentic.)
36. [ADOPT] Maintenance: quarterly sweep from res 0x3EA (verified bytes):
    elevators per car (std $10k / express $20k / svc $10k), escalator $5k,
    lobby per-cell star-gated 0/300/1000, metro $100k, housekeeping $10k,
    security $20k, recycling $50k, ramp $10k. Retire invented daily table.
37. [ADOPT] Rent cadence: quarterly 4:59AM lumps from res 0x3E9 class table
    (verified bytes: office 15k/10k/5k/2k, single 3k/2k/1.5k/0.5k, twin
    4.5k/3k/2k/0.8k, suite 9k/6k/4k/1.5k, shop 20k/15k/10k/4k, condo
    200k/150k/100k/40k); move-outs before rent (leavers pay nothing); hotels
    per-checkout; shops quarterly; condo one-time + buyback; rent-class writer
    in info dialog (cut = instant re-judge, class 3 forces metric 0).
38. [ADOPT] Retail economy: quota caps by period (WD/WE/rainy 35/50/25 food,
    25/30/18 shop), walk-in pacing (rand%36 day / rand%6 evening), door
    admission (bounce closed / refuse >=40), income tiers restaurant
    >=50/35/25 -> +10k/6k/4k else -6k; FF +5k/3k/2k else -3k; late-shopper
    elevator grading (+2/+1/+0).
39. [ADOPT] Hotel economy: checkout lump by class (single $3k .. suite $9k
    class 0), ka-ching cadence (every 2nd <20 then every 8th). Fixes our
    zero-revenue bug.
40. [ADOPT] Hotel lifecycle: 5PM demand pass arms booking; neglect fuse (3rd
    dirty-day closed -> infested), roach spread to abutting rooms, checkout
    cures, maids 6/unit floors-mod-6 <=4PM never during disasters 64-tick
    dwell never clean infested.
41. [ADOPT] Cinema: quota by age tier (hits 60/60/40/20; ordinary
    40/40/40/20), hourly show cycle (10AM reset/noon open/1PM matinee/3PM
    summon/4PM out/5PM evening/8PM bank), income tiers <40:0 / <80:2k /
    <100:10k / >=100:15k, movie change $300k/$150k (tuning +0x96/98) with
    deterministic stepping, ids 0-13 (index 14 unreachable, B12), films 3/6
    silent (B13).
42. [ADOPT] Party hall: opens 1PM closes 5PM, flat $20,000 if anyone came.
43. [ADOPT] Metro: 240-person visitor pool, rand%36==0 per 16-tick visit
    10AM-5PM, no commuters, no fare income, train toggle frames 0/2.
44. [ADOPT] Office presence counter 0-6 (+1/-1), night idle sentinel; keep
    our lunch routing plumbing.
45. [ADOPT] Condo/restaurant flows: even-room hotel dinner parity, weekend
    every-4th-condo per-floor dining, condo homebody shopping once/day,
    no-venue -> leaves tower.
46. [ADOPT] Noise model: same-floor 20-cell restaurant/shop/FF/office/cinema/
    party -> +60 hotel verdict. Retire radii model.
47. [ADOPT] No-route feedback: instant 300 stress + latched per-floor
    "People on Floor X need a path to Floor Y" message.
48. [ADOPT] Escalator landing validated once at placement against commercial
    whitelist; never re-validated (bulldoze trick stands).
49. [U] Financial report on 0x81f4 art, expense rows reconcile maintenance
    categories, hotels-show-income-at-zero-population quirk.
50. [U] Minimap overlays: mode colors/order per MapPaint, true-color elevator
    lines, abandoned/construction colors.
51. [U] Fast Mode = tick-throttle bypass semantics onto our speed UI.
52. [U] Hidden debug menu recreation (9000 Terrorist / 9001 Treasure / 9002
    Fire + 9003 redraw) as dev tool.
53. [U] Drop build-complete sound (EXE plays none; only jackhammer during).
54. [KEEP OURS] Tenancy length persisted across load (EXE-faithful omission
    has no .TDT field; strictly nicer, matches concilia mod note).
55. [ADOPT-S] Cinema movie-id space 0-13 (sim side, Wave S); soundtrack
    wiring (ids 9001+movie_id, films 3/6 silent) deferred to U.
56. [ADOPT] Elevator car add path remainder: home = clicked floor, caps,
    funds gate.
57. [FIX] Latent bugs unconditionally: queue give-up wrong stress scale,
    stairlike double-increment, queue tint scale, hourToAbsolute>24.
58. [KEEP OURS] OpenSky-additive features: plugin/edition art, TDT import/
    export, drag-build, zoom, lighting, spiral stairs, dark UI, inspector.

Retired by this review: burglar event (13), venue stress reliefs (30),
noise radii model (46), invented daily maintenance table (36), invented
weekly office rent (37), our $4M start (24), VIP cash rewards (19),
promotion bonus claim (22).

Deliberate divergences from the EXE (keep our fixed behavior):
- B7 chopper left-escape fixed (row 14).
- Shop population counter recomputed leak-free (B3).
- Medical center single-band registration (B4).
- InParkingCar double-assign prevented (B2).
- Tenancy length persisted (row 54).

## Wave S / closeout review (2026-09-15)

Implemented this review's adopted runtime semantics through Wave S:

- Banked person stress is clamped to 0..300, delivered once at final journey
  arrival, with the adopted stair/escalator/walking thresholds and latched
  no-route feedback.
- Tenant and visitor evaluations now use the replacement same-floor noise rule,
  live capacity/pool counters, daily category passes, and bounded venue quotas.
- Quarterly settlement is idempotent at the 4:59AM/5:00AM boundary, processes
  Office/Condo move-out eligibility before rent, applies the verified rent and
  maintenance classes, and charges hotel rooms at checkout.
- Retail, cinema, party-hall, and metro pacing/caps follow the adopted Wave S
  rules. Promotion, VIP, disaster, parking, and loader changes remain intact.

Known deliberate divergences / deferred items:

- Office and Condo rent class is neutral class 1 because their existing public
  save/API surface has no rent-class field; Shop rent class is persisted.
- Cinema soundtrack IDs and silent-film audio suppression remain deferred to
  Wave U.
- Bomb hunt freeze/destruction details and the financial-report art remain
  explicit Wave E/U follow-ups; current event behavior uses the implemented
  deterministic safeguards documented in the source.
- The no-route API accepts explicit floor pairs; an empty Route alone has no
  destination metadata, so its compatibility fallback latches the current
  journey pair.

Verification performed after Wave S integration: `cmake --build build
-j$(nproc)`, `build/OpenSkyTests`, and the default-tower load smoke under
  `xvfb-run`; all completed successfully.

## YootTower edition-rules research (2026-09-15)

- Implemented policy: `--ruleset simtower`, `--ruleset opensky` (the default),
  and `--ruleset yoottower` are the stable CLI identifiers. Horizontal
  placement enforces SimTower's `[-187, 188)` lot only; OpenSky and generic
  YootTower remain unbounded. A YootTower World plugin may later add a
  scenario boundary, but only after its data is semantically decoded.
- Proven resource shapes only: Tokyo `BINF` begins `[61,46]` followed by a
  `61*46` grid payload; Hawai `BINF` begins `[31,50]` followed by `31*50`;
  Kegon `FINF` begins `[44,192]` followed by `44*192`. Correlated `WORLDDEF`
  values are observed, but neither their axis/header meaning nor enforcement
  semantics is decoded; they MUST NOT become numeric build limits.
- Priority candidates once evidence exists: scenario boundary; `BlockT` frame
  boundaries; `Kinsoku` neighbor compatibility; `DPDF` placement/economy;
  transport costs and shaft span; `ATTR` unlock/prerequisite gating.
- Smallest next step: recover the plugin `.text` consumer that assigns these
  headers their meanings, then derive any scenario-boundary enforcement from
  that code rather than from resource dimensions alone.
