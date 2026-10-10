#!/usr/bin/env python3
"""Decode the rule-relevant fields of YootTower item plugins (.t2p).

Usage: python3 -I item_rules.py <Plugins dir or .t2p ...> [--csv]

Reads the template-def resource (PE type TNDF = 4cc "FDnT" tenant, MVDF =
"FDvM" mover) exactly in the order of the T2DLL constructors:
  T2ToolDef::T2ToolDef   0x10065540  (header + up to 4 variations)
  T2EquipDef::T2EquipDef 0x1002f240  (capacity, in-money table)
  T2TenantDef::T2TenantDef 0x100623c0 / T2MoverDef::T2MoverDef 0x1003ffb0
Money is in internal units; the worlds' ExchangeMoney (T2WorldDef 0x1000ab50)
multiplies by 100 for display (MVDF "$150000" == price 1500).
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from world_rules import R, resources  # noqa: E402

def tool(r):
    d = {}
    d['toolType'] = r.i(); d['f54'] = r.i(); d['f58'] = r.i(); d['f5c'] = r.i()
    d['attr'] = r.i() & 0xffffffff
    d['catDesc'] = r.s(); d['catName'] = r.s()
    d['category'] = r.i(); d['toolNo'] = r.i(); r.i(); r.i()
    d['subPluginType'] = r.i(); r.i(); r.i()
    n = r.i(); d['variations'] = []
    for _ in range(n):
        v = {}
        v['priceStr'], v['maintStr'], v['desc'], v['name'], v['name2'] = r.s(), r.s(), r.s(), r.s(), r.s()
        v['price'] = r.i(); v['outMoney'] = r.i(); r.i(); r.i(); r.i(); v['power'] = r.i()
        d['variations'].append(v)
    return d

def equip(r, d):
    d['capacity'] = r.i(); d['f120'] = r.i(); r.i()
    d['f154'] = [(r.i(), r.i(), r.i())[0] for _ in d['variations']]
    d['f134'] = r.i(); d['f138'] = r.i()
    n = r.i(); d['inMoney'] = [r.i() for _ in range(n)]

def tenant(r, d):
    names = ['registID', 'downFlag', 'numOfKind', 'numOfPattern', 'belongCapacity', 'senseOfNoise',
             'entranceV', 'entranceH', 'f16c', 'f170', 'f174', 'f178', 'customerUnit17c', 'maxBuildCount104']
    for k in names:
        d[k] = r.i()
    r.i(); r.i(); r.i(); d['movie'] = r.s()

def mover(r, d):
    t, l, b, rr = r.i(), r.i(), r.i(), r.i(); d['addSize(l,t,r,b)'] = (l, t, rr, b)
    d['maxLength'] = r.i(); d['minLength'] = r.i(); d['maxStop'] = r.i(); d['score'] = r.i()
    d['defaultWait'] = r.i(); d['speed'] = [r.i(), r.i(), r.i()]
    d['maxModule'] = r.i(); d['f188'] = r.i(); d['f18c'] = r.i(); d['f190'] = r.i(); d['f194'] = r.i()
    d['rect198'] = [r.i() for _ in range(4)]
    r.i(); r.i(); r.i(); r.i()

ATTR_BITS = {0x100: 'maint', 0x2000: 'perCell/stair', 0x4000: 'maintPerWidth', 0x8000: 'home'}

def decode(path):
    res = resources(path)
    key = ('TNDF', 1) if ('TNDF', 1) in res else ('MVDF', 1) if ('MVDF', 1) in res else None
    if key is None:
        return None
    r = R(res[key]); d = tool(r); equip(r, d)
    (tenant if key[0] == 'TNDF' else mover)(r, d)
    d['_kind'] = key[0]; d['_left'] = r.left()
    return d

def main():
    paths = []
    for a in sys.argv[1:]:
        if a.startswith('--'):
            continue
        if os.path.isdir(a):
            paths += [os.path.join(a, f) for f in sorted(os.listdir(a)) if f.lower().endswith('.t2p')]
        else:
            paths.append(a)
    for p in paths:
        with open(p, 'rb') as fh:
            if fh.read(2) != b'MZ':
                continue
        try:
            d = decode(p)
        except Exception as ex:
            print('%-22s ERROR %s' % (os.path.basename(p), ex)); continue
        if d is None:
            continue
        vs = d.pop('variations')
        vtxt = '; '.join('%s price=%d maint=%d power=%d' % (v['name'], v['price'], v['outMoney'], v['power']) for v in vs)
        d.pop('catDesc', None)
        print('%-22s %s | %s' % (os.path.basename(p), vtxt, {k: (hex(v) if k == 'attr' else v) for k, v in d.items()}))

if __name__ == '__main__':
    main()
