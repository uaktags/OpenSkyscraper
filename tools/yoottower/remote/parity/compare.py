"""compare.py ORACLE.jsonl OPENSKY.jsonl OPENSKY_SUMMARY.json [--title T] [-o report.md] [--json out.json]

Reduces both snapshot series (metrics.py records) to per-day metrics and writes a
markdown report: the day table (funds, population peak/night, grade, VIP flags),
customers and evaluation per tenant, and the events each game fired. A cell is
marked "x" when the two games differ by more than the tolerance (TOL).
"""
import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import metrics  # noqa: E402

# metric: (relative, absolute) tolerance; a value passes if it is within either.
TOL = {'funds_delta': (0.25, 20000), 'pop_max': (0.20, 25), 'pop_min': (0.25, 10),
       'customers': (0.30, 10), 'estimate': (0.0, 60)}
QUIET = ('customers:', 'pool:', 'h: pool ')


def load(path):
    return [json.loads(line) for line in open(path) if line.strip()]


def close(a, b, tol):
    if a is None or b is None:
        return a == b
    rel, ab = tol
    return abs(a - b) <= ab or abs(a - b) <= rel * max(abs(a), abs(b))


def daily_customers(series):
    """{tenant: {day: customers}}: each count runs up from the tenant's opening and drops at
    the next opening; a run's peak belongs to the day it was reached (before 05:00 counts
    as the day before, for tenants that close after midnight)."""
    out = collections.defaultdict(dict)
    keys = set(k for r in series for k in r['tenants'])
    for k in keys:
        run_max, run_day = 0, None
        for r in series:
            t = r['tenants'].get(k)
            if t is None:
                continue
            c = t['customers'] or 0
            if c < run_max and run_day is not None:
                out[k][run_day] = max(out[k].get(run_day, 0), run_max)
                run_max, run_day = 0, None
            if c > run_max:
                run_max = c
                run_day = r['day'] - (1 if r['minute'] < 300 else 0)
        if run_day is not None and run_max:
            out[k][run_day] = max(out[k].get(run_day, 0), run_max)
    return out


def _staff(t):
    """Hired staff: the original's belong count, OpenSky's staff / office population."""
    if not t:
        return 0
    return (t.get('belong') if 'belong' in t else t.get('workers')) or 0


def daily(series):
    days = collections.OrderedDict()
    for r in series:
        days.setdefault(r['day'], []).append(r)
    prev_funds = series[0]['funds'] if series else 0
    res = collections.OrderedDict()
    for d, rs in days.items():
        last = rs[-1]
        res[d] = {'funds': last['funds'], 'funds_delta': last['funds'] - prev_funds,
                  'pop_max': max(r['population'] for r in rs), 'pop_min': min(r['population'] for r in rs),
                  'grade': last['grade'], 'vip': last['vip'], 'snaps': len(rs),
                  'estimate': {k: v['estimate'] for k, v in last['tenants'].items()},
                  'status': {k: v['status'] for k, v in last['tenants'].items()},
                  'workers': {k: max(_staff(r['tenants'].get(k)) for r in rs) for k in last['tenants']}}
        peak = max(rs, key=lambda r: r['population'])
        if 'populationDetail' in peak:
            res[d]['populationDetail'] = dict(peak['populationDetail'], minute=peak['minute'])
        prev_funds = last['funds']
    return res


def mover_load(orc, summary):
    """{day: (orig waiting, orig riding, OpenSky waiting, OpenSky riding)}: mean people waiting
    at and riding the movers over the 08:00-20:00 samples."""
    acc = collections.defaultdict(lambda: [[], [], [], []])
    for r in orc:
        if 'people' in r and 480 <= r['minute'] < 1200:
            acc[r['day']][0].append(r['people'].get('6', 0))
            acc[r['day']][1].append(r['people'].get('8', 0))
    for t, v in metrics.os_people(summary).items():
        day, minute = int(t), int(round((t - int(t)) * 1440))
        if 480 <= minute < 1200:
            acc[day][2].append(v.get('waiting', 0))
            acc[day][3].append(v.get('riding', 0))
    mean = lambda a: round(sum(a) / len(a)) if a else None
    return {d: tuple(mean(a) for a in v) for d, v in acc.items()}


def oracle_events(series):
    """{day: [text]}: tower-event records whose state changed between saves, grade and VIP
    changes, and dialogs the harness closed."""
    out = collections.defaultdict(list)
    prev = None
    trains = collections.Counter()
    for r in series:
        d = r['day']
        for t in r.get('dialogs', []):
            out[d].append('dialog "%s" @%02d:%02d' % (t, r['minute'] // 60, r['minute'] % 60))
        if prev:
            for k, v in r['events'].items():
                if prev['events'].get(k) == v:
                    continue
                if k.startswith('vERM#') and v.startswith('1:'):
                    trains[d] += 1   # MREV: a subway train leaves standby (12 a day)
                elif not k.startswith('vERM#'):
                    out[d].append('%s %s->%s @%02d:%02d' % (k, prev['events'].get(k), v, r['minute'] // 60, r['minute'] % 60))
            if r['grade'] != prev['grade']:
                out[d].append('grade %d->%d' % (prev['grade'], r['grade']))
            if r['vip'] != prev['vip']:
                out[d].append('VIP flags %d->%d' % (prev['vip'], r['vip']))
        prev = r
    for d, n in trains.items():
        out[d].append('MREV trains running: %d' % n)
    return out


def opensky_events(summary):
    out = collections.defaultdict(list)
    for e in json.load(open(summary))['events']:
        t, msg = e.split(' ', 1)
        if any(q in msg for q in QUIET):
            continue
        t = float(t)
        day = int(t)
        line = '%s @%02d:%02d' % (msg, int((t - day) * 24), int((t - day) * 1440) % 60)
        if line not in out[day]:
            out[day].append(line)
    return out


def mark(ok):
    return '' if ok else ' x'


def fmt(v):
    return '-' if v is None else ('{:,}'.format(v) if isinstance(v, int) else str(v))


def report(orc, osk, summary, title):
    do, ds = daily(orc), daily(osk)
    co, cs = daily_customers(orc), daily_customers(osk)
    days = [d for d in do if d in ds]
    lines = ['## %s' % title, '']
    fails = collections.Counter()
    checks = collections.Counter()
    lines += ['| Day | Funds change (orig / OpenSky) | Pop. peak | Pop. night | Grade | VIP flags |',
              '|---|---|---|---|---|---|']
    for d in days:
        o, s = do[d], ds[d]
        cells = []
        for m in ('funds_delta', 'pop_max', 'pop_min'):
            ok = close(o[m], s[m], TOL[m])
            checks[m] += 1
            fails[m] += not ok
            cells.append('%s / %s%s' % (fmt(o[m]), fmt(s[m]), mark(ok)))
        for m in ('grade', 'vip'):
            ok = o[m] == s[m]
            checks[m] += 1
            fails[m] += not ok
            cells.append('%s / %s%s' % (o[m], s[m], mark(ok)))
        lines.append('| %d | %s |' % (d, ' | '.join(cells)))
    lines.append('')
    if any('populationDetail' in do[d] and 'populationDetail' in ds[d] for d in days):
        lines += ['Population composition at each game’s sampled daily peak. Pool includes departures until midnight; '
                  'origins count live general visitors and reserved staff/residents, including those still in the pool. '
                  'OpenSky legacy item populations have unknown demand/transport types.', '',
                  '| Day / game | Minute | Pool / deferred shrink | Origins | Demand types | Transport types |',
                  '|---|---|---|---|---|---|']
        for d in days:
            for label, values in [('original', do[d]), ('OpenSky', ds[d])]:
                p = values.get('populationDetail')
                if not p:
                    continue
                counts = lambda field: ', '.join('%s: %d' % (k, v) for k, v in sorted(p[field].items())) or '-'
                lines.append('| %d / %s | %d | %d / %d | %s | %s | %s |' % (
                    d, label, p['minute'], p['pool'], p['shrink'], counts('origins'), counts('demands'), counts('transports')))
        lines.append('')
    load = mover_load(orc, summary)
    if any(v[0] is not None for v in load.values()):
        lines += ['Mean people at the movers, 08:00-20:00 (orig / OpenSky): ' + '; '.join(
            'day %d waiting %s / %s, riding %s / %s' % ((d,) + tuple(fmt(x) for x in load[d]))
            for d in days if d in load), '']
    keys = sorted(set(do[days[0]]['estimate']) | set(ds[days[0]]['estimate'])) if days else []
    only_o = [k for k in keys if k not in ds[days[0]]['estimate']]
    only_s = [k for k in keys if k not in do[days[0]]['estimate']]
    if only_o or only_s:
        lines += ['Tenants in one game only: original %s; OpenSky %s.' % (', '.join(only_o) or '-', ', '.join(only_s) or '-'), '']
    lines += ['| Tenant | Customers per day (orig / OpenSky) | Evaluation at day end | Status | Staff |',
              '|---|---|---|---|---|']
    for k in keys:
        if k in only_o or k in only_s:
            continue
        cust, est, st, wk = [], [], [], []
        for d in days:
            a, b = co[k].get(d, 0), cs[k].get(d, 0)
            ok = close(a, b, TOL['customers'])
            if a or b:
                checks['customers'] += 1
                fails['customers'] += not ok
            cust.append('%d/%d%s' % (a, b, mark(ok)))
            ea, eb = do[d]['estimate'].get(k), ds[d]['estimate'].get(k)
            ok = ea is None or eb is None or close(ea, eb, TOL['estimate'])
            if ea is not None and eb is not None:
                checks['estimate'] += 1
                fails['estimate'] += not ok
            est.append('%s/%s%s' % (fmt(ea), fmt(eb), mark(ok)))
            st.append('%s/%s' % (fmt(do[d]['status'].get(k)), fmt(ds[d]['status'].get(k))))
            wk.append('%s/%s' % (fmt(do[d]['workers'].get(k)), fmt(ds[d]['workers'].get(k))))
        lines.append('| %s | %s | %s | %s | %s |' % (k, ', '.join(cust), ', '.join(est), ', '.join(st), ', '.join(wk)))
    lines.append('')
    eo, es = oracle_events(orc), opensky_events(summary)
    lines += ['| Day | Original events | OpenSky events |', '|---|---|---|']
    for d in days:
        lines.append('| %d | %s | %s |' % (d, '<br>'.join(eo.get(d, [])) or '-',
                                           '<br>'.join(es.get(d, [])) or '-'))
    lines.append('')
    score = ', '.join('%s %d/%d' % (m, checks[m] - fails[m], checks[m]) for m in checks)
    lines += ['Within tolerance: %s.' % score, '']
    return '\n'.join(lines), {'days': days, 'checks': dict(checks), 'fails': dict(fails),
                              'orig': do, 'opensky': ds,
                              'customers': {k: {'orig': co[k], 'opensky': cs[k]} for k in keys}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('oracle')
    ap.add_argument('opensky')
    ap.add_argument('summary')
    ap.add_argument('--title', default='tower')
    ap.add_argument('-o')
    ap.add_argument('--json')
    a = ap.parse_args()
    text, data = report(load(a.oracle), load(a.opensky), a.summary, a.title)
    if a.o:
        open(a.o, 'w').write(text + '\n')
    else:
        print(text)
    if a.json:
        json.dump(data, open(a.json, 'w'), indent=1, default=str)


if __name__ == '__main__':
    main()
