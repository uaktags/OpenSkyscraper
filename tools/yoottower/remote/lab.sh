#!/usr/bin/env bash
# Run OpenSky builds/tests/--sim and the YootTower Wine oracle on a remote
# Docker host (default: the Unraid server) instead of this machine.
#
#   lab.sh image                      build the lab image on the host (Dockerfile next to this script)
#   lab.sh up | down | status         start/stop the long-running lab container
#   lab.sh seed                       one-time: copy the original game + mfc42.dll to the host
#   lab.sh sync [TREE] [NAME]         rsync a source tree (default: repo root) to /lab/src/NAME
#   lab.sh build NAME                 cmake configure + build inside the container
#   lab.sh test NAME                  OpenSkyTests + ctest (under xvfb-run)
#   lab.sh sim NAME ARGS...           xvfb-run OpenSkyscraper ARGS... (paths under /lab or relative to the tree)
#   lab.sh oracle INSTANCE ARGS...    tools/yoottower/oracle/run_yoot.sh ARGS... with YOOT_INSTANCE=INSTANCE
#   lab.sh exec CMD...                arbitrary command in the container (cwd /lab)
#   lab.sh push LOCAL REMOTE          copy a local file/dir to the host's /lab/REMOTE
#   lab.sh pull REMOTE LOCAL          copy /lab/REMOTE back to LOCAL
#
# Environment: LAB_SSH (default "ssh -p 2223 root@192.168.4.3"), LAB_DIR on the host
# (default /mnt/cache/appdata/yoottower-lab), LAB_CPUS (16), LAB_MEM (16g).
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)
LAB_SSH=${LAB_SSH:-"ssh -o BatchMode=yes -p 2223 root@192.168.4.3"}
LAB_DIR=${LAB_DIR:-/mnt/cache/appdata/yoottower-lab}
LAB_CPUS=${LAB_CPUS:-16}
LAB_MEM=${LAB_MEM:-16g}
IMAGE=yoottower-lab:latest
CTR=yoottower-lab
read -r -a SSH <<<"$LAB_SSH"
HOST=${SSH[-1]}
PORT=22
for ((i = 0; i < ${#SSH[@]}; i++)); do [[ ${SSH[i]} == -p ]] && PORT=${SSH[i + 1]}; done
RSYNC=(rsync -az --delete -e "ssh -o BatchMode=yes -p $PORT")

die() { echo "lab: $*" >&2; exit 1; }
remote() { "${SSH[@]}" "$@"; }
q() { printf '%q ' "$@"; }
in_ctr() { remote "docker exec -i -w /lab $CTR bash -lc $(printf '%q' "$*")"; }

cmd=${1:-}; shift || true
case $cmd in
image)
    remote "mkdir -p $LAB_DIR/image"
    "${RSYNC[@]}" "$HERE/Dockerfile" "$HOST:$LAB_DIR/image/"
    remote "cd $LAB_DIR/image && docker build -t $IMAGE ." ;;
up)
    remote "mkdir -p $LAB_DIR/src $LAB_DIR/oracle $LAB_DIR/out"
    remote "docker inspect $CTR >/dev/null 2>&1 && docker start $CTR >/dev/null || \
        docker run -d --name $CTR --restart unless-stopped --cpus $LAB_CPUS --memory $LAB_MEM \
            --shm-size 1g -v $LAB_DIR:/lab $IMAGE >/dev/null"
    echo "lab container up" ;;
down) remote "docker rm -f $CTR >/dev/null 2>&1 || true"; echo "lab container removed" ;;
status) remote "docker ps -a --filter name=^/$CTR\$ --format '{{.Names}} {{.Status}}'; du -sh $LAB_DIR 2>/dev/null" ;;
seed)
    src=${YOOT_SRC:-$HOME/Downloads/Yoot-Tower_Win_EN/YOOT}
    [[ -f $src/T2.exe ]] || die "original game not found at $src"
    remote "mkdir -p $LAB_DIR/game-src"
    "${RSYNC[@]}" "$src/" "$HOST:$LAB_DIR/game-src/YOOT/"
    m=${MFC42_DLL:-$HOME/.var/app/com.usebottles.bottles/data/bottles/bottles/YootTower/drive_c/windows/system32/mfc42.dll}
    [[ -f $m ]] || die "mfc42.dll not found (set MFC42_DLL)"
    "${RSYNC[@]}" "$m" "$HOST:$LAB_DIR/mfc42.dll"
    echo "seeded game + mfc42.dll" ;;
sync)
    tree=$(cd "${1:-$REPO}" && pwd); name=${2:-$(basename "$tree")}
    [[ $name =~ ^[A-Za-z0-9._-]+$ ]] || die "bad tree name: $name"
    remote "mkdir -p $LAB_DIR/src/$name"
    "${RSYNC[@]}" --exclude /build/ --exclude /.git/ --exclude /.claude/ --exclude __pycache__/ \
        --exclude '/debug-*.log' "$tree/" "$HOST:$LAB_DIR/src/$name/"
    echo "synced $tree -> /lab/src/$name" ;;
build)
    name=${1:?tree name}
    in_ctr "cd /lab/src/$name && cmake -S . -B build -DCMAKE_BUILD_TYPE=\${BUILD_TYPE:-Debug} >/dev/null && cmake --build build -j$LAB_CPUS 2>&1 | grep -E 'error|warning: unused|Built target' | tail -20" ;;
test)
    name=${1:?tree name}
    in_ctr "cd /lab/src/$name/build && ./OpenSkyTests 2>/dev/null | tail -1 && xvfb-run -a ctest 2>&1 | tail -3" ;;
sim)
    name=${1:?tree name}; shift
    in_ctr "cd /lab/src/$name && xvfb-run -a -s '-screen 0 1280x800x24' build/OpenSkyscraper $(q "$@")" ;;
oracle)
    # Oracle scripts run from a synced tree: LAB_TREE (default "OpenSky", the repo root's name).
    inst=${1:?instance}; shift
    in_ctr "test -x /lab/src/${LAB_TREE:-OpenSky}/tools/yoottower/oracle/run_yoot.sh || { echo 'lab: sync a tree first' >&2; exit 1; }; YOOT_INSTANCE=$(printf '%q' "$inst") /lab/src/${LAB_TREE:-OpenSky}/tools/yoottower/oracle/run_yoot.sh $(q "$@")" ;;
exec) in_ctr "$*" ;;
push) "${RSYNC[@]}" "${1:?local}" "$HOST:$LAB_DIR/${2:?remote}" ;;
pull) "${RSYNC[@]}" "$HOST:$LAB_DIR/${1:?remote}" "${2:?local}" ;;
*) sed -n '2,20p' "$0"; exit 1 ;;
esac
