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
   plugin runtime / SDK. Its ABI is reconstructed here from the import names
   plus the plugin's own code; the key parts are now **confirmed against the
   real `T2DLL.dll`** (loaded in Ghidra — see §9).
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
5       u32      mAttrib        16 (Coin), 2 (Condo/Apt), 1 (Burger/transport) — packed bitfield; see §9.1
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

| Plugin        | mItemType | mAttrib | mGameLevel | mID   | mPluginName        |
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

> **Open question:** ~~`field5` (16/2/1) has no obvious analogue in the text
> form…~~ **RESOLVED.** `field5` is **`mAttrib`** — confirmed by
> `CFilePlugin::GetAttrib` in `T2DLL.dll` (§9.1). The text form's
> `(mAttrib) 16777215` was the oracle all along; the binary value is a packed
> attribute bitfield (Coin=0x10, Condo/Apt=0x02, Burger/transport=0x01).
> `mItemType` suffices for type detection regardless.

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

> **CORRECTED in §10.5.** Decompiling `T2PlaceParamDef`'s accessors shows
> DPDF is a per-`EPlace` scoring table (`{Percent, Score, Rate}`, 0xc stride),
> not w/h/baseX/baseY. The `[70,50,100,80]` above are the leading u32s before
> the EPlace array. A richer DPDF is also nested inside the `FDmT` member
> table (§10.4). The w/h/baseX/baseY reading here was a byte-pattern guess
> that happened to align; prefer §10.5 for new code.

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

> **CORRECTED in §10.4.** Decompiling `T2TenantMemberTableDef`
> (`T2DLL.dll @ 0x100631e0`) shows this "SLDF" is actually the **`FDmT`
> member table** opened from inside the Tenant Def constructor, and each
> entry is a 0x30-byte `T2TenantMemberDef` whose real fields are
> `DemandType / StartSilhouetteType / EndSilhouetteType / OptionType /
> NumOfPeople / Rate(float) / Name / Percent` — not roleId/f1/f2/f3. The
> "roleId 1000+" above was `StartSilhouetteType`; "f3=300" was `Percent`.
> Prefer §10.4 for new code; the byte offsets here are a coincidental
> alignment of the first member.

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

- ~~**ATTR `field5`** (16/2/1): no clean analogue in the text form.~~
  **RESOLVED in §9.1** — it is `mAttrib`, confirmed by `CFilePlugin::GetAttrib`
  in `T2DLL.dll`. A packed attribute bitfield.
- **WODF `cond`/`color`**: the per-entry u32 pair (always 3 / 0xFF) needs an
  enum sweep across more plugins. *(Loading path now known: WODF reaches the
  engine via the `T2WordDef`/`T2WordDefArray` readers; the TNDF/SLDF/member
  data is parsed by `T2TenantMemberTableDef` — see §10.3/§10.4.)*
- **MVDF flag u32s**: capacity / max-floors / speed for transport. *(The Mover
  Def parser is `T2MoverDef`, opened as `"FDvM"` id 1 by `LoadT2MoverDef`
  `0x100420c0`; its constructor is the target for field-mapping — see §10.)*
  Map when implementing the elevator/stair sim.
- **TMDF / PADF**: binary structs deferred; OBJMAP already covers static
  rendering. Revisit when animating shop interiors.
- ~~**SLDF/DPDF byte layouts (§3.3, §3.7)**:~~ **CORRECTED in §10.4/§10.5.**
  The Def constructors in `T2DLL.dll` are now the source of truth.
- ~~**Population sweep**:~~ **DONE.** `scratch/plugin_resource_audit.py`
  parses ATTR/DESC/DPDF across all 105 PE plugins; 105/105 parse cleanly.
  Two new DESC kinds (`lPdW` World, `lPoO` OuterObj) fell out of the sweep
  and are now in the table above.
- **SP-format plugins** (51 of them, magic `"SP"`): a separate container format
  handled by `WindowsPEExecutable::loadSP`. Out of scope here; they lack
  OBJMAP/ATTR and need their own doc.
- **Superseded field maps:** see `doc/yoottower/plugin-schema.md` for the byte-exact, population-validated schema of every resource (several labels in §3/§9 above are corrected there — ATTR mID/mItemType, TMDF vs SLDF, PLDF/DPDF/PADF), and `tools/yoottower/t2p_schema.py` for the reference codec.

---

## 8. Implementation status

The HIGH-confidence resource types are parsed in C++:

- `source/PluginResources.{h,cpp}` — `FourCC` helper + `PluginAttr` /
  `PluginDesc` / `PluginDpdf` / `PluginPldf` / `PluginMvdf` POD structs +
  `parsePlugin{Attr,Desc,Dpdf,Pldf,Mvdf}` functions. Pure functions over a
  byte buffer (no Plugin/PE dependency), so they unit-test with hand-crafted
  inputs. MVDF's binary-header fields (itemID confirmed = mID; the capacity/
  speed dwords remain inferred pending the T2DLL Ghidra pass) plus a robust
  printable-ASCII-run extraction of the price/category/name strings.
- `Plugin::getAttr()/getDesc()/getDpdf()/getPldf()/getMvdf()` — thin accessors
  that fetch the resource and call the parsers (ATTR/DESC/DPDF/PLDF at id 128,
  MVDF at id 1). Lazy via `ensureLoaded()`.
- `source/PluginObjmap.{h,cpp}` — the OBJMAP sprite-layout parser + writer
  (`parsePluginObjmap` / `writePluginObjmap` / `findObjmapBlock`). Implements
  the line-based algorithm in `objmap-format.md` §"Implementation guidance".
  Parse → write → parse is identity (covered by `testObjmapRoundTrip`).
- `source/OspPlugin.{h,cpp}` — the OpenSkyscraper plugin format: reader
  (`loadOspPlugin`), writer (`saveOspPlugin`), and the read-only t2p bridge
  (`convertT2pToOsp`). See `doc/openskyscraper/plugin-format.md`.
- `source/PluginBitmapLoader` — gained `decodeBitmapResource` (Image-returning
  variant) for the editor's PNG export path.
- `tests/test_main.cpp` — byte-level parser tests (calibration hex) plus
  integration tests against the real vendored plugins, including OBJMAP
  (Stair spiral-staircase oracle) and the OSP round-trip.
- `scratch/plugin_resource_audit.py` — population-wide validator; sweeps
  all 105 PE plugins, reports DESC-kind / mItemType distribution, exits
  non-zero on any parse failure.

**Standalone plugin editor** (`tools/PluginEditor/`, `PluginEditor` target):
loads `.t2p` read-only or `.osp-plugin` directories, edits the Tenant-class
fields (identity / price / sprites / transparent color), and saves only to
the OSP format. Tenant vertical slice; see `doc/openskyscraper/plugin-format.md`.

What remains for full data-driven item loading (future phases):

1. **Remaining medium-confidence parsers** (WODF/SLDF/TNDF): structure is
   documented in §3.6–§3.8; promote to C++ once a consumer needs them (e.g.
   WODF for speech, SLDF for spawned roles). PLDF (§3.10) and MVDF (§3.9) are
   now parsed; MVDF's capacity/speed header dwords are still field-mapped by
   inference pending the T2DLL Ghidra pass.
2. **OBJMAP 200/1100 (SHIL walking frames):** the parser handles them and the
   OSP converter + editor now surface the walking-frames sheet (Mover slice).
   Directional-frame *editing* in the UI is still a follow-up.
3. **Replace hardcoded plugin-name matching.** `YootCondo.cpp` currently
   hardcodes `"Condo.t2p"`; the DESC kind + ATTR mItemType now let the item
   registry pick plugins data-drivenally. Do this together with the
   transparent-color table (§"Open questions" of `objmap-format.md`).
4. **Wire OSP plugins into `Factory::loadPrototypes`** so the game consumes
   the native format instead of the hardcoded prototype registry.
5. **TMDF/PADF** (LOW): revisit when animating shop interiors.
6. **Delete the broken non-suffixed Ghidra misimports** (see Ghidra notes).

## 9. T2DLL.dll confirmation pass (Ghidra, the real runtime)

> **Breakthrough:** OpenSky now has the actual `T2DLL.dll` (the game's plugin
> runtime/SDK) loaded in Ghidra. The earlier sections of this doc were
> reconstructed *without* it — purely from plugin import names and the
> plugins' own code. The decompilation below confirms those inferences and
> resolves the two oldest open questions. Every offset in this section is
> read straight from the disassembly; `✓` marks a claim from §1–§8 that the
> binary proves exactly.

### 9.1 Open question #1 RESOLVED — ATTR `field5` is `mAttrib`

The mysterious ATTR byte (values 1/2/16 across the calibration set, "no clean
analogue in the text form") is **`mAttrib`**. `CFilePlugin::GetAttrib`
(`T2DLL.dll @ 0x10003680`) is a one-liner:

```c
uint CFilePlugin::GetAttrib(CFilePlugin *this) { return *(uint*)(this + 0x4c); }
```

The text form of ATTR in `DefaultToolPalette.t2p` even lists it:
`(mAttrib) 16777215` (= `0xFFFFFF`). So the ATTR binary layout (§3.1) is now
fully named — `field5` should be relabelled **`mAttrib`**. The low bits are a
packed attribute bitfield (Coin=0x10, Condo/Apt=0x02, Burger/transport=0x01).

### 9.2 `CFilePlugin` struct layout (confirmed)

Decompiling every `CFilePlugin::Get*` accessor pins down the in-memory layout
that `Load` (§5 step 3) populates from the ATTR resource. This is the
definitive field map; the ATTR byte offsets in §3.1 map 1:1 onto these:

| `CFilePlugin` offset | Accessor (T2DLL addr)        | ATTR field        |
|----------------------|------------------------------|-------------------|
| `+0x48`              | `GetID` (`0x10003670`)        | `mID`             |
| `+0x4c`              | `GetAttrib` (`0x10003680`)    | `mAttrib` (was field5) |
| `+0x50`              | `GetDependID` (`0x10003690`)  | `mDependID`       |
| `+0x54`              | `GetGameLevel` (`0x100036a0`) | `mGameLevel`      |
| `+0x58`              | `GetSecretItem` (`0x100036b0`)| `mSecretItem`     |
| `+0x5c`              | `GetItemType` (`0x100036c0`)  | `mItemType` (4cc) |
| `+0x60`              | `GetMustItem` (`0x100036d0`)  | `mMustItem`       |

### 9.3 `T2TenantDef` accessor offsets confirmed ✓

The §2.3 offsets (inferred without T2DLL) are **byte-for-byte correct**:

| Accessor                        | T2DLL addr  | Offset | ✓ |
|---------------------------------|-------------|--------|---|
| `T2TenantDef::GetNumOfPattern`  | `0x100054f0`| `0x15c` | ✓ |
| `T2TenantDef::GetEntranceV`     | `0x100054b0`| `0x164` | ✓ |
| `T2TenantDef::GetEntranceH`     | `0x100054c0`| `0x168` | ✓ |

### 9.4 `GetRsrcType` proves the DESC↔class identity

`T2*Plugin::GetRsrcType()` returns the class's own DESC 4cc. Sample
(`T2AdvertisePlugin::GetRsrcType @ 0x10003f00`):

```c
uint T2AdvertisePlugin::GetRsrcType(...) { return 0x4164506c; } // "lPdA"
```

This is the inverse of the §2.2 calibration proof: the engine asks a plugin
"what DESC tag are you?" and the plugin answers with the literal constant it
was constructed with. `lPdA` = `0x4164506c` (little-endian) decodes to the
Advertise-plugin class tag — the same byte-reversed mnemonic scheme as the
other `lPnT`/`lPvM`/`lPdW`/`lPoO` tags in §3.2.

### 9.5 OBJMAP parser is `T2ImageObj::AddObject` (the reference implementation)

The single xref to the `"OBJMAP"` string (`0x100c7938`) is inside
`T2ImageObj::AddObject(HMODULE, uint id, T2BitImage*)` at `0x100391c0`. This
is the engine's own OBJMAP parser, and it **confirms and extends** the
grammar in `objmap-format.md`:

- It calls `FindResourceA(hMod, MAKEINTRESOURCE(id), "OBJMAP")` then
  `LoadResource`/`LockResource`/`SizeofResource`, copies the bytes to a
  malloc'd buffer, and tokenizes with **`strtok`** on the delimiter set at
  `0x100c7910`. Tokens are matched case-insensitively (`_stricmp`).
- **Each DefParts block is `0x68` (104) bytes** in the parsed struct array
  (`GlobalReAlloc(..., (count+1)*0x68, ...)`). This is the per-block record
  size OpenSky's `ObjmapBlock` should mirror for byte-fidelity.
- Token keywords confirmed (string addrs in `.rsrc`/`.data`):
  `DefParts`, `Name`/`ID`, `Pattern`, `Grade`, `Offset`, `Floor`, `Parts`,
  and rect flags `Transparent` / `Halftone` (plus two more flag names). Rect
  coords are parsed via `sscanf(buf, "%hd %hd %hd %hd", ...)` after commas
  are rewritten to spaces — i.e. **`Rect a,b,c,d`** as documented. ✓
- `#<n>` after `Parts` is a numeric part id; a bare name after `Parts`
  resolves to a previously-defined part by linear scan (the `Parts`/child
  reference mechanism).
- Unknown tokens and undefined-part references raise `MessageBoxA` errors
  (`"Undefined token: %s"`, `"Undefined parts:%s"`), so a malformed OBJMAP
  is loud-fail, not silent.

> Net: the OBJMAP text format is now backed by the parser source, not just
> by hex inspection. The §"Implementation guidance" line-based parser in
> `objmap-format.md` remains correct (strtok-on-whitespace is equivalent to
> the tab/CRLF split described there).

### 9.6 Binary resource reader: `T2Archive`

The binary resources (ATTR/DPDF/WODF/MVDF/PLDF/...) are not parsed ad-hoc;
they go through a stream object, **`T2Archive`** (`T2DLL.dll`), which wraps
the resource bytes. Key primitives (all confirmed by decompilation):

| Primitive (`T2Archive::`) | Addr        | Reads |
|---------------------------|-------------|-------|
| `operator>>(uint&)`       | (stream op) | one little-endian u32 |
| `operator>>(ushort&)`     | (stream op) | one little-endian u16 |
| `operator>>(uchar&)`      | (stream op) | one byte |
| `ReadPStr(char*)`         | `0x100171c0`| 1-byte length `n`, then `n` bytes (Pascal string — matches §3 pstr convention ✓) |
| `ReadSRect(RECT&)`        | `0x10017360`| four little-endian **short** (left/top/right/bottom) |
| `LArray::ReadAsDWord`      | `0x100136b0`| count + count×u32 (array form) |
| `LArray::ReadAsWord`       | `0x100134f0`| header + count×u16 |
| `LArray::ReadAsChar`       | `0x10013720`| header + count×u8 |

This confirms the "version byte then fixed/ counted fields" pattern assumed
throughout §3: the leading `01` is just the first `operator>>` these readers
consume. `ReadPStr` precisely validates the pstr convention used by
WODF/SLDF/TNDF/MVDF (§3.6–§3.9). `ReadSRect` shows OBJMAP rects and any
embedded `SRect` use **16-bit** coordinates.

### 9.7 `CPEFile` — how the engine enumerates `.t2p` resources

`CPEFile::GetResourceName` (`0x1000f120`) decodes `_IMAGE_RESOURCE_DIRECTORY`
name entries (handling the high-bit "name-offset vs ID" flag, converting
UTF-16 resource names to ANSI). This is the PE-walker the engine uses to
discover the custom resource types generically — i.e. it does **not**
hardcode `FindResource(...,"ATTR")` for every type; it walks the directory
and dispatches by name. `CResFile::GetResourceInfo` (`0x10001700`) hands back
the cached `HINSTANCE`/`HRSRC` pair a parsed resource is bound to.

### 9.8 What this changes for OpenSky

- **Relabel `field5` → `mAttrib`** in `PluginAttr` / the OSP schema / tests.
- **Trust the §2.3 / §3.1 field maps**: they were right without T2DLL, and
  T2DLL confirms them. No rework needed.
- The OBJMAP parser can be tightened to mirror the engine's record size
  (`0x68`/block) if we ever want bit-exact fidelity (not required for
  rendering, useful for a future round-trip-to-t2p goal).
- Future medium-confidence work (MVDF/WODF/SLDF field-mapping, §3.6–§3.9)
  should target the typed `T2*Def` constructors (§10) now that their addresses
  are known, rather than continuing to byte-guess. **The Def constructors live
  in `T2DLL.dll`, not the plugins** — see §10.2.

## 10. The plugin-resource loading architecture (T2DLL, authoritative)

> §9 confirmed individual facts; this section documents the **mechanism** by
> which the engine turns a `.t2p`'s `.rsrc` blob into typed `T2*Def` objects.
> All addresses are in `T2DLL.dll` unless noted. This supersedes the §5 step-4
> guess ("ConstructTemplatePlugin builds the T2*Def").

### 10.1 There are TWO resource-type naming families

Decompiling every `CResFile::OpenResource` call site reveals that resource
types are addressed by a **4cc packed big-endian into a u32** (i.e. the type
name's ASCII bytes, MSB first). Two families exist:

**Family A — identity resources (id 0x80).** Type name is the literal 4cc.
These are read once, in `CFilePlugin::CFilePlugin` (§9.2), to populate the
shared identity fields:

| u32 tag        | bytes (LE)   | type name | parsed by                |
|----------------|--------------|-----------|--------------------------|
| `0x41545452`   | `52 54 54 41`| `ATTR`    | `CFilePlugin` ctor       |

(The `DAT_100c7028` default-resource path uses the string `"Attr"`; the
file-backed path uses the u32 `0x41545452`. Same resource.)

**Family B — template/Def resources (id 1).** Type name is **`"FD" + <class
mnemonic>`**, where the mnemonic mirrors the DESC tag's suffix. These carry
the per-class behavior data (the TNDF/SLDF/WODF/MVDF/DPDF content):

| u32 tag        | type name | mnemonic | class           | opened by            |
|----------------|-----------|----------|-----------------|----------------------|
| `0x546e4466`   | `FDnT`    | `nT`     | Tenant          | `LoadT2TenantDef` `0x10063840` |
| `0x4d764466`   | `FDvM`    | `vM`     | Mover           | `LoadT2MoverDef`  `0x100420c0` |
| `0x4164506c`   | `lPdA`*   | —        | Advertise       | `T2AdvertisePlugin::LoadSelf` `0x10015d60` |
| `0x48454446`   | `FDEH`*   | —        | HallEvent       | `T2HallEventPlugin::LoadSelf` `0x10038300` |

\* Advertise/HallEvent use a slightly different tag shape (`"lPdA"`, `"FDEH"`)
because they are leaf plugins that load their own body directly rather than
spawning a `T2*Def`. The Tenant/Mover `FDxx` tags are the load-bearing ones
for item plugins.

> **Naming rosetta stone:** `"FD"` = "Field/Format Def"; the suffix reuses the
> DESC mnemonic (`nT`=te**N**a**T**, `vM`=mo**V**e**M**). This is why the
> doc's §3.6–§3.9 never found "WODF"/"SLDF"/"TNDF" strings in T2DLL — those
> resource-type **names** only appear in the `.t2p` PE directory; the engine
> addresses the data through the typed `FDxx` Def constructor instead.

### 10.2 The Def constructors live in T2DLL, not the plugins

`LoadT2TenantDef` / `LoadT2MoverDef` do **not** parse bytes. They:
1. `OpenResource(hMod, id=1, type="FDnT"/"FDvM")` → a `CResFile` stream;
2. `GetProcAddress(hMod, "ConstructTemplatePlugin")`;
3. call the **plugin's** `ConstructTemplatePlugin(specifier, resFile)`.

But the plugin's `ConstructTemplatePlugin` (verified in `Burger.t2p`) just
`operator new(0x1b8)`s the Def and **delegates to T2DLL's
`T2TenantDef::T2TenantDef`** (a thunk the plugin imports). So the actual
byte-level parsing of TNDF/SLDF/WODF/DPDF lives in `T2DLL.dll` at
`0x100623c0` (`T2TenantDef::T2TenantDef`). The plugin's only real job is to
**declare which class** (via the 4cc it passes) — confirming §2.2's thesis
that DESC is the class discriminator.

`ConstructProgramPlugin` (Burger `0x1000b900`) allocates `0x6c` and passes
`0x546e506c` (`"lPnT"`); `ConstructTemplatePlugin` (Burger `0x1000b970`)
allocates **`0x1b8` (440)** and passes `0x546e4466` (`"FDnT"`). The 440-byte
size **confirms the doc's §2.3 "T2TenantDef is 0x1b8 (440) bytes" claim
exactly**. ✓

### 10.3 `T2TenantDef::T2TenantDef` — the master TNDF/SLDF/DPDF parser

`0x100623c0`. Reads the `FDnT` stream sequentially via `CResFile::operator>>`
and, partway through, opens two **nested** Family-B resources. Inheritance
chain: `T2TenantDef` → `T2EquipDef` (`0x1002f240`) → `T2ToolDef` → base; each
constructor reads its own slice of the same stream. Field order written into
the struct (offsets cross-checked against §9.3 accessors):

```
T2EquipDef slice:  +0x11c, +0x120, (loop NumOfInMoney × {+0x154,u,u}),
                   +0x134, +0x138, +0x13c (count), +0x144[×count≤4]
T2TenantDef slice: +0x19c, +0x1a0, +0x158, +0x15c (NumOfPattern ✓),
                   +0x160, +0x1a4, +0x164 (EntranceV ✓), +0x168 (EntranceH ✓),
                   +0x16c, +0x170, +0x174, +0x178, +0x17c, +0x104,
                   (3 discarded u32), +0x184 (CString name)
then opens nested resources:
  "FDmT" (id 1) → T2TenantMemberTableDef (0x28 B)  → stored at +0x18c  [SLDF]
  "FDaP" (id 1) → T2PeopleAnimeDef     (0x18 B)  → stored at +0x1a8  [people anim]
```

So the doc's standalone "SLDF"/"TNDF" resources are actually the **`FDmT`
member table** opened from inside the Tenant Def constructor. This corrects
§3.7/§3.8: SLDF and TNDF are not two independent top-level resources; they
are sub-blocks of the `FDnT` Tenant template, parsed by
`T2TenantMemberTableDef`.

### 10.4 `T2TenantMemberTableDef` — the real SLDF/member parser (corrects §3.7)

`0x100631e0`. This is what the doc called "SLDF". Layout it reads:

```
+0x08  int          (table kind / category)
+0x0c  uint         specialFlag   (matches GetSpecialFlag ✓)
optional DayParamDef*  +0x14     (present if a leading count ≠ 0)
optional PlaceParamDef* +0x18    (present if a leading count ≠ 0)  [DPDF lives here]
+0x20  int          NumOfElem     (matches GetNumOfElem ✓)
+0x24  T2TenantMemberDef[count]   (each 0x30 / 48 bytes)
```

**Two corrections to the doc:**
1. **DPDF is embedded here**, not a standalone id-128 resource. The
   `T2PlaceParamDef` (§10.5) is an optional sub-block referenced by the
   member table. (The doc's §3.3 standalone DPDF at id 128 is the
   *default-placement* variant used by the placement cursor; the richer
   per-EPlace DPDF is nested in the member table.)
2. The §3.7 "SLDF entry = `{roleId, f1, f2, f3, pstr}`" was a byte-pattern
   guess. The real entry is a **`T2TenantMemberDef` (0x30 B)** whose
   accessors (Burger ordinals) are: `GetDemandType`, `GetStartSilhouetteType`,
   `GetEndSilhouetteType`, `GetOptionType`, `GetNumOfPeople`, `GetRate(float)`,
   `GetName(CString)`, `GetPercent`. The doc's "roleId 1000+" was actually
   `StartSilhouetteType`; "f3=300" was likely `Percent`.

### 10.5 `T2PlaceParamDef` — DPDF corrected (replaces §3.3's w/h/baseX/baseY)

`0x100045d0`. Accessors (§9.3-area) reveal DPDF is **not** the simple
`w/h/baseX/baseY` the doc guessed. It is an array indexed by `EPlace` with a
**0xc (12) byte stride**:

```
per EPlace entry (at base + EPlace*0xc):
  +0x04  uint    Percent
  +0x08  short   Score
  +0x0c  float   Rate
```

`GetPercent(EPlace)`→`*(u32*)(this + e*0xc + 4)`;
`GetScore(EPlace)` →`*(short*)(this + e*0xc + 8)`;
`GetRate(EPlace)`  →`*(float*)(this + e*0xc + 0xc)`.

The doc's §3.3 "`[70,50,100,80]`" were the first four raw u32s of this
structure (the leading fields before the EPlace array), not w/h/baseX/baseY.
The placement footprint (w/h) is read elsewhere; DPDF is a placement-scoring
table.

## Ghidra project notes

- **`T2DLL.dll` is now loaded** (`x86:LE:32:default`, image base `0x10000000`,
  6460 functions, 25025 symbols). It is the authoritative source for every
  claim in §9. The earlier "we do not have T2DLL.dll" caveat in §1 is
  superseded for the items §9 covers.
- The **canonical** programs are the suffixed `.0` / `.1` / `.2` variants
  (e.g. `/Coin.t2p.0`): `x86:LE:32:default`, image base `0x10000000`, ~1100
  functions after auto-analysis, 2298 symbols.
- The **non-suffixed** programs (`/Coin.t2p`, `/Condo.t2p`, …) are broken
  `x86:LE:16:Real Mode` misimports (1168 bytes, 1 function) — **do not use**.
  They should be deleted from the project in a future cleanup.
- Auto-analysis was run on the 8 calibration `.0` programs for this doc.
