This is the experimental `restart` branch of OpenSkyscraper, an open-source C++
reimplementation of SimTower and Yoot Tower (Tower II). It supports classic SimTower,
open-ended OpenSky, and YootTower rulesets, custom plugins, and deterministic headless simulation.

**You are welcome to join in the experiments, put forward suggestions or supply your own experimental code!**


How to try stuff
----------------
After you've cloned the repository, run `git submodule update --init .` inside your cloned
repository so the submodules are loaded appopriately.

To build the game, create a directory `build` and run `$ cmake ..` from within. Check that you have
all the necessary dependencies installed by looking at the CMake output warnings/errors. Once your
system checks out, run `$ make` to build, and `$ ./OpenSkyscraper` to launch the game. Observe the
game's output carefully as it may contain some hints why stuff isn't working.

At the moment, you need a `SIMTOWER.EXE` installed somewhere on your system. The game searches a
number of locations for this file. Check the `WindowsNEExecutable.cpp.15, load: from .../SIMTOWER.EXE`
lines of output for a list of locations.

If you instead wish to use the SIMTOWER.EX_ compressed from the disk, you must build with the optional
dependency of libmspack. As long as you have it installed in your system when you run "cmake .." then
it should be picked up.

Compile and run with Ubuntu 20.04 under WSL2
--------------------------------------------
* Install dependencies with `sudo apt install build-essential cmake doxygen graphviz libsfml-dev libfreetype-dev libmspack-dev`
* Clone https://github.com/libRocket/libRocket
* Build libRocket with `cd libRocket/Build && cmake . && make && sudo make install`
* Clone OpenSkyscraper repository
* Run `git submodule update --init .`
* Build OpenSkyscraper with `mkdir build && cd build && cmake .. && make`
* Run game with `./OpenSkyscraper`

Build with docs
---------------
* Replace the cmake step above with `cmake -DBUILD_WITH_DOCS=ON ..`

Game modes (rulesets)
---------------------
OpenSky supports three gameplay rulesets, selectable via `--ruleset <name>` or in the New Game dialog:

- **`opensky` (default):** Default experience with an unbounded horizontal lot, standard $2,000,000
  starting funds, SimTower-derived pacing, and support for all skins.
- **`simtower`:** Faithful recreation of classic 1994 SimTower rules, limitations, and evaluation logic.
- **`yoottower`:** Yoot Tower (Tower II) ruleset driven by world plugins, modular economics,
  infrastructure networks, and authentic scoring.

Key behavioural differences:
- **Build boundaries:** `simtower` restricts building to the original 375-cell lot (-187 to 188);
  `opensky` is unbounded; `yoottower` clamps construction to each world's build area rectangle,
  vertical floor limits, and terrain masks (FINF).
- **Starting funds & star rating:** `simtower` and `opensky` start with $2,000,000 and 1 star;
  `yoottower` starts with world-specific funds ($10M in Tokyo, $5M in Hawaii, $2M in Kegon) and
  star ratings (1★ Tokyo, 2★ Hawaii, 3★ Kegon).
- **Clock and calendar:** `simtower` and `opensky` run a nonlinear 2,600-frame day with weekend every
  third day; `yoottower` runs a minute-linear 24-hour clock with
  world-specific holidays, and deterministic rain (Tokyo days 4 and 8; Hawaii and Kegon never rain).
- **Economy & maintenance:** `simtower` and `opensky` settle finances around 05:00; `yoottower` charges
  maintenance at midnight on day 1 of each quarter, collects office rent quarterly at 04:52 (with an
  initial move-in fee), and ties commercial tenant profits directly to customer foot traffic.
- **Star ratings & VIPs:** `simtower` and `opensky` gate promotions behind specific facility counts
  (Security, Medical, Recycling, Metro) and VIP satisfaction; `yoottower` promotions depend strictly
  on population thresholds and annual VIP visits (or landmark events like Tokyo Stadium for 5★ / Tower).
- **Vertical transport:** `simtower` elevators span up to 29 floors; `yoottower` Simple Elevators are
  capped at 15 floors (max 6 cars of 17 capacity), Large Elevators can span any height but only serve
  up to 8 stops (40 capacity), and Kegon uses dedicated cliff elevators.
- **Facilities & hazards:** `yoottower` introduces Power Rooms (adding transformers to avert blackouts),
  Trash Facilities (mandatory from 3★ for restaurant waste), Restrooms, Doctor's Offices (curing colds),
  and Front Desk / Housekeeping systems for hotel rooms.


YootTower mode
--------------
Launch YootTower mode using `--ruleset yoottower` (or in the New Game dialog). Pick a world with
`--world <tokyo|hawaii|kegon>` (defaults to Tokyo) and use `--skin yoottower` for authentic visuals
(selected by default under the YootTower ruleset).

- **Worlds:**
  - **Tokyo:** 100 floors above, 9 basements; $10,000,000 start; 3-row lobby; final goal is a baseball
    game at the Tokyo Stadium.
  - **Hawaii:** 45 floors above, 6 basements; $5,000,000 start; 2-row lobby; final goal is a wedding
    ceremony at the Cathedral.
  - **Kegon:** Cliffside tower building downwards (2 floors above, 32 basements); $2,000,000 start;
    1-row lobby; final goal is the Attraction renovation.
- **Original resources (.t2p plugins):** Original YootTower game files are **not included** and must
  come from your own copy of Yoot Tower. OpenSky searches for `.t2p` files in `data/Plugins/`
  or in the folder set under **YOOTTOWER RESOURCES** in the in-game Settings
  dialog (saved as `yootPluginsPath` in `~/.openskyscraper/settings.xml`).
- **Loading & exporting saves:**
  - Load original YootTower `.bld` saves directly: `./OpenSkyscraper --ruleset yoottower --load-game file.bld`.
  - Export an OpenSky `.tower` save back to `.bld` using `BldTool`:
    ```sh
    ./build/BldTool export input.tower template.bld output.bld
    ```
    *Limitation:* Exporting requires an existing `template.bld` from the *same world* to supply sections
    OpenSky does not synthesize from scratch (plugin catalogs, visitor pool tables, world event lists).


Plugin Editor
-------------
OpenSky includes a standalone editor (`PluginEditor`) for inspecting and editing YootTower `.t2p` files
and OpenSky `.osp-plugin` packages.

- **Supported classes:** Edits all plugin classes, including Tenants (shops, restaurants, offices,
  condos), Movers (elevators, stairs, escalators), OuterObj (billboards), ToolPalette, World plugins,
  and SP sub-plugins (Movies `MV_*`, Hall Events `Pl*`, and Advertisements `lPdA`).
- **Running the editor:**
  - Interactive GUI: `./build/PluginEditor [path/to/plugin.t2p|plugin.osp-plugin]`
  - CLI inspection & batch editing:
    ```sh
    ./build/PluginEditor plugin.t2p --select TNDF:1
    ./build/PluginEditor plugin.t2p --set ATTR:128:name="New Name" --export-t2p modified.t2p
    ./build/PluginEditor plugin.t2p --save-osp unpacked.osp-plugin
    ```
- **Storage formats:**
  - `.osp-plugin`: OpenSkyscraper native directory bundle featuring human-readable JSON manifests,
    individual resource files, and PNG sprite sheets suited for Git and diffs.
  - `.t2p`: Binary PE DLL / SP container used by original Yoot Tower.
- **Plugin cache:** The original Yoot Tower game caches plugin names and metadata in `Plugins/Database`.
  If you add or modify `.t2p` files for use in the original executable, delete or update `Plugins/Database`
  so the original game rescans them.


Headless simulation and testing
-------------------------------
The `--sim` flag runs the simulation headlessly for deterministic simulation, automated QA, and
regression testing without rendering, audio, or user interaction:

```sh
cd build
./OpenSkyscraper --ruleset opensky --new-game --sim 2000 --sim-out /tmp/out.tower
./OpenSkyscraper --ruleset yoottower --load-game /abs/path/tower.bld --sim 3d --sim-out /tmp/out.tower
```

- **Options:** Specify duration using tick counts (`--sim 5000`) or game days (`--sim 3d`). Companion
  flags include `--sim-out <file.tower>` (required; also writes a `<file.tower>.json` summary),
  `--sim-summary <file.json>`, `--sim-seed <n>` (default 1), `--sim-speed <0..4>` (default 3), and
  `--sim-dump-every <n> <dir>`.
- **Determinism:** `--sim` seeds RNG deterministically, sorts objects by creation serial (`SimLess`)
  rather than heap addresses, and drains route queues per tick. Identical inputs produce byte-identical
  saves and JSON summaries across runs.
- **Running tests:** Regression tests must be executed from the `build` directory:
  ```sh
  cd build && ctest -R sim_ --output-on-failure
  ```
  These tests run fixed-tick simulations against reference golden saves in `tests/sim/` and verify summary
  JSON outputs. On systems without a display, run under `xvfb-run -a`. See `doc/headless-sim.md` for full details.


Known gaps
----------
OpenSky approximates Yoot Tower closely but not exactly, and several subsystems remain
approximate or missing (see `doc/yoottower/game-parity.md` for the full audit):

- **Elevator mechanics:** the Large Elevator now exports to `.bld` and matches the original's stops and
  riders per car, but car travel times have not been compared at fine time resolution; on the people tower,
  holiday (day 2) riders per car run high (9.5 vs 6.0).
- **Routing & transit:** People walk along floors and ride movers, but pathing uses OpenSky's A* PathFinder
  rather than original routing score tables; floor underneath Sky Bridges remains walkable in OpenSky routing.
- **Facilities & services:** Restroom and doctor visits update tenant satisfaction and health, but people
  do not visually walk to restrooms/clinics and back; clinic failure dialogs lack custom popups.
- **Disaster handling:** Power blackouts follow the original (the tower stays dark until supply is restored;
  tenants and movers freeze), but per-transformer individual breaker toggles are omitted; fires use SimTower's
  2-button response dialog rather than YootTower's 3-way response and burn spread model.
- **Media & cosmetics:** Movie theaters play soundtrack audio from `Movie/*.MOV` when present, but Cinepak
  video decoding is not supported (video stills remain static); vehicles (subway trains, parking gate cars)
  lack animated sprites.
- **Visuals checked only at the logic level:** Mr. Maru turning back at the row end, rubble tiling (drawn per
  cell, not per tenant), the kite in Kegon, and the Billboard countdown differ from or are not confirmed
  against the original; clouds, kite and dolphins have not been compared pixel for pixel.
- **Event Center:** booked events import from and export to `.bld`, open and settle on the original's
  days and hours, and now show the original's status chain (30 empty, 31 some inside, 32 from half the
  capacity) and its hourly profit estimate (300 once three visitors have paid). The audience matches the
  oracle on the current tree (five seeds: 53-84 on day 1 against 70, 128-152 on day 2 against 143). Not
  closed: the target of the first run (the original's `*(slot + 4)`), and the evening events stay unobserved.
- **Hotels:** after restoring the maids' cleaning step (lost in the YootTower merge) OpenSky fills 73-77% of
  the eligible rooms against the original's 82% on day 1, and day-2 funds are back inside tolerance on all six
  seeds; room estimates still run higher than the original's (280-300 against 248-276).
- **Population detail:** Hawaii has one condo resident more than the original (55 vs 54) and a few more
  daytime visitors; a coffee shop's pre-blackout demand is below the original's.

What is being tested?
---------------------
Currently we're performing the following experiments and tests:

- **SFML** as platform API (instead of SDL)
  - loads bitmaps, fonts and sounds directly from memory
  - modern C++ interface simplifies code


Coding Style
------------

- Add a copyright line in the top comment of each file you edit:
  ```
  /* Copyright © 2010-2014 John Doe */
  /* Copyright © 2013 Mark Mensk */
  ```

- Everything resides in the OT namespace.

- Source file names match the class names in capitalization, e.g. the class `Application` yields `Application.{h|cpp}`.

- The directory hierarchy reflects the namespace hierarchy, so `Math::Vector2D` goes into `Math/Vector2D.{h|cpp}`.

There are some violations of this style left in the source. I'm working on getting rid of them.

*General note on source files:* Lowercase source files are likely to be outdated and removed in the future. They're probably there because they contain some working code snippet.
