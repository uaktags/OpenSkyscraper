# YootTower `.bld` tower-save format

**Status:** fully decoded for the shipped worlds. `tools/yoottower/bld.py` parses
both shipped saves and two saves written by the game under the Wine oracle
(`doc/yoottower/oracle.md`). It writes all of them back **byte-identical**.
Every structure below comes from the game's own load and save code (T2.exe plus
T2DLL.dll), decompiled with Ghidra 12.1.3 (project `~/yoottower-decomp/save-agent/proj`).
The structures are also cross-checked against the live game: the funds,
population and date that the oracle shows on screen match the decoded values
for every save.

```
python3 -I tools/yoottower/bld.py summary  "Sample Tokyo Map.bld"
python3 -I tools/yoottower/bld.py dump     X.bld [--full] [-o x.json]
python3 -I tools/yoottower/bld.py roundtrip *.bld
python3 -I tools/yoottower/bld.py write    x.json -o X.bld    # needs dump --full
```

Confidence tags: **H** means read straight from the decompiled reader and writer,
and the round trip passes. **M** means the field name comes from an accessor or
from context. **L** means the name is a guess. Unnamed fields are called `fXX`,
where XX is the field's offset in the in-memory object. The byte layout of every
field is H. Only the names vary in confidence.

## 1. Stream primitives (`T2Archive`, T2DLL)

* The file is a single raw `T2Archive` stream: little-endian and unaligned, with no
  compression. `BeginCompressedStream`/`EndCompressedStream` (0x10016d60/0x10016d70) are
  no-ops. There is also no chunk directory and no length header. The file is a plain
  sequence of reads, so a parser must understand every structure to reach the end.
* Primitive operators: `>>`/`<<` for u8, s8, u16, s16, u32 and s32 (0x10016d80–0x10016f20).
  `ReadPStr` (0x100171c0) reads a u8 length followed by that many bytes. **`ReadSRect`
  (0x10017360) reads a Mac QuickDraw Rect**: four **big-endian** s16 in the order
  `top, left, bottom, right`. FUN_100173d0 is a byte swap. This corrects
  resource-formats.md §9.6, which describes LE left/top/right/bottom. bld.py stores
  rects as `[left, top, right, bottom]`.
* Many sub-objects begin with a 4-byte marker. bld.py displays these in **file byte
  order** (the bytes as stored), which is the same convention as
  itemtype-catalog.md. The code compares the marker as a u32. For example, `>NT<` is
  `0x3c544e3e`.
* LArray serialisations (T2DLL), used by several ID lists:
  * `ReadAsWord` (0x100134f0): s32 itemSize(2), s32 count, s32, s32, u8, u8, s32 dataBytes,
    count×u16, then padding u16s up to dataBytes/itemSize.
  * `ReadAsChar` (0x10013720): s32 itemSize(1), s32 count, s32, s32, s32 total,
    count×u8, then padding u8s up to total.

## 2. Top level

Load: **T2.exe `FUN_00419a50`**, called from the document's OnOpenDocument (xref 0x433904).
Save: **T2.exe `FUN_0041b23f`**. Ghidra does not create this function automatically.
It begins with `64 a1 00 00 00 00`, right after a `ret 4` + nop pad.

| # | Field | Type | Notes |
|---|---|---|---|
| 1 | magic | 4 bytes `002T` | u32 `0x54323030` 'T200'. Any other value: load fails (H) |
| 2 | world | 4cc | world plugin mID, looked up with `FindPlugin(id,'lPdW')`. `IAWH` Hawaii, `KJNS` Tokyo (H) |
| 3 | header.level | s16 | doc+0x8c, the **star level**. Compared against plugin `GetGameLevel` to unlock tools (M). Valid range: the world's startGrade..5. Loading runs FUN_00420b50, which loads the grade icon at WorldDef+0x144+level*4 (gradeIconAnimIds at +0x148..+0x158). Level 6 reads the towerNamePrefix string as a bitmap id, `T2BitImage` throws, and the load fails with "File load error" (oracle) |
| 4 | header.f9c | s16 | doc+0x9c |
| 5 | header.fa0 | u32 | doc+0xa0 |
| 6 | header.f80 | s16 | doc+0x80 |
| 7 | header.f88 | u8 | doc+0x88 bool. The writer always writes 0 |
| 8 | header.speed | s16 | `UT2Coordinate::ChangeSpeedMtoF(GetSpeed())`. Both samples: -2 (M) |
| 9 | header.zero | s16 | always 0 |
| 10 | header.fa4 | s16 | doc+0xa4, a view mode passed to doc vtbl+0x144 after load (L) |
| 11 | header.reserved | 10×u8 | always 0 |
| 12 | date | 4×u16 | `T2DateTime(T2Archive&)` 0x1001c910: **year**, **month0** (0-based; `GetMonth()`=month0+1), **minutes** since 00:00 (`Get24Hour`=min/60), **seconds** (H) |
| 13 | plugins | s32 n + n×CPluginInfo | §3 (H) |
| 14 | people | T2PeopleArrayList | §5 (H) |
| 15 | floor | T2FloorInfo | §4 (H) |
| 16 | queue | s16 present; if >0: u32 n + n×s16 tenant equipID | tenants pushed onto doc+0x1c8 (a CObjectQueue) |
| 17 | f1e0 | u8 | doc+0x1e0 bool |
| 18 | f1f0 | s16 | doc+0x1f0 |
| 19 | pool | T2Pool | read through doc vtbl+0x1e0 → `FUN_0041c910` → `T2Pool(T2Archive&,…)` 0x100502b0. §6 |
| 20 | trafficMap | cells×(u16 traffic, u16 range) | `T2TrafficInfo::Read` 0x1006b890. §7 |
| 21 | settlement | T2Settlement | §8 (H) |
| 22 | movies | u32 n + n×(4cc id, u32 len, len bytes) | `T2MoviePluginList::Read` 0x10043a10. The payload is `T2MoviePlugin::Read` (u32 + 6×s16 = 12 bytes; persons R/F, schedule) |
| 23 | towerEvent | u32 latestBuild, s16 n, n×event | `T2WorldDef::ReadTowerEvent` → `T2TowerEvent::Read` 0x10069fc0. §9 |
| 24 | ui | 4×SRect + 2×s32 + 4 bytes | main window placement, then the main view scroll in 1:1 pixels: **vertical first** (36 px per row), then horizontal (8 px per unit), then the Tools / Control / Message palette rects, an all-zero SRect, and 4 bytes that are always `01` (H). bld.py names the two scroll values `scrollH`, `scrollV`; the order is the other way round (oracle: Tokyo's 3516, 918 = top row 97.7, left unit 114.75) |
| 25 | searchedTenant | u16 | doc+0x174 tenant equipID, or 0 |
| 26 | names | s32 n + n×T2Name | `T2NameList::Read` 0x10044e60. §10 |
| 27 | worldStats | world-specific | `T2WorldDef` vtbl+0xd0 `ReadStatisticsProc`. §11 |

Load-time side effects worth knowing for an importer:

* Plugins whose CPluginInfo has `mustItem≠0` but that are not installed make the
  load throw. The game shows dialog 0x456 listing them.
* Tenants or out-objects whose def cannot be found are destroyed.
* People in status 0xf who are on a tenant with def+0x34==0x5f1 are reset.
* Elevators whose module count does not match their shaft are removed. Their requests
  are broken out at the same time.

## 3. Plugin table (`CPluginInfo::Read` 0x10010720)

| Field | Type | Meaning |
|---|---|---|
| type | 4cc | DESC class: `lPnT` tenant, `lPvM` mover, `lPoO` outdoor object, `lPdW` world, `lPeH` hall event, `lPoM` movie (H) |
| id | 4cc | **`CFilePlugin::GetID` (+0x48)**. This is the 4-byte code that itemtype-catalog.md lists, for example `GRUB`, `ROLF`, `EdtS` (H) |
| itemType | s16 | **`CFilePlugin::GetItemType` (+0x5c)**, the *numeric* type, for example Floor=1000, Office=1200, Restroom=1500, Simple Elevator=1810, Billboard=2100. Every object record refers to its plugin through this number (H) |
| mustItem | s16 | `GetMustItem` (+0x60). 1 for Floor, Sky Bridge and the world (H) |
| name | pstr | plugin name (H) |
| fileName | pstr | absolute path of the plugin on the saving machine (`GetModuleFileNameA`). This path is **not** used to locate plugins on load (the oracle loads both samples from C:\YOOT) (H) |
| used | u8 | `IsUsed`. 0 in every save seen so far |

> **Naming note:** the code calls the 4cc `ID` (mID) and the number `ItemType`.
> itemtype-catalog.md and resource-formats.md §9.2 call the 4cc "mItemType". The
> .bld (via `CFilePlugin::GetPluginInfo` 0x1000e160) shows that the ATTR 4cc is
> `mID` and the numeric field is `mItemType`. OpenSky's importer needs a map
> from numeric itemType to plugin, and this table supplies it for each save.

The table lists every plugin installed when the game saved, whether or not the
tower uses it. That includes all movies (`lPoM`) and the hall events built into
T2.EXE (`lPeH`).

## 4. Floor info (`T2FloorInfo::Read` 0x10033db0 / `Write` 0x10033f00)

```
s32 vRange            rows   (Tokyo 122, Hawaii 62)   must equal WorldDef+0x1f8 or the rest is skipped
s32 hRange            columns (Tokyo 368, Hawaii 400)  must equal WorldDef+0x1fc
'ItnU'  vRange*hRange x T2UnitInfo { u16 moverOrRequestID; u16 tenantID }   (T2UnitInfo::Read 0x1006c970)
'AtnT'  T2TenantArrayList   s32 nextTenantNumber; s32 nBlocks; nBlocks x 128 tenants
'ArvM'  T2MoverArrayList    s32 nBlocks;  nBlocks x 32 movers
'AqeR'  T2RequestArrayList  s32 nBlocks;  nBlocks x 64 requests
'AbOO'  T2OutObjArrayList   u32 nBlocks;  nBlocks x 64 out-objects
'ItuO'  T2OutsideInfo       u32 n (= vRange*hRange); n x u16 out-object id
```

The unit map is row-major (`row*hRange+col`). In the first u16, values below 1000
are mover equipIDs (the elevator shaft column) and values of 1000 and up are
request (waiting-area) IDs. The second u16 is the tenant equipID covering the
cell. Tokyo has its ground line at row 110: floor number = 110 − row, so row 109
is 1F and rows 107–109 are the 3-storey atrium lobby. That fits WorldDef+0x54 →
FloorInfo+0xc `GetGroundLine` (M).

Object blocks have a fixed size. **Every slot is written, used or not.** An unused
slot is just its marker plus the 4-byte T2Object header. bld.py keeps unused slots
as `null`, or as `[status, favorite]` when either value is nonzero. Slot *k* of
block *b* has equipID `base + b*per + k`: tenants from 1000 (+128 per block),
movers from 1 (+32), requests from 1000 (+64), out-objects from 1000 (+64), people
from 1 (+256). These bases come from the `T2*Array(uint startID)` constructors called
in each `*ArrayList::Read`.

### 4.1 Common headers

`T2Object::LoadSelf` 0x100453b0: `u8 used, s16 status, u8 favorite`. If `used==0`,
nothing else follows (for every class).

`T2Equip::LoadSelf` 0x1002ef60 (tenants and movers), if used:

| Field | Type | Accessor / meaning |
|---|---|---|
| equipID | u16 | `GetEquipID` (+0x20) (H) |
| area | SRect | unit rect `[left, topRow, right, bottomRow)` (H, matches the unit map) |
| equipType | s16 | `GetEquipType` (+0x34) = **plugin numeric itemType**. `FindTenantDef`/`FindMoverDef` look it up (H) |
| drawMode | s16 | `GetDrawMode` (M) |
| patIndex | u8 | `GetPatIndex` (M) |
| f44 | s16 | |
| inMoneyIndex | s16 | `GetInMoneyIndex` (rent tier) (M) |
| f48 | s16 | |
| buildDay | s32 | `GetBuildDay` (M) |
| ce | CEArray | 4cc class `AEC_`/`AECT` (tenant)/`AECR` (route)/`AECF` (floor)/`AECM` (mover), followed by an LArray-word of connected equipIDs. `AECx` means none (H) |
| valiation | u32 | `T2HaveOutViewObject::GetValiation` (+0x1c) (M) |

### 4.2 Tenant (`T2Tenant::LoadSelf` 0x1005e820 / `SaveSelf` 0x1005eb50)

Marker `>NT<`, then T2Equip, then if used:
`u8 kindIndex, s32 resideDay, s16 f8c, s16 estimate, u8 bind, u16 relatedTenantID,
s16 reserveCount, s16 belongCount, s16 customerCount, s16 workCount, s16 totalCustomer,
u16 tenantNumber (room number), u32 relatedTenantListID, s16 n + n×u8 marks,
'>kLp', u32 firstPeople, u32 people(+0x64), u32 hasBelongList [if 1: s32 n + n×u32 people
IDs (T2PeoplePtrList::LoadSelf 0x1004dc50)], then def extra data.`

The names come from the inlined accessors in T2.exe/T2DLL (`GetKindIndex`, `GetResideDay`,
`GetEstimate`, `Bind`, `GetRelatedTenantID`, `GetReserveCount`, `GetBelongCount`,
`GetCustomerCount`, `GetWorkCount`, `GetTotalCustomer`, `GetTenantNumber`,
`GetRelatedTenantList`, `GetFirstPeople`, `GetBelongList`). Names: M. Layout: H.
People are referenced by **people ID** (§5), which equals the slot ID.

**Extra data** goes through the def's vtbl+0x50 `LoadExtraData`. Every def class in
T2DLL (`T2TenantDef`, `T2MoverDef`, `T2EquipDef`) uses `T2EquipDef::LoadExtraData`
0x1002f8c0, which reads `s32 n + n bytes`. The writer `SaveExtraData` 0x1002f900
always writes `n=0`. If the def is missing, the same `s32 n + n` is skipped. A plugin
could subclass the def and override this. None of the tenants in the saves seen so
far do, but **StdElev.t2p does** (MoverDef vtbl+0x50 `FUN_1000f4e0` / +0x54
`FUN_1000f570`): it writes `n = 80` and then 2 × 10 four-byte records (a day-type ×
period table, most likely the car schedule), and its reader reads those 20 records
**whatever n says**. A Simple Elevator written with any other payload size desyncs the
load and crashes T2.exe (verified under the oracle). A fresh shaft stores
`00 00 06 00` in every record.

**Every plugin that overrides the extra data** (audit:
`plugin_code_audit.py`, plus the two defs with a statically linked base class,
ClassicTheater and BigElev, whose slots +0x50/+0x54 were read from the vtable):

| Plugin | n written | Record (reader) |
|---|---|---|
| StdElev | 80 | 20 x (s16, s16), read whatever n says |
| KElev, KServ, ServElev, BigElev | 120 | 2 x 10 x 3 s16, read whatever n says |
| Theater, ClassicTheater | 20 | u32 movie sub-plugin mID + 4 x s32 |
| EventCenter | 64 + 9k | u32 sub-plugin, 4 x s16, s16 k + k x (u32, s16, s16, s8), s16 12 + 12 x u32 |
| Front, RFront | 20 | u16 3 + 3 x (u16, s32) |
| PowerRoom | 9 | u16 5 + 5 x u8 + u16 |
| Subway | 2 | s16 |
| Attraction | 2 or 0 | s16 (renovated flag) |
| Boatdeck | 0 | nothing (the reader makes the CrEv cruise event) |
| **Stadium** | **0x405** | pstr team name, u8, u16, two team records (pstr, u8, 3 x 20 x u16, 3 x u16), u8, u8, u16: 264 bytes with empty names |

All of them read their record when n > 0 and skip n bytes when the tenant has
no object, so `s32 n + n bytes` is exact for every one **except Stadium**
(SaveExtraData 0x1000bee0 writes n = 0x405 from FUN_1000f7e0, then the record
FUN_1000f6a0; LoadExtraData 0x1000be40 reads the record whatever n is). bld.py
(`EXTRA_SIZE`) and BldLoader (`stadiumExtraSize`) size a Stadium record from
its strings and keep the announced n (`extraCount`) for the writer.

### 4.3 Mover (`T2Mover::LoadSelf` 0x1003f400 / `SaveSelf` 0x1003f530)

Marker `>VM<`, then T2Equip, then if used:
`u8 showShaft, s16 length (floors), 'ADIR'+LArray-word request IDs (the stop list,
one per floor and side; 0 = no stop), s16 waitCount, s16 f6c, module list, s32 refCon,
def extra data (s32 n + n)`.

Module list (`T2MoverModuleList::ReadMMList` 0x10041000). The 4cc picks the class:

* `LMlE` = **T2ElvModuleList** (elevator cars). `s16 f20, SRect rect, s32 n,
  n×T2ElevatorModule, SRect rect2`.
* `LMtS` = T2StairModuleList (stairs, escalator). `s16, SRect, s32 n, n×T2StairModule`.
* `LMM_` = the base list. It reads only `s16, SRect`. `LMMx` means no list.

Every module starts with the T2Object header. `T2MoverModule::LoadSelf` 0x10040980 then
reads `s16 position, direction, waitCounter, patIndex, numPeople, f4c, u32 firstPeople,
u32 lastPeople`. An elevator car (`T2ElevatorModule::LoadSelf` 0x1002e320) then reads
`s16 index, homePosition, offsetPos, nextStop, f6c, topTurn, bottomTurn` and a
`'AfIS'`+LArray-char **stop table**: 2 bytes per shaft floor (20 entries for
Tokyo's 10 floors, probably up and down), holding the car's stop flags. That is
where the car schedule lives (layout H, byte meanings unknown). A stair module
(`T2StairModule::LoadSelf` 0x1005c3b0) has just an `SRect moduleRect`. The Tokyo
elevator has 6 car slots. Two are used: car 0 parked at floor 0 and car 1 at
floor 9. It spans 10 floors and has a 20-entry request ID list.

### 4.4 Request (`T2Request::LoadSelf` 0x10053660)

Marker `>QR<`, then T2Object, then if used: `u16 requestID, u16 moverID, u8 upDown,
s16 f18 (column), s16 f1c (row), s8 moduleIndex, s16 drawWidth, 4×u32 people IDs`.
Requests are the waiting areas beside each elevator stop.

### 4.5 Out-object (`T2OutObj::LoadSelf` 0x100462e0)

There is **no marker**. T2Object, then if used: `u16 outObjID, SRect area, u32 valiation,
s16 outObjType (plugin itemType, e.g. Billboard 2100)`, then the OuterObjDef's
`LoadExtraData` (vtbl+0x50), or `s32 n + n` bytes when the def is missing. The base
`T2ToolDef::LoadExtraData` reads nothing, but the only shipped out-object plugin,
**Billboard.t2p, overrides it** (0x1000c470 / 0x1000c550): `s32 n` (6) + `u32` ad
sub-plugin mID (0 = no ad) + `s16`, or n = 0 without a record. So `s32 n + n` is exact.

## 5. People (`T2PeopleArrayList::Read` 0x1004b900, `T2People::LoadSelf` 0x1004a810)

`s32 nBlocks`, then nBlocks × 256 people. Each person:

```
'>PP<'  T2Object hdr; if used:
  T2Matter (0x1003bfc0): u32 matterID (= people ID), u16 workTenant, u16 homeTenant,
     u16 currEquipID, u16 destination, s16 curH, s16 curV, s16 dstH, s16 dstV,
     s16 startTime, s8 direction, s8 walkStyle,
     4cc 'KTSR' -> s32 n + n x (u16 tenant, u16 time)   return stack ('KSRx' = none)
  T2People: u8 specialFlag, u8 walk, s16 stress, u16 f80, s16 estimate, s16 color,
     s16 f68, s16 prevTenantType, s32 style,
     4cc 'PYTP' -> T2PeopleType 5 x s8 (f0, silhouetteType, econoType, transportType, life)
                 ('PTPx' = none)
     3 x s16 silhouette IDs (FindSilhouette), '>kLp', u32 linkPrev, u32 linkNext
```

T2People has CLink as its first base, at +0..+0xb (vtbl, prev, next). The Matter
sub-object starts at +0xc, which is why the T2People accessor offsets are 0xc
higher than the LoadSelf offsets. Names: M. Layout: H. In Tokyo, the 2 residents
have workTenant=1003. That is the 4F tenant whose belongList is `[1,2]`.

## 6. Pool (`T2Pool(T2Archive&,…)` 0x100502b0, `Write` 0x10050a90)

`s16 gameLevel, s32 population (GetPopulation, the number shown in the Information bar), s32 f18`,
then T2PeopleDemandList (0x1004bde0): `u32 nTimeZones`, each a T2PeopleTimeZoneList
(0x1004de10): `s32 n, u8 fixed, s8, s8 currentItem, s8`, then n×T2PeopleTypeArray
(0x1004ebf0): `s32 k, k×T2PeopleType (5×s8), u32, s32, s32`. This is the visitor
pool. Tokyo has 5 time zones of 4 arrays, up to 200 people types each. The pool is
present only if the world def has a PoolDef (WorldDef+0x1d4). All shipped worlds do.

## 7. Traffic map

`T2TrafficInfo(rect, 100, 500, 1000, 4)` is built over
`(WorldDef+0x200, FloorInfo top line +0x10, WorldDef+0x208, bottom line +0x14)`. It holds
`height × (width/4)` cells of `(u16 traffic, u16 range)`. **These dimensions come from
the world plugin, not the save.** bld.py therefore finds the end of the map by
validating the fixed tail of the file: the next structure must parse to EOF, with the
always-zero SRect and the four constant `01` bytes at the right place. The world
sizes seen so far are Hawaii 4284 cells (51×84) and Tokyo 8066 cells. The map is
all zero in the shipped saves and nonzero after play.

## 8. Settlement / finances (`T2Settlement(T2TowerDoc*,T2Archive&)` 0x100591d0, `Write` 0x10059370)

| Field | Type | Meaning |
|---|---|---|
| funds | s32 | `GetCurrentFunds` (+4). **Units of $100**: 78770 = $7,877,000 on screen (H, oracle) |
| previousFunds | s32 | `GetPreviousFunds`: funds at the last `Update()`, i.e. the start of the period (H) |
| totals[0..4] | 5×s32 | `GetTotalSettlement(cat)`. `DoPayTool` adds construction spending to cat 0 or 3. `DoPay` adds running costs to cat 1, 2 or 4. cat 5 = 0+3 and cat 6 = 2+4 are derived. All are reset by `Update()` (M; which category is which is L) |
| tenantCosts | u32 n + n×(s16 itemType, s32 amount) | per tenant def, the def+0x110 running total (construction cost this period) |
| moverCosts | same | per mover def |
| outObjCosts | same | per out-object def |

## 9. Tower events (`T2TowerEvent::Read` 0x10069fc0 / `Write` 0x10069dd0)

`u32 latestBuild (GetLatestBuild), s16 n`. Each event is `4cc type, s16 id, u8 standby`
followed by the event's own `Read` (vtbl+0x30). In every T2DLL class, that `Read` starts
with a **u32 payload length** (`T2EventItem::Read`/`Write` 0x10030650/0x10030640:
`u32 len + len bytes`, written as 0). `T2BlackOut::Write` writes 6 and then 3×s16. On
load, the game matches each record by (type, id) against the world's event lists. If
nothing matches, it skips `len` bytes. bld.py keeps the payload as hex.

**Exceptions** (no length prefix): `vESH` T2HotSpring (Kegon.t2p and Hawai.t2p
0x10012cd0) writes **nothing**; the T2Transport events, `vERM` metro trains (T2DLL vtbl
0x10085f80) and `vErC` cruise (Hawai.t2p, made by Boatdeck), write `u32 firstPeople` and,
when it is not 0, `u32 lastPeople` (T2Transport::Write 0x1006c020). An empty train looks
like a zero length; a train with people does not. X events that do not override
Read/Write would write nothing too (T2WorldPlugin::XEventWrite is empty), but every
shipped one seen either overrides them (Baseball 8, Wedding 13) or derives from T2EventItem
(T2Tonbi: u32 0, written with type 0; that is the `0x00000000` record).

Type codes found (from the class `GetType`): `vEOB` BlackOut, `vEBF` FireBurning,
`vEaS` SantaClaus, `vErT` Terrorist, `vEiV` VisitVIP (14-byte payload), `vEaM`
Maru_Reggae (14), `vEVT` TowerVision. `vEBB`, `vERM`, `vEdO` and a `0x00000000` type
appear in the saves but do not belong to any T2DLL class. They are probably defined
in T2.exe or the world plugins (`T2XEvent`). Their payloads were always empty in the
samples.

## 10. Names (`T2Name(T2Archive&)` 0x10044000 / `Write` 0x10044680)

`u16 type, u32 id, u8 favorite, s32 len (strlen+1), u8 n (=len), n bytes (string plus NUL)`.
The type comes from the T2Name constructors: 1 = tenant (uses tenant+0x20 equipID),
2 = ? (+0x20), 3 = mover? (+0x50), 5 = people (+0x1c). The reader has special cases
for len 2 and 4 (old 2-byte and 4-byte name formats). Both samples have no names.

## 11. World statistics (world plugin)

The base `T2WorldDef::ReadStatisticsProc` (0x10072ed0) reads nothing. The shipped
worlds override it: Kegon.dll `WorldDef::ReadStatisticsProc` 0x10015360 calls
FUN_1000dc30 (reader) and FUN_1000dd30 (writer). The block is
`s16 a, s16 b, s16 swapped, u8 enabled, s16 f49, 64×s32` = **265 bytes**, the same in
both samples. Hawaii and Tokyo both store `a=4, b=8, f49=2` and 64 zeros. bld.py
decodes the block when exactly 265 bytes remain and keeps raw hex otherwise.

## 12. The sample towers (parity oracles)

| | `Yooooot.bld` (182,123 B) | `Sample Tokyo Map.bld` (324,441 B) |
|---|---|---|
| World | `IAWH` Hawaii | `KJNS` Tokyo (SINJYUKU.T2P) |
| Grid | 62 rows × 400 cols | 122 × 368 (ground line row 110) |
| Date | Year 1, Feb (month0=1), 10:47 | Year 1, Jan, 09:34:02 |
| Star level | 2 (pool gameLevel 2) | 1 |
| Funds | 50,000 → $5,000,000 (previous the same) | 78,770 → $7,877,000 (previous $10,000,000) |
| Population | 0 (0 people objects) | 2 (2 residents of the Apartment) |
| Tenants | **none** (nextTenantNumber 12: a lot that was built on and then cleared) | 26: Floor ×8 (1000), Office ×9 (1200), Restroom ×7 (1500), Ice Cream Stand ×1 (1600), Apartment ×1 (1102) |
| Movers | none | 1 Simple Elevator `EdtS` (1810): 10 floors, 2 cars (of 6 slots) |
| Out-objects | none | none |
| Plugin table | 73: 23 tenant, 4 mover, 1 outobj, 1 world, 6 hall events, 38 movies; paths `C:\YOOT\PLUGINS\…` | 69; paths `C:\PROGRAM FILES\YOOTTOWER\YOOTTOWER\PLUGINS\…` |
| Settlement totals | all 0 | cat1 = 21,230 (a DoPay category) |
| Tower events | 11 records | 24 records |
| Oracle screenshot | `~/yoottower-decomp/oracle-shots/stage_c_yooooot.png` (empty Hawaii lot) | `stage_c_tokyo_via_open.png` |

Extra validation saves were written by the real game under the oracle: Tokyo after
about 6 h and 24 h of play, saved through File > Save. They are in
`~/yoottower-decomp/save-agent/saves/` (`tokyo_run1.bld`: 15:45, funds 79,047,
population 96, 63 people objects; `tokyo_run2.bld`: Feb 09:54, funds 79,049,
population 57). Both round-trip byte-identical, and the on-screen values match
(`oracle-shots/sv_1.png`, `sv_6.png`).

## 13. Unknowns / risks

* Some field names are still placeholders: `f9c`, `fa0`, `f80`, `f88`, `fa4`, `f1e0`,
  `f1f0`, tenant `f8c` (300 in every tenant), mover `f6c`, the request columns, the
  meaning of the stop-table bytes, the settlement categories, and `T2PeopleType.f0`.
  Their layout is certain; only their meaning is not.
* The traffic-map dimensions depend on the world plugin. A new world falls back to
  the validated search, which is O(cells) but fast.
* Plugin defs that override `LoadExtraData`/`SaveExtraData` and events without a length
  prefix are handled as listed in §4.2, §4.5 and §9. A third-party plugin with its own
  layout would fail loudly at the next marker (`>NT<`, `>VM<`, …).
* The samples contain no out-objects, names, or non-empty event payloads other than
  VisitVIP, Maru_Reggae and BlackOut. Those paths follow the decompiled code but have
  not been exercised against real data.

## 14. Function index

| Function | Binary | Address |
|---|---|---|
| doc load (`OnOpenDocument` body) | T2.exe | 0x419a50 |
| doc save | T2.exe | 0x41b23f |
| pool read (doc vtbl+0x1e0) | T2.exe | 0x41c910 |
| T2Archive Read / Write / ReadPStr / ReadSRect | T2DLL | 0x10016c50 / 0x10016ce0 / 0x100171c0 / 0x10017360 |
| CPluginInfo::Read / Write | T2DLL | 0x10010720 / 0x10010830 |
| T2DateTime(T2Archive&) / Write | T2DLL | 0x1001c910 / 0x1001cc10 |
| T2PeopleArrayList::Read, T2People::LoadSelf/SaveSelf, T2Matter::LoadSelf | T2DLL | 0x1004b900, 0x1004a810/0x1004a9d0, 0x1003bfc0 |
| T2FloorInfo::Read / Write | T2DLL | 0x10033db0 / 0x10033f00 |
| T2Tenant::LoadSelf / SaveSelf, T2Equip::LoadSelf | T2DLL | 0x1005e820 / 0x1005eb50, 0x1002ef60 |
| T2Mover::LoadSelf, ReadMMList, T2ElvModuleList::Read, T2ElevatorModule::LoadSelf | T2DLL | 0x1003f400, 0x10041000, 0x1002eaf0, 0x1002e320 |
| T2Request::LoadSelf, T2OutObj::LoadSelf, T2OutsideInfo::Read | T2DLL | 0x10053660, 0x100462e0, 0x10047790 |
| T2CrossEquipArray::ReadCEArray, T2RequestIDArray::ReadReqIDArray, T2StopInfoArray::ReadStopInfoArray | T2DLL | 0x1001c610, 0x10054840, 0x1005cea0 |
| T2Pool ctor, T2PeopleDemandList, T2PeopleTimeZoneList, T2PeopleTypeArray::Read | T2DLL | 0x100502b0, 0x1004bde0, 0x1004de10, 0x1004ebf0 |
| T2TrafficInfo ctor / Read | T2DLL | 0x1006b4f0 / 0x1006b890 |
| T2Settlement(T2Archive&) / Write | T2DLL | 0x100591d0 / 0x10059370 |
| T2MoviePluginList::Read, T2TowerEvent::Read, T2NameList::Read, T2Name(T2Archive&) | T2DLL | 0x10043a10, 0x10069fc0, 0x10044e60, 0x10044000 |
| WorldDef::Read/WriteStatisticsProc | Kegon.dll | 0x10015360 / 0x10015340 |

The decompiled C and the vtable dumps are in `~/yoottower-decomp/save-agent/decomp/`.
In that copy, the T2Archive operators were renamed `ar_rd_u16` and so on, so the
overloads can be told apart: Ghidra prints `char&` and `char*` identically.

## 15. OpenSky import and export (`source/BldLoader.*`, `source/BldExport.*`)

`BldLoader` is a C++ port of bld.py with the same symmetric reader/writer, so every save
listed above rewrites byte-identical. `--load-game X.bld` (and the Load dialog) imports a
save into YootTower mode: `toTowerXml` resolves each record's numeric item type through
the save's plugin table and the shipped catalog (`ItemTypeCatalog.h`) to a plugin stem,
then through `yootPrototypeLinks()` to an OpenSky prototype. Positions use
`x = unit - width/2` and `floor = groundLine - bottomRow`.

| Imported | How |
|---|---|
| world, star level, VIP flags (doc+0xa0) | `world`, `rating = level - 1`, `yootVipFlags` |
| date | `time = (year-1)*12 + month0 + minutes/1440` (one YootTower month is one day) |
| funds, previous funds | dollars (x100); `lastQuarterBalance` and the period's net change |
| Floor.t2p | `floor` (one slab per level; several records on a level are coalesced), variation 1 = `lobby` with its height |
| tenants with a linked prototype | footprint must match the prototype; 1..300 `estimate` -> `yootEstimate` (+ evaluation/3); status >= 10 -> occupied / let; filled belong-list slots -> office population; shop price index -> `rentClass` |
| 16xx shops without a link (Ice Cream Stand, ...) | the generic `shop` when the footprint fits (reported) |
| Simple / Service / Large elevators | span from `length` (the shaft rect adds a machine and a pit row), unserviced floors from the request-id pairs, every car's position and home |
| stairs / escalators | one flight per stair module rect (no sample contains one, untested) |
| Billboard out-objects | `billboard` at `x = left - width/2`, `y = groundLine - bottom`; the ad's sub-plugin mID resolves to its SP plugin file (`subPluginLookup`), with the days left |

Dropped and reported: tenants without an OpenSky counterpart (in the samples the 7
Restrooms and the Apartment), people (the crowd re-simulates; OpenSky
counts an occupied office's 6 staff at once, so populations differ), the visitor pool,
traffic map, elevator stop tables and schedules, the per-category settlement totals,
names, movies and tower-event state. The game speed is always imported as 1x.

`BldExport` (`BldTool export IN.tower TEMPLATE.bld OUT.bld`) writes an OpenSky YootTower
tower back: floors, lobbies, offices (Office.t2p), shops (as Ice Cream Stands) and Simple
Elevators with cars, stop lists, waiting-area requests, unit map, cross-equip links and
room numbers laid out as the game does. It reproduces the sample's request table exactly. Billboards are written as the game
writes them (oracle save `save2-agent/saves/billboard.bld`): out-object type **2000** (the
OODF tool type; the plugin table lists ATTR 2100), area 20x2, status 11 with the ad's
`u32` mID and `s16` days left as extra data, or status 0 and n = 0 for an empty board, and
the outside map filled at LArray index `row * hRange + unit` (1-based, so file position
`row * hRange + unit - 1`). The unit map is not touched. An export of the imported save
loads in T2.exe with the GAMEWORKS board on the facade (`save2-agent/shots/g0.png`).
The world-wide sections (plugin table, pool, movies, events, palettes, world statistics)
are copied from a template save of the same world. Both an export of the imported
Sample Tokyo Map and an OpenSky-authored Hawaii tower load and run in T2.exe under the
oracle (screenshots in `~/yoottower-decomp/bld-agent/`).

**Kegon.** Its unit map carries the world's FINF mask: `T2UnitInfo::InitMask` sets **0x8000
in the first u16** of every unbuildable cell (5849 of 8448 cells: the cliff left of the
shaft, the right edge and the two bottom rows), and the game saves the map with the bit.
The exporter ORs it back from Kegon.t2p's FINF resource (`YootWorld::unbuildable`), which
matches the oracle saves cell for cell, and writes Kegon's own KFloor (1010) and KElev
(1818) records; KElev's extra data is 2 x 10 records of three s16, fresh `(0, 0, 6)`
(FUN_100116f0), where StdElev has two. Restrooms fall back to KRestroom. An import of a
game-written Kegon save exports back with the same flags and items, the original loads
the export, and its own re-save round-trips (`save2-agent/saves-raw/kegon_resaved.bld`).
