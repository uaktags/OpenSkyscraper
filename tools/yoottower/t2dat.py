#!/usr/bin/env python3
"""Reference parser/writer for YootTower's T2.dat, t2s.dat and T2Icons.dll.

Findings (see doc/yoottower/world-and-sp-formats.md):
  * T2.dat  -- a plain Windows BMP (BITMAPFILEHEADER + BITMAPINFOHEADER,
    640x480, 8bpp, 256-colour table, uncompressed). T2.exe opens
    "<TowerDir>T2.dat" in the opening/splash frame window
    ("T2OpenningWindow", T2.exe FUN_004162e0) through its DIB loader
    FUN_0040b9d0. It is the title/splash screen.
  * t2s.dat -- same format, 460x320 8bpp; T2.exe opens "T2S.dat" in the
    start-up T2Dialog (FUN_00416ad0) -- backdrop of the new/open game dialog
    (art on the left 240 px, flat grey on the right for the controls).
  * T2Icons.dll -- not game art: a COM in-proc server ("Tower2 Plugin
    Helper", registered by T2.exe FUN_0040d3e0 under CLSID
    {F6FC9820-57D7-11d2-A2FD-...}) that gives .t2p files per-kind Explorer
    icons (9 RT_GROUP_ICONs, keyed by DESC kind lPnT/lPvM/lPoO/lPdW/lPeH/
    lPdA/lPoM/LPOT/LPPO) and a property page (dialog 132: category, plugin
    name, plugin type, grade, price).

Usage (python3 -I):
  t2dat.py info <T2.dat|t2s.dat|T2Icons.dll>
  t2dat.py roundtrip <T2.dat|t2s.dat>        exit 1 unless byte-identical
  t2dat.py extract <file> <outdir>           PNG (needs Pillow) / .ico files
"""
import os
import struct
import sys


class FormatError(Exception):
    pass


# ---------------------------------------------------------------------------
# BMP (T2.dat / t2s.dat)
# ---------------------------------------------------------------------------

def read_bmp(data):
    if data[:2] != b'BM':
        raise FormatError('not a BMP')
    bf_size, r1, r2, off_bits = struct.unpack_from('<IHHI', data, 2)
    (bi_size, w, h, planes, bpp, comp, size_image, xppm, yppm,
     clr_used, clr_imp) = struct.unpack_from('<IiiHHIIiiII', data, 14)
    if bi_size != 40:
        raise FormatError('unsupported info header size %d' % bi_size)
    ncol = clr_used or (1 << bpp if bpp <= 8 else 0)
    pal_off = 14 + bi_size
    palette = [tuple(data[pal_off + 4 * k:pal_off + 4 * k + 4]) for k in range(ncol)]  # B,G,R,0
    gap = data[pal_off + 4 * ncol:off_bits]
    stride = ((w * bpp + 31) // 32) * 4
    rows = abs(h)
    pix_len = stride * rows
    pixels = data[off_bits:off_bits + pix_len]
    if len(pixels) != pix_len:
        raise FormatError('truncated pixel data')
    trailer = data[off_bits + pix_len:]
    return {
        'bfSize': bf_size, 'bfReserved': (r1, r2), 'bfOffBits': off_bits,
        'width': w, 'height': h, 'planes': planes, 'bpp': bpp, 'compression': comp,
        'sizeImage': size_image, 'ppm': (xppm, yppm), 'clrUsed': clr_used,
        'clrImportant': clr_imp, 'palette': palette, 'gap': gap,
        'stride': stride, 'pixels': pixels, 'trailer': trailer,
    }


def write_bmp(b):
    out = bytearray(b'BM')
    out += struct.pack('<IHHI', b['bfSize'], b['bfReserved'][0], b['bfReserved'][1], b['bfOffBits'])
    out += struct.pack('<IiiHHIIiiII', 40, b['width'], b['height'], b['planes'], b['bpp'],
                       b['compression'], b['sizeImage'], b['ppm'][0], b['ppm'][1],
                       b['clrUsed'], b['clrImportant'])
    for e in b['palette']:
        out += bytes(e)
    out += b['gap']
    out += b['pixels']
    out += b['trailer']
    return bytes(out)


def bmp_rgb_rows(b):
    """Top-down list of rows of (r,g,b) tuples (8bpp only)."""
    if b['bpp'] != 8 or b['compression'] != 0:
        raise FormatError('only 8bpp uncompressed supported')
    w, h, st = b['width'], abs(b['height']), b['stride']
    pal = [(e[2], e[1], e[0]) for e in b['palette']]
    rows = []
    for y in range(h):
        src = y if b['height'] < 0 else h - 1 - y
        line = b['pixels'][src * st:src * st + w]
        rows.append([pal[i] for i in line])
    return rows


def bmp_to_png(b, path):
    from PIL import Image
    w, h = b['width'], abs(b['height'])
    im = Image.new('RGB', (w, h))
    im.putdata([px for row in bmp_rgb_rows(b) for px in row])
    im.save(path)


# ---------------------------------------------------------------------------
# T2Icons.dll
# ---------------------------------------------------------------------------

def read_icons_dll(path):
    import pefile
    pe = pefile.PE(path)
    icons, groups, dialogs = {}, {}, {}
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        for e in t.directory.entries:
            lang = e.directory.entries[0]
            d = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
            if t.id == 3:
                icons[e.id] = d
            elif t.id == 14:
                groups[e.id] = d
            elif t.id == 5:
                dialogs[e.id] = d
    exports = [s.name.decode() for s in pe.DIRECTORY_ENTRY_EXPORT.symbols] if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT') else []
    return {'icons': icons, 'groups': groups, 'dialogs': dialogs, 'exports': exports}


def parse_group(gd):
    reserved, typ, count = struct.unpack_from('<HHH', gd, 0)
    ents = []
    for k in range(count):
        w, h, cc, res, planes, bits, size, nid = struct.unpack_from('<BBBBHHIH', gd, 6 + 14 * k)
        ents.append({'w': w or 256, 'h': h or 256, 'colors': cc, 'planes': planes,
                     'bits': bits, 'size': size, 'id': nid})
    return ents


def build_ico(gd, icons):
    """Rebuild a standalone .ico from an RT_GROUP_ICON + its RT_ICON images."""
    ents = parse_group(gd)
    out = bytearray(struct.pack('<HHH', 0, 1, len(ents)))
    off = 6 + 16 * len(ents)
    blobs = []
    for k, e in enumerate(ents):
        img = icons[e['id']]
        raw = gd[6 + 14 * k:6 + 14 * k + 12]
        out += raw + struct.pack('<I', off)
        off += len(img)
        blobs.append(img)
    for b in blobs:
        out += b
    return bytes(out)


# ---------------------------------------------------------------------------

def kind_of(path):
    with open(path, 'rb') as f:
        head = f.read(2)
    return {'BM': 'bmp', 'MZ': 'pe'}.get(head.decode('latin1'), 'unknown')


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd, path = argv[1], argv[2]
    k = kind_of(path)
    if cmd == 'info':
        if k == 'bmp':
            b = read_bmp(open(path, 'rb').read())
            print('BMP %dx%d %dbpp comp=%d colours=%d offBits=%d trailer=%d' % (
                b['width'], b['height'], b['bpp'], b['compression'], len(b['palette']),
                b['bfOffBits'], len(b['trailer'])))
        elif k == 'pe':
            d = read_icons_dll(path)
            print('exports:', d['exports'])
            for gid, gd in sorted(d['groups'].items()):
                print('group', gid, [(e['w'], e['h'], e['bits'] or e['colors'], e['id']) for e in parse_group(gd)])
            print('dialogs:', sorted(d['dialogs']))
        return 0
    if cmd == 'roundtrip':
        data = open(path, 'rb').read()
        ok = write_bmp(read_bmp(data)) == data
        print('ok' if ok else 'MISMATCH')
        return 0 if ok else 1
    if cmd == 'extract':
        out = argv[3]
        os.makedirs(out, exist_ok=True)
        base = os.path.basename(path)
        if k == 'bmp':
            bmp_to_png(read_bmp(open(path, 'rb').read()), os.path.join(out, base + '.png'))
        elif k == 'pe':
            d = read_icons_dll(path)
            for gid, gd in d['groups'].items():
                with open(os.path.join(out, 'T2Icons_%d.ico' % gid), 'wb') as f:
                    f.write(build_ico(gd, d['icons']))
        return 0
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
