"""pipeline.py TOWER DAYS OUTDIR [--inst NAME] [--skip-oracle] [--skip-opensky] [--seed N]

One parity comparison inside the lab container (run from a synced, built tree):

1. BldTool export TOWER -> OUTDIR/input.bld, against the world's sample save
   (Tokyo: "Sample Tokyo Map.bld", Hawaii: "Yooooot.bld").
2. OpenSky: --ruleset yoottower --load-game input.bld --sim DAYSd, a dump every
   DUMP ticks, reduced to OUTDIR/opensky/series.jsonl.
3. The original: oracle_run.py on input.bld -> OUTDIR/oracle/series.jsonl.
4. compare.py -> OUTDIR/report.md and report.json.

Both games start from the same exported save, so OpenSky also goes through the
.bld import. Steps 2 and 3 run in parallel.
"""
import argparse
import glob
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..'))
sys.path.insert(0, HERE)
import compare  # noqa: E402
import metrics  # noqa: E402

GAME = os.environ.get('YOOT_GAME_SRC', '/lab/game-src/YOOT')
TEMPLATES = {'tokyo': 'Sample Tokyo Map.bld', 'hawaii': 'Yooooot.bld'}
DUMP = 20

ap = argparse.ArgumentParser()
ap.add_argument('tower')
ap.add_argument('days', type=float)
ap.add_argument('out')
ap.add_argument('--inst', default='parity')
ap.add_argument('--seed', default='1')
ap.add_argument('--skip-oracle', action='store_true')
ap.add_argument('--skip-opensky', action='store_true')
ap.add_argument('--template')
ap.add_argument('--exe-tree', default=TREE, help='tree whose build/OpenSkyscraper runs the OpenSky side')
ap.add_argument('--os-dir', default='opensky', help='OUTDIR subdirectory for the OpenSky run')
a = ap.parse_args()
tower = os.path.abspath(a.tower)
out = os.path.abspath(a.out)
os.makedirs(out, exist_ok=True)
title = os.path.splitext(os.path.basename(tower))[0]
world = ET.parse(tower).getroot().get('world', 'tokyo')
templ = a.template or os.path.join(GAME, TEMPLATES[world])
bld_in = out + '/input.bld'

if not os.path.exists(bld_in) or not a.skip_oracle:
    r = subprocess.run([TREE + '/build/BldTool', 'export', tower, templ, bld_in], cwd=TREE,
                       capture_output=True, text=True)
    open(out + '/export.txt', 'w').write(r.stdout + r.stderr)
    print(r.stdout + r.stderr, end='')
    if r.returncode:
        sys.exit('export failed')

procs = []
if not a.skip_opensky:
    od = out + '/' + a.os_dir
    os.makedirs(od + '/dumps', exist_ok=True)
    for f in glob.glob(od + '/dumps/*'):
        os.remove(f)
    home = '/tmp/parity-home-%s-%s' % (title, a.os_dir)
    os.makedirs(home, exist_ok=True)
    cmd = ['xvfb-run', '-a', '-s', '-screen 0 1280x800x24', a.exe_tree + '/build/OpenSkyscraper', '--ruleset', 'yoottower',
           '--load-game', bld_in, '--sim', '%gd' % a.days, '--sim-seed', a.seed, '--sim-out', od + '/out.tower',
           '--sim-dump-every', str(DUMP), od + '/dumps']
    procs.append(('opensky', subprocess.Popen(cmd, cwd=a.exe_tree, env=dict(os.environ, HOME=home),
                                              stdout=open(od + '/sim.log', 'w'), stderr=subprocess.STDOUT)))
if not a.skip_oracle:
    cmd = [sys.executable, HERE + '/oracle_run.py', bld_in, tower, out + '/oracle', str(a.days), '--inst', a.inst]
    os.makedirs(out + '/oracle', exist_ok=True)
    procs.append(('oracle', subprocess.Popen(cmd, stdout=open(out + '/oracle/run.out', 'w'), stderr=subprocess.STDOUT)))
for name, p in procs:
    print(name, 'exit', p.wait(), flush=True)

if not a.skip_opensky:
    od = out + '/' + a.os_dir
    with open(od + '/series.jsonl', 'w') as f:
        for j in sorted(glob.glob(od + '/dumps/tick_*.json')):
            f.write(json.dumps(metrics.os_record(j[:-5] + '.tower', j)) + '\n')

orc, osk = out + '/oracle/series.jsonl', out + '/%s/series.jsonl' % a.os_dir
suffix = '' if a.os_dir == 'opensky' else '-' + a.os_dir
if os.path.exists(orc) and os.path.exists(osk):
    text, data = compare.report(compare.load(orc), compare.load(osk), out + '/%s/out.tower.json' % a.os_dir, title)
    open(out + '/report%s.md' % suffix, 'w').write(text + '\n')
    json.dump(data, open(out + '/report%s.json' % suffix, 'w'), indent=1, default=str)
    print(text)
