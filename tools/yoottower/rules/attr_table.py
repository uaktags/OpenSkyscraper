#!/usr/bin/env python3
"""Tabulate the ATTR (id 128) + DESC resources of every PE .t2p plugin.

Usage: python3 -I attr_table.py <Plugins dir>

Columns: file, DESC class tag, mItemType, mAttrib (hex), mDependID (4cc at
ATTR+9, '****' = none), mGameLevel, ATTR+17 (mMustItem?), mID, ATTR+25
(mSecretItem?), mPluginName. The CFilePlugin ctor (T2DLL 0x1000df20) reads the
fields in this order; see doc/yoottower/game-parity.md section "Unlocks".
"""
import os, sys, struct
import pefile

def res(pe, tname, rid):
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        n = str(t.name) if t.name else None
        if n != tname:
            continue
        for e in t.directory.entries:
            if e.id == rid:
                d = e.directory.entries[0].data.struct
                return pe.get_memory_mapped_image()[d.OffsetToData:d.OffsetToData + d.Size]
    return None

def main(d):
    for f in sorted(os.listdir(d)):
        if not f.lower().endswith('.t2p'):
            continue
        p = os.path.join(d, f)
        with open(p, 'rb') as fh:
            if fh.read(2) != b'MZ':
                continue
        try:
            pe = pefile.PE(p, fast_load=True)
            pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
            a = res(pe, 'ATTR', 128); ds = res(pe, 'DESC', 128)
        except Exception as ex:
            print(f, 'ERR', ex); continue
        if not a:
            print('%-24s no ATTR' % f); continue
        it = a[1:5].decode('latin1'); attrib, = struct.unpack_from('<I', a, 5)
        dep = a[9:13].decode('latin1')
        lvl, must, mid, sec = struct.unpack_from('<IIII', a, 13)
        n = a[29]; name = a[30:30 + n].decode('latin1')
        cls = ds[1:5].decode('latin1') if ds else '----'
        print('%-24s %s %s %08x %s lvl=%d must=%d id=%d sec=%d %s' % (f, cls, it, attrib, dep, lvl, must, mid, sec, name))

if __name__ == '__main__':
    main(sys.argv[1])
