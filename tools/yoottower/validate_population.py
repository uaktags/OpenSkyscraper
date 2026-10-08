#!/usr/bin/env python3
"""Population sweep for tools/yoottower/t2p_schema.py.

For every PE-format .t2p under the plugin directory, decode every resource, push the result
through JSON (so the decoded form is proven serializable), re-encode it and compare bytes.

Per resource type it reports:
  n         resources seen            plugins   distinct plugins carrying it
  roundtrip byte-identical re-encodes  full      schema consumed every byte (no raw tail)
  partial   schema decoded a prefix, rest kept raw (trailing / raw_tail)
  textmode  CResFile text-form resources (values parsed, raw text kept)
  opaque    no structural decode (error or unknown type)

Exit status is non-zero if any resource fails to round-trip or a schema'd type fails to parse.

Usage: python3 -I tools/yoottower/validate_population.py [Plugins dir] [--verbose]
"""
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t2p_schema as S  # noqa: E402

DEFAULT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))), 'data', 'Plugins')


def classify(d):
    c = d['codec']
    if c == 'cresfile':
        tail = bool(d.get('trailing'))
        f = d['fields']
        if isinstance(f.get('tail'), str) and f['tail']:
            tail = True
        return 'partial' if tail else 'full'
    if c == 'cresfile-text':
        return 'textmode'
    if c == 'opaque':
        return 'opaque'
    if c in ('riff', 'clut', 'strings', 'version'):
        return 'partial' if d.get('trailing') else 'full'
    return 'full'  # text, dib


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    verbose = '--verbose' in sys.argv
    pdir = args[0] if args else DEFAULT_DIR
    stats = collections.defaultdict(lambda: collections.Counter())
    plugins = collections.defaultdict(set)
    failures, n_pe, n_sp = [], 0, 0
    for f in sorted(os.listdir(pdir)):
        if not f.lower().endswith('.t2p'):
            continue
        data = open(os.path.join(pdir, f), 'rb').read()
        if data[:2] != b'MZ':
            n_sp += 1
            continue
        n_pe += 1
        for t, i, lang, payload in S.read_resources(data):
            st = stats[t]
            st['n'] += 1
            plugins[t].add(f)
            try:
                d = json.loads(json.dumps(S.decode_resource(t, payload)))
                enc = S.encode_resource(t, d)
            except Exception as e:  # noqa: BLE001
                failures.append(f'{f} {t}:{i} exception {e}')
                continue
            kind = classify(d)
            st[kind] += 1
            if enc == payload:
                st['roundtrip'] += 1
            else:
                failures.append(f'{f} {t}:{i} round-trip mismatch ({len(enc)} vs {len(payload)})')
            if d['codec'] == 'opaque' and t in S.SCHEMAS:
                failures.append(f"{f} {t}:{i} schema parse error: {d.get('error')}")
            if verbose and kind in ('partial', 'opaque'):
                print(f'  {kind:8s} {f} {t}:{i} {d.get("error", "")}')
    print(f'PE plugins: {n_pe}  (SP-format skipped: {n_sp})')
    hdr = f"{'type':11s} {'n':>5s} {'plugins':>7s} {'roundtrip':>9s} {'full':>5s} " \
          f"{'partial':>7s} {'textmode':>8s} {'opaque':>6s}"
    print(hdr)
    print('-' * len(hdr))
    tot = collections.Counter()
    for t in sorted(stats, key=str):
        st = stats[t]
        tot.update(st)
        print(f"{t:11s} {st['n']:5d} {len(plugins[t]):7d} {st['roundtrip']:9d} {st['full']:5d} "
              f"{st['partial']:7d} {st['textmode']:8d} {st['opaque']:6d}")
    print('-' * len(hdr))
    print(f"{'TOTAL':11s} {tot['n']:5d} {'':7s} {tot['roundtrip']:9d} {tot['full']:5d} "
          f"{tot['partial']:7d} {tot['textmode']:8d} {tot['opaque']:6d}")
    pairs = sum(len(v) for v in plugins.values())
    print(f'type x plugin pairs: {pairs}; resources: {tot["n"]}; '
          f'byte-identical: {tot["roundtrip"]}/{tot["n"]}')
    if failures:
        print(f'\nFAILURES ({len(failures)}):')
        for x in failures[:200]:
            print('  ' + x)
        sys.exit(1)
    print('OK: every resource round-trips byte-identically')


if __name__ == '__main__':
    main()
