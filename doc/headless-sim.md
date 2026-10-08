# Headless deterministic simulation (`--sim`)

`--sim` runs the simulation without the interactive loop: it loads a tower
(or seeds a new one), advances it with a fixed timestep, writes the final
save plus a JSON summary, and exits. Two runs with the same binary and
inputs produce byte-identical saves and summaries, so the harness can be
used to diff behaviour between commits or rulesets.

## Usage

```sh
cd build
./OpenSkyscraper --ruleset opensky --load-game /abs/path/tower.tower \
    --sim 3d --sim-out /tmp/out.tower
./OpenSkyscraper --ruleset simtower --new-game --sim 5000 --sim-out /tmp/new.tower
```

| Flag | Meaning |
| --- | --- |
| `--sim <n>` / `--sim <n>d` | Run `n` fixed ticks, or until the clock advanced `n` game days. Implies a game start (`--new-game` unless `--load-game` is given). |
| `--sim-out <path.tower>` | Final save (required). The summary goes to `<path.tower>.json`. |
| `--sim-summary <path.json>` | Summary path override. |
| `--sim-seed <n>` | `srand()` seed (default 1). Every `rand()` consumer draws from it. |
| `--sim-speed <0..4>` | Speed mode while simulating (default 3 = 12x; 4 = Fast, 3 ticks per step). |
| `--sim-dump-every <n> <dir>` | Also write `<dir>/tick_<n>.tower` and `.json` every `n` ticks. |

Each step is one `Game::advanceHeadless(1/60 s)`: the same sim ticks as
`Game::advance` (clock, accounting, items, people, hour events, grades) and
the Sky weather roll, but no rendering, input, sound pacing or UI refresh.
A pending emergency choice (fire crew, ransom) is answered "yes" and
logged. At speed 3 one game day is about 960 ticks.

`--load-game` accepts anything the game can load: native `.tower`, SimTower
`.tdt` and YootTower `.bld` (imported through BldLoader), e.g.
`--ruleset yoottower --load-game "/abs/Sample Tokyo Map.bld" --sim 3d`.

Use absolute `--load-game` paths: relative names are looked up in the user
data directory (`~/.openskyscraper`) first. The ruleset follows the usual
rules (`--ruleset` overrides the save's `ruleset=` attribute).

The game still creates its SFML window (TGUI and textures need a GL
context) but hides it. On a machine without a display run it under
`xvfb-run -a`, or point `DISPLAY` at one shared `Xvfb :97` when running
many sims in parallel (parallel `xvfb-run -a` calls can race for a
display number).

## Summary JSON

`ruleset`, `world`, `seed`, `ticks`, `dt`, `speed`, `time` (absolute, hour,
day, quarter, year), `funds`, `rating`, `population`, `people`, `rainy`,
`money` (today/quarter income and expenses), `items` (count per type
bucket) and `events`: every player-visible message (`TimeWindow` messages,
income/cost notices, weather, VIP/event notices) and rating change,
stamped with the absolute game time.

## Determinism

`--sim` switches on `Sim::deterministic()`, which gates the few
host-dependent paths. Normal play is unchanged:

* `rand()` is seeded with `--sim-seed` before any game object exists.
* Pointer-keyed sets and maps of game objects (`Game::items`,
  `Game::people`, per-item people, elevator cars and queues, customers,
  guests, workers, judge/event bookkeeping) use `SimLess`, which orders by
  each `GameObject`'s creation serial instead of its heap address. Heap
  addresses differ run to run (GL drivers, threads), and iteration order
  drives both the save's item order and the order of `rand()` draws.
* The lobby-route rebuild queue (`Game::pumpRouteBuilds`) is drained each
  tick instead of stopping after a 4 ms wall-clock budget.
* Autosave (throttled by the wall clock) is off.

A `--sim` run reproduces another `--sim` run, not an interactive session:
play advances by the real frame time and also runs the per-frame UI,
camera and sound work that the headless step skips.

The game still reads `~/.openskyscraper/settings.xml` (SIMTOWER.EXE and
YootTower plugin paths). That changes the art and the `skin=` save
attribute but not the SimTower/OpenSky simulation; the CTest goldens pass
with an empty `HOME`.

## Regression tests

CTest runs `sim_<default|new>_<simtower|opensky>`: 2000 ticks on
`data/default.tower` and on a new tower, comparing the summary with
`tests/sim/<name>.json` (`tests/sim/run_sim_golden.cmake`). They are
skipped when neither `DISPLAY` nor `xvfb-run` is available.
`sim_people_yoottower` runs `tests/sim/yoot_people.tower` (a small Tokyo
tower with nine plugin tenants; the plugins come from `data/Plugins`) for
2000 ticks and pins the visitor pool and the walking customers: the
summary's events include each plugin tenant's daily `customers:` count and
a midnight `pool:` line (people called and entered, pool population,
visitors by phase). YootTower visitors live in a plain vector in creation
order, so they need no `SimLess`.

```sh
cd build && ctest -R sim_ --output-on-failure
```

After an intended behaviour change, regenerate a golden:

```sh
cmake -DEXE=build/OpenSkyscraper -DTOWER=$PWD/data/default.tower \
      -DRULESET=opensky -DTICKS=2000 -DOUT_DIR=/tmp/sim \
      -DGOLDEN=$PWD/tests/sim/sim_default_opensky.json -DUPDATE_GOLDEN=ON \
      -P tests/sim/run_sim_golden.cmake
```

## Comparing two commits

Build the harness on both commits (back-port the `--sim` patch to older
ones), run the same tower x ruleset x length on each, and `cmp` the saves
and summaries. A difference that only adds YootTower attributes
(`world=`, `yootVipFlags=`) cannot appear for SimTower/OpenSky runs, so
any diff there is a behaviour change. Bisect by rebuilding intermediate
commits with the patch applied.
