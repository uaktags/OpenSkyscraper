#!/usr/bin/env python3
"""List which T2DLL virtuals a plugin's Def class overrides with its own code.

Usage: python3 -I overrides.py <T2DLL.dll> <T2DLL.funcs.tsv> <plugin.t2p> <plugin.decomp.c> <plugin.funcs.tsv>

Finds ConstructTemplatePlugin in the plugin's Ghidra decompilation, follows it
to the Def constructor, takes the last vtable stored to *this, then compares
it slot-by-slot against the T2DLL base vtable for the plugin's Def class
(T2TenantDef 0x100871b8, T2MoverDef located via CalcMentenanceCostProc,
T2WorldDef 0x10087da0). A slot whose target starts with FF 25 (jmp [IAT]) is
an inherited import thunk; anything else is code compiled into the plugin.
If the plugin function's name equals the base slot's name it is an inline
header copy (same behaviour); otherwise it's a real override ("OVERRIDE").
"""
import re, struct, sys
import pefile

BASES = {'TNDF': 0x100871b8, 'MVDF': None, 'WORLDDEF': 0x10087da0}
NSLOTS = {'TNDF': 50, 'MVDF': 50, 'WORLDDEF': 80}

def names(tsv):
    d = {}
    for l in open(tsv):
        a, n = l.split('\t')[:2]
        d[int(a, 16)] = n
    return d

def u32(pe, va):
    return struct.unpack('<I', pe.get_data(va - pe.OPTIONAL_HEADER.ImageBase, 4))[0]

def func_body(src, name):
    m = re.search(r'// ==== FUNCTION %s @ ([0-9a-f]+)\n(.*?)(?=\n// ==== FUNCTION)' % re.escape(name), src, re.S)
    return m and m.group(2)

def main():
    dll = pefile.PE(sys.argv[1], fast_load=True); dn = names(sys.argv[2])
    pe = pefile.PE(sys.argv[3], fast_load=True); src = open(sys.argv[4]).read(); pn = names(sys.argv[5])
    body = func_body(src, 'ConstructTemplatePlugin')
    if not body:
        print('no ConstructTemplatePlugin'); return
    ctor = re.search(r'= \(?[A-Za-z0-9_ *]*\)?(FUN_[0-9a-f]+)\((?:[A-Za-z_0-9]+,)?0x([0-9a-f]+)', body)
    tag = struct.pack('>I', int(ctor.group(2), 16)).decode('latin1')[::-1].upper()
    kind = {'FDNT': 'TNDF', 'FDVM': 'MVDF', 'LPDW': 'WORLDDEF'}.get(tag)
    cb = func_body(src, ctor.group(1)) or ''
    vts = re.findall(r'\*\(undefined \*\*\*\)(?:param_1|this|\(?[a-z]*Var[0-9]+\)?) = &PTR_[A-Za-z_]*([0-9a-f]{8});', cb)
    print('ctor', ctor.group(1), 'tag', tag, 'kind', kind, 'vtable', vts[-1:] )
    if not vts or kind is None:
        return
    vt = int(vts[-1], 16)
    base = BASES.get(kind)
    for i in range(NSLOTS[kind]):
        t = u32(pe, vt + 4 * i)
        try:
            b = pe.get_data(t - pe.OPTIONAL_HEADER.ImageBase, 2)
        except Exception:
            b = b''
        bn = dn.get(u32(dll, base + 4 * i), '?') if base else '?'
        if b == b'\xff\x25':
            continue
        n = pn.get(t, 'FUN_%08x' % t)
        flag = 'inline-copy' if n.split('::')[-1] == bn.split('::')[-1] else 'OVERRIDE'
        print('slot %2d +0x%03x base=%-40s plugin=%08x %s  %s' % (i, 4 * i, bn, t, n, flag))

if __name__ == '__main__':
    main()
