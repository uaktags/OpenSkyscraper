#!/usr/bin/env bash
# SimTower 16-bit Oracle Headless Container Harness
# Runs original 16-bit SIMTOWER.EXE under 32-bit Wine on Unraid server
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLAY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LAB_HOST="${LAB_HOST:-192.168.4.3}"
LAB_PORT="${LAB_PORT:-2223}"
LAB_USER="${LAB_USER:-root}"
LAB_DIR="${LAB_DIR:-/mnt/cache/appdata/opensky-play}"
LAB_RSH="ssh -o BatchMode=yes -p $LAB_PORT"
LAB_SSH="$LAB_RSH $LAB_USER@$LAB_HOST"
LAB_CPUS="${LAB_CPUS:-4}"
LAB_MEM="${LAB_MEM:-4g}"
IMAGE="${IMAGE:-opensky-oracle:latest}"
CTR="${CTR:-opensky-oracle}"

in_ctr() {
  local cmd="$*"
  $LAB_SSH "docker exec -i -u lab -w /lab $CTR bash -lc $(printf %q "$cmd")"
}

sync_oracle_tools() {
  echo "[oracle] Syncing helper scripts to Unraid lab..."
  $LAB_SSH "mkdir -p $LAB_DIR/tools $LAB_DIR/data $LAB_DIR/out/oracle32"
  rsync -avz -e "$LAB_RSH" \
    "$SCRIPT_DIR/oracle_sim.py" \
    "$LAB_USER@$LAB_HOST:$LAB_DIR/tools/"
  
  if [ -f "/home/tim/Projects/OpenSky/SIMTOWER.EXE" ]; then
    rsync -avz -e "$LAB_RSH" "/home/tim/Projects/OpenSky/SIMTOWER.EXE" "$LAB_USER@$LAB_HOST:$LAB_DIR/SIMTOWER.EXE"
  fi
}

cmd_image() {
  echo "[oracle] Building container image $IMAGE on $LAB_HOST..."
  $LAB_SSH "mkdir -p $LAB_DIR/docker-oracle"
  rsync -avz -e "$LAB_RSH" \
    "$SCRIPT_DIR/Dockerfile" \
    "$SCRIPT_DIR/entrypoint.sh" \
    "$SCRIPT_DIR/SIMTOWER.INI" \
    "$SCRIPT_DIR/WAVEMIX.INI" \
    "$SCRIPT_DIR/WavMix16.DLL" \
    "$SCRIPT_DIR/WAVMIX16.DLL" \
    "$LAB_USER@$LAB_HOST:$LAB_DIR/docker-oracle/"
  $LAB_SSH "docker build -t $IMAGE $LAB_DIR/docker-oracle"
  echo "[oracle] Image $IMAGE built successfully."
}

cmd_up() {
  echo "[oracle] Starting oracle container $CTR..."
  sync_oracle_tools
  if $LAB_SSH "docker ps -q -f name=^/${CTR}$ | grep -q ."; then
    echo "[oracle] Container $CTR is already running."
    return 0
  fi
  if $LAB_SSH "docker ps -a -q -f name=^/${CTR}$ | grep -q ."; then
    echo "[oracle] Removing existing stopped container $CTR..."
    $LAB_SSH "docker rm -f $CTR"
  fi
  $LAB_SSH "docker run -d --name $CTR \
    --restart=unless-stopped \
    --cpus $LAB_CPUS --memory $LAB_MEM --shm-size 2g \
    -v $LAB_DIR:/lab \
    $IMAGE sleep infinity"
  echo "[oracle] Container $CTR is up and running."
}

cmd_down() {
  echo "[oracle] Stopping and removing container $CTR..."
  $LAB_SSH "docker rm -f $CTR" || true
  echo "[oracle] Container $CTR stopped."
}

cmd_probe() {
  echo "[oracle] Running headless probe test on container $CTR..."
  sync_oracle_tools
  $LAB_SSH "docker exec -i -u lab -w /lab $CTR python3 /lab/tools/oracle_sim.py probe /lab/out/oracle32"
  echo "[oracle] Probe passed successfully! Screenshot generated at /lab/out/oracle32/probe.png"
}

resolve_tdt_path() {
  local tower="$1"
  # If local path exists, sync it
  if [ -f "$tower" ]; then
    local base="$(basename "$tower")"
    rsync -az -e "$LAB_RSH" "$tower" "$LAB_USER@$LAB_HOST:$LAB_DIR/data/$base" >&2
    echo "/lab/data/$base"
    return
  fi
  # Check common candidate locations inside container
  local candidates=(
    "$tower"
    "/lab/data/$tower"
    "/lab/data/$(basename "$tower")"
  )
  for c in "${candidates[@]}"; do
    if in_ctr "[ -f '$c' ]"; then
      echo "$c"
      return
    fi
  done
  echo "$tower"
}

cmd_run() {
  local tdt_arg="$1"
  local days="${2:-1}"
  if [ -z "$tdt_arg" ]; then
    echo "Usage: oracle.sh run <TDT_FILE> [DAYS]"
    exit 1
  fi

  sync_oracle_tools
  local tdt_path="$(resolve_tdt_path "$tdt_arg")"
  local name="$(basename "$tdt_arg")"
  name="${name%.*}"

  echo "[oracle] Starting simulation: tower=$tdt_path, days=$days, output=/lab/out/oracle32/$name"
  $LAB_SSH "docker exec -i -u lab -w /lab $CTR python3 /lab/tools/oracle_sim.py run '$tdt_path' '$days' '/lab/out/oracle32/$name'"
  echo "[oracle] Simulation completed successfully for $name ($days day(s))."
}

cmd_pull() {
  local name="$1"
  if [ -z "$name" ]; then
    echo "Usage: oracle.sh pull <NAME>"
    exit 1
  fi

  local dest="$PLAY_DIR/out/oracle32/$name"
  mkdir -p "$dest"
  echo "[oracle] Pulling results for '$name' to $dest..."
  rsync -avz -e "$LAB_RSH" "$LAB_USER@$LAB_HOST:$LAB_DIR/out/oracle32/$name/" "$dest/"

  # Downscale pulled screenshots to max 640x400 and prune to max 48 images
  if [ -f "$PLAY_DIR/downscale_pull.py" ]; then
    python3 "$PLAY_DIR/downscale_pull.py" "$dest" 48 640 400
  fi
  echo "[oracle] Pulled and processed results for $name."
}

usage() {
  echo "Usage: oracle.sh <subcommand> [args...]"
  echo "Subcommands:"
  echo "  image                     Build opensky-oracle:latest Docker image"
  echo "  up                        Start opensky-oracle container on Unraid"
  echo "  down                      Stop and remove opensky-oracle container"
  echo "  probe                     Verify SimTower executes headlessly and takes screenshot"
  echo "  run <TDT_FILE> [DAYS]     Run headless SimTower simulation on TDT for N days"
  echo "  pull <NAME>               Pull run results locally and downscale screenshots"
  exit 1
}

case "${1:-}" in
  image)      cmd_image ;;
  up)         cmd_up ;;
  down)       cmd_down ;;
  probe)      cmd_probe ;;
  run)        shift; cmd_run "$@" ;;
  pull)       shift; cmd_pull "$@" ;;
  *)          usage ;;
esac
