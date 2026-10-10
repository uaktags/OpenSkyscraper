#!/usr/bin/env bash
# OpenSky Playthrough Harness (WP6)
# Repeatable scripted runs for OpenSky C++ and Web ports with metrics & screenshots
set -eo pipefail
#
# METRIC METHOD (three sources, see README "Metric method"):
#   C++ headless  : `cpp` runs --sim Nd, values come from the SimHarness summary JSON (exact state;
#                   rating is 0-based, stars = rating+1); hourly state via --sim-dump-every 40 + cpp_hourly.py.
#                   Set PLAY_CPP_DUMP_EVERY=40 to retain dumps for each cpp checkpoint run.
#   C++ GUI shots : `cpp-shots` + extract_cpp_metrics.py -> ocr.py digit-template OCR of the time window.
#   Web           : web_drive.mjs logs game state AND the HUD DOM text/boxes per hour (hourly.json);
#                   ocr.py reads the HUD pixels with templates trained on the DOM text (cross-check).
#   Oracle        : oracle/oracle.sh run -> ocr.py on the 1024x768 SimTower HUD (templates labelled once).
#   ocr_validate.py / `ocr.py validate` hold the evidence; crops are kept in out/compare/ocr/.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

LAB_HOST="${LAB_HOST:-192.168.4.3}"
LAB_PORT="${LAB_PORT:-2223}"
LAB_USER="${LAB_USER:-root}"
LAB_DIR="${LAB_DIR:-/mnt/cache/appdata/opensky-play}"
LAB_RSH="ssh -o BatchMode=yes -p $LAB_PORT"
LAB_SSH="$LAB_RSH $LAB_USER@$LAB_HOST"
LAB_CPUS="${LAB_CPUS:-12}"
LAB_MEM="${LAB_MEM:-12g}"
IMAGE="${IMAGE:-opensky-play:latest}"
CTR="${CTR:-opensky-play}"

in_ctr() {
  local cmd="$*"
  $LAB_SSH "docker exec -i -w /lab $CTR bash -lc $(printf %q "$cmd")"
}

sync_tools() {
  echo "[play] Syncing playthrough helper tools to Unraid lab..."
  $LAB_SSH "mkdir -p $LAB_DIR/tools $LAB_DIR/data $LAB_DIR/out $LAB_DIR/src $LAB_DIR/wine"
  rsync -avz -e "$LAB_RSH" \
    "$SCRIPT_DIR/web_drive.mjs" \
    "$SCRIPT_DIR/compare.py" \
    "$SCRIPT_DIR/parse_tower_metrics.py" \
    "$SCRIPT_DIR/downscale_pull.py" \
    "$SCRIPT_DIR/extract_cpp_metrics.py" \
    "$SCRIPT_DIR/ocr.py" \
    "$SCRIPT_DIR/ocr_templates.json" \
    "$SCRIPT_DIR/cpp_hourly.py" \
    "$SCRIPT_DIR/dismiss_dialog.sh" \
    "$SCRIPT_DIR/cpp_view.sh" \
    "$SCRIPT_DIR/oracle_probe.sh" \
    "$LAB_USER@$LAB_HOST:$LAB_DIR/tools/"
  # Also sync SIMTOWER.EXE if present
  if [ -f "/home/tim/Projects/OpenSky/SIMTOWER.EXE" ]; then
    rsync -avz -e "$LAB_RSH" "/home/tim/Projects/OpenSky/SIMTOWER.EXE" "$LAB_USER@$LAB_HOST:$LAB_DIR/SIMTOWER.EXE"
  fi
}

cmd_image() {
  echo "[play] Building container image $IMAGE on $LAB_HOST..."
  $LAB_SSH "mkdir -p $LAB_DIR/docker"
  rsync -avz -e "$LAB_RSH" "$SCRIPT_DIR/Dockerfile" "$LAB_USER@$LAB_HOST:$LAB_DIR/docker/Dockerfile"
  $LAB_SSH "docker build -t $IMAGE $LAB_DIR/docker"
  echo "[play] Image $IMAGE built successfully."
}

cmd_up() {
  echo "[play] Starting lab container $CTR..."
  sync_tools
  if $LAB_SSH "docker ps -q -f name=^/${CTR}$ | grep -q ."; then
    echo "[play] Container $CTR is already running."
    return 0
  fi
  if $LAB_SSH "docker ps -a -q -f name=^/${CTR}$ | grep -q ."; then
    echo "[play] Removing existing stopped container $CTR..."
    $LAB_SSH "docker rm -f $CTR"
  fi
  $LAB_SSH "docker run -d --name $CTR \
    --restart=unless-stopped \
    --cpus $LAB_CPUS --memory $LAB_MEM --shm-size 2g \
    -v $LAB_DIR:/lab \
    $IMAGE sleep infinity"
  echo "[play] Container $CTR is up and running."
}

cmd_down() {
  echo "[play] Stopping and removing container $CTR..."
  $LAB_SSH "docker rm -f $CTR" || true
  echo "[play] Container $CTR stopped."
}

cmd_sync() {
  local tree="$1"
  local name="$2"
  if [ -z "$tree" ] || [ -z "$name" ]; then
    echo "Usage: play.sh sync <TREE_PATH> <NAME>"
    exit 1
  fi
  if [ ! -d "$tree" ]; then
    echo "Error: Tree directory does not exist: $tree"
    exit 1
  fi
  echo "[play] Syncing $tree to $LAB_DIR/src/$name..."
  $LAB_SSH "mkdir -p $LAB_DIR/src/$name"
  rsync -avz --delete -e "$LAB_RSH" \
    --exclude '.git' \
    --exclude 'build' \
    --exclude 'node_modules' \
    --exclude '.cache' \
    "$tree/" "$LAB_USER@$LAB_HOST:$LAB_DIR/src/$name/"
  echo "[play] Synced tree '$name'."
}

cmd_build() {
  local name="$1"
  if [ -z "$name" ]; then
    echo "Usage: play.sh build <NAME>"
    exit 1
  fi
  echo "[play] Building C++ tree '$name' with Ninja Release..."
  in_ctr "cd /lab/src/$name && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target OpenSkyscraper -j$LAB_CPUS"
  echo "[play] Built $name binary successfully."
}

resolve_tower_path() {
  local name="$1"
  local tower="$2"
  # If local path exists, sync it
  if [ -f "$tower" ]; then
    local base="$(basename "$tower")"
    rsync -az -e "$LAB_RSH" "$tower" "$LAB_USER@$LAB_HOST:$LAB_DIR/data/$base" >&2
    echo "/lab/data/$base"
    return
  fi
  # Check if inside container
  local candidates=(
    "$tower"
    "/lab/src/$name/$tower"
    "/lab/src/$name/scratch/concilia-tower/tests/fixtures/$(basename "$tower")"
    "/lab/src/$name/tests/parity/$(basename "$tower")"
    "/lab/src/$name/data/towers/$(basename "$tower")"
    "/lab/src/$name/data/$(basename "$tower")"
    "/lab/data/$(basename "$tower")"
  )
  for c in "${candidates[@]}"; do
    if in_ctr "[ -f '$c' ]"; then
      # absolute path: --load-game looks relative names up in ~/.openskyscraper first
      case "$c" in /*) echo "$c" ;; *) echo "/lab/$c" ;; esac
      return
    fi
  done
  # Fallback to direct path
  echo "$tower"
}

cmd_cpp() {
  local name="$1"
  local tower_arg="$2"
  local days="${3:-1}"
  if [ -z "$name" ] || [ -z "$tower_arg" ]; then
    echo "Usage: play.sh cpp <NAME> <TOWER> <DAYS>"
    exit 1
  fi

  sync_tools
  local tower="$(resolve_tower_path "$name" "$tower_arg")"
  echo "[play] Resolved tower path inside container: $tower"

  in_ctr "mkdir -p /lab/out/$name"
  echo "[play] Running C++ headless sim for $days day(s)..."

  in_ctr "python3 -c '
import os, sys, subprocess, json, csv

name = \"$name\"
tower = \"$tower\"
days = int(\"$days\")
dump_every = int(\"${PLAY_CPP_DUMP_EVERY:-0}\")
out_dir = f\"/lab/out/{name}\"
bin_path = f\"/lab/src/{name}/build/OpenSkyscraper\"

daily_metrics = []

for day in range(1, days + 1):
    day_tower = f\"{out_dir}/day{day}.tower\"
    day_json = f\"{out_dir}/day{day}_summary.json\"
    cmd = [
        \"xvfb-run\", \"-a\", \"-s\", \"-screen 0 1280x800x24\",
        bin_path,
        \"--load-game\", tower,
        \"--sim\", f\"{day}d\",
        \"--sim-out\", day_tower,
        \"--sim-summary\", day_json
    ]
    if dump_every > 0:
        dump_dir = f\"{out_dir}/day{day}_dumps\"
        os.makedirs(dump_dir, exist_ok=True)
        cmd += [\"--sim-dump-every\", str(dump_every), dump_dir]
    print(f\"[play] Simulating Day {day}/{days}: {\" \".join(cmd)}\")
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f\"Error running day {day}:\", res.stderr, file=sys.stderr)
        sys.exit(res.returncode)

    # Parse metrics
    parse_cmd = [\"python3\", \"/lab/tools/parse_tower_metrics.py\", day_tower, day_json, str(day)]
    parse_res = subprocess.run(parse_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if parse_res.returncode == 0:
        m = json.loads(parse_res.stdout)
        daily_metrics.append(m)
        print(f\"[play] Day {day}: funds=\${m[\"funds\"]} pop={m[\"population\"]} rating={m[\"rating\"]} stressed={m[\"stressed_people\"]} avg_wait={m[\"elevator_avg_wait\"]}\")
    else:
        print(f\"Parse error for day {day}: {parse_res.stderr}\")

# Write metrics.json
with open(f\"{out_dir}/metrics.json\", \"w\") as f:
    json.dump(daily_metrics, f, indent=2)

# Write metrics.csv
with open(f\"{out_dir}/metrics.csv\", \"w\") as f:
    writer = csv.writer(f)
    writer.writerow([\"day\", \"funds\", \"population\", \"rating\", \"stressed_people\", \"elevator_avg_wait\"])
    for m in daily_metrics:
        writer.writerow([m[\"day\"], m[\"funds\"], m[\"population\"], m[\"rating\"], m[\"stressed_people\"], m[\"elevator_avg_wait\"]])

# Copy last day as end.tower
if os.path.exists(f\"{out_dir}/day{days}.tower\"):
    import shutil
    shutil.copyfile(f\"{out_dir}/day{days}.tower\", f\"{out_dir}/end.tower\")
print(\"[play] C++ playthrough completed successfully.\")
'"
}

cmd_cpp_shots() {
  local name="$1"
  local tower_arg="$2"
  local days="${3:-1}"
  if [ -z "$name" ] || [ -z "$tower_arg" ]; then
    echo "Usage: play.sh cpp-shots <NAME> <TOWER> <DAYS>"
    exit 1
  fi

  sync_tools
  local tower="$(resolve_tower_path "$name" "$tower_arg")"
  echo "[play] Running C++ graphical playthrough with hourly screenshots under Xvfb..."

  in_ctr "mkdir -p /lab/out/${name}-shots && rm -f /lab/out/${name}-shots/*.png"
  # main menu (no --load-game): menu background shot
  in_ctr "xvfb-run -a -s \"-screen 0 1280x800x24\" bash -c '/lab/src/$name/build/OpenSkyscraper >/dev/null 2>&1 & P=\$!; sleep 9; import -window root /lab/out/${name}-shots/menu_cpp.png; kill \$P' || true"
  in_ctr "xvfb-run -a -s \"-screen 0 1280x800x24\" bash -c '
    out=\"/lab/out/${name}-shots\"
    bin=\"/lab/src/$name/build/OpenSkyscraper\"
    
    \$bin --load-game \"$tower\" >/lab/out/${name}-shots/gui.log 2>&1 &
    PID=\$!
    echo \"Launched OpenSkyscraper PID \$PID on \$DISPLAY\"
    sleep 3

    # Send key 3 to ensure speed is max
    xdotool key 3 || true
    # Camera on the ground lobby (cpp_view.sh: minimap click + Down clamp + Up presses)
    bash /lab/tools/cpp_view.sh goto lobby || true

    total_hours=\$(( $days * 24 ))
    echo \"Capturing \$total_hours hourly screenshots...\"
    for ((h=0; h<total_hours; h++)); do
      d=\$(( h / 24 + 1 ))
      hr=\$(( h % 24 ))
      shot_name=\$(printf \"shot_d%d_h%02d.png\" \$d \$hr)
      # Dismiss the modal Financial Report / rating dialog (Close button) so the sim keeps running
      bash /lab/tools/dismiss_dialog.sh
      import -window root \"\$out/\$shot_name\" || xwd -root -silent | convert xwd:- \"\$out/\$shot_name\" || true
      if [ \$hr -eq 8 ] || [ \$hr -eq 12 ] || [ \$hr -eq 18 ] || [ \$hr -eq 23 ]; then
        # per-day views: hotel floors, cathedral, ground lobby (hourly shots stay on the lobby)
        for v in floors cathedral lobby; do
          bash /lab/tools/cpp_view.sh goto \$v
          bash /lab/tools/dismiss_dialog.sh
          import -window root \"\$out/view_d\${d}_h\$(printf %02d \$hr)_\$v.png\" || true
        done
      fi
      sleep 1.8
    done

    kill \$PID || true
    echo \"Finished capturing screenshots.\"
  '"
  echo "[play] Graphical run finished."
  in_ctr "python3 /lab/tools/extract_cpp_metrics.py /lab/out/${name}-shots $days"
}

cmd_web() {
  local name="$1"
  local tower_arg="$2"
  local days="${3:-1}"
  if [ -z "$name" ]; then
    echo "Usage: play.sh web <NAME> [TOWER] [DAYS]"
    exit 1
  fi

  sync_tools
  local tower=""
  if [ -n "$tower_arg" ]; then
    tower="$(resolve_tower_path "$name" "$tower_arg")"
    # .TDT saves are loaded by web_drive.mjs through the web port's own importer
    # (src/core/tdt-parser.js), so the run exercises the current TDT import fix.
  fi

  echo "[play] Building and serving web port '$name'..."
  in_ctr "bash -c '
    cd /lab/src/$name
    # Install dependencies and build if needed
    if [ ! -d node_modules ]; then
      bun install
    fi
    bun run build || true
    
    # Kill any existing server on 3000 by exact process name (pkill -f with the script name
    # matched this very bash -c command line and aborted the run).
    pkill -x bun || true
    sleep 1

    # Start the dev/prod server detached, with stdin closed so docker exec can return
    mkdir -p /lab/out
    setsid nohup bun run server.js >/lab/out/web_server.log 2>&1 </dev/null &
    sleep 3
    curl -s -o /dev/null -w \"server HTTP %{http_code}\\n\" http://localhost:3000/ || true
  '"

  echo "[play] Driving web playthrough via Playwright..."
  local tower_param=""
  if [ -n "$tower" ]; then
    tower_param="--tower $tower"
  fi
  in_ctr "mkdir -p /lab/out/$name && rm -f /lab/out/$name/shot_*.png /lab/out/$name/view_*.png"
  in_ctr "bun /lab/tools/web_drive.mjs --url http://localhost:3000/?edition=opensky $tower_param --days $days --out /lab/out/$name"
  echo "[play] Web playthrough complete."
}

cmd_oracle() {
  sync_tools
  echo "[play] Running Oracle probe (SIMTOWER.EXE under Wine 11.0 Staging)..."
  in_ctr "bash /lab/tools/oracle_probe.sh"
}

cmd_compare() {
  local a="$1"
  local b="$2"
  if [ -z "$a" ] || [ -z "$b" ]; then
    echo "Usage: play.sh compare <RUN_A_OR_FILE> <RUN_B_OR_FILE>"
    exit 1
  fi
  python3 "$SCRIPT_DIR/compare.py" "$a" "$b"
}

cmd_pull() {
  local name="$1"
  if [ -z "$name" ]; then
    echo "Usage: play.sh pull <NAME>"
    exit 1
  fi
  local dest="$SCRIPT_DIR/out/$name"
  mkdir -p "$dest"
  echo "[play] Pulling results for '$name' to $dest..."
  rsync -avz -e "$LAB_RSH" "$LAB_USER@$LAB_HOST:$LAB_DIR/out/$name/" "$dest/"
  
  # Also check if shots directory exists
  if $LAB_SSH "[ -d '$LAB_DIR/out/${name}-shots' ]"; then
    mkdir -p "$SCRIPT_DIR/out/${name}-shots"
    rsync -avz -e "$LAB_RSH" "$LAB_USER@$LAB_HOST:$LAB_DIR/out/${name}-shots/" "$SCRIPT_DIR/out/${name}-shots/"
    python3 "$SCRIPT_DIR/downscale_pull.py" "$SCRIPT_DIR/out/${name}-shots" 48 640 400
  fi

  # Downscale pulled screenshots to 640x400 and prune to max 48 images
  python3 "$SCRIPT_DIR/downscale_pull.py" "$dest" 48 640 400
  echo "[play] Results pulled and downscaled successfully."
}

usage() {
  echo "Usage: play.sh <subcommand> [args...]"
  echo "Subcommands:"
  echo "  image                     Build opensky-play:latest Docker image"
  echo "  up                        Start opensky-play container on Unraid"
  echo "  down                      Stop and remove opensky-play container"
  echo "  sync <TREE> <NAME>        Rsync local source tree to /lab/src/<NAME>"
  echo "  build <NAME>              Build C++ target OpenSkyscraper via Ninja Release"
  echo "  cpp <NAME> <TOWER> <DAYS> Run headless C++ simulation and export metrics"
  echo "  cpp-shots <NAME> <TOWER> <DAYS> Run C++ GUI under Xvfb and capture hourly screenshots"
  echo "  web <NAME> [TOWER] [DAYS] Build/serve web port and drive via Playwright"
  echo "  oracle [TOWER] [DAYS]     Probe SIMTOWER.EXE under Wine in fresh prefix"
  echo "  compare <A> <B>           Compare two playthrough runs/metrics files"
  echo "  pull <NAME>               Pull run results locally, downscaled (<50MB, max 48 shots)"
  exit 1
}

case "${1:-}" in
  image)      cmd_image ;;
  up)         cmd_up ;;
  down)       cmd_down ;;
  sync)       shift; cmd_sync "$@" ;;
  build)      shift; cmd_build "$@" ;;
  cpp)        shift; cmd_cpp "$@" ;;
  cpp-shots)  shift; cmd_cpp_shots "$@" ;;
  web)        shift; cmd_web "$@" ;;
  oracle)     shift; cmd_oracle "$@" ;;
  compare)    shift; cmd_compare "$@" ;;
  pull)       shift; cmd_pull "$@" ;;
  *)          usage ;;
esac
