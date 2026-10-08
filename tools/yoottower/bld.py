#!/usr/bin/env python3
"""YootTower (The Tower II, Win32 1999) ``.bld`` tower-save codec.

Parses a ``.bld`` save into a JSON-able dict and writes it back
byte-for-byte.  One set of structure functions drives both directions
(``Codec`` in read or write mode), so the writer cannot drift from the
reader.  Format spec, field tables and the T2.exe/T2DLL.dll function
addresses each structure was taken from: ``doc/yoottower/bld-format.md``.

Usage (always run with ``python3 -I``):

    bld.py dump  SAVE.bld [-o out.json] [--full]   # parse -> JSON
    bld.py summary SAVE.bld                         # human summary
    bld.py roundtrip SAVE.bld                       # parse+write, compare bytes
    bld.py write in.json -o out.bld                 # JSON (from dump --full) -> .bld

``dump`` without ``--full`` drops the bulky per-cell grids (unit map,
traffic map) from the JSON; ``--full`` keeps everything needed by
``write``.
"""
import argparse
import json
import struct
import sys

MAGIC = b"002T"  # u32 0x54323030 'T200' written little-endian


class BldError(Exception):
    pass


def tag_str(b):
    """4 raw file bytes -> display string (file byte order, the same
    convention as doc/yoottower/itemtype-catalog.md)."""
    if all(32 <= c < 127 for c in b):
        return b.decode("ascii")
    return "0x" + b.hex()


def tag_bytes(s):
    if s.startswith("0x") and len(s) == 10:
        return bytes.fromhex(s[2:])
    b = s.encode("ascii")
    if len(b) != 4:
        raise BldError("bad 4cc %r" % s)
    return b


class Codec:
    """Symmetric primitive reader/writer.  Every primitive takes the dict
    ``d`` and key ``k``: in read mode it reads and stores d[k], in write
    mode it fetches d[k] and emits it.  Both return the value."""

    def __init__(self, data=None):
        self.reading = data is not None
        self.buf = data if self.reading else bytearray()
        self.pos = 0
        self.trace = []  # (offset, path) breadcrumbs for error messages

    # -- raw -------------------------------------------------------------
    def _take(self, n):
        if self.pos + n > len(self.buf):
            raise BldError("unexpected EOF at 0x%x (need %d) in %s"
                           % (self.pos, n, "/".join(self.trace[-4:])))
        b = bytes(self.buf[self.pos:self.pos + n])
        self.pos += n
        return b

    def _num(self, d, k, fmt):
        size = struct.calcsize(fmt)
        if self.reading:
            v = struct.unpack(fmt, self._take(size))[0]
            if d is not None:
                d[k] = v
            return v
        v = d[k] if d is not None else k
        self.buf += struct.pack(fmt, v)
        return v

    def u8(self, d, k):  return self._num(d, k, "<B")
    def s8(self, d, k):  return self._num(d, k, "<b")
    def u16(self, d, k): return self._num(d, k, "<H")
    def s16(self, d, k): return self._num(d, k, "<h")
    def u32(self, d, k): return self._num(d, k, "<I")
    def s32(self, d, k): return self._num(d, k, "<i")

    def bool8(self, d, k):
        """u8 stored as 0/1.  Kept as int so odd values round-trip."""
        return self.u8(d, k)

    def tag(self, d, k):
        if self.reading:
            v = tag_str(self._take(4))
            d[k] = v
            return v
        v = d[k]
        self.buf += tag_bytes(v)
        return v

    def expect(self, raw):
        """A constant 4cc marker (e.g. '>NT<')."""
        if self.reading:
            got = self._take(len(raw))
            if got != raw:
                raise BldError("expected %r at 0x%x, got %r (%s)" % (
                    raw, self.pos - len(raw), got, "/".join(self.trace[-4:])))
        else:
            self.buf += raw

    def raw(self, d, k, n):
        if self.reading:
            v = self._take(n).hex()
            d[k] = v
            return v
        v = d[k]
        self.buf += bytes.fromhex(v)
        return v

    def srect(self, d, k):
        """T2Archive::ReadSRect: Mac QuickDraw Rect, four BIG-endian s16
        in order top,left,bottom,right.  Stored as [l, t, r, b]."""
        if self.reading:
            t, l, b, r = struct.unpack(">hhhh", self._take(8))
            d[k] = [l, t, r, b]
            return d[k]
        l, t, r, b = d[k]
        self.buf += struct.pack(">hhhh", t, l, b, r)
        return d[k]

    def pstr(self, d, k):
        """u8 length + bytes (T2Archive::ReadPStr / CPluginInfo strings).
        Text decoded as cp1252; a trailing NUL (T2Name writes strlen+1) is
        kept verbatim in the string."""
        if self.reading:
            n = self._take(1)[0]
            v = self._take(n).decode("cp1252")
            d[k] = v
            return v
        b = d[k].encode("cp1252")
        self.buf += bytes([len(b)]) + b
        return d[k]

    # -- structure helpers -------------------------------------------------
    def items(self, d, k, n, fn):
        """n sub-records, each a dict filled by fn(codec, dict)."""
        if self.reading:
            lst = []
            for i in range(n):
                e = {}
                self.trace.append("%s[%d]" % (k, i))
                fn(self, e)
                self.trace.pop()
                lst.append(e)
            d[k] = lst
            return lst
        lst = d[k]
        if len(lst) != n:
            raise BldError("%s: count %d != len %d" % (k, n, len(lst)))
        for e in lst:
            fn(self, e)
        return lst

    def values(self, d, k, n, prim):
        """n scalar values of one primitive type, stored as a flat list."""
        if self.reading:
            tmp = {}
            d[k] = [prim(tmp, "v") for _ in range(n)]
            return d[k]
        lst = d[k]
        if len(lst) != n:
            raise BldError("%s: count %d != len %d" % (k, n, len(lst)))
        for v in lst:
            prim({"v": v}, "v")
        return lst

    def count_of(self, d, k, listkey, prim):
        """Write a count taken from len(d[listkey]); read stores it."""
        if not self.reading:
            d[k] = len(d[listkey])
        return prim(d, k)


# ---------------------------------------------------------------------------
# LArray serialisations (T2DLL LArray::ReadAsWord/ReadAsChar/ReadAsDWord)
# ---------------------------------------------------------------------------

def larray_word(c, d):
    # 0x100134f0 ReadAsWord / 0x100135e0 WriteAsWord
    c.s32(d, "itemSize")        # always 2
    n = c.count_of(d, "count", "items", c.s32)
    c.s32(d, "h2")              # count*2
    c.s32(d, "h3")              # count*2
    c.u8(d, "h4")
    c.u8(d, "h5")
    nbytes = c.s32(d, "dataBytes")
    c.values(d, "items", n, c.u16)
    isz = d["itemSize"] or 1
    pad = max(0, nbytes // isz - n)
    if pad or "pad" in d:
        c.values(d, "pad", pad, c.u16)


def larray_char(c, d):
    # 0x10013720 ReadAsChar / 0x10013870 WriteAsChar
    c.s32(d, "itemSize")        # always 1
    n = c.count_of(d, "count", "items", c.s32)
    c.s32(d, "h2")
    c.s32(d, "h3")
    total = c.s32(d, "total")
    c.values(d, "items", n, c.u8)
    pad = max(0, total - n)
    if pad or "pad" in d:
        c.values(d, "pad", pad, c.u8)


# ---------------------------------------------------------------------------
# T2Object hierarchy (T2DLL *::LoadSelf / *::SaveSelf)
# ---------------------------------------------------------------------------

def object_hdr(c, d):
    """T2Object::LoadSelf 0x100453b0: used, status, favorite."""
    used = c.bool8(d, "used")
    c.s16(d, "status")
    c.bool8(d, "favorite")
    return used


CE_TAGS = {"AEC_", "AECT", "AECR", "AECF", "AECM"}  # T2CrossEquipArray::ReadCEArray 0x1001c610


def ce_array(c, d):
    t = c.tag(d, "ceClass")   # 'AEC_' base, 'AECT' tenant, 'AECR' route, 'AECF' floor, 'AECM' mover; 'AECx' = none
    if t in CE_TAGS:
        larray_word(c, sub(c, d, "ids"))


def _sub(d, k):
    d[k] = {}
    return d[k]


def sub(c, d, k):
    return _sub(d, k) if c.reading else d[k]


def equip(c, d):
    """T2Equip::LoadSelf 0x1002ef60 (shared by tenants and movers)."""
    if not object_hdr(c, d):
        return False
    c.u16(d, "equipID")
    c.srect(d, "area")             # unit coords [left, top(floor line), right, bottom]
    c.s16(d, "equipType")          # == plugin mItemType (numeric), see plugins[]
    c.s16(d, "drawMode")
    c.u8(d, "patIndex")
    c.s16(d, "f44")
    c.s16(d, "inMoneyIndex")
    c.s16(d, "f48")
    c.s32(d, "buildDay")
    ce_array(c, sub(c, d, "ce"))
    c.u32(d, "valiation")          # T2HaveOutViewObject +0x1c
    return True


def extra_data(c, d):
    """T2EquipDef::LoadExtraData 0x1002f8c0: s32 n + n bytes (writer
    always emits n=0).  Plugin-specific defs may override; none of the
    sample saves need it."""
    n = c.s32(d, "extraLen")
    c.raw(d, "extra", n)


def tenant(c, d):
    """T2Tenant::LoadSelf 0x1005e820 / SaveSelf 0x1005eb50."""
    c.expect(b">NT<")
    if not equip(c, d):
        return
    c.u8(d, "kindIndex")
    c.s32(d, "resideDay")
    c.s16(d, "f8c")
    c.s16(d, "estimate")
    c.bool8(d, "bind")
    c.u16(d, "relatedTenantID")
    c.s16(d, "reserveCount")
    c.s16(d, "belongCount")
    c.s16(d, "customerCount")
    c.s16(d, "workCount")
    c.s16(d, "totalCustomer")
    c.u16(d, "tenantNumber")
    c.u32(d, "relatedTenantListID")
    n = c.count_of(d, "nMarks", "marks", c.s16)
    c.values(d, "marks", max(n, 0), c.u8)
    c.expect(b">kLp")
    c.u32(d, "firstPeople")
    c.u32(d, "people64")
    has = c.u32(d, "hasBelongList")
    if has:
        bl = sub(c, d, "belongList")       # T2PeoplePtrList::LoadSelf 0x1004dc50
        n = c.count_of(bl, "count", "people", c.s32)
        c.values(bl, "people", n, c.u32)
    extra_data(c, d)


def mover_module(c, d, kind):
    """T2MoverModule::LoadSelf 0x10040980 + Elevator 0x1002e320 / Stair 0x1005c3b0."""
    if not object_hdr(c, d):
        return
    c.s16(d, "position")
    c.s16(d, "direction")
    c.s16(d, "waitCounter")
    c.s16(d, "patIndex")
    c.s16(d, "numPeople")
    c.s16(d, "f4c")
    c.u32(d, "firstPeople")
    c.u32(d, "lastPeople")
    if kind == "LMlE":
        c.s16(d, "index")
        c.s16(d, "homePosition")
        c.s16(d, "offsetPos")
        c.s16(d, "nextStop")
        c.s16(d, "f6c")
        c.s16(d, "topTurn")
        c.s16(d, "bottomTurn")
        t = c.tag(d, "stopTag")          # 'AfIS' -> T2StopInfoArray (LArray char)
        if t == "AfIS":
            larray_char(c, sub(c, d, "stops"))
    elif kind == "LMtS":
        c.srect(d, "moduleRect")


def module_list(c, d):
    """T2MoverModuleList::ReadMMList 0x10041000."""
    kind = c.tag(d, "kind")   # 'LMlE' elevator, 'LMtS' stairs/escalator, 'LMM_' base, 'LMMx' none
    if kind not in ("LMlE", "LMtS", "LMM_"):
        return
    c.s16(d, "f20")
    c.srect(d, "rect")
    if kind == "LMM_":
        return
    n = c.count_of(d, "count", "modules", c.s32)
    c.items(d, "modules", n, lambda cc, e: mover_module(cc, e, kind))
    if kind == "LMlE":
        c.srect(d, "rect2")


def mover(c, d):
    """T2Mover::LoadSelf 0x1003f400 / SaveSelf 0x1003f530."""
    c.expect(b">VM<")
    if not equip(c, d):
        return
    c.bool8(d, "showShaft")
    c.s16(d, "length")
    t = c.tag(d, "reqIDTag")          # 'ADIR' -> T2RequestIDArray (LArray word)
    if t == "ADIR":
        larray_word(c, sub(c, d, "reqIDs"))
    c.s16(d, "waitCount")
    c.s16(d, "f6c")
    module_list(c, sub(c, d, "modules"))
    c.s32(d, "refCon")
    extra_data(c, d)


def request(c, d):
    """T2Request::LoadSelf 0x10053660 / SaveSelf 0x10053860."""
    c.expect(b">QR<")
    if not object_hdr(c, d):
        return
    c.u16(d, "requestID")
    c.u16(d, "moverID")
    c.bool8(d, "upDown")
    c.s16(d, "f18")
    c.s16(d, "f1c")
    c.s8(d, "moduleIndex")
    c.s16(d, "drawWidth")
    c.u32(d, "people34")
    c.u32(d, "people38")
    c.u32(d, "people3c")
    c.u32(d, "people40")


def outobj(c, d):
    """T2OutObj::LoadSelf 0x100462e0 (no marker).  The OuterObjDef extra
    data is empty (T2ToolDef::LoadExtraData) when the def is found."""
    if not object_hdr(c, d):
        return
    c.u16(d, "outObjID")
    c.srect(d, "area")
    c.u32(d, "valiation")
    c.s16(d, "outObjType")


def people(c, d):
    """T2People::LoadSelf 0x1004a810 (+T2Matter::LoadSelf 0x1003bfc0)."""
    c.expect(b">PP<")
    if not object_hdr(c, d):
        return
    # T2Matter
    c.u32(d, "matterID")
    c.u16(d, "workTenant")
    c.u16(d, "homeTenant")
    c.u16(d, "currEquipID")
    c.u16(d, "destination")
    c.s16(d, "curH")
    c.s16(d, "curV")
    c.s16(d, "dstH")
    c.s16(d, "dstV")
    c.s16(d, "startTime")
    c.s8(d, "direction")
    c.s8(d, "walkStyle")
    t = c.tag(d, "returnTag")          # 'KTSR' -> return stack, 'KSRx' none
    if t == "KTSR":
        rs = sub(c, d, "returnStack")
        n = c.count_of(rs, "count", "entries", c.s32)
        c.items(rs, "entries", n, lambda cc, e: (cc.u16(e, "tenant"), cc.u16(e, "time")))
    # T2People
    c.u8(d, "specialFlag")
    c.bool8(d, "walk")
    c.s16(d, "stress")
    c.u16(d, "f80")
    c.s16(d, "estimate")
    c.s16(d, "color")
    c.s16(d, "f68")
    c.s16(d, "prevTenantType")
    c.s32(d, "style")
    t = c.tag(d, "typeTag")            # 'PYTP' -> T2PeopleType, 'PTPx' none
    if t == "PYTP":
        pt = sub(c, d, "peopleType")   # T2PeopleType::Read 0x1004e6c0
        people_type(c, pt)
    c.values(d, "silhouettes", 3, c.s16)
    c.expect(b">kLp")
    c.u32(d, "linkPrev")
    c.u32(d, "linkNext")


def slot_block(c, d, k, n, fn, marker):
    """A fixed-size object block (n slots).  Unused slots are kept as
    [status, favorite] (they still carry an object header)."""
    if c.reading:
        out = []
        for i in range(n):
            e = {}
            c.trace.append("%s[%d]" % (k, i))
            fn(c, e)
            c.trace.pop()
            if e.get("used"):
                out.append(e)
            else:
                out.append([e["status"], e["favorite"]] if (e["status"] or e["favorite"]) else None)
        d[k] = out
        return out
    for e in d[k]:
        if isinstance(e, dict):
            fn(c, e)
        else:
            st, fav = e if e else (0, 0)
            if marker:
                c.buf += marker
            c.buf += struct.pack("<BhB", 0, st, fav)
    return d[k]


def block_list(c, d, key, per, fn, marker, count_prim):
    n = c.count_of(d, "nBlocks", key, count_prim)
    if c.reading:
        d[key] = []
        for b in range(n):
            blk = {}
            c.trace.append("%s#%d" % (key, b))
            slot_block(c, blk, "slots", per, fn, marker)
            c.trace.pop()
            d[key].append(blk)
    else:
        for blk in d[key]:
            slot_block(c, blk, "slots", per, fn, marker)


# ---------------------------------------------------------------------------
# Floor info (T2FloorInfo::Read 0x10033db0)
# ---------------------------------------------------------------------------

def floor_info(c, d):
    v = c.s32(d, "vRange")       # floors (rows)
    h = c.s32(d, "hRange")       # columns (units)
    c.expect(b"ItnU")
    if c.reading:
        units = c._take(v * h * 4)
        d["unitMap"] = units.hex()   # v*h x (u16 tenantID, u16 moverID/requestID)
    else:
        c.buf += bytes.fromhex(d["unitMap"])
    c.expect(b"AtnT")
    tl = sub(c, d, "tenants")
    c.s32(tl, "nextTenantNumber")      # T2TenantArrayList +0x20
    block_list(c, tl, "blocks", 128, tenant, b">NT<", c.s32)
    c.expect(b"ArvM")
    block_list(c, sub(c, d, "movers"), "blocks", 32, mover, b">VM<", c.s32)
    c.expect(b"AqeR")
    block_list(c, sub(c, d, "requests"), "blocks", 64, request, b">QR<", c.s32)
    c.expect(b"AbOO")
    block_list(c, sub(c, d, "outObjs"), "blocks", 64, outobj, b"", c.u32)
    c.expect(b"ItuO")
    oi = sub(c, d, "outside")          # T2OutsideInfo::Read 0x10047790
    n = c.count_of(oi, "count", "ids", c.u32)
    c.values(oi, "ids", n, c.u16)


# ---------------------------------------------------------------------------
# Pool / demand (T2Pool ctor 0x100502b0, T2PeopleDemandList 0x1004bde0)
# ---------------------------------------------------------------------------

def people_type(c, e):
    """T2PeopleType::Read 0x1004e6c0 (file order -> object offset)."""
    c.s8(e, "f0")               # +0x00
    c.s8(e, "silhouetteType")   # +0x04 (GetAge/IsMale derive from it)
    c.s8(e, "econoType")        # +0x08
    c.s8(e, "transportType")    # +0x10
    c.s8(e, "life")             # +0x0c (127 = immortal?)


def type_array(c, d):
    n = c.count_of(d, "count", "types", c.s32)
    c.items(d, "types", n, people_type)
    c.u32(d, "f24")
    c.s32(d, "f28")
    c.s32(d, "f2c")


def time_zone(c, d):
    n = c.count_of(d, "count", "typeArrays", c.s32)
    c.bool8(d, "fixed")
    c.s8(d, "f28")
    c.s8(d, "currentItem")
    c.s8(d, "f30")
    c.items(d, "typeArrays", n, type_array)


def pool(c, d):
    c.s16(d, "gameLevel")
    c.s32(d, "population")
    c.s32(d, "f18")
    n = c.count_of(d, "nTimeZones", "timeZones", c.u32)
    c.items(d, "timeZones", n, time_zone)


# ---------------------------------------------------------------------------
# Document (T2.exe FUN_00419a50 load / FUN_0041b23f save)
# ---------------------------------------------------------------------------

def plugin_info(c, d):
    """CPluginInfo::Read 0x10010720."""
    c.tag(d, "type")         # DESC class: lPnT tenant, lPvM mover, lPoO outobj, lPdW world, lPeH hall event, ...
    c.tag(d, "id")           # CFilePlugin mID 4cc (the code itemtype-catalog.md lists)
    c.s16(d, "itemType")     # numeric mItemType; equipType/outObjType in records refer to this
    c.s16(d, "mustItem")     # nonzero = load fails if plugin missing
    c.pstr(d, "name")
    c.pstr(d, "fileName")
    c.bool8(d, "used")


def tower_event(c, d):
    c.tag(d, "type")         # event class 4cc
    c.s16(d, "id")
    c.u8(d, "standby")       # 0 = list +0x10/+0x30, 1 = list +0x50
    n = c.s32(d, "len")      # every T2EventItem::Write starts with its payload length
    c.raw(d, "data", n)


def name_rec(c, d):
    c.u16(d, "type")         # 1 tenant, 2 ?, 3 mover?, 5 people (T2Name ctors)
    c.u32(d, "id")
    c.bool8(d, "favorite")
    c.s32(d, "len")          # strlen+1
    c.pstr(d, "name")        # u8 len (= strlen+1) + bytes incl. NUL


def movie(c, d):
    c.tag(d, "id")
    n = c.u32(d, "len")
    c.raw(d, "data", n)


def def_cost(c, e):
    c.s16(e, "itemType")
    c.s32(e, "amount")


def document(c, d):
    c.expect(MAGIC)
    c.tag(d, "world")                 # world plugin mID (lPdW)
    h = sub(c, d, "header")
    c.s16(h, "level")                 # doc +0x8c (tower star level)
    c.s16(h, "f9c")                   # doc +0x9c
    c.u32(h, "fa0")                   # doc +0xa0
    c.s16(h, "f80")                   # doc +0x80
    c.u8(h, "f88")                    # doc +0x88 (writer: 0)
    c.s16(h, "speed")                 # UT2Coordinate::ChangeSpeedMtoF(GetSpeed)
    c.s16(h, "zero")                  # always 0
    c.s16(h, "fa4")                   # doc +0xa4 (read into a local)
    c.values(h, "reserved", 10, c.u8)  # 3 + 7 zero bytes
    dt = sub(c, d, "date")            # T2DateTime(T2Archive&) 0x1001c910
    c.u16(dt, "year")
    c.u16(dt, "month0")               # 0-based month index
    c.u16(dt, "minutes")              # raw minutes since 00:00 (0..1439)
    c.u16(dt, "seconds")
    n = c.count_of(d, "nPlugins", "plugins", c.s32)
    c.items(d, "plugins", n, plugin_info)
    pl = sub(c, d, "people")
    block_list(c, pl, "blocks", 256, people, b">PP<", c.s32)
    c.trace.append("floor")
    floor_info(c, sub(c, d, "floor"))
    c.trace.pop()
    fl = sub(c, d, "queue")          # tenants queued for something (doc +0x1c8 CObjectQueue)
    has = c.s16(fl, "present")
    if has > 0:
        n = c.count_of(fl, "count", "tenants", c.u32)
        c.values(fl, "tenants", n, c.s16)
    c.bool8(d, "f1e0")
    c.s16(d, "f1f0")
    c.trace.append("pool")
    pool(c, sub(c, d, "pool"))
    c.trace.pop()
    # T2TrafficInfo::Read 0x1006b890: one (u16 traffic, u16 range) per
    # cell over the world's build rect (T2WorldDef +0x200..+0x20c / floor
    # lines +0x68/+0x6c; height x width/4).  Those dims live in the world
    # plugin, not the save, so the reader locates the end of the map by
    # validating the fixed-shape tail (see find_traffic_cells).
    if c.reading:
        ncell = find_traffic_cells(c.buf, c.pos)
        d["trafficCells"] = ncell
        d["trafficMap"] = c._take(ncell * 4).hex()
    else:
        c.buf += bytes.fromhex(d["trafficMap"])
    tail(c, d)


def tail(c, d):
    st = sub(c, d, "settlement")      # T2Settlement(T2TowerDoc*,T2Archive&) 0x100591d0
    c.s32(st, "funds")
    c.s32(st, "previousFunds")
    c.values(st, "totals", 5, c.s32)
    for k in ("tenantCosts", "moverCosts", "outObjCosts"):
        n = c.count_of(st, "n" + k, k, c.u32)
        c.items(st, k, n, def_cost)
    n = c.count_of(d, "nMovies", "movies", c.u32)
    c.items(d, "movies", n, movie)
    te = sub(c, d, "towerEvent")      # T2TowerEvent::Read 0x10069fc0
    c.u32(te, "latestBuild")
    n = c.count_of(te, "count", "events", c.s16)
    c.items(te, "events", n, tower_event)
    ui = sub(c, d, "ui")
    c.srect(ui, "mainWindow")
    c.s32(ui, "scrollH")
    c.s32(ui, "scrollV")
    c.srect(ui, "toolPalette")
    c.srect(ui, "controlPalette")
    c.srect(ui, "messageWindow")
    c.srect(ui, "unused")
    c.values(ui, "flags", 4, c.u8)
    c.u16(d, "searchedTenant")        # doc +0x174
    nl = sub(c, d, "names")
    n = c.count_of(nl, "count", "names", c.s32)
    c.items(nl, "names", n, name_rec)
    # T2WorldDef vtable +0xd0 ReadStatisticsProc: world-plugin specific.
    # The base T2WorldDef reads nothing; the shipped worlds (Kegon.dll
    # WorldDef::ReadStatisticsProc 0x10015360 -> FUN_1000dc30) write a
    # 265-byte block: s16,s16,s16 swapped, u8 enabled, s16, 64 x s32.
    if c.reading:
        rest = len(c.buf) - c.pos
        if rest == 265:
            ws = sub(c, d, "worldStats")
            world_stats(c, ws)
        else:
            d["worldStats"] = {"raw": bytes(c.buf[c.pos:]).hex()}
            c.pos = len(c.buf)
    else:
        ws = d["worldStats"]
        if "raw" in ws:
            c.buf += bytes.fromhex(ws["raw"])
        else:
            world_stats(c, ws)


def world_stats(c, d):
    c.s16(d, "a")
    c.s16(d, "b")
    c.s16(d, "swapped")
    c.bool8(d, "enabled")
    c.s16(d, "f49")
    c.values(d, "values", 64, c.s32)


# cells per world observed so far (Hawaii 'IAWH' = 51 x 84, Tokyo 'KJNS');
# tried first, then every n is tried.
KNOWN_TRAFFIC_CELLS = (4284, 8066)


def find_traffic_cells(buf, pos):
    """Smallest cell count n for which everything after the traffic map
    parses and the constant UI flags (four 0x01 bytes, then an all-zero
    SRect is written before them) are where the writer puts them."""
    remaining = len(buf) - pos
    for n in [n for n in KNOWN_TRAFFIC_CELLS if n * 4 <= remaining] + list(range(remaining // 4 + 1)):
        c = Codec(bytes(buf))
        c.pos = pos + n * 4
        d = {}
        try:
            tail(c, d)
        except (BldError, UnicodeDecodeError, struct.error, ValueError):
            continue
        ui = d["ui"]
        if ui["flags"] == [1, 1, 1, 1] and ui["unused"] == [0, 0, 0, 0]:
            return n
    raise BldError("could not locate end of traffic map after 0x%x" % pos)


def parse(data):
    c = Codec(data)
    d = {}
    document(c, d)
    return d


def build(d):
    c = Codec()
    document(c, d)
    return bytes(c.buf)


# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

def used(blocks):
    for b in blocks:
        for s in b["slots"]:
            if isinstance(s, dict):
                yield s


def summarize(d):
    plugins = d["plugins"]
    by_item = {}
    for p in plugins:
        by_item.setdefault(p["itemType"], p)
    tenants = list(used(d["floor"]["tenants"]["blocks"]))
    movers = list(used(d["floor"]["movers"]["blocks"]))
    outs = list(used(d["floor"]["outObjs"]["blocks"]))
    ppl = list(used(d["people"]["blocks"]))

    def count(objs, key):
        res = {}
        for o in objs:
            p = by_item.get(o[key])
            label = "%s %s" % (p["id"], p["name"]) if p else "itemType %d" % o[key]
            res[label] = res.get(label, 0) + 1
        return dict(sorted(res.items(), key=lambda kv: -kv[1]))

    world = next((p for p in plugins if p["type"] == "lPdW" and p["id"] == d["world"]), None)
    dt = d["date"]
    return {
        "world": "%s %s" % (d["world"], world["name"] if world else "?"),
        "date": "year %d, month %d, %02d:%02d" % (dt["year"], dt["month0"] + 1,
                                                  dt["minutes"] // 60, dt["minutes"] % 60),
        "level": d["header"]["level"],
        "funds": d["settlement"]["funds"],
        "previousFunds": d["settlement"]["previousFunds"],
        "population": d["pool"]["population"],
        "peopleObjects": len(ppl),
        "grid": [d["floor"]["vRange"], d["floor"]["hRange"]],
        "plugins": len(plugins),
        "pluginsUsed": sum(1 for p in plugins if p["used"]),
        "tenants": len(tenants),
        "tenantsByPlugin": count(tenants, "equipType"),
        "movers": len(movers),
        "moversByPlugin": count(movers, "equipType"),
        "outObjs": len(outs),
        "outObjsByPlugin": count(outs, "outObjType"),
        "names": [n["name"].rstrip("\x00") for n in d["names"]["names"]],
        "towerEvents": [e["type"] for e in d["towerEvent"]["events"]],
        "worldStats": "raw %d bytes" % (len(d["worldStats"]["raw"]) // 2) if "raw" in d["worldStats"] else "standard 265-byte block",
    }


def strip_bulk(d):
    d = dict(d)
    d["floor"] = dict(d["floor"])
    d["floor"]["unitMap"] = "<%d bytes omitted; use --full>" % (len(d["floor"]["unitMap"]) // 2)
    d["trafficMap"] = "<%d bytes omitted; use --full>" % (len(d["trafficMap"]) // 2)
    return d


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("dump"); p.add_argument("bld"); p.add_argument("-o"); p.add_argument("--full", action="store_true")
    p = sp.add_parser("summary"); p.add_argument("bld")
    p = sp.add_parser("roundtrip"); p.add_argument("bld", nargs="+")
    p = sp.add_parser("write"); p.add_argument("json"); p.add_argument("-o", required=True)
    a = ap.parse_args(argv)

    if a.cmd == "dump":
        d = parse(open(a.bld, "rb").read())
        out = d if a.full else strip_bulk(d)
        s = json.dumps(out, indent=1, ensure_ascii=False)
        if a.o:
            open(a.o, "w", encoding="utf-8").write(s + "\n")
        else:
            print(s)
    elif a.cmd == "summary":
        print(json.dumps(summarize(parse(open(a.bld, "rb").read())), indent=1, ensure_ascii=False))
    elif a.cmd == "roundtrip":
        ok = True
        for path in a.bld:
            data = open(path, "rb").read()
            d = parse(data)
            d2 = json.loads(json.dumps(d))       # prove the JSON form is sufficient
            out = build(d2)
            same = out == data
            ok &= same
            if same:
                print("OK   %s (%d bytes)" % (path, len(data)))
            else:
                i = next((i for i in range(min(len(out), len(data))) if out[i] != data[i]), min(len(out), len(data)))
                print("FAIL %s: first diff at 0x%x (orig %d bytes, rebuilt %d)" % (path, i, len(data), len(out)))
        return 0 if ok else 1
    elif a.cmd == "write":
        d = json.load(open(a.json, encoding="utf-8"))
        if isinstance(d["floor"]["unitMap"], str) and d["floor"]["unitMap"].startswith("<"):
            raise SystemExit("JSON was dumped without --full; cannot rebuild grids")
        open(a.o, "wb").write(build(d))
    return 0


if __name__ == "__main__":
    sys.exit(main())
