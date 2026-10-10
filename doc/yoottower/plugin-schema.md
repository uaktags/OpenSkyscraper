# YootTower `.t2p` Plugin Resource Schema (byte-exact, population-validated)

**Status:** every resource in all **105 PE-format plugins** decodes and re-encodes
**byte-identically** (2983/2983 resources, 1283 type×plugin pairs). 2969 resources are decoded
completely, 7 are CResFile *text-form* resources (parsed values plus raw text kept), and
7 world-specific event resources keep a raw tail that only the world plugin's own code reads.
The 51 SP-format plugins (`MV_*`, `Pl*`, `Ob*`) and the World semantics (BINF/FINF/WORLDDEF/
events) belong to the world/SP agent. They are inventoried and round-tripped here, but not
named in depth.

**Reference implementation:** `tools/yoottower/t2p_schema.py` (decoder, encoder, and the
schema tables. Each field there carries its T2DLL address). Population gate:
`python3 -I tools/yoottower/validate_population.py`, which exits non-zero on any
failure. Code classifier: `python3 -I tools/yoottower/plugin_code_audit.py data/Plugins
<T2DLL.dll>`.

**Method:** the schemas were read from the T2DLL.dll constructors that consume each resource,
using Ghidra 12.1.3 headless with a full decompile of all 6004 functions. Field names come from
T2DLL accessor symbols, T2.exe call sites, and the legacy text templates (`TENANTDEF`,
`PTDF`, text `ATTR`). The tables were then checked against all 105 plugins.
"Addr" = T2DLL.dll VA unless prefixed `exe:`.

Confidence: **H** = the constructor read order plus a named accessor or consumer proves it.
**M** = the read order is proven but the name or semantics are inferred from values or legacy
templates. **L** = no consumer was found, so the field is named by position only.

---

## 0. Corrections to earlier docs (read first)

| Earlier claim | Correct (proof) |
|---|---|
| ATTR offset 1 = `mItemType` 4cc, offset 21 = `mID` (resource-formats §3.1, itemtype-catalog) | Offset 1 is **`mID`**, the plugin's unique 4cc (`BURG`, `FLOR`, `StdE`). Offset 21 is **`mItemType`**, the numeric item type (Floor 1000, Office 1200, Simple Elevator 1810) and equal to the ToolDef `mToolType`. `CFilePlugin::CFilePlugin` 0x1000df20 reads mID→+0x48 (`GetID`) first and mItemType→+0x5c (`GetItemType`) sixth. This agrees with the .bld agent: saves map 4cc ID → numeric type via a plugin table. |
| ATTR offset 9 "marker ****/NORF" | **`mDependID`**: the 4cc of a required plugin. `FRON` (bytes "NORF") means the item needs a hotel Front; `****` means none. |
| `SLDF` = tenant member table (§3.7) | `TMDF` (= engine tag `TmDf` 0x546d4466) is the member table. id 1 = employees and id 2 = customers (`MakeCustomerTableDef(2)`). `SLDF` is the **silhouette list** (`T2PeopleLoader::LoadSilhouette` 0x1004d4d0). |
| `PLDF` = price list | `PLDF` = **T2PlaceParamDef** (3×{percent, score}) 0x1004ed80. Prices live in the TNDF/MVDF variation records. |
| `DPDF` = placement w/h/baseX/baseY, or EPlace table | `DPDF` = **T2DayParamDef** (2×2 percent table [EDay][EWhether]) 0x1001d240. 0x100045d0 is only the *copy* constructor. |
| `PADF` = placement/palette animation | `PADF` = **T2PeopleAnimeDef** 0x1004ad60. |
| `TNDF`/`SLDF` nested inside FDnT | `TNDF` *is* the FDnT stream (`OpenResource(...,"TnDf")`, case-insensitive). `TMDF` and `PADF` are separate resources that the TenantDef constructor opens. |
| ATTR `mAttrib` "packed bitfield" | It is the **world-availability mask**. exe:FUN_0041ca80 loads a plugin only if `world.mAttrib & plugin.mAttrib != 0`, `mGameLevel <= tower level`, and the secret item is enabled (see §2.1). |
| SRect convention (§9.6 "LE left,top,right,bottom") | Plugin resources: `CResFile>>RECT` 0x10011730 reads **4 × LE int32 slots in order top,left,bottom,right**. Saves: `T2Archive::ReadSRect` 0x10017360 reads 8 bytes as **big-endian int16 top,left,bottom,right**. The order is the same Mac order; only width and endianness differ. `POINT` = y,x; `SIZE` = cy,cx. |
| PTDF/BINF/FINF binary | All three are **text-form** CResFile resources (decimal tokens). |

## 1. Encoding rules (CResFile, T2DLL 0x10010ca0–0x10011990)

* **Mode:** a first byte of `0x01` selects binary mode and is consumed (`SetupResource`
  0x10011230). Any other first byte selects text mode. Text mode splits tokens on
  space/tab/CR/LF/comma, skips `( … )` comments (so `(mID) 1000` is just `1000`), and reads
  strings to end of line (`Token` 0x10011290, `operator>>(char*)` 0x10011830).
* **Integers:** `operator>>` for uchar/short/ushort/int/uint/long/ulong always consumes **one
  4-byte LE slot** (0x10011310–0x10011650). For uchar/short only the low byte(s) are used. The
  schema preserves the unused high bytes as `<field>__hi`. In practice only PLDF negative
  scores have them, and those are sign-extension 0xFFFF bytes.
* **Strings:** length byte *n*, then *n* bytes, then a terminator byte. The total is n+2 bytes
  (the loop copies n+1). Strings are cp1252 and at most 255 bytes. Fixed destination buffers
  impose tighter limits: member names hold 16 bytes (≤15 chars) and silhouette names follow
  the struct layout.
* **Resource lookup:** `OpenResource(hInst, id, int type)` 0x10010cf0 byte-swaps the u32 tag
  into a string. For example, 0x546d4466 becomes "TmDf", which Windows matches
  case-insensitively against `TMDF`. Default ids: ATTR/DESC 128 and Def streams 1. Ids for
  DPDF/PLDF come from TMDF, ids for VIPL from events, and ids for PZDF/PGDF from the pool
  tables.
* **4cc display:** the schema shows the Mac source-order constant. File bytes `GRUB` display as
  `BURG`, and DESC `lPnT` displays as `TnPl`.

## 2. Per-resource field tables

Ranges below are observed values across the population. `_skipN` fields are read and thrown
away by the engine. They must be kept for byte-exactness, but the editor should hide them and
reuse the observed default.

### 2.1 `ATTR` id 128: plugin identity (`CFilePlugin` ctor 0x1000df20, `T2PluginSpecifier::Initialize` 0x1004fd00)

| # | Field | Type | Range / values | Meaning | Conf |
|---|---|---|---|---|---|
|0|mID|4cc|unique per plugin (`BURG`,`StdE`,`SNJK`…)|plugin ID, `GetID` 0x10003670. The save files reference it.|H|
|1|mAttrib|u32 mask|0x1,0x2,0x4,0x10,0x100, +bit24 on 3 plugins; worlds Tokyo 0x7, Hawai 0xF01, Kegon 0xF0; palette 0xFFFFFF|world-availability mask (exe:0x0041ca80 `world.mAttrib & attrib`)|H|
|2|mDependID|4cc|`****` (none) / `FRON`|required plugin (hotel rooms → Front)|M|
|3|mGameLevel|i32|1–6|star level needed (`spec+0x20 <= doc+0x8c`)|H|
|4|mSecretItem|i32|0/1 (Spa, Kegon)|hidden unless secret mode (`doc+0x9c`)|H|
|5|mItemType|i32|1000–2100 (world: 0/10/20)|numeric item type, == ToolDef mToolType (Billboard: 2100 vs 2000)|H|
|6|mMustItem|i32|0/1|always-available / mandatory item|M|
|7|mPluginName|str|—|display name|H|

The text form appears only in DefaultToolPalette. It has the same token order, and its
`(…)` labels are comments.

### 2.2 `DESC` id 128: plugin class (`T2PluginSpecifier::Initialize` 0x1004fd00)

|Field|Type|Values|Meaning|Conf|
|---|---|---|---|---|
|mType|4cc|`TnPl` 93, `MvPl` 7, `WdPl` 3, `OoPl` 1, `TOPL` 1|plugin class (= `ConstructProgramPlugin` literal, `GetRsrcType`)|H|
|mVersion|i32|3 (all)|`T2PluginLoader::GetNext` 0x1004fa70 skips anything ≠ 3|H|

### 2.3 `TNDF` / `MVDF` / `OODF` id 1: Tool → Equip → Tenant / Mover / OuterObj chain

The stream is consumed by `T2ToolDef` 0x10065540, then `T2EquipDef` 0x1002f240, then
`T2TenantDef` 0x100623c0 or `T2MoverDef` 0x1003ffb0. `T2OuterObjDef` 0x10046b90 reads only
the ToolDef part.

**ToolDef part** (all three):

|Field|Type|Range|Meaning (accessor / consumer)|Conf|
|---|---|---|---|---|
|mToolType|i32|1000–2002, 1800–1819, 2000|+0x50 `GetToolType`; the CalcSoundID prefix|H|
|mWidth|i32|1–56|+0x54 footprint width in units (`QueryCursor`, `CalcEquipRect`)|H|
|mHeight|i32|1–8|+0x58 footprint height in floors|H|
|mLevel|i32|1–4|+0x5c, equal to ATTR.mGameLevel in all 101 Defs. No reader was found.|M|
|mAttribute|u32 bits|see below|+0x60 `GetAttribute`/`IsSetAttribute`|H|
|mCategoryHelp|str|10 distinct|+0x6c category description|M|
|mCategoryName|str|"Shops","Restaurants","Transit Systems"…|+0x68 `GetCategoryName`|H|
|mCategory|i32|10–200|+0x64 `GetCategory`, the toolbox group. SortKey = cat<<16 \| toolNo.|H|
|mToolNo|i32|10–600|+0x70 `GetToolNo`, the order inside the group|H|
|_skip1,_skip2|i32|0/0 (movers 15–20/1)|discarded|H|
|mSubPluginType|4cc|0, `MoPI` (theaters), `HePI` (hall), `BoPI` (billboard)|+0x108 `GetSubPluginType`: which SP plugins attach (movies/hall events/ads)|H|
|_skip3,_skip4|i32|127–129 / 300|discarded (legacy Mac res ids)|H|
|mValiationCnt|i32|1 (90), 2 (3)|+0x74 number of variations (≤4)|H|
|variations[]|×cnt|||
| .mPriceString|str|"$200000","$150000:$50000/car"|+0x78[i] help "Cons. Cost" (`T2ToolHelpWnd::OpenToolHelp` 0x100669e0)|H|
| .mOutMoneyString|str|"$800/M","$12000/Q"|+0x88[i] help "Maint. Cost"|H|
| .mHelp|str|—|+0x98[i] help body|H|
| .mHelpTitle|str|—|+0xb8[i] help title|H|
| .mName|str|—|+0xc8[i] `GetName`/`GetToolName` (used in messages)|H|
| .mPrice|i32|5–50000|+0xd8[i] `CalcPayment`, construction cost in $100 units (Floor: per unit)|H|
| .mOutMoney|i32|0–1000|+0xa8[i] `GetOutMoney`, maintenance per period in $100 units|H|
| ._skip5–7|i32|128–130 / 300|discarded|H|
| .mConsumptionPower|i32|0–300|+0xf0[i] `GetConsumptionPower`|H|

`mAttribute` bits proven in code: 0x100 = pays maintenance (`CalcMentenanceCostProc`),
0x2000 = floor-type/stair (`IsFloor`, `IsStair`, `CalcPayment` per empty unit),
0x4000 = variable length (`IsValiableLength`), 0x8000 = home (`IsHome`), 0x20000 = concat,
0x800000 = people-anime (`IsPeopleAnime`), 0x2 = mover (`IsMover`), 0x80 = drag-making
(`IsAbleDragMaking`). The observed values are 26 combinations, e.g. 0x3000480 for shops and
0x1208680 for condos.

**EquipDef part** (TNDF, MVDF):

|Field|Type|Range|Meaning|Conf|
|---|---|---|---|---|
|mCapacity|i32|0–500|+0x11c `GetCapacity` (customers per unit; customer-unit maths)|H|
|mStress|i32|0, 10–30|+0x120. No reader was found. The name comes from the TENANTDEF template.|L|
|_skip8|i32|= mValiationCnt (movers 3)|discarded; keep equal to the variation count|H|
|equipVariations[]|×mValiationCnt|||
| .mOutSpan|i32|0, 131–200|+0x154 (the last one wins). TenantDef sets +0x194 = (value==0).|M|
| ._skip9, ._skip10|i32|0/140/150…|discarded|H|
|mInSpan|i32|0|+0x134, no reader|L|
|mBuildDays|i32|0|+0x138, no reader|L|
|mNumOfInMoney|i32|0,1,4|+0x13c `GetNumOfInMoney` (clamped to 4)|H|
|inMoney[].mInMoney|i32|0–20000|+0x144[i] `GetInMoney(i)`: income per price setting ($100). T2Tenant::GetPriceIndex selects the index.|H|

**TenantDef part** (TNDF):

|Field|Type|Range|Meaning|Conf|
|---|---|---|---|---|
|mRegistID|u32|0–10|+0x19c `GetRegistID`, the registry class: 1 hotel room, 2 trash, 3 parking, 4 doctor, 5 security/fire, 6 school, 7 restroom, 8 subway, 9 boat, 10 ticket|H (the values are named from the plugins)|
|mDownFlag|u32|0–4|+0x1a0 `GetDownFlag`|M|
|mNumOfKind|i32|1–6|+0x158 `GetNumOfKind` (sprite kinds)|H|
|mNumOfPattern|i32|1–14|+0x15c `GetNumOfPattern` (sprite patterns per kind)|H|
|mBelongCapacity|i32|0–60|+0x160 residents/staff|H|
|mSenseOfNoise|i32|0/100/150|+0x1a4 noise sensitivity (`UpdateResidencialEstimate`)|H|
|mEntranceV/H|i32|0–2 / 0–36|+0x164/+0x168 entrance cell|H|
|mExitV/H|i32|0–2 / 0–38|+0x16c/+0x170 exit cell|H|
|mOpenTime|i32|0–1080 min|+0x174 `T2Tenant::GetOpenTime`|H|
|mCloseTime|i32|0–1440 min|+0x178|H|
|mBindTime|i32|0–180 min|+0x17c `GetBindTime` (stay length); customer-unit divisor|H|
|mMaxBuildCount|i32|0 (∞) / 1|+0x104 `GetMaxBuildCount` ("No Limit" when 0)|H|
|_skip11–13|u32|0/128/129|discarded|H|
|mMovieName|str|"" / `[QT]:OF:` (Office)|+0x184 QuickTime movie tag|M|

The constructor also opens `TMDF` id 1 (employees, +0x18c), `TMDF` id 2 (customers, +0x190),
and `PADF` id 1 (+0x1a8).

**MoverDef part** (MVDF):

|Field|Type|Range|Meaning|Conf|
|---|---|---|---|---|
|mAddSize|rect|top −1/0, bottom 0/1|+0x158 `GetAddSize`, the shaft margin|H|
|mMaxLength|i32|0, 15, 50, 1000|+0x168 max floors (0 = unlimited/stairs)|H|
|mMinLength|i32|0/1|+0x16c|H|
|mMaxStop|i32|0/8|+0x170 `GetMaxStop` (express)|H|
|mScore|i32|5–50|+0x1a8 route score (`T2Mover::CalcScore`)|H|
|mDefaultWait|i32|3/4|+0x174|H|
|mSpeed[3]|i32|0–12|+0x178 `GetSpeed(i)`|H|
|mMaxModule|i32|0/6/8|+0x184 max cars|H|
|mModuleUnitH, mModuleUnitV, mModuleParam1, mModuleParam2|i32|1, 0, 0, 0–1500|+0x188–0x194: no reader in T2DLL, T2.exe, or any mover plugin. Param2 tracks the price tier.|L|
|mModuleInset|rect|±2|+0x198 `GetModuleInset`|H|
|_skip14–17|i32|6001/6002/0/0|discarded (legacy sound ids)|H|

**OuterObjDef** (OODF): the ToolDef part only. The constructor forces +0x74 = 1. Two trailing
int32 values (`_unreadA/B`, 0) are never read.

### 2.4 `TMDF` id 1/2: T2TenantMemberTableDef (0x100630a0)

|Field|Type|Range|Meaning|Conf|
|---|---|---|---|---|
|mEconoType|u32|−1 (none), 0–4, bit4 flag|+0x08 `GetEconoType`; bit4 = `IsCheckOnlyFirstEconoType`|H|
|mSpecialFlag|u32|0,1,2,4|+0x0c `GetSpecialFlag`|H|
|mDayParamID|i32|0 / 128|DPDF id to load (`MakeDayParamDef`)|H|
|mPlaceParamID|i32|0 / 128|PLDF id (`MakePlaceParamDef`). If absent, `GetScore(EPlace0)` = 1000.|H|
|mNumOfElem|i32|0–4|+0x20|H|
|members[] (0x30 B, `T2TenantMemberDef::Initialize` 0x10063000)||||
| .mName|str ≤15|—|+0x04|H|
| .mDemandType|i32|−1–4|`GetDemandType` (pool demand class)|H|
| .mStart/EndSilhouetteType|i32|−5…61|people-type range (PPDF mPeopleType ids)|M|
| .mOptionType|i32|−1,1,2,10|`GetOptionType`|H|
| .mPercent|u32|0–100|`GetPercent`. Rate = percent × customerUnits × k (float).|H|
| .mNumOfPeople|u32|0–60|`GetNumOfPeople`|H|

### 2.5 `DPDF`: T2DayParamDef (0x1001d240), `PLDF`: T2PlaceParamDef (0x1004ed80), `PADF`: T2PeopleAnimeDef (0x1004ad60)

* **DPDF:** `mPercent[EDay 0..1][EWhether 0..1]`, u32, values 0–100. Read order is
  d0w0, d0w1, d1w0, d1w1. Rate = v × 0.01. `GetPercent(EDay,EWhether)` 0x1000d280. (H. EDay is
  weekday/holiday and EWhether is fine/rain, both M.)
* **PLDF:** `places[EPlace 0..2]` = {mPercent u32 0–100, mScore short-in-slot −100…1000}.
  EPlace 0 = floor (used for floor-type tenants), 1 = tenant, 2 = pool. The consumers are
  `T2Tenant::CollectCustomerFrom{Floor,Tenant,Pool}` and `SearchRange`. (H for layout, M for
  the EPlace names.)
* **PADF:** mParam1 (always 1), mParam2 (always = count), mNumOfElem, then elems[4×i32]:
  position (−1…6), pattern (1–4), silhouette index (0–28), anime id (0–37). The names come
  from `T2Tenant::SetPeopleAnime` usage. (M.)

### 2.6 `SLDF` id 1: silhouettes (`LoadSilhouette` 0x1004d4d0 → `T2SilhouetteDef` 0x10059f00)

The layout is an int32 count, then per entry: mSilhouetteType (300–1012, `GetSilhouetteType`),
mWidth (1/2), mRouteType (−1/0/1), mBitmapID (RT_BITMAP 300–500; 16×36 frames, 6 rows), and
mName. Duplicate types are dropped (`FindSilhouette`). (H)

### 2.7 `PPDF` id 1 (world): people types (`LoadPeople` 0x1004d600 → `T2PeopleDef` 0x1004bc00)

The layout is an int32 count, then per entry: mPeopleType (1–61), mAttribute (0),
mSilhouetteType (300–355), mStressMax (300), mStressBlueLimit (100), mStressYellowLimit (230).
(H)

### 2.8 `WODF` id 1: speech lines (`T2WordDefArray::GetWords` 0x1006e2f0, `T2WordDef::Read` 0x1006e210)

The layout is an int32 count (3–33), then per entry: mSexMask (bit = 1<<sex; 1, 2, 3, 0xFF),
mAgeMask (bit = 1<<(silhouette>>3); 0xFF = all), mFlags (exact match: b0 at own tenant,
b1/b2 people state, b3 = home/lobby, b4 = VIP verdict), mSpecial (exact: 0 normal, 1/2
status, 3–5 VIP grades, 6 Maru, 7 Reggae), and mText. The matching logic is in
`T2WordDef::GetWords` 0x1006e270 and the caller is `T2People::GetInterviewWords` 0x1004a140.
(H for layout, M for the flag names.)

### 2.9 World-plugin resources (inventory + layout; semantics → world agent)

|Type|Reader|Layout|
|---|---|---|
|WORLDDEF|`T2WorldDef` 0x1006e4e0|15 ints + rect + point …, grades[≤8]×6, name, inline-counted list. Fully consumed.|
|WATR (128)|same ctor ('WAtr')|6 discarded ints, 3 ints, str|
|PTDF (text)|`T2PaletteAnimeDef` 0x10047e60|8 ints: mNumOfTowerColor … mSEInterval|
|PODF|`MakePoolDef`→FUN_10050e00|i32, 5 PGDF ids, u8, u8 count, count×{i32,u8}|
|PGDF|`T2Pool::GameLevelChanged` 0x10050390|count × (count × {PZDF id, rate})|
|PZDF|`T2PeopleTimeZoneList::Add` 0x1004dfd0|count × 4 uchar slots|
|SPDF|FUN_100585d0|12 u32 (monthly %). Kegon's is id 128, but the engine opens id 1, so it is **dead data**.|
|CODF|FUN_1001c400|u32 + 12 u32|
|T2EV|`T2TowerEvent` 0x10068ca0|count × {event 4cc, res id}|
|*EV|`T2EventItem` 0x10030050 + subclass|5 base ints + FBEV/BOEV/TREV/SAEV/MAEV/ODEV/MREV/VIEV tails decoded. CLEV/CREV/BBEV/DOEV/HSEV/TOEV/WDEV are world `T2XEvent` (plugin code), with a raw tail.|
|VIPL|Maru_Reggae/VisitVIP|6 ints; only the first is read|
|STRTABLE/STRTBL|`LoadStringTable` 0x10012af0|count + strings|
|BINF/FINF (text)|`InitBkgndInfoProc` 0x10073150 / `T2FloorInfo::InitMask` 0x10033d40|w,h,… then w×h ints|
|PALETTE|FindResourceA("PALETTE")|id 100 = Mac `clut` (BE, 8 + 8n). 101+ = Mac `pltt` (16 + 16n).|

### 2.10 Non-CResFile resources

OBJMAP (see objmap-format.md), T2DLG, and TENANTDEF are kept verbatim as cp1252 text.
TENANTDEF (SkyBrdge only) is a Shift-JIS Mac-era template with no reader, but its labels
confirmed several names above. RT_BITMAP is decoded as DIB header, palette, and pixels. WAVE
is decoded as RIFF chunks with the fmt fields parsed. RT_STRING is decoded as 16 UTF-16
strings. RT_VERSION is decoded as a VS_VERSIONINFO tree with pad bytes preserved.

### 2.11 Data defects found in the shipped plugins (the editor should lint these)

* J-Snack: TMDF 2 references DPDF/PLDF **128**, but the plugin ships them as id 1, so the
  engine falls back to defaults.
* KRestroom: DPDF is id 1 while TMDF wants 128, so the resource is unused.
* HawaianCondo: WODF id 128 is never opened (GetWords opens id 1), so it has no speech lines.
* Kegon: SPDF id 128 is never opened.

## 3. Plugin CODE: pure-data vs custom behavior

`plugin_code_audit.py` follows each factory to its local constructor and reads the subclass
vftable. It names each slot from T2DLL's exported base vftable. An override is a slot that
points at a local, non-exported, non-destructor function. Identical override bodies are
fingerprinted with addresses masked, so shared code can be clustered.

**Summary.** No PE plugin is a pure data shell except DefaultToolPalette, which is its own
`ToolPalette` UI class, and **Kegon**, a World plugin with zero overrides that runs entirely on
T2DLL defaults. The other 103 subclass a T2DLL Def and override 4–20 virtuals. The code is
heavily templated: the 105 plugins collapse to **55 distinct behavior fingerprints**.

* **Shops (28)**: ChangeStatusOnIdle ("Income from shop", calling
  `CollectCustomer`/`CalcInMoney`/`UpdateCommercialEstimate`), Enter/LeaveTenant (pattern
  index and people anime), EmergencyProc (`PushOutAll`), LoadSound, ConstructInfoDialog, and
  a plugin-added virtual (slot 50) that picks a male or female silhouette for the animated
  staff. There are 4 code variants; for example, 21 shops are byte-identical.
* **Restaurants (22)**: the same template plus `PushOutAllCustomer` at closing. 11+10 of them
  are byte-identical.
* **Residential** (Condo, Apartment, HawaianCondo): sales/rent income, people go to sleep,
  holiday logic, and BurntDownProc.
* **Office** (+ Doctor's): employee commuting (`CallEmployee`, return stack) and rent income.
* **Hotel**: Front/RFront save extra data, check "A Front is required…", and use a custom
  SearchRange. Rooms are classified under facility in the table but are the same family.
* **Facilities/structures (33)**: Floor/KFloor/SkyBridge override drawing, cursor, area check,
  and destruct. Parking, Restroom, Trash, Security, Fire, and School each have their own idle
  logic. Theaters/EventCenter/Cathedral/Stadium attach SP sub-plugins (`GetSubPlugin`) and
  save extra data.
* **Movers (7)**: every mover reimplements drawing, cursor, area check, build/destruct, module
  (car) management, and debug info. Stair/Escalator add 3 plugin-defined virtuals (people
  drawing with `SHIL`).
* **OuterObj** (Billboard): it draws the attached `BoPI` ad sub-plugin and saves which ad is
  selected.
* **World**: Tokyo and Hawai override about 20 `T2WorldDef` virtuals (clock, calendar, sky/SE
  palette, holidays, XEvents such as `T2Baseball`/`T2Cloud`, treasure, grade-up messages).
  Kegon overrides none.

Full per-plugin table (generated by `plugin_code_audit.py`; "Family" is a heuristic from the
override strings):

| Plugin | Family | Def base | #ovr | overridden virtuals (Def ; plugin shell) | identical code in |
|---|---|---|---|---|---|
| Aloha | Shop | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | BCDMart, DutyFree, Fruits, SurfShop |
| Apartment | Residential | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Attraction | Venue | T2TenantDef | 9 | LoadSound, DestructFinish, LoadExtraData, SaveExtraData, AreaCheck, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, newV50 |  |
| BCDMart | Shop | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Aloha, DutyFree, Fruits, SurfShop |
| Barber | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Bookstore, Boutique, Convenience, Drug +17 |
| BigElev | Mover | T2MoverDef | 2 | newV0, newV7 |  |
| Billboard | OuterObj | T2OuterObjDef | 6 | IdleProc, DrawProc, DestructFinish, LoadExtraData, SaveExtraData, ConstructInfoDialog |  |
| Boatdeck | Facility/structure | T2TenantDef | 9 | DestructFinish, LoadExtraData, SaveExtraData, AreaCheck, BuildFinish, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Bookstore | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Boutique, Convenience, Drug +17 |
| Boutique | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Convenience, Drug +17 |
| Burger | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Chinese, Cocktail, Coffee, FoodCoat +16 |
| Cathedral | Facility/structure | T2TenantDef | 8 | DestructFinish, AreaCheck, BuildFinish, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, newV50, newV51 |  |
| Chinese | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Cocktail, Coffee, FoodCoat +16 |
| Cigar | Facility/structure | T2TenantDef | 5 | LoadSound, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| ClassicTheater | Facility/structure | T2TenantDef | 6 | newV0 ; shell: newV10, newV11, newV12, newV16, newV17 |  |
| Cocktail | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Coffee, FoodCoat +16 |
| Coffee | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, FoodCoat +16 |
| Coin | Facility/structure | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Condo | Residential | T2TenantDef | 6 | DestructFinish, ConstructInfoDialog, BurntDownProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Convenience | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Drug +17 |
| DefaultToolPalette | ToolPalette | ToolPalette (own class) | 0 | (none) |  |
| Doctor's | Office | T2TenantDef | 5 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 |  |
| Drink | Facility/structure | T2TenantDef | 5 | LoadSound, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Drug | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| DutyFree | Shop | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Aloha, BCDMart, Fruits, SurfShop |
| Electric | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Escalator | Mover | T2MoverDef | 13 | QueryCursor, IdleProc, DrawProc, DoDestructProc, AreaCheck, GetPartFromPoint, DoBuildProc, DebugInfo, AddModuleProc, CalcModuleRectProc, ShowInfoDialog, newV54, newV55 |  |
| EventCenter | Facility/structure | T2TenantDef | 10 | DrawProc, DestructFinish, LoadExtraData, SaveExtraData, BuildFinish, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant ; shell: newV11 |  |
| FireDepartment | Facility/structure | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Floor | Facility/structure | T2TenantDef | 8 | RequierToolIcon, QueryCursor, IdleProc, DrawProc, DoDestructProc, DrawHitMask, AreaCheck, BuildFinish |  |
| Flower | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| FoodCoat | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| French | Restaurant | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 |  |
| Front | Hotel | T2TenantDef | 11 | DestructFinish, LoadExtraData, SaveExtraData, BuildFinish, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, SearchRange, newV50 | RFront |
| Fruits | Shop | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Aloha, BCDMart, DutyFree, SurfShop |
| Gameworks | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| GatheringArea | Facility/structure | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Hawai | World | T2WorldDef | 19 | LoadSound, IsHoliday, DrawOptionObj, MakeXEvent, GetCruise, InitializeDocument, IdleProc, GetGradeUpMessage, ViewModeChanged, ChangeFastSEProc, WriteStatisticsProc, ReadStatisticsProc, CheckTreasure, DrawClock, ClickClock, ClickCalendar, ClickFunds, ClickPopulation, GetClockTip |  |
| HawaianCondo | Venue | T2TenantDef | 6 | DestructFinish, ConstructInfoDialog, BurntDownProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Icecream | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Indian | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Itarian | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| J-Pub | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| J-Snack | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Japanese | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Jenny's | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Juicebar | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| KDrink | Facility/structure | T2TenantDef | 5 | LoadSound, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant | kCigar |
| KElev | Mover | T2MoverDef | 18 | LoadSound, QueryCursor, IdleProc, DrawProc, DoDestructProc, LoadExtraData, SaveExtraData, AreaCheck, GetPartFromPoint, DoBuildProc, FingerToolProc, DebugInfo, AddModuleProc, CalcModuleRectProc, GetInfoDialogID, ConstructInfoDialog, newV54 ; shell: newV17 |  |
| KFloor | Facility/structure | T2TenantDef | 8 | RequierToolIcon, QueryCursor, IdleProc, DrawProc, DoDestructProc, DrawHitMask, AreaCheck, BuildFinish |  |
| KIcecream | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| KRestroom | Facility/structure | T2TenantDef | 5 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| KServ | Mover | T2MoverDef | 17 | LoadSound, QueryCursor, IdleProc, DrawProc, DoDestructProc, LoadExtraData, SaveExtraData, AreaCheck, GetPartFromPoint, DoBuildProc, FingerToolProc, DebugInfo, AddModuleProc, CalcModuleRectProc, GetInfoDialogID, ConstructInfoDialog, newV54 | ServElev |
| KTrashFacility | Facility/structure | T2TenantDef | 5 | DefIdleProc, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, SearchRange | TrashFacility |
| Kegon | World | T2WorldDef | 0 | (none) |  |
| Kids | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| KoreanBBQ | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Largeroom | Facility/structure | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| LocalPickle | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Maintenance | Facility/structure | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, SearchRange |  |
| Men'sClothing | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Mexcan | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Observatory | Facility/structure | T2TenantDef | 6 | DrawProc, AreaCheck, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Office | Office | T2TenantDef | 5 | LoadSound, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| OldTimeCafe | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| OrangeJulius | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 |  |
| Parking | Facility/structure | T2TenantDef | 14 | LoadSound, QueryCursor, IdleProc, DrawProc, DoDestructProc, DestructFinish, AreaCheck, ClickProc, BuildFinish, GetInfoDialogID, ConstructInfoDialog, EmergencyProc, EnterTenant, newV50 |  |
| Petshop | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Photo | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| PowerRoom | Facility/structure | T2TenantDef | 7 | DefIdleProc, DestructFinish, LoadExtraData, SaveExtraData, BuildFinish, ConstructInfoDialog, ChangeStatusOnIdle |  |
| RFront | Hotel | T2TenantDef | 11 | DestructFinish, LoadExtraData, SaveExtraData, BuildFinish, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, SearchRange, newV50 | Front |
| RMaintenance | Facility/structure | T2TenantDef | 5 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant, SearchRange |  |
| Ramen | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Record | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Restroom | Facility/structure | T2TenantDef | 5 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| School | Facility/structure | T2TenantDef | 7 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50, newV51, newV52 |  |
| SeaFood | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Securityroom | Facility/structure | T2TenantDef | 5 | DefIdleProc, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| ServElev | Mover | T2MoverDef | 17 | LoadSound, QueryCursor, IdleProc, DrawProc, DoDestructProc, LoadExtraData, SaveExtraData, AreaCheck, GetPartFromPoint, DoBuildProc, FingerToolProc, DebugInfo, AddModuleProc, CalcModuleRectProc, GetInfoDialogID, ConstructInfoDialog, newV54 | KServ |
| Singleroom | Facility/structure | T2TenantDef | 5 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| SkyBrdge | Facility/structure | T2TenantDef | 6 | DrawProc, DestructFinish, DrawHitMask, AreaCheck, BuildFinish, OptionProc |  |
| Smallroom | Facility/structure | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| SobaNoodle | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Souvenirs | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Spa | Facility/structure | T2TenantDef | 4 | ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Sportsshop | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Stadium | Venue | T2TenantDef | 11 | LoadSound, QueryCursor, DrawProc, DestructFinish, LoadExtraData, SaveExtraData, OptionProc, AreaCheck, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant |  |
| Stair | Mover | T2MoverDef | 13 | QueryCursor, IdleProc, DrawProc, DoDestructProc, AreaCheck, GetPartFromPoint, DoBuildProc, DebugInfo, AddModuleProc, CalcModuleRectProc, ShowInfoDialog, newV54, newV55 |  |
| StdElev | Mover | T2MoverDef | 17 | LoadSound, QueryCursor, IdleProc, DrawProc, DoDestructProc, LoadExtraData, SaveExtraData, AreaCheck, GetPartFromPoint, DoBuildProc, FingerToolProc, DebugInfo, AddModuleProc, CalcModuleRectProc, GetInfoDialogID, ConstructInfoDialog, newV54 |  |
| Steak | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Subway | Facility/structure | T2TenantDef | 9 | DestructFinish, LoadExtraData, SaveExtraData, AreaCheck, BuildFinish, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Suiteroom | Facility/structure | T2TenantDef | 5 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| SuperMarket | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| SurfShop | Shop | T2TenantDef | 6 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Aloha, BCDMart, DutyFree, Fruits |
| Sushibar | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| Theater | Facility/structure | T2TenantDef | 10 | DrawProc, DestructFinish, LoadExtraData, SaveExtraData, BuildFinish, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, newV50 ; shell: newV11 |  |
| Ticket | Venue | T2TenantDef | 5 | DrawProc, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Tokyo | World | T2WorldDef | 20 | LoadSound, IsHoliday, IsRainyDay, MakeXEvent, InitializeDocument, IdleProc, GetGradeUpMessage, ViewModeChanged, ChangeSEProc, ChangeSkyColorProc, AdjustSkyStateProc, WriteStatisticsProc, ReadStatisticsProc, CheckTreasure, DrawClock, ClickClock, ClickCalendar, ClickFunds, ClickPopulation, GetClockTip |  |
| Toyworld | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Trad-Japanese | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| TrashFacility | Facility/structure | T2TenantDef | 5 | DefIdleProc, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, SearchRange | KTrashFacility |
| Twinroom | Facility/structure | T2TenantDef | 5 | ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant |  |
| Video&DVD | Shop | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Barber, Bookstore, Boutique, Convenience +17 |
| Weinstein'sToffee | Restaurant | T2TenantDef | 7 | LoadSound, ConstructInfoDialog, EmergencyProc, ChangeStatusOnIdle, EnterTenant, LeaveTenant, newV50 | Burger, Chinese, Cocktail, Coffee +16 |
| kCigar | Facility/structure | T2TenantDef | 5 | LoadSound, ConstructInfoDialog, ChangeStatusOnIdle, EnterTenant, LeaveTenant | KDrink |

## 4. Editor field list (what the Plugin Editor should expose)

Hide every `_skip*`, `_unread*`, `__hi`, and `__term` field. Write them back unchanged, or use
the population defaults for new plugins: `_skip3`=128, `_skip4`=300, `_skip5..7`=129/130/300,
`_skip8`=variation count, `_skip11..13`=128/129/128, and `_skip14..17`=6001/6002/0/0.
`t2p_schema.normalize()` recomputes counts from list lengths.

**All classes (ATTR + DESC):** mID (4cc, unique), mPluginName (str), mItemType (int, must
equal mToolType), mGameLevel (1–6), mAttrib (world checkboxes: Tokyo 0x1/0x2/0x4, Kegon 0x10,
Hawai 0x100), mDependID (4cc picker: none/FRON/…), mMustItem (bool), mSecretItem (bool).
DESC mType is fixed by the plugin class, and mVersion = 3.

**Tenant (TNDF + TMDF×2 + DPDF + PLDF + PADF + SLDF + WODF):**
* *Toolbox:* mCategory (enum 10 Basic, 20 Transit, 30 Residential, 40 Office, 50 Restaurants,
  60 Hotel, 70 Bldg. Facilities, 80 Shops, 90 Entertainment, 100 Special, 200 Outside),
  mCategoryName, mCategoryHelp, mToolNo, mToolType.
* *Footprint:* mWidth (1–56), mHeight (1–8), mEntranceV/H, mExitV/H.
* *Flags:* mAttribute bits (§2.3), mRegistID (enum §2.3), mDownFlag (0–4), and
  mSubPluginType (none/MoPI/HePI).
* *Variations (1–4):* name, help title, help text, price ($100), price string, maintenance
  ($100), maintenance string, and power.
* *Economy:* mCapacity, inMoney[1 or 4], mBindTime, mOpenTime/mCloseTime (minutes),
  mBelongCapacity, mSenseOfNoise, and mMaxBuildCount.
* *Sprites:* mNumOfKind and mNumOfPattern, kept consistent with OBJMAP.
* *Employee/customer tables:* mEconoType, mSpecialFlag, and members (name ≤15, demand type,
  people-type range, option, percent, count), plus the Day table (2×2 %) and the Place table
  (3 × {%, score}).
* *People animation:* elems (position, pattern, silhouette, anime id).
* *Silhouettes:* type, width, route type, bitmap id, name.
* *Speech:* text, sex mask, age mask, flags, special.

**Mover (MVDF + WODF):** the Toolbox/variation fields above, then mCapacity (car capacity),
mStress, mAddSize/mModuleInset rects, mMaxLength, mMinLength, mMaxStop, mScore, mDefaultWait,
speeds[3], and mMaxModule. Show mModuleUnit*/Param* read-only and marked as having no known
consumer.

**OuterObj (OODF):** the Toolbox/variation fields with a single variation, plus
mSubPluginType (`BoPI`).

**ToolPalette:** the ATTR text form only (no Def).

**World:** handled by the world agent. The schema already round-trips WORLDDEF, PODF, PGDF,
PZDF, SPDF, CODF, PTDF, WATR, VIPL, T2EV, and the event resources.

## 5. Remaining unknowns

* Mover +0x188–0x194 (`mModuleUnitH/V`, `mModuleParam1/2`), Equip +0x120/+0x134/+0x138, and
  ToolDef +0x5c (= game level): no consumer found in T2DLL, T2.exe (exe3 decompile), or mover
  plugin code. The Wine oracle (doc/yoottower/oracle.md) could confirm them by mutation.
* The bit-by-bit meaning of TMDF mEconoType/mSpecialFlag, TenantDef mDownFlag, and WODF mFlags.
* World XEvent tails (CLEV/CREV/BBEV/DOEV/HSEV/TOEV): parsed by the world plugin's code
  (`T2WorldDef` vtable +0x4c), so they are left to the world agent.
