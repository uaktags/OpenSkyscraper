/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootPool.h"

#include "AbstractPlugin.h"
#include "PluginResources.h"

#include <algorithm>
#include <sstream>

namespace OT
{
	namespace Yoot
	{
		// ---- resources ----

		bool parsePoolDef(const char * data, size_t length, PoolDef & out)
		{
			// FUN_10050e00: int; int[5] PGDF ids; uchar; uchar n; n x {int, uchar}.
			CResReader r(data, length);
			PoolDef d;
			d.unk08 = r.i32();
			for (int k = 0; k < 5; ++k) d.gradePoolIds[k] = r.i32();
			d.specialFlag = int(r.u32() & 0xff);
			const int n = int(r.u32() & 0xff);
			for (int k = 0; k < n && r.ok(); ++k)
			{
				PoolDemandDef dem;
				dem.fixed = r.i32();
				dem.numZones = int(r.u32() & 0xff);
				d.demands.push_back(dem);
			}
			if (!r.ok()) return false;
			d.valid = true;
			out = d;
			return true;
		}

		bool parsePoolGradeDef(const char * data, size_t length, PoolGradeDef & out)
		{
			// FUN_10050c70: count x FUN_10050be0 {count x {int PZDF id, uint rate}}.
			CResReader r(data, length);
			PoolGradeDef g;
			const int n = r.count(64);
			for (int k = 0; k < n && r.ok(); ++k)
			{
				std::vector<PoolGradeZone> zones;
				const int z = r.count(64);
				for (int j = 0; j < z && r.ok(); ++j)
				{
					PoolGradeZone e;
					e.zoneDefId = r.i32();
					e.rate = int(r.u32());
					zones.push_back(e);
				}
				g.demands.push_back(zones);
			}
			if (!r.ok()) return false;
			out = g;
			return true;
		}

		bool parsePoolZoneDef(const char * data, size_t length, PoolZoneDef & out)
		{
			// FUN_10050ad0: count x 4 uchar slots; +4 = slot3 x 20 / 100.
			CResReader r(data, length);
			PoolZoneDef z;
			const int n = r.count(4096);
			for (int k = 0; k < n && r.ok(); ++k)
			{
				PoolZoneElem e;
				e.silhouette = int(r.u32() & 0xff);
				e.econo = int(r.u32() & 0xff);
				e.transport = int(r.u32() & 0xff);
				e.raw = int(r.u32() & 0xff);
				e.count = e.raw * 20 / 100;
				z.elems.push_back(e);
			}
			if (!r.ok()) return false;
			out = z;
			return true;
		}

		bool loadPoolDef(const AbstractPlugin & world, PoolDef & out)
		{
			const WindowsPEExecutable::Resource * podf = world.getResource("PODF", 1);
			PoolDef d;
			if (!podf || !parsePoolDef(podf->data, podf->length, d)) return false;
			for (int g = 1; g <= 5; ++g)
			{
				const int id = d.gradePoolId(g);
				if (id <= 0 || d.grades.count(id)) continue;
				const WindowsPEExecutable::Resource * pgdf = world.getResource("PGDF", id);
				PoolGradeDef gd;
				if (!pgdf || !parsePoolGradeDef(pgdf->data, pgdf->length, gd)) continue;
				for (const std::vector<PoolGradeZone> & dem : gd.demands)
					for (const PoolGradeZone & z : dem)
					{
						if (d.zones.count(z.zoneDefId)) continue;
						const WindowsPEExecutable::Resource * pzdf = world.getResource("PZDF", z.zoneDefId);
						PoolZoneDef zd;
						if (pzdf && parsePoolZoneDef(pzdf->data, pzdf->length, zd)) d.zones[z.zoneDefId] = zd;
					}
				d.grades[id] = gd;
			}
			out = d;
			return true;
		}

		// ---- people types ----

		bool checkSilhouetteType(const PeopleType & t, const MemberDef & m)
		{
			const int s = m.startSilhouetteType;
			const int v = t.silhouette;
			if (s < -1)
			{
				switch (s)
				{
				case -6: return v > 29 && v < 51;
				case -5: return v > 23 && v < 56;
				case -4: return v > 15 && v < 24;
				case -3: return v > 7 && v < 16;
				default: return true;
				}
			}
			if (s < 0) return true;
			return s <= v && v <= m.endSilhouetteType;
		}

		bool checkSilhouetteOption(const PeopleType & t, int option)
		{
			if (option < 0) return true;
			const bool male = t.male();
			const bool bit2 = ((t.silhouette / 4) & 1) != 0;
			bool ok = true;
			if (option & 1) { if (!male) ok = false; }
			else if ((option & 2) && male) ok = false;
			if (option & 4) { if (bit2) return false; }
			else if ((option & 8) && !bit2) ok = false;
			return ok;
		}

		bool checkPeopleType(const PeopleType & t, const MemberDef & m, int econo, unsigned routeMask)
		{
			if (((1u << (unsigned(t.transport) & 0x1f)) & routeMask) == 0) return false;
			if (!checkSilhouetteType(t, m)) return false;
			if (!checkSilhouetteOption(t, m.optionType)) return false;
			if (econo > -1 && (t.econo < econo || econo + 3 <= t.econo)) return false;
			return true;
		}

		bool checkWithDemand(const PeopleType & t, const MemberDef & m, int econo)
		{
			if (m.demandType != -1 && t.demand != m.demandType) return false;
			return checkPeopleType(t, m, econo, 0xff);
		}

		PeopleType duplicateType(const PeopleType & t, int roll)
		{
			PeopleType d = t;
			d.life = 1;
			const int e = roll % 3 - 1 + t.econo;
			d.econo = e < 1 ? 1 : std::min(e, 6);
			return d;
		}

		int float2IntRng(float value, const PoolRng & rng)
		{
			if (!(value > 0.0f)) return 0;
			const int whole = (int)value;
			const int tenths = (int)((value - (float)whole) * 10.0f);
			if (tenths == 0) return whole;
			return whole + ((rng() % 10) < tenths ? 1 : 0);
		}

		unsigned checkRouteFromPool(const RouteFromPoolInputs & in)
		{
			unsigned mask = 0;
			if (in.lobby) mask = kRouteLobby;
			if (in.parking) mask |= kRouteParking;
			if (in.subway && in.trainAvailable) mask |= kRouteSubway;
			if (in.ship && in.cruiseAvailable) mask |= kRouteShip;
			return mask;
		}

		// ---- T2PeopleTypeArray ----

		void PeopleTypeArray::insertFront(const PeopleType & t, int n)
		{
			if (n <= 0) return;
			types.insert(types.begin(), size_t(n), t);
		}

		void PeopleTypeArray::initSearchLimit(float factor)
		{
			start = 1;
			limit = count();
			if (!fixed) limit = (int)((float)count() * factor);
		}

		bool PeopleTypeArray::find(const MemberDef & m, int econo, unsigned routeMask, int numNeeded, bool onlyFirstEcono) const
		{
			int need = numNeeded;
			if (need > 1 && fixed) need = 1;
			int found = 0;
			for (int idx = start; idx <= limit; ++idx)
			{
				if (idx < 1 || idx > count()) continue;
				if (!checkPeopleType(types[idx - 1], m, econo, routeMask)) continue;
				++found;
				if (found < need)
				{
					if (found == 1 && onlyFirstEcono) econo = -1;
				}
				else return true;
			}
			return false;
		}

		bool PeopleTypeArray::call(const MemberDef & m, int econo, unsigned routeMask, PeopleType & out)
		{
			for (int idx = start; idx <= limit; ++idx)
			{
				if (idx < 1 || idx > count()) continue;
				if (!checkPeopleType(types[idx - 1], m, econo, routeMask)) continue;
				out = types[idx - 1];
				if (!fixed)
				{
					types.erase(types.begin() + (idx - 1));
					--limit;
				}
				return true;
			}
			return false;
		}

		void PeopleTypeArray::restore(PeopleType t)
		{
			if (t.life != kImmortalLife) t.life = 12;  // T2PeopleType::RecoverLife
			insertFront(t, 1);
			++start;
			++limit;
		}

		void PeopleTypeArray::adjustLife()
		{
			for (int idx = limit; idx >= start; --idx)
			{
				if (idx < 1 || idx > count()) continue;
				PeopleType & t = types[idx - 1];
				if (t.life != kImmortalLife) --t.life;
				if (t.life < 1) types.erase(types.begin() + (idx - 1));
			}
		}

		// ---- T2PeopleTimeZoneList ----

		void PeopleTimeZoneList::init(int hour, float factor)
		{
			if (hoursPerZone <= 0) return;
			current = hour / hoursPerZone;
			hourInZone = hour % hoursPerZone;
			for (PeopleTypeArray & z : zones) z.initSearchLimit(factor);
		}

		void PeopleTimeZoneList::incHour(float factor)
		{
			++hourInZone;
			if (hourInZone < hoursPerZone) return;
			if (PeopleTypeArray * z = currentZone()) z->adjustLife();
			hourInZone = 0;
			++current;
			if (current >= (int)zones.size()) current = 0;
			if (PeopleTypeArray * z = currentZone()) z->initSearchLimit(factor);
		}

		// ---- T2Pool ----

		void Pool::create(const PoolDef & def, int grade, int hour)
		{
			*this = Pool();
			specialFlag = def.specialFlag;
			for (const PoolDemandDef & dd : def.demands)
			{
				PeopleTimeZoneList list;
				list.fixed = dd.fixed != 0;
				for (int z = 0; z < dd.numZones; ++z)
				{
					PeopleTypeArray a;
					a.fixed = list.fixed;
					a.initSearchLimit(1.0f);
					list.zones.push_back(a);
				}
				if (dd.numZones > 0) list.hoursPerZone = 24 / dd.numZones;
				demands.push_back(list);
			}
			for (int g = 1; g <= grade; ++g) gameLevelChanged(def, g);
			// T2PeopleDemandList::Init: no season table -> factor 0.0.
			for (PeopleTimeZoneList & l : demands) l.init(hour, 0.0f);
		}

		void Pool::gameLevelChanged(const PoolDef & def, int grade)
		{
			const int old = gameLevel;
			gameLevel = grade;
			if (old >= grade) return;
			auto g = def.grades.find(def.gradePoolId(grade));
			if (g == def.grades.end()) return;
			for (size_t d = 0; d < demands.size() && d < g->second.demands.size(); ++d)
			{
				const std::vector<PoolGradeZone> & elem = g->second.demands[d];
				PeopleTimeZoneList & list = demands[d];
				for (size_t z = 0; z < list.zones.size() && z < elem.size(); ++z)
				{
					auto zd = def.zones.find(elem[z].zoneDefId);
					if (zd == def.zones.end()) continue;
					PeopleType t;
					t.demand = int(d) & 7;
					t.timeZone = int(z) & 0x1f;
					for (int k = 0; k < elem[z].rate; ++k)
						for (const PoolZoneElem & e : zd->second.elems)
						{
							t.silhouette = e.silhouette;
							t.econo = e.econo;
							t.life = kImmortalLife;
							t.transport = e.transport;
							list.zones[z].insertFront(t, e.count);
						}
				}
			}
		}

		void Pool::incHour(int hour)
		{
			for (PeopleTimeZoneList & l : demands) l.incHour(1.0f);
			if (hour == 0)
			{
				// T2Pool::DayChanged
				population -= shrinkCount;
				shrinkCount = 0;
			}
		}

		bool Pool::isChangePopulation()
		{
			if (population == shownPopulation) return false;
			shownPopulation = population;
			return true;
		}

		bool Pool::find(const MemberTableDef & table, int econo, unsigned routeMask, bool onlyFirst) const
		{
			const int n = (int)table.members.size();
			if (n <= 0) return false;
			bool first = onlyFirst;
			for (int i = 0; i < n; ++i)
			{
				if (i == 1 && onlyFirst) { econo = -1; first = false; }
				const MemberDef & m = table.members[i];
				if (m.demandType < 0 || m.demandType >= (int)demands.size()) return false;
				const PeopleTypeArray * z = demands[m.demandType].currentZone();
				if (!z || !z->find(m, econo, routeMask, (int)m.numOfPeople, first)) return false;
			}
			return true;
		}

		bool Pool::callType(const MemberDef & m, int econo, unsigned routeMask, PeopleType & out, const PoolRng & rng)
		{
			const int n = (int)demands.size();
			if (m.demandType == -1)
			{
				int idx = n == 0 ? 0 : rng() % n;
				for (int k = 0; k < n; ++k)
				{
					if (++idx > n) idx = 1;
					// GetItem is 0-based but the loop runs 1..n: list 0 is never
					// tried and index n is past the end.
					if (idx >= n || demands[idx].fixed) continue;
					PeopleTypeArray * z = demands[idx].currentZone();
					if (z && z->call(m, econo, routeMask, out)) return true;
				}
				return false;
			}
			if (m.demandType < 0 || m.demandType >= n) return false;
			PeopleTypeArray * z = demands[m.demandType].currentZone();
			return z && z->call(m, econo, routeMask, out);
		}

		bool Pool::call(const MemberDef & m, int econo, unsigned routeMask, PeopleType & out, const PoolRng & rng)
		{
			if (!callType(m, econo, routeMask, out, rng)) return false;
			++population;  // RecoverPeople
			return true;
		}

		bool Pool::callBelong(const MemberTableDef & table, int econo, unsigned routeMask, bool onlyFirst,
			std::vector<PeopleType> & out, const PoolRng & rng)
		{
			out.clear();
			if (!find(table, econo, routeMask, onlyFirst)) return false;
			for (size_t i = 0; i < table.members.size(); ++i)
				for (int n = 0; n < std::max(1, (int)table.members[i].numOfPeople); ++n)
				{
					PeopleType t;
					if (callType(table.members[i], onlyFirst && i > 0 ? -1 : econo, routeMask, t, rng)) out.push_back(t);
				}
			return !out.empty();
		}

		void Pool::releaseBelong(const std::vector<PeopleType> & types)
		{
			for (const PeopleType & t : types)
				if (t.immortal()) dispatchRestore(t);
		}

		PeopleType Pool::bornVIP(int silhouette, int transport)
		{
			PeopleType t;
			t.silhouette = silhouette;
			t.transport = transport >= 0 ? transport : 7;
			t.life = kImmortalLife;
			++population;
			return t;
		}

		void Pool::dispatchRestore(const PeopleType & t)
		{
			if (t.demand < 0 || t.demand >= (int)demands.size()) return;
			PeopleTimeZoneList & l = demands[t.demand];
			if (t.timeZone < 0 || t.timeZone >= (int)l.zones.size()) return;
			l.zones[t.timeZone].restore(t);
		}

		void Pool::dispatchAdd(const PeopleType & t)
		{
			if (t.demand < 0 || t.demand >= (int)demands.size()) return;
			PeopleTimeZoneList & l = demands[t.demand];
			if (t.timeZone < 0 || t.timeZone >= (int)l.zones.size()) return;
			l.zones[t.timeZone].append(t);
		}

		void Pool::duplicate(const PeopleType & t, const PoolRng & rng)
		{
			dispatchAdd(duplicateType(t, rng()));
		}

		void Pool::removePeople(const PeopleType & t)
		{
			++shrinkCount;
			if (t.immortal()) dispatchRestore(t);
		}

		void Pool::enter(const PeopleType & t, int estimate, const PoolRng & rng)
		{
			++shrinkCount;  // ShrinkPeople
			if (estimate > 200)
			{
				dispatchRestore(t);
				duplicate(t, rng);
			}
			else if (estimate > 70)
			{
				dispatchRestore(t);
				if (++duplicateCounter > 4)
				{
					duplicate(t, rng);
					duplicateCounter = 0;
				}
			}
			else if (t.immortal())
				dispatchRestore(t);
		}

		int Pool::available(int demand) const
		{
			if (demand < 0 || demand >= (int)demands.size()) return 0;
			const PeopleTypeArray * z = demands[demand].currentZone();
			if (!z) return 0;
			return std::max(0, std::min(z->limit, z->count()) - z->start + 1);
		}

		int Pool::totalTypes() const
		{
			int n = 0;
			for (const PeopleTimeZoneList & l : demands)
				for (const PeopleTypeArray & z : l.zones) n += z.count();
			return n;
		}

		void Pool::encode(std::string & out) const
		{
			std::ostringstream o;
			o << 1 << ' ' << gameLevel << ' ' << specialFlag << ' ' << population << ' ' << shrinkCount << ' '
			  << shownPopulation << ' ' << duplicateCounter << ' ' << demands.size();
			for (const PeopleTimeZoneList & l : demands)
			{
				o << ' ' << (l.fixed ? 1 : 0) << ' ' << l.hoursPerZone << ' ' << l.current << ' ' << l.hourInZone << ' ' << l.zones.size();
				for (const PeopleTypeArray & z : l.zones)
				{
					o << ' ' << (z.fixed ? 1 : 0) << ' ' << z.start << ' ' << z.limit << ' ' << z.types.size();
					// Run-length encoded: runs of identical types.
					for (size_t i = 0; i < z.types.size();)
					{
						size_t j = i + 1;
						while (j < z.types.size() && z.types[j] == z.types[i]) ++j;
						const PeopleType & t = z.types[i];
						o << ' ' << (j - i) << ' ' << t.demand << ' ' << t.timeZone << ' ' << t.silhouette << ' '
						  << t.econo << ' ' << t.life << ' ' << t.transport;
						i = j;
					}
				}
			}
			out = o.str();
		}

		bool Pool::decode(const std::string & in)
		{
			std::istringstream s(in);
			Pool p;
			int version = 0;
			size_t nd = 0;
			if (!(s >> version) || version != 1) return false;
			s >> p.gameLevel >> p.specialFlag >> p.population >> p.shrinkCount >> p.shownPopulation >> p.duplicateCounter >> nd;
			if (!s || nd > 64) return false;
			for (size_t d = 0; d < nd; ++d)
			{
				PeopleTimeZoneList l;
				int fixed = 0;
				size_t nz = 0;
				s >> fixed >> l.hoursPerZone >> l.current >> l.hourInZone >> nz;
				if (!s || nz > 64) return false;
				l.fixed = fixed != 0;
				for (size_t z = 0; z < nz; ++z)
				{
					PeopleTypeArray a;
					size_t nt = 0;
					s >> fixed >> a.start >> a.limit >> nt;
					if (!s || nt > 1000000) return false;
					a.fixed = fixed != 0;
					while (a.types.size() < nt)
					{
						size_t run = 0;
						PeopleType t;
						s >> run >> t.demand >> t.timeZone >> t.silhouette >> t.econo >> t.life >> t.transport;
						if (!s || run == 0 || a.types.size() + run > nt) return false;
						a.types.insert(a.types.end(), run, t);
					}
					l.zones.push_back(a);
				}
				p.demands.push_back(l);
			}
			*this = p;
			return true;
		}

		std::string encodeStayParty(const std::vector<PeopleType> & party)
		{
			std::ostringstream o;
			for (size_t i = 0; i < party.size(); ++i)
			{
				const PeopleType & t = party[i];
				if (i) o << ';';
				o << t.demand << ' ' << t.timeZone << ' ' << t.silhouette << ' ' << t.econo << ' ' << t.life << ' ' << t.transport;
			}
			return o.str();
		}

		bool decodeStayParty(const std::string & in, std::vector<PeopleType> & out)
		{
			out.clear();
			std::istringstream all(in);
			std::string group;
			while (std::getline(all, group, ';'))
			{
				std::istringstream s(group);
				PeopleType t;
				if (!(s >> t.demand >> t.timeZone >> t.silhouette >> t.econo >> t.life >> t.transport)) return false;
				out.push_back(t);
			}
			return !out.empty();
		}

		bool decodeStay(int status, const std::string & party, std::vector<PeopleType> & out)
		{
			return status >= 32 && status <= 34 && decodeStayParty(party, out);
		}
	}
}
