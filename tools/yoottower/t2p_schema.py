#!/usr/bin/env python3
"""YootTower (.t2p) plugin resource schema: reference decoder + byte-exact encoder.

Every PE-format YootTower plugin is a Win32 DLL whose data lives in custom resources. The game
runtime (T2DLL.dll) reads the binary ones through `CResFile` (T2DLL 0x10010ca0..0x10011990):

  * A resource whose first byte is 0x01 is BINARY (CResFile::SetupResource 0x10011230); the
    0x01 is consumed. Otherwise it is TEXT (whitespace/comma separated tokens, `(...)` comments
    skipped, strings run to end of line - CResFile::Token 0x10011290).
  * Binary: EVERY integer extraction (uchar, short, ushort, int, uint, long, ulong) consumes one
    4-byte little-endian slot (operator>> 0x10011310..0x10011650). A uchar/short field therefore
    keeps 3/2 "high" bytes that the engine ignores; we preserve them (`<name>__hi`).
  * Binary strings (operator>>(char*) 0x10011830 / CString 0x10011940): length byte n, then n
    bytes of text, then one terminator byte (normally 0) - n+2 bytes total.
  * RECT binary order is top,left,bottom,right (0x10011730); POINT is y,x (0x100116e0).
  * Resource *type* names are case-insensitive 4cc's: the engine opens e.g. 0x546d4466 which
    CResFile::OpenResource(HINSTANCE,int,int) (0x10010cf0) byte-swaps into "TmDf" == "TMDF".

This module knows a schema (ordered field list mirroring the T2DLL constructor that consumes the
resource) for every resource type found in the 105 PE plugins, plus structured codecs for the
non-CResFile types (OBJMAP/T2DLG text, RT_BITMAP DIB, WAVE RIFF, Mac 'clut' PALETTE, RT_STRING,
RT_VERSION). decode() -> JSON-safe dict; encode(dict) -> the original bytes, byte for byte.

4cc fields are shown in Mac source order (the u32 constant the code compares against), e.g.
ATTR bytes "GRUB" -> "BURG", DESC bytes "lPnT" -> "TnPl". Non-printable 4cc values stay ints.

Usage:
  python3 -I tools/yoottower/t2p_schema.py dump <plugin.t2p> [--blobs] > plugin.json
  python3 -I tools/yoottower/t2p_schema.py check <plugin.t2p>     # round-trip every resource
  python3 -I tools/yoottower/t2p_schema.py hex <plugin.t2p> <TYPE> [id]
"""
import base64
import json
import struct
import sys

__all__ = ['decode_resource', 'encode_resource', 'read_resources', 'SCHEMAS', 'RESOURCE_INFO']

ENC = 'cp1252'


class SchemaError(Exception):
    pass


# --------------------------------------------------------------------------------------------
# PE resource directory reader (no dependencies)
# --------------------------------------------------------------------------------------------
_RT = {1: 'RT_CURSOR', 2: 'RT_BITMAP', 3: 'RT_ICON', 4: 'RT_MENU', 5: 'RT_DIALOG', 6: 'RT_STRING',
       9: 'RT_ACCELERATOR', 10: 'RT_RCDATA', 14: 'RT_GROUP_ICON', 16: 'RT_VERSION'}


def read_resources(data):
    """Return [(type, id, lang, bytes)] for every leaf of the PE resource tree."""
    u16 = lambda o: struct.unpack_from('<H', data, o)[0]
    u32 = lambda o: struct.unpack_from('<I', data, o)[0]
    if data[:2] != b'MZ':
        raise SchemaError('not a PE file')
    pe = u32(0x3c)
    nsec, osz = u16(pe + 6), u16(pe + 20)
    opt = pe + 24
    magic = u16(opt)
    ddir = opt + (96 if magic == 0x10b else 112)
    rsrc_rva = u32(ddir + 2 * 8)
    secs = []
    for i in range(nsec):
        s = opt + osz + 40 * i
        secs.append((u32(s + 12), max(u32(s + 8), u32(s + 16)), u32(s + 20)))

    def off(rva):
        for va, vs, raw in secs:
            if va <= rva < va + vs:
                return rva - va + raw
        raise SchemaError(f'rva {rva:#x} outside sections')

    base = off(rsrc_rva)
    out = []

    def name(e):
        if e & 0x80000000:
            o = base + (e & 0x7fffffff)
            return data[o + 2:o + 2 + 2 * u16(o)].decode('utf-16le')
        return e

    def walk(o, path):
        n = u16(o + 12) + u16(o + 14)
        for k in range(n):
            e = o + 16 + 8 * k
            nm, tgt = u32(e), u32(e + 4)
            p = path + [name(nm)]
            if tgt & 0x80000000:
                walk(base + (tgt & 0x7fffffff), p)
            else:
                d = base + tgt
                fo = off(u32(d))
                t = p[0]
                if isinstance(t, int):
                    t = _RT.get(t, f'#{t}')
                out.append((t, p[1], p[2], bytes(data[fo:fo + u32(d + 4)])))

    walk(base, [])
    return out


# --------------------------------------------------------------------------------------------
# 4cc helpers
# --------------------------------------------------------------------------------------------
def fourcc_to_json(v):
    b = struct.pack('>I', v)
    if all(32 <= c < 127 for c in b):
        return b.decode('ascii')
    return v


def fourcc_from_json(v):
    if isinstance(v, str):
        if len(v) != 4:
            raise SchemaError(f'bad 4cc {v!r}')
        return struct.unpack('>I', v.encode('ascii'))[0]
    return int(v) & 0xffffffff


# --------------------------------------------------------------------------------------------
# CResFile stream codec
# --------------------------------------------------------------------------------------------
class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def left(self):
        return len(self.d) - self.p

    def slot(self):
        if self.p + 4 > len(self.d):
            raise SchemaError(f'slot past end at {self.p}')
        v = struct.unpack_from('<I', self.d, self.p)[0]
        self.p += 4
        return v

    def pstr(self):
        if self.p >= len(self.d):
            raise SchemaError('string past end')
        n = self.d[self.p]
        if self.p + 2 + n > len(self.d):
            raise SchemaError(f'string len {n} past end at {self.p}')
        s = self.d[self.p + 1:self.p + 1 + n]
        term = self.d[self.p + 1 + n]
        self.p += n + 2
        return s, term


class Writer:
    def __init__(self):
        self.b = bytearray()

    def slot(self, v):
        self.b += struct.pack('<I', v & 0xffffffff)

    def pstr(self, s, term=0):
        if len(s) > 255:
            raise SchemaError('string longer than 255 bytes')
        self.b += bytes([len(s)]) + s + bytes([term])


def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def str_to_json(b):
    return b.decode(ENC, 'surrogateescape')


def str_from_json(s):
    return s.encode(ENC, 'surrogateescape')


# Field spec: (name, kind[, arg[, subfields]])
#   kinds: i32 u32 hex 4cc u8 s16 str rect point  - scalar slot/str types
#          list (arg = count source: field name | int | '#prefix' = inline i32 count) subfields
#          raw_tail - rest of resource (only as last field)
def _read_fields(r, fields, ctx):
    out = {}
    for f in fields:
        name, kind = f[0], f[1]
        if kind == 'i32':
            out[name] = _s32(r.slot())
        elif kind in ('u32', 'hex'):
            out[name] = r.slot()
        elif kind == '4cc':
            out[name] = fourcc_to_json(r.slot())
        elif kind in ('u8', 's16'):
            v = r.slot()
            if kind == 'u8':
                out[name] = v & 0xff
                hi = v >> 8
            else:
                lo = v & 0xffff
                out[name] = lo - 0x10000 if lo & 0x8000 else lo
                hi = v >> 16
            if hi:
                out[name + '__hi'] = hi
        elif kind == 'str':
            s, term = r.pstr()
            out[name] = str_to_json(s)
            if term:
                out[name + '__term'] = term
        elif kind == 'rect':
            t, l, b, rr = (_s32(r.slot()) for _ in range(4))
            out[name] = {'top': t, 'left': l, 'bottom': b, 'right': rr}
        elif kind == 'point':
            y, x = _s32(r.slot()), _s32(r.slot())
            out[name] = {'y': y, 'x': x}
        elif kind == 'list':
            src = f[2]
            if isinstance(src, int):
                n = src
            elif src == '#prefix':
                n = _s32(r.slot())
                out[name + '_count'] = n
            else:
                n = ctx.get(src, out.get(src))
                if n is None:
                    raise SchemaError(f'count {src} unknown')
            if n < 0 or n > 100000:
                raise SchemaError(f'bad count {n} for {name}')
            lst = []
            for i in range(n):
                sub_ctx = dict(ctx)
                sub_ctx.update(out)
                lst.append(_read_fields(r, f[3], sub_ctx))
            out[name] = lst
        elif kind == 'raw_tail':
            out[name] = r.d[r.p:].hex()
            r.p = len(r.d)
        else:
            raise SchemaError(f'kind {kind}')
        ctx_val = out.get(name)
        if isinstance(ctx_val, int):
            ctx[name] = ctx_val
    return out


def _write_fields(w, fields, obj, ctx):
    for f in fields:
        name, kind = f[0], f[1]
        if kind in ('i32', 'u32', 'hex'):
            w.slot(int(obj[name]))
        elif kind == '4cc':
            w.slot(fourcc_from_json(obj[name]))
        elif kind == 'u8':
            w.slot((int(obj[name]) & 0xff) | (int(obj.get(name + '__hi', 0)) << 8))
        elif kind == 's16':
            w.slot((int(obj[name]) & 0xffff) | (int(obj.get(name + '__hi', 0)) << 16))
        elif kind == 'str':
            w.pstr(str_from_json(obj[name]), int(obj.get(name + '__term', 0)))
        elif kind == 'rect':
            v = obj[name]
            for k in ('top', 'left', 'bottom', 'right'):
                w.slot(int(v[k]))
        elif kind == 'point':
            w.slot(int(obj[name]['y']))
            w.slot(int(obj[name]['x']))
        elif kind == 'list':
            lst = obj[name]
            src = f[2]
            if src == '#prefix':
                w.slot(len(lst))
            elif isinstance(src, int):
                if len(lst) != src:
                    raise SchemaError(f'{name} needs exactly {src} entries')
            else:
                want = ctx.get(src, obj.get(src))
                if want is not None and int(want) != len(lst):
                    raise SchemaError(f'{name}: count field {src}={want} but {len(lst)} entries '
                                      f'(call normalize())')
            for e in lst:
                sub = dict(ctx)
                sub.update({k: v for k, v in obj.items() if isinstance(v, int)})
                _write_fields(w, f[3], e, sub)
        elif kind == 'raw_tail':
            w.b += bytes.fromhex(obj.get(name, ''))
        v = obj.get(name)
        if isinstance(v, int):
            ctx[name] = v


# --------------------------------------------------------------------------------------------
# Schemas (field order == T2DLL constructor read order). Addresses are T2DLL.dll VAs.
# --------------------------------------------------------------------------------------------
ATTR = [  # CFilePlugin::CFilePlugin 0x1000df20, T2PluginSpecifier::Initialize 0x1004fd00
    ('mID', '4cc'),            # unique plugin id (Mac 4cc), e.g. 'BURG'; GetID 0x10003670
    ('mAttrib', 'hex'),        # GetAttrib 0x10003680
    ('mDependID', '4cc'),      # '****' = none, else mID of a required plugin ('FRON' for rooms)
    ('mGameLevel', 'i32'),     # star level that unlocks the item
    ('mSecretItem', 'i32'),
    ('mItemType', 'i32'),      # == ToolDef mToolType (e.g. 1301); GetItemType 0x100036c0
    ('mMustItem', 'i32'),
    ('mPluginName', 'str'),
]

DESC = [  # T2PluginSpecifier::Initialize 0x1004fd00 (+0x18, +0x28)
    ('mType', '4cc'),          # plugin class: TnPl MvPl OoPl WdPl TOPL
    ('mVersion', 'i32'),       # T2PluginLoader::GetNext 0x1004fa70 only accepts 3
]

TOOL_DEF = [  # T2ToolDef::T2ToolDef 0x10065540
    ('mToolType', 'i32'),      # +0x50 GetToolType; == ATTR.mItemType
    ('mWidth', 'i32'),         # +0x54 footprint width (units), QueryCursor/CalcEquipRect
    ('mHeight', 'i32'),        # +0x58 footprint height (floors)
    ('mLevel', 'i32'),         # +0x5c no T2DLL/T2.exe reader found (always 1)
    ('mAttribute', 'hex'),     # +0x60 GetAttribute / IsSetAttribute bitfield
    ('mCategoryHelp', 'str'),  # +0x6c category description
    ('mCategoryName', 'str'),  # +0x68 GetCategoryName (toolbox group label)
    ('mCategory', 'i32'),      # +0x64 GetCategory (toolbox group no.)
    ('mToolNo', 'i32'),        # +0x70 GetToolNo (position in group); SortKey=cat<<16|no
    ('_skip1', 'i32'),         # read & discarded
    ('_skip2', 'i32'),         # read & discarded
    ('mSubPluginType', '4cc'), # +0x108 GetSubPluginType ('BoPI' billboard ads, 'MoPI' ...)
    ('_skip3', 'i32'),         # read & discarded (legacy res id, 127/128)
    ('_skip4', 'i32'),         # read & discarded (legacy res id, 300)
    ('mValiationCnt', 'i32'),  # +0x74 number of variations (<=4; OuterObjDef forces 1)
    ('variations', 'list', 'mValiationCnt', [
        ('mPriceString', 'str'),     # +0x78[i] help "Cons. Cost" text (T2ToolHelpWnd 0x100669e0)
        ('mOutMoneyString', 'str'),  # +0x88[i] help "Maint. Cost" text
        ('mHelp', 'str'),            # +0x98[i] help body text
        ('mHelpTitle', 'str'),       # +0xb8[i] help window title
        ('mName', 'str'),            # +0xc8[i] GetName/GetToolName
        ('mPrice', 'i32'),           # +0xd8[i] CalcPayment (x$100)
        ('mOutMoney', 'i32'),        # +0xa8[i] GetOutMoney (maintenance per period, x$100)
        ('_skip5', 'i32'),           # read & discarded (legacy res ids 128..130)
        ('_skip6', 'i32'),
        ('_skip7', 'i32'),
        ('mConsumptionPower', 'i32'),  # +0xf0[i] GetConsumptionPower
    ]),
]

EQUIP_DEF = [  # T2EquipDef::T2EquipDef 0x1002f240
    ('mCapacity', 'i32'),      # +0x11c GetCapacity
    ('mStress', 'i32'),        # +0x120 (no reader in T2DLL/T2.exe)
    ('_skip8', 'i32'),         # read & discarded
    ('equipVariations', 'list', 'mValiationCnt', [
        ('mOutSpan', 'i32'),   # +0x154 (only last value kept; TenantDef +0x194 = (last==0))
        ('_skip9', 'i32'),
        ('_skip10', 'i32'),
    ]),
    ('mInSpan', 'i32'),        # +0x134 (no reader found)
    ('mBuildDays', 'i32'),     # +0x138 (no reader found)
    ('mNumOfInMoney', 'i32'),  # +0x13c GetNumOfInMoney (clamped to 4 in memory)
    ('inMoney', 'list', 'mNumOfInMoney', [
        ('mInMoney', 'i32'),   # +0x144[i] GetInMoney(i) income per price index (x$100)
    ]),
]

TENANT_DEF = [  # T2TenantDef::T2TenantDef 0x100623c0
    ('mRegistID', 'u32'),      # +0x19c GetRegistID (tenant registry kind; T2Tenant::IsRegist)
    ('mDownFlag', 'u32'),      # +0x1a0 GetDownFlag
    ('mNumOfKind', 'i32'),     # +0x158 GetNumOfKind
    ('mNumOfPattern', 'i32'),  # +0x15c GetNumOfPattern
    ('mBelongCapacity', 'i32'),  # +0x160 GetBelongCapacity (residents/workers)
    ('mSenseOfNoise', 'i32'),  # +0x1a4 GetSenseOfNoise (UpdateResidencialEstimate)
    ('mEntranceV', 'i32'),     # +0x164
    ('mEntranceH', 'i32'),     # +0x168
    ('mExitV', 'i32'),         # +0x16c
    ('mExitH', 'i32'),         # +0x170
    ('mOpenTime', 'i32'),      # +0x174 T2Tenant::GetOpenTime (minutes from 0:00)
    ('mCloseTime', 'i32'),     # +0x178 T2Tenant::GetCloseTime
    ('mBindTime', 'i32'),      # +0x17c T2Tenant::GetBindTime; customer-unit divisor
    ('mMaxBuildCount', 'i32'),  # +0x104 T2ToolDef::GetMaxBuildCount (0 = no limit)
    ('_skip11', 'u32'),        # read & discarded (legacy res ids 128/129)
    ('_skip12', 'u32'),
    ('_skip13', 'u32'),
    ('mMovieName', 'str'),     # +0x184 CString (QuickTime movie name; empty in all plugins)
]

MOVER_DEF = [  # T2MoverDef::T2MoverDef 0x1003ffb0
    ('mAddSize', 'rect'),      # +0x158 GetAddSize (shaft margin added around cars)
    ('mMaxLength', 'i32'),     # +0x168 GetMaxLength (max floors spanned)
    ('mMinLength', 'i32'),     # +0x16c GetMinLength
    ('mMaxStop', 'i32'),       # +0x170 GetMaxStop
    ('mScore', 'i32'),         # +0x1a8 GetScore (route score; T2Mover::CalcScore)
    ('mDefaultWait', 'i32'),   # +0x174 GetDefaultWait
    ('mSpeed', 'list', 3, [('v', 'i32')]),  # +0x178[3] GetSpeed(0..2)
    ('mMaxModule', 'i32'),     # +0x184 GetMaxModule (max cars)
    ('mModuleUnitH', 'i32'),   # +0x188 used only by plugin code
    ('mModuleUnitV', 'i32'),   # +0x18c used only by plugin code
    ('mModuleParam1', 'i32'),  # +0x190 used only by plugin code
    ('mModuleParam2', 'i32'),  # +0x194 used only by plugin code
    ('mModuleInset', 'rect'),  # +0x198 GetModuleInset
    ('_skip14', 'i32'),        # read & discarded (legacy sound ids 6001/6002)
    ('_skip15', 'i32'),
    ('_skip16', 'i32'),
    ('_skip17', 'i32'),
]

MEMBER_TABLE = [  # T2TenantMemberTableDef::T2TenantMemberTableDef 0x100630a0 / 0x100631e0
    ('mEconoType', 'hex'),     # +0x08 GetEconoType (bit4 = IsCheckOnlyFirstEconoType); -1 none
    ('mSpecialFlag', 'hex'),   # +0x0c GetSpecialFlag
    ('mDayParamID', 'i32'),    # DPDF resource id (0 = none) -> +0x14 T2DayParamDef
    ('mPlaceParamID', 'i32'),  # PLDF resource id (0 = none) -> +0x18 T2PlaceParamDef
    ('mNumOfElem', 'i32'),     # +0x20 GetNumOfElem
    ('members', 'list', 'mNumOfElem', [  # T2TenantMemberDef::Initialize 0x10063000, 0x30 B each
        ('mName', 'str'),               # +0x04 char[16] GetName
        ('mDemandType', 'i32'),         # +0x14 GetDemandType (-1 any)
        ('mStartSilhouetteType', 'i32'),  # +0x18 (SLDF silhouette type range start)
        ('mEndSilhouetteType', 'i32'),  # +0x1c
        ('mOptionType', 'i32'),         # +0x20 GetOptionType
        ('mPercent', 'u32'),            # +0x24 GetPercent (Rate = Percent*units*k)
        ('mNumOfPeople', 'u32'),        # +0x28 GetNumOfPeople
    ]),
]

DAY_PARAM = [  # T2DayParamDef::T2DayParamDef 0x1001d240; GetPercent(EDay,EWhether) 0x1000d280
    ('mPercent', 'list', 2, [  # [EDay]
        ('byWhether', 'list', 2, [('v', 'u32')]),  # [EWhether]; rate = v * 0.01
    ]),
]

PLACE_PARAM = [  # T2PlaceParamDef::T2PlaceParamDef 0x1004ed80; GetPercent/GetScore(EPlace)
    ('places', 'list', 3, [  # [EPlace]
        ('mPercent', 'u32'),
        ('mScore', 's16'),     # short in a 4-byte slot
    ]),
]

PEOPLE_ANIME = [  # T2PeopleAnimeDef::T2PeopleAnimeDef 0x1004ad60 (+ Elem::Initialize 0x1004aee0)
    ('mParam1', 'i32'),        # +0x04
    ('mParam2', 'i32'),        # +0x08
    ('mNumOfElem', 'i32'),     # +0x0c
    ('elems', 'list', 'mNumOfElem', [
        ('mPosition', 'i32'),  # +0x04
        ('mPattern', 'i32'),   # +0x08
        ('mSilhouette', 'i32'),  # +0x0c
        ('mAnimeID', 'i32'),   # +0x10
    ]),
]

SILHOUETTE_LIST = [  # T2PeopleLoader::LoadSilhouette 0x1004d4d0 -> T2SilhouetteDef 0x10059f00
    ('silhouettes', 'list', '#prefix', [
        ('mSilhouetteType', 'i32'),  # +0x58 GetSilhouetteType / GetSortKey
        ('mWidth', 'i32'),           # +0x5c GetWidth (units)
        ('mRouteType', 'i32'),       # +0x60 GetRouteType
        ('mBitmapID', 'i32'),        # +0x64 RT_BITMAP id (16x36 frames, 6 rows)
        ('mName', 'str'),            # +0x68 GetName
    ]),
]

PEOPLE_LIST = [  # T2PeopleLoader::LoadPeople 0x1004d600 -> T2PeopleDef 0x1004bc00
    ('people', 'list', '#prefix', [
        ('mPeopleType', 'i32'),      # +0x50 (T2MatterDef) GetPeopleType
        ('mAttribute', 'i32'),       # +0x54 (T2MatterDef) GetAttribute
        ('mSilhouetteType', 'i32'),  # +0x58 (T2MatterDef) AssignSilhouette
        ('mStressMax', 'i32'),       # +0x60 GetStressMax
        ('mStressBlueLimit', 'i32'),   # +0x64
        ('mStressYellowLimit', 'i32'),  # +0x68
    ]),
]

WORD_LIST = [  # T2WordDefArray::GetWords 0x1006e2f0 -> T2WordDef::Read 0x1006e210
    ('words', 'list', '#prefix', [
        ('mSexMask', 'hex'),     # +0x04 bit per sex (1<<sex) - T2WordDef::GetWords 0x1006e270
        ('mAgeMask', 'hex'),     # +0x08 bit per age class (silhouette>>3)
        ('mFlags', 'u32'),       # +0x0c exact match: b0 at-own-tenant, b1/b2 mood, b3 home, b4 VIP-ok
        ('mSpecial', 'u32'),     # +0x10 exact match: 0 normal, 1/2 status, 3-5 VIP, 6 Maru, 7 Reggae
        ('mText', 'str'),        # +0x14 the speech-bubble line
    ]),
]

STRTABLE = [  # LoadStringTable 0x10012af0
    ('strings', 'list', '#prefix', [('s', 'str')]),
]

WATR = [  # T2WorldDef::T2WorldDef 0x1006e4e0 (opens 'WAtr' id 128)
    ('_skip', 'list', 6, [('v', 'i32')]),
    ('mParam24c', 'i32'), ('mParam250', 'i32'), ('mParam254', 'i32'),
    ('mText', 'str'),          # +0x258
]

PALETTE_ANIME = [  # T2PaletteAnimeDef::T2PaletteAnimeDef 0x10047e60 (PTDF)
    ('mNumOfTowerColor', 'i32'), ('mNumOfAnimColor', 'i32'), ('mAnimStartIndex', 'i32'),
    ('mNumOfRefrectColor', 'i32'), ('mNumOfSkyColor', 'i32'), ('mNumOfSEKind', 'i32'),
    ('mNumOfSEColor', 'i32'), ('mSEInterval', 'i32'),
]

POOL_DEF = [  # T2WorldDef::MakePoolDef 0x1006eb10 -> FUN_10050e00 (PODF)
    ('mParam08', 'i32'),
    ('mGradeID', 'list', 5, [('v', 'i32')]),  # PGDF ids per game level
    ('mParam20', 'u8'),
    ('mNumOfDemand', 'u8'),
    ('demand', 'list', 'mNumOfDemand', [('mParam0', 'i32'), ('mParam4', 'u8')]),
]

POOL_GRADE = [  # T2Pool::GameLevelChanged 0x10050390 -> FUN_10050c70 (PGDF)
    ('demands', 'list', '#prefix', [  # FUN_10050be0
        ('zones', 'list', '#prefix', [
            ('mTimeZoneID', 'i32'),  # PZDF id
            ('mRate', 'u32'),
        ]),
    ]),
]

POOL_TIMEZONE = [  # T2PeopleTimeZoneList::Add 0x1004dfd0 -> FUN_10050ad0 (PZDF)
    ('elems', 'list', '#prefix', [
        ('b0', 'u8'), ('b1', 'u8'), ('b2', 'u8'), ('b3', 'u8'),  # 4 uchar slots; +4 = b3*20/100
    ]),
]

SEASON_PARAM = [('mPercent', 'list', 12, [('v', 'u32')])]  # FUN_100585d0 (SPDF, month)
COLD_TABLE = [('mParam', 'u32'), ('mMonth', 'list', 12, [('v', 'u32')])]  # FUN_1001c400 (CODF)
VIPL = [  # 6 ints; T2Maru_Reggae 0x1003a770 / T2VisitVIP::Init 0x1006d360 read only the first
    ('mValue', 'i32'),
    ('_unread', 'list', 5, [('v', 'i32')]),
]
T2EV = [  # T2TowerEvent::T2TowerEvent 0x10068ca0
    ('events', 'list', '#prefix', [('mType', '4cc'), ('mResID', 'i32')]),
]
EVENT_ITEM = [  # T2EventItem::T2EventItem 0x10030050 (base of every *EV resource)
    ('mLevelBit', 'i32'),      # +0x14 game-level mask the event may fire in
    ('mExclusive', 'i32'),     # +0x18 (stored as !=0)
    ('mCycleDay', 'i32'),      # +0x1c (min 1)
    ('mBeginTime', 'i32'),     # +0x20 minutes
    ('mEndTime', 'i32'),       # +0x24 minutes
]
EVENT_SCHEMAS = {
    'FBEV': ('T2FireBurning', EVENT_ITEM + [  # 0x10030c10
        ('m38', 'i32'), ('mNumOfParam', 'i32'),
        ('params', 'list', 'mNumOfParam', [('v', 'i32')])]),
    'BOEV': ('T2BlackOut', EVENT_ITEM + [  # 0x1001b580
        ('m30', 'i32'), ('m34', 'i32'), ('m38', 'i32')]),
    'TREV': ('T2Terrorist', EVENT_ITEM + [  # 0x100638e0
        ('m30', 'i32'), ('m34', 'i32'), ('m38', 'i32')]),
    'SAEV': ('T2SantaClaus', EVENT_ITEM + [('m30', 'i32')]),  # 0x100560b0 (0 -> 32)
    'MAEV': ('T2Maru_Reggae', EVENT_ITEM + [  # 0x1003a770
        ('m30', 'i32'), ('mVIPListID', 'i32'), ('m38', 'i32'), ('m3c', 'i32'),
        ('mMessage', 'str')]),
    'ODEV': ('T2OutDecoration', EVENT_ITEM + [  # 0x10045760
        ('decorations', 'list', '#prefix', [('mID', 'i32'), ('mParam', 'u32')])]),
    'MREV': ('T2MaruReggae transport event (FUN_1003d690)', EVENT_ITEM + [
        ('m74', 'i32'),
        ('transports', 'list', '#prefix', [('m0', 'i32'), ('m1', 'i32')])]),  # T2Transport::Init
    'VIEV': ('T2VisitVIP', EVENT_ITEM + [  # 0x1006cb40, entries via T2VisitVIP::Init 0x1006d360
        ('visits', 'list', '#prefix', [
            ('m30', 'i32'), ('mTenantType', '4cc'), ('m38', 'i32'), ('m3c', 'i32'),
            ('m40', 'i32'), ('m44', 'i32'), ('mVIPListID', 'i32'),
            ('mGoodText', 'str'), ('mBadText', 'str')])]),
}
# World-specific XEvents (T2XEvent 0x10073360 -> T2WorldDef vtable +0x4c, plugin code)
for _ev in ('CLEV', 'CREV', 'BBEV', 'DOEV', 'HSEV', 'TOEV', 'WDEV'):
    EVENT_SCHEMAS[_ev] = ('T2XEvent (world plugin code)', EVENT_ITEM + [('tail', 'raw_tail')])
FLOOR_INFO = [  # T2FloorInfo::InitMask 0x10033d40 (FINF)
    ('mHeader', 'list', 4, [('v', 'i32')]),
    ('tail', 'raw_tail'),      # w*h unit masks (i32 each; nonzero -> 0x8000 flag)
]
BKGND_INFO = [  # T2WorldPlugin::InitBkgndInfoProc 0x10073150 (BINF)
    ('mWidth', 'i32'), ('mHeight', 'i32'), ('mParam', 'i32'),
    ('cells', 'list', 'mHeight', [('row', 'list', 'mWidth', [('v', 'u32')])]),
]
WORLD_DEF = [  # T2WorldDef::T2WorldDef 0x1006e4e0
    ('m50', 'i32'), ('m1f8', 'i32'), ('m1fc', 'i32'), ('m54', 'i32'),
    ('m200', 'rect'), ('m60', 'point'),
    ('m68', 'i32'), ('m6c', 'i32'), ('m70', 'i32'), ('m74', 'i32'), ('m58', 'i32'),
    ('mGroundLevel', 'i32'),  # +0x218 (also bitmap id base: T2EquipDef uses +0x218+1001)
    ('m17c', 'i32'), ('m228', 'i32'),
    ('mNumOfGrade', 'i32'),   # +0x84 (<=8)
    ('grades', 'list', 'mNumOfGrade', [
        ('mParam0', 'u32'), ('mParam1', 'i32'), ('mParam2', 'i32'), ('mParam3', 'i32'),
        ('mParam4', 'i32'), ('mParam5', 'i32')]),
    ('m180', 'i32'), ('m184', 'i32'), ('m188', 'i32'), ('m18c', 'i32'),
    ('_skip', 'list', 10, [('v', 'i32')]),
    ('m148', 'i32'), ('m14c', 'i32'), ('m150', 'i32'), ('m154', 'i32'), ('m158', 'i32'),
    ('mName', 'str'),         # +0x15c
    ('m170', 'u32'), ('m174', 'u32'), ('m178', 'u32'),
    ('_skip2', 'i32'),
    ('m22c', 'list', '#prefix', [('v', 'i32')]),  # up to 8 kept
    ('tail', 'raw_tail'),
]

SCHEMAS = {
    'ATTR': ('CFilePlugin', ATTR),
    'DESC': ('T2PluginSpecifier', DESC),
    'TNDF': ('T2TenantDef', TOOL_DEF + EQUIP_DEF + TENANT_DEF),
    'MVDF': ('T2MoverDef', TOOL_DEF + EQUIP_DEF + MOVER_DEF),
    'OODF': ('T2OuterObjDef', TOOL_DEF + [('_unreadA', 'i32'), ('_unreadB', 'i32')]),
    'TMDF': ('T2TenantMemberTableDef', MEMBER_TABLE),
    'DPDF': ('T2DayParamDef', DAY_PARAM),
    'PLDF': ('T2PlaceParamDef', PLACE_PARAM),
    'PADF': ('T2PeopleAnimeDef', PEOPLE_ANIME),
    'SLDF': ('T2SilhouetteDef[]', SILHOUETTE_LIST),
    'PPDF': ('T2PeopleDef[]', PEOPLE_LIST),
    'WODF': ('T2WordDef[]', WORD_LIST),
    'STRTABLE': ('LoadStringTable', STRTABLE),
    'STRTBL': ('string list (world plugin code)', STRTABLE),
    'WATR': ('T2WorldDef WAtr', WATR),
    'PTDF': ('T2PaletteAnimeDef', PALETTE_ANIME),
    'PODF': ('T2PoolDef', POOL_DEF),
    'PGDF': ('T2PoolGradeDef', POOL_GRADE),
    'PZDF': ('T2PoolTimeZoneDef', POOL_TIMEZONE),
    'SPDF': ('T2SeasonParamDef', SEASON_PARAM),
    'CODF': ('T2ColdTableDef', COLD_TABLE),
    'VIPL': ('VIP list value', VIPL),
    'T2EV': ('T2TowerEvent', T2EV),
    'FINF': ('T2FloorInfo mask', FLOOR_INFO),
    'BINF': ('BkgndInfo', BKGND_INFO),
    'WORLDDEF': ('T2WorldDef', WORLD_DEF),
}
SCHEMAS.update(EVENT_SCHEMAS)

TEXT_TYPES = {'OBJMAP', 'T2DLG', 'TENANTDEF'}
RESOURCE_INFO = {
    'OBJMAP': 'T2ImageObj::AddObject 0x100391c0 (text sprite map, see objmap-format.md)',
    'T2DLG': 'T2Dialog template (text)',
    'TENANTDEF': 'legacy Mac text template (Shift-JIS); no reader in T2DLL',
    'WAVE': 'RIFF WAVE, id = sound id (T2SoundPlayer)',
    'RT_BITMAP': 'Windows DIB (BITMAPINFOHEADER + palette + pixels)',
    'PALETTE': "Mac 'clut' (id 100) or 'pltt' (101+) big-endian; read via FindResourceA(PALETTE)",
    'RT_STRING': 'Win32 string table block (16 x u16-len UTF-16)',
    'RT_VERSION': 'VS_VERSIONINFO (opaque)',
}


# --------------------------------------------------------------------------------------------
# Non-CResFile codecs
# --------------------------------------------------------------------------------------------
def _b64(b):
    return base64.b64encode(b).decode('ascii')


def _unb64(s):
    return base64.b64decode(s)


def decode_text(data):
    t = data.decode(ENC, 'surrogateescape')
    return {'text': t}


def encode_text(obj):
    return str_from_json(obj['text'])


def decode_bitmap(data):
    hs, w, h, planes, bpp, comp, isz, xp, yp, used, imp = struct.unpack_from('<IiiHHIIiiII', data, 0)
    ncol = used or (1 << bpp if bpp <= 8 else 0)
    pal = data[hs:hs + 4 * ncol]
    return {'header': {'biSize': hs, 'width': w, 'height': h, 'planes': planes, 'bitCount': bpp,
                       'compression': comp, 'sizeImage': isz, 'xPelsPerMeter': xp,
                       'yPelsPerMeter': yp, 'clrUsed': used, 'clrImportant': imp},
            'headerExtra': data[40:hs].hex(),
            'palette': [list(pal[i:i + 4]) for i in range(0, len(pal), 4)],
            'pixels': _b64(data[hs + 4 * ncol:])}


def encode_bitmap(obj):
    h = obj['header']
    b = struct.pack('<IiiHHIIiiII', h['biSize'], h['width'], h['height'], h['planes'],
                    h['bitCount'], h['compression'], h['sizeImage'], h['xPelsPerMeter'],
                    h['yPelsPerMeter'], h['clrUsed'], h['clrImportant'])
    b += bytes.fromhex(obj.get('headerExtra', ''))
    b += b''.join(bytes(e) for e in obj['palette'])
    return b + _unb64(obj['pixels'])


def decode_wave(data):
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        return {'raw': _b64(data)}
    out = {'riffSize': struct.unpack_from('<I', data, 4)[0], 'chunks': []}
    p = 12
    while p + 8 <= len(data):
        cid = data[p:p + 4].decode('latin-1')
        n = struct.unpack_from('<I', data, p + 4)[0]
        body = data[p + 8:p + 8 + n]
        ch = {'id': cid, 'size': n}
        if cid == 'fmt ' and n >= 16:
            f = struct.unpack_from('<HHIIHH', body)
            ch['fmt'] = dict(zip(('format', 'channels', 'rate', 'byteRate', 'align', 'bits'), f))
            ch['fmtExtra'] = body[16:].hex()
        else:
            ch['data'] = _b64(body)
        pad = n & 1 and p + 8 + n < len(data)
        if pad:
            ch['pad'] = data[p + 8 + n]
        out['chunks'].append(ch)
        p += 8 + n + (1 if pad else 0)
    out['trailing'] = data[p:].hex()
    return out


def encode_wave(obj):
    if 'raw' in obj:
        return _unb64(obj['raw'])
    b = bytearray(b'RIFF' + struct.pack('<I', obj['riffSize']) + b'WAVE')
    for ch in obj['chunks']:
        if 'fmt' in ch:
            f = ch['fmt']
            body = struct.pack('<HHIIHH', f['format'], f['channels'], f['rate'], f['byteRate'],
                               f['align'], f['bits']) + bytes.fromhex(ch.get('fmtExtra', ''))
        else:
            body = _unb64(ch['data'])
        b += ch['id'].encode('latin-1') + struct.pack('<I', ch['size']) + body
        if 'pad' in ch:
            b.append(ch['pad'])
    return bytes(b) + bytes.fromhex(obj.get('trailing', ''))


def decode_clut(data):
    n16 = struct.unpack_from('>H', data, 0)[0]
    if 16 + 16 * n16 == len(data):  # Mac 'pltt' (PaletteMgr): 16-byte header + 16-byte entries
        ents = []
        for i in range(n16):
            r, g, b, usage, tol = struct.unpack_from('>HHHHH', data, 16 + 16 * i)
            ents.append({'rgb': [r, g, b], 'usage': usage, 'tolerance': tol,
                         'private': data[16 + 16 * i + 10:16 + 16 * i + 16].hex()})
        return {'format': 'pltt', 'header': data[2:16].hex(), 'entries': ents}
    seed, flags, n = struct.unpack_from('>IHH', data, 0)
    ents = []
    for i in range(n + 1):
        ents.append(list(struct.unpack_from('>HHHH', data, 8 + 8 * i)))
    return {'format': 'clut', 'seed': seed, 'flags': flags, 'entries': ents,
            'trailing': data[8 + 8 * (n + 1):].hex()}


def encode_clut(obj):
    if obj.get('format') == 'pltt':
        b = struct.pack('>H', len(obj['entries'])) + bytes.fromhex(obj['header'])
        for e in obj['entries']:
            b += struct.pack('>HHHHH', *e['rgb'], e['usage'], e['tolerance'])
            b += bytes.fromhex(e['private'])
        return b
    b = struct.pack('>IHH', obj['seed'], obj['flags'], len(obj['entries']) - 1)
    b += b''.join(struct.pack('>HHHH', *e) for e in obj['entries'])
    return b + bytes.fromhex(obj.get('trailing', ''))


def decode_strings(data):
    out, p = [], 0
    while p + 2 <= len(data) and len(out) < 16:
        n = struct.unpack_from('<H', data, p)[0]
        out.append(data[p + 2:p + 2 + 2 * n].decode('utf-16le', 'surrogatepass'))
        p += 2 + 2 * n
    return {'strings': out, 'trailing': data[p:].hex()}


def encode_strings(obj):
    b = bytearray()
    for s in obj['strings']:
        e = s.encode('utf-16le', 'surrogatepass')
        b += struct.pack('<H', len(e) // 2) + e
    return bytes(b) + bytes.fromhex(obj.get('trailing', ''))


def _align4(p):
    return (p + 3) & ~3


def _vs_node(data, p):
    """Decode one VS_VERSIONINFO-style node at p (keeps every pad byte for byte-exactness)."""
    wlen, vlen, wtype = struct.unpack_from('<HHH', data, p)
    end = p + wlen
    q = p + 6
    k = q
    while struct.unpack_from('<H', data, k)[0] != 0:
        k += 2
    key = data[q:k].decode('utf-16le')
    q = k + 2
    node = {'key': key, 'type': wtype, 'valueLength': vlen, 'length': wlen}
    a = _align4(q)
    if data[q:a].strip(b'\0'):
        node['pad1'] = data[q:a].hex()
    q = a
    vbytes = vlen * 2 if wtype == 1 else vlen
    val = data[q:q + vbytes]
    if wtype == 1:
        node['value'] = val.decode('utf-16le', 'surrogatepass')
    else:
        node['valueHex'] = val.hex()
    q += vbytes
    children = []
    a = _align4(q)
    if a <= end and data[q:a].strip(b'\0'):
        node['pad2'] = data[q:a].hex()
    if a <= end:
        q = a
    while q + 6 <= end:
        clen = struct.unpack_from('<H', data, q)[0]
        if clen == 0:
            break
        child, nq = _vs_node(data, q)
        children.append(child)
        q = nq
        a = min(_align4(q), end)
        if data[q:a].strip(b'\0'):
            child['padAfter'] = data[q:a].hex()
        q = a
    if children:
        node['children'] = children
    if q < end:
        node['extra'] = data[q:end].hex()
    return node, end


def _vs_encode(node, base_len):
    out = bytearray(struct.pack('<HHH', node['length'], node['valueLength'], node['type']))
    out += node['key'].encode('utf-16le') + b'\0\0'
    pad = (-(base_len + len(out))) & 3
    out += bytes.fromhex(node['pad1']) if 'pad1' in node else b'\0' * pad
    if node['type'] == 1:
        out += node['value'].encode('utf-16le', 'surrogatepass')
    else:
        out += bytes.fromhex(node.get('valueHex', ''))
    if 'children' in node or 'extra' in node or 'pad2' in node:
        pad = (-(base_len + len(out))) & 3
        if base_len + len(out) + pad <= base_len + node['length']:
            out += bytes.fromhex(node['pad2']) if 'pad2' in node else b'\0' * pad
    for c in node.get('children', []):
        out += _vs_encode(c, base_len + len(out))
        if 'padAfter' in c:
            out += bytes.fromhex(c['padAfter'])
        else:
            pad = (-(base_len + len(out))) & 3
            if len(out) + pad <= node['length']:
                out += b'\0' * pad
    out += bytes.fromhex(node.get('extra', ''))
    return bytes(out)


def decode_version(data):
    node, end = _vs_node(data, 0)
    return {'root': node, 'trailing': data[end:].hex()}


def encode_version(obj):
    return _vs_encode(obj['root'], 0) + bytes.fromhex(obj.get('trailing', ''))


# --------------------------------------------------------------------------------------------
# CResFile text mode (e.g. DefaultToolPalette ATTR, world PTDF): decode values for display,
# keep raw text for byte-exact re-encoding.
# --------------------------------------------------------------------------------------------
def _text_values(text, fields):
    toks = []
    p = 0
    t = text
    vals = {}

    def skip():
        nonlocal p
        while p < len(t) and t[p] in ' \t\n\r,(':
            if t[p] == '(':
                while p + 1 < len(t) and t[p + 1] != ')':
                    p += 1
                p += 1
            p += 1

    def token():
        nonlocal p
        skip()
        s = p
        while p < len(t) and t[p] not in ' \t\n\r,(':
            p += 1
        return t[s:p]

    def line():
        nonlocal p
        skip()
        s = p
        while p < len(t) and t[p] not in '\r\n':
            p += 1
        return t[s:p]

    for f in fields:
        name, kind = f[0], f[1]
        if kind in ('i32', 'u32', 'hex', 'u8', 's16'):
            tok = token()
            try:
                vals[name] = int(tok)
            except ValueError:
                vals[name] = tok
        elif kind == '4cc':
            tok = token()
            try:
                vals[name] = fourcc_to_json(int(tok) & 0xffffffff)
            except ValueError:
                vals[name] = tok
        elif kind == 'str':
            vals[name] = line()
        else:  # list / raw tail in text form: remaining whitespace-separated tokens
            rest = []
            while True:
                tok = token()
                if not tok:
                    break
                try:
                    rest.append(int(tok))
                except ValueError:
                    rest.append(tok)
            vals['tokens'] = rest
            break
    return vals


# --------------------------------------------------------------------------------------------
# Public API
# --------------------------------------------------------------------------------------------
def decode_resource(rtype, data):
    """Decode one resource payload into a JSON-safe dict (always re-encodable)."""
    key = rtype.upper() if isinstance(rtype, str) else rtype
    if key in SCHEMAS:
        cls, fields = SCHEMAS[key]
        if data[:1] != b'\x01':
            return {'codec': 'cresfile-text', 'class': cls,
                    'text': str_to_json(data), 'values': _text_values(str_to_json(data), fields)}
        r = Reader(data[1:])
        try:
            fields_out = _read_fields(r, fields, {})
        except SchemaError as e:
            return {'codec': 'opaque', 'class': cls, 'error': str(e), 'raw': _b64(data)}
        out = {'codec': 'cresfile', 'class': cls, 'fields': fields_out}
        if r.left():
            out['trailing'] = r.d[r.p:].hex()
        return out
    if key in TEXT_TYPES:
        return dict(codec='text', **decode_text(data))
    if key == 'RT_BITMAP':
        return dict(codec='dib', **decode_bitmap(data))
    if key == 'WAVE':
        return dict(codec='riff', **decode_wave(data))
    if key == 'PALETTE':
        return dict(codec='clut', **decode_clut(data))
    if key == 'RT_STRING':
        return dict(codec='strings', **decode_strings(data))
    if key == 'RT_VERSION':
        try:
            d = dict(codec='version', **decode_version(data))
            if encode_version(d) == data:
                return d
        except (struct.error, UnicodeDecodeError, IndexError):
            pass
    return {'codec': 'opaque', 'raw': _b64(data)}


def encode_resource(rtype, obj):
    c = obj['codec']
    if c == 'cresfile':
        key = rtype.upper()
        _, fields = SCHEMAS[key]
        w = Writer()
        _write_fields(w, fields, obj['fields'], {})
        return b'\x01' + bytes(w.b) + bytes.fromhex(obj.get('trailing', ''))
    if c == 'cresfile-text':
        return str_from_json(obj['text'])
    if c == 'text':
        return encode_text(obj)
    if c == 'dib':
        return encode_bitmap(obj)
    if c == 'riff':
        return encode_wave(obj)
    if c == 'clut':
        return encode_clut(obj)
    if c == 'strings':
        return encode_strings(obj)
    if c == 'version':
        return encode_version(obj)
    if c == 'opaque':
        return _unb64(obj['raw'])
    raise SchemaError(f'unknown codec {c}')


def normalize(rtype, obj):
    """Make count fields agree with list lengths after editing (in place)."""
    if obj.get('codec') != 'cresfile':
        return obj
    _, fields = SCHEMAS[rtype.upper()]

    def fix(fs, o):
        for f in fs:
            if f[1] == 'list':
                lst = o.get(f[0], [])
                if isinstance(f[2], str) and f[2] != '#prefix':
                    if f[2] in o:
                        o[f[2]] = len(lst)
                elif f[2] == '#prefix':
                    o[f[0] + '_count'] = len(lst)
                for e in lst:
                    fix(f[3], e)
    fix(fields, obj['fields'])
    # cross-struct: EquipDef variations follow ToolDef mValiationCnt
    fl = obj['fields']
    if 'equipVariations' in fl:
        fl['mValiationCnt'] = len(fl['variations'])
    return obj


def decode_plugin(data, blobs=True):
    res = []
    for t, i, lang, payload in read_resources(data):
        d = decode_resource(t, payload)
        if not blobs:
            for k in ('pixels', 'raw'):
                if k in d:
                    d[k] = f'<{len(payload)} bytes>'
            if d.get('codec') == 'riff':
                for ch in d['chunks']:
                    if 'data' in ch:
                        ch['data'] = f"<{ch['size']} bytes>"
        res.append({'type': t, 'id': i, 'lang': lang, 'size': len(payload), **d})
    return res


def _main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    cmd, path = argv[1], argv[2]
    data = open(path, 'rb').read()
    if cmd == 'dump':
        json.dump(decode_plugin(data, '--blobs' in argv), sys.stdout, indent=1, ensure_ascii=False)
        print()
    elif cmd == 'check':
        bad = 0
        for t, i, lang, payload in read_resources(data):
            d = json.loads(json.dumps(decode_resource(t, payload)))
            ok = encode_resource(t, d) == payload
            bad += not ok
            print(f"{'ok ' if ok else 'BAD'} {t}:{i} {d['codec']} {len(payload)}")
        sys.exit(1 if bad else 0)
    elif cmd == 'hex':
        for t, i, lang, payload in read_resources(data):
            if t == argv[3] and (len(argv) < 5 or str(i) == argv[4]):
                print(f'== {t}:{i} {len(payload)} bytes')
                for o in range(0, len(payload), 16):
                    c = payload[o:o + 16]
                    print(f"{o:04x} {c.hex(' '):<48} "
                          f"{''.join(chr(x) if 32 <= x < 127 else '.' for x in c)}")


if __name__ == '__main__':
    _main(sys.argv)
