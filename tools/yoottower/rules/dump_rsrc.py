#!/usr/bin/env python3
"""Dump a PE file's resource directory summary and its STRINGTABLE entries.

Usage: python3 -I dump_rsrc.py <file.exe|.dll|.t2p> [--strings] [--type NAME --id N --raw]

Read-only research helper for doc/yoottower/game-parity.md. Prints
"type/id/lang size" lines, then (with --strings) every RT_STRING entry as
"<string id>\t<text>". With --type/--id/--raw it writes the raw resource bytes
to stdout (hex dump) so custom YootTower resources can be inspected.
"""
import sys
import pefile

def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__); return 2
    pe = pefile.PE(a[0], fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
    want_type = a[a.index('--type') + 1] if '--type' in a else None
    want_id = int(a[a.index('--id') + 1]) if '--id' in a else None
    root = getattr(pe, 'DIRECTORY_ENTRY_RESOURCE', None)
    if root is None:
        print('no resources'); return 1
    for t in root.entries:
        tname = str(t.name) if t.name else pefile.RESOURCE_TYPE.get(t.id, str(t.id))
        for e in t.directory.entries:
            rid = str(e.name) if e.name else e.id
            for l in e.directory.entries:
                d = l.data.struct
                data = pe.get_memory_mapped_image()[d.OffsetToData:d.OffsetToData + d.Size]
                if want_type:
                    if tname == want_type and (want_id is None or rid == want_id):
                        if '--raw' in a:
                            sys.stdout.buffer.write(data)
                        else:
                            for off in range(0, len(data), 16):
                                ch = data[off:off + 16]
                                print('%06x  %-48s %s' % (off, ch.hex(' '), ''.join(chr(c) if 32 <= c < 127 else '.' for c in ch)))
                    continue
                if tname == 'RT_STRING' and '--strings' in a:
                    base = (e.id - 1) * 16
                    p = 0
                    for i in range(16):
                        n = int.from_bytes(data[p:p + 2], 'little'); p += 2
                        s = data[p:p + 2 * n].decode('utf-16-le', 'replace'); p += 2 * n
                        if s:
                            print('%d\t%s' % (base + i, s.replace('\n', '\\n').replace('\r', '\\r')))
                elif '--strings' not in a:
                    print('%s/%s/%s %d' % (tname, rid, l.id, d.Size))
    return 0

if __name__ == '__main__':
    sys.exit(main())
