# OpenSkyscraper Plugin Format (OSP) — v2

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
    100.png              RT_BITMAP 100  — toolbox icon sheet (~26x130)
    1000.png             RT_BITMAP 1000 — primary sprite sheet
    1100.png             RT_BITMAP 1100 — people animation (Tenant, optional)
    200.png              RT_BITMAP 200  — mover walking-frame SHIL (Mover, optional)
  meta/
    objmap_100.txt       toolbox-icon rects (OBJMAP grammar; see objmap-format.md)
    objmap_1000.txt      primary sprite rects
    objmap_200.txt       mover walking-frame rects (Mover plugins only)
```

Sheet files are named after their RT_BITMAP id (`100`/`1000`/`1100`/`200`).
The loader also accepts the long names (`sprites/toolbox.png`,
`sprites/ingame.png`, `sprites/occupants.png`, `sprites/walking.png`) and
honors a per-sheet `relPath` in the manifest when present, but the numeric
names are what the editor writes and what the shipped OpenTower plugins use.

Missing sprite sheets are tolerated: a plugin without occupants simply omits
`sprites/1100.png` and its `bitmaps.occupants.present` is `false`.

## `plugin.json` schema

```json
{
  "format": "openskyscraper-plugin",
  "formatVersion": 2,
  "sourceFormat": "yoottower-t2p",
  "sourceFile": "Burger.t2p",
  "class": "Tenant",
  "attr": {
    "pluginId": "BURG",
    "itemType": 1301,
    "name": "Burger land",
    "gameLevel": 1,
    "attrib": 1,
    "dependPluginId": "****",
    "secretItem": 0,
    "mustItem": 0
  },
  "dayParam":   { "percent": [[70, 50], [100, 80]] },
  "placeParam": { "places": [[40, 1000], [10, 300], [50, 100]] },
  "transport":  { "priceString": "$150000:$50000/car", "outMoneyString": "$12000/Q", "name": "Simple Elevator" },
  "bitmaps": {
    "toolbox":   { "present": true, "width": 26,  "height": 130, "transparentColor": null,         "maskDiscovery": "none", "relPath": "sprites/100.png" },
    "ingame":    { "present": true, "width": 128, "height": 360, "transparentColor": [105,33,104], "maskDiscovery": "manual", "relPath": "sprites/1000.png" },
    "occupants": { "present": true, "width": 16,  "height": 984, "transparentColor": [255,255,255], "maskDiscovery": "manual", "relPath": "sprites/1100.png" },
    "walking":   { "present": false, "width": 0,   "height": 0,   "transparentColor": null,         "maskDiscovery": "none" }
  },
  "spriteLayout": { "toolbox": "meta/objmap_100.txt", "ingame": "meta/objmap_1000.txt", "walking": "meta/objmap_200.txt" }
}
```

### Fields

| Field | Required | Notes |
|---|---|---|
| `format` | yes | Must be `"openskyscraper-plugin"`. |
| `formatVersion` | yes | `2` is written; `1` and `2` are read (v1 key mapping below). |
| `sourceFormat` | yes | `"yoottower-t2p"` (converted), `"openskyscraper-plugin"` (re-saved), or `"scratch"` (new). |
| `sourceFile` | no | The original `.t2p` filename, when converted. Absent for scratch plugins. |
| `class` | yes | One of `Tenant`, `Mover`, `ToolPalette`, `World`, `OuterObj`. |
| `attr` | yes | ATTR id 128 (`PluginAttr` in `source/PluginResources.h`, doc/yoottower/plugin-schema.md §2.1). `pluginId` = mID, the plugin's unique 4cc in Mac order (`"BURG"`; the file bytes are `GRUB`). `itemType` = mItemType, the numeric item type (== the TNDF/MVDF mToolType). `attrib` = world-availability mask (Tokyo 0x1/0x2/0x4, Kegon 0x10, Hawai 0x100). `dependPluginId` = mDependID in Mac order (`"****"` none, `"FRON"` = needs a hotel Front). |
| `dayParam` | no | DPDF id 128 (T2DayParamDef): `percent[EDay][EWhether]`, customer rate by weekday/holiday × fine/rain. |
| `placeParam` | no | PLDF id 128 (T2PlaceParamDef): `[percent, score]` for EPlace 0 floor, 1 tenant, 2 pool. `score` is the full 4-byte slot (the engine reads its low 16 bits). |
| `transport` | no | Mover plugins: the first MVDF variation's `priceString` (e.g. `"$150000:$50000/car"`), `outMoneyString` (e.g. `"$12000/Q"`) and `name`. The full MVDF is decoded by `TemplateDef` (`source/PluginDefs.h`) but is not yet part of the manifest. |
| `bitmaps` | yes | Per-sheet dimensions + transparency. `transparentColor` is `null` or `[r,g,b]`. `relPath` (optional) points at the sheet PNG relative to the plugin dir; the loader falls back to the numeric then canonical names when absent. `walking` is present only on Mover plugins. |
| `spriteLayout` | yes | Relative paths to the OBJMAP text files under `meta/`. `walking` (`objmap_200.txt`) is present only on Mover plugins. |

### Reading v1 manifests

v1 used names that turned out to be wrong (doc/yoottower/plugin-schema.md §0).
`readOspAttr` / `loadOspPlugin` map them onto the corrected fields, and the
OpenTower plugins under `data/Plugins/OpenTower/` still ship v1 manifests:

| v1 key | Meaning | Read as |
|---|---|---|
| `attr.itemType` (string) | ATTR offset 1, file-order bytes (`"GRUB"`) | `pluginId` (`"BURG"`) |
| `attr.id` (number) | ATTR offset 21 | numeric `itemType` |
| `attr.marker` (string) | ATTR offset 9, file-order bytes (`"NORF"`) | `dependPluginId` (`"FRON"`) |
| `attr.dependID` (number) | never populated from binary plugins | `dependPluginId` only when non-zero and no `marker` |
| `placement` `{w, h, baseX, baseY}` | DPDF slots 0..3 | `dayParam.percent` `[[w, h], [baseX, baseY]]` |
| `price` `{f1, basePrice, f2, f3, f4, f5}` | PLDF slots 0..5 | `placeParam.places` `[[f1, basePrice], [f2, f3], [f4, f5]]` |
| `transport.quarterlyString` | MVDF mOutMoneyString | `transport.outMoneyString` |

Saving always writes v2.

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

1. Pulls `ATTR`/`DESC`, `DPDF` 128 and `PLDF` 128 through the typed
   accessors (`getAttr`, `getDesc`, `getDayParamDef`, `getPlaceParamDef`), and
   for movers the first `MVDF` variation through `getTemplateDef`.
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
- Full read/write of the `attr`, `dayParam` (DPDF), `placeParam` (PLDF),
  `transport` (MVDF strings), and `bitmaps` sections.
- **Tenant class** end-to-end: Condo/Burger/etc. convert, edit, save, reload.
- **Mover class**: stairs/escalators/elevators convert (DESC class preserved,
  MVDF price strings extracted), edit (price strings in the inspector),
  save, reload. The walking-frames sheet (RT_BITMAP 200 + OBJMAP 200) round-
  trips and is selectable in the preview canvas.
- OBJMAP parse → write → parse identity (toolbox + ingame + walking layouts).
- Transparent-color discovery (manual + corner) and the editor eyedropper.

**Not yet (future iterations):**
- **Other classes** (`World`, `OuterObj`, `ToolPalette`): the format records
  their `class`, and convert still pulls their metadata, but the editor UI is
  Tenant/Mover-focused.
- **The full template definitions** (TNDF/MVDF/OODF, TMDF, PADF, SLDF, WODF):
  every field is decoded and re-encoded byte-exactly by `source/PluginDefs.h`
  and exposed through `AbstractPlugin` (`getTemplateDef`, `getMemberTables`,
  ...), but the manifest does not carry them yet.
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
