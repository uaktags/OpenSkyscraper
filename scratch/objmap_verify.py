"""Programmatic verifier for the OBJMAP format hypotheses in objmap-format.md.

Sweeps the entire vendored plugin catalog and asserts that the format
claims hold across the population, not just the 5-plugin calibration set.
Reports any plugin that violates a claim so the format doc can be updated
before D.2 (the C++ parser) is built.

Usage: python3 scratch/objmap_verify.py
"""
import os
import re
import sys
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGINS = os.path.join(ROOT, "data", "Plugins")

# A DefParts block header. Three tab-separated fields: literal "DefParts",
# literal "Name", then the semantic name (which may contain spaces, as in
# "Burger land").
HEADER_RE = re.compile(rb"^DefParts\tName\t([^\r\n]+)\r?\n")
# An attribute line: two leading tabs, then key \t value.
ATTR_RE = re.compile(rb"^\t\t([^\t\r\n]+)\t([^\r\n]*)\r?\n")
# A Rect declaration inside a List attribute.
RECT_RE = re.compile(rb"^Rect (\d+),(\d+),(\d+),(\d+)(?:\s+([A-Za-z]+))?(?:\s+([A-Za-z]+))?")


def parse_objmap(raw):
    """Parse OBJMAP bytes per objmap-format.md. Returns a list of blocks."""
    if not raw or not raw.startswith(b"DefParts"):
        return None  # not the format we expect
    blocks = []
    current = None
    for line in raw.split(b"\r\n"):
        if not line:
            if current:
                blocks.append(current)
                current = None
            continue
        if line == b"End":
            break
        m = HEADER_RE.match(line + b"\r\n")
        if m:
            current = {"name": m.group(1).decode("ascii", "replace"), "attrs": {}}
            continue
        if current is None:
            continue
        m = ATTR_RE.match(line + b"\r\n")
        if m:
            current["attrs"].setdefault(m.group(1).decode("ascii", "replace"),
                                        m.group(2).decode("ascii", "replace"))
    if current:
        blocks.append(current)
    # Post-process: split the List attribute into rect + flags.
    for b in blocks:
        if "List" in b["attrs"]:
            m = RECT_RE.match(b["attrs"]["List"].encode("ascii", "replace"))
            if m:
                b["rect"] = tuple(int(x) for x in m.groups()[:4])
                b["flags"] = [g.decode("ascii") for g in m.groups()[4:] if g]
            else:
                b["rect"] = None
    return blocks


def check_claims():
    files = sorted(f for f in os.listdir(PLUGINS) if f.endswith(".t2p"))
    stats = {"plugins": 0, "parse_ok": 0, "objmap100_count": 0,
             "objmap1000_count": 0, "violations": []}

    for fn in files:
        try:
            pe = pefile.PE(os.path.join(PLUGINS, fn), fast_load=True)
            pe.parse_data_directories()
        except pefile.PEFormatError:
            continue  # SP-format; out of scope for this sweep
        stats["plugins"] += 1

        objmaps = {}
        for te in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            tn = str(te.name) if te.name else str(pefile.RESOURCE_TYPE.get(te.id, te.id))
            if tn != "OBJMAP":
                continue
            for ie in te.directory.entries:
                rid = ie.id if hasattr(ie, "id") and ie.id else None
                for le in ie.directory.entries:
                    objmaps[rid] = pe.get_data(le.data.struct.OffsetToData, le.data.struct.Size)

        if not objmaps:
            stats["violations"].append((fn, "no OBJMAP resources at all"))
            continue

        # Claim 1: every plugin starts with the "DefParts" magic.
        for rid, raw in objmaps.items():
            if not raw.startswith(b"DefParts"):
                stats["violations"].append((fn, f"OBJMAP {rid} doesn't start with DefParts"))
                break
        else:
            stats["parse_ok"] += 1

        # Claim 2: OBJMAP 100 always has exactly 5 blocks (3 Category + 2 item).
        if 100 in objmaps:
            stats["objmap100_count"] += 1
            blocks = parse_objmap(objmaps[100])
            if blocks and len(blocks) != 5:
                stats["violations"].append(
                    (fn, f"OBJMAP 100 has {len(blocks)} blocks, expected 5"))
            elif blocks:
                cats = sum(1 for b in blocks if b["name"] == "Category")
                if cats != 3:
                    stats["violations"].append(
                        (fn, f"OBJMAP 100 has {cats} Category blocks, expected 3"))

        # Claim 3: OBJMAP 1000 is parseable and has at least one View block.
        if 1000 in objmaps:
            stats["objmap1000_count"] += 1
            blocks = parse_objmap(objmaps[1000])
            if not blocks:
                stats["violations"].append((fn, "OBJMAP 1000 failed to parse"))
            else:
                views = [b for b in blocks if b["name"] == "View"]
                if not views:
                    stats["violations"].append(
                        (fn, "OBJMAP 1000 has no View blocks"))
                else:
                    # Every View must have a parseable Rect.
                    for v in views:
                        if not v.get("rect"):
                            stats["violations"].append(
                                (fn, f"View block without Rect: {v}"))

    return stats


def main():
    if not os.path.isdir(PLUGINS):
        sys.exit(f"Plugin dir not found: {PLUGINS}")
    stats = check_claims()
    print(f"Swept {stats['plugins']} PE-format plugins")
    print(f"  All-defparts-magic: {stats['parse_ok']}/{stats['plugins']}")
    print(f"  Plugins with OBJMAP 100:  {stats['objmap100_count']}")
    print(f"  Plugins with OBJMAP 1000: {stats['objmap1000_count']}")
    print(f"  Violations: {len(stats['violations'])}")
    if stats["violations"]:
        print("\nFirst 10 violations:")
        for fn, msg in stats["violations"][:10]:
            print(f"  {fn}: {msg}")
    return 0 if not stats["violations"] else 1


if __name__ == "__main__":
    sys.exit(main())
