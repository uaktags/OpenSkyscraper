#!/usr/bin/env python3
"""Print / locate C++ vtables in a 32-bit PE (T2DLL.dll or a .t2p).

Usage:
  python3 -I vtable.py <pe> <funcs.tsv> dump <vtable VA hex> [count]
  python3 -I vtable.py <pe> <funcs.tsv> find <function VA hex>

funcs.tsv is the "<addr>\t<name>\t<size>" list written by the Ghidra
DumpAll.java helper. Entries are resolved to names; imported T2DLL thunks
show as T2DLL.DLL::...  Used to discover which T2WorldDef / T2TenantDef
virtuals a plugin overrides (slot-by-slot diff against T2DLL's vtable).
"""
import struct, sys
import pefile

def load(pe_path, tsv):
    pe = pefile.PE(pe_path, fast_load=True)
    names = {}
    for line in open(tsv):
        a, n = line.split('\t')[:2]
        names[int(a, 16)] = n
    return pe, names

def u32(pe, va):
    return struct.unpack('<I', pe.get_data(va - pe.OPTIONAL_HEADER.ImageBase, 4))[0]

def main():
    pe, names = load(sys.argv[1], sys.argv[2])
    cmd = sys.argv[3]
    if cmd == 'dump':
        va = int(sys.argv[4], 16); n = int(sys.argv[5]) if len(sys.argv) > 5 else 64
        for i in range(n):
            f = u32(pe, va + 4 * i)
            print('%3d +0x%03x %08x %s' % (i, 4 * i, f, names.get(f, '?')))
    elif cmd == 'find':
        target = struct.pack('<I', int(sys.argv[4], 16))
        base = pe.OPTIONAL_HEADER.ImageBase
        for s in pe.sections:
            data = s.get_data(); off = data.find(target)
            while off >= 0:
                print('%08x in %s' % (base + s.VirtualAddress + off, s.Name.rstrip(b'\0').decode()))
                off = data.find(target, off + 1)

if __name__ == '__main__':
    main()
