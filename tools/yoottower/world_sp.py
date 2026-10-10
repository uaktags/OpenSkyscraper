#!/usr/bin/env python3
"""Reference parser/writer for YootTower World plugins and SP-format plugins.

Covers:
  * World plugins (DESC ``lPdW``: Tokyo.t2p, Hawai.t2p, Kegon.t2p) -- PE DLLs
    whose ``.rsrc`` carries WORLDDEF/WATR/BINF/FINF/PTDF/PODF/CODF/SPDF/T2EV
    and the event resources (``*EV``), palettes, bitmaps and sounds.
  * SP-format plugins (magic ``"SP"``: MV_* movies ``lPoM``, Pl* hall events
    ``lPeH``, advertisement plugins ``lPdA``).

Every binary layout below mirrors a T2DLL.dll constructor; addresses are given
in comments and in doc/yoottower/world-and-sp-formats.md.

CResFile rules (T2DLL CResFile::SetupResource 0x10011230, operator>> 0x10011310..):
  * first byte 0x01 => binary mode, otherwise text mode;
  * binary: every scalar (char/short/int/uint) occupies 4 little-endian bytes;
    RECT is stored top,left,bottom,right; POINT y,x; strings are
    <u8 len><len bytes><NUL>;
  * text: tokens separated by space/tab/CR/LF/comma, "(...)" is a comment.

Usage (run with ``python3 -I``):
  world_sp.py dump <file.t2p> [--json]      decode a World or SP plugin
  world_sp.py roundtrip <file.t2p>          decode+encode every decoded
                                            resource (and SP container),
                                            report byte-identity
Requires ``pefile`` for PE plugins only.
"""
import json
import struct
import sys

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

STR_ENC = 'cp932'  # Japanese Windows; ASCII-compatible. surrogateescape keeps it lossless.


def dec_str(b):
    return b.decode(STR_ENC, 'surrogateescape')


def enc_str(s):
    return s.encode(STR_ENC, 'surrogateescape')


def tag_str(u32):
    """u32 read from the file (LE) -> the 4 bytes in file order as text."""
    return struct.pack('<I', u32 & 0xffffffff).decode('latin1')


def str_tag(s):
    return struct.unpack('<I', s.encode('latin1'))[0]


def be_name(u32):
    """Resource-type name the engine uses for a CResFile tag constant (big-endian 4cc)."""
    return struct.pack('>I', u32 & 0xffffffff).decode('latin1')


class FormatError(Exception):
    pass


class BinReader:
    def __init__(self, data):
        if not data or data[0] != 1:
            raise FormatError('not a binary CResFile resource (first byte != 0x01)')
        self.d = data
        self.p = 1

    def i(self):
        if self.p + 4 > len(self.d):
            raise FormatError('read past end at %d' % self.p)
        v = struct.unpack_from('<i', self.d, self.p)[0]
        self.p += 4
        return v

    def u(self):
        return self.i() & 0xffffffff

    def s(self):
        n = self.d[self.p]
        b = self.d[self.p + 1:self.p + 1 + n]
        if self.p + 1 + n >= len(self.d) or self.d[self.p + 1 + n] != 0:
            raise FormatError('pstr at %d not NUL terminated' % self.p)
        self.p += n + 2
        return dec_str(b)

    def left(self):
        return len(self.d) - self.p

    def rest(self):
        r = self.d[self.p:]
        self.p = len(self.d)
        return r


class BinWriter:
    def __init__(self):
        self.b = bytearray(b'\x01')

    def i(self, v):
        self.b += struct.pack('<i', v if v < 0x80000000 else v - 0x100000000)

    u = i

    def s(self, v):
        b = enc_str(v)
        if len(b) > 255:
            raise FormatError('pstr too long')
        self.b += bytes([len(b)]) + b + b'\0'

    def raw(self, b):
        self.b += b

    def bytes(self):
        return bytes(self.b)


# ---------------------------------------------------------------------------
# schema engine: list of (name, kind); kinds:
#   'i' int32, 'u' uint32, 'tag' 4cc (file order), 's' pstr, 'rect', 'point',
#   ('n', name)          -> int count stored in the stream, later used by
#   ('list', countname, schema)
#   ('fixed', n, schema) -> n repetitions
#   ('ints', n)          -> list of n ints
#   ('strs', countname)  -> countname strings
#   ('ilist', countname) -> countname ints
# ---------------------------------------------------------------------------

def _read(r, schema, out):
    for name, kind in schema:
        if kind == 'i':
            out[name] = r.i()
        elif kind == 'u':
            out[name] = r.u()
        elif kind == 'tag':
            out[name] = tag_str(r.u())
        elif kind == 's':
            out[name] = r.s()
        elif kind == 'rect':
            t, l, b, rr = r.i(), r.i(), r.i(), r.i()
            out[name] = {'left': l, 'top': t, 'right': rr, 'bottom': b}
        elif kind == 'point':
            y, x = r.i(), r.i()
            out[name] = {'x': x, 'y': y}
        elif kind[0] == 'ints':
            out[name] = [r.i() for _ in range(kind[1])]
        elif kind[0] == 'list':
            n = out[kind[1]]
            out[name] = [_read(r, kind[2], {}) for _ in range(max(n, 0))]
        elif kind[0] == 'fixed':
            out[name] = [_read(r, kind[2], {}) for _ in range(kind[1])]
        elif kind[0] == 'strs':
            out[name] = [r.s() for _ in range(max(out[kind[1]], 0))]
        elif kind[0] == 'ilist':
            out[name] = [r.i() for _ in range(max(out[kind[1]], 0))]
        else:
            raise ValueError(kind)
    return out


def _write(w, schema, obj):
    for name, kind in schema:
        v = obj[name]
        if kind in ('i', 'u'):
            w.i(v)
        elif kind == 'tag':
            w.i(str_tag(v))
        elif kind == 's':
            w.s(v)
        elif kind == 'rect':
            w.i(v['top']); w.i(v['left']); w.i(v['bottom']); w.i(v['right'])
        elif kind == 'point':
            w.i(v['y']); w.i(v['x'])
        elif kind[0] == 'ints':
            assert len(v) == kind[1]
            for x in v:
                w.i(x)
        elif kind[0] in ('list', 'fixed'):
            if kind[0] == 'list':
                assert len(v) == max(obj[kind[1]], 0), name
            for e in v:
                _write(w, kind[2], e)
        elif kind[0] == 'strs':
            assert len(v) == max(obj[kind[1]], 0), name
            for e in v:
                w.s(e)
        elif kind[0] == 'ilist':
            assert len(v) == max(obj[kind[1]], 0), name
            for x in v:
                w.i(x)


def decode_schema(data, schema, allow_tail=False):
    r = BinReader(data)
    out = _read(r, schema, {})
    if r.left():
        if not allow_tail:
            raise FormatError('%d trailing bytes' % r.left())
        out['_tail'] = r.rest().hex()
    return out


def encode_schema(obj, schema):
    w = BinWriter()
    _write(w, schema, obj)
    if '_tail' in obj:
        w.raw(bytes.fromhex(obj['_tail']))
    return w.bytes()


# ---------------------------------------------------------------------------
# shared identity resources
# ---------------------------------------------------------------------------

# CFilePlugin::CFilePlugin @ 0x1000df20 (ATTR, id 128)
ATTR = [('mID', 'tag'), ('mAttrib', 'u'), ('mDependID', 'tag'), ('mGameLevel', 'i'),
        ('mSecretItem', 'i'), ('mItemType', 'i'), ('mMustItem', 'i'), ('name', 's')]
# DESC (id 128): plugin-class 4cc + int (always 3 in the shipped set)
DESC = [('kind', 'tag'), ('version', 'i')]

# ---------------------------------------------------------------------------
# World plugin resources
# ---------------------------------------------------------------------------

# T2WorldDef::T2WorldDef @ 0x1006e4e0 (WORLDDEF id 1, opened by
# T2WorldPlugin::LoadT2WorldDef @ 0x10073040). Offsets = T2WorldDef members.
GRADE = [('vipFlag', 'u'),          # +0x88  must be set in doc+0xa0 (VIP visited) - CheckGameLevel 0x10073290
         ('population', 'i'),       # +0x8c  population needed; VIP comes at > population/2 (T2VisitVIP::Start 0x1006cd90)
         ('gradeUpAnimID', 'i'),    # +0x90  RT_BITMAP/OBJMAP id of the grade-up animation (SetupGradeUpAnimation 0x10072e70)
         ('gradeUpAnimFrames', 'i'),  # +0x94 frames in that strip (bitmap height / 96)
         ('unk98', 'i'),            # +0x98  always 0
         ('gradeUpAnimParam', 'i')]  # +0x9c T2DlgItemAnimation::SetAnimationParam(100, value)
WORLDDEF = [
    ('sortKey', 'i'),             # +0x50  GetSortKey (world list order)
    ('height', 'i'),              # +0x1f8 GetHeight: world height in floors (unit rows)
    ('width', 'i'),               # +0x1fc GetWidth: world width in units
    ('groundLine', 'i'),          # +0x54  row index of the ground floor (rows < groundLine are above ground)
    ('buildArea', 'rect'),        # +0x200 GetBuildArea: clamp for any construction (T2FloorInfo::IsValidRange)
    ('entrance', 'point'),        # +0x60  unconsumed by T2DLL code we found (x,y in units; near ground)
    ('topFloorLine', 'i'),        # +0x68  T2FloorInfo::GetTopFloorLine: highest buildable floor row
    ('bottomFloorLine', 'i'),     # +0x6c  T2FloorInfo::GetBottomFloorLine: lowest buildable floor row
    ('startGrade', 'i'),          # +0x70  (inferred) grade a new tower starts at: 1/2/3 for Tokyo/Hawaii/Kegon;
                                  #        lower grade entries/icons are dummies or missing in the data
    ('unk74', 'i'),               # +0x74  always startGrade-1 in the shipped worlds
    ('lobbyHeight', 'i'),         # +0x58  GetLobbyHeight; IsStoppableFloor: rows [ground-lobby, ground-1) are lobby
    ('outViewKind', 'i'),         # +0x218 GetOutViewKind
    ('unk17c', 'i'),              # +0x17c
    ('tiledBkgnd', 'i'),          # +0x228 1 = BINF tile map, 0 = one bitmap stretched (DrawBkgnd 0x1006eef0)
    ('numGrades', 'i'),           # +0x84  number of grade-up steps (<= 8)
    ('grades', ('list', 'numGrades', GRADE)),
    ('bkgndTileHeight', 'i'),     # +0x180 px per background tile row (2 floors)
    ('bkgndTileWidth', 'i'),      # +0x184 px per background tile column (8 units)
    ('unk188', 'i'),              # +0x188
    ('unk18c', 'i'),              # +0x18c
    ('discarded', ('ints', 10)),  # read and thrown away by the constructor
    ('gradeIconAnimIDs', ('ints', 5)),  # +0x148..0x158 star/"Tower" grade icon strips (bitmaps 3011-3015), 0 = none
    ('towerNamePrefix', 's'),     # +0x15c char[] e.g. "Yoot Tower:" (title-bar prefix)
    ('dustOnAt', 'u'),            # +0x170 IsDustOn(x): x >= value
    ('toiletAt', 'u'),            # +0x174 GetToiletDenomi(x): value <= x -> toiletDenomi
    ('toiletDenomi', 'u'),        # +0x178
    ('discarded2', 'i'),
    ('numCash', 'i'),
    ('cashAmounts', ('ilist', 'numCash')),  # +0x22c GetCashAmount(0..7), only first 8 kept
]


# WATR id 128, opened by T2WorldDef ctor (tag 0x57417472 "WAtr").
WATR = [('unk', ('ints', 6)),        # discarded by T2WorldDef; [3]=floors above ground, [4]=basement floors (observed)
        ('initialFunds', 'i'),        # +0x24c GetInitialFunds
        ('exchangeMul', 'i'),         # +0x250 ExchangeMoney(x) = x/exchangeDiv*exchangeMul (identity if both 1)
        ('exchangeDiv', 'i'),         # +0x254
        ('moneyUnit', 's')]           # +0x258 GetMoneyUnit

# PODF id 1 -> T2PoolDef (FUN_10050e00 via T2WorldDef::MakePoolDef 0x1006eb10)
PODF = [('unk08', 'i'),
        ('gradePoolIDs', ('ints', 5)),   # PGDF resource id used when the game reaches grade 1..5 (0 = none); T2Pool::GameLevelChanged 0x10050390
        ('unk20', 'i'),                  # read as uchar
        ('numDemand', 'i'),              # read as uchar
        ('demand', ('list', 'numDemand', [('a', 'i'), ('b', 'i')]))]  # (int, uchar) pairs
# CODF id 1 -> T2ColdTableDef (FUN_1001c400 via MakeColdTableDef 0x1006ec50); GetColdDenomi
CODF = [('unk', 'u'), ('monthly', ('ints', 12))]
# SPDF id 1 -> T2SeasonParamDef (FUN_100585d0 via MakeSeasonParamDef 0x1006eba0): 12 monthly percents
SPDF = [('monthlyPercent', ('ints', 12))]
# T2EV id 1 -> T2TowerEvent::T2TowerEvent 0x10068ca0
T2EV = [('count', 'i'), ('events', ('list', 'count', [('type', 'tag'), ('resID', 'i')]))]
# T2EventItem::T2EventItem 0x10030050 - header of every *EV resource
EVENT_HEADER = [('levelBit', 'u'),   # GetLevelBit: grades on which the event may run (bitmask)
                ('exclusive', 'i'),  # IsExclusive (stored as bool)
                ('cycleDay', 'i'),   # GetCycleDay: runs when day % cycleDay == 0 (min 1)
                ('beginTime', 'i'),  # GetBeginTime: minute of day (0..1439)
                ('endTime', 'i')]    # GetEndTime
EVENT_BODIES = {
    # T2BlackOut::T2BlackOut 0x1001b580
    'BOEv': [('unk30', 'i'), ('unk34', 'i'), ('unk38', 'i')],
    # T2Terrorist::T2Terrorist 0x100638e0
    'TrEv': [('unk30', 'i'), ('unk34', 'i'), ('unk38', 'i')],
    # T2SantaClaus::T2SantaClaus 0x100560b0 (0 -> 32)
    'SaEv': [('unk30', 'i')],
    # T2FireBurning::T2FireBurning 0x10030c10
    'FBEv': [('unk38', 'i'), ('count', 'i'), ('values', ('ilist', 'count'))],
    # T2OutDecoration::T2OutDecoration 0x10045760: (bitmap id, value) pairs
    'OdEv': [('count', 'i'), ('items', ('list', 'count', [('imageID', 'i'), ('value', 'u')]))],
    # T2VisitVIP::T2VisitVIP 0x1006cb40 + T2VisitVIP::Init 0x1006d360
    'ViEv': [('count', 'i'), ('vips', ('list', 'count', [
        ('grade', 'i'), ('tenantType', 'tag'), ('unk38', 'i'), ('unk3c', 'i'), ('unk40', 'i'),
        ('unk44', 'i'), ('viplID', 'i'), ('comment', 's'), ('comment2', 's')]))],
    # T2Baseball::T2Baseball (Tokyo.t2p 0x100118d0): six shorts + two ints
    'BBEv': [('unk58', 'i'), ('unk5a', 'i'), ('unk5c', 'i'), ('unk5e', 'i'), ('unk60', 'i'),
             ('unk62', 'i'), ('unk64', 'i'), ('unk68', 'i')],
    # T2Cloud::T2Cloud (Tokyo.t2p 0x10012bd0 / Hawai.t2p): cloud bitmaps + spawn zones (unit rects)
    'ClEv': [('numImages', 'i'), ('imageIDs', ('ilist', 'numImages')),
             ('numZones', 'i'), ('zones', ('list', 'numZones', [('rect', 'rect')]))],
    # T2HotSpring::T2HotSpring (Kegon.t2p 0x10012940)
    'HSEv': [('unk68', 'i'), ('unk6c', 'i'), ('tenantType1', 'tag'), ('tenantType2', 'tag')],
    # T2Wedding::T2Wedding (Hawai.t2p 0x10014ed0): header only
    'WdEv': [],
    # T2Maru_Reggae::T2Maru_Reggae 0x1003a770 (Tokyo)
    'MaEv': [('unk30', 'i'), ('viplID', 'i'), ('unk38', 'i'), ('unk3c', 'i'), ('message', 's')],
}
# VIPL ids 220-224 (VIP people): first int read by T2VisitVIP::Init / Maru_Reggae
VIPL = [('values', ('ints', 6))]
# STRTBL id 1 (Hawai): count + strings
STRTBL = [('count', 'i'), ('strings', ('strs', 'count'))]

# PTDF field names, T2PaletteAnimeDef::T2PaletteAnimeDef 0x10047e60 (text mode)
PTDF_FIELDS = ['mNumOfTowerColor', 'mNumOfAnimColor', 'mAnimStartIndex', 'mNumOfRefrectColor',
               'mNumOfSkyColor', 'mNumOfSEKind', 'mNumOfSEColor', 'mSEInterval']


# ---------------------------------------------------------------------------
# text-mode resources
# ---------------------------------------------------------------------------

def decode_numbers_text(data):
    """BINF/FINF: one decimal number per CRLF line."""
    txt = data.decode('ascii')
    if not txt.endswith('\r\n'):
        raise FormatError('text grid not CRLF terminated')
    return [int(x) for x in txt[:-2].split('\r\n')]


def encode_numbers_text(nums):
    return ('\r\n'.join(str(n) for n in nums) + '\r\n').encode('ascii')


def decode_binf(data):
    """T2WorldPlugin::InitBkgndInfoProc 0x10073150.
    Header rows (= world height/2), cols (= width/8), unused; then rows*cols
    tile indices stored COLUMN-MAJOR (for each column, top row first).
    Value & 0xffff indexes 'Sky' tiles in RT_BITMAP 1000 (UT2BkgndInfo::GetBkgndInfo)."""
    n = decode_numbers_text(data)
    rows, cols, unused = n[0], n[1], n[2]
    body = n[3:]
    if len(body) != rows * cols:
        raise FormatError('BINF expects %d cells, got %d' % (rows * cols, len(body)))
    grid = [[body[c * rows + r] for c in range(cols)] for r in range(rows)]
    return {'rows': rows, 'cols': cols, 'unused': unused, 'grid': grid}


def encode_binf(o):
    rows, cols = o['rows'], o['cols']
    body = [o['grid'][r][c] for c in range(cols) for r in range(rows)]
    return encode_numbers_text([rows, cols, o['unused']] + body)


def decode_finf(data):
    """T2WorldPlugin::InitFloorInfoProc 0x100730d0 -> T2FloorInfo::InitMask 0x10033d40.
    Header: int, int, int, RECT (text order l,t,r,b) -- all read and discarded;
    then height*width values ROW-MAJOR (T2FloorInfo unit array is
    width*height, index = row*width+col).  Non-zero -> unit mask 0x8000
    (T2UnitInfo::InitMask 0x1006c940) -> T2UnitInfo::IsBuildable() false."""
    n = decode_numbers_text(data)
    h, w, extra = n[0], n[1], n[2]
    rect = n[3:7]
    body = n[7:]
    if len(body) != h * w:
        raise FormatError('FINF expects %d cells, got %d' % (h * w, len(body)))
    grid = [body[r * w:(r + 1) * w] for r in range(h)]
    return {'height': h, 'width': w, 'unk': extra, 'rectLTRB': rect, 'grid': grid}


def encode_finf(o):
    body = [v for row in o['grid'] for v in row]
    return encode_numbers_text([o['height'], o['width'], o['unk']] + list(o['rectLTRB']) + body)


def decode_ptdf(data):
    """Text '(name)<tabs>value\r\n' lines; tabs preserved for byte-identical output."""
    txt = data.decode('ascii')
    lines = txt.split('\r\n')
    if lines[-1] != '':
        raise FormatError('PTDF not CRLF terminated')
    out = []
    for ln in lines[:-1]:
        if not ln.startswith('('):
            raise FormatError('PTDF line %r' % ln)
        name, rest = ln[1:].split(')', 1)
        sep = rest[:len(rest) - len(rest.lstrip('\t '))]
        out.append({'name': name, 'sep': sep, 'value': int(rest.strip())})
    return {'fields': out}


def encode_ptdf(o):
    return ''.join('(%s)%s%d\r\n' % (f['name'], f['sep'], f['value']) for f in o['fields']).encode('ascii')


# ---------------------------------------------------------------------------
# PALETTE: Mac 'clut' (first u16 == 0) or 'pltt' (first u16 = count); 0x300 = raw ACT
# LoadACTPaletteEntries 0x100120e0
# ---------------------------------------------------------------------------

def decode_palette(data):
    if len(data) == 0x300:
        return {'format': 'act', 'colors': [list(data[i:i + 3]) for i in range(0, 0x300, 3)]}
    if data[0] == 0 and data[1] == 0:
        seed, flags, cnt = struct.unpack_from('>IHH', data, 0)
        n = (len(data) - 8) // 8
        ents = [list(struct.unpack_from('>HHHH', data, 8 + 8 * k)) for k in range(n)]
        if 8 + 8 * n != len(data):
            raise FormatError('clut size')
        # NOTE: engine loads only `cnt` entries (cnt is count-1 in Mac terms -> last entry skipped)
        return {'format': 'clut', 'seed': seed, 'flags': flags, 'ctSize': cnt,
                'entries': ents}  # [index, r16, g16, b16]
    cnt = struct.unpack_from('>H', data, 0)[0]
    hdr = data[2:16].hex()
    if 16 + 16 * cnt != len(data):
        raise FormatError('pltt size')
    ents = []
    for k in range(cnt):
        r, g, b, usage, tol, flags, priv = struct.unpack_from('>HHHHHHI', data, 16 + 16 * k)
        ents.append([r, g, b, usage, tol, flags, priv])
    return {'format': 'pltt', 'header': hdr, 'entries': ents}  # [r16,g16,b16,usage,tolerance,privFlags,private]


def encode_palette(o):
    if o['format'] == 'act':
        return bytes(c for rgb in o['colors'] for c in rgb)
    if o['format'] == 'clut':
        b = struct.pack('>IHH', o['seed'], o['flags'], o['ctSize'])
        return b + b''.join(struct.pack('>HHHH', *e) for e in o['entries'])
    b = struct.pack('>H', len(o['entries'])) + bytes.fromhex(o['header'])
    return b + b''.join(struct.pack('>HHHHHHI', *e) for e in o['entries'])


def palette_rgb(o):
    """256 (r,g,b) tuples the way the engine builds the LOGPALETTE."""
    pal = [(0, 0, 0)] * 256
    if o['format'] == 'act':
        return [tuple(c) for c in o['colors']]
    if o['format'] == 'clut':
        for e in o['entries'][:o['ctSize']]:
            pal[e[0] & 0xff] = (e[1] >> 8, e[2] >> 8, e[3] >> 8)
        return pal
    for k, e in enumerate(o['entries'][:256]):
        pal[k] = (e[0] >> 8, e[1] >> 8, e[2] >> 8)
    return pal


# ---------------------------------------------------------------------------
# SP-format sub-plugins (T2SubPlugin family)
# ---------------------------------------------------------------------------

# T2SubPlugin::LoadRsrcFromStream 0x1005d110
SUBPLUGIN_HEADER = [('title', 's'),
                    ('tieupDate', 'u')]  # BCD 0xYYYYMMDD (day stored minus one); 0 = not a tie-up. IsTieupFinish compares with today.
# T2MoviePlugin::LoadRsrcFromStream 0x100430e0  (MODF id 128, tag 0x4d4f4446)
MODF = SUBPLUGIN_HEADER + [
    ('imageID', 'i'),        # ushort +0xc8: poster bitmap id (128) - UnfoldPluginSelf
    ('filmKind', 'i'),       # +0xcc 0 = roadshow (IsRoadshowFilm), 2 = famous (IsFamousFilm)
    ('phase1Months', 'i'),   # +0xd0 life-cycle phase lengths, made cumulative in UnfoldPluginSelf;
    ('phase2Months', 'i'),   # +0xd4 GetMovieLifePhase = (month - release - 12) % total
    ('phase3Months', 'i'),   # +0xd8
    ('phase4Months', 'i'),   # +0xdc read then zeroed by the loader
    ('cycleMonths', 'i'),    # +0xe0 read then zeroed by the loader
    ('production', 's'),     # +0xec "Published by" credit
    ('director', 's'),       # +0xf0
    ('starring', 's'),       # +0xf4
    ('runningTime', 'i'),    # +0xf8 minutes
    ('unkFC', 'i'),          # +0xfc
    ('description', 's'),    # +0x100 genre / blurb
    ('customerTableID', 'i'),  # ushort -> +0xb4 DupCustomerTableDefRes: TMDF id of the audience table
    ('unk104', 'i'), ('unk108', 'i'),
    ('famousAudience', 'i'),   # +0x10c ReMake: audience >= -> famous film (flag 2)
    ('remakeAudience', 'i'),   # +0x110 ReMake: audience >= -> sequel/re-run, else film retires (flag 0x8000)
    ('unk114', 'i'),
    ('numExtra', 'i'),         # +0x120
    ('extra', ('strs', 'numExtra')),  # +0x124 CString[]
    ('stillImageID', 'i'),     # +0x118 second bitmap (129, 208x117 still)
    ('clipSeconds', 'i'),      # +0x11c (60/140; clip length guess)
    ('movieFile', 's'),        # +0xe4 file name inside Movie/ (e.g. MV_1stContact.MOV)
]
# T2HallEventPlugin::LoadRsrcFromStream 0x100380e0 (HEDF id 128, tag 0x48454446)
HEDF = SUBPLUGIN_HEADER + [
    ('imageID', 'i'),        # ushort +0xc8: 4 stacked frames -> OBJMAP-less parts 200..203
    ('unkCC', 'u'),
    ('bookingDays', 'u'),    # +0xd0 copied into the EventCenter booking entry (+0x10) (inferred)
    ('openTime', 'u'),       # +0xb8 GetOpenTime (minute of day)
    ('closeTime', 'u'),      # +0xbc GetCloseTime
    ('monthMask', 'u'),      # +0xd4 12-bit month mask, bit0 = January (data pattern)
    ('cost', 'i'),           # +0xd8 settled at close: funds += tenantValue - cost (EventCenter)
    ('customerTableID', 'i'),  # ushort -> +0xb4 (TMDF id)
    ('unkDC', 'i'), ('unkE0', 'i'), ('unkE4', 'i'),
    ('bindTime', 'i'),       # +0xe8 GetBindTime
    ('stillImageID', 'i'),   # +0xf4 second bitmap (129)
    ('description', 's'),    # +0xf8
    ('movieFile', 's'),      # +0xec file name inside Movie/ (HE_*.MOV)
]
# T2AdvertisePlugin::LoadRsrcFromStream 0x10015bd0 (ADDF id 128, tag 0x41444446)
ADDF = SUBPLUGIN_HEADER + [
    ('subType', 'tag'),      # +0xc8 GetSubType: target advertising medium (4cc)
    ('imageID', 'i'),        # ushort +0xcc: bitmap id
    ('unkD0', 'u'),
    ('days', 'u'),           # +0xd4 GetDay: campaign length
    ('money', 'i'),          # +0xd8 GetMoney: cost
    ('path', 's'),           # +0xdc GetPath
]

SCHEMAS = {
    'ATTR': ATTR, 'DESC': DESC, 'WORLDDEF': WORLDDEF, 'WATR': WATR, 'PODF': PODF,
    'CODF': CODF, 'SPDF': SPDF, 'T2EV': T2EV, 'VIPL': VIPL, 'STRTBL': STRTBL,
    'MODF': MODF, 'HEDF': HEDF, 'ADDF': ADDF,
}
TEXT_CODECS = {
    'BINF': (decode_binf, encode_binf),
    'FINF': (decode_finf, encode_finf),
    'PTDF': (decode_ptdf, encode_ptdf),
    'PALETTE': (decode_palette, encode_palette),
}


def decode_event(data, evtag=None):
    """*EV resource: T2EventItem header + type-specific body (raw ints/hex if unknown)."""
    r = BinReader(data)
    out = _read(r, EVENT_HEADER, {})
    body = EVENT_BODIES.get(evtag)
    if body is not None:
        try:
            p0 = r.p
            out['body'] = _read(r, body, {})
            if r.left():
                raise FormatError('tail')
            out['bodyKind'] = evtag
            return out
        except FormatError:
            r.p = p0
            out.pop('body', None)
    rest = r.rest()
    if len(rest) % 4 == 0:
        out['bodyInts'] = list(struct.unpack('<%di' % (len(rest) // 4), rest))
    else:
        out['bodyHex'] = rest.hex()
    return out


def encode_event(o):
    w = BinWriter()
    _write(w, EVENT_HEADER, o)
    if 'body' in o:
        _write(w, EVENT_BODIES[o['bodyKind']], o['body'])
    elif 'bodyInts' in o:
        for x in o['bodyInts']:
            w.i(x)
    else:
        w.raw(bytes.fromhex(o['bodyHex']))
    return w.bytes()


def decode_resource(rtype, data, evtags=None):
    """Return (decoded, encoder) or (None, None) if the type is not decoded here."""
    if rtype in TEXT_CODECS:
        dec, enc = TEXT_CODECS[rtype]
        return dec(data), enc
    if rtype in SCHEMAS:
        sch = SCHEMAS[rtype]
        return decode_schema(data, sch), (lambda o, s=sch: encode_schema(o, s))
    if rtype.endswith('EV') and rtype != 'T2EV' and data[:1] == b'\x01':
        evtag = (evtags or {}).get(rtype)
        return decode_event(data, evtag), encode_event
    return None, None


# ---------------------------------------------------------------------------
# containers
# ---------------------------------------------------------------------------

def is_sp(path):
    with open(path, 'rb') as f:
        return f.read(2) == b'SP'


def read_sp(data):
    """SP container (CResFile::OpenResource(char*,...) 0x10010fd0 / OpenSubPluginFS 0x10012570):
    'SP' u32 count, count x {u32 type, u32 id, u32 offset, u32 size}; payloads follow.
    `type` is the CResFile tag constant (e.g. 0x4d4f4446 -> bytes 'FDOM' -> name 'MODF');
    type 0 = Windows DIB (BITMAPINFOHEADER, no BITMAPFILEHEADER)."""
    if data[:2] != b'SP':
        raise FormatError('not an SP container')
    n = struct.unpack_from('<I', data, 2)[0]
    ents = []
    for k in range(n):
        typ, rid, off, size = struct.unpack_from('<IIII', data, 6 + 16 * k)
        if off + size > len(data):
            raise FormatError('entry %d out of range' % k)
        name = be_name(typ).strip('\0') if typ else 'BITMAP'
        ents.append({'type': typ, 'name': name, 'id': rid, 'offset': off, 'data': data[off:off + size]})
    return ents


def write_sp(ents):
    """Inverse of read_sp; payloads laid out contiguously in directory order (as shipped)."""
    n = len(ents)
    off = 6 + 16 * n
    hdr = bytearray(b'SP' + struct.pack('<I', n))
    body = bytearray()
    for e in ents:
        hdr += struct.pack('<IIII', e['type'], e['id'], off + len(body), len(e['data']))
        body += e['data']
    return bytes(hdr + body)


def read_pe_resources(path):
    import pefile
    pe = pefile.PE(path, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_RESOURCE']])
    out = []
    if not hasattr(pe, 'DIRECTORY_ENTRY_RESOURCE'):
        return out
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tname = str(t.name) if t.name is not None else pefile.RESOURCE_TYPE.get(t.id, str(t.id))
        for e in t.directory.entries:
            rid = str(e.name) if e.name is not None else e.id
            for lang in e.directory.entries:
                s = lang.data.struct
                out.append({'name': tname, 'id': rid, 'lang': lang.id,
                            'data': pe.get_data(s.OffsetToData, s.Size)})
    return out


def load_plugin(path):
    """Return ('SP'|'PE', [resource dicts with name,id,data])."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] == b'SP':
        return 'SP', read_sp(data)
    if data[:2] == b'MZ':
        return 'PE', read_pe_resources(path)
    raise FormatError('unknown container')


def _evtags(res):
    """Map *EV resource names to their T2EV tag text (for picking the body schema)."""
    m = {}
    for r in res:
        if r['name'] == 'T2EV':
            for ev in decode_schema(r['data'], T2EV)['events']:
                tag = ev['type'][::-1]           # file order 'vEOB' -> 'BOEv'
                m[tag.upper()] = tag
    return m


def decode_plugin(path):
    kind, res = load_plugin(path)
    evtags = _evtags(res)
    out = {'container': kind, 'resources': []}
    for r in res:
        entry = {'type': r['name'], 'id': r['id'], 'size': len(r['data'])}
        try:
            dec, _ = decode_resource(r['name'], r['data'], evtags)
        except FormatError as ex:
            dec = None
            entry['error'] = str(ex)
        if dec is not None:
            entry['decoded'] = dec
        out['resources'].append(entry)
    return out


def roundtrip_plugin(path):
    """Returns list of (type, id, status) where status in ok/mismatch/undecoded/error."""
    kind, res = load_plugin(path)
    evtags = _evtags(res)
    results = []
    for r in res:
        try:
            dec, enc = decode_resource(r['name'], r['data'], evtags)
        except FormatError as ex:
            results.append((r['name'], r['id'], 'error: %s' % ex))
            continue
        if dec is None:
            results.append((r['name'], r['id'], 'undecoded'))
            continue
        again = json.loads(json.dumps(dec))  # prove the JSON model is sufficient
        results.append((r['name'], r['id'], 'ok' if enc(again) == r['data'] else 'mismatch'))
    if kind == 'SP':
        with open(path, 'rb') as f:
            data = f.read()
        results.append(('<container>', '', 'ok' if write_sp(read_sp(data)) == data else 'mismatch'))
    return results


def main(argv):
    if len(argv) < 3 or argv[1] not in ('dump', 'roundtrip'):
        print(__doc__)
        return 2
    path = argv[2]
    if argv[1] == 'dump':
        d = decode_plugin(path)
        if '--json' in argv:
            json.dump(d, sys.stdout, indent=1, ensure_ascii=False)
            print()
        else:
            print('container:', d['container'])
            for r in d['resources']:
                line = '%-10s %-6s %7d' % (r['type'], r['id'], r['size'])
                if 'decoded' in r:
                    dd = r['decoded']
                    if r['type'] in ('BINF', 'FINF'):
                        dd = {k: v for k, v in dd.items() if k != 'grid'}
                    if r['type'] == 'PALETTE':
                        dd = {'format': dd['format'], 'entries': len(dd.get('entries', dd.get('colors', [])))}
                    line += '  ' + json.dumps(dd, ensure_ascii=False)[:300]
                elif 'error' in r:
                    line += '  ERROR ' + r['error']
                print(line)
        return 0
    bad = 0
    for t, i, st in roundtrip_plugin(path):
        if st not in ('ok', 'undecoded'):
            bad += 1
        print('%-10s %-6s %s' % (t, i, st))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
