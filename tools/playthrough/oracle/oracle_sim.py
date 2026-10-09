#!/usr/bin/env python3
"""SimTower 16-bit Oracle automation driver running inside opensky-oracle container."""

import sys
import os
import time
import shutil
import subprocess
from PIL import Image

WINE_EXE = "/lab/SIMTOWER.EXE"
WINE_PREFIX = "/lab/wine32"

def start_xvfb(display=":99"):
    # Clean up any leftover lock files
    lock_file = f"/tmp/.X{display[1:]}-lock"
    if os.path.exists(lock_file):
        try: os.remove(lock_file)
        except OSError: pass
    xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", "1024x768x24", "-nolisten", "tcp"])
    time.sleep(1.5)
    return xvfb

def run_probe(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    probe_img = os.path.join(out_dir, "probe.png")
    log_file = os.path.join(out_dir, "probe_wine.log")
    
    print(f"[oracle] Running probe test, writing to {probe_img}...")
    env = dict(os.environ, WINEPREFIX=WINE_PREFIX, WINEDEBUG="-all", DISPLAY=":99")
    xvfb = start_xvfb(":99")
    
    with open(log_file, "w") as log_out:
        wine = subprocess.Popen(
            ["wine", "explorer", "/desktop=simtower,1024x768", WINE_EXE],
            env=env, stdout=log_out, stderr=subprocess.STDOUT
        )
        
        # 1. Dismiss sound card dialog (centered at 465, 417)
        time.sleep(3.5)
        subprocess.run(["xdotool", "mousemove", "465", "417", "click", "1"], env=env)
        
        # 2. Dismiss title screen (centered at 512, 384)
        time.sleep(3.5)
        subprocess.run(["xdotool", "mousemove", "512", "384", "click", "1"], env=env)
        time.sleep(3.0)
        
        # 3. Capture screenshot
        subprocess.run(["import", "-window", "root", probe_img], env=env, check=True)
        time.sleep(1.0)
        
        wine.terminate()
        time.sleep(1.0)
        xvfb.terminate()
        try: wine.kill()
        except OSError: pass
        try: xvfb.kill()
        except OSError: pass

    if not os.path.exists(probe_img) or os.path.getsize(probe_img) < 1000:
        print("[oracle] ERROR: Probe screenshot was not created or empty!", file=sys.stderr)
        sys.exit(1)
        
    # Check that probe image contains actual SimTower UI (not black or blank)
    im = Image.open(probe_img)
    colors = len(im.getcolors(maxcolors=10000) or [])
    print(f"[oracle] Probe screenshot generated ({im.size[0]}x{im.size[1]}, {colors} colors, {os.path.getsize(probe_img)} bytes).")
    if colors < 10:
        print("[oracle] ERROR: Probe screenshot has insufficient colors!", file=sys.stderr)
        sys.exit(1)
        
    print("[oracle] Probe succeeded!")

def run_simulation(tdt_path, days, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    if not os.path.isfile(tdt_path):
        print(f"[oracle] ERROR: TDT file not found: {tdt_path}", file=sys.stderr)
        sys.exit(1)

    print(f"[oracle] Setting up simulation: file={tdt_path}, days={days}, out={out_dir}")
    
    # Copy TDT file to C:\windows\system32\ACTIVE.TDT
    c_active = os.path.join(WINE_PREFIX, "drive_c/windows/system32/ACTIVE.TDT")
    shutil.copyfile(tdt_path, c_active)

    env = dict(os.environ, WINEPREFIX=WINE_PREFIX, WINEDEBUG="-all", DISPLAY=":99")
    xvfb = start_xvfb(":99")
    log_file = os.path.join(out_dir, "wine.log")

    with open(log_file, "w") as log_out:
        cmd = ["wine", "explorer", "/desktop=simtower,1024x768", WINE_EXE, "C:\\windows\\system32\\ACTIVE.TDT"]
        wine = subprocess.Popen(cmd, env=env, stdout=log_out, stderr=subprocess.STDOUT)
        
        # Dismiss sound warning dialog
        time.sleep(3.5)
        subprocess.run(["xdotool", "mousemove", "465", "417", "click", "1"], env=env)
        
        # Dismiss title screen
        time.sleep(3.5)
        subprocess.run(["xdotool", "mousemove", "512", "384", "click", "1"], env=env)
        time.sleep(4.0)

        # Click Fast Speed button (x=299, y=504)
        print("[oracle] Activating Fast Speed simulation...")
        subprocess.run(["xdotool", "mousemove", "299", "504", "click", "1"], env=env)
        time.sleep(1.0)

        for d in range(1, days + 1):
            print(f"[oracle] Simulating Day {d}/{days} (24 hourly captures)...")
            for h in range(24):
                shot_name = os.path.join(out_dir, f"shot_d{d}_h{h:02d}.png")
                subprocess.run(["import", "-window", "root", shot_name], env=env)
                time.sleep(1.0)
            
            # Save state at end of day
            print(f"[oracle] Saving tower state at end of day {d}...")
            subprocess.run(["xdotool", "key", "alt+f"], env=env)
            time.sleep(0.5)
            subprocess.run(["xdotool", "key", "s"], env=env)
            time.sleep(2.0)
            
            day_tdt = os.path.join(out_dir, f"day{d}.TDT")
            shutil.copyfile(c_active, day_tdt)
            print(f"[oracle] Wrote {day_tdt} ({os.path.getsize(day_tdt)} bytes).")

        time.sleep(1.0)
        wine.terminate()
        time.sleep(1.0)
        xvfb.terminate()
        try: wine.kill()
        except OSError: pass
        try: xvfb.kill()
        except OSError: pass

    print(f"[oracle] Playthrough simulation complete for {days} day(s).")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: oracle_sim.py probe <out_dir> | oracle_sim.py run <tdt_file> <days> <out_dir>")
        sys.exit(1)
        
    mode = sys.argv[1]
    if mode == "probe":
        out = sys.argv[2] if len(sys.argv) > 2 else "/lab/out/oracle32"
        run_probe(out)
    elif mode == "run":
        if len(sys.argv) < 5:
            print("Usage: oracle_sim.py run <tdt_file> <days> <out_dir>")
            sys.exit(1)
        tdt = sys.argv[2]
        days = int(sys.argv[3])
        out = sys.argv[4]
        run_simulation(tdt, days, out)
    else:
        print(f"Unknown mode: {mode}")
        sys.exit(1)
