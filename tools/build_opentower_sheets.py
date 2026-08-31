#!/usr/bin/env python3
"""Build OpenTower folder-plugin sprite sheets + OBJMAPs from the new art.

Takes the loose frames under data/SpriteSheets/originals/<Category>/ and
composes the combined RT_BITMAP 1000 sheets that the engine expects
(downscaled to the legacy cell grid), plus OBJMAP-grammar .txt files and
plugin.json manifests, under data/Plugins/OpenTower/<Plugin>/.

Engine cell grids (from SimTowerLoader + per-item updateSprite math):
    Office : 72 x 24 cells, sheet 144 x 168  (2 cols x 7 rows)
    Condo  : 128 x 24 cells, sheet 640 x 72  (5 cols x 3 rows)
    Hotel-Single : 32 x 24, sheet 288 x 48  (9 cols x 2 rows)
    Hotel-Double : 48 x 24, sheet 432 x 96  (9 cols x 4 rows)
    Hotel-Suite  : 56 x 24, sheet 504 x 48  (9 cols x 2 rows)

Run from the repo root:  python3 tools/build_opentower_sheets.py
Idempotent: re-running overwrites the output plugins in place.
"""
import json
import os
import sys

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "data", "SpriteSheets", "originals")
DST = os.path.join(REPO, "data", "Plugins", "OpenTower")
LEGACY_CONDO_SHEET = os.path.join(REPO, "data", "bitmaps.hidden", "condo.png")


def downscale(img, target_w, target_h):
    """Box-filter downscale preserving alpha (prevents dark fringes)."""
    if img.mode != "RGBA":
        img = img.convert("RGBA")
    if img.width <= target_w or img.height <= target_h:
        return img.resize((target_w, target_h), Image.LANCZOS)
    # PIL's box-average thumbnail gives clean downscaling with alpha.
    img = img.resize((target_w, target_h), Image.LANCZOS)
    return img


def sheet_blank(cell_w, cell_h, cols, rows):
    return Image.new("RGBA", (cell_w * cols, cell_h * rows), (0, 0, 0, 0))


def paste_cell(sheet, img, cell_w, cell_h, col, row):
    cell = downscale(img, cell_w, cell_h)
    sheet.paste(cell, (col * cell_w, row * cell_h))


def write_objmap(path, cell_w, cell_h, cols, rows):
    """Emit an OBJMAP describing one full-sheet Csr block + per-cell View
    blocks. The engine doesn't read this for the legacy items today, but
    it makes the folder a faithful 'decompiled plugin' and is required by
    the OBJMAP-roundtrip test shape."""
    lines = []
    lines.append("DefParts\tName\tCsr")
    lines.append(f"\t\tID\t100")
    lines.append(f"\t\tList\tRect 0,0,{cols*cell_w},{rows*cell_h} Halftone")
    lines.append("")
    pattern = 100
    for row in range(rows):
        for col in range(cols):
            x1, y1 = col * cell_w, row * cell_h
            x2, y2 = x1 + cell_w, y1 + cell_h
            lines.append(f"DefParts\tName\tView")
            lines.append(f"\t\tID\t1000")
            lines.append(f"\t\tPattern\t{pattern}")
            lines.append(f"\t\tList\tRect {x1},{y1},{x2},{y2} Transparent")
            lines.append("")
            pattern += 1
    lines.append("End")
    with open(path, "w", newline="\r\n") as f:
        f.write("\r\n".join(lines) + "\r\n")


def write_manifest(plugin_dir, class_name, item_type, display_name, game_level,
                   sheet_w, sheet_h, cell_w, cell_h, cols, rows):
    manifest = {
        "format": "openskyscraper-plugin",
        "formatVersion": 1,
        "sourceFormat": "scratch",
        "class": class_name,
        "attr": {
            "itemType": item_type,
            "name": display_name,
            "gameLevel": game_level,
            "id": 1,
            "marker": "****",
            "attrib": 1,
            "mustItem": 0,
            "dependID": 0,
            "secretItem": 0,
        },
        "bitmaps": {
            "ingame": {
                "present": True,
                "width": cols * cell_w,
                "height": rows * cell_h,
                "transparentColor": None,
                "maskDiscovery": "none",
                "downscale": {"toW": cols * cell_w, "toH": rows * cell_h},
            },
        },
        "spriteLayout": {"ingame": "meta/objmap_1000.txt"},
    }
    with open(os.path.join(plugin_dir, "plugin.json"), "w") as f:
        json.dump(manifest, f, indent=2)


def load_png(category, name):
    p = os.path.join(SRC, category, name)
    return Image.open(p) if os.path.exists(p) else None


# ---- per-plugin builders ----

def build_office():
    """Office: 2 cols (lit/unlit) x 7 rows (variants 0-5 occupied, 6 empty).
    Source frames office_t{tier}_{state}_{frame}.png at 160x48/144x48."""
    cell_w, cell_h, cols, rows = 72, 24, 2, 7
    sheet = sheet_blank(cell_w, cell_h, cols, rows)
    cat = "Office"
    # Row 0-5: occupied variants (use t0_1_* occupied frames, cycling).
    occupied_frames = sorted(
        f for f in os.listdir(os.path.join(SRC, cat))
        if f.startswith("office_t0_1_") and f.endswith(".png")
    )
    # Row 6: empty/unlit (t0_0_0 base room).
    for row in range(6):
        fname = occupied_frames[row % len(occupied_frames)]
        img = load_png(cat, fname)
        if img:
            paste_cell(sheet, img, cell_w, cell_h, col=0, row=row)  # lit
            paste_cell(sheet, img, cell_w, cell_h, col=1, row=row)  # unlit (same art)
    base = load_png(cat, "office_t0_0_0.png")
    if base:
        paste_cell(sheet, base, cell_w, cell_h, col=0, row=6)
        paste_cell(sheet, base, cell_w, cell_h, col=1, row=6)
    _emit("Office", sheet, cell_w, cell_h, cols, rows, "Tenant", "CFFO", "Office", 1)


def build_condo():
    """Condo: 5 cols x 3 rows of 128x24.

    Condo::updateSprite expects occupied day/lit/night in columns 0-2 and
    vacant day/night in columns 3-4.  The new loose vacant art does not
    include its sale sign, so retain just that overlay from the legacy sheet.
    """
    cell_w, cell_h, cols, rows = 128, 24, 5, 3
    sheet = sheet_blank(cell_w, cell_h, cols, rows)
    cat = "Condo"
    legacy = Image.open(LEGACY_CONDO_SHEET).convert("RGBA")

    def add_sale_sign(cell, variant, legacy_col):
        old = legacy.crop((legacy_col * cell_w, variant * cell_h,
                           (legacy_col + 1) * cell_w, (variant + 1) * cell_h))
        # The signs are diagonal parallelograms. Their horizontal placement
        # differs slightly for each room variant.
        left = (42, 38, 39)[variant]
        mask = Image.new("L", (cell_w, cell_h), 0)
        ImageDraw.Draw(mask).polygon(
            [(left + 6, 14), (left + 42, 3), (left + 50, 5),
             (left + 50, 11), (left + 14, 22), (left + 6, 19)],
            fill=255)
        cell.paste(old, (0, 0), mask)

    # 3 room variants, each with the five states expected by Condo.cpp.
    for variant in range(3):
        empty_day = load_png(cat, f"condo-empty{variant+1}-day-0.png")
        empty_night = load_png(cat, f"condo-empty{variant+1}-night-0.png")
        occupied = sorted(f for f in os.listdir(os.path.join(SRC, cat))
                          if f.startswith(f"condo-occupied{variant+1}") and f.endswith(".png"))
        for col in range(3):
            if occupied:
                paste_cell(sheet, load_png(cat, occupied[col % len(occupied)]),
                           cell_w, cell_h, col=col, row=variant)
        for col, img, legacy_col in ((3, empty_day, 3), (4, empty_night, 4)):
            if img:
                cell = downscale(img, cell_w, cell_h)
                add_sale_sign(cell, variant, legacy_col)
                sheet.paste(cell, (col * cell_w, variant * cell_h))
    _emit("Condo", sheet, cell_w, cell_h, cols, rows, "Tenant", "TRPA", "Condo", 1)


def build_hotel(variant_key, item_type, display_name, cell_w, cols, rows, src_prefix):
    """Hotel rooms. cell_h always 24. src_prefix selects the MM_Hotel-* art."""
    cell_h = 24
    sheet = sheet_blank(cell_w, cell_h, cols, rows)
    cat = "Hotel"
    days = sorted(f for f in os.listdir(os.path.join(SRC, cat))
                  if f.startswith(f"MM_Hotel-{src_prefix}-day") and f.endswith(".png"))
    nights = sorted(f for f in os.listdir(os.path.join(SRC, cat))
                    if f.startswith(f"MM_Hotel-{src_prefix}-night") and f.endswith(".png"))
    # Row layout: row 0 = clean states, subsequent rows = dirty/occupied variants.
    for col in range(min(cols, len(days))):
        img = load_png(cat, days[col])
        if img:
            paste_cell(sheet, img, cell_w, cell_h, col=col, row=0)
    for col in range(min(cols, len(nights))):
        img = load_png(cat, nights[col])
        if img:
            paste_cell(sheet, img, cell_w, cell_h, col=col, row=min(rows - 1, 1))
    _emit(display_name, sheet, cell_w, cell_h, cols, rows, "Tenant", item_type, display_name, 1)


def build_repeated(plugin, category, sources, cell_w, cell_h, cols, rows,
                   cls, item_type, display):
    """Build a fixed engine-grid sheet by cycling first-party source art."""
    sheet = sheet_blank(cell_w, cell_h, cols, rows)
    images = [load_png(category, name) for name in sources]
    images = [img for img in images if img is not None]
    if not images:
        raise RuntimeError(f"{plugin}: no source images")
    for row in range(rows):
        for col in range(cols):
            paste_cell(sheet, images[row % len(images)], cell_w, cell_h, col, row)
    _emit(plugin, sheet, cell_w, cell_h, cols, rows,
          cls, item_type, display, 1)


def build_first_party_candidates():
    # Simple facilities whose runtime consumes a fixed frame grid.
    build_repeated("Floor", "Facility", ["floor.png"],
                   8, 36, 1, 1, "Tenant", "OLFK", "Floor")
    build_repeated("Security", "Facility", ["security_u.png"],
                   128, 24, 3, 1, "Tenant", "IBEK", "Security")
    build_repeated("MedicalCenter", "Facility", ["MedicalClinic_u.png"],
                   256, 24, 1, 1, "Tenant", "PSOH", "Medical Center")
    build_repeated("Recycling", "Facility", ["MM_Recycle-empty.png"],
                   200, 60, 1, 1, "Tenant", "SUDK", "Recycling Center")
    party = sheet_blank(192, 60, 2, 1)
    paste_cell(party, load_png("Facility", "partyhall-0-day-0.png"),
               192, 60, 0, 0)
    paste_cell(party, load_png("Facility", "MM_PartyHall-night.png"),
               192, 60, 1, 0)
    _emit("PartyHall", party, 192, 60, 2, 1,
          "Tenant", "IOKI", "Party Hall", 1)
    build_repeated("Housekeeping", "Facility",
                   ["MM_PrivateHousekeeping-day.png"],
                   120, 24, 1, 1, "Tenant", "NEMR", "Housekeeping")

    fastfood = sorted(f for f in os.listdir(os.path.join(SRC, "Commercial"))
                      if f.startswith("MM_FastFood-") and f.endswith(".png"))[:5]
    build_repeated("FastFood", "Commercial", fastfood,
                   128, 24, 4, 5, "Tenant", "GRUB", "Fast Food")
    restaurants = sorted(f for f in os.listdir(os.path.join(SRC, "Commercial"))
                         if f.startswith("MM_Restaurant-") and f.endswith(".png"))[:4]
    build_repeated("Restaurant", "Commercial", restaurants,
                   192, 24, 4, 4, "Tenant", "ANIC", "Restaurant")

    # Parking is a horizontally repeating 8-pixel tile at runtime.
    parking_source = load_png("Transport", "OT-parking.png")
    parking_tile = downscale(parking_source, 64, 24)
    parking = Image.new("RGBA", (480, 24), (0, 0, 0, 0))
    for x in range(0, parking.width, parking_tile.width):
        parking.paste(parking_tile.crop((0, 0, min(64, parking.width - x), 24)), (x, 0))
    _emit("Parking", parking, 480, 24, 1, 1,
          "Tenant", "KRAP", "Parking", 1)
    build_repeated("Parking-Ramp", "Transport", ["opentowerparkingramp2.png"],
                   384, 24, 1, 1, "Tenant", "KRAP", "Parking Ramp")

    shop_sources = ["MM_shop-closed.png", "MM_vendingmachine.png",
                    "MM_SodaMachines.png"]
    build_repeated("Shops", "Commercial", shop_sources,
                   96, 48, 3, 6, "Tenant", "BNOC", "Shops")

    # Standard stairs and escalators are horizontal animation strips.
    build_repeated("Stairs", "Transport", ["MM_stairs_1story.png"],
                   64, 60, 14, 1, "Mover", "RATS", "Stairs")
    build_repeated("Escalator", "Transport", ["MM_Escalator.png"],
                   64, 72, 8, 1, "Mover", "ACSE", "Escalator")

    # Elevator cars use five occupancy frames. Shaft sheets use seven
    # sections (base plus three paired motor animation states).
    build_repeated("Elevator-Standard", "Transport", ["MM_Elevator-local.png"],
                   28, 30, 5, 1, "Mover", "EdtS", "Standard Elevator")
    build_repeated("Elevator-Service", "Transport", ["MM_Elevator-service.png"],
                   28, 30, 5, 1, "Mover", "EreS", "Service Elevator")
    build_repeated("Elevator-Express", "Transport", ["MM_Elevator-express.png"],
                   44, 30, 5, 1, "Mover", "EgiB", "Express Elevator")
    build_repeated("Elevator-Shaft-Narrow", "Transport", ["elevatorshafts.png"],
                   32, 36, 7, 1, "Mover", "EdtS", "Narrow Elevator Shaft")
    build_repeated("Elevator-Shaft-Wide", "Transport", ["elevatorshafts.png"],
                   48, 36, 7, 1, "Mover", "EgiB", "Wide Elevator Shaft")

    # Lobby rendering expects a 56-pixel overlay plus a 256-pixel repeating
    # segment, with one, three, or nine 36-pixel vertical states.
    build_repeated("Lobby-Normal", "Lobbies", ["MM_GrandLobby-1star.png"],
                   312, 108, 1, 1, "Tenant", "NORF", "Lobby")
    build_repeated("Lobby-Sky", "Lobbies", ["MM_SkyLobby-1star.png"],
                   312, 108, 1, 1, "Tenant", "NORF", "Sky Lobby")
    build_repeated("Lobby-High", "Lobbies", ["MM_lobbyandMezz-3star.png"],
                   312, 324, 1, 1, "Tenant", "NORF", "Grand Lobby")


def _emit(plugin_name, sheet, cell_w, cell_h, cols, rows, cls, item_type, display, level):
    plugin_dir = os.path.join(DST, plugin_name)
    os.makedirs(os.path.join(plugin_dir, "sprites"), exist_ok=True)
    os.makedirs(os.path.join(plugin_dir, "meta"), exist_ok=True)
    sheet.save(os.path.join(plugin_dir, "sprites", "1000.png"))
    write_objmap(os.path.join(plugin_dir, "meta", "objmap_1000.txt"),
                 cell_w, cell_h, cols, rows)
    write_manifest(plugin_dir, cls, item_type, display, level,
                   cols * cell_w, rows * cell_h, cell_w, cell_h, cols, rows)
    print(f"built {plugin_name}: {cols*cell_w}x{rows*cell_h} ({cols}x{rows} cells of {cell_w}x{cell_h})")


def main():
    os.makedirs(DST, exist_ok=True)
    build_office()
    build_condo()
    # Hotel variants: single 32px/9col/2row, double 48px/9col/4row, suite 56px/9col/2row.
    build_hotel("single", "LGSH", "Hotel-Single", 32, 9, 2, "single")
    build_hotel("double", "NWTH", "Hotel-Double", 48, 9, 4, "double")
    build_hotel("suite", "IUSH", "Hotel-Suite", 56, 9, 2, "Suite")
    build_first_party_candidates()
    print("done.")


if __name__ == "__main__":
    sys.exit(main())
