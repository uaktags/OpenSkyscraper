/* Copyright (c) 2026 OpenSkyscraper Project */
#include "PeopleSystem.h"

#include "Game.h"
#include "Item/Elevator/Car.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Elevator/Queue.h"
#include "Item/YootSpecialItems.h"
#include "Item/YootTenant.h"
#include "Item/YootVenueItems.h"
#include "YootFacilities.h"
#include "YootRules.h"
#include "YootTenants.h"
#include "YootWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <tinyxml2.h>

using namespace OT;

namespace
{
	/// OpenSky performance guard (the original's people arrays grow without
	/// a limit; the pool bounds them).
	const size_t kMaxVisitors = 4000;
	/// T2People::PushOutPeople: no next destination -> home, start + 180.
	const int kHomeMinutes = 0xb4;
	const int kNoTime = 10000;

	Person::Type personTypeFor(const Yoot::PeopleType & t)
	{
		if (t.silhouette < 16) return Person::kChild;
		if (t.male()) return t.econo >= 3 ? Person::kSalesman : Person::kMan;
		return ((t.silhouette / 4) & 1) ? Person::kWoman2 : Person::kWoman1;
	}

	int rngInt() { return std::rand(); }

	int centerX(const Item::Item * i) { return i->position.x + i->size.x / 2; }
}

YootVisitor::YootVisitor(Game * game, const Yoot::PeopleType & type)
: Person(game, personTypeFor(type)), peopleType(type)
{
}

long long YootVisitor::freeTime(long long now) const
{
	// T2People::CalcFreeTime 0x100495b0.
	if (phase == kLeaving) return startMinute > now ? startMinute - now : 0;
	if (!dest)
	{
		if (returns.empty()) return kNoTime;
		const long long t = returns.back().second;
		return t > now ? t - now : 0;
	}
	return startMinute > now ? startMinute - now : 0;
}

bool YootVisitor::yootWalkTo(Item::Item * next, int floor)
{
	return game->eventSystem.people.walkToNode(this, next, floor);
}

PeopleSystem::PeopleSystem(Game * game) : GameObject(game)
{
}

PeopleSystem::~PeopleSystem()
{
	for (YootVisitor * v : visitorList) delete v;
	visitorList.clear();
}

void PeopleSystem::reset()
{
	for (YootVisitor * v : visitorList) delete v;
	visitorList.clear();
	pool = Yoot::Pool();
	def = Yoot::PoolDef();
	defLoaded = false;
	worldId.clear();
	lastMinute = -1;
	pendingPool.clear();
	routeCache.clear();
	tables.clear();
	stays.clear();
	roomOuting.clear();
	calledToday = enteredToday = ridersToday = 0;
	returnedToday[0] = returnedToday[1] = returnedToday[2] = 0;
}

bool PeopleSystem::enabled() const
{
	return game && game->activeRules.profile().yootWorld;
}

bool PeopleSystem::ensurePool()
{
	const std::string id = game->activeRules.profile().worldId;
	if (!defLoaded || id != worldId)
	{
		worldId = id;
		defLoaded = true;
		def = Yoot::PoolDef();
		YootWorld::Id wid;
		if (parseYootWorldIdentifier(id, wid))
		{
			AbstractPlugin * p = game->app.pluginManager.get(yootWorldPluginFile(wid));
			if (p && p->ensureLoaded()) Yoot::loadPoolDef(*p, def);
		}
		if (!pendingPool.empty())
		{
			pool.decode(pendingPool);
			pendingPool.clear();
		}
	}
	if (!def.valid) return false;
	if (pool.empty())
	{
		const int hour = std::max(0, std::min(23, (int)std::floor(game->time.hour)));
		pool.create(def, game->rating + 1, hour);
	}
	return true;
}

void PeopleSystem::advance()
{
	if (!enabled() || !ensurePool()) return;
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	if (lastMinute < 0 || now < lastMinute || now - lastMinute > 1440) lastMinute = now - 1;
	for (long long m = lastMinute + 1; m <= now; ++m) minuteTick(m);
	lastMinute = now;
	if (pool.isChangePopulation()) game->populationNeedsUpdate = true;
}

void PeopleSystem::minuteTick(long long m)
{
	const int minute = (int)(((m % 1440) + 1440) % 1440);
	if (minute % 60 == 0)
	{
		pool.incHour(minute / 60);
		routeCache.clear();
		{
			// The visitors by T2People status (1 pool, 5/10/12 walking, 6
			// waiting for a mover, 8 riding, 11 inside), for the oracle.
			int pooled = 0, walking = 0, waiting = 0, riding = 0, inside = 0, other = 0;
			for (YootVisitor * v : visitorList)
			{
				if (v->phase == YootVisitor::kInPool) ++pooled;
				else if (v->phase == YootVisitor::kInTenant) ++inside;
				else if (v->walking) ++walking;
				else if (v->at && v->at->isElevator()) (waitingAt(static_cast<Item::Elevator::Elevator *>(v->at), v) &&
					!static_cast<Item::Elevator::Elevator *>(v->at)->cars.empty() && std::none_of(
						static_cast<Item::Elevator::Elevator *>(v->at)->cars.begin(), static_cast<Item::Elevator::Elevator *>(v->at)->cars.end(),
						[v](Item::Elevator::Car * c) { return c->passengers.count(v) > 0; }) ? waiting : riding)++;
				else ++other;
			}
			game->recordSimEvent("people " + std::to_string(minute / 60) + "h: pool " + std::to_string(pooled) + " walking " +
				std::to_string(walking) + " waiting " + std::to_string(waiting) + " riding " + std::to_string(riding) +
				" inside " + std::to_string(inside) + " other " + std::to_string(other));
		}
		if (minute == 0)
		{
			int phases[5] = {0, 0, 0, 0, 0};
			for (const YootVisitor * v : visitorList) phases[v->phase]++;
			game->recordSimEvent("pool: called " + std::to_string(calledToday) + " entered " + std::to_string(enteredToday) +
				" population " + std::to_string(pool.population) + " visitors " + std::to_string(visitorList.size()) +
				" (pool " + std::to_string(phases[0]) + " walking " + std::to_string(phases[1] + phases[2]) +
				" inside " + std::to_string(phases[3]) + " leaving " + std::to_string(phases[4]) + ")" +
				" returned " + std::to_string(returnedToday[0]) + "/" + std::to_string(returnedToday[1]) + "/" + std::to_string(returnedToday[2]) +
				(ridersToday ? " riders " + std::to_string(ridersToday) : std::string()));
			calledToday = enteredToday = ridersToday = 0;
			returnedToday[0] = returnedToday[1] = returnedToday[2] = 0;
		}
	}
	// T2.exe FUN_0041da20: T2Pool::GameLevelChanged on every grade change.
	const int grade = game->rating + 1;
	if (grade != pool.gameLevel) pool.gameLevelChanged(def, grade);

	const std::vector<YootVisitor *> list = visitorList;
	for (YootVisitor * v : list)
		if (std::find(visitorList.begin(), visitorList.end(), v) != visitorList.end())
			visitorTick(v, m);
	hotelTick(m);
	transportTick(m);
}

void PeopleSystem::transportTick(long long now)
{
	// Subway.t2p / Boatdeck.t2p FUN_1000d330 while the train (8 minutes) or
	// ship (30) is in: the riders aboard get off at the station and set off
	// for their tenant with a return to the station (LeaveFirst, SetReturn);
	// the people back at the station board it; at departure the ones
	// aboard go into the pool (T2Transport::Stop 0x1006bf70). The station
	// closing pushes everyone inside out.
	for (int k = 0; k < 2; ++k)
	{
		const bool ship = k == 1;
		Item::Item * stopped = game->eventSystem.venues.transportStopped(ship);
		const std::vector<YootVisitor *> list = visitorList;
		for (YootVisitor * v : list)
		{
			if (!v->aboard || std::find(visitorList.begin(), visitorList.end(), v) == visitorList.end()) continue;
			const bool here = v->aboard->prototype->id == (ship ? "boatdeck" : "subway");
			if (!here) continue;
			if (!v->dest)
			{
				if (v->aboard != stopped) goOutTower(v);  // the train left with them
				continue;
			}
			if (v->aboard != stopped) continue;
			Item::Item * station = v->aboard;
			v->aboard = nullptr;
			v->curH = entranceX(station, v);
			v->curV = station->position.y;
			if (v->returns.size() < 3) v->returns.push_back(std::make_pair(station, now));
			game->eventSystem.facilities.onEnterTower(v);
			if (Item::YootVenue * deck = dynamic_cast<Item::YootVenue *>(station)) deck->visitorsToday++;
			++ridersToday;
			walkTo(v, station, v->dest, now);
		}
		const Game::ItemSet & stations = game->itemsByType[ship ? "boatdeck" : "subway"];
		for (Item::Item * station : std::vector<Item::Item *>(stations.begin(), stations.end()))
		{
			Item::Subway * subway = dynamic_cast<Item::Subway *>(station);
			Item::YootVenue * deck = dynamic_cast<Item::YootVenue *>(station);
			const bool open = subway ? subway->open : deck && deck->isOpen();
			for (YootVisitor * v : visitorsIn(station))
			{
				if (!open) { pushOut(v); continue; }
				if (station != stopped || v->dest) continue;
				station->removePerson(v);
				v->inside = nullptr;
				v->aboard = station;
			}
		}
	}
}

void PeopleSystem::stationEnter(Item::Item * station, YootVisitor * v)
{
	if (v->dest != station) return;
	Item::Subway * subway = dynamic_cast<Item::Subway *>(station);
	Item::YootVenue * deck = dynamic_cast<Item::YootVenue *>(station);
	const bool open = subway ? subway->open : deck && deck->isOpen();
	// EnterTenant clears the destination: the rider waits for a train.
	arrived(v, station, open, 100000000LL);
	v->stayEnd = -1;
}

// ------------------------------------------------------------------ routes

bool PeopleSystem::reachableWithin(Item::Item * from, Item::Item * to, int score)
{
	if (!from || !to) return false;
	const int dx = std::abs(centerX(from) - centerX(to));
	const bool sameFloor = from->position.y == to->position.y ||
		(from->isLobby() && to->position.y >= from->position.y && to->position.y < from->position.y + from->size.y);
	if (sameFloor) return dx <= score;
	// T2RoutingTable::CheckRoute: the mover route score (MoverDef score x
	// floors, P3) plus the walk must stay within the PLDF score. Routes are
	// cached until the hour or the item set changes.
	if (game->items.size() != routeCacheItems) { routeCache.clear(); routeCacheItems = game->items.size(); }
	const std::pair<unsigned long long, unsigned long long> key(from->simSerial, to->simSerial);
	std::map<std::pair<unsigned long long, unsigned long long>, int>::iterator c = routeCache.find(key);
	if (c == routeCache.end())
	{
		const Route & r = game->findRoute(from, to);
		c = routeCache.insert(std::make_pair(key, r.empty() ? -1 : r.score())).first;
	}
	return c->second >= 0 && c->second + dx <= score;
}

unsigned PeopleSystem::routeMask(Item::Item * tenant, int score)
{
	Yoot::RouteFromPoolInputs in;
	in.lobby = game->mainLobby && reachableWithin(game->mainLobby, tenant, score);
	const Game::ItemSet & lots = game->itemsByType["parkinglot"];
	for (Item::Item * i : lots)
	{
		Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(i);
		if (!lot || lot->underConstruction || !lot->model.hasSpace()) continue;
		// FindEmptyParking returns the first lot with room.
		in.parking = reachableWithin(lot, tenant, score);
		break;
	}
	const Game::ItemSet & subways = game->itemsByType["subway"];
	for (Item::Item * i : subways)
	{
		Item::Subway * s = dynamic_cast<Item::Subway *>(i);
		if (!s || s->underConstruction || !s->open) continue;
		in.subway = reachableWithin(s, tenant, score);
		in.trainAvailable = game->eventSystem.venues.transportAvailable(false);
		break;
	}
	const Game::ItemSet & decks = game->itemsByType["boatdeck"];
	for (Item::Item * i : decks)
	{
		Item::YootVenue * d = dynamic_cast<Item::YootVenue *>(i);
		if (!d || d->underConstruction || !d->isOpen()) continue;
		in.ship = reachableWithin(d, tenant, score);
		in.cruiseAvailable = game->eventSystem.venues.transportAvailable(true);
		break;
	}
	return Yoot::checkRouteFromPool(in);
}

// ----------------------------------------------------------------- sources

YootVisitor * PeopleSystem::spawn(const Yoot::PeopleType & type, Item::Item * dest, long long start, unsigned specialFlag)
{
	YootVisitor * v = new YootVisitor(game, type);
	v->dest = dest;
	v->startMinute = start;
	v->yootFlags |= (int)specialFlag;  // T2People::SetSpecialFlag
	v->goingTo = dest->prototype->name;
	v->state = Person::kShopping;
	visitorList.push_back(v);
	++calledToday;
	return v;
}

int PeopleSystem::callFromPool(Item::Item * tenant, const YootTenantDef & def, int free, long long now)
{
	if (visitorList.size() >= kMaxVisitors) return 0;
	const unsigned mask = routeMask(tenant, Yoot::customerScore(def, 0));
	if (mask == 0) return 0;
	const int minute = (int)(((now % 1440) + 1440) % 1440);
	const unsigned flag = def.customers.specialFlag;
	return Yoot::collectFromPool(def, pool, mask, free, minute, rngInt,
		[&](const Yoot::PeopleType & t, int start) { spawn(t, collectDest ? collectDest : tenant, now + (start - minute), flag); });
}

namespace
{
	/// Visitors inside an item (status 11), in the item's stable order.
	std::vector<YootVisitor *> visitorsInside(Item::Item * item)
	{
		std::vector<YootVisitor *> out;
		for (Person * p : item->people)
		{
			YootVisitor * v = dynamic_cast<YootVisitor *>(p);
			if (v && v->phase == YootVisitor::kInTenant && v->inside == item) out.push_back(v);
		}
		return out;
	}

	/// T2CustomerTableIterator over the customer table: members with a
	/// non-zero Float2Int(rate), from a random start, once round.
	template <typename F>
	int forEachMember(const YootTenantDef & def, int free, F && f)
	{
		const int n = (int)def.customers.members.size();
		if (n == 0 || free <= 0) return 0;
		const float units = Yoot::customerUnits(def.capacity, def.bindMinutes);
		const int first = std::rand() % n;
		int called = 0;
		for (int k = 0; k < n && free > 0; ++k)
		{
			const MemberDef & m = def.customers.members[(first + k) % n];
			int count = Yoot::float2IntRng(Yoot::memberRate(m.percent, units), rngInt);
			if (count == 0) continue;
			count = std::min(count, free);
			const int got = f(m, count);
			called += got;
			free -= got;
		}
		return called;
	}
}

namespace
{
	/// T2People +0x6c holds the equip type of the last tenant left, so a
	/// person is not called to the same kind of tenant twice in a row.
	bool sameKind(const Item::Item * a, const Item::Item * b)
	{
		return a && b && a->prototype == b->prototype;
	}

	/// Movers a person can board on `floor` (T2FloorInfo mover list of the
	/// floor, T2RoutingTable::IsRelatedMover), in the item order.
	std::vector<Item::Item *> moversOnFloor(Game * game, int floor)
	{
		std::vector<Item::Item *> out;
		for (Item::Item * i : game->items)
			if (!i->underConstruction && (i->isElevator() || i->isStairlike()) && i->canHaulPeople() && i->connectsFloor(floor))
				out.push_back(i);
		return out;
	}
}

Item::Item * PeopleSystem::selectNearTenant(Item::Item * tenant, int score)
{
	// T2RoutingTable::SelectNearTenant 0x10055db0 from the tenant's entrance:
	// a random related mover of the entrance floor (scanning on from a
	// random start), then T2MoverRoutingTable::SelectNearFloor 0x100428d0:
	// the walk to the mover (CalcHScore = |dx|) comes off the score, and a
	// random floor the mover serves within the rest (MoverDef score x
	// floors, P3) becomes the search floor, at the mover, with what is left.
	// Without one the entrance floor is searched (with the score already
	// reduced by the walk when that left something).
	int floor = tenant->position.y;
	int x = centerX(tenant);
	int s = score;
	const std::vector<Item::Item *> movers = moversOnFloor(game, floor);
	if (!movers.empty())
	{
		Item::Item * m = movers[std::rand() % movers.size()];
		const int mx = centerX(m);
		const int left = s - std::abs(mx - x);
		if (left > 0)
		{
			s = left;
			const int moverScore = m->prototype->yoot ? m->prototype->yoot->routeScore : 0;
			std::vector<std::pair<int, int> > floors;  // floor, score left
			for (int f = m->position.y; f < m->position.y + m->size.y; ++f)
			{
				if (!m->connectsFloor(f)) continue;
				const int cost = Yoot::moverRouteScore(moverScore, f - tenant->position.y);
				if (cost < s && s - cost != 0) floors.push_back(std::make_pair(f, s - cost));
			}
			if (!floors.empty())
			{
				const std::pair<int, int> & pick = floors[std::rand() % floors.size()];
				floor = pick.first;
				x = mx;
				s = pick.second;
			}
		}
	}
	// T2RoutingTable::SelectNearTenant 0x10055ec0: tenants of that floor in
	// [x - s, x + s] with people inside, one at random.
	std::vector<Item::Item *> near;
	for (Item::Item * i : game->items)
	{
		if (i == tenant || i->underConstruction || i->position.y != floor) continue;
		if (i->position.x + i->size.x <= x - s || i->position.x >= x + s) continue;
		if (!visitorsInside(i).empty()) near.push_back(i);
	}
	if (near.empty()) return nullptr;
	return near[std::rand() % near.size()];
}

int PeopleSystem::callFromTenant(Item::Item * tenant, const YootTenantDef & def, int free, long long now)
{
	// T2Tenant::CollectCustomerFromTenant 0x1005f0c0: the people inside a
	// tenant near the entrance (SelectNearTenant) within the PLDF tenant
	// score.
	Item::Item * from = selectNearTenant(tenant, Yoot::customerScore(def, 1));
	if (!from) return 0;
	const int econo = Yoot::customerEconoType(def);
	const unsigned flag = def.customers.specialFlag;
	return forEachMember(def, free, [&](const MemberDef & m, int count) {
		int got = 0;
		for (YootVisitor * v : visitorsInside(from))
		{
			if (got >= count) break;
			// T2People::CalledInTenant 0x10049550.
			if (((unsigned)v->yootFlags & flag) != flag) continue;
			if (v->freeTime(now) <= def.bindMinutes || sameKind(v->prevTenant, tenant)) continue;
			if (!Yoot::checkWithDemand(v->peopleType, m, econo)) continue;
			v->dest = collectDest ? collectDest : tenant;
			v->startMinute = now;
			v->goingTo = v->dest->prototype->name;
			++got;
		}
		return got;
	});
}

int PeopleSystem::callFromFloor(Item::Item * tenant, const YootTenantDef & def, int free, long long now)
{
	// T2Tenant::CollectCustomerFromFloor 0x1005f250 / StopOnHisWay
	// 0x100494b0: people walking on the tenant's floor (the floor tenant's
	// people) whose position is within the score of the tenant (negative:
	// without the free-time check) and who are not heading for the same
	// kind of tenant turn in, pushing their destination onto the return
	// stack (3 deep).
	int score = Yoot::customerScore(def, 2);
	const bool checkFree = score >= 0;
	score = std::abs(score);
	const int floor = tenant->position.y;
	const int left = tenant->position.x, right = tenant->position.x + tenant->size.x;
	std::vector<YootVisitor *> passing;
	for (YootVisitor * v : visitorList)
		if (v->walking && v->curV == floor && Yoot::passesTenant(v->curH, left, right, score) && !sameKind(v->dest, tenant))
			passing.push_back(v);
	if (passing.empty()) return 0;
	const int econo = Yoot::customerEconoType(def);
	const unsigned flag = def.customers.specialFlag;
	return forEachMember(def, free, [&](const MemberDef & m, int count) {
		int got = 0;
		for (YootVisitor * v : passing)
		{
			if (got >= count) break;
			if (v->dest == tenant || !v->walking) continue;
			if (((unsigned)v->yootFlags & flag) != flag) continue;
			if (checkFree && v->freeTime(now) <= def.bindMinutes) continue;
			if (sameKind(v->prevTenant, tenant)) continue;
			if (!Yoot::checkWithDemand(v->peopleType, m, econo)) continue;
			if (v->returns.size() >= 3) continue;  // SetDestinationToReturn failed
			v->returns.push_back(std::make_pair(v->dest, v->startMinute));
			v->dest = collectDest ? collectDest : tenant;
			v->startMinute = now;
			v->goingTo = v->dest->prototype->name;
			v->state = Person::kShopping;
			// ChangeStatus(3): a new route from where the person stands.
			v->journey.clear();
			if (v->dest->position.y == floor) beginWalk(v, v->dest, floor, YootVisitor::kWalkDest);
			else walkTo(v, tenant, v->dest, now);
			++got;
		}
		return got;
	});
}

int PeopleSystem::drawVisitors(Item::Item * item, int n)
{
	if (n <= 0 || !enabled() || pool.empty() || !item->prototype->yoot) return n;
	const std::string & plugin = item->prototype->yoot->plugin;
	std::map<std::string, Table>::iterator ti = tables.find(plugin);
	if (ti == tables.end())
	{
		Table t;
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		if (p && p->ensureLoaded() && p->getMemberTable(t.table, 2) && !t.table.members.empty())
		{
			t.valid = true;
			if (t.table.placeParamID != 0) t.hasPlace = p->getPlaceParamDef(t.place, t.table.placeParamID);
		}
		ti = tables.insert(std::make_pair(plugin, t)).first;
	}
	const Table & t = ti->second;
	if (!t.valid) return n;
	const int score = t.hasPlace ? t.place.places[0].score() : 1000;
	const unsigned mask = routeMask(item, score);
	if (mask == 0) return 0;
	const int econo = t.table.econoType == 0xFFFFFFFFu ? -1 : int(t.table.econoType & ~0x10u);
	const int members = (int)t.table.members.size();
	int k = std::rand() % members, misses = 0, drawn = 0;
	while (drawn < n && misses < members)
	{
		Yoot::PeopleType type;
		if (pool.call(t.table.members[k], econo, mask, type, rngInt))
		{
			// The visit is not walked: the person returns satisfied at once.
			pool.enter(type, Yoot::kPeopleEstimateStart, rngInt);
			++drawn;
			misses = 0;
		}
		else ++misses;
		k = (k + 1) % members;
	}
	return drawn;
}

// ------------------------------------------------------------------ venues

const YootTenantDef * PeopleSystem::venueDef(Item::Item * venue, const std::string & subPlugin, int tableId)
{
	if (!venue->prototype->yoot) return nullptr;
	const std::string & plugin = venue->prototype->yoot->plugin;
	const std::string key = plugin + "|" + subPlugin + "|" + std::to_string(tableId);
	std::map<std::string, bool>::const_iterator known = venueDefValid.find(key);
	if (known != venueDefValid.end()) return known->second ? &venueDefs[key] : nullptr;
	YootTenantDef d;
	bool ok = false;
	AbstractPlugin * p = game->app.pluginManager.get(plugin);
	const YootTenantPlugin entry = {plugin.c_str(), venue->prototype->id.c_str(), YootTenantFamily::None};
	if (p && p->ensureLoaded() && loadYootTenantDef(*p, entry, d))
	{
		ok = d.hasCustomers;
		if (!subPlugin.empty())
		{
			// T2Tenant::SetCustomerTable 0x10062c00: the sub-plugin's table.
			AbstractPlugin * sp = game->app.pluginManager.get(subPlugin);
			MemberTableDef t;
			ok = sp && sp->ensureLoaded() && sp->getMemberTable(t, tableId) && !t.members.empty();
			if (ok)
			{
				d.customers = t;
				d.hasCustomers = true;
				d.hasDayParam = t.dayParamID != 0 && sp->getDayParamDef(d.dayParam, t.dayParamID);
				d.hasPlaceParam = t.placeParamID != 0 && sp->getPlaceParamDef(d.placeParam, t.placeParamID);
				d.bindMinutes = 1;
			}
		}
	}
	venueDefValid[key] = ok;
	if (!ok) return nullptr;
	venueDefs[key] = d;
	return &venueDefs[key];
}

int PeopleSystem::collectVenue(Item::Item * venue, const YootTenantDef & def, int free, long long now, Item::Item * dest)
{
	if (!enabled() || pool.empty() || free <= 0 || visitorList.size() >= kMaxVisitors) return 0;
	// CollectCustomer 0x1005f440 after the venue's own gates: the pool
	// (IsCollectFromPool 0x10063430: PLDF rate and DPDF holiday rate, fair
	// weather), then the tenant and the floor sources by their PLDF rates.
	collectDest = dest && dest != venue ? dest : nullptr;
	auto roll = [](uint32_t percent) { return Yoot::float2IntRng((float)percent * 0.01f, rngInt) != 0; };
	const bool holiday = game->time.isHoliday();
	int called = 0;
	bool fromPool = !def.hasPlaceParam || roll(def.placeParam.places[0].percent);
	if (fromPool && def.hasDayParam && !roll(def.dayParam.percent[holiday ? 1 : 0][0])) fromPool = false;
	if (fromPool) called += callFromPool(venue, def, free - called, now);
	if (def.hasPlaceParam && roll(def.placeParam.places[1].percent) && free - called > 0)
		called += callFromTenant(venue, def, free - called, now);
	if (def.hasPlaceParam && roll(def.placeParam.places[2].percent) && free - called > 0)
		called += callFromFloor(venue, def, free - called, now);
	collectDest = nullptr;
	return called;
}

std::vector<YootVisitor *> PeopleSystem::visitorsIn(Item::Item * item) const
{
	return visitorsInside(item);
}

void PeopleSystem::releaseVisitors(Item::YootFacility * f, long long now)
{
	if (f->people.empty()) return;
	std::vector<YootVisitor *> inside = visitorsInside(f);
	// T2People::Idle case 11: the stay ended.
	for (size_t k = 0; k < inside.size();)
	{
		YootVisitor * v = inside[k];
		if (v->startMinute > now) { ++k; continue; }
		std::vector<long long>::iterator e = std::find(f->leaveAt.begin(), f->leaveAt.end(), v->stayEnd);
		if (e != f->leaveAt.end()) f->leaveAt.erase(e);
		inside.erase(inside.begin() + k);
		pushOut(v);
	}
	// PushOutAll / PushOutCustomer(n) shrank the list: the first due leave.
	std::stable_sort(inside.begin(), inside.end(), [](const YootVisitor * a, const YootVisitor * b) { return a->startMinute < b->startMinute; });
	for (size_t k = 0; inside.size() - k > f->leaveAt.size(); ++k) pushOut(inside[k]);
}

// --------------------------------------------------------------- lifecycle

bool PeopleSystem::waitingAt(Item::Elevator::Elevator * e, Person * p) const
{
	for (Item::Elevator::Queue * q : e->queues)
		if (std::find(q->people.begin(), q->people.end(), p) != q->people.end()) return true;
	for (Item::Elevator::Car * c : e->cars)
		if (c->passengers.count(p)) return true;
	return false;
}

Item::Item * PeopleSystem::entranceFor(YootVisitor * v)
{
	// T2People::MoveToTowerEntrance 0x10048910: by transport type; a
	// missing entrance costs 100 estimate and sends the person to the lobby.
	Item::Item * e = nullptr;
	switch (v->peopleType.transport)
	{
	case Yoot::kTransportCar:
		for (Item::Item * i : game->itemsByType["parkinglot"])
		{
			Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(i);
			if (lot && !lot->underConstruction && lot->park(v)) { e = lot; break; }
		}
		break;
	case Yoot::kTransportSubway:
	case Yoot::kTransportShip:
	{
		// MoveToSubway 0x10048a90 / MoveToShip 0x10048af0: board a train or
		// ship arriving within the hour (T2Transport::Enter, at most 300
		// aboard); it lands the rider at the station when it stops.
		const bool ship = v->peopleType.transport == Yoot::kTransportShip;
		Item::Item * station = nullptr;
		for (Item::Item * i : game->itemsByType[ship ? "boatdeck" : "subway"])
			if (!i->underConstruction) { station = i; break; }
		if (station && game->eventSystem.venues.transportAvailable(ship))
		{
			int aboard = 0;
			for (const YootVisitor * o : visitorList) if (o->aboard == station) ++aboard;
			if (aboard < 300) { v->aboard = station; return nullptr; }
		}
		break;
	}
	default:
		return game->mainLobby;
	}
	if (!e)
	{
		v->yootEstimate = Yoot::incEstimate(v->yootEstimate, -100);
		return game->mainLobby;
	}
	return e;
}

void PeopleSystem::enterTower(YootVisitor * v, long long now)
{
	// T2People::Idle case 1: the start minute came.
	Item::Item * from = entranceFor(v);
	if (v->aboard) return;  // riding in (transportTick)
	if (!from || !v->dest) { goOutTower(v); return; }
	if (from != game->mainLobby) game->eventSystem.facilities.onEnterTower(v);
	// MoveToLobby 0x10048990: transport 0 appears at the lobby's left
	// edge, the others at its right edge - 2.
	v->curH = entranceX(from, v);
	v->curV = from->position.y;
	walkTo(v, from, v->dest, now);
}

int PeopleSystem::entranceX(Item::Item * item, const YootVisitor * v) const
{
	if (item->isLobby())
		return v->peopleType.transport == Yoot::kTransportLobbyLeft ? item->position.x : item->position.x + item->size.x - 2;
	return centerX(item);
}

void PeopleSystem::beginWalk(YootVisitor * v, Item::Item * target, int floor, YootVisitor::WalkKind kind)
{
	if (v->at) v->at->removePerson(v);
	v->walking = true;
	v->walkTarget = target;
	v->walkKind = kind;
	v->curV = floor;
	v->dstH = entranceX(target, v);
	if (kind != YootVisitor::kWalkNode) v->phase = kind == YootVisitor::kWalkExit ? YootVisitor::kLeaving : YootVisitor::kStopping;
}

bool PeopleSystem::walkToNode(YootVisitor * v, Item::Item * next, int floor)
{
	// Movers and the end of the route are walked to; a lobby on the way is
	// crossed by the lobby's own atrium walk.
	if (!enabled() || (!v->journey.atLastNode() && !next->isElevator() && !next->isStairlike())) return false;
	beginWalk(v, next, floor, YootVisitor::kWalkNode);
	if (v->curH != v->dstH) return true;
	v->walking = false;
	v->walkTarget = nullptr;
	return false;
}

void PeopleSystem::walkStep(YootVisitor * v)
{
	// T2People::IsReachDestination 0x10049310, once per people idle: 16
	// units towards the destination; off the ground floor each step costs
	// IncStress(step / 4).
	v->facingLeft = v->dstH < v->curH;
	const int step = Yoot::walkStep(v->curH, v->dstH);
	if (v->curV != (game->mainLobby ? game->mainLobby->position.y : 0)) v->addStress(step / 4);
	if (v->curH != v->dstH) return;
	Item::Item * t = v->walkTarget;
	v->walking = false;
	v->walkTarget = nullptr;
	switch (v->walkKind)
	{
	case YootVisitor::kWalkNode:
		if (v->journey.atLastNode()) v->yootArrived(t);
		t->addPerson(v);
		break;
	case YootVisitor::kWalkDest:
		if (v->dest) v->dest->addPerson(v);
		else goOutTower(v);
		break;
	case YootVisitor::kWalkExit:
		goOutTower(v);
		break;
	}
}

void PeopleSystem::walkTo(YootVisitor * v, Item::Item * from, Item::Item * to, long long now)
{
	v->phase = to == game->mainLobby && !v->dest ? YootVisitor::kLeaving : YootVisitor::kWalking;
	if (!from || !to) { if (v->phase == YootVisitor::kLeaving) goOutTower(v); return; }
	const bool sameFloor = from->position.y == to->position.y ||
		(from->isLobby() && to->position.y >= from->position.y && to->position.y < from->position.y + from->size.y) ||
		(to->isLobby() && from->position.y >= to->position.y && from->position.y < to->position.y + to->size.y);
	Route r;
	if (!sameFloor) r = game->findRoute(from, to);
	if (!r.empty())
	{
		v->noRouteTries = 0;
		v->journey.set(r);
		return;
	}
	if (sameFloor)
	{
		// A walk along the floor.
		if (from->isLobby() && v->dest) game->eventSystem.facilities.onEnterTower(v);
		beginWalk(v, to, to->isLobby() ? from->position.y : to->position.y,
			v->phase == YootVisitor::kLeaving ? YootVisitor::kWalkExit : YootVisitor::kWalkDest);
		return;
	}
	// T2People::SolveNoRoute 0x10048c40: +30 stress per try; at 300 the
	// person jumps to the destination with -150 estimate.
	v->recordNoRoute(from->position.y, to->position.y);
	++v->noRouteTries;
	if (v->stress < Yoot::kMaxStress && v->noRouteTries < 10) return;
	v->noRouteTries = 0;
	if (v->at) v->at->removePerson(v);
	if (v->phase == YootVisitor::kLeaving) { goOutTower(v); return; }
	v->phase = YootVisitor::kStopping;
	v->arriveMinute = now;
}

void PeopleSystem::visitorTick(YootVisitor * v, long long now)
{
	if (v->walking)
	{
		if (now % Yoot::kPeopleIdleMinutes == 0) walkStep(v);
		return;
	}
	switch (v->phase)
	{
	case YootVisitor::kInPool:
		if (!v->aboard && now >= v->startMinute) enterTower(v, now);
		break;
	case YootVisitor::kWalking:
	case YootVisitor::kLeaving:
		if (v->at && v->at->isElevator() && now % Yoot::kPeopleIdleMinutes == 0 && !waitingAt(static_cast<Item::Elevator::Elevator *>(v->at), v))
		{
			// The waiting area was full (Queue rejects the 41st person): the
			// person keeps waiting (IdleWaitMover stress) and tries again.
			Item::Item * e = v->at;
			if (v->yootWaitTick()) break;
			e->removePerson(v);
			e->addPerson(v);
			break;
		}
		if (v->phase == YootVisitor::kLeaving)
		{
			if (!v->at && !v->journey.item) goOutTower(v);
			break;
		}
		if (!v->at && !v->journey.item)
		{
			// Still without a route (SolveNoRoute retries each people idle).
			if (v->noRouteTries > 0 && now % Yoot::kPeopleIdleMinutes == 0)
			{
				Item::Item * from = v->inside ? v->inside : (v->prevTenant ? v->prevTenant : game->mainLobby);
				walkTo(v, from, v->dest, now);
			}
			else if (v->noRouteTries == 0 && v->dest)
				v->dest->addPerson(v);  // the journey ended at the tenant's door
		}
		break;
	case YootVisitor::kStopping:
		// Jumped to the destination (SolveNoRoute gave up).
		if (now >= v->arriveMinute)
		{
			if (v->dest) v->dest->addPerson(v);
			else goOutTower(v);
		}
		break;
	case YootVisitor::kInTenant:
		break;
	}
}

void PeopleSystem::arrived(YootVisitor * v, Item::Item * tenant, bool admitted, long long leaveMinute)
{
	if (admitted)
	{
		// T2Tenant::EnterTenant: ClearDestination, UpdateEstimation, stay.
		v->phase = YootVisitor::kInTenant;
		v->inside = tenant;
		v->curH = centerX(tenant);
		v->curV = tenant->position.y;
		v->dest = nullptr;
		v->startMinute = leaveMinute;
		v->noRouteTries = 0;
		v->yootFinishTrip();
		++enteredToday;
		return;
	}
	// Not open: T2People::Return and -100 estimate.
	v->yootEstimate = Yoot::incEstimate(v->yootEstimate, -100);
	v->curH = centerX(tenant);
	v->curV = tenant->position.y;
	v->inside = tenant;
	v->dest = nullptr;
	pushOut(v);
}

void PeopleSystem::pushOut(YootVisitor * v)
{
	Item::Item * from = v->inside;
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	v->prevTenant = from;
	v->inside = nullptr;
	if (!v->dest && !v->returns.empty())
	{
		v->dest = v->returns.back().first;
		v->startMinute = v->returns.back().second;
		v->returns.pop_back();
	}
	if (v->dest)
	{
		v->goingTo = v->dest->prototype->name;
		walkTo(v, from, v->dest, now);
		return;
	}
	v->startMinute = now + kHomeMinutes;
	v->goingTo = "Exit";
	v->state = Person::kReturning;
	walkTo(v, from, game->mainLobby, now);
}

void PeopleSystem::goOutTower(YootVisitor * v)
{
	// T2People::GoOutTower 0x10048f40 -> T2Pool::Enter 0x10050610.
	if (v->at) v->at->removePerson(v);
	v->walking = false;
	v->walkTarget = nullptr;
	v->aboard = nullptr;
	for (Item::Item * i : game->itemsByType["parkinglot"])
	{
		Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(i);
		if (lot) lot->leave(v);
	}
	if (v->yootFlags & Yoot::kPeopleNeedsToilet) v->yootEstimate = Yoot::incEstimate(v->yootEstimate, -30);
	v->yootFinishTrip();
	++returnedToday[v->yootEstimate > 200 ? 2 : v->yootEstimate > 70 ? 1 : 0];
	pool.enter(v->peopleType, v->yootEstimate, rngInt);
	destroy(v);
}

void PeopleSystem::destroy(YootVisitor * v)
{
	leaveParty(v);
	std::vector<YootVisitor *>::iterator it = std::find(visitorList.begin(), visitorList.end(), v);
	if (it != visitorList.end()) visitorList.erase(it);
	delete v;
}

void PeopleSystem::itemRemoved(Item::Item * item)
{
	stays.erase(item);
	roomOuting.erase(item);
	for (std::map<Item::Item *, HotelStay>::iterator s = stays.begin(); s != stays.end(); ++s)
		if (s->second.desk == item) s->second.desk = nullptr;
	// T2People::TenantRemoved: people bound for or inside the item, and
	// everyone on a route (whose nodes may name the item), go home.
	const std::vector<YootVisitor *> list = visitorList;
	for (YootVisitor * v : list)
	{
		for (size_t k = 0; k < v->returns.size();)
			if (v->returns[k].first == item) v->returns.erase(v->returns.begin() + k);
			else ++k;
		if (v->prevTenant == item) v->prevTenant = nullptr;
		const bool walking = v->phase == YootVisitor::kWalking || v->phase == YootVisitor::kLeaving;
		const bool bound = v->dest == item || v->inside == item || v->at == item || v->walkTarget == item || v->aboard == item;
		if (bound || (walking && item->canHaulPeople()))
		{
			v->dest = nullptr;
			v->inside = nullptr;
			if (v->at) v->at->removePerson(v);
			v->journey.clear();
			goOutTower(v);
		}
	}
}

// ------------------------------------------------------------------ saving

void PeopleSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	if (!enabled() || pool.empty()) return;
	// Visitors are transient: the save holds them back in the pool
	// (T2Pool::RemovePeople: immortal types return, duplicates are lost).
	Yoot::Pool copy = pool;
	for (const YootVisitor * v : visitorList) copy.removePeople(v->peopleType);
	std::string s;
	copy.encode(s);
	xml.PushAttribute("yootPool", s.c_str());
}

void PeopleSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	for (YootVisitor * v : visitorList) delete v;
	visitorList.clear();
	stays.clear();
	pool = Yoot::Pool();
	lastMinute = -1;
	const char * s = xml.Attribute("yootPool");
	pendingPool = s ? s : "";
	defLoaded = false;
}
