/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "Person.h"
#include "YootPool.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	struct YootTenantDef;
	namespace Item { class Item; class YootTenant; namespace Elevator { class Elevator; } }

	/** A YootTower visitor called from the pool (T2People with a
	 * T2PeopleType): it waits in the pool until its start minute, enters
	 * the tower at the entrance of its transport type, walks (rides the
	 * movers, queues and gets stressed) to its destination tenant, stays
	 * the tenant's bind time and walks out again, back into the pool. */
	class YootVisitor : public Person
	{
	public:
		YootVisitor(Game * game, const Yoot::PeopleType & type);

		enum Phase
		{
			kInPool,      ///< status 1: called, waiting for its start minute
			kWalking,     ///< status 2..10: on its way to `dest`
			kStopping,    ///< walking over on the same floor (StopOnHisWay)
			kInTenant,    ///< status 11: inside `inside`
			kLeaving,     ///< status 12: on its way out of the tower
		};

		Yoot::PeopleType peopleType;  ///< T2People +0x88
		Phase phase = kInPool;
		Item::Item * dest = nullptr;  ///< T2Matter +0x28 (null = none)
		long long startMinute = -1;   ///< T2Matter +0x3c: start / leave minute
		Item::Item * inside = nullptr;
		Item::Item * prevTenant = nullptr;  ///< +0x6c
		/// T2Matter return stack (FUN_100549a0, 3 deep): destination, time.
		std::vector<std::pair<Item::Item *, long long> > returns;
		int noRouteTries = 0;
		long long arriveMinute = -1;  ///< kStopping: arrival minute
		long long stayEnd = -1;       ///< the tenant's TenantModel::inside entry

		/// T2People::CalcFreeTime 0x100495b0 at `now`.
		long long freeTime(long long now) const;
	};

	/** The YootTower visitor pool and its walking visitors (game-parity.md
	 * P4; plan step 15). Owned by EventSystem next to the other YootTower
	 * systems; every entry point is a no-op outside YootTower.
	 *
	 * The pool itself (Yoot::Pool) is a verbatim port. Simplifications
	 * (documented in game-parity.md):
	 * - routes come from OpenSky's PathFinder; the PLDF route score caps
	 *   the walking distance on the floor and the mover score of the route
	 *   (CheckRoute), not T2RoutingTable's per-floor tables;
	 * - SelectNearTenant / StopOnHisWay pick people inside tenants / waiting
	 *   at the movers of a floor (OpenSky has no walking positions);
	 * - car visitors enter at a Parking Lot item, ship visitors at the Boat
	 *   Deck, subway visitors at the Subway station; all leave by the lobby. */
	class PeopleSystem : public GameObject
	{
	public:
		PeopleSystem(Game * game);
		~PeopleSystem();

		void reset();
		bool enabled() const;

		/// Per-frame: catches up minute by minute (pool hours, visitors).
		void advance();

		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		/// People called today and still counted (T2Pool::GetPopulation).
		int visitorPopulation() const { return pool.population; }
		Yoot::Pool pool;
		Yoot::PoolDef def;
		const std::vector<YootVisitor *> & visitors() const { return visitorList; }

		/// T2Tenant::CheckRouteFromPool 0x10060bf0 for a tenant.
		unsigned routeMask(Item::Item * tenant, int score);

		// --- the customer sources of a plugin tenant (CollectCustomer) ---
		int callFromPool(Item::YootTenant * tenant, const YootTenantDef & def, int free, long long now);
		int callFromTenant(Item::YootTenant * tenant, const YootTenantDef & def, int free, long long now);
		int callFromFloor(Item::YootTenant * tenant, const YootTenantDef & def, int free, long long now);

		/// The tenant took the visitor in (status 11) or turned it away.
		void arrived(YootVisitor * v, Item::Item * tenant, bool admitted, long long leaveMinute);
		/// T2Tenant::PushOutPeople 0x1005f720: the visitor leaves `tenant`.
		void pushOut(YootVisitor * v);
		/// Counted visitors of the items that do not walk them (venues, the
		/// cruise, hotel guests): up to `n` people are called out of the pool
		/// with the item plugin's customer table (TMDF 2, PLDF pool score,
		/// CheckRouteFromPool mask) and return at once. Returns how many came
		/// (`n` when the pool or the table is missing).
		int drawVisitors(Item::Item * item, int n);
		/// Visitors bound for / inside a removed item.
		void itemRemoved(Item::Item * item);

		/// Diagnostics for the sim summary.
		int calledToday = 0;
		int enteredToday = 0;

	private:
		std::string worldId;
		bool defLoaded = false;
		long long lastMinute = -1;
		std::vector<YootVisitor *> visitorList;
		std::string pendingPool;  ///< decoded before the pool def was loaded
		std::map<std::pair<unsigned long long, unsigned long long>, int> routeCache;
		size_t routeCacheItems = 0;
		struct Table { bool valid = false; MemberTableDef table; bool hasPlace = false; PlaceParamDef place; };
		std::map<std::string, Table> tables;

		bool ensurePool();
		void minuteTick(long long absMinute);
		void visitorTick(YootVisitor * v, long long now);
		YootVisitor * spawn(const Yoot::PeopleType & type, Item::Item * dest, long long start, unsigned specialFlag);
		Item::Item * entranceFor(YootVisitor * v);
		void enterTower(YootVisitor * v, long long now);
		void walkTo(YootVisitor * v, Item::Item * from, Item::Item * to, long long now);
		void goOutTower(YootVisitor * v);
		bool reachableWithin(Item::Item * from, Item::Item * to, int score);
		void destroy(YootVisitor * v);
		bool waitingAt(Item::Elevator::Elevator * e, Person * p) const;
	};
}
