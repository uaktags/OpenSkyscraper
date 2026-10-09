"""Common per-snapshot metrics for the parity harness.

Both games are reduced to the same record (one per snapshot):

    {"t": absolute game day (float), "day": int, "minute": 0..1439,
     "funds": dollars, "population": int, "grade": star level (1..5),
     "vip": VIP flag word, "events": {key: payload} (original only),
     "tenants": {key: {"type", "customers", "estimate", "status", "workers"}}}

Tenant keys are "<opensky type>@x,y" in OpenSky coordinates, so the same room
matches in both games. The original's records come from a .bld written by the
game (tools/yoottower/bld.py), OpenSky's from a --sim dump (.tower + .json).
"""
import json
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..')))
import bld  # noqa: E402  tools/yoottower/bld.py

# Plugin file stem (lower case) -> OpenSky prototype id, for the plugins the
# exporter writes (source/YootTenants.cpp, yootPrototypeLinks in YootWorld.cpp).
NATIVE = {
    'office': 'office', 'restroom': 'restroom', 'krestroom': 'restroom', 'apartment': 'apartment',
    'singleroom': 'hotel_single', 'twinroom': 'hotel_double', 'suiteroom': 'hotel_suite',
    'smallroom': 'hotel_single', 'largeroom': 'hotel_suite',
    'parking': 'parking', 'securityroom': 'security', "doctor's": 'doctor',
    'trashfacility': 'trashfacility', 'ktrashfacility': 'trashfacility', 'powerroom': 'powerroom',
    'theater': 'theater', 'classictheater': 'classictheater', 'eventcenter': 'eventcenter',
    'billboard': 'billboard', 'stadium': 'stadium', 'cathedral': 'yootcathedral',
    'attraction': 'attraction', 'observatory': 'observatory', 'ticket': 'ticketbooth',
    'gatheringarea': 'gatheringarea', 'boatdeck': 'boatdeck', 'subway': 'subway',
    'front': 'frontdesk', 'rfront': 'frontdesk', 'maintenance': 'housekeeping',
    'rmaintenance': 'housekeeping', 'firedepartment': 'firedept', 'school': 'school',
    'skybrdge': 'skybridge', 'condo': 'condo', 'hawaiancondo': 'condo',
}
# OpenSky prototype ids that differ from "yt_" + stem.
YT = {"men'sclothing": 'yt_mensclothing', 'video&dvd': 'yt_videodvd', 'j-pub': 'yt_jpub',
      'j-snack': 'yt_jsnack', "jenny's": 'yt_jennys', 'trad-japanese': 'yt_tradjapanese',
      "weinstein'stoffee": 'yt_weinsteinstoffee'}
STRUCTURE = {'floor', 'lobby', 'kfloor'}


def proto_of_plugin(filename):
    stem = re.split(r'[\\/]', filename)[-1].rsplit('.', 1)[0].lower()
    if stem in ('floor', 'kfloor'):
        return 'floor'
    if stem in NATIVE:
        return NATIVE[stem]
    return YT.get(stem, 'yt_' + re.sub(r'[^a-z0-9]', '', stem))


# -- original (.bld) --------------------------------------------------------

def bld_frame(tower_path, bld_dump):
    """(origin, ground) that map .bld cells to OpenSky x, y: x = left - origin,
    y = ground - bottom (source/BldLoader.cpp toTowerXml)."""
    origin = bld_dump['floor']['hRange'] // 2
    ground = None
    lobby = None
    for it in ET.parse(tower_path).getroot().iter('item'):
        if it.get('type') == 'lobby':
            lobby = (int(it.get('x')), int(it.get('y')))
            break
    for t in bld_tenants(bld_dump):
        if t.get('valiation') == 1 and lobby and t['area'][0] - origin == lobby[0]:
            g = t['area'][3] + lobby[1]
            ground = g if ground is None else max(ground, g)
    return origin, ground


def bld_tenants(d):
    for b in d['floor']['tenants']['blocks']:
        for t in b['slots']:
            if isinstance(t, dict) and t.get('used'):
                yield t


def bld_record(d, frame):
    origin, ground = frame
    files = {p['itemType']: p['fileName'] for p in d['plugins']}
    dt = d['date']
    day = (dt['year'] - 1) * 12 + dt['month0']
    rec = {'t': day + dt['minutes'] / 1440.0, 'day': day, 'minute': dt['minutes'],
           'funds': d['settlement']['funds'] * 100, 'population': d['pool']['population'],
           'grade': d['header']['level'], 'vip': d['header']['fa0'],
           'ledger': list(d['settlement']['totals']), 'tenants': {}, 'events': {}}
    for t in bld_tenants(d):
        proto = proto_of_plugin(files.get(t['equipType'], str(t['equipType'])))
        if proto in STRUCTURE:
            continue
        left, top, right, bottom = t['area']
        if proto == 'parking':
            proto = 'parkinggate' if bottom - top >= 2 else 'parkinglot'
        key = '%s@%d,%d' % (proto, left - origin, ground - bottom)
        rec['tenants'][key] = {'type': proto, 'customers': t.get('totalCustomer') or 0,
                               'estimate': t.get('estimate'), 'status': t.get('status'),
                               'workers': t.get('workCount'), 'belong': t.get('belongCount'),
                               'inside': t.get('customerCount')}
    rec['partial'] = d.get('partial', False)
    for e in d['towerEvent'].get('events', []):
        rec['events']['%s#%d' % (e['type'], e['id'])] = '%d:%s' % (e['standby'], e.get('data'))
    # T2People status (+8, T2People::Idle 0x10048570): 1 in the pool, 5 walking to a
    # mover, 6 waiting at it, 8 riding, 10 walking to a tenant, 11 inside, 12 leaving.
    people = {}
    for b in d['people']['blocks']:
        for q in b['slots']:
            if isinstance(q, dict) and q.get('used'):
                people[str(q['status'])] = people.get(str(q['status']), 0) + 1
    rec['people'] = people
    return rec


def parse_save(data):
    """bld.parse, or a lenient parse when the tail after the traffic map does not parse:
    some saves written while a Hawaii cruise is docked (CREV vErC) fail bld.py's
    traffic-map search. Then the known cell count for the world is used and the tail is
    read as far as it goes (settlement first, so funds survive; d['partial'] is set)."""
    try:
        return bld.parse(data)
    except bld.BldError:
        pass
    strict_find, strict_tail = bld.find_traffic_cells, bld.tail

    def find(buf, pos):
        for n in bld.KNOWN_TRAFFIC_CELLS:
            c = bld.Codec(bytes(buf))
            c.pos = pos + n * 4
            st = {}
            try:
                c.s32(st, 'funds'); c.s32(st, 'previousFunds')
                c.values(st, 'totals', 5, c.s32)
            except Exception:  # noqa: BLE001
                continue
            if abs(st['funds']) < 10 ** 8 and abs(st['previousFunds']) < 10 ** 8:
                return n
        raise bld.BldError('no traffic map size fits')

    def tail(c, d):
        d.setdefault('towerEvent', {'events': []})
        try:
            strict_tail(c, d)
        except Exception:  # noqa: BLE001
            d['partial'] = True
            c.pos = len(c.buf)

    bld.find_traffic_cells, bld.tail = find, tail
    try:
        return bld.parse(data)
    finally:
        bld.find_traffic_cells, bld.tail = strict_find, strict_tail


def bld_file_record(path, tower_path, frame=None):
    d = parse_save(open(path, 'rb').read())
    return bld_record(d, frame or bld_frame(tower_path, d))


# -- OpenSky (--sim dump) ---------------------------------------------------

def _num(v, default=None):
    if v is None:
        return default
    try:
        return int(float(v))
    except ValueError:
        return {'true': 1, 'false': 0}.get(v, default)


def os_record(tower_path, json_path):
    s = json.load(open(json_path))
    root = ET.parse(tower_path).getroot()
    t = s['time']['absolute']
    day = int(t)
    rec = {'t': t, 'day': day, 'minute': int(round((t - day) * 1440)) % 1440,
           'funds': s['funds'], 'population': s['population'], 'grade': s['rating'] + 1,
           'vip': _num(root.get('yootVipFlags'), 0), 'blackout': _num(root.get('powerBlackout'), 0),
           'money': s.get('money', {}), 'tenants': {}}
    for it in root.iter('item'):
        typ = it.get('type')
        if typ in ('floor', 'lobby') or typ.startswith('elevator') or typ in ('stairs', 'escalator'):
            continue
        a = it.attrib
        cust = a.get('customers', a.get('visitorsToday'))
        status = a.get('tenantStatus', a.get('status', a.get('roomState')))
        workers = a.get('staff', a.get('population'))
        rec['tenants']['%s@%s,%s' % (typ, a.get('x'), a.get('y'))] = {
            'type': typ, 'customers': _num(cust, 0), 'estimate': _num(a.get('yootEstimate')),
            'status': _num(status), 'workers': _num(workers), 'inside': _num(a.get('inside'))}
    return rec


def os_people(summary_path):
    """{absolute day: {pool, walking, waiting, riding, inside, other}} from the summary's
    hourly 'people Nh:' lines (PeopleSystem)."""
    out = {}
    for t, msg in os_events(summary_path):
        if msg.startswith('people ') and 'h: pool ' in msg:
            v = msg.split(': ', 1)[1].split()
            out[t] = {v[i]: int(v[i + 1]) for i in range(0, len(v) - 1, 2)}
    return out


def os_events(summary_path):
    """[(t, message)] from a --sim summary."""
    out = []
    for e in json.load(open(summary_path))['events']:
        t, msg = e.split(' ', 1)
        out.append((float(t), msg))
    return out
