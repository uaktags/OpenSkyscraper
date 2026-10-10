#!/usr/bin/env python3
"""Decode the game-rule parts of a YootTower World plugin (Tokyo/Hawai/Kegon).

Usage: python3 -I world_rules.py <World.t2p> [--events]

Field order mirrors the T2DLL constructors (addresses in comments) so every
printed value is traceable to code; see doc/yoottower/game-parity.md.
CResFile binary mode: a leading 0x01 byte, then every scalar is a 4-byte LE
int (even `char`), strings are <u8 len><len bytes><NUL>, POINT = y,x and
RECT = top,left,bottom,right (CResFile::operator>> 0x100116e0/0x10011730).
"""
import struct, sys
import pefile

class R:
    def __init__(self, b):
        assert b[0] == 1, 'not a binary CResFile resource'
        self.b, self.p = b, 1
    def i(self):
        v, = struct.unpack_from('<i', self.b, self.p); self.p += 4; return v
    def s(self):
        n = self.b[self.p]; v = self.b[self.p + 1:self.p + 1 + n].decode('latin1')
        self.p += n + 2; return v
    def left(self):
        return len(self.b) - self.p

def resources(path):
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
    out = {}
    img = pe.get_memory_mapped_image()
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(t.name) if t.name else str(t.id)
        for e in t.directory.entries:
            d = e.directory.entries[0].data.struct
            out[(tn, e.id)] = img[d.OffsetToData:d.OffsetToData + d.Size]
    return out

def fourcc(v):
    return struct.pack('>I', v & 0xffffffff).decode('latin1')

def world(res):
    r = R(res[('WORLDDEF', 1)])
    w = {}
    # T2WorldDef::T2WorldDef @ 0x1006e4e0
    w['sortKey+0x50'] = r.i(); w['height+0x1f8'] = r.i(); w['width+0x1fc'] = r.i()
    w['groundLine+0x54'] = r.i()
    top, left, bottom, right = r.i(), r.i(), r.i(), r.i()
    w['buildArea(l,t,r,b)+0x200'] = (left, top, right, bottom)
    y, x = r.i(), r.i(); w['pt+0x60(x,y)'] = (x, y)
    for k in ('+0x68', '+0x6c', '+0x70', '+0x74'):
        w[k] = r.i()
    w['lobbyHeight+0x58'] = r.i(); w['outViewKind+0x218'] = r.i(); w['+0x17c'] = r.i(); w['+0x228'] = r.i()
    n = r.i(); w['numGrades+0x84'] = n
    grades = []
    for _ in range(n):
        grades.append(dict(zip(('vipMask', 'population', 'animImage', 'animA', 'animB', 'animC'), [r.i() for _ in range(6)])))
    w['grades'] = grades
    w['+0x180..0x18c'] = [r.i() for _ in range(4)]
    w['discarded10'] = [r.i() for _ in range(10)]
    w['+0x148..0x158'] = [r.i() for _ in range(5)]
    w['name+0x15c'] = r.s()
    w['dustOnLevel+0x170'] = r.i(); w['toiletLevel+0x174'] = r.i(); w['toiletDenomi+0x178'] = r.i()
    w['discarded'] = r.i()
    c = r.i(); w['cashAmounts+0x22c'] = [r.i() for _ in range(c)]
    w['_unparsed_bytes'] = r.left()
    a = R(res[('WATR', 128)])
    w['WAtr.discarded6'] = [a.i() for _ in range(6)]
    w['initialFunds+0x24c'] = a.i(); w['exchangeNum+0x250'] = a.i(); w['exchangeDen+0x254'] = a.i()
    w['moneyUnit+600'] = a.s()
    return w

def events(res):
    if ('T2EV', 1) not in res:
        return []
    r = R(res[('T2EV', 1)])
    out = []
    for _ in range(r.i()):
        typ = r.i(); rid = r.i()
        tn = fourcc(typ).upper()
        key = next((k for k in res if k[0] == tn and k[1] == rid), None)
        if key is None:
            out.append((fourcc(typ), rid, 'MISSING RESOURCE', None)); continue
        e = R(res[key])
        # T2EventItem::T2EventItem(T2TowerEvent*,CResFile*,int) @ 0x10030050
        hdr = dict(levelBit=e.i(), exclusive=e.i(), cycleDay=e.i(), beginMin=e.i(), endMin=e.i())
        body = []
        try:
            if tn == 'VIEV':  # T2VisitVIP ctor 0x1006cb40 + Init 0x1006d360
                for _ in range(e.i()):
                    v = dict(level=e.i(), destPluginID=e.i(), f38=e.i(), f3c=e.i(), f40=e.i(), f44=e.i(), viplRes=e.i())
                    v['goodMsg'] = e.s(); v['badMsg'] = e.s(); body.append(v)
            else:
                while e.left() >= 4:
                    body.append(e.i())
        except Exception as ex:
            body.append('ERR %s' % ex)
        out.append((fourcc(typ), rid, hdr, body))
    return out

def main():
    path = sys.argv[1]
    res = resources(path)
    for k, v in world(res).items():
        if k == 'grades':
            for i, g in enumerate(v):
                print('  grade[%d] (star %d -> %d): %s' % (i, i + 1, i + 2, g))
        else:
            print('%-28s %s' % (k, v))
    if '--events' in sys.argv:
        for ev in events(res):
            print('EVENT', ev[0], ev[1], ev[2])
            for b in (ev[3] if isinstance(ev[3], list) else []):
                print('    ', b)
            if not isinstance(ev[3], list):
                print('    ', ev[3])

if __name__ == '__main__':
    main()
