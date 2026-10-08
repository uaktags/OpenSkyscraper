"""recompare.py OUTDIR... : rewrite report*.md / report*.json of finished pipeline runs (after a
compare.py change), one per OpenSky run directory (opensky, opensky-*, os-*)."""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compare  # noqa: E402

for out in sys.argv[1:]:
    out = os.path.normpath(out)
    orc = out + '/oracle/series.jsonl'
    if not os.path.exists(orc):
        continue
    title = os.path.basename(out)
    for od in sorted(glob.glob(out + '/opensky*') + glob.glob(out + '/os-*')):
        if not os.path.exists(od + '/series.jsonl') or not os.path.exists(od + '/out.tower.json'):
            continue
        name = os.path.basename(od)
        suffix = '' if name == 'opensky' else '-' + name
        text, data = compare.report(compare.load(orc), compare.load(od + '/series.jsonl'), od + '/out.tower.json',
                                    title if not suffix else '%s (%s)' % (title, name))
        open(out + '/report%s.md' % suffix, 'w').write(text + '\n')
        json.dump(data, open(out + '/report%s.json' % suffix, 'w'), indent=1, default=str)
        print('%s/report%s.md' % (out, suffix))
