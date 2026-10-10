#!/usr/bin/env python3
"""Sweep YootTower World plugins, SP-format plugins, T2.dat/t2s.dat and
T2Icons.dll; exit non-zero on any failure.

Usage: python3 -I validate_world_sp.py [--game DIR] [--plugins DIR] [-v]
  --game     YootTower install dir holding T2.dat, t2s.dat, T2Icons.dll and
             Movie/ (default: $YOOT_DIR or ~/Downloads/Yoot-Tower_Win_EN/YOOT)
  --plugins  plugin dir (default: <game>/Plugins, falling back to the
             repo's data/Plugins)

Checks:
  * every decoded resource re-encodes byte-identically (world + SP);
  * SP containers rebuild byte-identically; exactly the expected 51 SP files
    (38 lPoM movies, 6 lPeH hall events, 7 lPdA adverts);
  * DESC kind <-> payload resource (lPoM:MODF, lPeH:HEDF, lPdA:ADDF);
    bitmap ids referenced by the payload exist; movie files exist in Movie/;
  * World invariants: BINF rows/cols == height/2, width/8 (tiled worlds),
    FINF == height x width (Kegon), build area inside the world, floor lines
    inside the build area, every T2EV entry resolves to a resource, every
    grade animation bitmap exists, background bitmap 1000 matches the map;
  * T2.dat / t2s.dat BMP round trip; T2Icons.dll groups resolve.
"""
import os
import struct
import sys

sys.dont_write_bytecode = True  # keep the repo free of __pycache__
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import t2dat  # noqa: E402
import world_sp as W  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
WORLDS = ('Tokyo.t2p', 'Hawai.t2p', 'Kegon.t2p')
EXPECTED_SP = {'lPoM': 38, 'lPeH': 6, 'lPdA': 7}
PAYLOAD = {'lPoM': 'MODF', 'lPeH': 'HEDF', 'lPdA': 'ADDF'}


class V:
    def __init__(self, verbose):
        self.fail = 0
        self.ok = 0
        self.verbose = verbose

    def check(self, cond, msg):
        if cond:
            self.ok += 1
            if self.verbose:
                print('  ok   ', msg)
        else:
            self.fail += 1
            print('  FAIL ', msg)
        return cond


def dib_size(data):
    _, w, h = struct.unpack_from('<Iii', data, 0)
    return w, abs(h)


def validate_world(v, path):
    name = os.path.basename(path)
    print('[world]', name)
    for t, i, st in W.roundtrip_plugin(path):
        if st != 'undecoded':
            v.check(st == 'ok', '%s %s %s round trip: %s' % (name, t, i, st))
    kind, res = W.load_plugin(path)
    by = {}
    for r in res:
        by[(r['name'], r['id'])] = r['data']
    names = {r['name'] for r in res}
    desc = W.decode_schema(by[('DESC', 128)], W.DESC)
    v.check(desc['kind'] == 'lPdW', '%s DESC is lPdW' % name)
    wd = W.decode_schema(by[('WORLDDEF', 1)], W.WORLDDEF)
    H, Wd = wd['height'], wd['width']
    ba = wd['buildArea']
    v.check(0 <= ba['left'] < ba['right'] <= Wd and 0 <= ba['top'] < ba['bottom'] <= H,
            '%s build area %s inside %dx%d' % (name, ba, Wd, H))
    v.check(ba['top'] <= wd['topFloorLine'] < wd['groundLine'] <= wd['bottomFloorLine'] < ba['bottom'],
            '%s floor lines top=%d ground=%d bottom=%d within build area' % (
                name, wd['topFloorLine'], wd['groundLine'], wd['bottomFloorLine']))
    v.check(wd['numGrades'] <= 8 and len(wd['cashAmounts']) <= 8, '%s grade/cash table sizes' % name)
    bmps = {r['id']: r['data'] for r in res if r['name'] == 'RT_BITMAP'}
    start = wd['startGrade']
    for gi, g in enumerate(wd['grades']):
        if gi + 1 < start:
            continue  # grade-up from a grade below the start grade: never used (Kegon refs missing 1206)
        if g['gradeUpAnimID']:
            ok = g['gradeUpAnimID'] in bmps
            if ok:
                w_, h_ = dib_size(bmps[g['gradeUpAnimID']])
                ok = h_ == g['gradeUpAnimFrames'] * 96
            v.check(ok, '%s grade anim %d has %d frames of 96px' % (name, g['gradeUpAnimID'], g['gradeUpAnimFrames']))
    for gi, gid in enumerate(wd['gradeIconAnimIDs']):
        if gid and gi + 1 >= start:
            v.check(gid in bmps, '%s grade icon bitmap %d exists' % (name, gid))
    if wd['tiledBkgnd']:
        v.check('BINF' in names, '%s tiled background has BINF' % name)
        b = W.decode_binf(by[('BINF', 1)])
        v.check(b['rows'] * 2 == H and b['cols'] * 8 == Wd,
                '%s BINF %dx%d == height/2 x width/8' % (name, b['rows'], b['cols']))
        tw, th = dib_size(bmps[1000])
        v.check(tw == wd['bkgndTileWidth'], '%s tile sheet width %d == bkgndTileWidth' % (name, tw))
        ntiles = th // wd['bkgndTileHeight']
        mx = max(max(row) & 0xffff for row in b['grid'])
        v.check(mx < ntiles, '%s BINF max tile %d < %d tiles in bitmap 1000' % (name, mx, ntiles))
    else:
        tw, th = dib_size(bmps[1000])
        v.check(tw == Wd * 4 and th == H * 18,
                '%s single background %dx%d == width*4 x height*18 (half scale)' % (name, tw, th))
    if 'FINF' in names:
        f = W.decode_finf(by[('FINF', 1)])
        v.check(f['height'] == H and f['width'] == Wd, '%s FINF %dx%d == world' % (name, f['height'], f['width']))
    if ('T2EV', 1) in by:
        ev = W.decode_schema(by[('T2EV', 1)], W.T2EV)
        for e in ev['events']:
            rname = e['type'][::-1].upper()
            v.check((rname, e['resID']) in by, '%s T2EV %s -> resource %s %d' % (name, e['type'][::-1], rname, e['resID']))
    for pid in W.decode_schema(by[('PODF', 1)], W.PODF)['gradePoolIDs']:
        if pid:
            v.check(('PGDF', pid) in by, '%s PODF grade pool PGDF %d exists' % (name, pid))


def validate_sp(v, plugdir, moviedir):
    counts = {}
    files = sorted(f for f in os.listdir(plugdir) if f.lower().endswith('.t2p'))
    for f in files:
        path = os.path.join(plugdir, f)
        if not W.is_sp(path):
            continue
        data = open(path, 'rb').read()
        ents = W.read_sp(data)
        rt = W.write_sp(ents) == data
        v.check(rt, '%s SP container round trip' % f)
        for t, i, st in W.roundtrip_plugin(path):
            if st != 'undecoded':
                v.check(st == 'ok', '%s %s %s round trip: %s' % (f, t, i, st))
        by = {(e['name'], e['id']): e['data'] for e in ents}
        desc = W.decode_schema(by[('DESC', 128)], W.DESC)
        k = desc['kind']
        counts[k] = counts.get(k, 0) + 1
        pt = PAYLOAD.get(k)
        if not v.check(pt and (pt, 128) in by, '%s DESC %s has payload %s' % (f, k, pt)):
            continue
        p = W.decode_schema(by[(pt, 128)], W.SCHEMAS[pt])
        attr = W.decode_schema(by[('ATTR', 128)], W.ATTR)
        v.check(attr['name'] == p['title'], '%s ATTR name == payload title (%r)' % (f, p['title']))
        v.check(('BITMAP', p['imageID']) in by, '%s image bitmap %d exists' % (f, p['imageID']))
        if k in ('lPoM', 'lPeH'):
            still = p['stillImageID']
            v.check(('BITMAP', still) in by, '%s still bitmap %d exists' % (f, still))
            v.check(('TMDF', p['customerTableID']) in by, '%s customer table TMDF %d exists' % (f, p['customerTableID']))
            if moviedir:
                v.check(os.path.exists(os.path.join(moviedir, p['movieFile'])),
                        '%s movie file Movie/%s exists' % (f, p['movieFile']))
    total = sum(counts.values())
    print('[sp] %d SP files: %s' % (total, counts))
    v.check(counts == EXPECTED_SP, 'SP kind counts %s == %s' % (counts, EXPECTED_SP))


def validate_dat(v, game):
    for n in ('T2.dat', 't2s.dat'):
        p = os.path.join(game, n)
        if not os.path.exists(p):
            v.check(False, '%s present' % n)
            continue
        data = open(p, 'rb').read()
        b = t2dat.read_bmp(data)
        v.check(t2dat.write_bmp(b) == data, '%s BMP round trip (%dx%d %dbpp)' % (n, b['width'], b['height'], b['bpp']))
    p = os.path.join(game, 'T2Icons.dll')
    if os.path.exists(p):
        d = t2dat.read_icons_dll(p)
        v.check(len(d['groups']) == 9, 'T2Icons.dll has 9 icon groups')
        for gid, gd in d['groups'].items():
            ok = all(e['id'] in d['icons'] and d['icons'][e['id']].__len__() == e['size']
                     for e in t2dat.parse_group(gd))
            v.check(ok, 'T2Icons group %d resolves' % gid)
    else:
        v.check(False, 'T2Icons.dll present')


def main(argv):
    verbose = '-v' in argv
    game = os.environ.get('YOOT_DIR', os.path.expanduser('~/Downloads/Yoot-Tower_Win_EN/YOOT'))
    plug = None
    if '--game' in argv:
        game = argv[argv.index('--game') + 1]
    if '--plugins' in argv:
        plug = argv[argv.index('--plugins') + 1]
    if plug is None:
        plug = os.path.join(game, 'Plugins')
        if not os.path.isdir(plug):
            plug = os.path.join(REPO, 'data', 'Plugins')
    moviedir = os.path.join(game, 'Movie')
    if not os.path.isdir(moviedir):
        moviedir = None
    v = V(verbose)
    for w in WORLDS:
        p = os.path.join(plug, w)
        if v.check(os.path.exists(p), '%s present' % w):
            validate_world(v, p)
    validate_sp(v, plug, moviedir)
    if os.path.isdir(game):
        validate_dat(v, game)
    else:
        print('[dat] game dir %s missing; skipping T2.dat/t2s.dat/T2Icons.dll' % game)
    print('checks: %d ok, %d failed' % (v.ok, v.fail))
    return 1 if v.fail else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
