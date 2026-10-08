/* Copyright (c) 2026 OpenSkyscraper Project */
#include "BldExport.h"

#include "AbstractPlugin.h"
#include "ItemTypeCatalog.h"
#include "PluginManager.h"
#include "YootCatalog.h"
#include "YootPool.h"
#include "YootTenants.h"
#include "YootWorld.h"
#include "tinyxml2.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <deque>
#include <map>
#include <set>

namespace OT {
namespace Bld {

	// ---- Native XML -> TowerState ----------------------------------------------

	bool parseTowerXml(const std::string & text, TowerState & out, std::string & error)
	{
		tinyxml2::XMLDocument xml;
		if (xml.Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS || !xml.RootElement())
		{
			error = "not a tower XML document";
			return false;
		}
		const tinyxml2::XMLElement * root = xml.RootElement();
		out = TowerState();
		out.ruleset = root->Attribute("ruleset") ? root->Attribute("ruleset") : "";
		out.world = root->Attribute("world") ? root->Attribute("world") : "tokyo";
		out.funds = root->Int64Attribute("funds");
		out.rating = root->IntAttribute("rating");
		out.time = root->DoubleAttribute("time");
		out.vipFlags = root->UnsignedAttribute("yootVipFlags", 0);
		out.secretItems = root->UnsignedAttribute("yootSecretItems", 0);
		out.traffic = root->Attribute("yootTraffic") ? root->Attribute("yootTraffic") : "";
		out.pool = root->Attribute("yootPool") ? root->Attribute("yootPool") : "";
		out.lastQuarterBalance = out.funds;
		if (const tinyxml2::XMLElement * money = root->FirstChildElement("money"))
			out.lastQuarterBalance = money->Int64Attribute("lastQuarterBalance", out.funds);
		for (const tinyxml2::XMLElement * e = root->FirstChildElement("item"); e; e = e->NextSiblingElement("item"))
		{
			TowerItem it;
			it.type = e->Attribute("type") ? e->Attribute("type") : "";
			it.x = e->IntAttribute("x");
			it.y = e->IntAttribute("y");
			it.width = e->IntAttribute("width", 0);
			it.height = e->IntAttribute("height", 0);
			it.occupied = e->BoolAttribute("occupied", false) || e->BoolAttribute("let", false);
			it.yootEstimate = e->IntAttribute("yootEstimate", 0);
			it.rentClass = e->IntAttribute("rentClass", 1);
			it.tenantStatus = e->IntAttribute("tenantStatus", -1);
			it.variant = e->IntAttribute("variant", 0);
			it.buildDay = e->IntAttribute("buildDay", -1);
			if (const char * h = e->Attribute("holes"))
			{
				std::string s(h);
				size_t pos = 0;
				while (pos < s.size())
				{
					size_t end = s.find(',', pos);
					if (end == std::string::npos) end = s.size();
					const std::string tok = s.substr(pos, end - pos);
					const size_t colon = tok.find(':');
					if (colon != std::string::npos)
						it.holes.push_back(std::make_pair(atoi(tok.substr(0, colon).c_str()), atoi(tok.substr(colon + 1).c_str())));
					pos = end + 1;
				}
			}
			for (const tinyxml2::XMLElement * u = e->FirstChildElement("unserviced"); u; u = u->NextSiblingElement("unserviced"))
				it.unserviced.push_back(u->IntAttribute("floor"));
			for (const tinyxml2::XMLElement * c = e->FirstChildElement("car"); c; c = c->NextSiblingElement("car"))
			{
				TowerCar car;
				car.altitude = c->DoubleAttribute("altitude");
				car.homeFloor = c->IntAttribute("homeFloor", (int)std::floor(car.altitude));
				it.cars.push_back(car);
			}
			out.items.push_back(it);
		}
		return true;
	}

	// ---- TowerState -> BldData ---------------------------------------------------

	static const char * worldMac(YootWorld::Id id)
	{
		switch (id)
		{
		case YootWorld::Id::Tokyo:  return "SNJK";
		case YootWorld::Id::Hawaii: return "HWAI";
		case YootWorld::Id::Kegon:  return "KGON";
		}
		return "";
	}

	static LArrayWord wordArray(const std::vector<uint16_t> & items)
	{
		LArrayWord a;
		a.itemSize = 2;
		a.items = items;
		a.h2 = a.h3 = a.dataBytes = (int32_t)(items.size() * 2);
		return a;
	}

	static LArrayChar charArray(size_t n)
	{
		LArrayChar a;
		a.itemSize = 1;
		a.items.assign(n, 0);
		a.h2 = a.h3 = a.total = (int32_t)n;
		return a;
	}

	/// Slot vectors hold whole blocks; ids are base + index.
	template <class T> static void sizeBlocks(std::vector<T> & slots, size_t used, size_t per)
	{
		const size_t blocks = std::max<size_t>(1, (used + per - 1) / per);
		slots.assign(blocks * per, T());
	}

	bool exportTower(const TowerState & tower, const BldData & templ, BldData & out,
		ImportReport & report, std::string & error, const PluginManager * plugins)
	{
		YootWorld::Id worldId;
		if (!parseYootWorldIdentifier(tower.world, worldId))
		{
			error = "unknown world '" + tower.world + "'";
			return false;
		}
		if (macOrder(templ.world) != worldMac(worldId))
		{
			error = "template is a " + macOrder(templ.world) + " save, the tower is " + tower.world;
			return false;
		}
		if (worldId == YootWorld::Id::Kegon)
		{
			error = "Kegon export is not supported (its unit map carries FINF flags)";
			return false;
		}
		const YootWorld world = builtinYootWorld(worldId);
		if (templ.hRange != world.width || templ.vRange != world.height)
		{
			error = "template grid does not match the world";
			return false;
		}
		report = ImportReport();
		report.worldId = tower.world;
		const int origin = world.width / 2;
		const int ground = world.groundLine;
		const int W = templ.hRange, H = templ.vRange;

		auto plugin = [&](const char * mac) -> const PluginInfo *
		{
			for (const PluginInfo & p : templ.plugins)
				if (macOrder(p.id) == mac) return &p;
			return nullptr;
		};
		const PluginInfo * floorPlugin = plugin("FLOR");
		const PluginInfo * officePlugin = plugin("OFFC");
		const PluginInfo * shopPlugin = plugin("ICES");
		const PluginInfo * restroomPlugin = plugin("TOIL");
		const PluginInfo * apartmentPlugin = plugin("HREN");
		const PluginInfo * elevPlugin = plugin("StdE");
		if (!floorPlugin)
		{
			error = "template has no Floor plugin";
			return false;
		}
		auto drop = [&](const std::string & type, const char * reason) { report.dropped[type + " (" + reason + ")"]++; };

		// -- World-wide sections from the template; tower state reset ----------
		out = templ;
		// The star level must be a grade the world has: startGrade .. the
		// last grade-up (5, the "Tower" icon). FUN_0041ca80 (doc vtbl+0x114,
		// run while loading) calls FUN_00420b50, which loads the grade icon
		// WorldDef+0x144+level*4 (gradeIconAnimIds[5] at +0x148..+0x158).
		// Level 6 reads towerNamePrefix (+0x15c) as a bitmap id, T2BitImage
		// throws, and the load's catch shows "File load error".
		const int maxLevel = (int)world.grades.size() + 1;
		out.level = (int16_t)std::max(world.startGrade, std::min(maxLevel, tower.rating + 1));
		if (out.level != tower.rating + 1)
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "Star level %d written as %d (the world's grades are %d..%d).",
				tower.rating + 1, (int)out.level, world.startGrade, maxLevel);
			report.notes.push_back(buf);
		}
		out.fa0 = tower.vipFlags;
		const double t = std::max(0.0, tower.time);
		const long long day = (long long)std::floor(t);
		out.year = (uint16_t)(day / 12 + 1);
		out.month0 = (uint16_t)(day % 12);
		out.minutes = (uint16_t)std::min(1439L, std::lround((t - (double)day) * 1440.0));
		out.seconds = 0;
		for (People & p : out.people) p = People();
		out.pool.population = 0;
		out.pool.f18 = 0;
		out.pool.gameLevel = out.level;
		{
			// T2Pool: the tower's own pool when it has one with the template's
			// shape, else the template's demand put on the save's clock. The
			// template's zone clock is its own (Sample Tokyo Map: about 9
			// hours from a new tower's 00:30), so every list gets
			// T2PeopleDemandList::Init(hour) with the windows of a fresh zone
			// (InitSearchLimit 1.0: start 1, limit count).
			Yoot::Pool own;
			const bool useOwn = own.decode(tower.pool) && own.demands.size() == out.pool.timeZones.size();
			const int hour = out.minutes / 60;
			for (size_t d = 0; d < out.pool.timeZones.size(); ++d)
			{
				TimeZone & z = out.pool.timeZones[d];
				if (useOwn)
				{
					const Yoot::PeopleTimeZoneList & l = own.demands[d];
					z.fixed = l.fixed ? 1 : 0;
					z.f28 = (int8_t)l.hoursPerZone;
					z.currentItem = (int8_t)l.current;
					z.f30 = (int8_t)l.hourInZone;
					z.typeArrays.assign(l.zones.size(), TypeArray());
					for (size_t k = 0; k < l.zones.size(); ++k)
					{
						const Yoot::PeopleTypeArray & a = l.zones[k];
						TypeArray & t = z.typeArrays[k];
						t.f24 = a.fixed ? 1 : 0;
						t.f28 = a.start;
						t.f2c = a.limit;
						for (const Yoot::PeopleType & y : a.types)
						{
							PeopleType pt;
							pt.f0 = (int8_t)((y.demand & 7) | ((y.timeZone & 0x1f) << 3));
							pt.silhouetteType = (int8_t)y.silhouette;
							pt.econoType = (int8_t)y.econo;
							pt.transportType = (int8_t)y.transport;
							pt.life = (int8_t)y.life;
							t.types.push_back(pt);
						}
					}
					continue;
				}
				if (z.f28 > 0)
				{
					z.currentItem = (int8_t)(hour / z.f28);
					z.f30 = (int8_t)(hour % z.f28);
				}
				for (TypeArray & a : z.typeArrays)
				{
					a.f28 = 1;
					a.f2c = (int32_t)a.types.size();
				}
			}
		}
		std::fill(out.trafficMap.begin(), out.trafficMap.end(), 0);
		out.f9c = (int16_t)tower.secretItems;
		{
			// YootExtrasSystem cells -> T2TrafficInfo (u16 count, u16 accum).
			size_t c = 0, pos = 0;
			const std::string & s = tower.traffic;
			while (!s.empty() && pos <= s.size() && (c + 1) * 4 <= out.trafficMap.size())
			{
				size_t end = s.find(',', pos);
				if (end == std::string::npos) end = s.size();
				const std::string tok = s.substr(pos, end - pos);
				const size_t colon = tok.find(':');
				if (colon != std::string::npos)
				{
					const int count = std::max(0, std::min(0xffff, atoi(tok.substr(0, colon).c_str())));
					const int accum = std::max(0, std::min(0xffff, atoi(tok.substr(colon + 1).c_str())));
					uint8_t * b = &out.trafficMap[c * 4];
					b[0] = (uint8_t)count; b[1] = (uint8_t)(count >> 8); b[2] = (uint8_t)accum; b[3] = (uint8_t)(accum >> 8);
				}
				c++;
				pos = end + 1;
			}
		}
		out.settlement.funds = (int32_t)std::max<long long>(INT_MIN, std::min<long long>(INT_MAX, tower.funds / 100));
		out.settlement.previousFunds = (int32_t)std::max<long long>(INT_MIN, std::min<long long>(INT_MAX, tower.lastQuarterBalance / 100));
		for (int32_t & v : out.settlement.totals) v = 0;
		for (std::vector<DefCost> * costs : {&out.settlement.tenantCosts, &out.settlement.moverCosts, &out.settlement.outObjCosts})
			for (DefCost & c : *costs) c.amount = 0;
		out.names.clear();
		out.searchedTenant = 0;
		out.queue.clear();
		for (OutObj & o : out.outObjs) o = OutObj();
		std::fill(out.outside.begin(), out.outside.end(), 0);
		out.unitMap.assign((size_t)W * H * 4, 0);

		auto cell = [&](int row, int unit) -> uint8_t * { return &out.unitMap[((size_t)row * W + unit) * 4]; };
		auto setU16 = [](uint8_t * p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); };
		auto inGrid = [&](const Rect & r) { return r.left >= 0 && r.top >= 0 && r.right <= W && r.bottom <= H && r.left < r.right && r.top < r.bottom; };

		// -- Tenants: floor slabs and lobbies first, then rooms -------------
		struct Pending { Tenant rec; bool slab; };
		std::vector<Pending> tenants;
		auto newTenant = [&](const PluginInfo & p, const Rect & area, bool slab) -> Tenant &
		{
			Pending pd;
			pd.slab = slab;
			Tenant & t = pd.rec;
			t.hdr.used = 1;
			t.hdr.status = 10;
			t.area = area;
			t.equipType = p.itemType;
			t.f8c = 300;
			t.bind = 1;
			t.ce.tag = slab ? "AECF" : "AECT";
			t.ce.ids = wordArray({});
			tenants.push_back(pd);
			return tenants.back().rec;
		};
		for (const TowerItem & it : tower.items)
		{
			if (it.type != "floor" && it.type != "lobby") continue;
			const int h = it.type == "lobby" ? std::max(1, it.height) : 1;
			// Floor holes: one Floor tenant per run of the row.
			std::vector<std::pair<int, int> > runs;
			{
				int from = it.x;
				for (const std::pair<int, int> & hole : it.holes)
				{
					if (hole.first > from) runs.push_back(std::make_pair(from, hole.first));
					from = std::max(from, hole.second);
				}
				if (from < it.x + std::max(1, it.width)) runs.push_back(std::make_pair(from, it.x + std::max(1, it.width)));
			}
			for (const std::pair<int, int> & run : runs)
			{
				Rect area;
				area.left = run.first + origin;
				area.right = run.second + origin;
				area.bottom = ground - it.y;
				area.top = area.bottom - h;
				if (!inGrid(area)) { drop(it.type, "outside the world grid"); continue; }
				Tenant & t = newTenant(*floorPlugin, area, true);
				t.valiation = it.type == "lobby" ? 1 : 0; // Floor.t2p variation 1 = lobby
				if (it.buildDay >= 0) t.buildDay = it.buildDay;
			}
			report.imported[it.type]++;
		}
		const size_t slabCount = tenants.size();
		auto slabAt = [&](int row, int unit) -> int
		{
			for (size_t i = 0; i < slabCount; i++)
			{
				const Rect & a = tenants[i].rec.area;
				if (row >= a.top && row < a.bottom && unit >= a.left && unit < a.right) return (int)i;
			}
			return -1;
		};
		// Plugin-backed tenants name their plugin; find it in the template.
		std::deque<PluginInfo> added;
		auto pluginStemLower = [](const PluginInfo & p)
		{
			std::string stem = pluginStem(p);
			for (char & c : stem) c = (char)tolower((unsigned char)c);
			return stem;
		};
		auto pluginForStem = [&](const char * file) -> const PluginInfo *
		{
			std::string want = file;
			const size_t dot = want.rfind('.');
			if (dot != std::string::npos) want = want.substr(0, dot);
			for (char & c : want) c = (char)tolower((unsigned char)c);
			for (const PluginInfo & p : templ.plugins)
			{
				std::string stem = pluginStem(p);
				for (char & c : stem) c = (char)tolower((unsigned char)c);
				if (stem == want) return &p;
			}
			for (const PluginInfo & p : added)
				if (pluginStemLower(p) == want) return &p;
			// Not in the template: the plugin table is per save, so list the
			// shipped plugin (CPluginInfo of an installed .t2p) in the export.
			for (const ItemTypeEntry & e : kItemTypeCatalog)
			{
				std::string stem = e.file;
				for (char & c : stem) c = (char)tolower((unsigned char)c);
				if (stem != want) continue;
				const std::string cls = e.cls;
				const char * type = cls == "Tenant" ? "lPnT" : cls == "Mover" ? "lPvM" : cls == "OuterObj" ? "lPoO" : nullptr;
				if (!type) return nullptr;
				// T2.exe refuses plugins outside the world's mask ("Plugin
				// Check Error") and drops their tenants.
				YootPluginIdentity ident;
				AbstractPlugin * source = plugins ? plugins->get(std::string(e.file) + ".t2p") : nullptr;
				if (!source || !source->ensureLoaded() || !readYootPluginIdentity(*source, ident) || !(ident.attrib & world.worldMask)) return nullptr;
				PluginInfo info;
				info.type = type;
				info.id = e.id().str();                 // file order
				info.itemType = (int16_t)e.itemType;
				info.mustItem = (int16_t)ident.mustItem;
				info.name = e.name;
				std::string upper = e.file;
				for (char & c : upper) c = (char)toupper((unsigned char)c);
				info.fileName = "C:\\YOOT\\PLUGINS\\" + upper + ".T2P";
				added.push_back(info);
				return &added.back();
			}
			return nullptr;
		};
		if (!officePlugin) officePlugin = pluginForStem("Office.t2p");
		if (!restroomPlugin) restroomPlugin = pluginForStem("Restroom.t2p");
		if (!apartmentPlugin) apartmentPlugin = pluginForStem("Apartment.t2p");
		// Other native YootTower tenants (hotel rooms, facilities, venues):
		// the first linked plugin in the template, its TNDF footprint and
		// variation. Movers and outer objects have no tenant record.
		std::set<const TowerItem *> nativeExported;
		auto nativeTenant = [&](const std::string & type, int & w, int & h, int & staff, int & variation) -> const PluginInfo *
		{
			if (!plugins || type == "floor" || type == "lobby") return nullptr;
			for (const YootPrototypeLink & link : yootPrototypeLinks())
			{
				if (type != link.prototypeId) continue;
				for (int k = 0; k < 5 && link.plugins[k]; ++k)
				{
					AbstractPlugin * source = plugins->get(link.plugins[k]);
					YootItemStats s;
					if (!source || !source->ensureLoaded() ||
						!loadYootItemStats(*source, yootPrototypeVariation(type), s) || s.mover || s.width <= 0) continue;
					const PluginInfo * info = pluginForStem(link.plugins[k]);
					if (!info) continue;
					w = type == "parkinggate" ? 16 : s.width;  // the gate is 16x2 (Csr2)
					h = type == "parkinggate" ? 2 : std::max(1, s.height);
					staff = std::max(0, s.belongCapacity);
					variation = s.variation;
					return info;
				}
			}
			return nullptr;
		};
		for (const TowerItem & it : tower.items)
		{
			const PluginInfo * p = nullptr;
			int w = 0, h = 1, staff = 0, variation = 0;
			const YootTenantPlugin * tenant = isYootTenantPrototypeId(it.type) ? findYootTenantPlugin(it.type) : nullptr;
			if (it.type == "office") { p = officePlugin; w = 9; }
			else if (it.type == "shop") { p = shopPlugin; w = 12; }
			else if (it.type == "restroom") { p = restroomPlugin; w = 9; }
			else if (it.type == "apartment") { p = apartmentPlugin; w = 4; }
			else if (it.type == "skybridge") { p = plugin("SKYB"); w = std::max(1, it.width); }
			else if (tenant)
			{
				p = pluginForStem(tenant->plugin);
				const AbstractPlugin * source = plugins ? plugins->get(tenant->plugin) : nullptr;
				YootTenantDef def;
				if (p && (!source || !source->ensureLoaded() || !loadYootTenantDef(*source, *tenant, def)))
				{
					drop(it.type, "plugin data unavailable");
					continue;
				}
				w = def.width;
				h = def.height;
				staff = def.belongCapacity;
			}
			else if ((p = nativeTenant(it.type, w, h, staff, variation)) != nullptr)
				nativeExported.insert(&it);
			else continue;
			if (!p) { drop(it.type, "plugin missing from the template"); continue; }
			Rect area;
			area.left = it.x + origin;
			area.right = area.left + w;
			area.bottom = ground - it.y;
			area.top = area.bottom - h;
			if (!inGrid(area)) { drop(it.type, "outside the world grid"); continue; }
			const int row = area.bottom - 1;   // the slab under the bottom floor
			const int under = slabAt(row, area.left);
			if (under < 0 || slabAt(row, area.right - 1) != under) { drop(it.type, "not on one floor slab"); continue; }
			Tenant & t = newTenant(*p, area, false);
			// T2Tenant::SetFloorID 0x1005dfa0: one floor id per row, bottom
			// row first; GetFloorID(row) reads entry bottom - row (Hawai.t2p's
			// DrawOptionObj dereferences the top row's floor).
			std::vector<uint16_t> floorIds;
			for (int r = area.bottom - 1; r >= area.top; r--)
			{
				const int slab = slabAt(r, area.left);
				floorIds.push_back((uint16_t)(1000 + (slab >= 0 ? slab : under)));
			}
			t.ce.ids = wordArray(floorIds);
			if (it.buildDay >= 0) t.buildDay = it.buildDay;
			if (it.type == "skybridge")
			{
				t.patIndex = 3;                         // BuildFinish FUN_1000c3f0
				t.estimate = 0;
				t.hasBelongList = 0;
			}
			else if (it.type == "office" || it.type == "apartment")
			{
				t.hdr.status = it.occupied ? 10 : 8;    // contracted / for rent
				t.estimate = (int16_t)(it.yootEstimate > 0 ? it.yootEstimate : (it.occupied ? 300 : 0));
				t.inMoneyIndex = 2;                     // default price selection
				t.hasBelongList = 1;                    // six empty staff places
				t.belongList.assign(it.type == "office" ? 6 : 1, 0); // Apartment.t2p: one resident
				// Office.t2p SetKind: a let office has kind 1..6, kind 0 is unlet.
				if (it.type == "office" && it.occupied) t.kindIndex = (uint8_t)(1 + ((it.variant % 6) + 6) % 6);
			}
			else if (it.type == "restroom")
			{
				t.hdr.status = 30;                      // open, empty (Restroom.t2p FUN_1000cd00)
				t.estimate = 0;
				t.inMoneyIndex = 0;
				t.hasBelongList = 0;                    // no staff
			}
			else if (tenant || nativeExported.count(&it))
			{
				t.valiation = (uint32_t)variation;
				// Just built: a saved status past hiring without staff
				// people would wait for them forever (oracle: an exported
				// status-12 Ice Cream never opened).
				t.hdr.status = 9;
				t.estimate = (int16_t)std::max(0, it.yootEstimate);
				t.inMoneyIndex = 0;                     // single price
				t.hasBelongList = staff > 0 ? 1 : 0;    // one empty place per staff member
				t.belongList.assign(staff, 0);
				// Stadium.t2p LoadExtraData 0x1000be40 leaves the game record
				// unset for n = 0 and the idle then writes through it; save a
				// fresh one as SaveExtraData 0x1000f6a0 does (n = 0x405, two
				// empty team records, 264 bytes).
				if (it.type == "stadium")
				{
					t.extraCount = 0x405;
					t.extra.assign(264, 0);
				}
			}
			else
			{
				// Ice Cream, just built like the plugin tenants above: a
				// status-12 record without staff people never opens.
				t.hdr.status = 9;
				t.estimate = (int16_t)std::max(0, it.yootEstimate);
				t.inMoneyIndex = 0;                     // Ice Cream has a single price
				t.hasBelongList = 1;                    // two empty staff places
				t.belongList.assign(2, 0);
			}
			report.imported[it.type]++;
		}
		for (const TowerItem & it : tower.items)
		{
			if (it.type == "floor" || it.type == "lobby" || it.type == "office" || it.type == "shop" || it.type == "restroom" || it.type == "apartment" || it.type == "skybridge") continue;
			if (isYootTenantPrototypeId(it.type) || nativeExported.count(&it)) continue;
			if (it.type == "elevator-standard" && elevPlugin) continue;
			drop(it.type, "no YootTower counterpart in the exporter");
		}
		// The game writes the table sorted by plugin id (byte order).
		for (const PluginInfo & p : added)
		{
			auto at = std::find_if(out.plugins.begin(), out.plugins.end(), [&](const PluginInfo & q) { return macOrder(q.id) > macOrder(p.id); });
			out.plugins.insert(at, p);
		}
		// Room numbers: per floor, counted from the right.
		{
			std::map<int, std::vector<size_t> > byRow;
			for (size_t i = slabCount; i < tenants.size(); i++) byRow[tenants[i].rec.area.top].push_back(i);
			for (auto & r : byRow)
			{
				std::sort(r.second.begin(), r.second.end(), [&](size_t a, size_t b) { return tenants[a].rec.area.left > tenants[b].rec.area.left; });
				for (size_t k = 0; k < r.second.size(); k++) tenants[r.second[k]].rec.tenantNumber = (uint16_t)(k + 1);
			}
		}

		// -- Simple Elevators, their stops and waiting areas -----------------
		struct PendingMover { Mover rec; std::vector<int> servedSlabs; };
		std::vector<PendingMover> movers;
		std::vector<Request> requests;
		int clampedCars = 0;
		for (const TowerItem & it : tower.items)
		{
			if (it.type != "elevator-standard" || !elevPlugin) continue;
			const int length = it.height;
			const int sl = it.x + origin, sr = sl + 4;
			Rect area;
			area.left = sl; area.right = sr;
			area.bottom = ground - it.y + 1;          // pit row below the bottom floor
			area.top = area.bottom - length - 2;      // machine row above the top floor
			if (length < 2 || !inGrid(area)) { drop(it.type, "shaft outside the world grid"); continue; }
			PendingMover pm;
			Mover & m = pm.rec;
			const uint16_t moverId = (uint16_t)(movers.size() + 1);
			m.hdr.used = 1;
			m.equipID = moverId;
			m.area = area;
			m.equipType = elevPlugin->itemType;
			m.showShaft = 1;
			m.length = (int16_t)length;
			m.reqIDTag = "ADIR";
			m.waitCount = 3;
			m.f6c = 6;
			const std::set<int> unserviced(it.unserviced.begin(), it.unserviced.end());
			std::vector<uint16_t> reqIds;
			for (int f = 0; f < length; f++)
			{
				const int floor = it.y + f;
				const int row = ground - 1 - floor;
				const int slab = slabAt(row, sl);
				const bool stops = slab >= 0 && slabAt(row, sr - 1) == slab &&
					(f == 0 || f == length - 1 || !unserviced.count(floor));
				if (!stops) { reqIds.push_back(0); reqIds.push_back(0); continue; }
				if (std::find(pm.servedSlabs.begin(), pm.servedSlabs.end(), slab) == pm.servedSlabs.end())
					pm.servedSlabs.push_back(slab);
				const Rect & sa = tenants[slab].rec.area;
				for (int side = 0; side < 2; side++)
				{
					Request q;
					q.hdr.used = 1;
					q.requestID = (uint16_t)(1000 + requests.size());
					q.moverID = moverId;
					q.upDown = (uint8_t)side;
					q.f18 = (int16_t)(side == 0 ? sl : sr);
					q.f1c = (int16_t)row;
					q.moduleIndex = -1;
					const int cells = side == 0 ? sl - sa.left : sa.right - sr;
					q.drawWidth = (int16_t)std::max(0, cells - 2);
					reqIds.push_back(q.requestID);
					// Waiting area: the slab's cells on that side of the shaft.
					const int from = side == 0 ? sa.left : sr, to = side == 0 ? sl : sa.right;
					for (int u = from; u < to; u++) setU16(cell(row, u), q.requestID);
					requests.push_back(q);
				}
			}
			m.reqIDs = wordArray(reqIds);
			// Cars (6 slots, the Simple Elevator's maximum).
			m.modules.kind = "LMlE";
			m.modules.f20 = 6;
			m.modules.rect.left = sl; m.modules.rect.right = sr;
			m.modules.rect.top = ground - 1 - it.y; m.modules.rect.bottom = m.modules.rect.top + 1;
			m.modules.rect2.left = 2; m.modules.rect2.top = 2; m.modules.rect2.right = -2; m.modules.rect2.bottom = -2;
			m.modules.modules.resize(6);
			std::vector<TowerCar> cars = it.cars;
			if (cars.empty()) cars.push_back(TowerCar());
			if (cars.size() > 6) { clampedCars += (int)cars.size() - 6; cars.resize(6); }
			for (size_t k = 0; k < cars.size(); k++)
			{
				MoverModule & c = m.modules.modules[k];
				c.hdr.used = 1;
				const int pos = std::max(0, std::min(length - 1, (int)std::lround(cars[k].altitude) - it.y));
				const int home = std::max(0, std::min(length - 1, cars[k].homeFloor - it.y));
				c.position = c.nextStop = c.topTurn = c.bottomTurn = (int16_t)pos;
				c.index = (int16_t)k;
				c.homePosition = (int16_t)home;
				c.stopTag = "AfIS";
				c.stops = charArray((size_t)length * 2);
			}
			// StdElev.t2p overrides LoadExtraData (FUN_1000f4e0): s32 n, then
			// always 2 x 10 four-byte records (a day-type x period table,
			// most likely the car schedule) whatever n says. Written with the
			// values the game saves for a fresh shaft.
			for (int k = 0; k < 2 * 10; k++)
			{
				const uint8_t e[4] = {0, 0, 6, 0};
				m.extra.insert(m.extra.end(), e, e + 4);
			}
			// Shaft cells, machine row to pit row.
			for (int row = area.top; row < area.bottom; row++)
				for (int u = sl; u < sr; u++) setU16(cell(row, u), moverId);
			movers.push_back(pm);
			report.imported[it.type]++;
		}
		if (clampedCars)
		{
			char buf[96];
			snprintf(buf, sizeof(buf), "%d elevator car(s) beyond the Simple Elevator's 6 were not written.", clampedCars);
			report.notes.push_back(buf);
		}

		// -- Cross-equip links -----------------------------------------------
		for (PendingMover & pm : movers)
		{
			// Mover: the floor tenants it serves, top to bottom.
			std::vector<int> slabs = pm.servedSlabs;
			std::sort(slabs.begin(), slabs.end(), [&](int a, int b) { return tenants[a].rec.area.top < tenants[b].rec.area.top; });
			std::vector<uint16_t> ids;
			for (int s : slabs) ids.push_back((uint16_t)(1000 + s));
			pm.rec.ce.tag = "AECM";
			pm.rec.ce.ids = wordArray(ids);
			// Floors: the movers stopping on them.
			for (int s : slabs) tenants[s].rec.ce.ids.items.push_back(pm.rec.equipID);
		}
		for (size_t i = 0; i < slabCount; i++) tenants[i].rec.ce.ids = wordArray(tenants[i].rec.ce.ids.items);

		// -- Unit map tenant ids (rooms over slabs) and the slot arrays ------
		for (int pass = 0; pass < 2; pass++)
			for (size_t i = 0; i < tenants.size(); i++)
			{
				if (tenants[i].slab != (pass == 0)) continue;
				const Rect & a = tenants[i].rec.area;
				for (int row = a.top; row < a.bottom; row++)
					for (int u = a.left; u < a.right; u++) setU16(cell(row, u) + 2, (uint16_t)(1000 + i));
			}
		// Open the main view on the tower (scroll px: 8 per unit, 36 per row;
		// roughly a 740 x 500 view, the game's default window).
		if (!tenants.empty())
		{
			int l = INT_MAX, r = INT_MIN, t0 = INT_MAX, b = INT_MIN;
			for (const Pending & p : tenants)
			{
				l = std::min(l, p.rec.area.left); r = std::max(r, p.rec.area.right);
				t0 = std::min(t0, p.rec.area.top); b = std::max(b, p.rec.area.bottom);
			}
			out.scrollLeft = std::max(0, (l + r) / 2 * 8 - 370);
			out.scrollTop = std::max(0, std::min(H * 36 - 500, (t0 + b) / 2 * 36 - 250));
		}
		sizeBlocks(out.tenants, tenants.size(), 128);
		for (size_t i = 0; i < tenants.size(); i++)
		{
			out.tenants[i] = tenants[i].rec;
			out.tenants[i].equipID = (uint16_t)(1000 + i);
		}
		sizeBlocks(out.movers, movers.size(), 32);
		for (size_t i = 0; i < movers.size(); i++) out.movers[i] = movers[i].rec;
		sizeBlocks(out.requests, requests.size(), 64);
		for (size_t i = 0; i < requests.size(); i++) out.requests[i] = requests[i];
		return true;
	}

}
}
