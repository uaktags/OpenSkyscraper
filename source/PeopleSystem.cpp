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
	calledToday = enteredToday = 0;
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
		if (minute == 0)
		{
			int phases[5] = {0, 0, 0, 0, 0};
			for (const YootVisitor * v : visitorList) phases[v->phase]++;
			game->recordSimEvent("pool: called " + std::to_string(calledToday) + " entered " + std::to_string(enteredToday) +
				" population " + std::to_string(pool.population) + " visitors " + std::to_string(visitorList.size()) +
				" (pool " + std::to_string(phases[0]) + " walking " + std::to_string(phases[1] + phases[2]) +
				" inside " + std::to_string(phases[3]) + " leaving " + std::to_string(phases[4]) + ")");
			calledToday = enteredToday = 0;
		}
	}
	// T2.exe FUN_0041da20: T2Pool::GameLevelChanged on every grade change.
	const int grade = game->rating + 1;
	if (grade != pool.gameLevel) pool.gameLevelChanged(def, grade);

	const std::vector<YootVisitor *> list = visitorList;
	for (YootVisitor * v : list)
		if (std::find(visitorList.begin(), visitorList.end(), v) != visitorList.end())
			visitorTick(v, m);
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
		in.trainAvailable = s->trainPresent;
		break;
	}
	const Game::ItemSet & decks = game->itemsByType["boatdeck"];
	for (Item::Item * i : decks)
	{
		Item::YootVenue * d = dynamic_cast<Item::YootVenue *>(i);
		if (!d || d->underConstruction || !d->isOpen()) continue;
		in.ship = reachableWithin(d, tenant, score);
		in.cruiseAvailable = game->eventSystem.venues.cruiseDocked();
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

int PeopleSystem::callFromPool(Item::YootTenant * tenant, const YootTenantDef & def, int free, long long now)
{
	if (visitorList.size() >= kMaxVisitors) return 0;
	const unsigned mask = routeMask(tenant, Yoot::customerScore(def, 0));
	if (mask == 0) return 0;
	const int minute = (int)(((now % 1440) + 1440) % 1440);
	const unsigned flag = def.customers.specialFlag;
	return Yoot::collectFromPool(def, pool, mask, free, minute, rngInt,
		[&](const Yoot::PeopleType & t, int start) { spawn(t, tenant, now + (start - minute), flag); });
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

int PeopleSystem::callFromTenant(Item::YootTenant * tenant, const YootTenantDef & def, int free, long long now)
{
	// T2RouteNavigator::SelectNearTenant 0x10055170: a random tenant with
	// people inside within the PLDF tenant score of the entrance.
	const int score = Yoot::customerScore(def, 1);
	std::vector<Item::Item *> near;
	for (Item::Item * i : game->items)
	{
		if (i == tenant || i->underConstruction || !dynamic_cast<Item::YootTenant *>(i)) continue;
		if (visitorsInside(i).empty()) continue;
		if (reachableWithin(i, tenant, score)) near.push_back(i);
	}
	if (near.empty()) return 0;
	Item::Item * from = near[std::rand() % near.size()];
	const int econo = Yoot::customerEconoType(def);
	const unsigned flag = def.customers.specialFlag;
	return forEachMember(def, free, [&](const MemberDef & m, int count) {
		int got = 0;
		for (YootVisitor * v : visitorsInside(from))
		{
			if (got >= count) break;
			// T2People::CalledInTenant 0x10049550.
			if (((unsigned)v->yootFlags & flag) != flag) continue;
			if (v->freeTime(now) <= def.bindMinutes || v->prevTenant == tenant || v->dest) continue;
			if (!Yoot::checkWithDemand(v->peopleType, m, econo)) continue;
			v->dest = tenant;
			v->startMinute = now;
			v->goingTo = tenant->prototype->name;
			++got;
		}
		return got;
	});
}

int PeopleSystem::callFromFloor(Item::YootTenant * tenant, const YootTenantDef & def, int free, long long now)
{
	// T2Tenant::CollectCustomerFromFloor 0x1005f250 / StopOnHisWay
	// 0x100494b0: people passing on the tenant's floor within the score
	// (negative: without the free-time check) turn in, pushing their
	// destination onto the return stack.
	int score = Yoot::customerScore(def, 2);
	const bool checkFree = score >= 0;
	score = std::abs(score);
	const int floor = tenant->position.y;
	const int left = tenant->position.x, right = tenant->position.x + tenant->size.x;
	std::vector<YootVisitor *> passing;
	for (YootVisitor * v : visitorList)
	{
		if ((v->phase != YootVisitor::kWalking && v->phase != YootVisitor::kLeaving) || !v->at) continue;
		// People waiting in a mover queue on the floor (not riding a car).
		bool onFloor;
		if (v->at->isElevator())
		{
			onFloor = false;
			if (v->journey.fromFloor == floor)
				for (Item::Elevator::Queue * q : static_cast<Item::Elevator::Elevator *>(v->at)->queues)
					if (std::find(q->people.begin(), q->people.end(), v) != q->people.end()) onFloor = true;
		}
		else
			onFloor = v->at->position.y <= floor && floor < v->at->position.y + v->at->size.y;
		if (!onFloor) continue;
		const int x = centerX(v->at);
		if (left - score < x && x < score + right) passing.push_back(v);
	}
	if (passing.empty()) return 0;
	const int econo = Yoot::customerEconoType(def);
	const unsigned flag = def.customers.specialFlag;
	return forEachMember(def, free, [&](const MemberDef & m, int count) {
		int got = 0;
		for (YootVisitor * v : passing)
		{
			if (got >= count) break;
			if (v->phase == YootVisitor::kStopping) continue;
			if (((unsigned)v->yootFlags & flag) != flag) continue;
			if (checkFree && v->freeTime(now) <= def.bindMinutes) continue;
			if (v->prevTenant == tenant || v->dest == tenant) continue;
			if (!Yoot::checkWithDemand(v->peopleType, m, econo)) continue;
			if (v->returns.size() >= 3) continue;  // SetDestinationToReturn failed
			v->returns.push_back(std::make_pair(v->dest, v->startMinute));
			v->dest = tenant;
			v->startMinute = now;
			v->goingTo = tenant->prototype->name;
			if (v->at) v->at->removePerson(v);
			v->journey.clear();
			v->phase = YootVisitor::kStopping;
			v->arriveMinute = now + 1;
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
		for (Item::Item * i : game->itemsByType["subway"])
		{
			Item::Subway * s = dynamic_cast<Item::Subway *>(i);
			if (s && s->trainPresent) { e = s; break; }
		}
		break;
	case Yoot::kTransportShip:
		if (game->eventSystem.venues.cruiseDocked())
			for (Item::Item * i : game->itemsByType["boatdeck"]) { e = i; break; }
		break;
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
	if (!from || !v->dest) { goOutTower(v); return; }
	if (from != game->mainLobby) game->eventSystem.facilities.onEnterTower(v);
	walkTo(v, from, v->dest, now);
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
		// A walk along the floor (OpenSky has no walking positions).
		if (v->at) v->at->removePerson(v);
		if (from->isLobby() && v->dest) game->eventSystem.facilities.onEnterTower(v);
		if (v->phase == YootVisitor::kLeaving) { goOutTower(v); return; }
		v->phase = YootVisitor::kStopping;
		v->arriveMinute = now + std::max(1, std::abs(centerX(from) - centerX(to)) / 16);
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
	switch (v->phase)
	{
	case YootVisitor::kInPool:
		if (now >= v->startMinute) enterTower(v, now);
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
		v->dest = nullptr;
		v->startMinute = leaveMinute;
		v->noRouteTries = 0;
		v->yootFinishTrip();
		++enteredToday;
		return;
	}
	// Not open: T2People::Return and -100 estimate.
	v->yootEstimate = Yoot::incEstimate(v->yootEstimate, -100);
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
	for (Item::Item * i : game->itemsByType["parkinglot"])
	{
		Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(i);
		if (lot) lot->leave(v);
	}
	if (v->yootFlags & Yoot::kPeopleNeedsToilet) v->yootEstimate = Yoot::incEstimate(v->yootEstimate, -30);
	v->yootFinishTrip();
	pool.enter(v->peopleType, v->yootEstimate, rngInt);
	destroy(v);
}

void PeopleSystem::destroy(YootVisitor * v)
{
	std::vector<YootVisitor *>::iterator it = std::find(visitorList.begin(), visitorList.end(), v);
	if (it != visitorList.end()) visitorList.erase(it);
	delete v;
}

void PeopleSystem::itemRemoved(Item::Item * item)
{
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
		const bool bound = v->dest == item || v->inside == item || v->at == item;
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
	pool = Yoot::Pool();
	lastMinute = -1;
	const char * s = xml.Attribute("yootPool");
	pendingPool = s ? s : "";
	defLoaded = false;
}
