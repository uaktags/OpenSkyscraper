#!/usr/bin/env python3
"""SimTower 16-bit Oracle automation driver running inside opensky-oracle container."""

# NOTE: the metrics.json written by extract_oracle_metrics() below is superseded/unreliable (hand-drawn
# glyph templates, wrong balance parse). Use tools/playthrough/ocr.py on the shots instead.
import sys
import os
import time
import json
import csv
import struct
import shutil
import subprocess
from PIL import Image

WINE_EXE = "/lab/SIMTOWER.EXE"
WINE_PREFIX = "/lab/wine32"

TRUTH = {
    "$": ["      ", "  ##  ", "##### ", "######", "####  ", "####  ", "  ####", "  ## #", "######", "######", "  ##  ", "  ##  "],
    "$_alt": ["      ", "      ", "##### ", "##    ", "##    ", "##    ", "   ## ", "     #", "##   #", "##### ", "      ", "  ##  "],
    "0": ["      ", "  ### ", " #  ##", "#    #", "#    #", "#    #", "#    #", "#    #", " #  ##", "  ### ", "      ", "      "],
    "1": ["     ", "###  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####", "     ", "     "],
    "1_wide": ["      ", "####  ", "## #  ", "## #  ", "## #  ", "## #  ", "   #  ", "   #  ", "## #  ", "######", "      ", "  ##  "],
    "2": ["      ", " ###  ", "#   # ", "     #", "    ##", "    # ", "   #  ", "  #   ", " #    ", "######", "      ", "      "],
    "3": ["      ", " #### ", "    ##", "     #", "    ##", "  ### ", "     #", "     #", "#    #", " #### ", "      ", "      "],
    "4": ["      ", "   ## ", "   ## ", "  # # ", " #  # ", " #  # ", "#   # ", "######", "    # ", "    # ", "      ", "      "],
    "5": ["      ", " #### ", "      ", "      ", " ###  ", "    ##", "     #", "     #", "#   ##", " ###  ", "      ", "      "],
    "6": ["      ", "  ### ", " #    ", "#     ", "# ### ", "##   #", "#    #", "#    #", " #   #", "  ### ", "      ", "      "],
    "7": ["      ", "######", "     #", "    # ", "    # ", "   #  ", "   #  ", "   #  ", "  #   ", "  #   ", "      ", "      "],
    "8": ["      ", " #### ", "##   #", "#    #", " #   #", " #### ", "##   #", "#    #", "#    #", " #### ", "      ", "      "],
    "9": ["      ", " ###  ", "##  ##", "#    #", "#    #", "##  ##", " ### #", "     #", "    # ", " ###  ", "      ", "      "],
}

def decode_crop(crop):
    w, h = crop.size
    cols = [any(sum(crop.getpixel((x, y))[:3]) < 300 for y in range(h)) for x in range(w)]
    glyphs = []
    in_g = False
    st = 0
    for x in range(w):
        if cols[x] and not in_g:
            in_g = True
            st = x
        elif not cols[x] and in_g:
            in_g = False
            if x - st >= 2:
                glyphs.append((st, x))
    if in_g and w - st >= 2:
        glyphs.append((st, w))
    
    res = ""
    for s, e in glyphs:
        gw = e - s
        rows = ["".join("#" if (sum(crop.getpixel((s + x, y))[:3]) < 300) else " " for x in range(gw)) for y in range(h)]
        best_ch = None
        best_diff = 999
        for ch_key, pat in TRUTH.items():
            diff = 0
            for y in range(min(len(pat), len(rows))):
                pr = pat[y]
                cr = rows[y]
                for x in range(max(gw, len(pr))):
                    pv = pr[x] if x < len(pr) else " "
                    cv = cr[x] if x < len(cr) else " "
                    if pv != cv:
                        diff += 1
            if diff < best_diff:
                best_diff = diff
                best_ch = ch_key.split("_")[0]
        if best_ch is not None and best_diff <= 16:
            res += best_ch
    return res

def count_stars(im):
    crop = im.crop((250, 18, 370, 36))
    w, h = crop.size
    yellow_cols = [x for x in range(w) if any(crop.getpixel((x, y))[0] > 180 and crop.getpixel((x, y))[1] > 180 and crop.getpixel((x, y))[2] < 120 for y in range(h))]
    if not yellow_cols:
        return 0
    clusters = 1
    for i in range(1, len(yellow_cols)):
        if yellow_cols[i] - yellow_cols[i-1] > 3:
            clusters += 1
    return clusters

def extract_oracle_metrics(out_dir, days):
    print(f"[oracle] Extracting metrics for {days} day(s) from {out_dir}...")
    metrics = []
    for d in range(1, days + 1):
        day_metrics = {
            "day": d,
            "funds": 0,
            "population": 0,
            "rating": 1,
            "stressed_people": 0,
            "elevator_avg_wait": 0.0,
            "items": {}
        }
        
        # 1. Read balance directly from day{d}.TDT header (stored balance * 100)
        day_tdt = os.path.join(out_dir, f"day{d}.TDT")
        if os.path.exists(day_tdt) and os.path.getsize(day_tdt) >= 8:
            try:
                with open(day_tdt, "rb") as f:
                    f.seek(4)
                    bal_raw = struct.unpack("<I", f.read(4))[0]
                    day_metrics["funds"] = bal_raw * 100
            except Exception as e:
                print(f"[oracle] Warning reading {day_tdt}: {e}")

        # 2. OCR from screenshots
        shot_h12 = os.path.join(out_dir, f"shot_d{d}_h12.png")
        shot_h23 = os.path.join(out_dir, f"shot_d{d}_h23.png")

        for shot_path in [shot_h23, shot_h12]:
            if os.path.exists(shot_path):
                try:
                    im = Image.open(shot_path)
                    if im.size[0] == 1024 and im.size[1] == 768:
                        # Funds crop
                        if day_metrics["funds"] == 0:
                            f_str = decode_crop(im.crop((548, 21, 635, 33))).replace("$", "").replace("-", "")
                            if f_str.isdigit():
                                day_metrics["funds"] = int(f_str)
                        # Pop crop
                        p_str = decode_crop(im.crop((550, 38, 635, 50))).replace("$", "").replace("-", "")
                        if p_str.isdigit():
                            pval = int(p_str)
                            if pval > day_metrics["population"]:
                                day_metrics["population"] = pval
                        # Stars
                        stars = count_stars(im)
                        if stars > 0:
                            day_metrics["rating"] = stars
                except Exception as e:
                    print(f"[oracle] Warning OCRing {shot_path}: {e}")

        metrics.append(day_metrics)
        f_val = day_metrics["funds"]
        p_val = day_metrics["population"]
        r_val = day_metrics["rating"]
        print(f"[oracle] Day {d} parsed: funds={f_val}, pop={p_val}, rating={r_val}")

    json_path = os.path.join(out_dir, "metrics.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(metrics, f, indent=2)
    print(f"[oracle] Wrote {json_path}")

    csv_path = os.path.join(out_dir, "metrics.csv")
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["day", "funds", "population", "rating", "stressed_people", "elevator_avg_wait"])
        for m in metrics:
            writer.writerow([m["day"], m["funds"], m["population"], m["rating"], m["stressed_people"], m["elevator_avg_wait"]])
    print(f"[oracle] Wrote {csv_path}")
    return metrics


# ---- modal dialog watchdog ---------------------------------------------------------------------
# SimTower pops modal dialogs (the "Congratulations ... Tower" rating promotion, VIP visits,
# disasters, the quarterly report, ...). While one is open the sim is frozen, so the run used to
# stall (day 3 hour 06). Under `wine explorer /desktop=` every Win16 window is a real X window, so
# a dialog shows up as a NEW visible X window next to the fixed set of main windows (desktop,
# tool/time/map windows, the "SimTower - ACTIVE" frame). The watchdog diffs that set, presses Enter
# (the default button is OK), falls back to clicking the known OK position and Escape, and logs
# every dismissal with a screenshot (dialog_NNN_*.png) into the run dir.
class DialogWatchdog:
    OK_FALLBACKS = [(502, 233)]          # "Tower" congratulations dialog OK button (1024x768)

    def __init__(self, env, out_dir):
        self.env = env
        self.out_dir = out_dir
        self.baseline = set()
        self.ignored = set()             # windows that survived every dismissal attempt
        self.count = 0
        self.log_path = os.path.join(out_dir, "dialogs.log")

    def _windows(self):
        r = subprocess.run(["xdotool", "search", "--onlyvisible", "--name", ""], env=self.env,
                           capture_output=True, text=True)
        return set(r.stdout.split())

    def _describe(self, wid):
        name = subprocess.run(["xdotool", "getwindowname", wid], env=self.env, capture_output=True, text=True).stdout.strip()
        geo = subprocess.run(["xdotool", "getwindowgeometry", wid], env=self.env, capture_output=True, text=True).stdout.replace("\n", " ")
        return f"{wid} name={name!r} {geo.strip()}"

    def _log(self, msg):
        line = time.strftime("%H:%M:%S ") + msg
        print("[oracle] " + line, flush=True)
        with open(self.log_path, "a") as f:
            f.write(line + "\n")

    def arm(self):
        """Record the main-window set (call once the game is on the main screen)."""
        self.baseline = self._windows()
        self._log(f"watchdog armed, {len(self.baseline)} baseline windows: {sorted(self.baseline)}")

    def check(self, tag=""):
        """Dismiss any modal dialog now open. Returns the number of dialogs dismissed."""
        dismissed = 0
        for _ in range(6):               # a dialog can be followed by another one
            new = self._windows() - self.baseline - self.ignored
            if not new:
                break
            self.count += 1
            n = self.count
            desc = "; ".join(self._describe(w) for w in sorted(new))
            shot = os.path.join(self.out_dir, f"dialog_{n:03d}_{tag}_before.png")
            subprocess.run(["import", "-window", "root", shot], env=self.env)
            self._log(f"dialog #{n} ({tag}): new windows: {desc}")
            gone = False
            for action in ("Return", "click", "Escape"):
                if action == "Return":
                    subprocess.run(["xdotool", "key", "Return"], env=self.env)
                elif action == "click":
                    for (x, y) in self.OK_FALLBACKS:
                        subprocess.run(["xdotool", "mousemove", str(x), str(y), "click", "1"], env=self.env)
                else:
                    subprocess.run(["xdotool", "key", "Escape"], env=self.env)
                time.sleep(1.0)
                if not (self._windows() & new):
                    gone = True
                    self._log(f"dialog #{n} dismissed with {action}")
                    break
            subprocess.run(["import", "-window", "root", os.path.join(self.out_dir, f"dialog_{n:03d}_{tag}_after.png")], env=self.env)
            if gone:
                dismissed += 1
            else:
                self._log(f"dialog #{n} NOT dismissed; ignoring windows {sorted(new)}")
                self.ignored |= new
        return dismissed

    def wait(self, seconds, tag=""):
        """sleep(seconds) in 1 s slices, checking for dialogs between slices."""
        end = time.time() + seconds
        while True:
            self.check(tag)
            left = end - time.time()
            if left <= 0:
                break
            time.sleep(min(1.0, left))


# ---- camera: drag the vertical scrollbar thumb of the "SimTower - ACTIVE" window ----------------
# (x=1007; thumb centre ~y=125 at the top of the tower). Calibrated on SCHMITT.TDT at 1024x768:
# 565 = ground lobby row in view with hotel floors above it, 330 = mid hotel floors, 125 = roof /
# cathedral. The sim keeps the camera where it is put, so the hourly shots are taken in the lobby view.
SCROLL_X = 1007
# The oracle advances by wall clock (~1 game day per 20 shots at 1.0 s); a longer interval lets a
# 3x24-shot run reach the later game days (and the Tower promotion dialog on the weekend).
SHOT_INTERVAL = float(os.environ.get("ORACLE_SHOT_INTERVAL", "1.6"))
VIEW_Y = {"cathedral": 125, "floors": 330, "lobby": 565}

class Camera:
    def __init__(self, env, wd):
        self.env, self.wd, self.y = env, wd, 125

    def go(self, name):
        target = VIEW_Y[name]
        if abs(target - self.y) < 4:
            return
        self.wd.check("camera")          # a modal dialog would swallow the drag
        e = self.env
        subprocess.run(["xdotool", "mousemove", str(SCROLL_X), str(self.y), "mousedown", "1"], env=e)
        time.sleep(0.3)
        mid = (self.y + target) // 2
        subprocess.run(["xdotool", "mousemove", str(SCROLL_X), str(mid)], env=e)
        time.sleep(0.3)
        subprocess.run(["xdotool", "mousemove", str(SCROLL_X), str(target)], env=e)
        time.sleep(0.3)
        subprocess.run(["xdotool", "mouseup", "1"], env=e)
        time.sleep(0.8)
        self.y = target

def start_xvfb(display=":99"):
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

        wd = DialogWatchdog(env, out_dir)
        wd.arm()
        cam = Camera(env, wd)

        # Click Fast Speed button (x=299, y=504)
        print("[oracle] Activating Fast Speed simulation...")
        subprocess.run(["xdotool", "mousemove", "299", "504", "click", "1"], env=env)
        time.sleep(1.0)
        cam.go("lobby")

        for d in range(1, days + 1):
            print(f"[oracle] Simulating Day {d}/{days} (24 hourly captures)...")
            for h in range(24):
                wd.check(f"d{d}h{h:02d}")
                shot_name = os.path.join(out_dir, f"shot_d{d}_h{h:02d}.png")
                subprocess.run(["import", "-window", "root", shot_name], env=env)
                wd.wait(SHOT_INTERVAL, f"d{d}h{h:02d}")
                if h == 12:
                    # per-day camera views: cathedral (roof), hotel floors, ground lobby
                    for view in ("cathedral", "floors", "lobby"):
                        cam.go(view)
                        subprocess.run(["import", "-window", "root", os.path.join(out_dir, f"view_d{d}_h12_{view}.png")], env=env)
            
            # Save state at end of day
            print(f"[oracle] Saving tower state at end of day {d}...")
            wd.check(f"d{d}save")
            subprocess.run(["xdotool", "key", "alt+f"], env=env)
            time.sleep(0.5)
            subprocess.run(["xdotool", "key", "s"], env=env)
            time.sleep(2.0)
            wd.check(f"d{d}saved")

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

    # Extract trustworthy metrics from screenshots and TDT
    extract_oracle_metrics(out_dir, days)
    print(f"[oracle] Playthrough simulation complete for {days} day(s).")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: oracle_sim.py probe <out_dir> | oracle_sim.py run <tdt_file> <days> <out_dir> | oracle_sim.py ocr <out_dir> [days]")
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
    elif mode == "ocr":
        out = sys.argv[2]
        days = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        extract_oracle_metrics(out, days)
    else:
        print(f"Unknown mode: {mode}")
        sys.exit(1)
