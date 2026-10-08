# Remote lab (heavy runs off the laptop)

`tools/yoottower/remote/lab.sh` runs OpenSky builds, tests, `--sim` runs and the
YootTower Wine oracle inside one long-running container (`yoottower-lab`) on a
remote Docker host, by default the Unraid server (`ssh -p 2223 root@192.168.4.3`).
Prefer it to local runs for anything long: oracle sessions, multi-day `--sim`
comparisons, full rebuilds.

## Layout on the host

`LAB_DIR` (default `/mnt/cache/appdata/yoottower-lab`, on the cache pool, not the
array) is mounted at `/lab` in the container:

| Path | Content |
|------|---------|
| `/lab/image/` | Dockerfile used to build `yoottower-lab:latest` |
| `/lab/src/<name>/` | source trees synced with `lab.sh sync` (each gets its own `build/`) |
| `/lab/game-src/YOOT/` | the original game, read-only input for the oracle (`lab.sh seed`) |
| `/lab/mfc42.dll` | VC6 runtime the game needs |
| `/lab/oracle/instances/<name>/` | per-`YOOT_INSTANCE` Wine prefix, game copy, logs |
| `/lab/out/` | scratch for artifacts to `pull` back |

The container is capped with `--cpus 16 --memory 16g` (`LAB_CPUS`, `LAB_MEM`).

## Typical use

```sh
L=tools/yoottower/remote/lab.sh
$L image && $L up && $L seed           # once
$L sync                                # repo root -> /lab/src/OpenSky
$L sync .claude/worktrees/agent-x wt-x # or a worktree under its own name
$L build OpenSky && $L test OpenSky
$L sim OpenSky --ruleset yoottower --load-game /lab/out/t.tower --sim 3d --sim-out /lab/out/o.tower
$L oracle people start "Sample Tokyo Map.bld"
$L oracle people shot /lab/out/shot.png
$L oracle people stop
$L pull out/ ~/yoottower-decomp/lab-out/
```

`oracle` runs `run_yoot.sh` from `/lab/src/$LAB_TREE` (default `OpenSky`), so sync
that tree first. Always pass a distinct instance name per concurrent user.

## Round trip through the original (bld_roundtrip.py)

`tools/yoottower/remote/bld_roundtrip.py` checks that an OpenSky tower survives
a trip through the original game. It runs inside the lab container:

```sh
L=tools/yoottower/remote/lab.sh
$L sync . save && $L build save
$L exec python3 /lab/src/save/tools/yoottower/remote/bld_roundtrip.py \
    --tree /lab/src/save --out /lab/out/roundtrip [--days 1] [NAME=TOWER[:TEMPLATE] ...]
```

Per case (default: Tokyo people, Tokyo visitors, Tokyo bridge, Hawaii, Kegon):

1. `BldTool export` writes the tower as a `.bld` against the world's template save,
   and `BldTool import` reads it back (the expected tower).
2. The oracle (instance `rt-NAME`) loads the `.bld`, runs one game day at full speed,
   and saves through File > Save As.
3. The game's save must rewrite byte-identical through `bld.py`, and OpenSky must
   load it (`OpenSkyscraper --load-game ... --sim 1`). Checks: items (no expected
   item missing; extra items are reported), funds, and population (the game's pool
   population must match the re-imported pool and OpenSky's people count).

If the original shows a "Plugin Check Error" dialog for the export, the case fails at
`load` with a screenshot in the case directory.
