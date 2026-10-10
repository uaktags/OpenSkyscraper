#!/usr/bin/env python3
"""Classify the CODE of every PE-format YootTower .t2p plugin: pure-data vs custom behavior.

Each .t2p statically links the same PowerPlant/T2 framework and imports the runtime from
T2DLL.dll. Behavior is customized only by subclassing a T2DLL class and overriding virtuals.
Every plugin exports two factories:

  ConstructProgramPlugin(spec)          -> new <PluginClass>(DESC-tag, spec)   e.g. T2TenantPlugin
  ConstructTemplatePlugin(spec, resfile)-> new <DefClass>(FD-tag, spec, ...)   e.g. T2TenantDef

Each factory calls a tiny LOCAL constructor that (1) calls the imported T2DLL base constructor and
(2) stores the subclass vftable (`mov dword ptr [esi], <vftable>`). We:

  1. disassemble both factories + local ctors (capstone) -> base class + plugin vftable address;
  2. name every slot from T2DLL's exported base vftable (??_7<Base>@@6B@ + export table);
  3. a slot is an OVERRIDE when it points at a local, non-exported function (exported locals are
     header-inline framework methods, e.g. IsMover/IsTenant, identical in every plugin);
  4. summarize each override: instruction count, imported T2DLL calls, a normalized fingerprint
     (mnemonics + operands with addresses masked) so identical behaviors cluster across plugins.

Requires: pefile, capstone (falls back to the user site-packages when run with -I).

Usage: python3 -I tools/yoottower/plugin_code_audit.py <Plugins dir> <T2DLL.dll> [--json out.json]
"""
import sys, os, re, json, struct, hashlib, collections, site

try:
    import pefile
except ImportError:  # pragma: no cover
    sys.path.append(site.getusersitepackages())
    import pefile
try:
    import capstone
except ImportError:  # pragma: no cover
    sys.path.append(site.getusersitepackages())
    import capstone

CS = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def demangle(n):
    m = re.match(r'\?\?0(\w+)@@', n)
    if m:
        return f"{m.group(1)}::{m.group(1)}"
    m = re.match(r'\?\?1(\w+)@@', n)
    if m:
        return f"{m.group(1)}::~{m.group(1)}"
    m = re.match(r'\?\?_[EG](\w+)@@', n)
    if m:
        return f"{m.group(1)}::`deleting dtor'"
    m = re.match(r'\?(\w+)@(\w+)@@', n)
    if m:
        return f"{m.group(2)}::{m.group(1)}"
    return n


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        self.pe.parse_data_directories(directories=[0, 1, 5])
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.secs = [(self.base + s.VirtualAddress,
                      self.base + s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData),
                      s.Name.rstrip(b'\0').decode(), bool(s.Characteristics & 0x20000000))
                     for s in self.pe.sections]
        self.exports = {}
        if hasattr(self.pe, 'DIRECTORY_ENTRY_EXPORT'):
            for e in self.pe.DIRECTORY_ENTRY_EXPORT.symbols:
                if e.name:
                    self.exports.setdefault(self.base + e.address, e.name.decode())
        self.by_name = {v: k for k, v in self.exports.items()}
        self.imports = {}
        if hasattr(self.pe, 'DIRECTORY_ENTRY_IMPORT'):
            for d in self.pe.DIRECTORY_ENTRY_IMPORT:
                for i in d.imports:
                    self.imports[i.address] = (i.name or b'#%d' % i.ordinal).decode()
        self.thunks = {}
        self._scan_thunks()
        # incremental-link (ILT) jmp stubs: resolve exports through them too
        for va, name in list(self.exports.items()):
            r = self.resolve(va)
            if r != va:
                self.exports.setdefault(r, name)

    def resolve(self, va):
        """Follow `jmp rel32` stubs (incremental-link tables) to the real body."""
        for _ in range(4):
            o = va - self.base
            if 0 <= o < len(self.mem) - 5 and self.mem[o] == 0xE9:
                va = va + 5 + struct.unpack_from('<i', self.mem, o + 1)[0]
            else:
                break
        return va

    def _scan_thunks(self):
        for lo, hi, n, x in self.secs:
            if not x:
                continue
            blob = self.mem[lo - self.base:hi - self.base]
            for m in re.finditer(rb'\xff\x25', blob):
                if m.end() + 4 <= len(blob):
                    iat = struct.unpack_from('<I', blob, m.end())[0]
                    if iat in self.imports:
                        self.thunks[lo + m.start()] = demangle(self.imports[iat])

    def u32(self, va):
        o = va - self.base
        return struct.unpack_from('<I', self.mem, o)[0] if 0 <= o <= len(self.mem) - 4 else None

    def is_code(self, va):
        return any(lo <= va < hi and x for lo, hi, n, x in self.secs)

    def is_data(self, va):
        return any(lo <= va < hi and not x for lo, hi, n, x in self.secs)

    def disasm(self, va, limit=4000):
        """Linear sweep from va to the furthest reachable ret (handles early returns)."""
        code = self.mem[va - self.base:va - self.base + limit]
        out, furthest = [], va
        for ins in CS.disasm(code, va):
            if ins.address > furthest and out and out[-1].mnemonic in ('ret', 'jmp') \
                    and out[-1].address >= furthest:
                break
            out.append(ins)
            if ins.mnemonic.startswith('j') and ins.op_str.startswith('0x'):
                t = int(ins.op_str, 16)
                if va <= t < va + limit:
                    furthest = max(furthest, t)
            if ins.mnemonic == 'int3':
                break
        while out and out[-1].mnemonic == 'int3':
            out.pop()
        return out

    def callee(self, ins):
        if ins.mnemonic == 'call' and ins.op_str.startswith('0x'):
            return self.resolve(int(ins.op_str, 16))
        return None

    def name_of(self, va):
        va = self.resolve(va)
        if va in self.thunks:
            return self.thunks[va]
        if va in self.exports:
            return demangle(self.exports[va])
        return None


def t2dll_vtables(dll):
    vt = {}
    for va, name in dll.exports.items():
        m = re.match(r'\?\?_7(\w+)@@6B@$', name)
        if not m:
            continue
        slots = []
        for i in range(400):
            p = dll.u32(va + 4 * i)
            if p is None or not dll.is_code(p):
                break
            if i and (va + 4 * i) in dll.exports:
                break
            nm = dll.exports.get(p)
            slots.append(demangle(nm) if nm else f'sub_{p:08x}')
        vt[m.group(1)] = slots
    return vt


def summarize(im, va):
    ins = im.disasm(va)
    calls, norm = [], []
    for i in ins:
        t = im.callee(i)
        if t is not None:
            n = im.name_of(t)
            calls.append(n or f'local_{t:08x}')
            norm.append(f'call {n or "L"}')
            continue
        ops = re.sub(r'0x[0-9a-f]{6,8}', 'A', i.op_str)
        norm.append(f'{i.mnemonic} {ops}')
    h = hashlib.md5('\n'.join(norm).encode()).hexdigest()[:8]
    strings = []
    for i in ins:
        for m in re.finditer(r'0x(1[0-9a-f]{7})', i.op_str):
            a = int(m.group(1), 16)
            if im.is_data(a):
                o = a - im.base
                s = im.mem[o:o + 64].split(b'\0')[0]
                if len(s) >= 3 and all(32 <= c < 127 for c in s):
                    strings.append(s.decode())
    return {'insns': len(ins), 'hash': h, 'calls': calls, 'strings': strings}


def factory(im, export):
    """Return (tag, alloc_size, local_ctor_va, base_ctor_name, vtable_va)."""
    va = im.by_name.get(export)
    if va is None:
        return None
    va = im.resolve(va)
    tag = size = ctor = None
    pushes = []
    for i in im.disasm(va, 400):
        if i.mnemonic == 'push' and i.op_str.startswith('0x'):
            pushes.append(int(i.op_str, 16))
        t = im.callee(i)
        if t is not None:
            if size is None and pushes:
                size = pushes[-1]
            elif pushes and pushes[-1] > 0x40000000:
                tag, ctor = pushes[-1], t
    if ctor is None:
        return None
    base = vt = None
    if ctor in im.thunks:
        base = im.thunks[ctor]
    else:
        for i in im.disasm(ctor, 600):
            t = im.callee(i)
            if t is not None and base is None and t in im.thunks:
                base = im.thunks[t]
            m = re.match(r'dword ptr \[(e[a-z]{2})\], (0x[0-9a-f]+)$', i.op_str)
            if i.mnemonic == 'mov' and m and im.is_data(int(m.group(2), 16)):
                vt = int(m.group(2), 16)
    tag_s = struct.pack('<I', tag)[::-1].decode('latin-1') if tag else None
    return {'tag': tag_s, 'alloc': size, 'ctor': ctor, 'base': (base or '').split('::')[0],
            'vtable': vt}


def analyze_plugin(path, basevt):
    im = Image(path)
    out = {}
    for export, kind in (('ConstructProgramPlugin', 'plugin'), ('ConstructTemplatePlugin', 'def')):
        f = factory(im, export)
        if not f:
            out[kind] = None
            continue
        rec = {'tag': f['tag'], 'alloc': f['alloc'], 'base': f['base'], 'overrides': [],
               'vtable': f['vtable'] and hex(f['vtable'])}
        slots = basevt.get(f['base'], [])
        if f['vtable']:
            n = len(slots)
            # subclasses may append new virtuals after the base slots
            i = 0
            while True:
                p = im.u32(f['vtable'] + 4 * i)
                if p is not None:
                    p = im.resolve(p)
                if p is None or not im.is_code(p) or (i >= n and i > 0 and
                                                      im.u32(f['vtable'] + 4 * i) is None):
                    break
                if i >= n + 8:
                    break
                if i >= n and (f['vtable'] + 4 * i) in im.exports:
                    break
                if p not in im.thunks and p not in im.exports:
                    meth = slots[i] if i < n else f'NEW_VIRTUAL_{i}'
                    if i == 0 or '`deleting' in meth or meth.startswith('sub_'):
                        meth = meth if not meth.startswith('sub_') else f'slot{i}:{meth}'
                    s = summarize(im, p)
                    s['trivial'] = s['insns'] <= 3 and not s['calls']
                    rec['overrides'].append(dict(slot=i, method=meth, va=hex(p), **s))
                i += 1
        # slots whose body is byte-identical to the vtable's slot-0 destructor are compiler
        # generated (scalar/vector deleting dtors of the subclass), not behavior
        dtor = {o['hash'] for o in rec['overrides'] if o['slot'] == 0}
        for o in rec['overrides']:
            if o['slot'] and o['hash'] in dtor:
                o['trivial'] = True
        out[kind] = rec
    return out


TRIVIAL = ("`deleting dtor'",)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    pdir, dllp = sys.argv[1], sys.argv[2]
    dll = Image(dllp)
    basevt = t2dll_vtables(dll)
    res = {}
    for f in sorted(os.listdir(pdir)):
        p = os.path.join(pdir, f)
        if not f.lower().endswith('.t2p'):
            continue
        with open(p, 'rb') as fh:
            if fh.read(2) != b'MZ':
                continue
        res[f] = analyze_plugin(p, basevt)
    groups = collections.defaultdict(list)
    for f, r in res.items():
        parts = []
        for kind in ('plugin', 'def'):
            rec = r.get(kind)
            if not rec:
                parts.append(f'{kind}:NONE')
                continue
            ov = [o for o in rec['overrides'] if not o['trivial'] and not o['method'].startswith('slot0')]
            parts.append(f"{rec['base']}[" + ','.join(
                o['method'].split('::')[-1] + '#' + o['hash'][:4] for o in ov) + ']')
        sig = ' '.join(parts)
        groups[sig].append(f)
        r['signature'] = sig
        r['custom'] = any(not o['trivial'] and not o['method'].startswith('slot0')
                          for k in ('plugin', 'def') if r.get(k) for o in r[k]['overrides'])
    print(f"{len(res)} PE plugins; custom-behavior: {sum(r['custom'] for r in res.values())}; "
          f"pure-data: {sum(not r['custom'] for r in res.values())}")
    print("\n# behavior clusters (count, plugin-class[overrides] def-class[overrides], members)")
    for s, fs in sorted(groups.items(), key=lambda x: (-len(x[1]), x[0])):
        print(f"{len(fs):3d} {s}\n      {', '.join(fs)}")
    if '--json' in sys.argv:
        with open(sys.argv[sys.argv.index('--json') + 1], 'w') as fh:
            json.dump(res, fh, indent=1)


if __name__ == '__main__':
    main()
