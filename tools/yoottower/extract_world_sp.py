#!/usr/bin/env python3
"""Dump YootTower World/SP plugin art and data plus T2.dat/t2s.dat/T2Icons.dll
to a directory (NOT inside the repo).

Usage: python3 -I extract_world_sp.py [OUTDIR] [--game DIR] [--plugins DIR]
  OUTDIR defaults to ~/yoottower-decomp/extract/

Writes, per World plugin (<OUT>/<World>/):
  bitmaps/<id>.png        every RT_BITMAP; the palette-animated range is
                          filled with the base daylight colours (PALETTE 101/102)
  background.png          the composed backdrop: BINF tile map over 'Sky'
                          tiles (bitmap 1000), or bitmap 1000 scaled x2 for
                          untiled worlds -- 8 px/unit, 36 px/floor
  buildmask.png           build area / floor lines / FINF mask overlay
  sounds/<id>.wav         WAVE resources
  world.json              every decoded resource (world_sp.decode_plugin)
Per SP plugin (<OUT>/SP/<name>/): bitmaps/*.png + plugin.json.
<OUT>/dat/: T2.dat.png, t2s.dat.png, T2Icons_<n>.ico.
Needs Pillow + pefile.
"""
import io
import json
import os
import struct
import sys

sys.dont_write_bytecode = True  # keep the repo free of __pycache__
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import t2dat  # noqa: E402
import world_sp as W  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, '..', '..'))


def dib_to_image(data):
    from PIL import Image
    hs = struct.unpack_from('<I', data, 0)[0]
    bpp = struct.unpack_from('<H', data, 14)[0]
    ncol = struct.unpack_from('<I', data, 32)[0] or (1 << bpp if bpp <= 8 else 0)
    off = 14 + hs + 4 * ncol
    bmp = b'BM' + struct.pack('<IHHI', 14 + len(data), 0, 0, off) + data
    return Image.open(io.BytesIO(bmp))


def daylight_palette(by):
    """Index->RGB overrides for the palette-animated range (T2PaletteAnime::ChangeSkyColor
    0x10047bd0 / ChangeSE 0x10047a40): entry k of PALETTE 101 (sky then refraction colours,
    unfiltered = base daylight) lands on index mAnimStartIndex+k; SE kind 0 of PALETTE 102
    follows. Returns {index: (r,g,b)}."""
    pt = {f['name']: f['value'] for f in W.decode_ptdf(by[('PTDF', 1)])['fields']}
    s, nsky, nref = pt['mAnimStartIndex'], pt['mNumOfSkyColor'], pt['mNumOfRefrectColor']
    out = {}
    if ('PALETTE', 101) in by:
        for k, e in enumerate(W.decode_palette(by[('PALETTE', 101)])['entries'][:nsky + nref]):
            out[s + k] = (e[0] >> 8, e[1] >> 8, e[2] >> 8)
    if ('PALETTE', 102) in by:
        for k, e in enumerate(W.decode_palette(by[('PALETTE', 102)])['entries'][:pt['mNumOfSEColor']]):
            out[s + nsky + nref + k] = (e[0] >> 8, e[1] >> 8, e[2] >> 8)
    return {i: c for i, c in out.items() if i < 256}


def apply_palette(im, over):
    if im.mode != 'P' or not over:
        return im
    pal = im.getpalette()[:768]
    pal += [0] * (768 - len(pal))
    for i, c in over.items():
        pal[i * 3:i * 3 + 3] = list(c)
    im = im.copy()
    im.putpalette(pal)
    return im


def save_json(obj, path):
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(obj, f, indent=1, ensure_ascii=False)


def world(path, out):
    from PIL import Image, ImageDraw
    name = os.path.splitext(os.path.basename(path))[0]
    od = os.path.join(out, name)
    os.makedirs(os.path.join(od, 'bitmaps'), exist_ok=True)
    os.makedirs(os.path.join(od, 'sounds'), exist_ok=True)
    kind, res = W.load_plugin(path)
    by = {(r['name'], r['id']): r['data'] for r in res}
    over = daylight_palette(by)
    for r in res:
        if r['name'] == 'RT_BITMAP':
            try:
                apply_palette(dib_to_image(r['data']), over).save(os.path.join(od, 'bitmaps', '%s.png' % r['id']))
            except Exception as ex:  # keep going on odd DIBs
                print('  bitmap', r['id'], 'failed:', ex)
        elif r['name'] == 'WAVE':
            with open(os.path.join(od, 'sounds', '%s.wav' % r['id']), 'wb') as f:
                f.write(r['data'])
    save_json(W.decode_plugin(path), os.path.join(od, 'world.json'))

    wd = W.decode_schema(by[('WORLDDEF', 1)], W.WORLDDEF)
    H, Wd = wd['height'], wd['width']
    sheet = apply_palette(dib_to_image(by[('RT_BITMAP', 1000)]), over).convert('RGB')
    UW, FH = 8, 36  # px per unit / per floor at full zoom
    if wd['tiledBkgnd']:
        b = W.decode_binf(by[('BINF', 1)])
        tw, th = wd['bkgndTileWidth'], wd['bkgndTileHeight']
        bg = Image.new('RGB', (b['cols'] * tw, b['rows'] * th))
        for r_ in range(b['rows']):
            for c in range(b['cols']):
                t = b['grid'][r_][c] & 0xffff
                bg.paste(sheet.crop((0, t * th, tw, (t + 1) * th)), (c * tw, r_ * th))
        bg = bg.resize((Wd * UW, H * FH), Image.NEAREST)
    else:
        bg = sheet.resize((Wd * UW, H * FH), Image.NEAREST)
    bg.save(os.path.join(od, 'background.png'))

    # build-area overlay at 2 px/unit, 4 px/floor
    sx, sy = 2, 4
    ov = bg.resize((Wd * sx, H * sy), Image.BILINEAR).convert('RGBA')
    lay = Image.new('RGBA', ov.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    if ('FINF', 1) in by:
        f = W.decode_finf(by[('FINF', 1)])
        for r_ in range(f['height']):
            for c in range(f['width']):
                if f['grid'][r_][c]:
                    d.rectangle([c * sx, r_ * sy, c * sx + sx - 1, r_ * sy + sy - 1], fill=(255, 0, 0, 90))
    ba = wd['buildArea']
    d.rectangle([ba['left'] * sx, ba['top'] * sy, ba['right'] * sx - 1, ba['bottom'] * sy - 1], outline=(255, 255, 0, 255))
    for row, col in ((wd['topFloorLine'], (0, 255, 255, 255)), (wd['groundLine'], (0, 255, 0, 255)),
                     (wd['bottomFloorLine'], (255, 128, 0, 255))):
        d.line([0, row * sy, Wd * sx, row * sy], fill=col)
    Image.alpha_composite(ov, lay).save(os.path.join(od, 'buildmask.png'))
    print('  %s: %dx%d units, background %s' % (name, Wd, H, bg.size))


def sp(path, out):
    name = os.path.splitext(os.path.basename(path))[0]
    od = os.path.join(out, 'SP', name)
    os.makedirs(os.path.join(od, 'bitmaps'), exist_ok=True)
    for e in W.read_sp(open(path, 'rb').read()):
        if e['type'] == 0:
            dib_to_image(e['data']).save(os.path.join(od, 'bitmaps', '%d.png' % e['id']))
    save_json(W.decode_plugin(path), os.path.join(od, 'plugin.json'))


def main(argv):
    args = [a for a in argv[1:]]
    game = os.environ.get('YOOT_DIR', os.path.expanduser('~/Downloads/Yoot-Tower_Win_EN/YOOT'))
    plug = None
    if '--game' in args:
        i = args.index('--game'); game = args[i + 1]; del args[i:i + 2]
    if '--plugins' in args:
        i = args.index('--plugins'); plug = args[i + 1]; del args[i:i + 2]
    out = os.path.abspath(os.path.expanduser(args[0] if args else '~/yoottower-decomp/extract/'))
    if out == REPO or out.startswith(REPO + os.sep):
        print('refusing to extract into the repository (%s)' % out)
        return 2
    if plug is None:
        plug = os.path.join(game, 'Plugins')
        if not os.path.isdir(plug):
            plug = os.path.join(REPO, 'data', 'Plugins')
    os.makedirs(out, exist_ok=True)
    for w in ('Tokyo.t2p', 'Hawai.t2p', 'Kegon.t2p'):
        p = os.path.join(plug, w)
        if os.path.exists(p):
            world(p, out)
    n = 0
    for f in sorted(os.listdir(plug)):
        p = os.path.join(plug, f)
        if f.lower().endswith('.t2p') and W.is_sp(p):
            sp(p, out)
            n += 1
    print('  %d SP plugins' % n)
    dd = os.path.join(out, 'dat')
    os.makedirs(dd, exist_ok=True)
    for f in ('T2.dat', 't2s.dat', 'T2Icons.dll'):
        p = os.path.join(game, f)
        if os.path.exists(p):
            t2dat.main(['t2dat', 'extract', p, dd])
    print('wrote', out)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
