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

### Game Speed
Game speed seems rather fast compared to elevator movements. Cinema customers hardly arrive at the theatre in time. Slow down time, or speed up elevators?

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
    - [DONE] **VIP visits** — `source/VipSystem.{h,cpp}` schedules random VIP visits once the tower hits 2★ / 100+ population (3–8 day gaps, longer after a bad review). Each ~2h visit evaluates the latest judge pass and grants a cash bonus (full $50k for excellent, half for content) or a complaint. State persists across save/reload via `vipNextVisit` / `vipVisiting` attributes; `positiveReviews()` is intended to gate the Cathedral ending.
    - [DONE] **Level-up dialog window** — `source/LevelUpDialog.{h,cpp}` TGUI modal pops up on every promotion showing the new star count and the comma-separated list of freshly-unlocked prototypes (driven by `LevelUp::minRatingToBuild`). Closes on OK button, title-bar X, or Escape; auto-closed on `clearWorld()`.
    - [ ] **Cathedral "Tower of the Year"** ending — needs a Cathedral item (none exists yet); the VIP `positiveReviews` counter is plumbed for future gating.
    - [ ] Spawn an actual walking VIP sprite (currently the visit is abstract).

- [ ] **3.3 Fire / Emergency / Thief events** — No files exist. Priority LOW.
    - [ ] `EventScheduler` randomly triggers events based on time + rating.
    - [ ] Fire: floor-to-floor spreading, helicopter extinguish, water spray.
    - [ ] Terror: bomb threats, player defusal minigame.
    - [ ] Thief: targets tenants, calls police, existing `Security` item becomes functional (currently STUB).
    - [ ] Emergency mode: reset all person states, evacuate.

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
| Level/Progression                 | codemap.md:1279-1343  | source/Game.cpp ratingMayIncrease, source/VipSystem.*, source/LevelUpDialog.* | PARTIAL  |
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
| Parking                           | codemap.md:1746-1776  | source/Item/Parking.*           | GOOD    |
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
| VIP                               | codemap.md:2653-2690  | source/VipSystem.*              | PARTIAL  |
