/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower Front Desk (Front.t2p / RFront.t2p), Housekeeping
 * (Maintenance.t2p / RMaintenance.t2p), Parking (Parking.t2p) and the Fire
 * Department (FireDepartment.t2p). game-parity.md W4, E7, I4-I6. */
#include "SpecialSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Hotel.h"
#include "Item/Housekeeping.h"
#include "Item/Item.h"
#include "Item/YootSpecialItems.h"
#include "Logger.h"
#include "Person.h"
#include "YootCatalog.h"
#include "YootRules.h"

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace OT;

namespace
{
	const Game::ItemSet & itemsOf(Game * game, const char * id)
	{
		static const Game::ItemSet none;
		Game::ItemSetByString::const_iterator it = game->itemsByType.find(id);
		return it == game->itemsByType.end() ? none : it->second;
	}

	const char * const kRoomKinds[] = {"hotel_single", "hotel_double", "hotel_suite"};

	bool built(const Item::Item * item) { return item && !item->underConstruction; }

	bool needsDesk(const Item::Item * room)
	{
		return room->prototype->yoot && Yoot::roomNeedsFrontDesk(room->prototype->yoot->dependId);
	}

	int roomType(const Item::Hotel * room)
	{
		if (room->prototype->yoot) {
			const int t = Yoot::guestroomType(room->prototype->yoot->toolType);
			if (t >= 0) return t;
		}
		return std::max(0, std::min(2, room->variant));
	}
}

SpecialSystem::SpecialSystem(Game * game)
: GameObject(game)
{
	reset();
}

void SpecialSystem::reset()
{
	lastMinute = prevMinute = -1;
	lastAbsMinute = -1;
	lastItemCount = 0;
	roomDesk.clear();
	parked.clear();
	trips.clear();
	beingCleaned.clear();
	routeCache.clear();
	hotelIncomePaid = 0;
}

bool SpecialSystem::enabled() const
{
	return game && game->activeRules.profile().yootWorld;
}

void SpecialSystem::advance()
{
	if (!enabled()) return;
	const int minute = std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
	if (minute == lastMinute) return;
	prevMinute = lastMinute;
	lastMinute = minute;
	minuteTick(minute);
	lastAbsMinute = Yoot::absoluteMinute(game->time.absolute);
}

bool SpecialSystem::crossed(int first) const
{
	if (prevMinute < 0) return lastMinute == first;
	if (prevMinute < lastMinute) return prevMinute < first && first <= lastMinute;
	return first > prevMinute || first <= lastMinute; // across midnight
}

bool SpecialSystem::exists(Item::Item * item, const char * kind) const
{
	const Game::ItemSet & set = itemsOf(game, kind);
	return set.find(item) != set.end();
}

bool SpecialSystem::reachable(Item::Item * from, Item::Item * to, bool service)
{
	if (!from || !to) return false;
	if (from->position.y == to->position.y) return true;
	const std::pair<std::pair<Item::Item *, Item::Item *>, bool> key(std::make_pair(from, to), service);
	std::map<std::pair<std::pair<Item::Item *, Item::Item *>, bool>, bool>::const_iterator c = routeCache.find(key);
	if (c != routeCache.end()) return c->second;
	// T2RouteNavigator::CheckRoute within the customer search score (the
	// score cap is not modelled; OpenSky routes are A*-bounded).
	const bool ok = game->routeExists(from, to, service);
	routeCache[key] = ok;
	return ok;
}

void SpecialSystem::purge()
{
	// Drop links to demolished rooms and desks (Front.t2p DestructFinish
	// FUN_10013360 releases the table; the rooms keep +0x94 = 0).
	for (std::map<Item::Item *, Item::FrontDesk *>::iterator it = roomDesk.begin(); it != roomDesk.end();) {
		bool room = false;
		for (const char * kind : kRoomKinds) room = room || exists(it->first, kind);
		if (!room || !exists(it->second, "frontdesk")) roomDesk.erase(it++);
		else ++it;
	}
	for (std::map<Person *, Item::ParkingLot *>::iterator it = parked.begin(); it != parked.end();) {
		if (!exists(it->second, "parkinglot")) parked.erase(it++);
		else ++it;
	}
	for (size_t t = 0; t < trips.size();) {
		Trip & trip = trips[t];
		const bool fromAlive = exists(trip.from, "housekeeping");
		trip.rooms.erase(std::remove_if(trip.rooms.begin(), trip.rooms.end(), [&](Item::Hotel * h) {
			bool alive = false;
			for (const char * kind : kRoomKinds) alive = alive || exists(h, kind);
			const bool done = !alive || h->roomState != Item::Hotel::kDirty;
			if (done) beingCleaned.erase(h);
			if (done && alive && fromAlive) ++trip.from->roomsCleaned;
			return done;
		}), trip.rooms.end());
		if (trip.rooms.empty() || !fromAlive) {
			if (fromAlive) trip.from->maidsOut = std::max(0, trip.from->maidsOut - 1);
			for (Item::Hotel * h : trip.rooms) beingCleaned.erase(h);
			trips.erase(trips.begin() + t);
		} else ++t;
	}
}

void SpecialSystem::minuteTick(int minute)
{
	if (game->items.size() != lastItemCount || minute % 60 == 0) {
		routeCache.clear();
		const bool changed = game->items.size() != lastItemCount;
		lastItemCount = game->items.size();
		purge();
		if (changed || minute % 60 == 0) linkRooms();
	} else {
		purge();
	}
	deskTick(minute);
	housekeepingTick(minute);
	securityTick(minute);
	if (minute % 60 == 0 || prevMinute < 0 || minute / 60 != prevMinute / 60) parkingTick();
}

void SpecialSystem::linkRooms()
{
	// FUN_10013b70 per desk, in list order: unlinked rooms in route range.
	const Game::ItemSet & desks = itemsOf(game, "frontdesk");
	for (Game::ItemSet::const_iterator d = desks.begin(); d != desks.end(); ++d) {
		Item::FrontDesk * desk = dynamic_cast<Item::FrontDesk *>(*d);
		if (!desk || !built(desk)) continue;
		std::vector<Item::Item *> rooms;
		std::vector<Yoot::RoomLink> links;
		for (const char * kind : kRoomKinds) {
			const Game::ItemSet & set = itemsOf(game, kind);
			for (Game::ItemSet::const_iterator r = set.begin(); r != set.end(); ++r) {
				if (!built(*r)) continue;
				Yoot::RoomLink l;
				l.linkedDesk = roomDesk.count(*r) ? 1 : 0;
				l.reachable = l.linkedDesk == 0 && reachable(desk, *r);
				rooms.push_back(*r);
				links.push_back(l);
			}
		}
		Yoot::linkRooms(2, links);
		for (size_t k = 0; k < rooms.size(); ++k)
			if (links[k].linkedDesk == 2) roomDesk[rooms[k]] = desk;
	}
	for (Game::ItemSet::const_iterator d = desks.begin(); d != desks.end(); ++d)
		if (Item::FrontDesk * desk = dynamic_cast<Item::FrontDesk *>(*d)) desk->linkedRooms = 0;
	for (std::map<Item::Item *, Item::FrontDesk *>::const_iterator it = roomDesk.begin(); it != roomDesk.end(); ++it)
		++it->second->linkedRooms;
	LOG(DEBUG, "front desks link %zu hotel rooms", roomDesk.size());
}

void SpecialSystem::securityTick(int minute)
{
	// Securityroom.t2p FUN_1000be00: status 10 hires the 12 TMDF 1 guards
	// (CollectEmployee, retried hourly); the room then runs day (06-17)
	// and night shifts that share the same belong people, swapped at
	// 05:00 and 17:00 (FUN_1000bf90), so the staff walk in once.
	const long long absMinute = Yoot::absoluteMinute(game->time.absolute);
	const Game::ItemSet & set = itemsOf(game, "security");
	for (Game::ItemSet::const_iterator s = set.begin(); s != set.end(); ++s) {
		if (!built(*s)) continue;
		game->eventSystem.people.staffFacility(*s, absMinute, minute, 0, 1440);
	}
}

void SpecialSystem::deskTick(int minute)
{
	const long long absMinute = Yoot::absoluteMinute(game->time.absolute);
	const Game::ItemSet & desks = itemsOf(game, "frontdesk");
	for (Game::ItemSet::const_iterator d = desks.begin(); d != desks.end(); ++d) {
		Item::FrontDesk * desk = dynamic_cast<Item::FrontDesk *>(*d);
		if (!desk || !built(desk)) continue;
		// Front.t2p's ChangeStatusOnIdle (the Maintenance.t2p machine,
		// FUN_100133d0): status 10 hires the 3 TMDF 1 staff; status 0xb
		// calls them in at [open, open+8).
		game->eventSystem.people.staffFacility(desk, absMinute, minute, desk->openMinute(), 8);
		desk->setPattern(desk->isOpen(minute) ? 1 : 0);
		// FUN_10015540 at 12:00 (once: the tenant idles every 8 minutes).
		if (crossed(Yoot::kHotelPayMinute)) {
			for (int type = 0; type < 3; ++type) {
				const int amount = desk->ledger.accrued[type];
				if (amount == 0) continue;
				hotelIncomePaid += amount;
				game->yootBook(amount, "hotel_income", Yoot::kHotelIncomeMessage);
			}
		}
		// FUN_10015450 at 17:00.
		if (crossed(Yoot::kHotelLedgerResetMinute)) desk->ledger.reset17();
	}
}

bool SpecialSystem::roomMayTakeGuests(Item::Hotel * room)
{
	if (!enabled() || !room) return true;
	if (!needsDesk(room)) return true;
	if (roomDesk.empty() && !itemsOf(game, "frontdesk").empty()) linkRooms();
	return deskOf(room) != nullptr;
}

Item::FrontDesk * SpecialSystem::deskOf(Item::Item * room) const
{
	std::map<Item::Item *, Item::FrontDesk *>::const_iterator it = roomDesk.find(room);
	return it == roomDesk.end() ? nullptr : it->second;
}

std::vector<std::pair<std::string, std::string> > SpecialSystem::rateChoices(Item::FrontDesk * desk) const
{
	std::vector<std::pair<std::string, std::string> > out;
	if (!desk) return out;
	// RFront (Kegon) has two tabs, "Small|Big"; Front has "Single|Twin|Suite".
	const bool kegon = game->activeRules.profile().worldId == "kegon";
	static const char * names[3] = {"Single", "Twin", "Suite"};
	static const char * kegonNames[2] = {"Small", "Big"};
	static const char * protos[3] = {"hotel_single", "hotel_double", "hotel_suite"};
	static const char * kegonProtos[2] = {"hotel_single", "hotel_suite"};
	for (int type = 0; type < (kegon ? 2 : 3); ++type) {
		std::map<std::string, Item::AbstractPrototype *>::const_iterator p =
			game->itemFactory.prototypesById.find(kegon ? kegonProtos[type] : protos[type]);
		if (p == game->itemFactory.prototypesById.end() || !p->second->yoot) continue;
		const std::vector<int> & money = p->second->yoot->inMoney;
		for (size_t s = 0; s < money.size(); ++s) {
			const bool current = desk->ledger.selection[type] == (int)s + 1;
			out.push_back(std::make_pair(std::string(current ? "* " : "") + (kegon ? kegonNames[type] : names[type]) +
				" rate: $" + std::to_string(money[s]), std::to_string(type) + ":" + std::to_string(s + 1)));
		}
	}
	return out;
}

bool SpecialSystem::chooseRate(Item::FrontDesk * desk, const std::string & id)
{
	const size_t colon = id.find(':');
	if (!desk || colon == std::string::npos) return false;
	const int type = atoi(id.substr(0, colon).c_str()), sel = atoi(id.substr(colon + 1).c_str());
	if (type < 0 || type > 2 || sel < 1 || sel > 4) return false;
	// T2GuestroomTable::SetInMoneyIndex: rooms with guests keep their rate
	// until the stay ends; new check-ins take the new one.
	desk->ledger.selection[type] = sel;
	return true;
}

int SpecialSystem::roomSelection(const Item::Item * room) const
{
	const Item::Hotel * hotel = dynamic_cast<const Item::Hotel *>(room);
	if (!enabled() || !hotel) return -1;
	Item::FrontDesk * desk = deskOf(const_cast<Item::Item *>(room));
	if (!desk) return -1;
	return std::max(0, desk->ledger.selection[roomType(hotel)] - 1);
}

void SpecialSystem::onCheckIn(Item::Hotel * room)
{
	if (!enabled() || !room || !room->prototype->yoot) return;
	const int income = Yoot::hotelRoomIncome(*room->prototype->yoot, game->yootPriceSelection(room));
	LOG(DEBUG, "%s checked in: $%d booked", room->prototype->name.c_str(), income);
	if (Item::FrontDesk * desk = deskOf(room)) {
		desk->ledger.checkIn(roomType(room), income);
		return;
	}
	// A room that needs no desk (Kegon Small/Large Room) and has none:
	// paid at once (unconfirmed; the original's unlinked flow is untraced).
	hotelIncomePaid += income;
	game->yootBook(income, "hotel_income", "");
}

void SpecialSystem::housekeepingTick(int minute)
{
	const long long absMinute = Yoot::absoluteMinute(game->time.absolute);
	const Game::ItemSet & offices = itemsOf(game, "housekeeping");
	for (Game::ItemSet::const_iterator h = offices.begin(); h != offices.end(); ++h) {
		Item::Housekeeping * hk = dynamic_cast<Item::Housekeeping *>(*h);
		if (!hk || !built(hk)) continue;
		// Maintenance.t2p FUN_100133d0: status 10 hires the 6 TMDF 1
		// maids; status 0xb calls them in at [open, open+8).
		game->eventSystem.people.staffFacility(hk, absMinute, minute, hk->openMinute(), 8);
		const bool working = Yoot::housekeepingDispatchMinute(minute, hk->openMinute(), hk->closeMinute());
		const bool open = minute >= hk->openMinute() && minute < hk->closeMinute();
		hk->setPattern(open ? 1 : 0);
		if (!working) continue;
		// FUN_1000bf70: one idle maid per tenant idle (every 8 minutes).
		const bool idleTick = prevMinute < 0 || minute / Yoot::kTenantIdleMinutes != prevMinute / Yoot::kTenantIdleMinutes;
		if (!idleTick || hk->maidsOut >= hk->maids()) continue;
		std::vector<Item::Hotel *> rooms;
		std::vector<bool> candidates;
		for (const char * kind : kRoomKinds) {
			const Game::ItemSet & set = itemsOf(game, kind);
			for (Game::ItemSet::const_iterator r = set.begin(); r != set.end(); ++r) {
				Item::Hotel * room = dynamic_cast<Item::Hotel *>(*r);
				if (!room || !built(room)) continue;
				rooms.push_back(room);
				// Maid reach: service movers only (Maintenance FUN_1000c010, table 1).
				candidates.push_back(room->roomState == Item::Hotel::kDirty && !beingCleaned.count(room) &&
				                     !room->housekeeper && reachable(hk, room, true));
			}
		}
		const std::vector<int> pick = Yoot::maidTrip(candidates);
		if (pick.empty()) continue;
		Trip trip;
		trip.from = hk;
		for (int k : pick)
			if (rooms[k]->yootSendHousekeeper(hk)) {
				trip.rooms.push_back(rooms[k]);
				beingCleaned.insert(rooms[k]);
			}
		if (trip.rooms.empty()) continue;
		++hk->maidsOut;
		trips.push_back(trip);
	}
}

bool SpecialSystem::driveIn(Person * driver, Item::Item * destination)
{
	if (!enabled() || !driver || !destination) return false;
	if (parked.count(driver)) return true;
	const Game::ItemSet & lots = itemsOf(game, "parkinglot");
	if (lots.empty()) return false; // no parking yet: in through the lobby
	// FindHisParking / FindEmptyParking: the first lot with a free slot
	// (here also reachable from the destination, CheckRouteFromPool bit 4).
	for (Game::ItemSet::const_iterator l = lots.begin(); l != lots.end(); ++l) {
		Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(*l);
		if (!lot || !built(lot) || !lot->model.hasSpace()) continue;
		if (!reachable(lot, destination)) continue;
		if (!lot->park(driver)) continue;
		parked[driver] = lot;
		return true;
	}
	// MoveToTowerEntrance: no parking -> lobby, -100 estimate.
	driver->yootEstimate = Yoot::incEstimate(driver->yootEstimate, -Yoot::kNoParkingEstimate);
	return false;
}

void SpecialSystem::driveOut(Person * driver)
{
	std::map<Person *, Item::ParkingLot *>::iterator it = parked.find(driver);
	if (it == parked.end()) return;
	if (exists(it->second, "parkinglot")) it->second->leave(driver);
	parked.erase(it);
}

void SpecialSystem::parkingTick()
{
	// Parking.t2p idle FUN_1000bd20: gate patterns by position in the stack.
	const int b1 = -1; // a first-floor gate has its bottom row on B1
	const Game::ItemSet & gates = itemsOf(game, "parkinggate");
	for (Game::ItemSet::const_iterator g = gates.begin(); g != gates.end(); ++g) {
		Item::ParkingGate * gate = dynamic_cast<Item::ParkingGate *>(*g);
		if (!gate || !built(gate)) continue;
		bool below = false;
		for (Game::ItemSet::const_iterator o = gates.begin(); o != gates.end(); ++o)
			if (*o != gate && (*o)->position.x == gate->position.x &&
			    (*o)->position.y + (*o)->size.y == gate->position.y) below = true;
		gate->setGatePattern(Yoot::gatePattern(gate->position.y == b1, below));
	}
	// Everyone has driven home by 03:00; clear slots of people deleted in
	// the meantime (drivers are transient).
	if (crossed(180)) {
		const Game::ItemSet & lots = itemsOf(game, "parkinglot");
		for (Game::ItemSet::const_iterator l = lots.begin(); l != lots.end(); ++l)
			if (Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(*l)) {
				lot->model = Yoot::ParkingLotModel();
				lot->drivers.clear();
				lot->setPattern(0);
			}
		parked.clear();
	}
}

std::vector<std::pair<Item::Item *, int> > SpecialSystem::firemen()
{
	std::vector<std::pair<Item::Item *, int> > out;
	std::vector<Item::Item *> sources;
	std::vector<int> present;
	for (const char * kind : {"security", "firedept"}) {
		const Game::ItemSet & set = itemsOf(game, kind);
		for (Game::ItemSet::const_iterator i = set.begin(); i != set.end(); ++i) {
			if (!built(*i)) continue;
			sources.push_back(*i);
			const Item::FireDepartment * fd = dynamic_cast<const Item::FireDepartment *>(*i);
			present.push_back(fd ? fd->firemen() : Yoot::kMaxFiremen);
		}
	}
	const std::vector<int> sent = Yoot::callFiremen(present);
	for (size_t k = 0; k < sources.size(); ++k)
		if (sent[k] > 0) out.push_back(std::make_pair(sources[k], sent[k]));
	return out;
}

std::string SpecialSystem::describe(Item::Item * item)
{
	if (!enabled() || !item) return std::string();
	std::ostringstream s;
	if (Item::FrontDesk * desk = dynamic_cast<Item::FrontDesk *>(item)) {
		s << "\nLinked rooms: " << desk->linkedRooms << "\n";
		s << "Since 17:00:  $" << desk->ledger.noonIncome() << " (paid at 12:00)\n";
	} else if (Item::Housekeeping * hk = dynamic_cast<Item::Housekeeping *>(item)) {
		s << "\nMaids out:   " << hk->maidsOut << "/" << hk->maids() << "\n";
		s << "Rooms made:  " << hk->roomsCleaned << "\n";
	} else if (Item::ParkingLot * lot = dynamic_cast<Item::ParkingLot *>(item)) {
		s << "\nCars:        " << lot->model.cars() << "/" << Yoot::ParkingLotModel::kSlots << "\n";
	} else if (Item::Hotel * room = dynamic_cast<Item::Hotel *>(item)) {
		if (needsDesk(room) && !deskOf(room)) s << "\n" << Yoot::kNoFrontDesk << "\n";
		if (room->roomState == Item::Hotel::kDirty) s << "\n" << Yoot::kBedsNotMade << "\n";
		if (room->roomState == Item::Hotel::kInfested) s << "\n" << Yoot::kRoomTooDirty << "\n";
	}
	return s.str();
}
