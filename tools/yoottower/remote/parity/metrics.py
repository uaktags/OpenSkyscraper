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
    for e in d['towerEvent']['events']:
        rec['events']['%s#%d' % (e['type'], e['id'])] = '%d:%s' % (e['standby'], e['data'])
    return rec


def bld_file_record(path, tower_path, frame=None):
    d = bld.parse(open(path, 'rb').read())
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


def os_events(summary_path):
    """[(t, message)] from a --sim summary."""
    out = []
    for e in json.load(open(summary_path))['events']:
        t, msg = e.split(' ', 1)
        out.append((float(t), msg))
    return out
