# OpenSky Playthrough Harness (WP6)

A repeatable, scripted headless and graphical playthrough harness for OpenSky C++ and OpenSkyWeb ports, containerized on the remote Unraid server (`192.168.4.3:2223`).

## Overview

The harness automates deterministic headless simulations, graphical runs capturing hourly screenshots under Xvfb/Playwright, daily metrics extraction (funds, population, rating, item counts, stressed people, elevator wait time), run comparisons, and asset synchronization back to the development environment.

All execution runs inside an isolated, resource-capped container (`opensky-play`) on the remote Unraid host (`yoottower-lab`), keeping the local laptop free from heavy compilation and execution.

## Architecture & Directory Layout

```
tools/playthrough/
├── Dockerfile             # Container definition (Fedora 44, Chromium, Bun, Playwright, ImageMagick)
├── play.sh                # Main CLI harness script controlling the lab container and workflows
├── oracle_probe.sh        # Wine 16-bit executable probe and diagnostic screen capture
├── web_drive.mjs          # Playwright headless driver for OpenSkyWeb (OpenSkyScraper edition, camera views, speed 3, screenshots, metrics)
├── cpp_view.sh            # C++ GUI camera helper (goto lobby/floors/cathedral via keys + minimap click)
├── dismiss_dialog.sh      # closes the C++ GUI's modal Financial Report
├── oracle/                # Wine oracle: oracle.sh, oracle_sim.py (dialog watchdog, scroll camera)
├── compare.py             # Day-by-day metrics aligner and delta diff generator
├── parse_tower_metrics.py # Metric extractor combining SimHarness JSON and .tower XML attributes
├── downscale_pull.py      # Downscales pulled screenshots to 640x400 and caps total archive size (<50MB)
├── out/                   # Local pulled results (downscaled PNGs, JSON/CSV metrics, logs)
│   ├── ref/               # C++ headless 3-day run results (day*.tower, day*_summary.json, metrics.*)
│   ├── ref-shots/         # C++ graphical 24 hourly screenshots (shot_d1_h*.png, gui.log)
│   ├── oracle/            # Wine probe output (wine_probe.log, wine_screenshot.png)
│   └── web/               # OpenSkyWeb Playwright run (shot_day1_hr*.png, metrics.*)
└── README.md              # Documentation, working commands, metrics, and known limits
```

### Remote Environment (Unraid `/mnt/cache/appdata/opensky-play/`)
- Container: `opensky-play` (`--cpus 12 --memory 12g --shm-size 2g`)
- Base Image: `opensky-play:latest` (built on top of `yoottower-lab:latest`)
- Directory Mapping: `/mnt/cache/appdata/opensky-play` -> `/lab` inside container
  - `/lab/src/ref`: C++ engine worktree
  - `/lab/src/web`: OpenSkyWeb worktree
  - `/lab/out/`: Output directories for each run target
  - `/lab/tools/`: Synced driver and evaluation scripts
  - `/lab/data/`: Game saves (`SCHMITT.TDT`, `BARKLE4D.TDT`, `THEECSTA.TDT`, converted `.tower` files)

---

## Working Commands & Workflow

All commands are driven via `./play.sh` from the repository root or `tools/playthrough/`:

### 1. Remote Container Management
- **Build Image**:
  ```bash
  ./play.sh image
  ```
  Syncs `Dockerfile` to Unraid and builds `opensky-play:latest`.
- **Start Container**:
  ```bash
  ./play.sh up
  ```
  Syncs helper tools and launches container `opensky-play` capped to 12 CPUs and 12GB RAM.
- **Stop Container**:
  ```bash
  ./play.sh down
  ```

### 2. Source Tree Synchronization & Compilation
- **Sync C++ Tree**:
  ```bash
  ./play.sh sync /home/tim/Projects/OpenSky ref
  ```
- **Sync Web Tree**:
  ```bash
  ./play.sh sync /home/tim/Projects/OpenSkyWeb-art-v2 web
  ```
- **Build C++ Engine (`OpenSkyscraper`)**:
  ```bash
  ./play.sh build ref
  ```
  Builds `OpenSkyscraper` with CMake and Ninja in `Release` mode on the container.

### 3. Simulation & Playthrough Runs
- **Run Headless 3-Day C++ Simulation (`SCHMITT.TDT`)**:
  ```bash
  ./play.sh cpp ref data/SCHMITT.TDT 3
  ```
  Executes headless simulation for days 1, 2, and 3. Emits `day*.tower`, `day*_summary.json`, `end.tower`, `metrics.json`, and `metrics.csv` to `/lab/out/ref/`.
- **Run Graphical C++ 24-Hour Screenshots (`SCHMITT.TDT`)**:
  ```bash
  ./play.sh cpp-shots ref data/SCHMITT.TDT 1
  ```
  Launches `OpenSkyscraper` under `xvfb-run` (1280x800x24), sets speed 3, and captures hourly screenshots (`shot_d1_h00.png` through `shot_d1_h23.png`).
- **Run Web Port Playwright Smoke**:
  ```bash
  ./play.sh web web data/SCHMITT.TDT 1
  ```
  Converts `SCHMITT.TDT` to XML `.tower`, launches the local web server (`bun run server.js`), connects Playwright, verifies `window.__game` initialization, captures initial screenshot, steps 24 in-game hours, and writes daily metrics.
- **Probe Wine 16-Bit Oracle**:
  ```bash
  ./play.sh oracle
  ```
  Tests `SIMTOWER.EXE` under Wine 11.0 Staging with Xvfb, saves `wine_probe.log` and `wine_screenshot.png`, and evaluates compatibility.

### 4. Metrics Comparison & Pulling Results
- **Compare Parity Between C++ Reference and Web Port**:
  ```bash
  ./play.sh compare out/ref out/web
  ```
  Parses `metrics.json` from both runs, aligns by day, and outputs deltas for funds, population, rating, stress, elevator wait, and per-item breakdowns.
- **Pull Run Results Locally**:
  ```bash
  ./play.sh pull ref
  ./play.sh pull oracle
  ./play.sh pull web
  ```
  Rsyncs outputs from Unraid to `tools/playthrough/out/`. Downscales all PNG screenshots to max 640x400 (LANCZOS) and caps the archive to 48 images, keeping pull payload under 50 MB total.

---

## 3-Day SCHMITT.TDT Reference Run Metrics

Generated by `./play.sh cpp ref data/SCHMITT.TDT 3`:

| Day | Funds ($) | Population | Rating (★) | Stressed People | Elevator Avg Wait (s) | Key Events / Economy Phase |
|:---:|:---------:|:----------:|:----------:|:---------------:|:---------------------:|:---------------------------|
| **1** | -167,644  | 376        | 4          | 0               | 0.0                   | Initial day operation; daily retail expenses ($177k). |
| **2** | -8,958,644| 421        | 4          | 0               | 0.0                   | Quarter 3 rollover; quarterly maintenance expenses ($8,767,000). |
| **3** | -9,159,644| 392        | 4          | 0               | 0.0                   | Post-rollover steady state; retail expenses settle. |

### Item Breakdown (Constant Across Days 1–3)
- **Offices**: 2,035
- **Parking Spaces**: 264
- **Hotel Rooms**: Double (240), Single (103), Suites (14)
- **Floors**: 113
- **Fast Food**: 59
- **Lobbies**: 9
- **Security Offices**: 8
- **Medical Centers**: 8
- **Restaurants**: 4
- **Cathedral, Cinema, Metro Station, Party Hall, Recycling**: 1 each

---

## Wine 16-Bit Oracle Verdict & Analysis

### Verdict: **UNSUPPORTED** in current container configuration
The 64-bit Wine 11.0 Staging build in Fedora 44 (`x86_64`) cannot execute `SIMTOWER.EXE`.

### Evidence:
- **Executable Format**: `SIMTOWER.EXE` is an x86 16-bit Windows 3.1 NE (New Executable).
- **Execution Log**:
  ```
  0024:fixme:winediag:loader_init wine-staging 11.0 is a testing version containing experimental patches.
  Could not load 'WAVMIX16.DLL' required by 'Tower', error=2
  ```
- **Screenshot**: Display buffer (`wine_screenshot.png`) remains blank (black 1280x800) because the PE loader halts during 16-bit DLL dependency resolution before initializing Win16 GDI or creating a window.

### Non-Invasive Alternatives (No Host Package Installations):
Because the Unraid host OS must not have packages installed, oracle alternatives should run strictly as containerized services:
1. **Containerized 32-bit Wine + WineVDM (`otvdm`)**: Run an `i386` Debian/Alpine container with 32-bit Wine and WineVDM to emulate the 16-bit Windows API on top of Win32.
2. **Containerized DOSBox-X with Windows 3.11**: An isolated Docker container running DOSBox-X in headless mode, booting a clean Windows 3.11 for Workgroups image with `SIMTOWER.EXE` auto-started via `AUTOEXEC.BAT` / `WIN.INI`.
3. **Containerized QEMU i386**: Headless virtual machine running Windows 95/98 in a QEMU container.

---

## Web Port (OpenSkyWeb) Game Instance Exposure

In OpenSkyWeb (`src/main.js`), the engine attaches internal handles to the global `window` object for debugging, automated testing, and metrics inspection:

- **`window.__game`**: The core `Game` state instance:
  - `game.funds`: Current cash balance.
  - `game.population`: Active population count.
  - `game.rating`: Tower star rating (0-indexed; rating 4 corresponds to 5-star tower).
  - `game.people`: Array of active `Person` instances (`p.stress` tracks individual stress level).
  - `game.items`: Array of placed tower items (elevators, offices, hotel rooms, etc.).
  - `game.decodeXML(xmlString)`: Directly loads a `.tower` XML save file.
  - `game.setSpeedMode(3)`: Sets simulation speed (1: normal, 2: fast, 3: ultra-fast).
  - `game.advance(dt)`: Advances game time deterministically by `dt` seconds.
- **`window.__renderer`**: The canvas renderer handle (`renderer.render()`).
- **`window.openSkyScraper`**: Edition controller:
  - `openSkyScraper.unlock()`: Unlocks the OpenSky community art edition.
  - `openSkyScraper.unlocked`: Boolean check.
- **URL Parameters**:
  - `?edition=opensky`: Automatically unlocks and switches to the OpenSkyScraper edition on page load.

---

## Metric method

| Source | Where the numbers come from |
|---|---|
| C++ headless (`play.sh cpp`) | SimHarness summary JSON (`funds`, `population`, `rating` 0-based => stars = rating+1, `time`). Hourly state: `--sim 3d --sim-dump-every 40 <dir>` then `cpp_hourly.py` (40 ticks = 1 game hour at speed 3). |
| C++ GUI (`play.sh cpp-shots`) | `extract_cpp_metrics.py` -> `ocr.py` on the time window (1280x800). `dismiss_dialog.sh` closes the modal Financial Report so the sim keeps running. |
| Web (`play.sh web`) | `web_drive.mjs` writes `hourly.json` per elapsed game hour: game state, plus the HUD DOM text and element boxes. `ocr.py train/validate web` OCRs the HUD pixels against the DOM text. `.TDT` saves are fed to the web port's own importer (`src/core/tdt-parser.js`). |
| Oracle (`oracle/oracle.sh run`) | HUD OCR with `ocr.py` (1024x768 SimTower window); `dialogs.log` lists dismissed modals. `oracle_sim.py`'s own `metrics.json` is superseded. `day*.TDT` headers cross-check. |

`ocr.py` = fixed HUD regions -> binarise -> split glyphs on empty columns -> look up bitmap templates in `ocr_templates.json`
(exact, else nearest within +/-1 px and a Hamming budget, else `?`). Dates are matched as whole strings. Templates come from
screenshots with known values (oracle/C++: clusters labelled once from a contact sheet and checked by `ocr_validate.py`;
web: trained from the DOM text). Evidence (region crops, validation JSON, glyph sheets) is in `out/compare/ocr/`.
`make_report.py` builds `out/compare/tables.md`; `montage_compare.py` the day-1 montages; see `out/compare/REPORT.md`.

## Known findings (SCHMITT.TDT, 3 game days, art branches with the TDT import fix)

* TDT import is now exact in both ports: freshly loaded HUD / state reads $93,560,900, pop 15,401, 5 stars, 2nd WD / 2Q / year 13.
* The oracle clock cannot be read digitally (analog watch only), and the oracle advances ~1 game day per 17-24 shots, so
  the tables key shots by index and also align by in-game date. The oracle starts one day later than the save (WE/2Q).
* Oracle modal dialogs: the "Congratulations ... Tower" rating dialog (and any other modal: VIP, disaster, quarterly report) froze the sim and the
  old run stalled at day 3 hour 06. `oracle_sim.py` now runs a `DialogWatchdog`: Wine's desktop mode exposes every Win16 window as an X window, so it diffs
  `xdotool search --onlyvisible --name ""` against the main-window baseline taken at start, presses Return (default OK), falls back to a click at (502,233) and
  Escape, and writes `dialogs.log` + `dialog_NNN_<tag>_before/after.png` into the run dir. It runs every shot, around the end-of-day save and before camera drags.
  `ORACLE_SHOT_INTERVAL` (default 1.6 s) sets the wall-clock gap between shots; the oracle advances ~1 game day per 22-27 shots, and the Tower dialog fires on the
  first weekend after the 5-star promotion (seen at d3_h16 and d4_h06 depending on pacing; use `oracle.sh run data/SCHMITT.TDT 4` to be sure to cross it).
  The oracle also moves its own camera (scrollbar thumb drag at x=1007: lobby y=565, floors 330, cathedral 125): hourly shots are lobby views and
  `view_dD_h12_{cathedral,floors,lobby}.png` are taken each day.
* Population: oracle stays ~15,0-16,2k; both ports drop ~440 (hotel checkout) to ~14.96k within ~5 hourly samples and then track the oracle within ~7% (the earlier 12,720 plateau came from a stale pre-WP9 C++ build; fresh C++ day-1 end 15,537, day-3 end 15,500).
* Quarter rollover (3Q) income: oracle +$14.2M ($94.6M -> $108.8M), C++ +$15.8M ($94.46M -> $110.25M), web +$18.2M (-> $112.9M).
  C++ is the closer port (+1.4% vs oracle after the rollover, web +3.8%). Web and C++ agree to the dollar before the rollover.
* Web performance bug: `Parking.advance()` calls `recomputeAllChains()` (src/game/items/parking.js) for each of the 264 parking
  items every tick and again per car arrival; each call is ~0.36 s, so one `game.advance(0.05)` takes ~95 s on this tower
  (profile: 99.9 % in recomputeAllChains). `web_drive.mjs` serves a memoised parking.js through `page.route` (per item count and
  game hour; identical for an unedited tower). `--no-parking-shim` measures the raw port.
* Web edition: `?edition=opensky` only unlocks the dev flag; the edition is active once the menu button `[data-mode=opensky]` has been clicked and
  `game.ownership.mode === "opensky"` (art sheets registered). `web_drive.mjs` now waits for that (and network idle) before loading the tower; previously a 500 ms sleep
  let the first frames use the procedural fallbacks. It also sets the camera through `game.poi` (`window.__setView('lobby'|'cathedral'|'floors')`, world px x=cell*8, y=floor*36):
  hourly shots are lobby views, `view_dD_h12_*` / `view_dD_h23_*` add cathedral and hotel floors.
* Web HUD after a TDT import was stale (FUND $2,000,000, 1 star): harness ordering, fixed by calling the same `game.ui.update*` / `reloadToolbox` / `renderMap` / `refreshFinance`
  as `refreshAll()` in src/main.js. The `★` rating glyphs still render as boxes in this container (missing font), a harness-environment issue.
  HUD panels also reflow, so the OCR crops use the logged DOM boxes.
* C++ GUI: the quarterly Financial Report modal pauses the sim (fixed in the harness by `dismiss_dialog.sh`); after the
  dialog the GUI run still advances only ~2 game days in 72 shots, so use the headless run for per-day numbers.
* `play.sh web` used to kill its own shell (`pkill -f server.js`) and silently stop after the build; fixed (`pkill -x bun`, detached server).
* `downscale_pull.py` pruning used to drop arbitrary shots; it now keeps hours 07/08/12/18/23 first.
* C++ GUI camera: the game starts at the top of the tower. `cpp_view.sh goto <lobby|floors|cathedral>` uses the game's own keys (Up/Down = 20 px per press, clamped at the
  world bottom) plus a minimap click for the horizontal centre; lobby = bottom clamp + 13 Up presses, floors 172, cathedral 193 (SCHMITT.TDT, 1280x800).
  `play.sh cpp-shots` puts the camera on the lobby for the hourly shots and takes `view_dD_h12_{floors,cathedral,lobby}.png` every day.
