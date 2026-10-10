/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower visitor pool (game-parity.md P4): the world's population
/// demand, from which every visitor, customer and staff member is called.
///
/// Verbatim port of the T2DLL.dll classes (addresses are T2DLL):
///   T2Pool               0x100501e0 ctor, GameLevelChanged 0x10050390,
///                        IncHour 0x10050470, DayChanged 0x100504a0,
///                        IsChangePopulation 0x100504c0, Find 0x10050500,
///                        Call 0x10050580, Enter 0x10050610,
///                        RecoverPeople 0x10050790, ShrinkPeople 0x10050870,
///                        DuplicatePeople 0x100508b0, RemovePeople 0x10050920,
///                        BornVIP 0x100509a0, DeadVIP 0x10050a30
///   T2PeopleDemandList   0x1004bd20 (one T2PeopleTimeZoneList per PODF demand)
///   T2PeopleTimeZoneList 0x1004dd60 (one T2PeopleTypeArray per time zone)
///   T2PeopleTypeArray    Add 0x1004e8b0, InitSearchLimit 0x1004e9d0,
///                        Find 0x1004ea10, Call 0x1004eaf0, Restore 0x1004ebc0,
///                        AdjustLife 0x1004e930
///   T2PeopleType         Check 0x1004e4c0 (+ CheckSilhouetteType 0x1004e590,
///                        CheckSilhouetteOptionType 0x1004e630, CheckWithDemand
///                        0x1004e520), Duplicate 0x1004e460
/// Resources: PODF id 1 (FUN_10050e00), PGDF (FUN_10050c70 / FUN_10050be0),
/// PZDF (FUN_10050ad0). The season table (SPDF id 1) is absent from all three
/// shipped worlds, so T2PeopleDemandList::Init uses 0.0 (an empty window
/// until the zone first changes) and IncHour uses 1.0.
///
/// The LArray semantics are kept: type arrays are 1-based with a search
/// window [start, limit]; called people are removed from non-fixed arrays,
/// returning people are inserted in front of the window (callable from the
/// next zone change) and duplicates are appended behind it.
///
/// Pure module: no Game dependency, linked into OpenSkyTests.
#pragma once

#include "PluginDefs.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace OT
{
	class AbstractPlugin;

	namespace Yoot
	{
		typedef std::function<int()> PoolRng;

		// ---- resources ----
		struct PoolZoneElem { int silhouette = 0, econo = 0, transport = 0, raw = 0, count = 0; };
		/// PZDF: people types of one time zone; count = raw x 20 / 100.
		struct PoolZoneDef { std::vector<PoolZoneElem> elems; };
		struct PoolGradeZone { int zoneDefId = 0; int rate = 0; };
		/// PGDF: per demand, per time zone {PZDF id, times to add}.
		struct PoolGradeDef { std::vector<std::vector<PoolGradeZone> > demands; };
		struct PoolDemandDef { int fixed = 0; int numZones = 0; };
		/// PODF + every PGDF/PZDF it names.
		struct PoolDef
		{
			bool valid = false;
			int unk08 = 0;
			int gradePoolIds[5] = {0, 0, 0, 0, 0};
			int specialFlag = 0;          ///< +0x20 (cleared on non-general people entering)
			std::vector<PoolDemandDef> demands;
			std::map<int, PoolGradeDef> grades;
			std::map<int, PoolZoneDef> zones;
			/// FUN_10050f50: PGDF id for grade g (1..5), 0 = none.
			int gradePoolId(int grade) const { return grade >= 1 && grade <= 5 ? gradePoolIds[grade - 1] : 0; }
		};
		bool parsePoolDef(const char * data, size_t length, PoolDef & out);
		bool parsePoolGradeDef(const char * data, size_t length, PoolGradeDef & out);
		bool parsePoolZoneDef(const char * data, size_t length, PoolZoneDef & out);
		/// Reads PODF id 1 and every PGDF/PZDF it uses from a world plugin.
		bool loadPoolDef(const AbstractPlugin & world, PoolDef & out);

		// ---- people types ----
		const int kImmortalLife = 0x7f;
		/// Transport types (T2PeopleType +0x10) and CheckRouteFromPool bits.
		enum Transport { kTransportLobbyLeft = 0, kTransportLobbyRight = 1, kTransportCar = 2, kTransportSubway = 3, kTransportShip = 4 };
		const unsigned kRouteLobby = 0x3, kRouteParking = 0x4, kRouteSubway = 0x8, kRouteShip = 0x10;

		struct PeopleType
		{
			int demand = 0;      ///< +0x00 bits 0-2
			int timeZone = 0;    ///< +0x00 bits 3-7
			int silhouette = 0;  ///< +0x04 (PPDF people type)
			int econo = 0;       ///< +0x08
			int life = 0;        ///< +0x0c (0x7f immortal)
			int transport = 0;   ///< +0x10
			bool immortal() const { return life == kImmortalLife; }
			int age() const { return silhouette >> 3; }
			bool male() const { return (silhouette & 1) == 0; }
			bool operator==(const PeopleType & o) const
			{
				return demand == o.demand && timeZone == o.timeZone && silhouette == o.silhouette &&
					econo == o.econo && life == o.life && transport == o.transport;
			}
		};

		/// T2PeopleType::CheckSilhouetteType: start < -1 selects an age band
		/// (-3 8..15, -4 16..23, -5 24..55, -6 30..50), -1 any, else start..end.
		bool checkSilhouetteType(const PeopleType & t, const MemberDef & m);
		/// T2PeopleType::CheckSilhouetteOptionType: bit 1 male, 2 female,
		/// 4 / 8 on bit 2 of the silhouette.
		bool checkSilhouetteOption(const PeopleType & t, int option);
		/// T2PeopleType::Check: the transport bit in `routeMask`, the
		/// silhouette range and option, and econo in [econo, econo + 2]
		/// unless econo < 0.
		bool checkPeopleType(const PeopleType & t, const MemberDef & m, int econo, unsigned routeMask);
		/// CheckWithDemand: also the member's demand type (-1 any), any route.
		bool checkWithDemand(const PeopleType & t, const MemberDef & m, int econo);
		/// T2PeopleType::Duplicate: life 1, econo + (rand % 3 - 1) in 1..6.
		PeopleType duplicateType(const PeopleType & t, int roll);
		/// UT2Utils::Float2Int: the integer part, +1 with probability of the
		/// first decimal (rand drawn only when it is non-zero).
		int float2IntRng(float value, const PoolRng & rng);

		/// T2Tenant::CheckRouteFromPool 0x10060bf0 inputs: routes within the
		/// PLDF score from each tower entrance to the tenant.
		struct RouteFromPoolInputs
		{
			bool lobby = false;           ///< lobby entrance -> tenant
			bool parking = false;         ///< FindEmptyParking found a lot and its exit reaches the tenant
			bool subway = false;          ///< registry 8 (Subway) open and its exit reaches the tenant
			bool trainAvailable = false;  ///< T2Transport::GetAvailable (a train in the station)
			bool ship = false;            ///< registry 9 (Boat Deck) open and its exit reaches the tenant
			bool cruiseAvailable = false; ///< the cruise is docked
		};
		/// The transport mask: 3 lobby, 4 parking, 8 subway (only while a train
		/// is in), 0x10 ship (only while the cruise is docked).
		unsigned checkRouteFromPool(const RouteFromPoolInputs & in);

		/// T2PeopleTypeArray.
		struct PeopleTypeArray
		{
			std::vector<PeopleType> types;  ///< LArray, index 1..n
			bool fixed = false;             ///< +0x24
			int start = 1;                  ///< +0x28
			int limit = 0;                  ///< +0x2c
			int count() const { return (int)types.size(); }
			void insertFront(const PeopleType & t, int n);   ///< InsertItemsAt(n, 1)
			void append(const PeopleType & t) { types.push_back(t); }
			void initSearchLimit(float factor);
			bool find(const MemberDef & m, int econo, unsigned routeMask, int numNeeded, bool onlyFirstEcono) const;
			bool call(const MemberDef & m, int econo, unsigned routeMask, PeopleType & out);
			void restore(PeopleType t);
			void adjustLife();
		};

		/// T2PeopleTimeZoneList: one array per time zone of a demand.
		struct PeopleTimeZoneList
		{
			std::vector<PeopleTypeArray> zones;
			bool fixed = false;       ///< +0x24
			int hoursPerZone = 0;     ///< +0x28 = 24 / zones
			int current = 0;          ///< +0x2c
			int hourInZone = 0;       ///< +0x30
			PeopleTypeArray * currentZone() { return current >= 0 && current < (int)zones.size() ? &zones[current] : nullptr; }
			const PeopleTypeArray * currentZone() const { return current >= 0 && current < (int)zones.size() ? &zones[current] : nullptr; }
			void init(int hour, float factor);
			void incHour(float factor);
		};

		/// T2Pool with its T2PeopleDemandList.
		class Pool
		{
		public:
			int gameLevel = 0;        ///< +0x08
			int specialFlag = 0;      ///< +0x0c (PODF +0x20)
			int population = 0;       ///< +0x14 people called today and present
			int shrinkCount = 0;      ///< +0x18 people back in the pool since midnight
			int shownPopulation = 0;  ///< +0x1c
			int duplicateCounter = 0; ///< DAT_100cbde4 (every 5th 71..200 return duplicates)
			std::vector<PeopleTimeZoneList> demands;

			bool empty() const { return demands.empty(); }
			/// T2Pool ctor (new tower): grades 1..grade, then Init(month, hour).
			void create(const PoolDef & def, int grade, int hour);
			/// Adds the PGDF of a newly reached grade (only going up).
			void gameLevelChanged(const PoolDef & def, int grade);
			/// T2Pool::IncHour at the new hour (0..23): zones roll, midnight
			/// subtracts the people who went home.
			void incHour(int hour);
			bool isChangePopulation();

			/// T2Pool::Find (a whole member table: every member must be found;
			/// with onlyFirst the econo filter applies to the first only).
			bool find(const MemberTableDef & table, int econo, unsigned routeMask, bool onlyFirst) const;
			/// T2PeopleDemandList::Call: a type for the member (demand -1: a
			/// random non-fixed demand list, list 0 excluded as in the engine).
			bool callType(const MemberDef & m, int econo, unsigned routeMask, PeopleType & out, const PoolRng & rng);
			/// T2Pool::Call = callType + RecoverPeople (population + 1).
			bool call(const MemberDef & m, int econo, unsigned routeMask, PeopleType & out, const PoolRng & rng);
			/// T2Tenant::CollectEmployeeFromPool 0x10060a50: the whole table
			/// must be found (Find), then each member is called numOfPeople
			/// times (`onlyFirst`: the econo applies to the first member).
			/// OpenSky counts belong people in their tenant's population, so
			/// RecoverPeople is not applied. False (nothing taken) when the
			/// table is not in the pool.
			bool callBelong(const MemberTableDef & table, int econo, unsigned routeMask, bool onlyFirst,
				std::vector<PeopleType> & out, const PoolRng & rng);
			/// T2Tenant::ClearBelongPeople -> RemovePeople for belong people:
			/// immortal types go back (not counted in the population here).
			void releaseBelong(const std::vector<PeopleType> & types);
			/// T2Pool::BornVIP: a VIP (life 0x7f) is recovered (population + 1).
			PeopleType bornVIP(int silhouette, int transport = -1);
			/// T2Pool::DeadVIP / RemovePeople (shrink; immortals go back).
			void deadVIP() { ++shrinkCount; }
			void removePeople(const PeopleType & t);
			/// T2Pool::Enter for a general person whose estimate (after
			/// UpdateEstimation) is `estimate`: back in the pool, restored
			/// and/or duplicated by estimate (> 200 dup; 71..200 every 5th;
			/// <= 70 only immortals return).
			void enter(const PeopleType & t, int estimate, const PoolRng & rng);

			/// Types in the current search window of a demand (diagnostics).
			int available(int demand) const;
			int totalTypes() const;

			void encode(std::string & out) const;
			bool decode(const std::string & in);

		private:
			void dispatchRestore(const PeopleType & t);
			void dispatchAdd(const PeopleType & t);
			void duplicate(const PeopleType & t, const PoolRng & rng);
		};

		/// The YootTower population (Game.cpp:1178-1187): the tenants' own
		/// populations plus the pool's visitors (T2Pool +0x14, called today and
		/// not yet subtracted at midnight). It is a full recount, not the
		/// SimTower incremental CountT global (countT.population).
		inline int towerPopulation(int tenantPopulation, int poolVisitors) { return tenantPopulation + poolVisitors; }

		/// Hotel stay party saved on a room (yootStayParty): one "demand
		/// timeZone silhouette econo life transport" group per member, members
		/// separated by ';'. The party is the room's pool people (they are
		/// not in the saved pool), so a reload spawns them again.
		std::string encodeStayParty(const std::vector<PeopleType> & party);
		bool decodeStayParty(const std::string & in, std::vector<PeopleType> & out);
		/// A saved stay restores only when it was in the room (status 32..34)
		/// with a party; old saves (no attributes) and other states are checked out.
		bool decodeStay(int status, const std::string & party, std::vector<PeopleType> & out);
	}
}
