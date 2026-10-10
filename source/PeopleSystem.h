/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "Person.h"
#include "YootPool.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	struct YootTenantDef;
	namespace Item { class Item; class YootTenant; class YootFacility; class Hotel; class FrontDesk; namespace Elevator { class Elevator; } }

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

		/// Floor walk (T2People +0x38 curH, +0x3c curV, +0x40 dstH, +0x58
		/// direction): `walking` people are on the floor `curV`, heading
		/// for `walkTarget`, which they enter on arrival.
		enum WalkKind { kWalkNode, kWalkDest, kWalkExit };
		bool walking = false;
		int curH = 0, curV = 0, dstH = 0;
		bool facingLeft = false;
		Item::Item * walkTarget = nullptr;
		WalkKind walkKind = kWalkNode;

		/// T2Transport rider: aboard the train / ship of this station
		/// (status 0x10, not in the tower).
		Item::Item * aboard = nullptr;
		/// A belong person's tenant (T2People +0x28 work / +0x2c home; null =
		/// a general visitor) and its index in the tenant's belong list.
		Item::Item * home = nullptr;
		int belongIndex = -1;

		/// The T2Maru_Reggae VIPs (1 Mr. Maru, 2 Uncle Reggae): their own
		/// silhouette (VIPL 223 -> SLDF 353, 224 -> 355; `style` 1 is the
		/// next sheet, 354), the walk style frame (0..2) and `posing` while
		/// they stand at a tenant instead of entering it.
		int vip = 0;
		int silhouette = -1;
		int style = 0;
		int walkStyle = 0;
		bool posing = false;

		/// T2People::CalcFreeTime 0x100495b0 at `now`.
		long long freeTime(long long now) const;
		bool yootWalkTo(Item::Item * next, int floor) override;
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
		int callFromPool(Item::Item * tenant, const YootTenantDef & def, int free, long long now);
		int callFromTenant(Item::Item * tenant, const YootTenantDef & def, int free, long long now);
		int callFromFloor(Item::Item * tenant, const YootTenantDef & def, int free, long long now);

		// --- venues (theaters, Event Center, Stadium, Cathedral, Attraction,
		// Observatory, Gathering Area): T2Tenant::CollectCustomer from the
		// venue plugin's idle ---
		/// The venue plugin's TenantDef with its TMDF 2 customer table, or
		/// with the table `tableId` of `subPlugin` (MovieDef / HallEventDef
		/// customerTableID; SetCustomerTable 0x10062c00 forces bind 1, so
		/// the units are the capacity). Null when there is no table.
		const YootTenantDef * venueDef(Item::Item * venue, const std::string & subPlugin = std::string(), int tableId = 0);
		/// CollectCustomer 0x1005f440's three sources for a venue whose own
		/// idle has decided to collect: the pool (PLDF and DPDF holiday
		/// rates), the tenant and the floor sources. The people walk to
		/// `dest` (the venue itself, or the Observatory's ticket booth).
		/// Returns how many were called (the venue's walking count).
		int collectVenue(Item::Item * venue, const YootTenantDef & def, int free, long long now, Item::Item * dest = nullptr);
		/// The visitors inside `f` whose stay ended (or beyond its leaveAt
		/// count: PushOutAll / PushOutCustomer) leave (PushOutPeople).
		void releaseVisitors(Item::YootFacility * f, long long now);
		/// Visitors inside `item` (status 11), in the item's stable order.
		std::vector<YootVisitor *> visitorsIn(Item::Item * item) const;

		// --- hotel guests (YootHotelGuests.cpp; game-parity.md W4, E7) ---
		/// The rooms' guests are pool people (true when the pool runs).
		bool hotelGuests() const;
		/// YootTower stay save/reload (Hotel encodeXML/decodeXML): the live
		/// stay of a room in states 32..34 (guests inside), if any.
		bool liveStay(const Item::Hotel * room, int & status, std::vector<Yoot::PeopleType> & party) const;
		/// Puts a saved stay back: the party is spawned inside the occupied room.
		void restoreStay(Item::Hotel * room, int status, const std::vector<Yoot::PeopleType> & party, long long now);
		/// A guest reaches its room / the Front Desk queue.
		void roomEnter(Item::Hotel * room, YootVisitor * v);
		void deskEnter(Item::FrontDesk * desk, YootVisitor * v);
		/// Room art: the guests in the room, asleep (33) or up (34).
		void roomLook(const Item::Hotel * room, int & in, int & asleep, int & up) const;

		// --- belong people (staff, office workers, students: TMDF 1) ---
		/// T2Tenant::CollectEmployee 0x1005f5a0 for `tenant` from its plugin's
		/// TMDF 1 (route score 1000); the types are kept for the tenant. True
		/// when hired (or when there is no pool or table to draw from).
		bool hireBelong(Item::Item * tenant);
		/// ClearBelongPeople: the tenant's belong people go back.
		void releaseBelong(Item::Item * tenant);
		const std::vector<Yoot::PeopleType> & belongOf(Item::Item * tenant) { return belong[tenant]; }
		const std::vector<Yoot::PeopleType> & belongOf(Item::Item * tenant) const {
			static const std::vector<Yoot::PeopleType> empty;
			const auto it = belong.find(tenant);
			return it == belong.end() ? empty : it->second;
		}
		/// Per belong person: T2People +0x78 flags kept while they are out
		/// of the tower (the School's colds).
		std::vector<int> & belongFlags(Item::Item * tenant) { return belongFlagMap[tenant]; }
		/// School.t2p's CallEmployee 0x100612a0: the called students walk in
		/// at their minutes (the sick stay home); students beyond `inside`
		/// (PushOutPeople / PushOutAll) walk home.
		void schoolTick(Item::Item * school, std::vector<long long> & arrivals, int inside, long long now);

		// --- subway riders and cruise passengers (T2Transport) ---
		/// A rider back at the station (Subway / Boat Deck): waits inside
		/// for the next stopped train or ship.
		void stationEnter(Item::Item * station, YootVisitor * v);

		// --- VIP walkers (T2Maru_Reggae 0x1003a770) ---
		/// BornVIP walking from the lobby to `target`, where he poses.
		YootVisitor * spawnVip(int kind, int silhouetteId, Item::Item * target);
		bool alive(const YootVisitor * v) const;
		/// From the tenant he poses at to the next one / out of the tower.
		void vipGo(YootVisitor * v, Item::Item * from, Item::Item * next);
		/// T2Tenant::CallBelongPeople 0x1005fb80: the hired staff who are
		/// not out walk to the tenant, each starting (rand() x 10) % 6
		/// minutes after `now`. Returns how many are on their way or in.
		int callStaff(Item::Item * tenant, long long now);
		/// Facility staffing, the plugin ChangeStatusOnIdle machines of
		/// Doctor's.t2p (FUN_1000be00), Maintenance.t2p (FUN_100133d0),
		/// Front.t2p and Securityroom.t2p (FUN_1000be00): the tenant
		/// hires its TMDF 1 belong people from the pool once built
		/// (status 10, T2Tenant::CollectEmployee, retried hourly) and
		/// calls them in for the opening -- Doctor's in
		/// [open-60, open-52), Front/Maintenance in [open, open+8); the
		/// Security Room never closes (its day and night shifts share the
		/// same belong people, swapped at 05:00 and 17:00). The tenant's
		/// population counts the hired staff, like an office's workers.
		/// Returns the hired staff count.
		int staffFacility(Item::Item * tenant, long long absMinute, int minuteOfDay,
		                  int callFromMinute, int spanMinutes);
		void vipLeave(YootVisitor * v, Item::Item * from);

		/// Skin::Yoot: the people walking on the floors and the posing VIPs
		/// (T2People::Draw 0x10049b70: frame = walk style + 3 x direction).
		void draw(sf::RenderTarget & target) const;

		/// The tenant took the visitor in (status 11) or turned it away.
		void arrived(YootVisitor * v, Item::Item * tenant, bool admitted, long long leaveMinute);
		/// T2Tenant::PushOutPeople 0x1005f720: the visitor leaves `tenant`.
		void pushOut(YootVisitor * v);
		/// Visitors bound for / inside a removed item.
		void itemRemoved(Item::Item * item);
		/// BOEV emergency stairs: clear trips, remove general people.
		void blackoutEscape();
		/// YootVisitor::yootWalkTo: starts the walk to a route node.
		bool walkToNode(YootVisitor * v, Item::Item * next, int floor);
		/// T2RoutingTable::SelectNearTenant 0x10055db0 from `tenant`'s
		/// entrance within `score` (null if none).
		Item::Item * selectNearTenant(Item::Item * tenant, int score);

		/// Diagnostics for the sim summary.
		int calledToday = 0;
		int enteredToday = 0;
		int ridersToday = 0;   ///< subway / ship riders landed at a station
		int returnedToday[3] = {0, 0, 0};  ///< T2Pool::Enter by estimate: <= 70, 71..200, > 200

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
		std::map<std::string, YootTenantDef> venueDefs;
		std::map<std::string, bool> venueDefValid;
		Item::Item * collectDest = nullptr;  ///< collectVenue: where the called walk to

		/// A guest room's stay (T2Tenant status 30..35, +0x88 outing flag,
		/// the belong list of guests, the linked desk).
		struct HotelStay
		{
			int status = 30;
			std::vector<YootVisitor *> party;
			Item::FrontDesk * desk = nullptr;
		};
		std::map<Item::Item *, HotelStay> stays;
		std::map<Item::Item *, bool> roomOuting;  ///< +0x88 (kept across stays)
		std::map<Item::Item *, std::vector<Yoot::PeopleType> > belong;
		std::map<Item::Item *, std::vector<int> > belongFlagMap;
		void hotelTick(long long now);
		void transportTick(long long now);
		void roomIdle(Item::Hotel * room, int minute, long long now);
		/// T2Tenant::UpdateResidencialEstimate for a guest room (belong list = the party).
		void roomEstimate(Item::Hotel * room, const std::vector<YootVisitor *> & party);
		void deskIdle(Item::FrontDesk * desk, long long now);
		bool callGuests(Item::Hotel * room, HotelStay & s, Item::FrontDesk * desk, long long now);
		void leaveParty(YootVisitor * v);

		bool ensurePool();
		void minuteTick(long long absMinute);
		void visitorTick(YootVisitor * v, long long now);
		YootVisitor * spawn(const Yoot::PeopleType & type, Item::Item * dest, long long start, unsigned specialFlag);
		Item::Item * entranceFor(YootVisitor * v);
		void enterTower(YootVisitor * v, long long now);
		void walkTo(YootVisitor * v, Item::Item * from, Item::Item * to, long long now);
		void goOutTower(YootVisitor * v);
		int entranceX(Item::Item * item, const YootVisitor * v) const;
		void beginWalk(YootVisitor * v, Item::Item * target, int floor, YootVisitor::WalkKind kind);
		void walkStep(YootVisitor * v);
		bool reachableWithin(Item::Item * from, Item::Item * to, int score);
		void destroy(YootVisitor * v);
		bool waitingAt(Item::Elevator::Elevator * e, Person * p) const;
	};
}
