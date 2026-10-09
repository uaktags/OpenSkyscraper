# OpenSkyscraper Plugin Format (OSP) — v3

**Status:** Implemented for every YootTower plugin class (Tenant, Mover,
OuterObj, ToolPalette, World, Movie, HallEvent, Advertise). This spec is the
source of truth for the `PluginEditor` and `source/OspPlugin.{h,cpp}`.

The OSP format is OpenSkyscraper's native, editable plugin format. The
editor saves to it, and since v3 it carries every resource of a converted
YootTower `.t2p` losslessly, so an OSP plugin can be exported back to a real
`.t2p` (`source/PluginT2pExport.h`) that the original game loads.

## Design goals

- **Human-readable & diff-friendly.** A loose directory of small files, not an
  opaque binary blob. Sprites are PNG (not the source DIB) so they preview in
  any tool and survive version-control diffs.
- **Lossless.** t2p → OSP v3 → re-encoded resources are byte-identical to
  the original for all 156 shipped plugins (3332 resources), and the exported
  `.t2p` is byte-identical to the input file (`testOspV3Sweep`).
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
  resources/             v3: one file per typed or text resource
    TNDF/1.json          typed Json (PluginResourceJson.h field names)
    OBJMAP/1000.txt      verbatim text (OBJMAP, T2DLG, TENANTDEF)
    PGDF/128.bin         raw bytes (resources without a decoder)
  bitmaps/               v3: every RT_BITMAP (PE) / DIB (SP)
    1000.png             the pixels (opaque palette colours)
    1000.dib.json        the original DIB header, palette, odd padding bytes
  sounds/                v3: every WAVE resource
    165.wav              the RIFF file, verbatim
  pe/original.t2p        v3, PE plugins: the original DLL (code for export)
```

The `sprites/` and `meta/` files are the v2 view. In a v3 plugin they are
derived from the resources on every save (`syncOspLegacyFromResources`), so
v2 readers and the game's folder-plugin loader (`DecompiledPlugin`) keep
working; the editor edits the resources.

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
  "formatVersion": 3,
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
  "spriteLayout": { "toolbox": "meta/objmap_100.txt", "ingame": "meta/objmap_1000.txt", "walking": "meta/objmap_200.txt" },
  "container": "PE",
  "peImage": "pe/original.t2p",
  "resources": [
    {"type": "ATTR", "id": 128, "codec": "Attr", "file": "resources/ATTR/128.json"},
    {"type": "TNDF", "id": 1, "codec": "TemplateDef", "file": "resources/TNDF/1.json"},
    {"type": "OBJMAP", "id": 1000, "codec": "Text", "file": "resources/OBJMAP/1000.txt"},
    {"type": "2", "id": 1000, "codec": "Raw", "file": "bitmaps/1000.png", "dib": "bitmaps/1000.dib.json"},
    {"type": "WAVE", "id": 165, "codec": "Raw", "file": "sounds/165.wav"}
  ]
}
```

### Fields

| Field | Required | Notes |
|---|---|---|
| `format` | yes | Must be `"openskyscraper-plugin"`. |
| `formatVersion` | yes | `3` is written; `1`, `2` and `3` are read (v1 key mapping below). |
| `sourceFormat` | yes | `"yoottower-t2p"` (converted), `"openskyscraper-plugin"` (re-saved), or `"scratch"` (new). |
| `sourceFile` | no | The original `.t2p` filename, when converted. Absent for scratch plugins. |
| `class` | yes | One of `Tenant`, `Mover`, `ToolPalette`, `World`, `OuterObj`, `Movie`, `HallEvent`, `Advertise`. |
| `attr` | yes | ATTR id 128 (`PluginAttr` in `source/PluginResources.h`, doc/yoottower/plugin-schema.md §2.1). `pluginId` = mID, the plugin's unique 4cc in Mac order (`"BURG"`; the file bytes are `GRUB`). `itemType` = mItemType, the numeric item type (== the TNDF/MVDF mToolType). `attrib` = world-availability mask (Tokyo 0x1/0x2/0x4, Kegon 0x10, Hawai 0x100). `dependPluginId` = mDependID in Mac order (`"****"` none, `"FRON"` = needs a hotel Front). |
| `dayParam` | no | DPDF id 128 (T2DayParamDef): `percent[EDay][EWhether]`, customer rate by weekday/holiday × fine/rain. |
| `placeParam` | no | PLDF id 128 (T2PlaceParamDef): `[percent, score]` for EPlace 0 floor, 1 tenant, 2 pool. `score` is the full 4-byte slot (the engine reads its low 16 bits). |
| `transport` | no | Mover plugins: the first MVDF variation's `priceString` (e.g. `"$150000:$50000/car"`), `outMoneyString` (e.g. `"$12000/Q"`) and `name`. In v3 the full MVDF is in `resources/MVDF/1.json`. |
| `bitmaps` | yes | Per-sheet dimensions + transparency. `transparentColor` is `null` or `[r,g,b]`. `relPath` (optional) points at the sheet PNG relative to the plugin dir; the loader falls back to the numeric then canonical names when absent. `walking` is present only on Mover plugins. |
| `spriteLayout` | yes | Relative paths to the OBJMAP text files under `meta/`. `walking` (`objmap_200.txt`) is present only on Mover plugins. |
| `container` | v3 | `"PE"` (DLL plugins) or `"SP"` (the `SP` sub-plugin container of movies, hall events and ads). |
| `peImage` | v3, PE | Path of the original DLL. Only its code and headers are used by the export; resources always come from `resources`. |
| `resources` | v3 | Every resource in container order: `type` (the resource type key: a name such as `"TNDF"`, or the decimal RT_* number, `"2"` = RT_BITMAP, `"16"` = RT_VERSION; SP files use the CResFile tag name and `"BITMAP"` for DIBs), `id`, `codec` (`resourceCodecName`), `file`, `dib` (bitmaps) and `spType` (SP: the u32 tag constant, 0 = DIB). |

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

Saving always writes v3. A v1/v2 plugin opened in the editor gets typed
ATTR/DESC/DPDF/PLDF resources synthesized from its manifest
(`synthesizeOspResources`); its `sprites/` PNGs are kept as they are.

### `maskDiscovery`

Records how the transparent color was chosen, so a reload can re-evaluate:

| Value | Meaning |
|---|---|
| `"none"` | No mask (opaque sheet). |
| `"manual"` | Hand-picked via the editor's eyedropper, or known from the source plugin (e.g. Condo's magenta `105,33,104`). |
| `"corner"` | Auto-discovered by sampling the sheet's top-left pixel. A heuristic; flagged so it can be reviewed. |

## The v3 resource store

Each resource is stored in the most useful lossless form:

| Codec | Stored as | Notes |
|---|---|---|
| typed (`Attr`, `Desc`, `TemplateDef`, `MemberTable`, `DayParam`, `PlaceParam`, `PeopleAnime`, `Silhouettes`, `People`, `Words`, `StringTable`, `Movie`, `HallEvent`, `Advertise`, `SubPluginPeopleParam`, `StringBlock`, `VersionInfo`, `WorldRecord`, `BkgndInfo`, `FloorInfoMask`, `PaletteAnime`, `Palette`) | `resources/<TYPE>/<id>.json` | `source/PluginResourceJson.h`. Keys are the struct field names (engine names without `m`); `skip*`/`unread*`/`unk*` fields are kept. Plugin strings are code-page bytes mapped through Latin-1 (any byte survives); 4ccs are Mac-order strings (`"BURG"`, `"BoPI"`), `""` for 0. TemplateDef flattens the ToolDef and EquipDef per-variation records into one `variations` list. World records (WORLDDEF, WATR, PODF, CODF, SPDF, T2EV, VIPL, `*EV`) follow `tools/yoottower/world_sp.py` (`source/WorldResourceCodec.h`); stored counts are derived from list lengths. BINF/FINF grids are row-major `grid` arrays. |
| `Text` (OBJMAP, T2DLG, TENANTDEF) | `resources/<TYPE>/<id>.txt` | Verbatim bytes. |
| RT_BITMAP / SP DIB | `bitmaps/<id>.png` + `bitmaps/<id>.dib.json` | 4/8-bit uncompressed DIBs. The PNG holds the palette colours; the sidecar holds the 40-byte header (hex), the palette (`[r, g, b, reserved]`), any non-zero row padding (`rowPadding`, keyed by file row) and trailing bytes (`tailHex`). Every shipped bitmap maps each colour to its first palette slot, so re-encoding is exact. An edited PNG is mapped back onto the palette (exact colour, else nearest); a resized one gets a new header. |
| WAVE | `sounds/<id>.wav` | The RIFF resource verbatim (some have a pad byte after an odd-length chunk). |
| `Raw` (PGDF, PZDF, anything without a decoder) | `resources/<TYPE>/<id>.bin` | Raw bytes. |

A typed file that does not encode (a string over 255 bytes, more than four
variations, ...) is a load error naming the file and field.

## Exporting to `.t2p`

`exportOspToT2p` / `writeT2pFile` (`source/PluginT2pExport.h`) build a real
YootTower plugin from the resource list:

* **PE plugins** keep the code of `pe/original.t2p`; only `.rsrc` changes.
  When the resource set (types and ids) is unchanged, the original section is
  kept byte for byte and changed payloads are patched in place, or appended
  at the end of the section when they outgrow their 8-byte-aligned slot (the
  data entry's RVA and size are updated). The linkers that built the shipped
  plugins left gaps and stale bytes in `.rsrc` (Condo's section has about 32 KB
  of unused space after the last payload, and some alignment padding is not zero), so
  regenerating the section would not be byte-identical; in-place patching
  is. When resources are added or removed the section is rebuilt with the MS
  linker layout: directory tables breadth first, data entries, the type-name
  strings (original order first), then the payloads 8-byte aligned (original
  order first). Either way `.reloc` and the debug data in the overlay are
  moved, and the section table, the resource and base-relocation data
  directories, `SizeOfImage`, `SizeOfInitializedData` and the debug
  directory's file pointers are fixed up. The shipped plugins have a zero
  checksum, which is kept; a non-zero checksum is recomputed.
* **SP plugins** re-encode the `SP` container in directory order
  (`encodeSpContainer`).

**Result:** exporting an unedited plugin is byte-identical to the original
file for all 156 plugins (105 PE + 51 SP). An edited Burger (new name, price
777 = $77,700) loads in the original game under Wine: the toolbox shows the
new name and building it takes exactly $77,700 from the funds.

Only plugins converted from a `.t2p` can be exported (v1/v2 and scratch
plugins have no code image).

## Converting from `.t2p`

`convertT2pToOsp` (`source/OspPlugin.cpp`) reads the plugin file, decodes
every resource with `decodeResource` (PE: in resource-directory order; SP: in
container order, with each entry's tag constant) into
`OspPlugin::resources`, keeps the DLL bytes in `peImage`, and builds the v2
view:

1. `ATTR`/`DESC`, `DPDF` 128, `PLDF` 128 and, for movers, the first `MVDF`
   variation.
2. Each `RT_BITMAP` (100/1000/1100/200) rasterized into an `sf::Image`, with
   the transparent color from a small known-per-plugin table (Condo
   ingame=magenta, Condo occupants=white), else corner discovery, and the
   matching `OBJMAP` parsed into `ObjmapBlock` lists.

## Plugin Editor guide

`PluginEditor` (target `PluginEditor`, `tools/PluginEditor/`) opens a `.t2p`
or an `.osp-plugin` directory. The window has three panes:

* **Resources** (left): the plugin's resources grouped as Identity
  (ATTR/DESC), Definition (TNDF/MVDF/OODF, MODF/HEDF/ADDF), Tables
  (TMDF/DPDF/PLDF/PADF/SLDF/PPDF/WODF/STRTABLE), World (WORLDDEF, WATR,
  T2EV, events, BINF/FINF, PTDF, PALETTE, PODF/CODF/SPDF/VIPL, PGDF/PZDF),
  Bitmaps, Sprite maps (OBJMAP), Sounds and Text and version. Tick
  **Advanced fields** to also show fields the engine ignores or whose
  meaning is unknown (named by key or position: `skip3`, `unk[0..5]`, ...).
* **Fields** (middle): a form built from the resource's Json. Edits are
  checked on Return or when the field loses focus: integer type and range,
  4cc length, string length; a rejected value is reverted and the error is
  shown in the status line. Enums (toolbox category, registry class, film
  kind, word specials, ...) are dropdowns; bitmasks (tool attributes, world
  mask, event grades, hall-event months, word sex/age/flags) are checkboxes;
  minute-of-day fields show `hh:mm`. Tables (variations, member tables, the
  WORLDDEF grade table, T2EV events, VIPs, speech lines, silhouettes, ...)
  are a row list with Add (duplicates the selected row), Remove, Up and
  Down, and the selected row's fields below. Text resources open in a text
  editor (Apply text); PGDF/PZDF show their 4-byte slots; bitmaps offer
  Import/Export PNG; sounds show their format and play.
* **Preview** (right): the sprite sheet with its OBJMAP rects (for a
  bitmap, its OBJMAP, or a template definition's sheet 1000). Pick a part in
  the list or click it on the sheet; its rect can be edited and applied
  (the OBJMAP text is updated in place). BINF draws the world backdrop from
  bitmap 1000's Sky tiles (click to paint the value in *Paint value*); FINF
  draws the build mask (click to toggle a unit). Palettes show as 16x16
  swatches; the Sounds group lists every WAVE with Play/Stop.

File > **Save** / **Save As** write OSP v3; **Export .t2p** writes
`<source file name>` into a chosen folder.

Command line (also used for headless smoke tests and scripted edits):

```
PluginEditor <plugin> [--select TYPE:ID] [--part N] [--advanced]
             [--set TYPE:ID:PATH=VALUE]... [--export-t2p out.t2p]
             [--save-osp dir] [--capture shot.png]
```

`--set` takes a Json path, e.g. `--set ATTR:128:name=Burger` or
`--set TNDF:1:variations[0].price=777`, and goes through the same
validation as the form. `--capture` (alias `--screenshot`) saves the third
rendered frame and exits.

**Not editable:** RT_VERSION's node lengths are stored, so version strings
are shown (`text`) but only the raw node values are editable (advanced);
bitmaps are replaced by importing a PNG (no pixel painting); OBJMAP blocks
are edited as text or by rect; PGDF/PZDF are slots of unknown meaning.

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
