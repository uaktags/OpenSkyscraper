/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootTenants.h"

#include "AbstractPlugin.h"
#include "PluginManager.h"
#include "YootWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace OT
{
	const std::vector<YootTenantPlugin> & yootTenantPlugins()
	{
		typedef YootTenantFamily F;
		// Families from plugin_code_audit.py (plugin-schema.md §3) and the
		// decompiled overrides (YootTenants.h). Spa runs the shop machine
		// (it pays without the message).
		static const std::vector<YootTenantPlugin> plugins = {
			// Shops (category 80)
			{"Aloha.t2p", "yt_aloha", F::Shop},
			{"BCDMart.t2p", "yt_bcdmart", F::Shop},
			{"Barber.t2p", "yt_barber", F::Shop},
			{"Bookstore.t2p", "yt_bookstore", F::Shop},
			{"Boutique.t2p", "yt_boutique", F::Shop},
			{"Convenience.t2p", "yt_convenience", F::Shop},
			{"Drug.t2p", "yt_drug", F::Shop},
			{"DutyFree.t2p", "yt_dutyfree", F::Shop},
			{"Electric.t2p", "yt_electric", F::Shop},
			{"Flower.t2p", "yt_flower", F::Shop},
			{"Fruits.t2p", "yt_fruits", F::Shop},
			{"Gameworks.t2p", "yt_gameworks", F::Shop},
			{"Icecream.t2p", "yt_icecream", F::Shop},
			{"Juicebar.t2p", "yt_juicebar", F::Shop},
			{"KIcecream.t2p", "yt_kicecream", F::Shop},
			{"Kids.t2p", "yt_kids", F::Shop},
			{"LocalPickle.t2p", "yt_localpickle", F::Shop},
			{"Men'sClothing.t2p", "yt_mensclothing", F::Shop},
			{"OrangeJulius.t2p", "yt_orangejulius", F::Shop},
			{"Petshop.t2p", "yt_petshop", F::Shop},
			{"Photo.t2p", "yt_photo", F::Shop},
			{"Record.t2p", "yt_record", F::Shop},
			{"Souvenirs.t2p", "yt_souvenirs", F::Shop},
			{"Sportsshop.t2p", "yt_sportsshop", F::Shop},
			{"SuperMarket.t2p", "yt_supermarket", F::Shop},
			{"SurfShop.t2p", "yt_surfshop", F::Shop},
			{"Toyworld.t2p", "yt_toyworld", F::Shop},
			{"Video&DVD.t2p", "yt_videodvd", F::Shop},
			{"Spa.t2p", "yt_spa", F::Shop},
			// Restaurants, fast food and the food court (category 50)
			{"Burger.t2p", "yt_burger", F::Restaurant},
			{"Chinese.t2p", "yt_chinese", F::Restaurant},
			{"Cocktail.t2p", "yt_cocktail", F::Restaurant},
			{"Coffee.t2p", "yt_coffee", F::Restaurant},
			{"FoodCoat.t2p", "yt_foodcoat", F::Restaurant},
			{"French.t2p", "yt_french", F::Restaurant},
			{"Indian.t2p", "yt_indian", F::Restaurant},
			{"Itarian.t2p", "yt_itarian", F::Restaurant},
			{"J-Pub.t2p", "yt_jpub", F::Restaurant},
			{"J-Snack.t2p", "yt_jsnack", F::Restaurant},
			{"Japanese.t2p", "yt_japanese", F::Restaurant},
			{"Jenny's.t2p", "yt_jennys", F::Restaurant},
			{"KoreanBBQ.t2p", "yt_koreanbbq", F::Restaurant},
			{"Mexcan.t2p", "yt_mexcan", F::Restaurant},
			{"OldTimeCafe.t2p", "yt_oldtimecafe", F::Restaurant},
			{"Ramen.t2p", "yt_ramen", F::Restaurant},
			{"SeaFood.t2p", "yt_seafood", F::Restaurant},
			{"SobaNoodle.t2p", "yt_sobanoodle", F::Restaurant},
			{"Steak.t2p", "yt_steak", F::Restaurant},
			{"Sushibar.t2p", "yt_sushibar", F::Restaurant},
			{"Trad-Japanese.t2p", "yt_tradjapanese", F::Restaurant},
			{"Weinstein'sToffee.t2p", "yt_weinsteinstoffee", F::Restaurant},
			// Vending machines (Drink.t2p code, byte-identical in all five)
			{"Drink.t2p", "yt_drink", F::Vending},
			{"KDrink.t2p", "yt_kdrink", F::Vending},
			{"Cigar.t2p", "yt_cigar", F::Vending},
			{"kCigar.t2p", "yt_kcigar", F::Vending},
			{"Coin.t2p", "yt_coin", F::Vending},
		};
		return plugins;
	}

	namespace
	{
		std::string lower(std::string s)
		{
			for (char & c : s) c = (char)std::tolower((unsigned char)c);
			return s;
		}
	}

	const YootTenantPlugin * findYootTenantPlugin(const std::string & key)
	{
		const std::string k = lower(key);
		for (const YootTenantPlugin & p : yootTenantPlugins())
			if (k == p.prototypeId || k == lower(p.plugin)) return &p;
		return nullptr;
	}

	bool isYootTenantPrototypeId(const std::string & prototypeId)
	{
		return prototypeId.compare(0, 3, "yt_") == 0 && findYootTenantPlugin(prototypeId) != nullptr;
	}

	const char * yootTenantFamilyName(YootTenantFamily family)
	{
		switch (family)
		{
		case YootTenantFamily::Shop: return "shop";
		case YootTenantFamily::Restaurant: return "restaurant";
		case YootTenantFamily::Vending: return "vending";
		default: return "none";
		}
	}

	bool loadYootTenantDef(const AbstractPlugin & plugin, const YootTenantPlugin & entry, YootTenantDef & out)
	{
		YootPluginIdentity ident;
		TemplateDef def;
		if (!readYootPluginIdentity(plugin, ident)) return false;
		if (!plugin.getTemplateDef(def) || def.kind != TemplateDef::Tenant) return false;
		if (def.tool.variations.empty()) return false;

		YootTenantDef d;
		d.plugin = entry.plugin;
		d.prototypeId = entry.prototypeId;
		d.family = entry.family;
		d.pluginId = ident.id;
		d.worldMask = ident.attrib;
		d.gameLevel = std::max(1, ident.gameLevel);
		d.pluginName = ident.name;

		const ToolVariation & v = def.tool.variations[0];
		d.name = v.name.empty() ? ident.name : v.name;
		d.width = std::max(1, (int)def.tool.width);
		d.height = std::max(1, (int)def.tool.height);
		d.attribute = def.tool.attribute;
		d.category = def.tool.category;
		d.categoryName = def.tool.categoryName;
		d.toolNo = def.tool.toolNo;
		d.price = v.price * 100;      // ExchangeMoney: $100 units
		d.outMoney = v.outMoney * 100;
		d.capacity = def.equip.capacity;
		for (int32_t m : def.equip.inMoney) d.inMoney.push_back(m * 100);
		d.numOfKind = std::max(1, (int)def.tenant.numOfKind);
		d.numOfPattern = std::max(1, (int)def.tenant.numOfPattern);
		d.belongCapacity = std::max(0, (int)def.tenant.belongCapacity);
		d.openMinute = def.tenant.openTime;
		d.closeMinute = def.tenant.closeTime;
		d.bindMinutes = def.tenant.bindTime;

		// T2TenantDef ctor: TMDF id 2 is the customer table; its DPDF/PLDF
		// ids are looked up as stored (J-Snack's 128 is missing, so the
		// engine and this loader both run without them).
		if (plugin.getMemberTable(d.customers, 2))
		{
			d.hasCustomers = true;
			if (d.customers.dayParamID != 0)
				d.hasDayParam = plugin.getDayParamDef(d.dayParam, d.customers.dayParamID);
			if (d.customers.placeParamID != 0)
				d.hasPlaceParam = plugin.getPlaceParamDef(d.placeParam, d.customers.placeParamID);
		}
		out = d;
		return true;
	}

	std::vector<YootTenantDef> loadYootTenantDefs(const PluginManager & plugins)
	{
		std::vector<YootTenantDef> out;
		for (const YootTenantPlugin & entry : yootTenantPlugins())
		{
			AbstractPlugin * p = plugins.get(entry.plugin);
			if (!p || !p->ensureLoaded()) continue;
			YootTenantDef d;
			if (loadYootTenantDef(*p, entry, d)) out.push_back(d);
		}
		return out;
	}

	namespace Yoot
	{
		static int wrapMinute(int m)
		{
			m %= 1440;
			return m < 0 ? m + 1440 : m;
		}

		bool withinMinutes(int now, int a, int b)
		{
			// AdjustMinutes folds both ends into the day.
			a = wrapMinute(a);
			b = wrapMinute(b);
			now = wrapMinute(now);
			if (a < b) return a <= now && now < b;
			if (a <= b) return true;          // a == b: the whole day
			return now < b || a <= now;
		}

		int float2Int(float value, int roll)
		{
			if (!(value > 0.0f)) return 0;
			const int whole = (int)value;
			const int tenths = (int)((value - (float)whole) * 10.0f);
			int r = roll % 10;
			if (r < 0) r += 10;
			return whole + ((tenths != 0 && r < tenths) ? 1 : 0);
		}

		float customerUnits(int capacity, int bindMinutes)
		{
			if (bindMinutes <= 0 || capacity <= 0) return 0.0f;
			if (bindMinutes > 8) return ((float)capacity * 8.0f) / (float)bindMinutes;
			return (float)capacity;
		}

		float memberRate(uint32_t percent, float units)
		{
			if (!(units > 0.0f) || percent == 0) return 0.0f;
			return (float)percent * units * 0.01f;
		}

		int commercialPattern(int status, int inside, int capacity)
		{
			if (status < kStatusOpen || status >= 10000) return 0;
			if (inside <= 0) return 1;
			return inside >= capacity / 2 ? 3 : 2;
		}

		int tenantIncome(const TenantModel & m, const YootTenantDef & def)
		{
			// CalcInMoney: inMoney(priceIndex) x customers / 100 - outMoney,
			// in $100 units. The covered plugins have a single price.
			const int in = def.inMoney.empty() ? 0 : def.inMoney[0] / 100;
			return ((in * m.customers) / 100 - def.outMoney / 100) * 100;
		}

		int tenantEstimate(const TenantModel & m, const YootTenantDef & def)
		{
			// CalcProfitEstimate: 70 x (income + outMoney) / outMoney, 1..300.
			const int out = def.outMoney / 100;
			int e = (tenantIncome(m, def) / 100 + out) * 70;
			if (e < 0) e = 0;
			if (out != 0) e /= out;
			return std::max(1, std::min(300, e));
		}

		namespace
		{
			bool isOpen(int status) { return status >= kStatusOpen && status < 10000; }

			void setPattern(TenantModel & m, int p, TenantEvents & ev)
			{
				if (m.pattern != p) { m.pattern = p; ev.patternChanged = true; }
			}

			void updateEstimate(TenantModel & m, const YootTenantDef & def, TenantEvents & ev)
			{
				const int e = tenantEstimate(m, def);
				if (e != m.estimate) { m.estimate = e; ev.estimateChanged = true; }
			}

			/// PushOutAllCustomer: customers inside and on their way leave.
			void pushOutCustomers(TenantModel & m) { m.inside.clear(); }
			/// PushOutAll: customers and staff leave.
			void pushOutAll(TenantModel & m) { pushOutCustomers(m); m.staffIn = 0; }

			void pay(TenantModel & m, const YootTenantDef & def, TenantEvents & ev)
			{
				ev.paid = true;
				ev.paidCustomers = m.customers;
				ev.income += tenantIncome(m, def);
			}

			/// UT2Utils::Float2Int of a stored percent x 0.01 rate (DPDF /
			/// PLDF floats): non-zero with the rate's probability.
			bool rateRoll(uint32_t percent, const RngFn & rng)
			{
				return float2IntRng((float)percent * 0.01f, [&rng]() { return rng(); }) != 0;
			}

			/// T2Tenant::CollectCustomer 0x1005f440: room, all staff in, the
			/// window open .. close - bindTime - 60, then the pool, tenant and
			/// floor sources in that order, each gated by its PLDF rate (the
			/// pool also by the DPDF holiday rate, weather 0:
			/// T2TenantMemberTableDef::IsCollectFromPool 0x10063430).
			void collectCustomers(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, const RngFn & rng)
			{
				if (!def.hasCustomers || !in.sources) return;
				if (m.insideCount() + m.walking >= def.capacity) return;
				if (m.staffIn != def.belongCapacity) return;
				if (!withinMinutes(in.minute, def.openMinute, def.closeMinute - def.bindMinutes - 60)) return;
				auto freeRoom = [&]() { return std::max(0, def.capacity - m.insideCount() - m.walking); };

				bool pool = !def.hasPlaceParam || rateRoll(def.placeParam.places[0].percent, rng);
				if (pool && def.hasDayParam && !rateRoll(def.dayParam.percent[in.holiday ? 1 : 0][0], rng)) pool = false;
				if (pool) m.walking += in.sources->fromPool(def, freeRoom(), in);
				if (def.hasPlaceParam && rateRoll(def.placeParam.places[1].percent, rng))
					m.walking += in.sources->fromTenant(def, freeRoom(), in);
				if (def.hasPlaceParam && rateRoll(def.placeParam.places[2].percent, rng))
					m.walking += in.sources->fromFloor(def, freeRoom(), in);
			}

			/// Shop / restaurant ChangeStatusOnIdle (Barber/Burger FUN_1000bc20).
			void commercialIdle(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, const RngFn & rng, TenantEvents & ev)
			{
				const bool restaurant = def.family == YootTenantFamily::Restaurant;
				const int open = def.openMinute, close = def.closeMinute;
				const int now = in.minute;
				const int s = m.status;
				if (!isOpen(s))
				{
					if (s == kStatusClosed || s == kStatusClosed2)
					{
						if (withinMinutes(now, open - 60, open - 52))
						{
							// CallBelongPeople: the staff commute in for the opening.
							m.staffIn = in.reachable ? m.staff : 0;
							m.status = kStatusCalling;
							if (restaurant)
							{
								m.customers = 0;
								m.walking = 0;
								updateEstimate(m, def, ev);
							}
						}
						else if (restaurant)
							pushOutAll(m);
					}
					else if (s == kStatusCalling)
					{
						if (withinMinutes(now, open, close))
						{
							if (m.staffIn == def.belongCapacity)
							{
								if (!restaurant)
								{
									m.customers = 0;
									m.walking = 0;
								}
								m.status = kStatusOpen;
								setPattern(m, 1, ev);
								if (!restaurant) updateEstimate(m, def, ev);
							}
						}
						else if (withinMinutes(now, close, close + 8))
						{
							if (!restaurant) pushOutAll(m);
							m.status = kStatusClosed2;
						}
					}
					else if (s == kStatusAfterClose)
					{
						// Garbage run (FUN_1000c480); trash itself is FacilitySystem's.
						m.staffIn = 0;
						m.status = kStatusClosed2;
					}
					else if (s == kStatusNight)
					{
						if (now < 8) m.status = kStatusClosed;
					}
					else if (s == kStatusHiring)
					{
						// CollectEmployee from the pool at the top of an hour.
						if (now % 60 < 8 && in.reachable && (!in.sources || in.sources->hire(def)))
						{
							m.staff = def.belongCapacity;
							m.status = now < open - 60 ? kStatusClosed : kStatusNight;
						}
					}
					else if (s == kStatusBuilt)
					{
						setPattern(m, 0, ev);
						if (def.belongCapacity > 0) m.status = kStatusHiring;
						else m.status = now < open ? kStatusClosed : kStatusNight;
					}
					return;
				}

				if (withinMinutes(now, close, close + 8))
				{
					pay(m, def, ev);
					if (open == close)
					{
						// Open around the clock: only the books are closed.
						m.customers = 0;
						if (!restaurant) m.walking = 0;
					}
					else if (restaurant)
					{
						pushOutCustomers(m);
						m.status = kStatusAfterClose;
						setPattern(m, 0, ev);
					}
					else
					{
						pushOutAll(m);
						m.status = kStatusClosed2;
						setPattern(m, 0, ev);
					}
					updateEstimate(m, def, ev);
					return;
				}
				collectCustomers(m, def, in, rng);
				if (now % 60 < 8) updateEstimate(m, def, ev);
			}

			/// Vending machine ChangeStatusOnIdle (Drink.t2p FUN_1000b930).
			void vendingIdle(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, const RngFn & rng, TenantEvents & ev)
			{
				const int open = def.openMinute, close = def.closeMinute;
				const int now = in.minute;
				const int s = m.status;
				if (s == kStatusBuilt)
				{
					if (open == close) { m.status = kStatusOpen; setPattern(m, 0, ev); return; }
					if (withinMinutes(now, open, close)) { m.status = kStatusOpen; setPattern(m, 1, ev); return; }
				}
				else
				{
					if (s == kStatusClosed2)
					{
						if (!withinMinutes(now, open, open + 8)) return;
						m.status = kStatusOpen;
						setPattern(m, 1, ev);
						updateEstimate(m, def, ev);
						m.customers = 0;
						m.walking = 0;
						return;
					}
					if (!isOpen(s)) return;
					if (!withinMinutes(now, close, close + 8))
					{
						collectCustomers(m, def, in, rng);
						if (now % 60 < 8) updateEstimate(m, def, ev);
						return;
					}
					pushOutCustomers(m);
					pay(m, def, ev);
					if (open == close)
					{
						m.customers = 0;
						m.walking = 0;
						return;
					}
				}
				m.status = kStatusClosed2;
				setPattern(m, 0, ev);
			}
		}

		void tenantIdleImpl(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, const RngFn & rng, TenantEvents & ev)
		{
			switch (def.family)
			{
			case YootTenantFamily::Shop:
			case YootTenantFamily::Restaurant: commercialIdle(m, def, in, rng, ev); break;
			case YootTenantFamily::Vending: vendingIdle(m, def, in, rng, ev); break;
			default: break;
			}
		}

		void tenantMinute(TenantModel & m, const YootTenantDef & def, long long absMinute, TenantEvents & ev)
		{
			const bool patterned = def.family != YootTenantFamily::Vending;
			// LeaveTenant: stays that ended.
			for (size_t i = 0; i < m.inside.size();)
			{
				if (m.inside[i] <= absMinute) m.inside.erase(m.inside.begin() + i);
				else ++i;
			}
			if (patterned && isOpen(m.status))
			{
				const int p = commercialPattern(m.status, m.insideCount(), def.capacity);
				m.status = p == 1 ? kStatusOpen : p == 2 ? kStatusOpen1 : kStatusOpen2;
				setPattern(m, p, ev);
			}
		}
	}

	namespace Yoot
	{
		bool customerArrives(TenantModel & m, const YootTenantDef & def, long long absMinute, TenantEvents & ev)
		{
			if (m.walking > 0) --m.walking;
			if (!isOpen(m.status)) return false;
			m.inside.push_back(absMinute + std::max(1, def.bindMinutes));
			++m.customers;
			if (def.family != YootTenantFamily::Vending)
			{
				const int p = commercialPattern(m.status, m.insideCount(), def.capacity);
				m.status = p == 1 ? kStatusOpen : p == 2 ? kStatusOpen1 : kStatusOpen2;
				setPattern(m, p, ev);
			}
			return true;
		}

		void customerCancelled(TenantModel & m)
		{
			if (m.walking > 0) --m.walking;
		}

		int customerEconoType(const YootTenantDef & def)
		{
			const uint32_t e = def.customers.econoType;
			return e == 0xFFFFFFFFu ? -1 : int(e & ~0x10u);
		}

		int customerScore(const YootTenantDef & def, int place)
		{
			if (def.hasPlaceParam) return def.placeParam.places[place].score();
			return place == 0 ? 1000 : 0;
		}

		int collectFromPool(const YootTenantDef & def, Pool & pool, unsigned routeMask, int free, int nowMinute,
			const PoolRng & rng, const std::function<void(const PeopleType &, int)> & onCalled)
		{
			if (!def.hasCustomers || routeMask == 0) return 0;
			const int n = (int)def.customers.members.size();
			if (n == 0) return 0;
			const int econo = customerEconoType(def);
			const float units = customerUnits(def.capacity, def.bindMinutes);
			int called = 0;
			// T2CustomerTableIterator: from a random member, once round.
			const int first = rng() % n;
			for (int k = 0; k < n && free > 0; ++k)
			{
				const MemberDef & member = def.customers.members[(first + k) % n];
				int count = float2IntRng(memberRate(member.percent, units), rng);
				if (count == 0) continue;
				count = std::min(count, free);
				int start = nowMinute;
				for (int c = 0; c < count; ++c, start += 2)
				{
					PeopleType t;
					if (!pool.call(member, econo, routeMask, t, rng)) break;
					onCalled(t, start);
					++called;
					--free;
				}
			}
			return called;
		}
	}

	std::vector<YootToolboxGroup> yootToolboxGroups(std::vector<YootToolboxEntry> entries)
	{
		// T2ToolDef::GetSortKey 0x10065a50: category << 16 | toolNo.
		std::stable_sort(entries.begin(), entries.end(), [](const YootToolboxEntry & a, const YootToolboxEntry & b) {
			if (a.category != b.category) return a.category < b.category;
			return a.toolNo < b.toolNo;
		});
		std::vector<YootToolboxGroup> groups;
		for (const YootToolboxEntry & e : entries)
		{
			if (groups.empty() || groups.back().category != e.category)
			{
				YootToolboxGroup g;
				g.category = e.category;
				g.name = e.categoryName;
				groups.push_back(g);
			}
			if (groups.back().name.empty()) groups.back().name = e.categoryName;
			groups.back().prototypeIds.push_back(e.prototypeId);
		}
		return groups;
	}
}
