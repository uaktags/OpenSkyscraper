/* Copyright (c) 2026 OpenSkyscraper Project */
#include "BldLoader.h"

#include "ItemTypeCatalog.h"
#include "YootPool.h"
#include "YootTenants.h"
#include "YootWorld.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace OT {
namespace Bld {

	// ---- Stream primitives (T2Archive, doc/yoottower/bld-format.md §1) -----

	static const char MAGIC[4] = {'0', '0', '2', 'T'}; // u32 0x54323030 'T200'
	static const size_t MAX_FILE_BYTES = 64u * 1024u * 1024u;

	struct BldError : std::runtime_error
	{
		explicit BldError(const std::string & m) : std::runtime_error(m) {}
	};

	/// Symmetric reader/writer, the C++ twin of bld.py's Codec: every
	/// primitive takes a reference, reads into it or writes from it.
	class Archive
	{
	public:
		/// Reader over `in` starting at `pos`.
		Archive(const std::vector<uint8_t> & in, size_t pos) : writing(false), in(&in), pos(pos) {}
		/// Writer.
		Archive() : writing(true), in(nullptr), pos(0) {}

		const bool writing;
		std::vector<uint8_t> out;

		size_t offset() const { return writing ? out.size() : pos; }
		size_t remaining() const { return writing ? 0 : in->size() - pos; }
		const std::vector<uint8_t> & input() const { return *in; }

		void push(const char * what) { trace.push_back(what); }
		void pop() { trace.pop_back(); }

		[[noreturn]] void fail(const std::string & what) const
		{
			char at[32];
			snprintf(at, sizeof(at), " at 0x%zx", offset());
			std::string path;
			for (size_t i = trace.size() > 4 ? trace.size() - 4 : 0; i < trace.size(); i++)
			{
				if (!path.empty()) path += "/";
				path += trace[i];
			}
			throw BldError(what + at + (path.empty() ? "" : " in " + path));
		}

		void bytes(void * p, size_t n)
		{
			if (writing)
			{
				const uint8_t * b = (const uint8_t *)p;
				out.insert(out.end(), b, b + n);
				return;
			}
			if (n > remaining())
			{
				char m[64];
				snprintf(m, sizeof(m), "unexpected EOF (need %zu)", n);
				fail(m);
			}
			memcpy(p, in->data() + pos, n);
			pos += n;
		}

		template <class T> void le(T & v)
		{
			uint8_t b[sizeof(T)];
			if (writing)
			{
				typedef typename std::make_unsigned<T>::type U;
				U u = (U)v;
				for (size_t i = 0; i < sizeof(T); i++) b[i] = (uint8_t)(u >> (8 * i));
				bytes(b, sizeof(T));
				return;
			}
			bytes(b, sizeof(T));
			typedef typename std::make_unsigned<T>::type U;
			U u = 0;
			for (size_t i = 0; i < sizeof(T); i++) u |= (U)((U)b[i] << (8 * i));
			v = (T)u;
		}

		void u8(uint8_t & v) { le(v); }
		void s8(int8_t & v) { le(v); }
		void u16(uint16_t & v) { le(v); }
		void s16(int16_t & v) { le(v); }
		void u32(uint32_t & v) { le(v); }
		void s32(int32_t & v) { le(v); }

		/// A 4-byte tag kept in file byte order.
		void tag(std::string & s)
		{
			char b[4] = {0, 0, 0, 0};
			if (writing)
			{
				if (s.size() != 4) fail("bad 4cc '" + s + "'");
				memcpy(b, s.data(), 4);
			}
			bytes(b, 4);
			if (!writing) s.assign(b, 4);
		}

		/// A constant marker such as '>NT<'.
		void expect(const char * m)
		{
			char b[4];
			memcpy(b, m, 4);
			if (writing) { bytes(b, 4); return; }
			bytes(b, 4);
			if (memcmp(b, m, 4) != 0)
			{
				pos -= 4;
				fail(std::string("expected '") + std::string(m, 4) + "', got '" + printable(b) + "'");
			}
		}

		void blob(std::vector<uint8_t> & v, size_t n)
		{
			if (!writing)
			{
				if (n > remaining()) fail("blob runs past EOF");
				v.assign(in->begin() + pos, in->begin() + pos + n);
				pos += n;
				return;
			}
			if (!v.empty()) bytes(v.data(), v.size());
		}

		/// T2Archive::ReadSRect: four big-endian s16, top,left,bottom,right.
		void srect(Rect & r)
		{
			uint8_t b[8];
			if (writing)
			{
				const int v[4] = {r.top, r.left, r.bottom, r.right};
				for (int i = 0; i < 4; i++) { b[2 * i] = (uint8_t)((uint16_t)v[i] >> 8); b[2 * i + 1] = (uint8_t)v[i]; }
				bytes(b, 8);
				return;
			}
			bytes(b, 8);
			int v[4];
			for (int i = 0; i < 4; i++) v[i] = (int16_t)(uint16_t)((b[2 * i] << 8) | b[2 * i + 1]);
			r.top = v[0]; r.left = v[1]; r.bottom = v[2]; r.right = v[3];
		}

		/// u8 length + bytes.
		void pstr(std::string & s)
		{
			uint8_t n = 0;
			if (writing)
			{
				if (s.size() > 255) fail("pstr longer than 255 bytes");
				n = (uint8_t)s.size();
			}
			u8(n);
			if (writing) { if (n) bytes(&s[0], n); return; }
			s.resize(n);
			if (n) bytes(&s[0], n);
		}

		/// Count of a list: written from `size`, read and sanity-checked
		/// against the bytes left (each element needs at least `minBytes`).
		template <class T> size_t count(T & n, size_t size, size_t minBytes, const char * what)
		{
			if (writing)
			{
				n = (T)size;
				if ((size_t)n != size) fail(std::string(what) + ": count does not fit its field");
			}
			le(n);
			if (writing) return size;
			if ((long long)n < 0) fail(std::string(what) + ": negative count");
			if (minBytes && (unsigned long long)n * minBytes > remaining())
				fail(std::string(what) + ": count runs past EOF");
			return (size_t)n;
		}

	private:
		static std::string printable(const char * b)
		{
			std::string s;
			for (int i = 0; i < 4; i++)
			{
				const unsigned char c = (unsigned char)b[i];
				if (c >= 32 && c < 127) s += (char)c;
				else { char h[8]; snprintf(h, sizeof(h), "\\x%02x", c); s += h; }
			}
			return s;
		}

		const std::vector<uint8_t> * in;
		size_t pos;
		std::vector<std::string> trace;
	};

	/// Runs `fn` with a trace breadcrumb (error messages name the path).
	template <class F> static void scoped(Archive & a, const char * what, F fn)
	{
		a.push(what);
		fn();
		a.pop();
	}

	// ---- LArray serialisations (T2DLL LArray::ReadAs*) ----------------------

	static void xfer(Archive & a, LArrayWord & d)
	{
		a.s32(d.itemSize);
		int32_t n32 = 0;
		const size_t n = a.count(n32, d.items.size(), 0, "LArray count");
		a.s32(d.h2);
		a.s32(d.h3);
		a.u8(d.h4);
		a.u8(d.h5);
		a.s32(d.dataBytes);
		if (!a.writing)
		{
			if (n * 2 > a.remaining()) a.fail("LArray word items run past EOF");
			d.items.resize(n);
		}
		for (uint16_t & v : d.items) a.u16(v);
		if (!a.writing)
		{
			const int32_t isz = d.itemSize ? d.itemSize : 1;
			if (isz < 0) a.fail("LArray negative itemSize");
			const long long pad = std::max(0LL, (long long)(d.dataBytes / isz) - (long long)n);
			if ((unsigned long long)pad * 2 > a.remaining()) a.fail("LArray word padding runs past EOF");
			d.pad.resize((size_t)pad);
		}
		for (uint16_t & v : d.pad) a.u16(v);
	}

	static void xfer(Archive & a, LArrayChar & d)
	{
		a.s32(d.itemSize);
		int32_t n32 = 0;
		const size_t n = a.count(n32, d.items.size(), 0, "LArray count");
		a.s32(d.h2);
		a.s32(d.h3);
		a.s32(d.total);
		if (!a.writing)
		{
			if (n > a.remaining()) a.fail("LArray char items run past EOF");
			d.items.resize(n);
		}
		for (uint8_t & v : d.items) a.u8(v);
		if (!a.writing)
		{
			const long long pad = std::max(0LL, (long long)d.total - (long long)n);
			if ((unsigned long long)pad > a.remaining()) a.fail("LArray char padding runs past EOF");
			d.pad.resize((size_t)pad);
		}
		for (uint8_t & v : d.pad) a.u8(v);
	}

	// ---- T2Object hierarchy (§4) ---------------------------------------------

	static bool xfer(Archive & a, ObjectHeader & h)
	{
		a.u8(h.used);
		a.s16(h.status);
		a.u8(h.favorite);
		return h.used != 0;
	}

	static bool isCeTag(const std::string & t)
	{
		return t == "AEC_" || t == "AECT" || t == "AECR" || t == "AECF" || t == "AECM";
	}

	static bool xferEquip(Archive & a, Equip & d)
	{
		if (!xfer(a, d.hdr)) return false;
		a.u16(d.equipID);
		a.srect(d.area);
		a.s16(d.equipType);
		a.s16(d.drawMode);
		a.u8(d.patIndex);
		a.s16(d.f44);
		a.s16(d.inMoneyIndex);
		a.s16(d.f48);
		a.s32(d.buildDay);
		a.tag(d.ce.tag);
		if (isCeTag(d.ce.tag)) xfer(a, d.ce.ids);
		a.u32(d.valiation);
		return true;
	}

	/// T2EquipDef::LoadExtraData 0x1002f8c0: s32 n + n bytes.
	static void xferExtra(Archive & a, std::vector<uint8_t> & extra)
	{
		int32_t n = 0;
		const size_t c = a.count(n, extra.size(), 1, "extra data");
		a.blob(extra, c);
	}

	static void xfer(Archive & a, Tenant & d)
	{
		a.expect(">NT<");
		if (!xferEquip(a, d)) return;
		a.u8(d.kindIndex);
		a.s32(d.resideDay);
		a.s16(d.f8c);
		a.s16(d.estimate);
		a.u8(d.bind);
		a.u16(d.relatedTenantID);
		a.s16(d.reserveCount);
		a.s16(d.belongCount);
		a.s16(d.customerCount);
		a.s16(d.workCount);
		a.s16(d.totalCustomer);
		a.u16(d.tenantNumber);
		a.u32(d.relatedTenantListID);
		// bld.py: a negative count reads zero marks; keep it verbatim.
		int16_t nMarks = d.nMarks;
		if (a.writing && (nMarks >= 0 || !d.marks.empty())) nMarks = (int16_t)d.marks.size();
		a.s16(nMarks);
		if (!a.writing)
		{
			d.nMarks = nMarks;
			const size_t n = nMarks > 0 ? (size_t)nMarks : 0;
			if (n > a.remaining()) a.fail("tenant marks run past EOF");
			d.marks.resize(n);
		}
		for (uint8_t & m : d.marks) a.u8(m);
		a.expect(">kLp");
		a.u32(d.firstPeople);
		a.u32(d.people64);
		a.u32(d.hasBelongList);
		if (d.hasBelongList)
		{
			int32_t n = 0;
			const size_t c = a.count(n, d.belongList.size(), 4, "belong list");
			if (!a.writing) d.belongList.resize(c);
			for (uint32_t & p : d.belongList) a.u32(p);
		}
		xferExtra(a, d.extra);
	}

	static void xfer(Archive & a, MoverModule & d, const std::string & kind)
	{
		if (!xfer(a, d.hdr)) return;
		a.s16(d.position);
		a.s16(d.direction);
		a.s16(d.waitCounter);
		a.s16(d.patIndex);
		a.s16(d.numPeople);
		a.s16(d.f4c);
		a.u32(d.firstPeople);
		a.u32(d.lastPeople);
		if (kind == "LMlE")
		{
			a.s16(d.index);
			a.s16(d.homePosition);
			a.s16(d.offsetPos);
			a.s16(d.nextStop);
			a.s16(d.f6c);
			a.s16(d.topTurn);
			a.s16(d.bottomTurn);
			a.tag(d.stopTag);
			if (d.stopTag == "AfIS") xfer(a, d.stops);
		}
		else if (kind == "LMtS")
		{
			a.srect(d.moduleRect);
		}
	}

	static void xfer(Archive & a, ModuleList & d)
	{
		a.tag(d.kind);
		if (d.kind != "LMlE" && d.kind != "LMtS" && d.kind != "LMM_") return;
		a.s16(d.f20);
		a.srect(d.rect);
		if (d.kind == "LMM_") return;
		int32_t n = 0;
		const size_t c = a.count(n, d.modules.size(), 4, "mover modules");
		if (!a.writing) d.modules.resize(c);
		for (MoverModule & m : d.modules) xfer(a, m, d.kind);
		if (d.kind == "LMlE") a.srect(d.rect2);
	}

	static void xfer(Archive & a, Mover & d)
	{
		a.expect(">VM<");
		if (!xferEquip(a, d)) return;
		a.u8(d.showShaft);
		a.s16(d.length);
		a.tag(d.reqIDTag);
		if (d.reqIDTag == "ADIR") xfer(a, d.reqIDs);
		a.s16(d.waitCount);
		a.s16(d.f6c);
		xfer(a, d.modules);
		a.s32(d.refCon);
		xferExtra(a, d.extra);
	}

	static void xfer(Archive & a, Request & d)
	{
		a.expect(">QR<");
		if (!xfer(a, d.hdr)) return;
		a.u16(d.requestID);
		a.u16(d.moverID);
		a.u8(d.upDown);
		a.s16(d.f18);
		a.s16(d.f1c);
		a.s8(d.moduleIndex);
		a.s16(d.drawWidth);
		for (uint32_t & p : d.people) a.u32(p);
	}

	static void xfer(Archive & a, OutObj & d)
	{
		if (!xfer(a, d.hdr)) return;
		a.u16(d.outObjID);
		a.srect(d.area);
		a.u32(d.valiation);
		a.s16(d.outObjType);
	}

	static void xfer(Archive & a, PeopleType & t)
	{
		a.s8(t.f0);
		a.s8(t.silhouetteType);
		a.s8(t.econoType);
		a.s8(t.transportType);
		a.s8(t.life);
	}

	static void xfer(Archive & a, People & d)
	{
		a.expect(">PP<");
		if (!xfer(a, d.hdr)) return;
		a.u32(d.matterID);
		a.u16(d.workTenant);
		a.u16(d.homeTenant);
		a.u16(d.currEquipID);
		a.u16(d.destination);
		a.s16(d.curH);
		a.s16(d.curV);
		a.s16(d.dstH);
		a.s16(d.dstV);
		a.s16(d.startTime);
		a.s8(d.direction);
		a.s8(d.walkStyle);
		a.tag(d.returnTag);
		if (d.returnTag == "KTSR")
		{
			int32_t n = 0;
			const size_t c = a.count(n, d.returnStack.size(), 4, "return stack");
			if (!a.writing) d.returnStack.resize(c);
			for (std::pair<uint16_t, uint16_t> & e : d.returnStack) { a.u16(e.first); a.u16(e.second); }
		}
		a.u8(d.specialFlag);
		a.u8(d.walk);
		a.s16(d.stress);
		a.u16(d.f80);
		a.s16(d.estimate);
		a.s16(d.color);
		a.s16(d.f68);
		a.s16(d.prevTenantType);
		a.s32(d.style);
		a.tag(d.typeTag);
		if (d.typeTag == "PYTP") xfer(a, d.type);
		for (int16_t & s : d.silhouettes) a.s16(s);
		a.expect(">kLp");
		a.u32(d.linkPrev);
		a.u32(d.linkNext);
	}

	/// A list of fixed-size slot blocks: count of blocks, then blocks x per.
	/// Every slot is kept (unused ones as their bare object header).
	template <class Count, class T> static void blockList(Archive & a, std::vector<T> & slots, size_t per, const char * what)
	{
		if (a.writing && slots.size() % per != 0) a.fail(std::string(what) + ": slot count is not a whole number of blocks");
		Count n = 0;
		// Each slot needs at least its 4-byte object header.
		const size_t blocks = a.count(n, slots.size() / per, per * 4, what);
		if (!a.writing) slots.resize(blocks * per);
		char crumb[64];
		for (size_t i = 0; i < slots.size(); i++)
		{
			snprintf(crumb, sizeof(crumb), "%s[%zu]", what, i);
			a.push(crumb);
			xfer(a, slots[i]);
			a.pop();
		}
	}

	// ---- Floor info (§4) ------------------------------------------------------

	static void xferFloor(Archive & a, BldData & d)
	{
		a.s32(d.vRange);
		a.s32(d.hRange);
		if (d.vRange < 0 || d.hRange < 0 || d.vRange > 4096 || d.hRange > 4096)
			a.fail("implausible floor grid size");
		a.expect("ItnU");
		const size_t cells = (size_t)d.vRange * (size_t)d.hRange;
		if (a.writing && d.unitMap.size() != cells * 4) a.fail("unit map size does not match the grid");
		a.blob(d.unitMap, cells * 4);
		a.expect("AtnT");
		a.s32(d.nextTenantNumber);
		blockList<int32_t>(a, d.tenants, 128, "tenants");
		a.expect("ArvM");
		blockList<int32_t>(a, d.movers, 32, "movers");
		a.expect("AqeR");
		blockList<int32_t>(a, d.requests, 64, "requests");
		a.expect("AbOO");
		blockList<uint32_t>(a, d.outObjs, 64, "outObjs");
		a.expect("ItuO");
		uint32_t n = 0;
		const size_t c = a.count(n, d.outside.size(), 2, "outside map");
		if (!a.writing) d.outside.resize(c);
		for (uint16_t & v : d.outside) a.u16(v);
	}

	// ---- Pool (§6) --------------------------------------------------------------

	static void xfer(Archive & a, TypeArray & d)
	{
		int32_t n = 0;
		const size_t c = a.count(n, d.types.size(), 5, "people types");
		if (!a.writing) d.types.resize(c);
		for (PeopleType & t : d.types) xfer(a, t);
		a.u32(d.f24);
		a.s32(d.f28);
		a.s32(d.f2c);
	}

	static void xfer(Archive & a, TimeZone & d)
	{
		int32_t n = 0;
		const size_t c = a.count(n, d.typeArrays.size(), 0, "type arrays");
		a.u8(d.fixed);
		a.s8(d.f28);
		a.s8(d.currentItem);
		a.s8(d.f30);
		if (!a.writing)
		{
			if (c * 16 > a.remaining()) a.fail("type arrays: count runs past EOF");
			d.typeArrays.resize(c);
		}
		for (TypeArray & t : d.typeArrays) xfer(a, t);
	}

	static void xfer(Archive & a, Pool & d)
	{
		a.s16(d.gameLevel);
		a.s32(d.population);
		a.s32(d.f18);
		uint32_t n = 0;
		const size_t c = a.count(n, d.timeZones.size(), 8, "time zones");
		if (!a.writing) d.timeZones.resize(c);
		for (TimeZone & t : d.timeZones) xfer(a, t);
	}

	// ---- Document tail: settlement .. world statistics (§8-§11) ------------

	static void xferCosts(Archive & a, std::vector<DefCost> & v, const char * what)
	{
		uint32_t n = 0;
		const size_t c = a.count(n, v.size(), 6, what);
		if (!a.writing) v.resize(c);
		for (DefCost & e : v) { a.s16(e.itemType); a.s32(e.amount); }
	}

	static void xferTail(Archive & a, BldData & d)
	{
		scoped(a, "settlement", [&] {
			a.s32(d.settlement.funds);
			a.s32(d.settlement.previousFunds);
			for (int32_t & t : d.settlement.totals) a.s32(t);
			xferCosts(a, d.settlement.tenantCosts, "tenantCosts");
			xferCosts(a, d.settlement.moverCosts, "moverCosts");
			xferCosts(a, d.settlement.outObjCosts, "outObjCosts");
		});
		{
			uint32_t n = 0;
			const size_t c = a.count(n, d.movies.size(), 8, "movies");
			if (!a.writing) d.movies.resize(c);
			for (Blob4cc & m : d.movies)
			{
				a.tag(m.id);
				uint32_t len = 0;
				const size_t l = a.count(len, m.data.size(), 1, "movie payload");
				a.blob(m.data, l);
			}
		}
		scoped(a, "towerEvent", [&] {
			a.u32(d.latestBuild);
			int16_t n = 0;
			const size_t c = a.count(n, d.events.size(), 11, "tower events");
			if (!a.writing) d.events.resize(c);
			for (TowerEventRec & e : d.events)
			{
				a.tag(e.type);
				a.s16(e.id);
				a.u8(e.standby);
				int32_t len = 0;
				const size_t l = a.count(len, e.data.size(), 1, "event payload");
				a.blob(e.data, l);
			}
		});
		a.srect(d.mainWindow);
		a.s32(d.scrollTop);
		a.s32(d.scrollLeft);
		a.srect(d.toolPalette);
		a.srect(d.controlPalette);
		a.srect(d.messageWindow);
		a.srect(d.unusedRect);
		for (uint8_t & f : d.uiFlags) a.u8(f);
		a.u16(d.searchedTenant);
		{
			int32_t n = 0;
			const size_t c = a.count(n, d.names.size(), 12, "names");
			if (!a.writing) d.names.resize(c);
			for (Name & nm : d.names)
			{
				a.u16(nm.type);
				a.u32(nm.id);
				a.u8(nm.favorite);
				a.s32(nm.len);
				a.pstr(nm.name);
			}
		}
		// T2WorldDef vtbl+0xd0 ReadStatisticsProc: the shipped worlds write
		// a 265-byte block; anything else is kept raw.
		if (!a.writing) d.hasWorldStats = a.remaining() == 265;
		if (d.hasWorldStats)
		{
			a.s16(d.wsA);
			a.s16(d.wsB);
			a.s16(d.wsSwapped);
			a.u8(d.wsEnabled);
			a.s16(d.wsF49);
			for (int32_t & v : d.wsValues) a.s32(v);
		}
		else
		{
			a.blob(d.worldStatsRaw, a.writing ? d.worldStatsRaw.size() : a.remaining());
		}
	}

	/// Traffic-map cell counts seen per world (Hawaii 51 x 84, Tokyo); tried
	/// first, then every count (bld.py find_traffic_cells).
	static const size_t KNOWN_TRAFFIC_CELLS[] = {4284, 8066};

	/// The traffic map's size lives in the world plugin, not the save: find
	/// the smallest cell count after which the fixed-shape tail parses to
	/// EOF with the constant UI bytes (zero SRect, four 0x01) in place.
	static size_t findTrafficCells(const std::vector<uint8_t> & buf, size_t pos)
	{
		const size_t remaining = buf.size() - pos;
		std::vector<size_t> tries;
		for (size_t n : KNOWN_TRAFFIC_CELLS)
			if (n * 4 <= remaining) tries.push_back(n);
		for (size_t n = 0; n <= remaining / 4; n++) tries.push_back(n);
		for (size_t n : tries)
		{
			Archive a(buf, pos + n * 4);
			BldData tail;
			try { xferTail(a, tail); }
			catch (const BldError &) { continue; }
			if (a.remaining() != 0) continue;
			const Rect & u = tail.unusedRect;
			if (tail.uiFlags[0] == 1 && tail.uiFlags[1] == 1 && tail.uiFlags[2] == 1 && tail.uiFlags[3] == 1 &&
			    u.left == 0 && u.top == 0 && u.right == 0 && u.bottom == 0)
				return n;
		}
		char m[96];
		snprintf(m, sizeof(m), "could not locate end of traffic map after 0x%zx", pos);
		throw BldError(m);
	}

	// ---- Document (T2.exe FUN_00419a50 load / FUN_0041b23f save) -----------

	static void xferDocument(Archive & a, BldData & d)
	{
		a.expect(MAGIC);
		a.tag(d.world);
		a.s16(d.level);
		a.s16(d.f9c);
		a.u32(d.fa0);
		a.s16(d.f80);
		a.u8(d.f88);
		a.s16(d.speed);
		a.s16(d.zero);
		a.s16(d.fa4);
		for (uint8_t & r : d.reserved) a.u8(r);
		a.u16(d.year);
		a.u16(d.month0);
		a.u16(d.minutes);
		a.u16(d.seconds);
		scoped(a, "plugins", [&] {
			int32_t n = 0;
			const size_t c = a.count(n, d.plugins.size(), 15, "plugins");
			if (!a.writing) d.plugins.resize(c);
			for (PluginInfo & p : d.plugins)
			{
				a.tag(p.type);
				a.tag(p.id);
				a.s16(p.itemType);
				a.s16(p.mustItem);
				a.pstr(p.name);
				a.pstr(p.fileName);
				a.u8(p.used);
			}
		});
		blockList<int32_t>(a, d.people, 256, "people");
		scoped(a, "floor", [&] { xferFloor(a, d); });
		a.s16(d.queuePresent);
		if (d.queuePresent > 0)
		{
			uint32_t n = 0;
			const size_t c = a.count(n, d.queue.size(), 2, "queue");
			if (!a.writing) d.queue.resize(c);
			for (int16_t & t : d.queue) a.s16(t);
		}
		a.u8(d.f1e0);
		a.s16(d.f1f0);
		scoped(a, "pool", [&] { xfer(a, d.pool); });
		// T2TrafficInfo::Read: one (u16 traffic, u16 range) per cell over the
		// world's build rect; the size comes from the world plugin.
		if (a.writing)
		{
			if (d.trafficMap.size() % 4 != 0) a.fail("traffic map is not whole cells");
		}
		const size_t trafficBytes = a.writing ? d.trafficMap.size() : findTrafficCells(a.input(), a.offset()) * 4;
		a.blob(d.trafficMap, trafficBytes);
		xferTail(a, d);
		if (!a.writing && a.remaining() != 0) a.fail("trailing bytes after world statistics");
	}

	// ---- Public codec entry points --------------------------------------------

	std::string macOrder(const std::string & fileOrder)
	{
		return std::string(fileOrder.rbegin(), fileOrder.rend());
	}

	const PluginInfo * BldData::pluginForItemType(int itemType) const
	{
		for (const PluginInfo & p : plugins)
			if (p.itemType == itemType) return &p;
		return nullptr;
	}

	int BldData::usedTenants() const
	{
		int n = 0;
		for (const Tenant & t : tenants) n += t.hdr.used ? 1 : 0;
		return n;
	}

	int BldData::usedMovers() const
	{
		int n = 0;
		for (const Mover & m : movers) n += m.hdr.used ? 1 : 0;
		return n;
	}

	int BldData::usedPeople() const
	{
		int n = 0;
		for (const People & p : people) n += p.hdr.used ? 1 : 0;
		return n;
	}

	static bool readFileBytes(const std::string & path, std::vector<uint8_t> & out, std::string & error)
	{
		FILE * f = fopen(path.c_str(), "rb");
		if (!f) { error = "could not open file"; return false; }
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (size < 0 || (size_t)size > MAX_FILE_BYTES) { fclose(f); error = "file is not a plausible YootTower save (size)"; return false; }
		out.resize((size_t)size);
		size_t got = size > 0 ? fread(&out[0], 1, (size_t)size, f) : 0;
		fclose(f);
		if (got != (size_t)size) { error = "could not read file"; return false; }
		return true;
	}

	bool looksLikeBld(const std::string & path)
	{
		FILE * f = fopen(path.c_str(), "rb");
		if (!f) return false;
		char b[4] = {0, 0, 0, 0};
		const size_t got = fread(b, 1, 4, f);
		fclose(f);
		return got == 4 && memcmp(b, MAGIC, 4) == 0;
	}

	bool parseBytes(const std::vector<uint8_t> & bytes, BldData & out, std::string & error)
	{
		out = BldData();
		try
		{
			Archive a(bytes, 0);
			xferDocument(a, out);
		}
		catch (const BldError & e)
		{
			error = e.what();
			return false;
		}
		return true;
	}

	bool loadFile(const std::string & path, BldData & out, std::string & error)
	{
		std::vector<uint8_t> bytes;
		if (!readFileBytes(path, bytes, error)) return false;
		return parseBytes(bytes, out, error);
	}

	bool writeBytes(const BldData & in, std::vector<uint8_t> & out, std::string & error)
	{
		try
		{
			Archive a;
			// The writer only reads from the data; the structure functions
			// take references for symmetry with the reader.
			xferDocument(a, const_cast<BldData &>(in));
			out.swap(a.out);
		}
		catch (const BldError & e)
		{
			error = e.what();
			return false;
		}
		return true;
	}

	// ---- Conversion to native XML ---------------------------------------------

	static std::string lower(std::string s)
	{
		for (char & c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		return s;
	}

	std::string pluginStem(const PluginInfo & p)
	{
		// The shipped catalog first: saves record install paths whose file
		// names differ from the vendored ones (Condo = HOUSESALE.T2P).
		if (const ItemTypeEntry * e = lookupPluginId(macOrder(p.id)))
			return e->file;
		std::string f = p.fileName;
		const size_t slash = f.find_last_of("\\/");
		if (slash != std::string::npos) f = f.substr(slash + 1);
		const size_t dot = f.rfind('.');
		if (dot != std::string::npos) f = f.substr(0, dot);
		return f;
	}

	std::string prototypeForPluginStem(const std::string & stem)
	{
		const std::string s = lower(stem);
		// Floor.t2p carries both variations; the lobby is picked per record.
		if (s == "floor" || s == "kfloor") return "floor";
		for (const YootPrototypeLink & link : yootPrototypeLinks())
			for (int k = 0; k < 5 && link.plugins[k]; ++k)
			{
				std::string ls = lower(link.plugins[k]);
				const size_t dot = ls.rfind('.');
				if (dot != std::string::npos) ls = ls.substr(0, dot);
				if (ls == s) return link.prototypeId;
			}
		return "";
	}

	/// Import-only fallback by numeric item-type family for plugins with no
	/// yootPrototypeLinks() row (YootTower groups mItemType by role).
	static std::string familyPrototype(int itemType)
	{
		if (itemType >= 1600 && itemType < 1700) return "shop";
		return "";
	}

	static bool worldFor4cc(const std::string & fileOrder, YootWorld::Id & id)
	{
		const std::string m = macOrder(fileOrder);
		if (m == "SNJK") { id = YootWorld::Id::Tokyo; return true; }
		if (m == "HWAI") { id = YootWorld::Id::Hawaii; return true; }
		if (m == "KGON") { id = YootWorld::Id::Kegon; return true; }
		return false;
	}

	struct Emitted
	{
		std::string xml;
		int minX = INT_MAX, maxX = INT_MIN, minY = INT_MAX, maxY = INT_MIN;
		void grow(int x, int y, int w, int h)
		{
			minX = std::min(minX, x); maxX = std::max(maxX, x + w);
			minY = std::min(minY, y); maxY = std::max(maxY, y + h);
		}
	};

	bool toTowerXml(const BldData & in, const PrototypeLookup & prototypes,
		std::string & xml, ImportReport & report, std::string & error)
	{
		YootWorld::Id worldId;
		if (!worldFor4cc(in.world, worldId))
		{
			error = "unknown YootTower world '" + macOrder(in.world) + "'";
			return false;
		}
		const YootWorld world = builtinYootWorld(worldId);
		report.worldId = yootWorldIdentifier(worldId);
		const int origin = world.width / 2;   // unit = x + width/2
		const int ground = world.groundLine;  // floor f sits on row ground-1-f
		char buf[512];
		if (in.hRange != world.width || in.vRange != world.height)
		{
			snprintf(buf, sizeof(buf), "Save grid %dx%d differs from the %s world (%dx%d); positions may be off.",
				in.hRange, in.vRange, yootWorldDisplayName(worldId), world.width, world.height);
			report.notes.push_back(buf);
		}

		auto label = [&](int itemType) -> std::string
		{
			const PluginInfo * p = in.pluginForItemType(itemType);
			if (!p)
			{
				snprintf(buf, sizeof(buf), "itemType %d", itemType);
				return buf;
			}
			return macOrder(p->id) + " " + p->name;
		};
		auto drop = [&](int itemType, const char * reason)
		{
			report.dropped[label(itemType) + " (" + reason + ")"]++;
		};
		// Bottom floor of a [top, bottom) row span.
		auto floorOf = [&](int bottomRow) { return ground - bottomRow; };

		Emitted out;

		// -- Tenants ----------------------------------------------------------
		struct Span { int left, right; };
		std::map<int, std::vector<Span> > floorSpans; // floor -> spans (coalesced below)
		int nonDefaultRent = 0;
		for (const Tenant & t : in.tenants)
		{
			if (!t.hdr.used) continue;
			const PluginInfo * p = in.pluginForItemType(t.equipType);
			const std::string stem = p ? pluginStem(*p) : "";
			std::string proto = prototypeForPluginStem(stem);
			bool approximated = false;
			if (proto.empty())
			{
				proto = familyPrototype(t.equipType);
				approximated = !proto.empty();
			}
			if (proto.empty()) { drop(t.equipType, "no OpenSky counterpart"); continue; }

			const int x = t.area.left - origin;
			const int y = floorOf(t.area.bottom);
			const int w = t.area.right - t.area.left;
			const int h = t.area.bottom - t.area.top;
			if (w <= 0 || h <= 0) { drop(t.equipType, "empty footprint"); continue; }

			if (proto == "floor" && t.valiation == 1) proto = "lobby"; // Floor.t2p variation 1
			// Parking.t2p FUN_1000cee0: wider than the TNDF's 4 units is a gate.
			if (proto == "parkinglot" && w > 4) proto = "parkinggate";
			int pw = 0, ph = 0;
			if (!prototypes(proto, pw, ph))
			{
				drop(t.equipType, ("prototype '" + proto + "' unavailable").c_str());
				continue;
			}
			if (proto == "floor")
			{
				for (int f = y; f < y + h; f++) floorSpans[f].push_back({x, x + w});
				continue;
			}
			if (proto != "lobby" && proto != "skybridge" && (w != pw || h != ph))
			{
				snprintf(buf, sizeof(buf), "%dx%d footprint, '%s' is %dx%d", w, h, proto.c_str(), pw, ph);
				drop(t.equipType, buf);
				continue;
			}
			if (approximated)
			{
				snprintf(buf, sizeof(buf), "%s imported as the generic '%s'.", label(t.equipType).c_str(), proto.c_str());
				if (std::find(report.notes.begin(), report.notes.end(), buf) == report.notes.end())
					report.notes.push_back(buf);
			}

			// Common attributes. The 1..300 estimate is Item::yootEstimate;
			// evaluation mirrors it /3 like JudgeSystem does.
			std::string attrs;
			snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\"", proto.c_str(), x, y);
			attrs = buf;
			if (t.estimate > 0)
				snprintf(buf, sizeof(buf), " evaluation=\"%.3f\" yootEstimate=\"%d\"", t.estimate / 3.0, (int)t.estimate);
			else
				snprintf(buf, sizeof(buf), " evaluation=\"50.0\"");
			attrs += buf;

			// T2Tenant status: 8 = for rent, 10 = contracted, 12+ = open/working.
			// Offices are let once SetKind picked a look (kind 1..6); status
			// 10/11 offices of kind 0 stand empty (oracle: day/night patterns 0/1).
			int residents = 0;
			for (uint32_t id : t.belongList) residents += id != 0 ? 1 : 0;
			const bool occupied = proto == "office" ? (t.kindIndex > 0 || residents > 0) : t.hdr.status >= 10;
			// The belong list has one slot per staff/resident place; filled
			// slots hold people ids (counted above).
			if (proto == "lobby")
			{
				snprintf(buf, sizeof(buf), " width=\"%d\" height=\"%d\"", w, h);
				attrs += buf;
			}
			else if (proto == "skybridge")
			{
				snprintf(buf, sizeof(buf), " width=\"%d\"", w);
				attrs += buf;
			}
			else if (proto == "office" || proto == "condo" || proto == "yoot_condo" || proto == "apartment")
			{
				snprintf(buf, sizeof(buf), " occupied=\"%d\" population=\"%d\"", occupied ? 1 : 0, occupied ? residents : 0);
				attrs += buf;
				if (proto == "office" && t.kindIndex > 0)
				{
					snprintf(buf, sizeof(buf), " variant=\"%d\"", (t.kindIndex - 1) % 6);
					attrs += buf;
				}
				if (t.inMoneyIndex != 2) nonDefaultRent++;
			}
			else if (proto == "shop")
			{
				// Price selection s <-> rentClass 3 - s (Yoot::selectionFromRentClass).
				const int rentClass = std::max(0, std::min(3, 3 - (int)t.inMoneyIndex));
				snprintf(buf, sizeof(buf), " let=\"%d\" rentClass=\"%d\"", occupied ? 1 : 0, rentClass);
				attrs += buf;
			}
			else if (isYootTenantPrototypeId(proto))
			{
				// Plugin-backed tenant: the T2Tenant status (+0x08) is the
				// family state machine's own (YootTenants.h).
				snprintf(buf, sizeof(buf), " tenantStatus=\"%d\"", (int)t.hdr.status);
				attrs += buf;
			}
			else if (t.inMoneyIndex != 0 && t.inMoneyIndex != 2)
			{
				nonDefaultRent++;
			}
			// T2Equip +0x4c buildDay = CalcTotalDays, the same day count as
			// the OpenSky absolute clock (J2 age term).
			if (t.buildDay >= 0)
			{
				snprintf(buf, sizeof(buf), " buildDay=\"%d\"", (int)t.buildDay);
				attrs += buf;
			}
			out.xml += attrs + "/>\n";
			out.grow(x, y, w, h);
			report.imported[proto]++;
		}
		// One floor item per level: coalesce the level's Floor records.
		int bridgedGaps = 0;
		for (std::map<int, std::vector<Span> >::iterator i = floorSpans.begin(); i != floorSpans.end(); ++i)
		{
			std::vector<Span> & v = i->second;
			std::sort(v.begin(), v.end(), [](const Span & a, const Span & b) { return a.left < b.left; });
			int l = v[0].left, r = v[0].right;
			std::string holes;
			for (size_t k = 1; k < v.size(); k++)
			{
				// Separate runs on one row stay separate (Floor::holes): two
				// towers, possibly joined by a Sky Bridge.
				if (v[k].left > r)
				{
					bridgedGaps++;
					holes += (holes.empty() ? "" : ",") + std::to_string(r) + ":" + std::to_string(v[k].left);
				}
				r = std::max(r, v[k].right);
			}
			snprintf(buf, sizeof(buf), "<item type=\"floor\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" width=\"%d\"", l, i->first, r - l);
			out.xml += buf;
			if (!holes.empty()) out.xml += " holes=\"" + holes + "\"";
			out.xml += "/>\n";
			out.grow(l, i->first, r - l, 1);
			report.imported["floor"]++;
		}
		if (bridgedGaps)
		{
			snprintf(buf, sizeof(buf), "%d gap(s) between floor slabs on one level kept as floor holes.", bridgedGaps);
			report.notes.push_back(buf);
		}
		if (nonDefaultRent)
		{
			snprintf(buf, sizeof(buf), "%d tenant(s) had a non-default rent level that has no per-item setting here.", nonDefaultRent);
			report.notes.push_back(buf);
		}

		// -- Movers -----------------------------------------------------------
		int stopTables = 0;
		for (const Mover & m : in.movers)
		{
			if (!m.hdr.used) continue;
			const PluginInfo * p = in.pluginForItemType(m.equipType);
			const std::string proto = prototypeForPluginStem(p ? pluginStem(*p) : "");
			if (proto.empty()) { drop(m.equipType, "no OpenSky counterpart"); continue; }
			int pw = 0, ph = 0;
			if (!prototypes(proto, pw, ph))
			{
				drop(m.equipType, ("prototype '" + proto + "' unavailable").c_str());
				continue;
			}
			const int x = m.area.left - origin;
			const int w = m.area.right - m.area.left;
			if (proto == "stairs" || proto == "escalator")
			{
				// One flight per stair module (LMtS moduleRect); unverified
				// against a real save (none of the samples has stairs).
				int flights = 0;
				for (const MoverModule & mod : m.modules.modules)
				{
					if (!mod.hdr.used) continue;
					const Rect & r = mod.moduleRect;
					if (r.right - r.left != pw) continue;
					const int fy = floorOf(r.bottom);
					snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>\n", proto.c_str(), r.left - origin, fy);
					out.xml += buf;
					out.grow(r.left - origin, fy, pw, ph);
					report.imported[proto]++;
					flights++;
				}
				if (!flights) drop(m.equipType, "no stair module matched the prototype footprint");
				continue;
			}
			// Elevator: the shaft rect adds a machine row above and a pit row
			// below the `length` floors it serves.
			const int length = m.length;
			if (length <= 0 || w != pw) { drop(m.equipType, "shaft does not fit the prototype"); continue; }
			if (m.area.bottom - m.area.top != length + 2)
			{
				snprintf(buf, sizeof(buf), "%s: shaft rect height %d does not match %d floors + 2.",
					label(m.equipType).c_str(), m.area.bottom - m.area.top, length);
				report.notes.push_back(buf);
			}
			const int bottom = floorOf(m.area.bottom - 1);
			snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" height=\"%d\" showShaft=\"%d\">",
				proto.c_str(), x, bottom, length, m.showShaft ? 1 : 0);
			std::string e = buf;
			// Stops: two request ids per floor from the bottom (one per side);
			// both zero = no stop. Our endpoints always stop.
			if (m.reqIDTag == "ADIR" && (int)m.reqIDs.items.size() >= 2 * length)
			{
				for (int f = 1; f < length - 1; f++)
				{
					if (m.reqIDs.items[2 * f] == 0 && m.reqIDs.items[2 * f + 1] == 0)
					{
						snprintf(buf, sizeof(buf), "<unserviced floor=\"%d\"/>", bottom + f);
						e += buf;
					}
				}
			}
			int cars = 0;
			for (const MoverModule & car : m.modules.modules)
			{
				if (!car.hdr.used) continue;
				const int pos = std::max(0, std::min(length - 1, (int)car.position));
				const int home = std::max(0, std::min(length - 1, (int)car.homePosition));
				snprintf(buf, sizeof(buf), "<car altitude=\"%d.0\" homeFloor=\"%d\"/>", bottom + pos, bottom + home);
				e += buf;
				cars++;
				for (uint8_t s : car.stops.items) if (s) { stopTables++; break; }
			}
			if (!cars)
			{
				snprintf(buf, sizeof(buf), "<car altitude=\"%d.0\" homeFloor=\"%d\"/>", bottom, bottom);
				e += buf;
			}
			out.xml += e + "</item>\n";
			out.grow(x, bottom, w, length);
			report.imported[proto]++;
		}
		if (stopTables)
		{
			snprintf(buf, sizeof(buf), "%d elevator car(s) had a non-empty stop table (byte meanings unknown; not imported).", stopTables);
			report.notes.push_back(buf);
		}

		// -- Things with no place here ----------------------------------------
		for (const OutObj & o : in.outObjs)
			if (o.hdr.used) drop(o.outObjType, "outdoor objects are not modelled");
		const int people = in.usedPeople();
		if (people)
		{
			snprintf(buf, sizeof(buf), "%d people were not imported; the crowd re-simulates (YootTower showed population %d).",
				people, (int)in.pool.population);
			report.notes.push_back(buf);
		}
		if (!in.names.empty())
		{
			snprintf(buf, sizeof(buf), "%zu custom name(s) were not imported.", in.names.size());
			report.notes.push_back(buf);
		}

		// -- Tower header -------------------------------------------------------
		// Money: $100 units -> dollars. Clock: one YootTower month is one
		// day (12 per year), minute-linear (Time linear clock).
		const long long funds = std::max<long long>(INT_MIN, std::min<long long>(INT_MAX, in.fundsDollars()));
		const long long previous = std::max<long long>(INT_MIN, std::min<long long>(INT_MAX, (long long)in.settlement.previousFunds * 100));
		const int rating = std::max(0, std::min(5, (int)in.level - 1));
		const double absolute = (double)std::max(0, (int)in.year - 1) * 12.0 + (double)(in.month0 % 12) +
			std::min(1439, (int)in.minutes) / 1440.0;
		int poiX = 0, poiY = 0;
		if (out.minX <= out.maxX)
		{
			poiX = (out.minX + out.maxX) / 2 * 8;
			poiY = (out.minY + out.maxY) / 2 * 36;
		}
		if (in.speed != -2)
		{
			snprintf(buf, sizeof(buf), "Saved speed %d has no known mapping; imported at normal speed.", (int)in.speed);
			report.notes.push_back(buf);
		}
		snprintf(buf, sizeof(buf),
			"<tower funds=\"%lld\" rating=\"%d\" time=\"%.6f\" speed=\"1\" rainy=\"0\" tool=\"inspector\" ruleset=\"yoottower\" world=\"%s\" yootVipFlags=\"%u\" x=\"%d\" y=\"%d\">\n",
			funds, rating, absolute, report.worldId.c_str(), (unsigned)in.fa0, poiX, poiY);
		xml = buf;
		// doc+0x9c (secret plugins loaded, e.g. after the Kegon hot spring)
		// and the T2TrafficInfo cells (u16 count, u16 accum) over the floor
		// area, 4 units per cell, row-major (YootExtrasSystem's layout).
		if (in.f9c != 0)
		{
			snprintf(buf, sizeof(buf), "yootSecretItems=\"%u\" ", (unsigned)(uint16_t)in.f9c);
			xml.insert(xml.size() - 2, std::string(" ") + buf);
		}
		{
			const size_t cells = in.trafficMap.size() / 4;
			const int cols = (world.buildArea.right - world.buildArea.left) / 4;
			if (cells > 0 && cols > 0 && cells % (size_t)cols == 0)
			{
				std::string tr;
				bool any = false;
				for (size_t c = 0; c < cells; c++)
				{
					const uint8_t * b = &in.trafficMap[c * 4];
					const unsigned count = b[0] | (b[1] << 8), accum = b[2] | (b[3] << 8);
					if (c) tr += ",";
					if (count || accum) { tr += std::to_string(count) + ":" + std::to_string(accum); any = true; }
				}
				if (any) xml.insert(xml.size() - 2, " yootTraffic=\"" + tr + "\"");
			}
		}
		if (!in.pool.timeZones.empty())
		{
			// T2Pool (bld §6) -> the OpenSky pool, so a .bld keeps its visitor
			// demand. The people out of the pool are dropped with the crowd.
			Yoot::Pool pool;
			pool.gameLevel = in.pool.gameLevel;
			pool.population = pool.shownPopulation = in.pool.population;
			pool.shrinkCount = in.pool.f18;
			for (const TimeZone & z : in.pool.timeZones)
			{
				Yoot::PeopleTimeZoneList l;
				l.fixed = z.fixed != 0;
				l.hoursPerZone = z.f28;
				l.current = z.currentItem;
				l.hourInZone = z.f30;
				for (const TypeArray & a : z.typeArrays)
				{
					Yoot::PeopleTypeArray t;
					t.fixed = a.f24 != 0;
					t.start = a.f28;
					t.limit = a.f2c;
					for (const PeopleType & pt : a.types)
					{
						Yoot::PeopleType y;
						y.demand = pt.f0 & 7;
						y.timeZone = (uint8_t(pt.f0) >> 3) & 0x1f;
						y.silhouette = pt.silhouetteType;
						y.econo = pt.econoType;
						y.transport = pt.transportType;
						y.life = pt.life;
						t.types.push_back(y);
					}
					l.zones.push_back(t);
				}
				pool.demands.push_back(l);
			}
			std::string encoded;
			pool.encode(encoded);
			const size_t at = xml.find(" x=\"");
			if (at != std::string::npos) xml.insert(at, " yootPool=\"" + encoded + "\"");
		}
		// Finances: previousFunds is the balance at the start of the period;
		// the per-category totals have unconfirmed meanings, so only the net
		// change is carried over.
		const long long net = funds - previous;
		snprintf(buf, sizeof(buf),
			"<money todayIncome=\"0\" todayExpenses=\"0\" yesterdayIncome=\"0\" yesterdayExpenses=\"0\" lastQuarterBalance=\"%lld\" quarterIncome=\"%lld\" quarterExpenses=\"%lld\"/>\n",
			previous, net > 0 ? net : 0, net < 0 ? -net : 0);
		xml += buf;
		xml += out.xml;
		xml += "</tower>\n";
		return true;
	}

}
}

