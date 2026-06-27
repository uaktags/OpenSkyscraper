# YootTower Plugin Resource Formats & Plugin Model

**Status:** Reverse-engineered and implemented. Confidence HIGH for the type
catalog and the ATTR/DESC/OBJMAP/T2DLG/WAVE/DPDF layouts (ATTR/DESC/DPDF are
now parsed in C++ and validated against all 105 PE plugins); MEDIUM for
MVDF/WODF/SLDF/TNDF/PLDF (structure clear, some fields inferred); the
TMDF/PADF binary structs are partial and flagged as such.
**Method:** Raw resource bytes dumped from the calibration set via
`pefile`, then cross-checked against the plugin DLLs' own code disassembled in
Ghidra (the `ConstructProgramPlugin` factory functions + `T2*Def`/`CFilePlugin`
class accessors). The plugin **class hierarchy** and the **DESC→class mapping**
were confirmed by decompilation, not just byte-guessing. The full-population
sweep (`scratch/plugin_resource_audit.py`) closes the loop: 105/105 PE plugins
parse cleanly, and surfaced two extra DESC kinds (World, OuterObj).
**Calibration oracle:** the `ConstructProgramPlugin` factory in each plugin
passes a 4cc constant to its item-class constructor; that constant reproduces
the DESC resource's tag byte-for-byte (proof in §3).

This document is the sibling of `objmap-format.md`. OBJMAP (sprite layout) and
`RT_BITMAP` (DIB sheets) are fully documented there and are not repeated here.

---

## 1. TL;DR — what a `.t2p` plugin actually is

A `.t2p` file is a **Win32 PE32 DLL** (~200–400 KB), not a passive asset blob.
Each plugin:

1. **Imports** ~400–480 mangled C++ symbols from `T2DLL.dll` — the game's
   plugin runtime / SDK. **We do not have `T2DLL.dll`**; its ABI is
   reconstructed here from the import names plus the plugin's own code.
2. **Exports** 867–920 symbols: a statically-linked framework (MetroWerks
   **PowerPlant** Mac class library — the `L`-prefixed classes — plus the
   `T2`/`C`-prefixed YootTower framework) and the plugin's own item classes.
3. Declares a **factory function** `ConstructProgramPlugin` (and a companion
   `ConstructTemplatePlugin`) which the engine calls to instantiate the
   plugin's item. The 4cc constant passed to the constructor **is** the DESC
   resource's tag — so DESC is the plugin-type discriminator.
4. Carries its item **data** in Win32 resources of several custom types
   (`ATTR`, `DESC`, `OBJMAP`, `DPDF`, …). The CODE (behavior) lives in the
   DLL's `.text`; the DATA (definition) lives in `.rsrc`.

**Implication for OpenSky:** because we cannot (and should not) execute the
Win32 code on Linux, "loading a plugin fully" means **parsing every resource
type as data** and reimplementing the behavior natively — exactly the pattern
OBJMAP/`RT_BITMAP` already established. The plugin's C++ classes are useful
only as semantic documentation (what kind of item is this, what does it
override), which §4–§5 capture.

---

## 2. Plugin / item class model (from Ghidra)

The framework is a small, regular hierarchy. Every calibration plugin
re-exports the same ~867-symbol framework; the per-plugin variation is which
`T2*Plugin` / `T2*Def` pair its factory constructs.

### 2.1 The CPlugin hierarchy (plugin shells)

```
CPlugin                      (base: GetType, GetModuleHandle, IsLoaded, GetInfoString)
├── CDefRsrcPlugin           (default-resource plugin)
├── CFilePlugin              (file-backed plugin: ATTR fields live here — §3.1)
│   ├── CResourcePlugin
│   └── CProgramPlugin       (behavior carrier: IdleProc, DrawProc, HasDrawProc,
│                            │  LoadAllTemplatePlugin, GetTemplatePlugin)
│   └── T2SubPlugin          (common base for the typed plugin kinds below)
└── CFilePluginDB / CFilePluginList   (the on-disk plugin catalog)
```

The **typed plugin kinds** (subclasses of `T2SubPlugin`, each with a matching
`T2*Def` definition class):

| Plugin class        | Def class          | Carries                                  |
|---------------------|--------------------|------------------------------------------|
| `T2TenantPlugin`    | `T2TenantDef`      | residences, shops, offices, venues       |
| `T2EquipPlugin`     | `T2EquipDef`       | equipment (fire dept., etc.)             |
| `T2MoverPlugin`     | *(transport def)*  | stairs, escalators, elevators            |
| `T2ToolPlugin`      | `T2ToolDef`        | buildable tools / structure              |
| `T2MatterPlugin`    | `T2MatterDef`      | matter/floor structure                   |
| `T2OuterObjPlugin`  | `T2OutObj` / `T2OuterObjDef` | out-view objects (exterior sprites) |
| `T2HallEventPlugin` | —                  | hall events (VIPS / scripted events)     |
| `T2WorldPlugin`     | `T2WorldDef`       | scenario/world plugins (Tokyo, Hawai)    |
| `T2OptionPlugin`    | —                  | option/settings plugins                  |
| `T2MoviePlugin` / `T2MoverPlugin` / `T2AdvertisePlugin` | — | media / ads |

### 2.2 Calibration proof — `ConstructProgramPlugin`

Decompiled in Ghidra (`Coin.t2p.0 @ 0x1000bda0`, etc.). Each factory allocates
its item-class object and passes a 4cc literal to the constructor:

| Plugin            | Constructs (Ghidra)      | 4cc literal      | = DESC tag | Meaning        |
|-------------------|--------------------------|------------------|------------|----------------|
| `Coin.t2p`        | `T2TenantPlugin`         | `0x546e506c`     | `lPnT`     | Tenant plugin  |
| `Burger.t2p`      | `T2TenantPlugin`         | `0x546e506c`     | `lPnT`     | Tenant plugin  |
| `StdElev.t2p`     | `T2MoverPlugin`          | `0x4d76506c`     | `lPvM`     | Mover plugin   |
| `DefaultToolPalette.t2p` | *(palette)*       | `0x544f504c`     | `LPOT`     | Tool palette   |

`0x546e506c` read as little-endian bytes is `6c 50 6e 54` = `"lPnT"`, which is
**exactly** the 4 bytes at offset 1 of that plugin's `DESC` resource.
The constructor 4cc therefore *is* the DESC tag. This is the single strongest
calibration point in the whole investigation: **DESC identifies the plugin
class; ATTR's tag identifies the specific item sub-type** (§3.2).

### 2.3 The `T2*Def` field model (sample: `T2TenantDef`)

`T2TenantDef` is 0x1b8 (440) bytes. Confirmed accessor offsets (Ghidra):

| Accessor             | Offset | Field                  |
|----------------------|--------|------------------------|
| `GetNumOfPattern`    | `0x15c`| pattern/state count    |
| `GetEntranceV`       | `0x164`| vertical entrance pos  |
| `GetEntranceH`       | `0x168`| horizontal entrance pos|
| `IsConcat`           | `0x60` (bit 17) | concat-flags     |

These are the fields the DPDF/PADF placement resources populate. The full
struct map is out of scope for this phase (it would require `T2DLL.dll` to see
the constructor body); the offsets above are enough to confirm that the binary
resources feed a fixed-layout C++ struct.

---

## 3. Resource type catalog

Every resource type observed across the calibration set, with confidence and
where it is documented:

| Type        | Encoding        | id(s) seen      | Conf. | §     | Meaning                                                    |
|-------------|-----------------|-----------------|-------|-------|------------------------------------------------------------|
| `RT_BITMAP` | DIB             | 100,200,1000,…  | HIGH  | objmap-format.md | sprite sheets (the actual pixels)              |
| `OBJMAP`    | text (CRLF)     | 100,200,1000,1001,1100 | HIGH | objmap-format.md | how to slice each RT_BITMAP into sprites     |
| `ATTR`      | binary          | 128             | HIGH  | 3.1   | item identity, unlock level, display name                  |
| `DESC`      | binary (9 B)    | 128             | HIGH  | 3.2   | plugin-class 4cc (Tenant / Mover / Palette)                |
| `DPDF`      | binary (17 B)   | 128             | HIGH  | 3.3   | default placement parameters (T2PlaceParamDef)             |
| `T2DLG`     | text (CRLF)     | 1000,1001       | HIGH  | 3.4   | inspector/info dialog layout                               |
| `WAVE`      | RIFF WAV        | 151,165,175,…   | HIGH  | 3.5   | sound effects                                             |
| `WODF`      | binary+pstr list| 1               | MED   | 3.6   | "words" — speech-bubble lines + color                      |
| `SLDF`      | binary+pstr list| 1               | MED   | 3.7   | people/role types the tenant spawns (staff, residents)     |
| `TNDF`      | binary+pstr     | 1               | MED   | 3.8   | tenant category def (toolbox group + description)          |
| `MVDF`      | binary+pstr blob| 1               | MED   | 3.9   | mover (transport) def: description + prices + name         |
| `PLDF`      | binary (u32[])  | 128             | MED   | 3.10  | price list (base price 1000, factors)                      |
| `PADF`      | binary (u32[])  | 1               | LOW   | 3.11  | placement/palette-animation def (partial)                  |
| `TMDF`      | binary (u32[])  | 1,2             | LOW   | 3.12  | template/frame def (partial; 0xFFFFFFFF sentinels)         |
| `RT_VERSION`| VS_VERSIONINFO  | 1               | —     | —     | standard Win32 version info (ignore)                      |

**Resource id convention:** `128` (0x80) is the id for an item's *primary*
`ATTR`/`DESC`/`DPDF`/`PLDF`. `1` is the id for category-level resources
(`WODF`/`SLDF`/`TNDF`/`MVDF`/`PADF`/`TMDF`). `1000`+ belong to specific
sub-features (dialogs, sprite sheets).

### 3.1 `ATTR` — item attributes (HIGH)

Two forms exist. The **binary form** is the production format used by every
gameplay item; the **text form** appears only in `DefaultToolPalette.t2p` and
is the rosetta stone for the field names.

**Binary layout** (little-endian, all resources begin with a version byte of `01`):

```
offset  type     field          notes
0       u8       version        always 1
1       char[4]  mItemType      item sub-type 4cc, e.g. "GRUB" (see table)
5       u32      field5         16 (Coin), 2 (Condo/Apt), 1 (Burger/transport) — see note
9       char[4]  marker         usually "****"; "NORF" on the hotel-room family
                                (Singleroom/Suiteroom/Twinroom/Maintenance). A
                                category/group code, NOT a fixed sentinel.
13      u32      mGameLevel     unlock level (1 = Star1, 3 = Star3). Coin=3.
17      u32      mMustItem      0/1 — prerequisite flag
21      u32      mID            item id (matches MVDF/TNDF key). Coin=1683.
25      u32      pad            0
29      pstr     mPluginName    length-prefixed (1-byte len) display name
```

`pstr` = Pascal string: 1-byte length `n` followed by `n` bytes (no NUL).

**Calibration (binary form):**

| Plugin        | mItemType | field5 | mGameLevel | mID   | mPluginName        |
|---------------|-----------|--------|------------|-------|--------------------|
| Coin          | `ADEM`    | 16     | 3          | 1683  | Coin Press Machine |
| Condo         | `TRPA`    | 2      | 1          | 1101  | Condo              |
| Apartment     | `NERH`    | 2      | 1          | 1102  | Apartment          |
| Burger        | `GRUB`    | 1      | 1          | 1301  | Burger land        |
| Stair         | `RATS`    | 1      | 1          | 1800  | Stairs             |
| Escalator     | `ACSE`    | 1      | 1          | 1801  | Escalators         |
| StdElev       | `EdtS`    | 1      | 1          | 1810  | Simple Elevator    |

**Text form** (`DefaultToolPalette.t2p`, ATTR id=128, 156 B):
```
(mID)        1000
(mAttrib)    16777215
(mDependID)  0
(mGameLevel) 1
(mSecretItem) 0
(mItemType)  1414484044
(mMustItem)  1
(mPluginName) Default Tool palette
```
`1414484044 == 0x544F504C == "LPOT"` (little-endian) — i.e. for the palette,
`mItemType` coincides with the DESC/plugin-class tag. This is the field-name
oracle: the binary-form columns above are named by analogy to these keys.

> **Open question:** `field5` (16/2/1) has no obvious analogue in the text form
> and may be a packed `mAttrib`/`mSecretItem` or a per-class variant selector.
> Confirm when implementing by reading `CFilePlugin::GetAttrib` in a plugin
> that has a richer ATTR. Does not block type detection (mItemType suffices).

### 3.2 `DESC` — plugin-class discriminator (HIGH)

Fixed 9-byte header:

```
0  u8      version     always 1
1  char[4] classTag    plugin class 4cc (see table) — == ConstructProgramPlugin literal
5  u32     value       always 3 in the calibration set (sub-version? resource count?)
```

**Calibration (binary form):**

| classTag (bytes) | u32 LE      | Plugin class (Ghidra)   | Population (105 PE plugins) |
|------------------|-------------|-------------------------|-----------------------------|
| `lPnT`           | `0x546E506C`| `T2TenantPlugin`        | 93                          |
| `lPvM`           | `0x4D76506C`| `T2MoverPlugin`         | 7                           |
| `lPdW`           | `0x5764506C`| `T2WorldPlugin`         | 3 (Tokyo, Hawai, Kegon)     |
| `lPoO`           | `0x4F6F506C`| `T2OuterObjPlugin`      | 1 (Billboard)               |
| `LPOT`           | `0x544F504C`| Tool palette            | 1 (DefaultToolPalette)      |

The "World" (`lPdW`) and "OuterObj" (`lPoO`) kinds were discovered by the
full population sweep (`scratch/plugin_resource_audit.py`) — they are absent
from the original 6-plugin calibration set. Every one of the 105 PE plugins
now classifies into a known kind; `Unknown` count is 0. The two new tags map
cleanly onto the `T2WorldPlugin` (scenario) and `T2OuterObjPlugin`
(out-view object) classes from §2.1.

> The naming reads as reversed mnemonic fragments: `lP`+`nT` ≈ "Plugin
> Te**n**a**t**", `lP`+`vM` ≈ "Plugin **M**o**v**er". Not load-bearing — the
> u32 is the reliable key. Expect additional tags for the other plugin kinds in
> §2.1 (e.g. an `lPeQ` for `T2EquipPlugin`) once non-calibration plugins are
> swept.

### 3.3 `DPDF` — default placement parameters (HIGH)

Backs `T2PlaceParamDef`. Fixed 17-byte layout:

```
0  u8   version   always 1
1  u32  w         70   (likely cell width / footprint-x)
5  u32  h         50   (likely cell height / footprint-y)
9  u32  baseX     100
13 u32  baseY     80
```

Calibration: Coin and Burger both → `[70, 50, 100, 80]`. These feed
`T2TenantDef`'s `GetEntranceH`/`GetEntranceV` (offsets 0x168/0x164) and the
placement cursor. Only present on placement-driven items (Coin, Burger);
absent on transport (Stair/StdElev use `MVDF` instead).

### 3.4 `T2DLG` — dialog layout (HIGH, already text)

Human-readable, CRLF, tab-separated. Describes the inspector/info dialog for
the item. Already self-documenting; examples:

```
WIDTH   296
HEIGHT  280
MODAL
FONT    12
TITLE   Info of Tenants
TEXT    7001  r  14  10  74  25       ; control-id align x1 y1 x2 y2
BOX     7501  12  8  76  26
EDIT    7003  112 8  285 26  16
IMAGE   7004  View 13 33 59 81        ; references OBJMAP "View" sprite
...
OK      223 181
```

Control keywords seen: `TEXT`, `BOX`, `EDIT`, `IMAGE`, `OK`, `CHECK`,
`FLRNUM`, `ELVMODL`, `PPVIEW` (the elevator "going up / going down" preview).
No binary decoding needed — a line-based parser suffices.

### 3.5 `WAVE` — sound effects (HIGH)

Standard RIFF/WAVE (`RIFF....WAVEfmt ....data`), 8-bit mono PCM, 11025 Hz
(typical). No custom container — feed directly to any WAV decoder. The resource
id is the sound-effect id referenced by the item's `PlaySoundA` calls.

### 3.6 `WODF` — speech-bubble lines (MEDIUM)

A counted list of visitor/resident utterances with color and a condition tag.

```
0  u8   version   always 1
1  u32  count     number of entries
5  entry[count]:
     u32  cond      3 in all calibration samples (condition/event type? — unknown)
     u32  color     0x000000FF (255) in all samples (RGB? text color?)
     u32  f2        0 or 1
     u32  f3        0
     pstr text      the speech line
```

Calibration: Burger → 28 entries, e.g. `"Let me have the potato too."`;
Condo → 13 entries, e.g. `"I'm going to sleep earlier today."`;
Apartment → 10 entries. StdElev → `"The Elevator's crowded."`

> The `cond`/`color` semantics need a second pass (likely an enum + a palette
> index). The text + count are solid.

### 3.7 `SLDF` — spawned people/role types (MEDIUM)

A counted list of character roles the tenant generates (waiters, residents…).

```
0  u8   version   always 1
1  u32  count
5  entry[count]:
     u32  roleId    1000+ (1000 = Elementary school student, 1004 = Waiter,
                  1005 = Waitress, …) — matches the People-anim enums
     u32  f1        1 or 2
     u32  f2        0
     u32  f3        300 (0x12C) — possibly a timing/weight
     pstr roleName
```

Calibration: Condo → `[1000 "Elementary school student", 1001 …]`;
Burger → `[1004 "Waiter", 1005 "Waitress", …]`. Present on tenants that emit
walking people; absent on transport/venue plugins.

### 3.8 `TNDF` — tenant category / toolbox group (MEDIUM)

Defines the toolbox category the item belongs to (e.g. "Residential").

```
0   u8   version     always 1
1   u32  categoryID  == the item's mID (Condo=1101, Burger=1301, …)
5   u32  f1          16
9   u32  f2          1
13  u32  f3          1
17  u32  flags       0x01208680 etc. (bitfield — unknown)
21  pstr description "Tenant living space categories."
..  pstr groupName   "Residential…"  (toolbox category label)
..  (further pstrs / sub-entries)
```

Calibration: Condo → categoryID 1101, "Tenant living space categories.";
Burger → 1301, "The category of restaurants you manage.". The categoryID
matching mID is the cross-reference key into ATTR.

### 3.9 `MVDF` — mover (transport) definition (MEDIUM)

The transport-plugin analogue of TNDF. Contains the pricing, the in-game
description, and the display name as a sequence of pascal strings.

```
0   u8   version     always 1
1   u32  itemID      == mID (Stair=1800, StdElev=1810)
5   u32  count       number of following pstr fields
9   u32  flags[]     a handful of u32s (capacity, speed, max-floors…)
..  pstr[]           description, category label, prices, name, name
```

Calibration (Stair): itemID 1800; strings include
`"The category of transit systems such as stairs and elevators."`,
`"Transit Systems"`, `"$5000"`, `"$0"`,
`"A transit system to connect the adjacent floors."`, `"Stairs"`, `"Stairs"`.
Calibration (StdElev): `"$150000:$50000/car"`, `"$12000/Q"`,
`"A simple elevator can be extended up to 15 floors."`, `"Simple Elevator"`.

> The u32 `flags` (capacity / max-floors / speed) are not yet field-mapped;
> the string sequence is stable. Present **only** on transport plugins
> (`DESC = lPvM`).

### 3.10 `PLDF` — price list (MEDIUM)

Short fixed-layout price definition for commercial items.

```
0  u8   version   always 1
1  u32  f1        40 (0x28)
5  u32  basePrice 1000 (0x3E8) — construction cost
9  u32  f2        10
13 u32  f3        300 (0x12C)
17 u32  f4        50
21 u32  f5        100
```

Calibration: Burger → `[40, 1000, 10, 300, 50, 100]`. The 1000 is the obvious
construction price; the rest are likely income/expense factors. Present on
shops (Burger); absent on residences/transport.

### 3.11 `PADF` — placement/palette-animation def (LOW)

Binary u32 array. Observed on Burger (61 B) only in the calibration set.
Begins `01 01 00 00 00 03 00 00 00 03 00 00 00 02 00 00 00 …` with a count
(`01`) then groups of u32s ending in the repeating value `0x0D` (13).
Likely describes multi-frame placement animation. **Not fully decoded** — defer
until a descriptor-driven shop renderer needs it.

### 3.12 `TMDF` — template/frame def (LOW)

Binary u32 array, 1–2 resources per plugin. Heavy use of `0xFFFFFFFF`
sentinels and small coord-like values (0x19=25, 0x1E=30, 0x24=36). Burger's
two TMDFs (id=1, id=2) suggest one per animation template (day/night, open/closed).
**Not fully decoded.** Likely the per-pattern frame timing/offset map that
complements OBJMAP's rects. Defer; OBJMAP already provides the rects needed
for static rendering.

---

## 4. The mItemType taxonomy (ATTR tag)

The ATTR `mItemType` 4cc identifies the specific item sub-type. Observed
(some are clearly reversed mnemonics — `RATS`=STAR/Stair, `GRUB`=BURG/Burger,
`EdtS`=StdE/St dElev):

| mItemType | Item            | Plugin class (DESC) |
|-----------|-----------------|---------------------|
| `ADEM`    | Coin Press      | `lPnT` (tenant)     |
| `TRPA`    | Condo           | `lPnT`              |
| `NERH`    | Apartment       | `lPnT`              |
| `GRUB`    | Burger          | `lPnT`              |
| `RATS`    | Stairs          | `lPvM` (mover)      |
| `ACSE`    | Escalator       | `lPvM`              |
| `EdtS`    | Simple Elevator | `lPvM`              |
| `LPOT`    | Default Palette | `LPOT` (palette)    |

The full ~105-plugin sweep (`scratch/plugin_resource_audit.py`) confirms the
encoding is a stable 4cc at ATTR offset 1 and that **every PE plugin carries a
unique mItemType** (105 distinct values observed — effectively a per-plugin
item-type id). The full table is generated by the audit; a few examples:

---

## 5. How the engine loads a plugin (reconstructed)

Putting §1–§4 together, the original YootTower engine does roughly:

1. `LoadLibrary("Xxx.t2p")` → gets `HMODULE`; the DLL's `DllMain` runs (CRT
   init only — no game logic).
2. `GetProcAddress(hMod, "ConstructProgramPlugin")` → called with a
   `T2PluginSpecifier`. The factory `new`s the right `T2*Plugin` (Tenant/Mover/
   …) passing the DESC 4cc. The plugin is now a typed object.
3. The engine calls `CFilePlugin::Load` (and `CResFile`), which uses Win32
   `FindResource(hMod, MAKEINTRESOURCE(128), "ATTR")` (and likewise for each
   type) to pull the resources out of `.rsrc` and populate the `CFilePlugin`
   fields (mID, mItemType, mGameLevel, mPluginName) and the `T2*Def` struct.
4. `ConstructTemplatePlugin` builds the matching `T2*Def` (e.g.
   `T2TenantDef`, 440 B) from `DPDF`/`SLDF`/`TNDF`/`TMDF`/`WODF`/`MVDF`/`PLDF`.
5. At runtime the engine calls the plugin's virtuals (`IdleProc`, `DrawProc`,
   `DrawSelf`, `SetUsed`, `DoDestruct`, `PlaySoundA`, …) to drive behavior.

**OpenSky equivalent:** steps 1–2 are replaced by `PluginManager` cataloging;
step 3 by a typed resource parser per type (this doc); step 4 by small POD
structs; step 5 by our own native `Item` subclasses. We never execute the
plugin's `.text`.

---

## 6. Calibration summary (which plugin proves what)

| Plugin            | Proves                                                       |
|-------------------|--------------------------------------------------------------|
| `Coin.t2p`        | ATTR binary form, DPDF, DESC=lPnT, factory 4cc               |
| `Burger.t2p`      | PADF, PLDF, SLDF (Waiter/Waitress), TMDF×2, WODF(28)         |
| `Condo.t2p`       | SLDF (residents), TNDF, WODF(13), TMDF                       |
| `Apartment.t2p`   | TNDF variant, WODF(10), TMDF                                 |
| `Stair.t2p`       | MVDF (transport pricing), DESC=lPvM                          |
| `Escalator.t2p`   | DESC=lPvM (2nd mover sample)                                 |
| `StdElev.t2p`     | MVDF ($150000:$50000/car), DESC=lPvM, richest T2DLL imports  |
| `DefaultToolPalette.t2p` | ATTR text form (field-name oracle), DESC=LPOT       |

---

## 7. Open questions (non-blocking)

- **ATTR `field5`** (16/2/1): no clean analogue in the text form. Likely a
  packed attrib/variant byte. Read `CFilePlugin::GetAttrib` to confirm.
  *(Marker field resolved: it's "****" for most plugins, "NORF" for hotel
  rooms — a category code, not a sentinel. The C++ parser records it.)*
- **WODF `cond`/`color`**: the per-entry u32 pair (always 3 / 0xFF) needs an
  enum sweep across more plugins.
- **MVDF flag u32s**: capacity / max-floors / speed for transport. Map when
  implementing the elevator/stair sim.
- **TMDF / PADF**: binary structs deferred; OBJMAP already covers static
  rendering. Revisit when animating shop interiors.
- ~~**Population sweep**:~~ **DONE.** `scratch/plugin_resource_audit.py`
  parses ATTR/DESC/DPDF across all 105 PE plugins; 105/105 parse cleanly.
  Two new DESC kinds (`lPdW` World, `lPoO` OuterObj) fell out of the sweep
  and are now in the table above.
- **SP-format plugins** (51 of them, magic `"SP"`): a separate container format
  handled by `WindowsPEExecutable::loadSP`. Out of scope here; they lack
  OBJMAP/ATTR and need their own doc.

---

## 8. Implementation status

The HIGH-confidence resource types are now parsed in C++:

- `source/PluginResources.{h,cpp}` — `FourCC` helper + `PluginAttr` /
  `PluginDesc` / `PluginDpdf` POD structs + `parsePlugin{Attr,Desc,Dpdf}`
  functions. Pure functions over a byte buffer (no Plugin/PE dependency),
  so they unit-test with hand-crafted inputs.
- `Plugin::getAttr()/getDesc()/getDpdf()` — thin accessors that fetch
  resource id 128 and call the parsers. Lazy via `ensureLoaded()`.
- `tests/test_main.cpp` — byte-level parser tests (calibration hex) plus
  integration tests against the real vendored plugins.
- `scratch/plugin_resource_audit.py` — population-wide validator; sweeps
  all 105 PE plugins, reports DESC-kind / mItemType distribution, exits
  non-zero on any parse failure.

What remains for full data-driven item loading (future phases):

1. **Medium-confidence parsers** (MVDF/WODF/SLDF/TNDF/PLDF): structure is
   documented in §3.6–§3.10; promote to C++ the same way once a consumer
   needs them (e.g. MVDF when wiring transport pricing, WODF for speech).
2. **Replace hardcoded plugin-name matching.** `YootCondo.cpp` currently
   hardcodes `"Condo.t2p"`; the DESC kind + ATTR mItemType now let the item
   registry pick plugins data-drivenally. Do this together with the
   transparent-color table (§3.1 of `objmap-format.md`).
3. **TMDF/PADF** (LOW): revisit when animating shop interiors.
4. **Delete the broken non-suffixed Ghidra misimports** (see Ghidra notes).

## Ghidra project notes

- The **canonical** programs are the suffixed `.0` / `.1` / `.2` variants
  (e.g. `/Coin.t2p.0`): `x86:LE:32:default`, image base `0x10000000`, ~1100
  functions after auto-analysis, 2298 symbols.
- The **non-suffixed** programs (`/Coin.t2p`, `/Condo.t2p`, …) are broken
  `x86:LE:16:Real Mode` misimports (1168 bytes, 1 function) — **do not use**.
  They should be deleted from the project in a future cleanup.
- Auto-analysis was run on the 8 calibration `.0` programs for this doc.
