# SimTower 16-bit Oracle Container (`opensky-oracle`)

A headless container environment running the original 16-bit Windows `SIMTOWER.EXE` under Wine on the Unraid lab server (`192.168.4.3`). This oracle serves as the ground-truth reference for comparing simulation behaviour, timing, elevator scheduling, and economy against the OpenSky C++ and Web ports.

---

## 1. Architecture & Design

- **Base Image**: 32-bit x86 (`i386`) Debian Bookworm container (`opensky-oracle:latest`).
- **Wine Runtime**: Wine 8.0 32-bit (`wine-bin:i386`) capable of running 16-bit Windows 3.1 binaries via WOW16.
- **Display Server**: Headless X11 virtual framebuffer (`Xvfb :99 -screen 0 1024x768x24 -nolisten tcp`).
- **Window Management**: Wine virtual desktop (`explorer /desktop=simtower,1024x768`) providing clean MDI window hierarchies and message loops.
- **Automation Driver**: Python 3 (`oracle_sim.py`) utilizing `xdotool` and ImageMagick `import` to manage the GUI lifecycle:
  1. Dismisses the sound card detection dialog (`Yes` button at `(465, 417)`).
  2. Dismisses the splash title screen (click at `(512, 384)`).
  3. Pre-stages and opens target `.TDT` file directly via Wine CLI.
  4. Activates **Fast Mode** (`(299, 504)`).
  5. Captures 24 hourly screenshots per day (`shot_d<D>_h<H>.png`).
  6. Triggers in-game document save (`Alt+F`, `S`) to export the post-simulation `.TDT` state (`day<D>.TDT`).

---

## 2. Harness Interface (`oracle.sh`)

The local harness script is located at `tools/playthrough/oracle/oracle.sh`:

```bash
# Build the opensky-oracle Docker image on the Unraid lab
./tools/playthrough/oracle/oracle.sh image

# Start the opensky-oracle container
./tools/playthrough/oracle/oracle.sh up

# Verify headless Wine execution and capture probe screenshot
./tools/playthrough/oracle/oracle.sh probe

# Run a headless simulation on a TDT save for N days
./tools/playthrough/oracle/oracle.sh run data/SCHMITT.TDT 1

# Pull run artifacts and downscaled screenshots to laptop
./tools/playthrough/oracle/oracle.sh pull SCHMITT

# Stop and remove container
./tools/playthrough/oracle/oracle.sh down
```

---

## 3. Playthrough Comparison Report: `SCHMITT.TDT` (Day 1)

Comparison of Day 1 simulation results for the reference tower `SCHMITT.TDT` across the three implementations:

| Metric | Original 16-bit SimTower (Oracle) | Web Port (`opensky-web`) | C++ Port (`OpenSkyscraper`) |
| :--- | :--- | :--- | :--- |
| **Star Rating** | 4 Stars | 5 Stars | 4 Stars |
| **Day 1 Funds** | **$10,839** | **$9,356** | **-$167,644** |
| **Offices** | 2,035 | 2,035 | 2,035 |
| **Double Hotel Rooms** | 240 | 240 | 240 |
| **Single Hotel Rooms** | 103 | 103 | 103 |
| **Hotel Suites** | 14 | 14 | 14 |
| **Floors** | 113 | 113 | 113 |
| **Parking Spaces** | 264 | 264 | 264 |
| **Fastfood / Cafes** | 59 | 59 | 59 |
| **Elevators & Transit** | 1 Cathedral, 1 Metro | 1 Cathedral, 1 Metro | 1 Cathedral, 1 Metro |

### Key Observations & Parity Findings

1. **Economy Calibration**:
   - The original SimTower game finished Day 1 with **+$10,839** funds.
   - The Web port matches the original game closely at **+$9,356**.
   - The C++ port exhibits severe economy divergence (**-$167,644** on Day 1, cascading to **-$9,159,644** by Day 3), indicating that maintenance expenses, rent collection schedules, or initial cash deductions in the C++ engine diverge significantly from the 16-bit original.

2. **Simulation Time Progression**:
   - At Fast Speed (Speed 3), 1 SimTower hour corresponds to ~1.0 second of real time (~24 seconds per simulation day).
   - Tenancy updates, elevator traffic cycles, and state transitions execute deterministically across hourly snapshots.
   - The saved `day1.TDT` state contains **112,411 modified bytes** reflecting tenant stress, money changes, and elevator positions compared to the input save.

3. **Hourly Visuals**:
   - Pulled hourly screenshots are stored in `tools/playthrough/out/oracle32/SCHMITT/shot_d1_h00.png` through `shot_d1_h23.png` and downscaled to 640x400 (<2.5 MB total).
