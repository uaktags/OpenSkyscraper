#!/usr/bin/env bash
# Parity campaign driver (laptop side): runs pipeline.py in the lab container.
#
#   parity.sh run TAG DAYS TOWER...   one pipeline per tower (repo-relative .tower paths), in
#                                     parallel, each with its own oracle instance (parity-TAG-N);
#                                     output in /lab/out/parity/TAG/<tower name>/
#   parity.sh status TAG              last oracle line per tower, and whether it finished
#   parity.sh pull TAG [DEST]         copy the results (without snapshots) to DEST
#                                     (default ~/yoottower-decomp/parity-agent/runs/TAG)
#   parity.sh report TAG              print every tower's report.md
#   parity.sh stop TAG                kill the pipelines and their oracle instances
#
# Sync and build the tree first: lab.sh sync <worktree> $LAB_TREE; lab.sh build $LAB_TREE.
# Extra pipeline.py options go in PIPE_OPTS (e.g. "--skip-oracle" to rerun OpenSky only).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LAB="bash $HERE/../lab.sh"
T=${LAB_TREE:-parity}
SRC=/lab/src/$T
cmd=${1:-}; shift || true
case $cmd in
run)
    tag=${1:?tag}; days=${2:?days}; shift 2
    n=0
    for tw in "$@"; do
        name=$(basename "$tw" .tower); n=$((n + 1))
        o=/lab/out/parity/$tag/$name
        $LAB exec "mkdir -p $o && cd $SRC && nohup python3 tools/yoottower/remote/parity/pipeline.py $tw $days $o \
            --inst parity-$tag-$n ${PIPE_OPTS:-} > $o/pipeline.log 2>&1 < /dev/null & echo started $name"
    done ;;
status)
    tag=${1:?tag}
    $LAB exec "for d in /lab/out/parity/$tag/*/; do n=\$(basename \$d); \
        printf '%-28s %s | %s\n' \$n \"\$(tail -1 \$d/oracle/oracle.log 2>/dev/null)\" \
        \"\$(test -f \$d/report.md && echo done || echo running)\"; done" ;;
pull)
    tag=${1:?tag}; dest=${2:-$HOME/yoottower-decomp/parity-agent/runs/$tag}
    mkdir -p "$dest"
    $LAB exec "cd /lab/out/parity && tar czf /lab/out/parity/$tag.tgz --exclude=snaps --exclude=dumps $tag"
    $LAB pull "out/parity/$tag.tgz" "$dest/../$tag.tgz"
    tar xzf "$dest/../$tag.tgz" -C "$dest/.." && rm "$dest/../$tag.tgz"
    echo "pulled to $dest" ;;
report)
    tag=${1:?tag}
    $LAB exec "cat /lab/out/parity/$tag/*/report.md" ;;
stop)
    tag=${1:?tag}
    $LAB exec "pkill -f 'pipeline.py.*/parity/$tag/' || true; pkill -f 'oracle_run.py.*parity-$tag-' || true; \
        for i in /lab/oracle/instances/parity-$tag-*; do [ -d \$i ] && YOOT_INSTANCE=\$(basename \$i) \
        bash $SRC/tools/yoottower/oracle/run_yoot.sh stop; done; true" ;;
*) sed -n '2,17p' "$0"; exit 1 ;;
esac
