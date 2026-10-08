#!/usr/bin/env python3
"""OpenSky -> .bld -> original YootTower -> .bld -> OpenSky round trip.

Runs inside the yoottower-lab container (doc/yoottower/remote-lab.md):

    lab.sh exec python3 /lab/src/TREE/tools/yoottower/remote/bld_roundtrip.py \\
        [--tree /lab/src/TREE] [--out /lab/out/roundtrip] [--days 1] [CASE...]

A CASE is NAME=TOWER[:TEMPLATE]; without cases the default set below runs
(all three worlds). For each case it:

 1. exports the OpenSky tower with BldTool against the world's template save
    and imports the export again with BldTool (what OpenSky expects back);
 2. loads the export in the original (Wine oracle, instance rt-NAME), runs it
    at full speed for DAYS game days (one YootTower day = 24 clock hours)
    and saves it through File > Save As;
 3. checks that the game's save parses and rewrites byte-identical with
    tools/yoottower/bld.py, imports it into OpenSky (OpenSkyscraper
    --load-game ... --sim 1 --sim-out) and compares items (per prototype),
    funds and population with what went in.

Writes OUT/NAME/{export.bld,resaved.bld,reimport.tower,...}, OUT/report.json
and prints a table. Exit status 0 when every case passed.
"""
import argparse, json, os, re, subprocess, sys, time
from collections import Counter

GAME = os.environ.get('YOOT_SRC', '/lab/game-src/YOOT')
DEFAULT_CASES = [
    ('tokyo_people', 'tests/sim/yoot_people.tower', GAME + '/Sample Tokyo Map.bld'),
    ('tokyo_visitors', 'tests/sim/yoot_visitors.tower', GAME + '/Sample Tokyo Map.bld'),
    ('tokyo_bridge', 'tools/yoottower/remote/towers/tokyo_bridge.tower', GAME + '/Sample Tokyo Map.bld'),
    ('hawaii', 'tools/yoottower/remote/towers/hawaii.tower', GAME + '/Yooooot.bld'),
    # No Kegon save ships with the game: any save the game wrote in Kegon.
    ('kegon', 'tools/yoottower/remote/towers/kegon.tower',
     os.environ.get('KEGON_TEMPLATE', '/lab/out/save2/saves/kegon.bld')),
]
WIN_RE = re.compile(r'\S+\s+(\d+)x(\d+)\s+\+(-?\d+)\+(-?\d+) ?(.*)$')


class Case:
    def __init__(self, name, tower, template):
        self.name, self.tower, self.template = name, tower, template
        self.result = {'name': name, 'tower': tower, 'template': template, 'checks': {}, 'notes': []}

    def check(self, key, ok, detail):
        self.result['checks'][key] = {'ok': bool(ok), 'detail': detail}


def run(cmd, env=None, check=True, timeout=600):
    r = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=timeout)
    if check and r.returncode != 0:
        raise RuntimeError('%s failed (%d): %s' % (' '.join(cmd[:3]), r.returncode, (r.stderr or r.stdout)[-400:]))
    return r


def tower_counts(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    return Counter(re.findall(r'<item type="([^"]+)"', text))


def placed(report_text):
    return Counter({m.group(2): int(m.group(1)) for m in re.finditer(r'placed\s+(\d+) x (\S+)', report_text)})


class Oracle:
    def __init__(self, tree, instance):
        self.sh = tree + '/tools/yoottower/oracle/run_yoot.sh'
        self.env = dict(os.environ, YOOT_INSTANCE=instance)

    def __call__(self, *args, check=True, timeout=180):
        return run(['bash', self.sh] + [str(a) for a in args], env=self.env, check=check, timeout=timeout).stdout

    def windows(self):
        out = {}
        for line in self('windows', check=False).splitlines():
            m = WIN_RE.match(line)
            if m:
                out[m.group(5)] = tuple(int(m.group(i)) for i in (3, 4, 1, 2))
        return out

    def window(self, pred):
        for title, g in self.windows().items():
            if pred(title):
                return g
        return None

    def toggle_run(self):
        x, y, _, _ = self.window(lambda t: t == 'Control') or (500, 100, 0, 0)
        self('click', x + 25, y + 30)

    def fastest(self):
        x, y, _, _ = self.window(lambda t: t == 'Control') or (500, 100, 0, 0)
        self('drag', x + 124, y + 28, x + 132, y + 28)

    def save_as(self, path):
        mx, my, _, _ = self.window(lambda t: t.endswith(' - T2')) or (48, 74, 0, 0)
        for attempt in range(3):
            self('click', mx + 13, my + 11)          # File
            time.sleep(0.8)
            self('click', mx + 37, my + 82)          # Save As
            self('wait-window', '^Save As$', 8, check=False)
            if self.window(lambda t: t == 'Save As'):
                break
            self('key', 'Escape', check=False)
            time.sleep(0.5)
        g = self.window(lambda t: t == 'Save As')
        if not g:
            raise RuntimeError('no Save As dialog')
        self('click', g[0] + 203, g[1] + 189)        # file name field
        self('key', 'End', 'shift+Home')
        self('type', 'Z:' + path.replace('/', '\\'))
        time.sleep(0.3)
        self('key', 'Return')
        self('wait-gone', '^Save As$', 10, check=False)
        time.sleep(1.5)
        if any(re.search('Overwrite|Confirm|Save As', t) for t in self.windows()):
            self('key', 'Return', check=False)
            time.sleep(1)


def normalize_ui(bld, path, ref):
    """Give the export the shipped Tokyo sample's window layout so the menu
    bar is not under the Information palette (the menus are clicked)."""
    d = bld.parse(open(path, 'rb').read())
    r = bld.parse(open(ref, 'rb').read())
    for k in ('mainWindow', 'toolPalette', 'controlPalette', 'messageWindow'):
        d['ui'][k] = r['ui'][k]
    open(path, 'wb').write(bld.build(d))


def minutes_of(d):
    t = d['date']
    return (t['year'] * 12 + t['month0']) * 1440 + t['minutes']


def run_case(c, a, bld):
    tree, out = a.tree, os.path.join(a.out, c.name)
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ, OPENSKY_PLUGINS=tree + '/data/Plugins')
    tool = tree + '/build/BldTool'
    tower = c.tower if c.tower.startswith('/') else os.path.join(tree, c.tower)
    export, expect_tower = out + '/export.bld', out + '/expected.tower'
    resaved, reimport = out + '/resaved.bld', out + '/reimport.tower'
    if not os.path.exists(c.template):
        raise RuntimeError('template missing: ' + c.template)

    # 1. export + what OpenSky gets back from the export itself
    r = run([tool, 'export', tower, c.template, export], env=env)
    open(out + '/export.log', 'w').write(r.stdout + r.stderr)
    exported = placed(r.stdout)
    dropped = re.findall(r'dropped\s+(\d+ x .*)', r.stdout)
    normalize_ui(bld, export, GAME + '/Sample Tokyo Map.bld')
    r = run([tool, 'import', export, expect_tower], env=env)
    expected = tower_counts(expect_tower)
    src = bld.parse(open(export, 'rb').read())
    c.result.update(exported=dict(exported), dropped=dropped, expected=dict(expected),
                    fundsIn=src['settlement']['funds'] * 100)

    # 2. the original: load, run DAYS game days at full speed, save
    o = Oracle(tree, 'rt-' + c.name)
    o('stop', check=False)
    o('start', export, timeout=240)
    if o.window(lambda t: 'Plugin Check' in t):
        # The original refuses the export before it runs (its plugin check dialog).
        c.check('load', False, 'the original shows "Plugin Check Error" for this export; screenshot in %s' % out)
        o('shot', out + '/plugin_check.png', check=False)
        o('stop', check=False)
        return
    o.toggle_run()                                  # pause right after loading
    o.fastest()
    snap = out + '/snap.bld'
    o.save_as(snap)
    start = cur = minutes_of(bld.parse(open(snap, 'rb').read()))
    rate, deadline = 30.0, time.time() + a.timeout
    while cur - start < a.days * 1440 and time.time() < deadline:
        want = a.days * 1440 - (cur - start)
        dt = max(1.0, min(30.0, want / rate))
        o.toggle_run()
        t0 = time.time()
        time.sleep(dt)
        o.toggle_run()
        ran = time.time() - t0
        time.sleep(0.5)
        o.save_as(snap)
        now = minutes_of(bld.parse(open(snap, 'rb').read()))
        if now > cur:
            rate = 0.5 * rate + 0.5 * (now - cur) / ran
        cur = now
    o.save_as(resaved)
    o('shot', out + '/oracle.png', check=False)
    o('stop', check=False)
    data = open(resaved, 'rb').read()
    game = bld.parse(data)
    ran_min = minutes_of(game) - start
    c.check('ran', ran_min >= a.days * 1440, '%d game minutes' % ran_min)
    c.check('reparse', bld.build(game) == data, 'bld.py rewrite of the game save')

    # 3. back into OpenSky (the game's own import path) and compare
    r = run(['xvfb-run', '-a', tree + '/build/OpenSkyscraper', '--ruleset', 'yoottower', '--load-game', resaved,
             '--sim', '1', '--sim-out', reimport, '--sim-summary', out + '/reimport.json'], env=env, check=False,
            timeout=300)
    open(out + '/reimport.log', 'w').write(r.stdout[-20000:] + r.stderr[-20000:])
    if not os.path.exists(reimport):
        c.check('reimport', False, 'OpenSky wrote no tower')
        return
    got = tower_counts(reimport)
    summary = json.load(open(out + '/reimport.json'))
    missing = {k: v - got.get(k, 0) for k, v in expected.items() if got.get(k, 0) < v}
    extra = {k: v - expected.get(k, 0) for k, v in got.items() if expected.get(k, 0) < v}
    c.check('items', not missing, 'missing %s, new %s' % (missing or '{}', extra or '{}'))
    game_funds = game['settlement']['funds'] * 100
    c.check('funds', summary.get('funds') == game_funds,
            'in $%d, game $%d after the run, OpenSky $%s' % (c.result['fundsIn'], game_funds, summary.get('funds')))
    game_pop = game['pool']['population']
    # yootPool: version, gameLevel, specialFlag, population, shrinkCount, shown population, ...
    m = re.search(r'yootPool="(\d+) (\d+) (\d+) (\d+) (\d+) (\d+)', open(reimport, encoding='utf-8', errors='replace').read())
    pool_pop = int(m.group(4)) if m else None
    # summary 'people' counts the people objects; 'population' is OpenSky's
    # own tower population figure (it is not compared with the pool).
    c.check('population', pool_pop == game_pop and summary.get('people') == game_pop,
            'game pool %d, re-imported pool %s, OpenSky people %s, OpenSky population %s' % (
                game_pop, pool_pop, summary.get('people'), summary.get('population')))
    c.result.update(got=dict(got), gameFunds=game_funds, gamePopulation=game_pop,
                    openSkyPopulation=summary.get('population'), gameMinutes=ran_min)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--tree', default=os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))))
    ap.add_argument('--out', default='/lab/out/roundtrip')
    ap.add_argument('--days', type=float, default=1.0)
    ap.add_argument('--timeout', type=float, default=900, help='wall seconds per case for the run')
    ap.add_argument('cases', nargs='*')
    a = ap.parse_args()
    sys.path.insert(0, a.tree + '/tools/yoottower')
    import bld  # noqa: E402
    cases = []
    for spec in a.cases:
        name, rest = spec.split('=', 1)
        tower, _, templ = rest.partition(':')
        cases.append(Case(name, tower, templ or GAME + '/Sample Tokyo Map.bld'))
    if not cases:
        cases = [Case(*c) for c in DEFAULT_CASES]
    os.makedirs(a.out, exist_ok=True)
    ok = True
    for c in cases:
        try:
            run_case(c, a, bld)
        except Exception as e:  # noqa: BLE001
            c.check('run', False, str(e)[:300])
            Oracle(a.tree, 'rt-' + c.name)('stop', check=False)
        c.result['ok'] = all(v['ok'] for v in c.result['checks'].values())
        ok &= c.result['ok']
        print('%-15s %s' % (c.name, 'PASS' if c.result['ok'] else 'FAIL'), flush=True)
        for k, v in c.result['checks'].items():
            print('    %-10s %-4s %s' % (k, 'ok' if v['ok'] else 'FAIL', v['detail']), flush=True)
    json.dump([c.result for c in cases], open(a.out + '/report.json', 'w'), indent=1)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
