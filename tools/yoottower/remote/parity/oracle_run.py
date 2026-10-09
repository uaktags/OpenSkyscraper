"""oracle_run.py SAVE.bld TOWER OUTDIR DAYS [--inst NAME] : run the original YootTower on SAVE
(inside the lab container) and save it about every game hour through File > Save As.

Every save is reduced to a metrics record (metrics.bld_record, keyed like the OpenSky
tower TOWER) and appended to OUTDIR/series.jsonl. Dialogs that pop up while the clock
runs (events, VIPs, warnings) are screenshotted to OUTDIR/dialogs/ and closed with
Return; their titles go to the record ("dialogs"). The run stops after DAYS game days.
Derived from the people agent's oracle_loop.py.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import metrics  # noqa: E402

OR = os.path.normpath(os.path.join(HERE, '..', '..', 'oracle'))
KNOWN = re.compile(r'( - T2|^Tools|^Information|^Control|^Message|^Save As)$')

ap = argparse.ArgumentParser()
ap.add_argument('save')
ap.add_argument('tower')
ap.add_argument('out')
ap.add_argument('days', type=float)
ap.add_argument('--inst', default='parity')
ap.add_argument('--keep', type=int, default=1, help='keep every Nth snapshot .bld (0: none)')
ap.add_argument('--step', type=float, default=0, help='game minutes between saves (0: 45 by day, 90 at night)')
a = ap.parse_args()
ENV = dict(os.environ, YOOT_INSTANCE=a.inst)
os.makedirs(a.out + '/snaps', exist_ok=True)
os.makedirs(a.out + '/dialogs', exist_ok=True)
log = open(a.out + '/oracle.log', 'a')
frame = None


def R(*args, check=True, capture=False):
    return subprocess.run(['bash', OR + '/run_yoot.sh'] + [str(x) for x in args], env=ENV, check=check,
                          capture_output=capture, text=True)


def say(s):
    log.write(s + '\n'); log.flush(); print(s, flush=True)


def windows():
    out = []
    for line in R('windows', capture=True, check=False).stdout.splitlines():
        m = re.match(r'\S+\s+(\d+)x(\d+)\s+\+(-?\d+)\+(-?\d+) ?(.*)$', line)
        if m:
            w, h, x, y = map(int, m.group(1, 2, 3, 4))
            out.append((m.group(5), x, y, w, h))
    return out


def geom(pat):
    for title, x, y, w, h in windows():
        if re.search(pat, title):
            return x, y, w, h
    raise RuntimeError('window not found: ' + pat)


def clear_dialogs(tag):
    """Screenshot and dismiss any window the harness did not open."""
    seen = []
    last = None
    for _ in range(6):
        extra = [w for w in windows() if not KNOWN.search(w[0])]
        if not extra:
            break
        title, x, y, w, h = extra[0]
        again = last == extra[0]
        last = extra[0]
        if not again:
            n = len(os.listdir(a.out + '/dialogs'))
            shot = '%s/dialogs/%03d_%s.png' % (a.out, n, tag)
            R('shot', shot, check=False)
            seen.append(title or '%dx%d' % (w, h))
            say('dialog %r %dx%d+%d+%d -> %s' % (title, w, h, x, y, shot))
        if title == 'Plugin Check Error':
            R('click', x + 359, y + 324)       # Continue
        elif title == 'Event' or again:
            # Event choices (terrorist: "Accept offer" / "Look for"; fire: helicopter /
            # staff) have no default button: take the first, as OpenSky's --sim answers
            # yes (it pays the ransom, sends the helicopter). Single-OK dialogs (the
            # blackout warning) place OK ~14 px above the bottom, so a retry clicks
            # lower and falls back to Return.
            R('click', x + w // 2, y + h - (24 if again else 46))
            if again:
                R('key', 'Return', check=False)
        else:
            R('key', 'Return', check=False)
        time.sleep(1.0)
    return seen


def menu_reachable():
    """Close the Tools palette when it covers the main window's File menu."""
    mx, my, _, _ = geom(r' - T2$')
    for title, x, y, w, h in windows():
        if title == 'Tools' and x <= mx + 13 <= x + w and y <= my + 11 <= y + h:
            R('click', x + 79, y + 11)
            time.sleep(0.5)


def saveas(n):
    global frame
    path = '%s/snaps/%s.bld' % (a.out, n)
    mx, my, _, _ = geom(r' - T2$')
    R('click', mx + 13, my + 11); time.sleep(0.8)   # File
    R('click', mx + 37, my + 82)                    # Save As...
    R('wait-window', '^Save As$', 10)
    x, y, _, _ = geom('^Save As$')
    R('click', x + 203, y + 189)
    R('key', 'End', 'shift+Home')
    R('type', 'Z:' + path.replace('/', '\\'))
    time.sleep(0.3); R('key', 'Return')
    R('wait-gone', '^Save As$', 10, check=False)
    time.sleep(1.5)
    if any(re.search('Overwrite|Confirm|Save As', w[0]) for w in windows()):
        say('WARN dialog left after save')
        R('key', 'Return', check=False); time.sleep(1)
    # The game may still be writing under load: wait for a stable size, retry the parse.
    size, d = -1, None
    for attempt in range(20):
        time.sleep(0.5)
        if not os.path.exists(path) or os.path.getsize(path) != size:
            size = os.path.getsize(path) if os.path.exists(path) else -1
            continue
        try:
            d = metrics.bld.parse(open(path, 'rb').read())
            break
        except metrics.bld.BldError:
            size = -1
    if d is None:
        d = metrics.parse_save(open(path, 'rb').read())
    if frame is None:
        frame = metrics.bld_frame(a.tower, d)
    rec = metrics.bld_record(d, frame)
    rec['snap'] = n
    return rec


def keep(idx, n):
    if a.keep <= 0 or idx % a.keep:
        os.remove('%s/snaps/%s.bld' % (a.out, n))


R('stop', check=False)
subprocess.run(['bash', OR + '/drive.sh', 'load', a.save, a.out + '/load.png'], env=dict(ENV, PAUSE='0'), check=True)
time.sleep(1)
clear_dialogs('load')
cx, cy, _, _ = geom('^Control$')
RUN = (cx + 25, cy + 30)
R('click', *RUN)                                    # pause (the save loads running)
R('drag', cx + 124, cy + 28, cx + 132, cy + 28)     # speed slider to the fastest step
time.sleep(0.5)
menu_reachable()
rec = saveas('snap_000')
rec['dialogs'] = []
series = open(a.out + '/series.jsonl', 'w')
series.write(json.dumps(rec) + '\n'); series.flush()
start = prev = rec['t'] * 1440
rate, idx = 10.0, 1
fails = 0
while prev - start < a.days * 1440:
    h = (prev % 1440) // 60
    target = a.step or (45 if 6 <= h <= 23 else 90)
    dt = max(0.4, min(20.0, target / rate))
    R('click', *RUN); t0 = time.time(); time.sleep(dt); R('click', *RUN)
    ran = time.time() - t0
    time.sleep(0.4)
    n = 'snap_%03d' % idx
    dialogs = clear_dialogs(n)
    try:
        menu_reachable()
        rec = saveas(n)
    except Exception as e:  # noqa: BLE001
        say('save failed %s: %s' % (n, e))
        fails += 1
        if fails > 5:
            break
        R('key', 'Escape', check=False); time.sleep(1)
        clear_dialogs(n + 'f')
        continue
    fails = 0
    rec['dialogs'] = dialogs
    series.write(json.dumps(rec) + '\n'); series.flush()
    cur = rec['t'] * 1440
    if cur > prev:
        rate = 0.5 * rate + 0.5 * ((cur - prev) / ran)
    say('%s day %d %02d:%02d pop %d funds %d grade %d rate %.1f' % (
        n, rec['day'], rec['minute'] // 60, rec['minute'] % 60, rec['population'], rec['funds'], rec['grade'], rate))
    keep(idx, n)
    prev = cur
    idx += 1
R('stop', check=False)
say('done')
