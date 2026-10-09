/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower hotel guests (game-parity.md W4, E7): pool people called by the
 * guest rooms, queued and checked in / out at the linked Front Desk.
 *
 * Ports (agent notes, ~/yoottower-decomp/hotel-agent and rules-agent plg):
 *   room ChangeStatusOnIdle   Single 0x100136b0, Twin/Suite 0x1000d0d0,
 *                             Small/Large 0x1000bee0 (statuses 30..35)
 *   guest call                FUN_10012d50 -> T2Tenant::CollectEmployee
 *                             0x1005f5a0 / CollectEmployeeFromPool 0x10060a50
 *   desk queue                Front.t2p FUN_10013570: FUN_10014000 (stress)
 *                             then FUN_10013ce0 twice (check-in / -out)
 *   outing                    FUN_10012e80 / FUN_10012f60
 * Simplifications: the room estimate is JudgeSystem's (UpdateResidencial-
 * Estimate), not the +5/+20 night bonus of an empty room; a guest party that
 * gives up while still walking leaves the tower at once; stays are not
 * saved (a reloaded occupied room is checked out). */
#include "PeopleSystem.h"

#include "Game.h"
#include "Item/Hotel.h"
#include "Item/YootSpecialItems.h"
#include "YootRules.h"

#include <algorithm>
#include <cstdlib>

using namespace OT;

namespace
{
	const char * const kRoomKinds[] = {"hotel_single", "hotel_double", "hotel_suite"};
	/// A queued / staying person has no stay end (T2Tenant::EnterTenant
	/// clears the start time).
	const long long kNoLeave = 100000000LL;
	/// Front.t2p FUN_10014000: +15 stress per desk idle while queued; above
	/// 249 the party gives up with -100 estimate.
	const int kQueueStress = 15, kQueueGiveUp = 249;
	const size_t kMaxGuests = 4000;  // PeopleSystem's visitor guard

	int idlePhaseOf(const Item::Item * i)
	{
		return (int)(((unsigned)(i->position.x * 31 + i->position.y * 17)) % Yoot::kTenantIdleMinutes);
	}

	bool within(int minute, int a, int b) { return minute >= a && minute < b; }
}

bool PeopleSystem::hotelGuests() const
{
	return enabled() && !pool.empty();
}

void PeopleSystem::leaveParty(YootVisitor * v)
{
	for (std::map<Item::Item *, HotelStay>::iterator s = stays.begin(); s != stays.end(); ++s)
	{
		std::vector<YootVisitor *> & p = s->second.party;
		p.erase(std::remove(p.begin(), p.end(), v), p.end());
	}
}

bool PeopleSystem::callGuests(Item::Hotel * room, HotelStay & s, Item::FrontDesk * desk, long long now)
{
	// CollectEmployeeFromPool 0x10060a50 with the room plugin's TMDF 1 (the
	// guests are the room's belong people): econo + priceIndex - 3 in 1..5
	// (0x10 = only the first member checks it), the whole table must be in
	// the pool (T2Pool::Find), then each member is called numOfPeople
	// times. No PLDF: the route score is 1000.
	if (!room->prototype->yoot) return false;
	const std::string & plugin = room->prototype->yoot->plugin;
	std::map<std::string, Table>::iterator ti = tables.find(plugin + "|1");
	if (ti == tables.end())
	{
		Table t;
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		t.valid = p && p->ensureLoaded() && p->getMemberTable(t.table, 1) && !t.table.members.empty();
		ti = tables.insert(std::make_pair(plugin + "|1", t)).first;
	}
	const Table & t = ti->second;
	if (!t.valid || visitorList.size() >= kMaxGuests) return false;
	const unsigned mask = routeMask(room, 1000);
	if (mask == 0) return false;
	const bool onlyFirst = t.table.econoType != 0xFFFFFFFFu && (t.table.econoType & 0x10u);
	int econo = -1;
	if (t.table.econoType != 0xFFFFFFFFu)
	{
		const int price = Yoot::priceIndex(game->yootPriceSelection(room), (int)room->prototype->yoot->inMoney.size());
		econo = std::max(1, std::min(5, int(t.table.econoType & ~0x10u) + price - 3));
	}
	if (!pool.find(t.table, econo, mask, onlyFirst)) return false;
	for (size_t i = 0; i < t.table.members.size(); ++i)
	{
		const MemberDef & m = t.table.members[i];
		for (int n = 0; n < std::max(1, (int)m.numOfPeople); ++n)
		{
			Yoot::PeopleType type;
			// CollectEmployeeFromPool 0x10060a50: T2Pool::Call gets the same econo for every
			// member; only T2Pool::Find (0x10050500) relaxes it from member 1 on.
			if (!pool.call(m, econo, mask, type, [] { return std::rand(); })) break;
			// SetDestination(desk, 0): they set off at once.
			YootVisitor * v = spawn(type, desk, now, 0);
			v->state = Person::kCommuting;
			s.party.push_back(v);
		}
	}
	return !s.party.empty();
}

void PeopleSystem::hotelTick(long long now)
{
	if (!hotelGuests()) return;
	const int minute = (int)(((now % 1440) + 1440) % 1440);
	for (const char * kind : kRoomKinds)
	{
		const Game::ItemSet & set = game->itemsByType[kind];
		const std::vector<Item::Item *> rooms(set.begin(), set.end());
		for (Item::Item * i : rooms)
		{
			Item::Hotel * room = dynamic_cast<Item::Hotel *>(i);
			if (!room || room->underConstruction) continue;
			// Guests called out of the room by a tenant (CalledInTenant) leave.
			for (YootVisitor * v : visitorsIn(room))
				if (v->startMinute <= now) pushOut(v);
			if (minute % Yoot::kTenantIdleMinutes == idlePhaseOf(room)) roomIdle(room, minute, now);
		}
	}
	const Game::ItemSet & desks = game->itemsByType["frontdesk"];
	const std::vector<Item::Item *> list(desks.begin(), desks.end());
	for (Item::Item * i : list)
	{
		Item::FrontDesk * desk = dynamic_cast<Item::FrontDesk *>(i);
		if (!desk || desk->underConstruction) continue;
		if (!desk->isOpen(minute))
		{
			// [close, close + 8): PushOutAll.
			for (YootVisitor * v : visitorsIn(desk)) pushOut(v);
			desk->leaveAt.clear();
			continue;
		}
		if (minute % Yoot::kTenantIdleMinutes == idlePhaseOf(desk)) deskIdle(desk, now);
		desk->setPattern(desk->leaveAt.empty() ? 1 : desk->users() < 10 ? 2 : 3);
	}
}

void PeopleSystem::roomIdle(Item::Hotel * room, int minute, long long now)
{
	std::map<Item::Item *, HotelStay>::iterator it = stays.find(room);
	const int open = room->prototype->yoot && room->prototype->yoot->openMinute > 0 ? room->prototype->yoot->openMinute : 1020;
	if (it == stays.end())
	{
		// The saved stay of a reloaded room is consumed here, once.
		const int saved = room->pendingStayStatus;
		const std::string savedParty = room->pendingStayParty;
		room->pendingStayStatus = 0;
		room->pendingStayParty.clear();
		// Status 30. A reloaded room that was let is checked out, unless its
		// stay was saved (yootStayStatus 32..34): the guests stay until checkout.
		if (room->roomState == Item::Hotel::kOccupied)
		{
			std::vector<Yoot::PeopleType> party;
			if (Yoot::decodeStay(saved, savedParty, party))
			{
				restoreStay(room, saved, party, now);
				return;
			}
			room->roomState = Item::Hotel::kDirty;
			room->dirtySince = game->time.absolute;
			room->spriteNeedsUpdate = true;
			return;
		}
		if (room->roomState != Item::Hotel::kClean || room->lobbyRoute.empty()) return;
		// 30 -> 31: estimate > 200, 17:00..22:00, one in 30, and an open
		// linked desk (FUN_10012d50).
		if (room->yootEstimate != 0 && room->yootEstimate <= 200) return;
		if (!within(minute, open, open + 300) || std::rand() % 30 != 0) return;
		Item::FrontDesk * desk = game->eventSystem.specials.deskOf(room);
		if (!desk || !desk->isOpen(minute)) return;
		HotelStay s;
		s.desk = desk;
		if (!callGuests(room, s, desk, now)) return;
		s.status = 31;
		room->yootStayBooked = false;
		stays[room] = s;
		return;
	}
	HotelStay & s = it->second;
	const int hour = minute / 60;
	switch (s.status)
	{
	case 31:
		if (s.party.empty()) stays.erase(it);
		return;
	case 32:
		if (within(minute, 1320, 1328) && !roomOuting[room])
		{
			// FUN_10012f60 at 22:00: stale returns to the room are dropped.
			for (YootVisitor * v : s.party)
				for (size_t k = 0; k < v->returns.size();)
					if (v->returns[k].first == room) v->returns.erase(v->returns.begin() + k);
					else ++k;
			roomOuting[room] = true;
		}
		else if (within(minute, 1020, 1260) && roomOuting[room])
		{
			// FUN_10012e80: everyone in, they may go out to eat and shop
			// (special flag 1, back by 22:00).
			const std::vector<YootVisitor *> in = visitorsIn(room);
			if (!s.party.empty() && in.size() == s.party.size())
			{
				roomOuting[room] = false;
				const long long at = now - minute + 1320;
				for (YootVisitor * v : s.party)
				{
					v->yootFlags |= 1;
					if (v->returns.size() < 3) v->returns.push_back(std::make_pair((Item::Item *)room, at));
				}
			}
		}
		if ((hour == 23 && std::rand() % 5 == 0) || within(minute, 60, 68)) s.status = 33;
		break;
	case 33:
		if ((within(minute, 360, 600) && std::rand() % 20 == 0) || hour == 10) s.status = 34;
		break;
	case 34:
		if (hour == 11 || std::rand() % 20 == 0)
		{
			// PushOutAll: the guests take their return to the desk to check
			// out (FUN_10013ce0 step 3).
			s.status = 35;
			for (YootVisitor * v : visitorsIn(room)) pushOut(v);
			room->roomState = Item::Hotel::kDirty;
			room->dirtySince = game->time.absolute;
		}
		break;
	case 35:
		// Housekeeping makes the bed (Hotel's maid): back to 30.
		if (room->roomState == Item::Hotel::kClean) stays.erase(it);
		return;
	}
	room->spriteNeedsUpdate = true;
}

void PeopleSystem::roomEnter(Item::Hotel * room, YootVisitor * v)
{
	std::map<Item::Item *, HotelStay>::iterator it = stays.find(room);
	if (v->dest != room) return;
	const bool ours = it != stays.end() && std::find(it->second.party.begin(), it->second.party.end(), v) != it->second.party.end();
	if (!ours || it->second.status >= 35)
	{
		arrived(v, room, false, -1);
		return;
	}
	arrived(v, room, true, kNoLeave);
	v->stayEnd = -1;
	v->state = Person::kHome;
	if (it->second.status == 31) it->second.status = 32;
	room->roomState = Item::Hotel::kOccupied;
	room->spriteNeedsUpdate = true;
}

void PeopleSystem::deskEnter(Item::FrontDesk * desk, YootVisitor * v)
{
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	if (v->dest && v->dest != desk)
	{
		// Checked in by a party member on the way: on to the room.
		walkTo(v, desk, v->dest, now);
		return;
	}
	const int minute = (int)(((now % 1440) + 1440) % 1440);
	if (!v->dest || !desk->isOpen(minute))
	{
		// Closed: Return and -100 (base EnterTenant); the party loses a guest.
		v->dest = desk;
		arrived(v, desk, false, -1);
		return;
	}
	arrived(v, desk, true, kNoLeave);
	v->stayEnd = kNoLeave + (long long)desk->leaveAt.size();
	desk->leaveAt.push_back(v->stayEnd);
}

void PeopleSystem::deskIdle(Item::FrontDesk * desk, long long now)
{
	std::vector<YootVisitor *> queue = visitorsIn(desk);
	if (queue.empty()) return;
	std::stable_sort(queue.begin(), queue.end(), [](const YootVisitor * a, const YootVisitor * b) { return a->stayEnd < b->stayEnd; });
	auto leave = [&](YootVisitor * v) {
		std::vector<long long>::iterator e = std::find(desk->leaveAt.begin(), desk->leaveAt.end(), v->stayEnd);
		if (e != desk->leaveAt.end()) desk->leaveAt.erase(e);
		queue.erase(std::remove(queue.begin(), queue.end(), v), queue.end());
		pushOut(v);
	};
	auto roomOf = [&](YootVisitor * v) -> Item::Item * {
		for (std::map<Item::Item *, HotelStay>::iterator s = stays.begin(); s != stays.end(); ++s)
			if (s->second.desk == desk && std::find(s->second.party.begin(), s->second.party.end(), v) != s->second.party.end())
				return s->first;
		return nullptr;
	};

	// FUN_10014000: the waiting get stressed; above 249 the party gives up.
	const std::vector<YootVisitor *> waiting = queue;
	for (YootVisitor * v : waiting)
	{
		if (v->dest || std::find(queue.begin(), queue.end(), v) == queue.end()) continue;
		v->addStress(kQueueStress);
		if (v->stress <= kQueueGiveUp) continue;
		Item::Item * room = roomOf(v);
		if (!room)
		{
			v->yootEstimate = Yoot::incEstimate(v->yootEstimate, -100);
			leave(v);
			continue;
		}
		HotelStay & s = stays[room];
		const std::vector<YootVisitor *> party = s.party;
		s.party.clear();
		for (YootVisitor * p : party)
		{
			p->yootEstimate = Yoot::incEstimate(p->yootEstimate, -100);
			p->returns.clear();
			p->dest = nullptr;
			if (std::find(queue.begin(), queue.end(), p) != queue.end()) leave(p);
			else if (p->phase == YootVisitor::kInTenant && p->inside) pushOut(p);
			else { p->journey.clear(); goOutTower(p); }
		}
		if (s.status == 31) stays.erase(room);
	}

	// FUN_10013ce0 twice: one check-in or check-out each.
	for (int pass = 0; pass < 2 && !queue.empty(); ++pass)
	{
		while (!queue.empty() && queue.front()->dest) leave(queue.front());
		if (queue.empty()) break;
		YootVisitor * v = queue.front();
		Item::Item * room = roomOf(v);
		if (!room) { leave(v); continue; }
		HotelStay & s = stays[room];
		if (s.status >= 35)
		{
			// Check-out: the guest goes home; the party is released.
			s.party.erase(std::remove(s.party.begin(), s.party.end(), v), s.party.end());
			leave(v);
			continue;
		}
		// Check-in: the party heads for the room and comes back here to
		// check out; the room's inMoney is booked on the desk (FUN_10015420).
		for (YootVisitor * p : s.party)
		{
			p->dest = room;
			p->startMinute = now;
			p->goingTo = room->prototype->name;
			if (p->returns.size() < 3) p->returns.push_back(std::make_pair((Item::Item *)desk, now));
		}
		Item::Hotel * h = dynamic_cast<Item::Hotel *>(room);
		if (h && !h->yootStayBooked)
		{
			h->yootStayBooked = true;
			game->eventSystem.specials.onCheckIn(h);
		}
	}
}

void PeopleSystem::roomLook(const Item::Hotel * room, int & in, int & asleep, int & up) const
{
	std::map<Item::Item *, HotelStay>::const_iterator it = stays.find(const_cast<Item::Hotel *>(room));
	if (it == stays.end()) return;
	const int n = (int)visitorsIn(const_cast<Item::Hotel *>(room)).size();
	in += n;
	if (it->second.status == 33) asleep += n;
	else if (it->second.status == 34) up += n;
}

bool PeopleSystem::liveStay(const Item::Hotel * room, int & status, std::vector<Yoot::PeopleType> & party) const
{
	std::map<Item::Item *, HotelStay>::const_iterator it = stays.find(const_cast<Item::Hotel *>(room));
	if (it == stays.end() || it->second.status < 32 || it->second.status > 34) return false;
	party.clear();
	for (const YootVisitor * v : it->second.party)
		if (alive(v)) party.push_back(v->peopleType);
	if (party.empty()) return false;
	status = it->second.status;
	return true;
}

void PeopleSystem::restoreStay(Item::Hotel * room, int status, const std::vector<Yoot::PeopleType> & party, long long now)
{
	// The saved party is back inside the room, as roomEnter leaves it: the
	// stay runs on from its saved status to the normal checkout (PushOutAll).
	HotelStay s;
	s.status = status;
	s.desk = game->eventSystem.specials.deskOf(room);
	for (const Yoot::PeopleType & t : party)
	{
		YootVisitor * v = spawn(t, room, now, 0);
		arrived(v, room, true, kNoLeave);
		v->stayEnd = -1;
		v->state = Person::kHome;
		s.party.push_back(v);
	}
	stays[room] = s;
	room->roomState = Item::Hotel::kOccupied;
	room->spriteNeedsUpdate = true;
}
