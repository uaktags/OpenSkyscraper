# YootTower World plugins, SP-format plugins, T2.dat / t2s.dat / T2Icons.dll

**Status:** reverse-engineered from `T2DLL.dll`, `T2.exe` and the world/tenant
plugins' own `.text`, decompiled with Ghidra 12.1.3 (image base `0x10000000` for
DLLs, `0x00400000` for T2.exe). Every byte of every WORLDDEF/WATR/BINF/FINF/PTDF/
PODF/CODF/SPDF/T2EV/`*EV`/VIPL/STRTBL/PALETTE resource of the three worlds, and
every byte of the 51 SP containers plus their ATTR/DESC/MODF/HEDF/ADDF payloads,
is consumed by the layouts below. Proof: `tools/yoottower/validate_world_sp.py`
(713 checks, all byte-identical round trips).

**Confidence key:** HIGH = read order and meaning taken from the consumer code.
MED = read order from code, meaning taken from accessor names or data patterns.
LOW / `unk*` = read order known, meaning unknown.

**Tools:** `tools/yoottower/world_sp.py` (parser and writer, World + SP),
`tools/yoottower/t2dat.py` (T2.dat/t2s.dat/T2Icons.dll),
`tools/yoottower/validate_world_sp.py` (sweep), `tools/yoottower/extract_world_sp.py`
(writes PNG/WAV/JSON to `~/yoottower-decomp/extract/`, never to the repo).
Run them all with `python3 -I`.

Related: `resource-formats.md` (PE plugin model, ATTR/DESC, `T2Archive`),
`objmap-format.md` (OBJMAP sprite text).

---

## 1. The CResFile reader (applies to every resource below)

All `.t2p` data resources are read through `CResFile` (T2DLL `0x10010ca0..0x10011990`).

| Rule | Evidence |
|---|---|
| A first byte of `0x01` means **binary mode** and is skipped. Any other first byte means **text mode**. | `CResFile::SetupResource` 0x10011230 |
| Binary: **every scalar takes 4 LE bytes**, including `uchar`, `short` and `ushort`. | `operator>>` 0x10011310 (uchar), 0x10011430 (short), 0x10011650 (ushort) |
| Binary RECT is stored **top, left, bottom, right**. POINT is stored **y, x**. SIZE is stored **cy, cx**. | 0x10011730, 0x100116e0, 0x100117e0 |
| Binary string: `u8 len`, `len` bytes, `NUL`. Strings are Shift-JIS (cp932) or ASCII. | `operator>>(char*)` 0x10011830 |
| Text: tokens split on space, tab, CR, LF or comma. `(...)` is a comment. Integers are digits only. Strings run to the end of the line. | `CResFile::Token` 0x10011290 |
| A resource type is a big-endian 4cc constant. The PE resource name is that constant uppercased. Examples: `0x50744466` "PtDf" is resource PTDF; `0x57417472` "WAtr" is WATR. | `OpenResource(HINSTANCE,int,int)` 0x10010cf0 |
| `OpenResource(const char *file, id, type)` checks the first 2 bytes of the file. **`SP`** dispatches to the SP container reader. **`MZ`** dispatches to `CPEFile`. | 0x10010fd0. The magics are at `0x100c70cc` / `0x100c70d0` |

---

## 2. World plugins (DESC `lPdW`): Tokyo.t2p, Hawai.t2p, Kegon.t2p

PE DLLs. `ConstructProgramPlugin` allocates `T2WorldPlugin` (0x74 bytes, tag `0x5764506c`).
`ConstructTemplatePlugin` allocates a world-specific `T2WorldDef` subclass (0x298
bytes, Tokyo 0x10013840). That subclass calls `T2WorldDef::T2WorldDef` in
T2DLL **0x1006e4e0**, which parses WORLDDEF. The subclasses add only behaviour
(ambient sounds, clocks, XEvents); they parse **no** extra resources.

### 2.1 Coordinate system (HIGH)

* A **unit** is one horizontal cell: 8 px at full zoom. A **row** is one floor: 36 px
  pitch, made of 24 px of interior and a 12 px floor/roof slab (Floor.t2p `View`
  rects are 24 / 60 / 96 px for lobby heights 1, 2 and 3).
* Rows grow downward. Row 0 is the top of the world.
  `T2FloorInfo::UnitToFloor(v)` (0x10033d20) returns `ground - v` if `v < ground`,
  otherwise `ground - v - 1`. So row `ground-1` is 1F and row `ground` is B1.

### 2.2 WORLDDEF (id 1, opened by `T2WorldPlugin::LoadT2WorldDef` 0x10073040). Binary.

Read order follows `T2WorldDef::T2WorldDef` 0x1006e4e0. The offset column is the
`T2WorldDef` member offset.

| # | Field | Offset | Tokyo | Hawai | Kegon | Meaning / consumer | Conf |
|---|---|---|---|---|---|---|---|
| 1 | sortKey | +0x50 | 0 | 20 | 10 | `GetSortKey`: order in the world list. Equals ATTR mItemType. | HIGH |
| 2 | height | +0x1f8 | 122 | 62 | 44 | `GetHeight`: world rows. Sizes the T2FloorInfo unit array (`T2FloorInfo` ctor 0x10033a20). | HIGH |
| 3 | width | +0x1fc | 368 | 400 | 192 | `GetWidth`: world units. | HIGH |
| 4 | groundLine | +0x54 | 110 | 55 | 10 | Ground row (`T2FloorInfo::GetGroundLine`, UnitToFloor). | HIGH |
| 5 | buildArea RECT (l,t,r,b) | +0x200 | 36,6,332,120 | 56,3,392,62 | 81,3,186,43 | `GetBuildArea`. `T2FloorInfo::IsValidRange` 0x10033fc0 rejects any rect outside it. | HIGH |
| 6 | entrance POINT (x,y) | +0x60 | 160,106 | 70,54 | 140,10 | No T2DLL consumer found. y sits at or just above ground, so this is probably the initial view or entrance position. | LOW |
| 7 | topFloorLine | +0x68 | 10 | 10 | 8 | `GetTopFloorLine`, FloorArea.top. Highest floor row. Also the crane/option-object floor limit (`CalcCranePos`, `CalcOptionObj`, Santa's flight row). | HIGH |
| 8 | bottomFloorLine | +0x6c | 119 | 61 | 42 | `GetBottomFloorLine`, FloorArea.bottom (RECT bottom, so exclusive). | HIGH (exclusivity MED) |
| 9 | startGrade | +0x70 | 1 | 2 | 3 | Inferred. Hawaii's grade-1 icon is 0 and its grade-1 table row is a dummy. Kegon's grade 1–2 rows point at a missing bitmap 1206 and its PODF has no pools for grades 1–2. | MED |
| 10 | unk74 | +0x74 | 0 | 1 | 2 | Always startGrade−1. | LOW |
| 11 | lobbyHeight | +0x58 | 3 | 2 | 1 | `GetLobbyHeight`. `IsStoppableFloor` 0x1000a310: rows `[ground-lobby, ground-1)` are lobby rows where elevators cannot stop. | HIGH |
| 12 | outViewKind | +0x218 | 2 | 1 | 0 | `GetOutViewKind`: exterior view style. | MED |
| 13 | unk17c | +0x17c | 0 | 0 | 2 | | LOW |
| 14 | tiledBkgnd | +0x228 | 1 | 1 | 0 | `DrawBkgnd` 0x1006eef0. 1 means draw the BINF tile map. 0 means draw bitmap 1000 scaled by (tileW, tileH) per BkgndInfo cell. | HIGH |
| 15 | numGrades | +0x84 | 4 | 4 | 4 | Grade-up steps (≤8). | HIGH |
| 16 | grades[n] (6 ints each) | +0x88 | see 2.2.1 | | | | |
| 17 | bkgndTileHeight | +0x180 | 72 | 36 | 36 | Background tile px (one tile is 2 rows). `DrawBkgnd(uint tile, …)` 0x1006ee50 reads tile `t` at `y = t*tileH`. | HIGH |
| 18 | bkgndTileWidth | +0x184 | 64 | 32 | 32 | One tile is 8 units. Hawai and Kegon art is half-scale and stretched by `BkgndToQD`. | HIGH |
| 19 | unk188 | +0x188 | 8 | 8 | 500 | | LOW |
| 20 | unk18c | +0x18c | 1 | 1 | 1 | | LOW |
| 21 | 10 ints | — | 200,230,250,260,270,280,500,3001,3000,290 | same | 250 and 290 are 0 | **Read and discarded** by the ctor. | HIGH |
| 22 | gradeIconAnimIDs[5] | +0x148..+0x158 | 3011..3015 | 0,3011,3012,3013,3015 | 3011..3015 | Control-panel "Grade" star / "Tower" strips (68x320 ANIM bitmaps). 0 means none. | MED |
| 23 | towerNamePrefix (pstr) | +0x15c | "Yzzt Tower:" | "Yzzt Tower" | "Yoot Tower:" | Title prefix. | MED |
| 24 | dustOnAt | +0x170 | 3 | 3 | 3 | `IsDustOn(x)` is true when `x >= value` (0x10072100). | HIGH (read) |
| 25 | toiletAt | +0x174 | 1 | 1 | 1 | `GetToiletDenomi(x)` returns toiletDenomi when `value <= x` (0x100720c0). | HIGH (read) |
| 26 | toiletDenomi | +0x178 | 10 | 10 | 10 | | HIGH (read) |
| 27 | int | — | 9999 | 9999 | 9999 | Discarded. | HIGH |
| 28 | numCash + cash[] | +0x22c | 8: 0,3000,5000,10000,20000,50000,100000,200000 | same | same | `GetCashAmount(0..7)`: the funds-dialog cash steps. | HIGH |

#### 2.2.1 Grade table (`grades[]`, 6 ints per row; row `L-1` is used when upgrading from grade L)

| Int | Name | Consumer | Tokyo rows (L=1..4) |
|---|---|---|---|
| 0 | vipFlag | `CheckGameLevel` 0x10073290: `(doc+0xa0 & vipFlag) == vipFlag`. `doc+0xa0` holds the VIP-visited bits that `T2VisitVIP` sets. | 1, 16, 256, 4096 |
| 1 | population | Upgrade needs population ≥ value. `T2VisitVIP::Start` 0x1006cd90 sends the VIP once population > value/2. On success it returns 0x2715 and T2.exe FUN_00421e90 increments the grade. | 1000, 2000, 5000, 12000 |
| 2 | gradeUpAnimID | `SetupGradeUpAnimation` 0x10072e70 | 1207, 1208, 1209, 1210 |
| 3 | gradeUpAnimFrames | The bitmap is `frames × 96` px tall (validated). | 13, 13, 13, 11 |
| 4 | unk | Always 0. | 0 |
| 5 | gradeUpAnimParam | `T2DlgItemAnimation::SetAnimationParam(100, v)` | 8, 8, 8, 6 |

Hawai's population steps are (dummy 1), 500, 2000, 8000. Kegon's are (dummy), (dummy), 200, 1000.

### 2.3 WATR (id 128, tag `0x57417472`). Binary.

Read in the `T2WorldDef` ctor right after WORLDDEF.

| Field | Tokyo | Hawai | Kegon | Meaning | Conf |
|---|---|---|---|---|---|
| 6 ints | 230,129,128,100,10,354 | 230,129,128,45,6,400 | 230,129,128,2,32,36 | **Discarded by T2WorldDef**, and no other reader was found. [3] equals the floors above ground (100/45/2). [4] equals the basement floors (10/6/32). | LOW |
| initialFunds | 100000 | 50000 | 20000 | `GetInitialFunds` +0x24c | HIGH |
| exchangeMul | 100 | 100 | 100 | `ExchangeMoney(x) = x/exchangeDiv*exchangeMul` 0x1000ab50 | HIGH |
| exchangeDiv | 1 | 1 | 1 | | HIGH |
| moneyUnit (pstr) | "$" | "$" | "$" | `GetMoneyUnit` +0x258 | HIGH |

So the internal money is ×100 when displayed: Tokyo starts with an internal 100000.

### 2.4 BINF: background tile map (Tokyo, Hawai). Text.

Consumer: `T2WorldPlugin::InitBkgndInfoProc` **0x10073150**. Storage:
`UT2BkgndInfo::SetupBkgndInfo` 0x10073e10. Lookup: `GetBkgndInfo` 0x10073f70.

```
rows            (= world height / 2)     Tokyo 61, Hawai 31
cols            (= world width / 8)      Tokyo 46, Hawai 50
unused          (read, never used)       0
rows*cols values, COLUMN-MAJOR: for col in 0..cols-1: for row in 0..rows-1
```
Each cell is 8 units × 2 rows. `value & 0xffff` indexes a 'Sky' tile in RT_BITMAP 1000.
OBJMAP 1000 declares those tiles as `DefParts Sky` patterns 0..N, each `Rect 0,k*tileH,tileW,(k+1)*tileH`.
The earlier reading ("[61,46] then 61×46 grid") was right about the header. The cell
order is column-major, and the third header int is unused.

**Rendered (see §2.13):** Tokyo shows the Shinjuku skyline (Metropolitan
Government Building) over a palette-animated sky. Its 6 bottom tile rows (values 100–105)
are the 12 underground rows. Hawai shows Waikiki and Diamond Head, with the sea and a pier at the
far left and 7 dirt rows. **BINF is purely decorative and does not constrain building.**

### 2.5 FINF: per-unit build mask (Kegon). Text.

Consumer: `T2WorldPlugin::InitFloorInfoProc` **0x100730d0**, which calls
`T2FloorInfo::InitMask` **0x10033d40**, which calls `T2UnitInfo::InitMask` 0x1006c940.

```
height, width, unk(=1), RECT l,t,r,b (=2,2,2,2)   -- 7 ints, all read and discarded
height*width values, ROW-MAJOR (index = row*width + col)
```
A non-zero value sets unit flag `0x8000`. `T2UnitInfo::IsBuildable` 0x1006c860 returns
`!(flag & 0x8000)`, and `T2FloorInfo::IsAreaBuildable` 0x10034000 requires every
unit in the rect to be buildable (and the rect to be inside buildArea). The header is
`[44,192]` plus 5 more ints, which explains the earlier "4 trailing values" puzzle.
The `2`s seen at the top of the old dump are the RECT header. The grid itself has
5849 cells set to 1 and 2599 cells set to 0.

**Rendered:** the 0 (buildable) region matches the **dark rock cliff** on the
right of the Kegon falls picture: units 131–184 in rows 0–11, widening to
120–185 by row 36 and to 81–185 in row 41. Rows 42–43 are fully blocked. Everything else (sky, the waterfall gorge
on the left, the trees) is blocked. Together with groundLine=10, topFloorLine=8 and
bottomFloorLine=42, Kegon is a **cliff-side dig**: 2 floors above the cliff top,
**32 basement floors** carved into the rock. Worlds without FINF leave every unit
buildable, limited only by buildArea and the floor lines.

### 2.6 PTDF: palette animation def (text, `(name)\tvalue` lines)

Consumer: `T2PaletteAnimeDef::T2PaletteAnimeDef` 0x10047e60, read in order.

| Field | Tokyo | Hawai | Kegon |
|---|---|---|---|
| mNumOfTowerColor | 238 | 256 | 256 |
| mNumOfAnimColor | 48 | 48 | 144 |
| mAnimStartIndex | 189 | 189 | 93 |
| mNumOfRefrectColor | 0 | 33 | 131 |
| mNumOfSkyColor | 32 | 6 | 1 |
| mNumOfSEKind | 6 | 6 | 6 |
| mNumOfSEColor | 16 | 9 | 12 |
| mSEInterval | 5 | 5 | 5 |

### 2.7 PALETTE (ids 100–103; Kegon also 150–154)

Loader: `LoadACTPaletteEntries` 0x100120e0. There are 3 formats:

* `size == 0x300`: raw ACT (768 bytes of RGB).
* first u16 == 0: Mac **`clut`**: `u32 seed, u16 flags, u16 ctSize`, then 8-byte entries
  `{u16 index, u16 r, u16 g, u16 b}` (big-endian; the high byte is used). The engine loads
  only `ctSize` entries, so the last of 256 is skipped. Id 100 is the base 256-colour
  palette ("tower palette").
* otherwise Mac **`pltt`**: `u16 count`, 14 reserved bytes, then 16-byte entries `{r,g,b, usage,
  tolerance, flags, u32 private}`. Id **101** is the daylight palette: sky colours, then
  refraction colours. Id **102** is the SE palette (`mNumOfSEKind × mNumOfSEColor` colours
  cycled every `mSEInterval`). Id **103** holds the "common colours" that are matched against id 100 to
  build `gCommonColor`.

Animation (`T2PaletteAnime::ChangeSkyColor` 0x10047bd0 / `ChangeSE` 0x10047a40 /
`DoAnimatePalette` 0x10047d80): palette index `mAnimStartIndex + k` takes daylight entry
`k`, filtered through `T2DayLightFilter::SkyColor` (sky entries) or `RefractionColor` (refraction entries),
followed by the current SE kind. Tokyo's daylight entries are flat grey, so its sky colour
comes **entirely from `T2DayLightFilter`**, which is x87 maths the decompiler drops:
`SetTime` 0x1001cd00, `CalcDayLightColor` 0x1001cec0, `SkyColor` 0x1001d110.
Hawai and Kegon carry real base colours. The extractor fills the animated range with these
unfiltered colours, which renders Kegon and Hawaii correctly in daylight.

### 2.8 People pool, seasons and colds

| Res | Consumer | Layout | Notes |
|---|---|---|---|
| PODF id 1 | `T2WorldDef::MakePoolDef` 0x1006eb10, which calls FUN_10050e00 | `int unk; int gradePoolIDs[5]; int unk(uchar); int n(uchar); n × {int, int(uchar)}` | `gradePoolIDs[g-1]` names the PGDF loaded by `T2Pool::GameLevelChanged` 0x10050390 when grade g is reached. Tokyo/Hawai use 128..132; Kegon uses 0,0,128,129,130. HIGH for the ids, LOW for the rest. |
| PGDF 128+ | FUN_10050c70 (`T2PoolGradeDef`) | `count` + count × FUN_10050be0 records | Per-grade population demand. The engine-rules agent's area. |
| PZDF 1000+ | `T2PeopleTimeZoneList::Add` 0x1004dfd0, which calls FUN_10050ad0 | `count` + count × 4 "uchar" ints (the 4th × 0.2 is stored) | Hourly time-zone demand. |
| PPDF / SLDF id 1 | `T2PeopleLoader::LoadPeople` 0x1004d600 / `LoadSilhouette` 0x1004d4d0 | | People types and sprites. Shared with the tenant agent. |
| CODF id 1 | `MakeColdTableDef` 0x1006ec50, which calls FUN_1001c400 | `uint unk(=3); uint monthly[12]` (20,30×9,20,20) | `GetColdDenomi` (catching colds by month). HIGH for the layout. |
| SPDF | `MakeSeasonParamDef` 0x1006eba0 opens **id 1** | `int monthlyPercent[12]`. Each is also stored as a float × const. | Kegon ships SPDF as **id 128** (30,20,30,40,50,60,70,80,90,100,100,30: an autumn-leaves peak), so `FindResource` fails and **Kegon's season table is never loaded**. Kegon's plugin code does not open it either. MED (data bug in the shipped game). |
| WODF id 1 | `T2WordDefArray` (ctor at 0x111223 area) | Speech-bubble words | See resource-formats.md §3.6. |
| TMDF id 1 | Hawai, Kegon only | 47 bytes | Same layout as a tenant TMDF. |

### 2.9 T2EV and the event resources (`*EV`)

`T2TowerEvent::T2TowerEvent` **0x10068ca0** reads T2EV (id 1): `count`, then
`count × {u32 typeTag, int resID}`. It opens resource `(resID, typeTag)` and builds
the event class (`T2TowerEvent::AppendStandby`). Tag bytes in the file are reversed: `vEOB` is the tag "BOEv", which is resource BOEV.
Unknown tags go to the world plugin's virtual `MakeXEvent` (vtable +0x60).

Common header (`T2EventItem::T2EventItem` 0x10030050, HIGH):

| Int | Field | Accessor |
|---|---|---|
| 0 | levelBit | `GetLevelBit`: bitmask of grades (bit g−1) on which the event may run |
| 1 | exclusive | `IsExclusive` (stored as bool) |
| 2 | cycleDay | `IsBeginDay(d)`: `d % cycleDay == 0` (min 1) |
| 3 | beginTime | Minute of day (0–1439). `IsBeginTime`. |
| 4 | endTime | `GetEndTime` |

| Res | Class (ctor) | Worlds | Body after header |
|---|---|---|---|
| FBEV | T2FireBurning 0x10030c10 | T/H | `int; n; n ints` (Tokyo: 3000; 8100–8103 = FIRE/HELI bitmaps) |
| BOEV | T2BlackOut 0x1001b580 | T/H | 3 ints (5000 ×3) |
| TREV | T2Terrorist 0x100638e0 | T/H | 3 ints (3000, 720, 5000). levelBit 6 = grades 2–3 (Tokyo) or 4 = grade 3 (Hawai). |
| SAEV | T2SantaClaus 0x100560b0 | T/H | 1 int (32; 0 becomes 32). Flies at topFloorLine across the world width. |
| ODEV | T2OutDecoration 0x10045760 | T/H | `n; n × {imageID, uint}`: outdoor decorations 9101–9109 |
| VIEV | T2VisitVIP 0x1006cb40 + `Init` 0x1006d360 | all | `n; n × {grade, tenantType 4cc, int, int, int, int, viplID, pstr comment, pstr comment2}`. levelBit becomes `1<<(grade-1)` and the VIP flag is the next free bit in `0x1<<(4·(grade−1))`. These are the 1/16/256/4096 flags the grade table checks. |
| VIPL 220–224 | read by VIP / Reggae | all | 6 ints; only the first (grade 1–5) is read here. |
| MREV | Metro railway, FUN_1003d690 | Tokyo | `int(5000); n(12); n × {minute, track}`: the subway timetable (410/1, 490/0 …). Bitmap 8000 holds MET1–MET4. |
| MAEV 128/129 | T2Maru_Reggae 0x1003a770 | Tokyo | `int, viplID, int, int, pstr` ("Uncle Reggae is noisy."). The noisy-neighbour event. |
| BBEV | T2Baseball (Tokyo.t2p 0x100118d0) | Tokyo | 6 shorts and 2 ints (12005–12007 dialog ids, 1024, 11001, 10001, 50, 300). Needs a Stadium (`IsValidStadium`); fireworks. |
| CLEV | T2Cloud (Tokyo.t2p 0x10012bd0) | T/H | `n; n cloud bitmap ids (9001–9004); m; m × RECT` spawn zones in units |
| CREV | T2Cruise (Hawai.t2p 0x10012ac0) | Hawai | Ints (46,30,4, then sailings at 420/660/900/1140). Ship bitmap 8800. |
| DOEV | T2Dolphin (Hawai.t2p 0x10013e10) | Hawai | Animation script (ints) |
| WDEV | T2Wedding (Hawai.t2p 0x10014ed0) | Hawai | Header only. Text in STRTBL; needs a Cathedral. |
| TOEV | T2Tonbi (Kegon.t2p 0x10013030) | Kegon | `int; n points (y,x); n anims` (black kite, bitmaps 8600+) |
| HSEV | T2HotSpring (Kegon.t2p 0x10012940) | Kegon | `int(100), int(8), tenantType 'IPnT', 'IKAT'` |

STRTBL (Hawai id 1): `count` + pstrs (the wedding announcement text). STRTABLE
(8100/8300) and T2DLG are text resources, documented elsewhere.

### 2.10 Bitmaps and OBJMAPs (where the YootTower base art lives)

These are per world. Ids are shared across worlds; T2DLL `T2WorldDef::SetupOffscreen` 0x1006fb50 loads the starred ones.

| Id | Size (Tokyo) | OBJMAP names | Use |
|---|---|---|---|
| **1000*** | 64×8064 (112 tiles); Hawai 32×13896; Kegon 768×792 single picture | `Sky` | **The sky and ground backdrop**, drawn via BINF or as one picture. Palette-animated sky. |
| 1001 | (OBJMAP only) | `Back` | |
| **1002*** | 24×216 | `SideL`, `SideR` | Tower side walls |
| **1003*** | 56×216 | `TentL`, `TentR` | Side awnings / option objects (`CalcOptionObj`) |
| **1004*** | 328×84 | `Floor`, `Roof`, `BuildPrc` | Floor and roof slabs (`DrawRoofAndFloor` 0x1006f020) and the under-construction pattern |
| **1005*** | 16×144 | `BuildOpt` | |
| **1006*** | 24×72 | `Mark` | Map marks |
| **1007*** | 640×36 | `Rubble` | Demolition rubble |
| 1207–1210 | 144×1248 | `ANIM` | Grade-up animations (§2.2.1) |
| **2000*** | 182×78 | `Tool*` | Tool cursors |
| **3000*** | 40×72 | `OutCrane` | Rooftop crane |
| 3011–3015 | 68×320 | `ANIM` | Grade star / "Tower" icons |
| 5000/5001/5003 | | FIRE, HELI/WATER, SANTA | Event sprites |
| 7000/7100 | | tabs/buttons | UI |
| 8000 (Tokyo) | 320×128 | MET1–4 | Metro trains |
| 8200–8209, 8300, 8400 | | ANIM | VIP / event animations |
| 9101–9109 | | | Outdoor decorations (ODEV) |
| **10000–10003*** | 558×90 … | Calendar/Clock/Funds/Grade/Message/Population; CtrlPal/buttons/slider; DLGITEM:*; SETTLEMENT | **Control-panel UI chrome** |
| 20000/20001 | 200×200, 60×60 | | World picture and icon (selector) |
| 282/283 | 160×22 | SILH7 | |
| 300–355 | 16×216 | | Lobby / entrance side strips |

**The lobby is not in the world plugin.** It is `Floor.t2p` RT_BITMAP 1000 (368×204,
OBJMAP `View` patterns 100/10100 for a normal floor, 1100/1200/1300 for lobby heights 1/2/3:
a 112 px fixed left part plus a looped 256 px section; `LCsr11..13` are the cursors), with
`OutView` 1001–1003 (64×540) as the exterior facade per lobby height. KFloor.t2p is the Kegon variant.

WAVE ids 142–154, 280–281 and 338–409 are world ambience. The plugin subclass registers them
by name: Tokyo FUN_10013fc0 registers BuildFloor/Build/Destroy/GradeUp/World Birds/Chime/Crow/Thunder/Cock/Rain.

### 2.11 ATTR / DESC of the worlds

ATTR (CFilePlugin ctor 0x1000df20) is `mID, mAttrib, mDependID, mGameLevel, mSecretItem,
mItemType, mMustItem, name`:
Tokyo `KJNS,7,****,6,0,0,1,"Tokyo"`; Hawai `IAWH,0xf01,****,6,0,20,1,"Hawaii"`;
Kegon `NOGK,0xf0,****,3,1,10,1,"The Kegon Falls"` (mSecretItem=1 marks it as an unlockable world).
DESC = `lPdW`, 3.

### 2.12 What the grids encode (summary)

* **BINF** is decoration only. It is a tile map of the backdrop at 8 units × 2 rows per cell. It does not mark terrain, floor limits or underground limits.
* **FINF** is the real terrain mask: per-unit build/no-build. It exists only where the
  terrain is irregular (Kegon). Its absence means "everything inside buildArea".
* The **limits** come from WORLDDEF: buildArea (hard clamp), topFloorLine and
  bottomFloorLine (floor rows), groundLine and lobbyHeight.
* Kinsoku (neighbour compatibility) and BlockT frame boundaries are **not** in world plugins.
  No world resource or `T2WorldDef` code references them.

### 2.13 Rendered observations (`extract_world_sp.py` writes `background.png` and `buildmask.png`)

* Tokyo: 2944×4392 px. Two Shinjuku skyscraper clusters, ground at row 110, dirt below.
  Build area is units 36–332 and rows 6–120.
* Hawai: Diamond Head ridge with palm-lined hotels. The sea, a pier and coral occupy the left 56 units,
  which are outside the build area (left = 56).
* Kegon: half-scale 768×792 picture scaled ×2. FINF follows the rock edge exactly.

---

## 3. SP-format plugins (51 files)

| DESC | Class | Files |
|---|---|---|
| `lPoM` | T2MoviePlugin | 38 `MV_*.t2p` |
| `lPeH` | T2HallEventPlugin | 6 `Pl*.t2p` (Art, CarShow, Concert, Fashon, Plant, Tennis) |
| `lPdA` | T2AdvertisePlugin | 7: AirJamaica, Aquazone, GAMEWORKSAD, Ob9003, SandalBeachResort, Segapc, Yoot |

`Ticket.t2p` is a PE plugin, not SP. `Plugins/Database` (magic `9c 00`) is the plugin
loader's cache.

### 3.1 Container (HIGH: `OpenSubPluginFS` 0x10012570, `CResFile::OpenResource(char*)` 0x10010fd0)

```
"SP"            2 bytes
u32 count
count x { u32 type; u32 id; u32 offset; u32 size }   -- SUBPLUGINFSENTRY, 16 bytes
payloads (contiguous, in directory order, no padding, no compression)
```
`type` is the CResFile tag constant stored LE, so the bytes read reversed: `FDOM` is type
`0x4d4f4446` "MODF". Lookup is a linear match on (type, id). **type 0 is a DIB**
(BITMAPINFOHEADER, colour table and bits, with no BITMAPFILEHEADER), loaded by
`T2BitImage(char *path, id, …)`. SP tags are ALL-CAPS (`MODF`, `HEDF`, `ADDF`,
`TMDF`, `DPDF`, `PPDF`, `WODF`), unlike the mixed-case PE tags. `write_sp()` rebuilds
all 51 files byte-identically.

Entries: every file has ATTR 128 and DESC 128. Movies add MODF, TMDF, WODF and DIBs 128/129. Hall events
add HEDF, TMDF, DPDF, PPDF, WODF and DIBs 128/129. Ads add ADDF, DIB 128 and (Ob9003 only) DIB 900.

### 3.2 Sub-plugin header (`T2SubPlugin::LoadRsrcFromStream` 0x1005d110)

| Field | Meaning |
|---|---|
| title (pstr) | +0x70 `GetTitle`. Equals ATTR name. |
| tieupDate (uint) | BCD `0xYYYYMMDD` (the day is stored minus one) converted to a `CTime`. Non-zero marks a real-world tie-up that expires (`IsTieupFinish`). All shipped files use 0. |

### 3.3 MODF: movie (`T2MoviePlugin::LoadRsrcFromStream` 0x100430e0)

| Field | Values seen | Meaning | Conf |
|---|---|---|---|
| imageID (ushort) | 128 | Poster DIB (`UnfoldPluginSelf` 0x100433b0) | HIGH |
| filmKind | 0 ×31, 2 ×7 | 0 = roadshow (`IsRoadshowFilm`), 2 = famous (`IsFamousFilm`) | HIGH |
| phase1..3Months | 0–24 / 6,10 / 0–18 | Life-cycle lengths, made cumulative. `GetMovieLifePhase` 0x10043320 computes `(month−release−12) % cycle` and returns phase 0/1/2/4. | MED |
| phase4Months, cycleMonths | 3, 1 | Read, then **zeroed** by the loader | HIGH |
| production, director, starring (pstr) | "OPeNBooK / Published by : OB9003", … | Credits | HIGH |
| runningTime | 120 | Minutes | MED |
| unkFC | 200–500 | Probably price or popularity | LOW |
| description (pstr) | "A science fiction film" | Genre blurb | HIGH |
| customerTableID (ushort) | 128 | +0xb4 `DupCustomerTableDefRes` points to TMDF 128 (audience member table) | HIGH |
| unk104, unk108 | 95, 101 | | LOW |
| famousAudience | 30–300 | `ReMake` 0x100434c0: audience ≥ this marks the film famous (flag 2) | HIGH |
| remakeAudience | 30–300 | Audience ≥ this lets the film re-run (count+1), otherwise it retires (flag 0x8000, phase 4) | HIGH |
| unk114 | 1 (one 0) | | LOW |
| numExtra + extra[] (pstr) | 0 | Extra strings | HIGH (layout) |
| stillImageID | 129 | Second DIB (208×117 still) | MED |
| clipSeconds | 50–140 | Length of the trailer clip | LOW |
| movieFile (pstr) | `MV_1stContact.MOV` | **The link to `Movie/*.MOV`** (+0xe4). Every file exists (validated). | HIGH |

### 3.4 HEDF: hall event (`T2HallEventPlugin::LoadRsrcFromStream` 0x100380e0)

| Field | Art / Concert / Tennis | Meaning | Conf |
|---|---|---|---|
| imageID (ushort) | 128 | 248×240 DIB split into 4 stacked frames (parts 200–203, `UnfoldPluginSelf` 0x100381e0) | HIGH |
| unkCC | 2 / 1 / 2 | | LOW |
| bookingDays (`unkD0`) | 3 / 1 / 3 | EventCenter FUN copies it into the booking entry (+0x10). Probably days per booking. | MED |
| openTime | 600 / 1080 / 600 | `GetOpenTime`, minute of day | HIGH |
| closeTime | 1020 / 1260 / 1020 | `GetCloseTime` | HIGH |
| monthMask (`unkD4`) | 0xF1F / 0xFFF / 0x1F8 | 12-bit month mask (bit0 = Jan): Tennis Apr–Sep, Plant Apr–Oct, Car show Jul–Dec, Fashion Mar–May & Sep–Nov | MED (data pattern) |
| cost (`unkD8`) | 250 / 250 / 400 | At close, EventCenter settles `funds += tenantValue − cost` | MED |
| customerTableID (ushort) | 128 | TMDF id | HIGH |
| unkDC | 1000 / 100 | Copied into the booking entry (+4). Probably a price. | LOW |
| unkE0, unkE4 | 50,10 / 5,10 | | LOW |
| bindTime | 60 / 180 | `GetBindTime` 0x10004030: minutes visitors stay | HIGH (accessor) |
| stillImageID (`unkF4`) | 129 | Second DIB | MED |
| description (pstr) | "Modern art exhibition.\r\n…" | | HIGH |
| movieFile (pstr) | `HE_Art.MOV` | **Link to `Movie/HE_*.MOV`** | HIGH |

### 3.5 ADDF: advertisement (`T2AdvertisePlugin::LoadRsrcFromStream` 0x10015bd0)

| Field | Values | Meaning | Conf |
|---|---|---|---|
| subType (4cc) | `IPoB` (all 7) | `GetSubType`. Must equal the hosting tool def's `T2ToolDef+0x108` (`T2ToolDef::Add(T2AdvertisePlugin*)` 0x10065cc0, `T2AdvertiseTable::Add` 0x10015f30), i.e. the Billboard medium. | HIGH |
| imageID (ushort) | 128 | Ad picture DIB (150×50 ×8bpp; Yoot 160×60) | HIGH |
| unkD0 | 1 | | LOW |
| days | 12 | `GetDay`: campaign length | HIGH (accessor) |
| money | 500 | `GetMoney`: cost | HIGH (accessor) |
| path (pstr) | "" | `GetPath` | HIGH |

### 3.6 Other SP payloads

TMDF (customer/member table), WODF (speech words), DPDF (placement params, 4
ints) and PPDF (6 ints) use the same layouts as in PE tenant plugins. They are kept raw by
`world_sp.py` and parsed by the tenant agent's tools (`resource-formats.md` §10).
ATTR in SP files: `mID` is a per-file 4cc (`OPSM`, `strA`, `9BOB` …), `mAttrib` 1,
`mGameLevel` 1.

---

## 4. T2.dat, t2s.dat, T2Icons.dll

| File | Format | Consumer | Content |
|---|---|---|---|
| T2.dat | Plain BMP, 640×480 8bpp, 256-colour table, uncompressed (308278 bytes) | T2.exe FUN_004162e0, the opening frame window `T2OpenningWindow` (string at 0x44970c). DIB loader FUN_0040b9d0. | **Title/splash screen** ("YOOT TOWER" logo over a stadium tower) |
| t2s.dat | Plain BMP, 460×320 8bpp | T2.exe FUN_00416ad0, a start-up `T2Dialog` (string `T2S.dat` at 0x449714) | Start dialog backdrop: art on the left 240 px, flat grey panel on the right for the controls |
| T2Icons.dll | PE DLL: 9 RT_GROUP_ICON (32×32 and 16×16, 16 colours), dialog 132, exports `DllGetClassObject` / `DllCanUnloadNow` | T2.exe FUN_0040d3e0 registers it as COM in-proc server "Tower2 Plugin Helper" (`CLSID {F6FC9820-57D7-11d2-A2FD-…}`) | **Explorer shell extension.** It provides per-kind `.t2p` icons for lPnT, lPvM, lPoO, lPdW, lPeH, lPdA, lPoM, LPOT and LPPO, and a property page (category / plugin name / plugin type / grade / price). **Not game art.** |

`t2dat.py` round-trips both BMPs byte-identically and rebuilds the icons as `.ico` files.
None of the three files holds sky, ground or lobby art.

---

## 5. Where OpenSky's YootTower skin finds sky, ground and lobby art

* **Sky and ground:** each world plugin's RT_BITMAP **1000** (OBJMAP `Sky`), placed by
  **BINF** (Tokyo, Hawai) or drawn whole (Kegon, 2× scale). Sky pixels use the animated
  palette range (`mAnimStartIndex..+mNumOfAnimColor`). Fill it from PALETTE 101/102 for daylight
  (Hawai, Kegon), or port `T2DayLightFilter` for time-of-day and for Tokyo.
* **Floor and roof slabs, tower sides, crane, rubble:** world bitmaps 1004, 1002/1003, 3000 and 1007.
* **Lobby:** `Floor.t2p` (and `KFloor.t2p`) RT_BITMAP 1000, View patterns 1100/1200/1300,
  plus exterior `OutView` 1001–1003.
* **UI chrome:** world bitmaps 10000–10003 (clock, calendar, funds, grade, control palette,
  dialog items) and 7000/7100 (tabs). T2.dat (splash) and t2s.dat (start dialog) come from the install dir.

---

## 6. Editor field list (OpenSky Plugin Editor)

**World (lPdW)**: ATTR (name, mID, mAttrib, mSecretItem, sortKey/mItemType). WORLDDEF:
height, width, groundLine, buildArea, topFloorLine, bottomFloorLine, lobbyHeight,
startGrade, outViewKind, tiledBkgnd, bkgndTileWidth/Height, grades[] (vipFlag,
population, gradeUpAnimID, frames, param), gradeIconAnimIDs[5], towerNamePrefix,
dustOnAt, toiletAt/toiletDenomi, cashAmounts[≤8]. Keep `entrance`, unk74, unk17c,
unk188/18c and the discarded ints as pass-through. WATR: initialFunds, exchangeMul/Div,
moneyUnit (keep 6 unk ints). BINF: grid editor (tile picker over bitmap 1000).
FINF: paint-mask editor. PTDF: 8 palette counts. PALETTE 100–103: colour tables.
PODF: gradePoolIDs[5]. CODF/SPDF: 12 monthly values (warn when SPDF is not id 1).
T2EV: event list (type, resID). Per event: header (levelBit as grade checkboxes,
exclusive, cycleDay, begin/end time) plus the typed body (§2.9). Raw-int bodies stay pass-through.
Bitmaps/OBJMAP/WAVE: asset slots by id (§2.10).

**Movie (lPoM)**: title, tieupDate, filmKind, phase1–3Months, production, director,
starring, runningTime, description, famousAudience, remakeAudience, clipSeconds,
movieFile (picker over Movie/), poster (DIB 128), still (DIB 129), customer table
(TMDF 128). Keep unkFC/104/108/114 and phase4/cycle as pass-through.

**HallEvent (lPeH)**: title, openTime, closeTime, monthMask (12 checkboxes),
bookingDays, cost, bindTime, description, movieFile, 4-frame image (DIB 128), still
(DIB 129), TMDF/DPDF/PPDF tables. Keep unkCC/DC/E0/E4 as pass-through.

**Advertise / "Object" (lPdA: Ob*, AirJamaica …)**: title, subType (fixed `IPoB`
= billboard), image (DIB 128, 150×50), days, money, path.

---

## 7. What OpenSky's YootTower game mode needs from worlds

1. **Bounds per world** come from WORLDDEF, not from BINF. Build only inside `buildArea`
   (Tokyo x∈[36,332), Hawai [56,392), Kegon [81,186)). Floors may use rows
   `[topFloorLine, bottomFloorLine)`: Tokyo **100 floors / 9 basements** (row 119 and B10
   are inside buildArea but outside the floor area; WATR says 10, probably the metro level),
   Hawaii **45 / 6**, Kegon **2 / 32**. Floor number = `ground−row` (above) or `ground−row−1` (below).
2. **Terrain:** apply FINF where present. A unit with a non-zero value blocks every
   construction covering it (Kegon's cliff). No FINF means no terrain mask.
3. **Lobby:** the lobby height per world is 3/2/1 rows (Tokyo/Hawai/Kegon). Elevators
   cannot stop on the upper lobby rows (`IsStoppableFloor`).
4. **Start state:** initial funds 100000/50000/20000 internal (×100 when displayed, `$`).
   Start grade 1/2/3 (MED).
5. **Grade progression:** upgrading from grade L needs `population ≥ grades[L-1].population`
   and the VIP flag of that grade. The VIP (VIEV entry for grade L, visiting a specific
   tenant type) arrives once population exceeds half the threshold, on day % 9 at 10:00 or later.
6. **Scenario events** (T2EV): fire, blackout, terrorist, Santa, VIP and outdoor decorations
   everywhere. Tokyo adds the metro timetable, baseball (needs a Stadium), noisy neighbour and clouds.
   Hawaii adds cruise ship, dolphins, wedding (needs a Cathedral) and clouds. Kegon adds the
   black kite and the hot spring (tenant types `IPnT`, `IKAT`). Use the event header for gating
   (levelBit / cycleDay / time window).
7. **Monthly modifiers:** CODF cold rates in all worlds. The Kegon season table exists but
   the shipped game never loads it (§2.8).
8. **Rendering:** backdrop (§5), palette-animated sky, Kegon's half-scale picture
   stretched ×2.

---

## 8. Remaining unknowns

* `T2DayLightFilter` maths (x87) for the time-of-day sky, which Tokyo needs.
* WORLDDEF `entrance` (+0x60), unk74, unk17c, outViewKind semantics, unk188/18c.
  WATR's 6 discarded ints (no reader found in T2DLL or T2.exe).
* MODF unkFC/104/108/114 and clipSeconds. HEDF unkCC/DC/E0/E4. ADDF unkD0.
* Bodies of MREV (T2Transport::Init), CREV, DOEV and TOEV are kept as raw ints. PODF demand pairs.
* startGrade is inferred from data, because T2.exe's new-game initialisation of `doc+0x8c` was not located.
