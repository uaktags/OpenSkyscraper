# OpenSkyscraper Plugin Format (OSP) — v1

**Status:** Implemented (Tenant slice). Spec is the source of truth for the
`PluginEditor` and the `source/OspPlugin.{h,cpp}` reader/writer.

The OSP format is OpenSkyscraper's native, editable plugin format. It is the
**only** format the editor saves to. YootTower `.t2p` files are read-only and
must be *converted* to OSP (via the editor's Convert / Save As) before they
can be re-saved.

## Design goals

- **Human-readable & diff-friendly.** A loose directory of small files, not an
  opaque binary blob. Sprites are PNG (not the source DIB) so they preview in
  any tool and survive version-control diffs.
- **Round-trippable.** A t2p → OSP convert, then an OSP load, must reproduce
  the same metadata, sprite dimensions, and OBJMAP layout (covered by
  `testOspRoundTrip` in `tests/test_main.cpp`).
- **Forward-compatible.** A `formatVersion` field gates loading; the editor
  refuses a version newer than it understands rather than silently corrupting.

## Directory layout

```
Name.osp-plugin/
  plugin.json            manifest (schema below)
  sprites/
    toolbox.png          RT_BITMAP 100  — toolbox icon sheet (~26x130)
    ingame.png           RT_BITMAP 1000 — primary sprite sheet
    occupants.png        RT_BITMAP 1100 — people animation (Tenant, optional)
    walking.png          RT_BITMAP 200  — mover walking-frame SHIL (Mover, optional)
  meta/
    objmap_100.txt       toolbox-icon rects (OBJMAP grammar; see objmap-format.md)
    objmap_1000.txt      primary sprite rects
    objmap_200.txt       mover walking-frame rects (Mover plugins only)
```

Missing sprite sheets are tolerated: a plugin without occupants simply omits
`sprites/occupants.png` and its `bitmaps.occupants.present` is `false`.

## `plugin.json` schema

```json
{
  "format": "openskyscraper-plugin",
  "formatVersion": 1,
  "sourceFormat": "yoottower-t2p",
  "sourceFile": "Condo.t2p",
  "class": "Tenant",
  "attr": {
    "itemType": "NERH",
    "name": "Apartment",
    "gameLevel": 2,
    "id": 1684,
    "marker": "****",
    "mustItem": 0,
    "dependID": 0,
    "secretItem": 0
  },
  "placement": { "w": 70, "h": 50, "baseX": 100, "baseY": 80 },
  "price":     { "basePrice": 50000, "f1": 40, "f2": 10, "f3": 300, "f4": 50, "f5": 100 },
  "transport": { "priceString": "$150000:$50000/car", "quarterlyString": "$12000/Q", "name": "Simple Elevator" },
  "bitmaps": {
    "toolbox":   { "present": true, "width": 26,  "height": 130, "transparentColor": null,         "maskDiscovery": "none" },
    "ingame":    { "present": true, "width": 128, "height": 360, "transparentColor": [105,33,104], "maskDiscovery": "manual" },
    "occupants": { "present": true, "width": 16,  "height": 984, "transparentColor": [255,255,255], "maskDiscovery": "manual" },
    "walking":   { "present": false, "width": 0,   "height": 0,   "transparentColor": null,         "maskDiscovery": "none" }
  },
  "spriteLayout": { "toolbox": "meta/objmap_100.txt", "ingame": "meta/objmap_1000.txt", "walking": "meta/objmap_200.txt" }
}
```

### Fields

| Field | Required | Notes |
|---|---|---|
| `format` | yes | Must be `"openskyscraper-plugin"`. |
| `formatVersion` | yes | Must be `1` for this version of the editor. |
| `sourceFormat` | yes | `"yoottower-t2p"` (converted), `"openskyscraper-plugin"` (re-saved), or `"scratch"` (new). |
| `sourceFile` | no | The original `.t2p` filename, when converted. Absent for scratch plugins. |
| `class` | yes | One of `Tenant`, `Mover`, `ToolPalette`, `World`, `OuterObj`. |
| `attr` | yes | Item identity; see `PluginAttr` in `source/PluginResources.h`. `itemType`/`marker` are 4-char codes. |
| `placement` | no | DPDF values; present on placement-driven items, absent on transport. |
| `price` | no | PLDF values (Tenant shops); `basePrice` is the construction cost. Present on shops, absent on residences/transport. |
| `transport` | no | MVDF values (Mover plugins): human-readable `priceString` (e.g. `"$150000:$50000/car"`), `quarterlyString` (e.g. `"$12000/Q"`), and `name`. Only the price strings round-trip today; the binary header dwords (capacity/speed) are inferred pending the T2DLL Ghidra pass. |
| `bitmaps` | yes | Per-sheet dimensions + transparency. `transparentColor` is `null` or `[r,g,b]`. `walking` is present only on Mover plugins. |
| `spriteLayout` | yes | Relative paths to the OBJMAP text files under `meta/`. `walking` (`objmap_200.txt`) is present only on Mover plugins. |

### `maskDiscovery`

Records how the transparent color was chosen, so a reload can re-evaluate:

| Value | Meaning |
|---|---|
| `"none"` | No mask (opaque sheet). |
| `"manual"` | Hand-picked via the editor's eyedropper, or known from the source plugin (e.g. Condo's magenta `105,33,104`). |
| `"corner"` | Auto-discovered by sampling the sheet's top-left pixel. A heuristic; flagged so it can be reviewed. |

## Converting from `.t2p`

The `convertT2pToOsp` function (`source/OspPlugin.cpp`) is the read-only bridge
from the YootTower format:

1. Pulls `ATTR`/`DESC`/`DPDF`/`PLDF`/`MVDF` via the existing typed accessors.
2. Rasterizes each `RT_BITMAP` (100/1000/1100/200) into an `sf::Image` via
   `PluginBitmapLoader::decodeBitmapResource`. The `200` (walking) sheet is
   pulled for Mover plugins; it's absent on tenants.
3. Resolves the transparent color: a small known-per-plugin table first
   (Condo ingame=magenta, Condo occupants=white), then corner discovery.
4. Parses the matching `OBJMAP` (100/1000, plus 200 for movers) into
   `ObjmapBlock` lists.

The result is an `OspPlugin` with `sourceFormat: "yoottower-t2p"`, ready to
edit and Save As. Saving to `.t2p` is intentionally unsupported.

## Scope of this version (Tenant + Mover slices)

**Implemented:**
- Full read/write of the `attr`, `placement`, `price` (PLDF), `transport`
  (MVDF), and `bitmaps` sections.
- **Tenant class** end-to-end: Condo/Burger/etc. convert, edit, save, reload.
- **Mover class**: stairs/escalators/elevators convert (DESC class preserved,
  MVDF transport pricing extracted), edit (price strings in the inspector),
  save, reload. The walking-frames sheet (RT_BITMAP 200 + OBJMAP 200) round-
  trips and is selectable in the preview canvas.
- OBJMAP parse → write → parse identity (toolbox + ingame + walking layouts).
- Transparent-color discovery (manual + corner) and the editor eyedropper.

**Not yet (future iterations):**
- **Other classes** (`World`, `OuterObj`, `ToolPalette`): the format records
  their `class`, and convert still pulls their metadata, but the editor UI is
  Tenant/Mover-focused.
- **MVDF binary header fields**: only the human-readable price strings round-
  trip today; the capacity/speed/max-floors dwords are captured generically
  but not editor-exposed, pending the T2DLL Ghidra pass to confirm semantics.
- **Free-form rect drawing** on the canvas: rects arrive from convert and are
  shown/overlaid; numeric editing of individual rects is a follow-up.
- **Animation playback** in the canvas (frame scrubbing): static sheets only.

## Game integration — folder plugins (OpenTower edition)

Folder plugins are loaded directly by the game, not just the editor. The
`DecompiledPlugin` class (`source/DecompiledPlugin.{h,cpp}`) implements the
same `AbstractPlugin` surface as the PE `Plugin`, reading `plugin.json` +
`sprites/*.png` + `meta/objmap_*.txt` from a directory. `PluginManager::
addPluginDirectory` discovers both `*.t2p` PE files and folder plugins (a
subdirectory containing `plugin.json`), cataloging the latter under their
relative path (e.g. `OpenTower/Office`).

This is the foundation of the **edition** concept — OpenTower (folder art),
SIMTOWER (the NE executable), and YootTower (`.t2p` plugins) can coexist.
`BitmapManager::editionTexture(logicalName, simtowerKey)` is the resolution
seam: legacy item classes keep their `App->bitmaps["simtower/office"]`
calls, but the manager prefers an OpenTower folder plugin when one is
cataloged, falling back to the simtower path otherwise.

### The `downscale` block

OpenTower art is authored at ~3x the engine's cell resolution (e.g. Office
rooms are 160×72 vs the engine's 72×24). A `downscale` block in the
manifest declares the target sheet size so `PluginBitmapLoader` resamples
the PNG at load time without any per-item frame-math change:

```json
"bitmaps": {
  "ingame": { "present": true, "width": 144, "height": 168,
              "downscale": { "toW": 144, "toH": 168 } }
}
```

The resample is an alpha-weighted box filter
(`PluginBitmapLoader::downscaleImage`) so colour-keyed sprites don't
develop dark fringes. PE plugins have no `downscale` block and load at
native resolution.

### Building OpenTower art

`tools/build_opentower_sheets.py` composes the combined sheets from the
loose frames under `data/SpriteSheets/originals/<Category>/`, emits the
OBJMAP `.txt` files, and writes `plugin.json` for each of Office, Condo,
and Hotel-Single/Double/Suite. Re-run after editing the source art; it is
idempotent and overwrites in place.

## Versioning

When the schema gains a field that an older editor cannot interpret, bump
`formatVersion` and gate `loadOspPlugin` on the supported version. Old files
remain loadable; new files are rejected by old editors with a clear message.
