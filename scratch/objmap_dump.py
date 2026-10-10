"""Dumps YootTower plugin OBJMAP resources for reverse-engineering.

The OBJMAP resource declares how a plugin's RT_BITMAP sheet is sliced
into sprites, animation frames, etc. We don't know its format yet -
this script dumps the bytes of OBJMAP 100, 1000, and 1100 from a small
calibration set so the layout can be inferred.

The calibration oracle is scratch/extract_t2p_stairs.py, which knows
that Stair.t2p OBJMAP 1000 corresponds to a 64x288 sheet with three
spans at y=0/60/156. Any struct-layout hypothesis must reproduce those
numbers from the bytes this script dumps.

Usage: python3 scratch/objmap_dump.py
"""
import os
import struct
import sys
import pefile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGINS = os.path.join(ROOT, "data", "Plugins")

# (filename, label, why it's interesting)
CALIBRATION = [
    ("Stair.t2p",     "transport", "KNOWN ORACLE: extract_t2p_stairs.py knows OBJMAP 1000 -> 3 spans at y=0/60/156 in a 64x288 sheet"),
    ("Burger.t2p",    "shop",      "Standard commercial plugin with day/night variants and walking-people anim"),
    ("Condo.t2p",     "room",      "Residential: 3 variants x 5 lighting states (the YootCondo sheet is 128x360)"),
    ("Cathedral.t2p", "venue",     "Large unique item: 4 state bitmaps, no people anim"),
    ("StdElev.t2p",   "transport", "Elevator: vertical transport, different shape from stairs"),
]


def hex_dump(data, width=16, max_lines=None):
    out = []
    n = len(data)
    for off in range(0, n, width):
        if max_lines and off // width >= max_lines:
            out.append(f"  ... ({n - off} more bytes)")
            break
        chunk = data[off:off + width]
        hex_part = " ".join(f"{b:02x}" for b in chunk)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        out.append(f"  {off:04x}  {hex_part:<{width*3}}  {ascii_part}")
    return "\n".join(out)


def get_resources(pe, type_name, type_id):
    """Yields (id, bytes) for every resource of a given type."""
    for te in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tn = str(te.name) if te.name is not None else str(pefile.RESOURCE_TYPE.get(te.id, te.id))
        if tn != type_name:
            continue
        for ie in te.directory.entries:
            rid = str(ie.name) if ie.name else str(ie.id)
            for le in ie.directory.entries:
                raw = pe.get_data(le.data.struct.OffsetToData, le.data.struct.Size)
                yield int(rid) if rid.isdigit() else rid, raw


def bmp_dimensions(raw):
    """Pull width/height out of a raw RT_BITMAP DIB header."""
    if len(raw) < 18:
        return None
    header_size = struct.unpack_from("<I", raw, 0)[0]
    if header_size < 16 or len(raw) < header_size + 8:
        return None
    w = struct.unpack_from("<i", raw, 4)[0]
    h = struct.unpack_from("<i", raw, 8)[0]
    # Negative height means top-down DIB.
    return (w, abs(h))


def scan_plugin(filename):
    path = os.path.join(PLUGINS, filename)
    print(f"\n{'=' * 78}")
    print(f"=== {filename} ({os.path.getsize(path)} bytes)")
    print(f"{'=' * 78}")
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories()

    # Show all RT_BITMAP dimensions for cross-reference.
    print("\nRT_BITMAP dimensions:")
    for rid, raw in get_resources(pe, "RT_BITMAP", None):
        dim = bmp_dimensions(raw) or "?"
        print(f"  id={rid:>5}  {len(raw):>7}b  dim={dim}")

    # Dump OBJMAP 100, 1000, 1100.
    for target_id in (100, 1000, 1100):
        print(f"\nOBJMAP id={target_id}:")
        for rid, raw in get_resources(pe, "OBJMAP", None):
            if rid != target_id:
                continue
            print(f"  ({len(raw)} bytes)")
            # First 8 bytes as little-endian uint32s - often reveals a
            # magic/version followed by a total count.
            if len(raw) >= 8:
                u32a, u32b = struct.unpack_from("<II", raw, 0)
                print(f"  leading u32s: {u32a} (0x{u32a:08x}), {u32b} (0x{u32b:08x})")
            print(hex_dump(raw, max_lines=12))
            break
        else:
            print("  (not present)")


def main():
    if not os.path.isdir(PLUGINS):
        sys.exit(f"Plugin dir not found: {PLUGINS}")
    for fn, label, why in CALIBRATION:
        print(f"\n# {fn} [{label}] - {why}")
        try:
            scan_plugin(fn)
        except Exception as e:
            print(f"  ERROR: {e}")


if __name__ == "__main__":
    main()
