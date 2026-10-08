/* Copyright (c) 2026 OpenSkyscraper Project */
#include "YootWorld.h"

#include "AbstractPlugin.h"
#include "PluginManager.h"
#include "PluginResources.h"
#include "WindowsPEExecutable.h"

#include <cctype>
#include <cstring>

namespace OT
{
	namespace
	{
		/// CResFile binary reader: leading 0x01, then 4-byte LE scalars.
		class ResReader
		{
		public:
			ResReader(const char * data, int length) : p_(data), end_(data + (length > 0 ? length : 0)), ok_(true)
			{
				if (!data || length < 1 || uint8_t(data[0]) != 0x01) { ok_ = false; return; }
				++p_;
			}
			int i()
			{
				if (!ok_ || end_ - p_ < 4) { ok_ = false; return 0; }
				const uint8_t * b = reinterpret_cast<const uint8_t *>(p_);
				p_ += 4;
				return int32_t(uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24));
			}
			uint32_t u() { return uint32_t(i()); }
			std::string s()
			{
				if (!ok_ || end_ - p_ < 1) { ok_ = false; return std::string(); }
				const int n = uint8_t(*p_);
				if (end_ - p_ < n + 2) { ok_ = false; return std::string(); }
				std::string v(p_ + 1, p_ + 1 + n);
				p_ += n + 2; // length byte + bytes + NUL
				return v;
			}
			int left() const { return int(end_ - p_); }
			std::string rest() { std::string v(p_, end_); p_ = end_; return v; }
			bool ok() const { return ok_; }
		private:
			const char * p_;
			const char * end_;
			bool ok_;
		};

		/// CResFile text-mode integer tokenizer (CResFile::Token 0x10011290):
		/// separators are space/tab/CR/LF/comma, "(...)" is a comment.
		bool readTextInts(const char * data, int length, std::vector<int> & out)
		{
			const char * p = data;
			const char * end = data + length;
			while (p < end)
			{
				const char c = *p;
				if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == '\0') { ++p; continue; }
				if (c == '(')
				{
					while (p < end && *p != ')') ++p;
					if (p < end) ++p;
					continue;
				}
				bool neg = false;
				if (c == '-') { neg = true; ++p; }
				if (p >= end || !std::isdigit(uint8_t(*p))) return false;
				long long v = 0;
				while (p < end && std::isdigit(uint8_t(*p))) { v = v * 10 + (*p - '0'); ++p; }
				out.push_back(int(neg ? -v : v));
			}
			return true;
		}

		/// T2EV stores the type tag as an LE int whose big-endian bytes spell
		/// e.g. "FBEv"; the resource type is that string uppercased.
		std::string eventResourceType(uint32_t tag)
		{
			std::string s;
			for (int shift = 24; shift >= 0; shift -= 8)
				s.push_back(char(std::toupper(int((tag >> shift) & 0xff))));
			return s;
		}

		const AbstractPlugin * findPlugin(const PluginManager & plugins, const char * file)
		{
			AbstractPlugin * p = plugins.get(file);
			if (p && p->ensureLoaded()) return p;
			return nullptr;
		}
	}

	long long YootWorld::initialFundsDollars() const
	{
		const int div = exchangeDiv ? exchangeDiv : 1;
		return (long long)initialFunds / div * exchangeMul;
	}

	bool YootWorld::isUnitBuildable(int unit, int row) const
	{
		if (unbuildable.empty() || unit < 0 || row < 0 || unit >= finfWidth || row >= finfHeight)
			return true;
		return unbuildable[size_t(row) * finfWidth + unit] == 0;
	}

	const YootEvent * YootWorld::findEvent(const std::string & resType) const
	{
		for (const YootEvent & e : events)
			if (e.resType == resType && e.found) return &e;
		return nullptr;
	}

	const char * yootWorldIdentifier(YootWorld::Id id)
	{
		switch (id)
		{
		case YootWorld::Id::Tokyo:  return "tokyo";
		case YootWorld::Id::Hawaii: return "hawaii";
		case YootWorld::Id::Kegon:  return "kegon";
		}
		return "tokyo";
	}

	const char * yootWorldDisplayName(YootWorld::Id id)
	{
		switch (id)
		{
		case YootWorld::Id::Tokyo:  return "Tokyo";
		case YootWorld::Id::Hawaii: return "Hawaii";
		case YootWorld::Id::Kegon:  return "Kegon";
		}
		return "Tokyo";
	}

	const char * yootWorldPluginFile(YootWorld::Id id)
	{
		switch (id)
		{
		case YootWorld::Id::Tokyo:  return "Tokyo.t2p";
		case YootWorld::Id::Hawaii: return "Hawai.t2p";
		case YootWorld::Id::Kegon:  return "Kegon.t2p";
		}
		return "Tokyo.t2p";
	}

	bool parseYootWorldIdentifier(const std::string & s, YootWorld::Id & out)
	{
		if (s == "tokyo")  { out = YootWorld::Id::Tokyo;  return true; }
		if (s == "hawaii") { out = YootWorld::Id::Hawaii; return true; }
		if (s == "kegon")  { out = YootWorld::Id::Kegon;  return true; }
		return false;
	}

	// T2WorldDef::T2WorldDef 0x1006e4e0 read order.
	bool parseYootWorldDef(const char * data, int length, YootWorld & w)
	{
		ResReader r(data, length);
		YootWorld t = w;
		t.sortKey = r.i();
		t.height = r.i();
		t.width = r.i();
		t.groundLine = r.i();
		t.buildArea.top = r.i();
		t.buildArea.left = r.i();
		t.buildArea.bottom = r.i();
		t.buildArea.right = r.i();
		t.entranceY = r.i(); // POINT is stored y, x
		t.entranceX = r.i();
		t.topFloorLine = r.i();
		t.bottomFloorLine = r.i();
		t.startGrade = r.i();
		t.startMonth = r.i();
		t.lobbyHeight = r.i();
		t.outViewKind = r.i();
		t.unk17c = r.i();
		t.tiledBkgnd = r.i();
		const int n = r.i();
		if (!r.ok() || n < 0 || n > 8) return false;
		t.grades.clear();
		for (int k = 0; k < n; ++k)
		{
			YootGradeRow g;
			g.vipFlag = r.u();
			g.population = r.i();
			g.animId = r.i();
			g.animFrames = r.i();
			g.unk = r.i();
			g.animParam = r.i();
			t.grades.push_back(g);
		}
		t.bkgndTileHeight = r.i();
		t.bkgndTileWidth = r.i();
		t.unk188 = r.i();
		t.unk18c = r.i();
		t.discarded10.clear();
		for (int k = 0; k < 10; ++k) t.discarded10.push_back(r.i());
		t.gradeIconAnimIds.clear();
		for (int k = 0; k < 5; ++k) t.gradeIconAnimIds.push_back(r.i());
		t.towerNamePrefix = r.s();
		t.dustOnAt = r.i();
		t.toiletAt = r.i();
		t.toiletDenomi = r.i();
		t.discarded9999 = r.i();
		const int c = r.i();
		if (!r.ok() || c < 0 || c > 64) return false;
		t.cashAmounts.clear();
		for (int k = 0; k < c; ++k) t.cashAmounts.push_back(r.i());
		if (!r.ok()) return false;
		w = t;
		return true;
	}

	bool parseYootWatr(const char * data, int length, YootWorld & w)
	{
		ResReader r(data, length);
		std::vector<int> ints;
		for (int k = 0; k < 6; ++k) ints.push_back(r.i());
		const int funds = r.i();
		const int mul = r.i();
		const int div = r.i();
		const std::string unit = r.s();
		if (!r.ok()) return false;
		w.watrInts = ints;
		w.initialFunds = funds;
		w.exchangeMul = mul;
		w.exchangeDiv = div;
		w.moneyUnit = unit;
		return true;
	}

	// T2WorldPlugin::InitFloorInfoProc 0x100730d0 -> T2UnitInfo::InitMask.
	bool parseYootFinf(const char * data, int length, YootWorld & w)
	{
		std::vector<int> v;
		if (!data || !readTextInts(data, length, v) || v.size() < 7) return false;
		const int h = v[0], wd = v[1];
		if (h <= 0 || wd <= 0 || v.size() < 7 + size_t(h) * wd) return false;
		w.finfHeader.assign(v.begin(), v.begin() + 7);
		w.finfHeight = h;
		w.finfWidth = wd;
		w.unbuildable.resize(size_t(h) * wd);
		for (size_t k = 0; k < w.unbuildable.size(); ++k)
			w.unbuildable[k] = v[7 + k] != 0 ? 1 : 0;
		return true;
	}

	bool parseYootEventResource(const std::string & resType, const char * data, int length, YootEvent & e)
	{
		ResReader r(data, length);
		YootEventHeader h;
		h.levelBit = r.i();
		h.exclusive = r.i();
		h.cycleDay = r.i();
		h.beginMin = r.i();
		h.endMin = r.i();
		if (!r.ok()) return false;
		e.header = h;
		e.body.clear();
		e.vips.clear();
		e.bodyTail.clear();
		e.text.clear();
		if (resType == "VIEV")
		{
			const int n = r.i();
			for (int k = 0; r.ok() && k < n; ++k)
			{
				YootVipDef v;
				v.grade = r.i();
				v.destPluginId = r.u();
				v.arrivalMin = r.i();
				v.f3c = r.i();
				v.f40 = r.i();
				v.f44 = r.i();
				v.viplRes = r.i();
				v.goodMsg = r.s();
				v.badMsg = r.s();
				if (r.ok()) e.vips.push_back(v);
			}
			if (!r.ok()) return false;
			assignYootVipFlags(e.vips);
			e.bodyTail = r.rest();
		}
		else if (resType == "MAEV")
		{
			// T2Maru_Reggae ctor 0x1003a770: 4 ints, then the claim pstr
			// (empty for Mr. Maru, "'Uncle Reggae is noisy.'" for Reggae).
			for (int k = 0; k < 4; ++k) e.body.push_back(r.i());
			e.text = r.s();
			if (!r.ok()) return false;
			e.bodyTail = r.rest();
		}
		else
		{
			while (r.left() >= 4) e.body.push_back(r.i());
			e.bodyTail = r.rest();
		}
		e.found = true;
		return true;
	}

	void assignYootVipFlags(std::vector<YootVipDef> & vips)
	{
		// T2VisitVIP::Init: flag = 1 << 4(g-1), shifted left while it collides
		// with the flags already handed out (shared accumulator at +0x88).
		uint32_t used = 0;
		for (YootVipDef & v : vips)
		{
			uint32_t flag = v.grade >= 1 ? (1u << ((4 * (v.grade - 1)) & 31)) : 1u;
			while (flag & used) flag <<= 1;
			used |= flag;
			v.vipFlag = flag;
		}
	}

	// T2WorldDef::T2WorldDef 0x1006e4e0 write order (inverse of parseYootWorldDef).
	bool encodeYootWorldDef(const YootWorld & t, std::string & out)
	{
		if (t.discarded10.size() != 10 || t.gradeIconAnimIds.size() != 5) return false;
		CResWriter w;
		w.i32(t.sortKey);
		w.i32(t.height);
		w.i32(t.width);
		w.i32(t.groundLine);
		w.i32(t.buildArea.top);
		w.i32(t.buildArea.left);
		w.i32(t.buildArea.bottom);
		w.i32(t.buildArea.right);
		w.i32(t.entranceY); // POINT is stored y, x
		w.i32(t.entranceX);
		w.i32(t.topFloorLine);
		w.i32(t.bottomFloorLine);
		w.i32(t.startGrade);
		w.i32(t.startMonth);
		w.i32(t.lobbyHeight);
		w.i32(t.outViewKind);
		w.i32(t.unk17c);
		w.i32(t.tiledBkgnd);
		w.count(t.grades.size());
		for (const YootGradeRow & g : t.grades)
		{
			w.u32(g.vipFlag);
			w.i32(g.population);
			w.i32(g.animId);
			w.i32(g.animFrames);
			w.i32(g.unk);
			w.i32(g.animParam);
		}
		w.i32(t.bkgndTileHeight);
		w.i32(t.bkgndTileWidth);
		w.i32(t.unk188);
		w.i32(t.unk18c);
		for (int v : t.discarded10) w.i32(v);
		for (int v : t.gradeIconAnimIds) w.i32(v);
		w.str(t.towerNamePrefix);
		w.i32(t.dustOnAt);
		w.i32(t.toiletAt);
		w.i32(t.toiletDenomi);
		w.i32(t.discarded9999);
		w.count(t.cashAmounts.size());
		for (int v : t.cashAmounts) w.i32(v);
		return w.finish(out);
	}

	bool encodeYootWatr(const YootWorld & t, std::string & out)
	{
		if (t.watrInts.size() != 6) return false;
		CResWriter w;
		for (int v : t.watrInts) w.i32(v);
		w.i32(t.initialFunds);
		w.i32(t.exchangeMul);
		w.i32(t.exchangeDiv);
		w.str(t.moneyUnit);
		return w.finish(out);
	}

	bool encodeYootFinf(const YootWorld & t, std::string & out)
	{
		if (t.finfHeader.size() != 7 || t.finfWidth <= 0 || t.finfHeight <= 0
		    || t.unbuildable.size() != size_t(t.finfHeight) * size_t(t.finfWidth))
			return false;
		out.clear();
		for (int v : t.finfHeader) { out += std::to_string(v); out += "\r\n"; }
		for (uint8_t v : t.unbuildable) out += v ? "1\r\n" : "0\r\n";
		return true;
	}

	bool encodeYootEventResource(const YootEvent & e, std::string & out)
	{
		CResWriter w;
		w.i32(e.header.levelBit);
		w.i32(e.header.exclusive);
		w.i32(e.header.cycleDay);
		w.i32(e.header.beginMin);
		w.i32(e.header.endMin);
		if (e.resType == "VIEV")
		{
			w.count(e.vips.size());
			for (const YootVipDef & v : e.vips)
			{
				w.i32(v.grade);
				w.u32(v.destPluginId);
				w.i32(v.arrivalMin);
				w.i32(v.f3c);
				w.i32(v.f40);
				w.i32(v.f44);
				w.i32(v.viplRes);
				w.str(v.goodMsg);
				w.str(v.badMsg);
			}
		}
		else if (e.resType == "MAEV")
		{
			for (int v : e.body) w.i32(v);
			w.str(e.text);
		}
		else
		{
			for (int v : e.body) w.i32(v);
		}
		if (!w.finish(out)) return false;
		out += e.bodyTail;
		return true;
	}

	bool encodeYootTowerEvents(const std::vector<YootEvent> & events, std::string & out)
	{
		CResWriter w;
		w.count(events.size());
		for (const YootEvent & e : events)
		{
			w.u32(e.typeTag);
			w.i32(e.resId);
		}
		return w.finish(out);
	}

	bool loadYootWorld(const AbstractPlugin & plugin, YootWorld::Id id, YootWorld & out, std::string * error)
	{
		YootWorld w;
		w.id = id;
		w.pluginName = plugin.name();
		const WindowsPEExecutable::Resource * def = plugin.getResource("WORLDDEF", 1);
		if (!def || !parseYootWorldDef(def->data, def->length, w))
		{
			if (error) *error = "WORLDDEF missing or malformed";
			return false;
		}
		const WindowsPEExecutable::Resource * watr = plugin.getResource("WATR", 128);
		if (!watr || !parseYootWatr(watr->data, watr->length, w))
		{
			if (error) *error = "WATR missing or malformed";
			return false;
		}
		YootPluginIdentity ident;
		if (readYootPluginIdentity(plugin, ident))
			w.worldMask = ident.attrib;
		if (const WindowsPEExecutable::Resource * finf = plugin.getResource("FINF", 1))
		{
			if (!parseYootFinf(finf->data, finf->length, w) && error)
				*error = "FINF malformed (ignored)";
		}
		if (const WindowsPEExecutable::Resource * t2ev = plugin.getResource("T2EV", 1))
		{
			// T2TowerEvent::T2TowerEvent 0x10068ca0: count, then {typeTag, resID}.
			ResReader r(t2ev->data, t2ev->length);
			const int n = r.i();
			for (int k = 0; r.ok() && k < n; ++k)
			{
				YootEvent e;
				e.typeTag = r.u();
				e.resId = r.i();
				if (!r.ok()) break;
				e.resType = eventResourceType(e.typeTag);
				if (const WindowsPEExecutable::Resource * res = plugin.getResource(e.resType, e.resId))
					parseYootEventResource(e.resType, res->data, res->length, e);
				w.events.push_back(e);
			}
		}
		w.fromPlugin = true;
		out = w;
		return true;
	}

	YootWorld builtinYootWorld(YootWorld::Id id)
	{
		// Decoded from the shipped world plugins (world_rules.py output);
		// see doc/yoottower/world-and-sp-formats.md §2.2-2.3.
		YootWorld w;
		w.id = id;
		w.pluginName = yootWorldPluginFile(id);
		w.exchangeMul = 100;
		w.exchangeDiv = 1;
		w.moneyUnit = "$";
		auto grades = [&](const int (&rows)[4][2]) {
			for (int k = 0; k < 4; ++k)
			{
				YootGradeRow g;
				g.vipFlag = uint32_t(rows[k][0]);
				g.population = rows[k][1];
				w.grades.push_back(g);
			}
		};
		switch (id)
		{
		case YootWorld::Id::Tokyo:
		{
			w.sortKey = 0; w.height = 122; w.width = 368; w.groundLine = 110;
			w.buildArea.left = 36; w.buildArea.top = 6; w.buildArea.right = 332; w.buildArea.bottom = 120;
			w.topFloorLine = 10; w.bottomFloorLine = 119; w.startGrade = 1; w.startMonth = 0; w.lobbyHeight = 3;
			const int rows[4][2] = {{1, 1000}, {16, 2000}, {256, 5000}, {4096, 12000}};
			grades(rows);
			w.watrInts = {230, 129, 128, 100, 10, 354};
			w.initialFunds = 100000; w.worldMask = 0x7;
			break;
		}
		case YootWorld::Id::Hawaii:
		{
			w.sortKey = 20; w.height = 62; w.width = 400; w.groundLine = 55;
			w.buildArea.left = 56; w.buildArea.top = 3; w.buildArea.right = 392; w.buildArea.bottom = 62;
			w.topFloorLine = 10; w.bottomFloorLine = 61; w.startGrade = 2; w.startMonth = 1; w.lobbyHeight = 2;
			const int rows[4][2] = {{1, 1}, {16, 500}, {256, 2000}, {4096, 8000}};
			grades(rows);
			w.watrInts = {230, 129, 128, 45, 6, 400};
			w.initialFunds = 50000; w.worldMask = 0xF01;
			break;
		}
		case YootWorld::Id::Kegon:
		{
			w.sortKey = 10; w.height = 44; w.width = 192; w.groundLine = 10;
			w.buildArea.left = 81; w.buildArea.top = 3; w.buildArea.right = 186; w.buildArea.bottom = 43;
			w.topFloorLine = 8; w.bottomFloorLine = 42; w.startGrade = 3; w.startMonth = 2; w.lobbyHeight = 1;
			const int rows[4][2] = {{1, 1}, {16, 1}, {256, 200}, {4096, 1000}};
			grades(rows);
			w.watrInts = {230, 129, 128, 2, 32, 36};
			w.initialFunds = 20000; w.worldMask = 0xF0;
			break;
		}
		}
		return w;
	}

	YootWorld loadYootWorld(const PluginManager & plugins, YootWorld::Id id, std::string * error)
	{
		YootWorld w;
		const AbstractPlugin * p = findPlugin(plugins, yootWorldPluginFile(id));
		if (p && loadYootWorld(*p, id, w, error))
			return w;
		if (error && !p) *error = std::string(yootWorldPluginFile(id)) + " not found";
		return builtinYootWorld(id);
	}

	bool readYootPluginIdentity(const char * data, int length, YootPluginIdentity & out)
	{
		// The shared ATTR codec (PluginResources.h) now names every field.
		PluginAttr a;
		if (!parsePluginAttr(data, length, a)) return false;
		YootPluginIdentity p;
		p.id = a.id.toUint32LE();
		p.attrib = a.attrib;
		p.dependId = a.dependID.toUint32LE();
		p.gameLevel = a.gameLevel;
		p.secretItem = a.secretItem;
		p.itemType = a.itemType;
		p.mustItem = a.mustItem;
		p.name = a.name;
		out = p;
		return true;
	}

	bool readYootPluginIdentity(const AbstractPlugin & plugin, YootPluginIdentity & out)
	{
		const WindowsPEExecutable::Resource * r = plugin.getResource("ATTR", 128);
		return r && readYootPluginIdentity(r->data, r->length, out);
	}

	bool yootPluginAvailable(const YootPluginIdentity & p, uint32_t worldMask, int grade, bool secretMode)
	{
		// FUN_0041ca80: world.mAttrib & spec.attrib, spec.level <= doc grade,
		// and secret items only in secret mode.
		if ((worldMask & p.attrib) == 0) return false;
		if (p.gameLevel > grade) return false;
		if (p.secretItem != 0 && !secretMode) return false;
		return true;
	}

	const std::vector<YootPrototypeLink> & yootPrototypeLinks()
	{
		// OpenSky prototype -> YootTower plugins of the same role. A prototype
		// is offered when ANY linked plugin is available in the world; its
		// unlock grade is the lowest mGameLevel among those. Prototypes with no
		// row here (secom) keep the SimTower lock table (LevelUp::minRatingToBuild).
		static const std::vector<YootPrototypeLink> links = {
			{"lobby",             {"Floor.t2p", "KFloor.t2p", nullptr}},
			{"floor",             {"Floor.t2p", "KFloor.t2p", nullptr}},
			{"stairs",            {"Stair.t2p", nullptr}},
			{"escalator",         {"Escalator.t2p", nullptr}},
			{"elevator-standard", {"StdElev.t2p", "KElev.t2p", nullptr}},
			{"elevator-express",  {"BigElev.t2p", nullptr}},
			{"elevator-service",  {"ServElev.t2p", "KServ.t2p", nullptr}},
			{"fastfood",          {"Burger.t2p", nullptr}},
			{"restaurant",        {"Sushibar.t2p", "Itarian.t2p", "Chinese.t2p", "OldTimeCafe.t2p", nullptr}},
			{"shop",              {"Record.t2p", "Bookstore.t2p", "Electric.t2p", "Photo.t2p", nullptr}},
			{"hotel_single",      {"Singleroom.t2p", "Smallroom.t2p", nullptr}},
			{"hotel_double",      {"Twinroom.t2p", "Smallroom.t2p", nullptr}},
			{"hotel_suite",       {"Suiteroom.t2p", "Largeroom.t2p", nullptr}},
			{"partyhall",         {"EventCenter.t2p", nullptr}},
			{"cinema",            {"Theater.t2p", "ClassicTheater.t2p", nullptr}},
			{"metro",             {"Subway.t2p", nullptr}},
			{"office",            {"Office.t2p", nullptr}},
			{"parking",           {"Parking.t2p", nullptr}},
			{"condo",             {"Condo.t2p", "HawaianCondo.t2p", nullptr}},
			{"yoot_condo",        {"Condo.t2p", "HawaianCondo.t2p", nullptr}},
			{"security",          {"Securityroom.t2p", nullptr}},
			{"medicalcenter",     {"Doctor's.t2p", nullptr}},
			{"recycling",         {"TrashFacility.t2p", "KTrashFacility.t2p", nullptr}},
			{"cathedral",         {"Cathedral.t2p", nullptr}},
			{"powerroom",         {"PowerRoom.t2p", nullptr}},
		};
		return links;
	}

	std::map<std::string, YootUnlock> resolveYootUnlocks(const PluginManager & plugins, uint32_t worldMask)
	{
		std::map<std::string, YootUnlock> out;
		for (const YootPrototypeLink & link : yootPrototypeLinks())
		{
			bool anyKnown = false;
			YootUnlock u;
			for (int k = 0; k < 5 && link.plugins[k]; ++k)
			{
				const AbstractPlugin * p = findPlugin(plugins, link.plugins[k]);
				YootPluginIdentity ident;
				if (!p || !readYootPluginIdentity(*p, ident)) continue;
				anyKnown = true;
				// Grade-independent part of the filter: world mask + secret.
				if (!yootPluginAvailable(ident, worldMask, ident.gameLevel)) continue;
				if (!u.available || ident.gameLevel < u.minGrade) u.minGrade = ident.gameLevel;
				u.available = true;
			}
			if (anyKnown) out[link.prototypeId] = u;
		}
		return out;
	}

	std::vector<std::string> yootPrototypesForPluginId(const PluginManager & plugins, uint32_t pluginId)
	{
		std::vector<std::string> out;
		for (const YootPrototypeLink & link : yootPrototypeLinks())
		{
			for (int k = 0; k < 5 && link.plugins[k]; ++k)
			{
				const AbstractPlugin * p = findPlugin(plugins, link.plugins[k]);
				YootPluginIdentity ident;
				if (p && readYootPluginIdentity(*p, ident) && ident.id == pluginId)
				{
					out.push_back(link.prototypeId);
					break;
				}
			}
		}
		return out;
	}
}
