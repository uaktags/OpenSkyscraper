"""reextract.py OUTDIR : rebuild OUTDIR/oracle/series.jsonl from the kept snapshots
(after a metrics.py change), keeping the dialog titles of the old series."""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import metrics  # noqa: E402

out = sys.argv[1]
tower = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'tests')
old = {}
path = out + '/oracle/series.jsonl'
for line in open(path):
    if line.strip():
        r = json.loads(line)
        old[r['snap']] = r
name = os.path.basename(os.path.normpath(out))
towers = glob.glob('%s/*/%s.tower' % (os.path.normpath(tower), name))
frame = None
recs = []
for snap in sorted(glob.glob(out + '/oracle/snaps/snap_*.bld')):
    n = os.path.basename(snap)[:-4]
    d = metrics.parse_save(open(snap, 'rb').read())
    frame = frame or metrics.bld_frame(towers[0], d)
    r = metrics.bld_record(d, frame)
    r['snap'] = n
    r['dialogs'] = old.get(n, {}).get('dialogs', [])
    recs.append(r)
with open(path, 'w') as f:
    for r in recs:
        f.write(json.dumps(r) + '\n')
print('%s: %d records' % (out, len(recs)))
