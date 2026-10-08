/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower venues (Theater, ClassicTheater, EventCenter, Billboard,
 * Observatory, Ticket booth, Gathering Area, Boat Deck), final items
 * (Stadium/baseball, Cathedral/wedding, Attraction renovation), the subway
 * timetable and the clouds. game-parity.md S1, S2, R4, V6, I7. */
#include "VenueSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "Item/YootVenueItems.h"
#include "Logger.h"
#include "PluginManager.h"
#include "YootCatalog.h"
#include "YootFacilities.h"
#include "YootRules.h"
#include "YootItemArt.h"
#include "YootWorld.h"
#include "YootWorldArt.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

	bool built(const Item::Item * item) { return item && !item->underConstruction; }

	template <class T> T * firstBuilt(Game * game, const char * id)
	{
		const Game::ItemSet & s = itemsOf(game, id);
		for (Game::ItemSet::const_iterator i = s.begin(); i != s.end(); ++i)
			if (built(*i)) if (T * t = dynamic_cast<T *>(*i)) return t;
		return nullptr;
	}

	int capacityOf(const Item::Item * item, int fallback)
	{
		return item->prototype->yoot && item->prototype->yoot->capacity > 0 ? item->prototype->yoot->capacity : fallback;
	}

	int stayOf(const Item::Item * item, int fallback)
	{
		return item->prototype->yoot && item->prototype->yoot->bindMinutes > 0 ? item->prototype->yoot->bindMinutes : fallback;
	}

	int outMoneyUnits(const Item::Item * item, int fallback)
	{
		return item->prototype->yoot ? item->prototype->yoot->outMoney / kYootMoneyUnit : fallback;
	}

	bool startsWith(const std::string & s, const char * p) { return s.compare(0, strlen(p), p) == 0; }

	/// Pushes out the visitors that are due (leaveAt <= now).
	void expire(Item::YootFacility * f, long long now)
	{
		for (size_t k = 0; k < f->leaveAt.size();)
			if (f->leaveAt[k] <= now) f->leaveAt.erase(f->leaveAt.begin() + k);
			else ++k;
	}
}

VenueSystem::VenueSystem(Game * game)
: GameObject(game)
{
	reset();
}

void VenueSystem::reset()
{
	lastMinute = -1;
	lastDay = INT_MIN;
	moviesLoaded = false;
	movieList.clear();
	dayParams.clear();
	hallCooldown.clear();
	hallValue.clear();
	movieBaseDay = -1;
	baseballDone = weddingDone = false;
	baseball = WorldEvent();
	wedding = WorldEvent();
	clouds.clear();
	cloudImages.clear();
	cloudsInit = false;
	lastCloudMinute = -1;
	cruiseInit = false;
	cruiseTimes.clear();
}

bool VenueSystem::enabled() const
{
	return game && game->activeRules.profile().yootWorld;
}

int VenueSystem::grade() const { return game->rating + 1; }

int VenueSystem::dayOfYear() const { return game->time.dayOfYear(); }

bool VenueSystem::rainy() const
{
	return game->activeRules.profile().worldId == "tokyo" && Time::yootTokyoIsRainy((unsigned)dayOfYear());
}

int VenueSystem::percentFor(const Item::Item * item)
{
	const std::string plugin = item->prototype->yoot ? item->prototype->yoot->plugin : std::string();
	if (plugin.empty()) return 100;
	std::map<std::string, DayParamDef>::iterator it = dayParams.find(plugin);
	if (it == dayParams.end())
	{
		DayParamDef d;
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		if (!p || !p->ensureLoaded() || !p->getDayParamDef(d, 128))
			d.percent[0][0] = d.percent[0][1] = d.percent[1][0] = d.percent[1][1] = 100;
		it = dayParams.insert(std::make_pair(plugin, d)).first;
	}
	return (int)it->second.percent[game->time.isHoliday() ? 1 : 0][rainy() ? 1 : 0];
}

bool VenueSystem::reachable(Item::Item * item) const
{
	// Visitors come from the street: a venue with no lobby route draws nobody.
	return item->position.y == 0 || !item->lobbyRoute.empty();
}

void VenueSystem::pay(Item::Item * item, int dollars, const char * message)
{
	(void)item;
	if (dollars == 0) return;
	game->transferFunds(dollars, "entertainment_income", message ? message : "");
}

// ---------------------------------------------------------------- movies

const std::vector<VenueSystem::Movie> & VenueSystem::movies()
{
	if (moviesLoaded) return movieList;
	moviesLoaded = true;
	// doc+0x128: every MV_* sub-plugin the game found.
	for (const std::string & name : game->app.pluginManager.enumerate())
	{
		if (!startsWith(name, "MV_")) continue;
		AbstractPlugin * p = game->app.pluginManager.get(name);
		Movie m;
		if (!p || !p->ensureLoaded() || !p->getMovieDef(m.def)) continue;
		m.plugin = name;
		movieList.push_back(m);
	}
	return movieList;
}

VenueSystem::Movie * VenueSystem::movie(const std::string & plugin)
{
	movies();
	for (Movie & m : movieList)
		if (m.plugin == plugin) return &m;
	return nullptr;
}

// ------------------------------------------------------------------ tick

void VenueSystem::advance()
{
	if (!enabled()) return;
	const int minute = std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
	if (minute == lastMinute) return;
	lastMinute = minute;
	minuteTick(minute);
}

void VenueSystem::newDay(int day)
{
	// Theater.t2p 0x1000e7b0 at the first idle after midnight: ChkReMake for
	// every movie with the days since the first theater opened.
	if (movieBaseDay >= 0)
		for (Movie & m : movieList)
			Yoot::movieChkReMake(m.def, m.state, day - movieBaseDay);

	// EventCenter.t2p FUN_1000dcb0: cooldowns, the last day's verdict, and
	// today's event.
	for (std::map<std::string, int>::iterator c = hallCooldown.begin(); c != hallCooldown.end(); ++c)
		if (c->second > 0) c->second--;
	const Game::ItemSet & halls = itemsOf(game, "eventcenter");
	for (Game::ItemSet::const_iterator i = halls.begin(); i != halls.end(); ++i)
	{
		Item::EventCenter * e = dynamic_cast<Item::EventCenter *>(*i);
		if (!e) continue;
		if (!e->today.empty() && !(e->runsOn(day) && e->event == e->today))
		{
			HallEventDef def;
			AbstractPlugin * p = game->app.pluginManager.get(e->today);
			if (p && p->ensureLoaded() && p->getHallEventDef(def))
			{
				const bool flop = Yoot::hallEventFlop(def, e->visitorsToday);
				hallValue[e->today] = Yoot::hallEventNextValue(def, e->visitorsToday);
				if (flop)
				{
					hallCooldown[e->today] = Yoot::kHallEventCooldownDays;
					if (e->event == e->today) { e->event.clear(); e->span = 0; }
					LOG(IMPORTANT, "Event Center: %s flopped (%d visitors)", def.header.title.c_str(), e->visitorsToday);
				}
			}
		}
		e->today.clear();
		if (e->runsOn(day) && hallCooldown[e->event] == 0) e->today = e->event;
		if (!e->event.empty() && day >= e->startDay + e->span) { e->event.clear(); e->span = 0; }
	}

	// Billboard.t2p FUN_1000bd20: one campaign day per new day.
	const Game::ItemSet & boards = itemsOf(game, "billboard");
	for (Game::ItemSet::const_iterator i = boards.begin(); i != boards.end(); ++i)
	{
		Item::Billboard * b = dynamic_cast<Item::Billboard *>(*i);
		if (!b || b->lastDay == day) continue;
		b->lastDay = day;
		if (b->daysLeft > 0 && --b->daysLeft == 0) b->showAd("", 0);
	}
}

void VenueSystem::minuteTick(int minute)
{
	const int day = (int)std::floor(game->time.absolute);
	movies();
	if (day != lastDay)
	{
		if (lastDay != INT_MIN) newDay(day);
		lastDay = day;
	}
	const long long now = Yoot::absoluteMinute(game->time.absolute);

	static const char * theaters[] = {"theater", "classictheater"};
	for (const char * kind : theaters)
	{
		const Game::ItemSet & s = itemsOf(game, kind);
		for (Game::ItemSet::const_iterator i = s.begin(); i != s.end(); ++i)
		{
			Item::Theater * t = dynamic_cast<Item::Theater *>(*i);
			if (!t || !built(t)) continue;
			// Theater.t2p BuildFinish 0x1000d140: the tower's movie clock
			// starts with its first theater.
			if (movieBaseDay < 0) movieBaseDay = day;
			theaterTick(t, minute, day);
		}
	}
	{
		const Game::ItemSet & s = itemsOf(game, "eventcenter");
		for (Game::ItemSet::const_iterator i = s.begin(); i != s.end(); ++i)
			if (Item::EventCenter * e = dynamic_cast<Item::EventCenter *>(*i))
				if (built(e)) { expire(e, now); eventCenterTick(e, minute, day); }
	}
	static const char * generic[] = {"stadium", "yootcathedral", "attraction", "observatory", "ticketbooth", "gatheringarea", "boatdeck"};
	for (const char * kind : generic)
	{
		const Game::ItemSet & s = itemsOf(game, kind);
		for (Game::ItemSet::const_iterator i = s.begin(); i != s.end(); ++i)
			if (Item::YootVenue * v = dynamic_cast<Item::YootVenue *>(*i))
				if (built(v)) { expire(v, now); genericTick(v, minute); }
	}
	subwayTick(minute);
	cruiseTick(minute);
	if (baseball.active) baseballTick();
	cloudTick(now);
}

void VenueSystem::collect(Item::YootVenue * v, int minute, int stay)
{
	(void)minute;
	if (!reachable(v)) return;
	const int cap = v->capacity();
	// The arrivals are people of the visitor pool (T2Pool::Call).
	const int n = game->eventSystem.people.drawVisitors(v, Yoot::venueArrivals(cap, v->users(), percentFor(v), stay, v->arrivalCarry));
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	for (int k = 0; k < n; ++k) v->leaveAt.push_back(now + std::max(1, stay));
	v->visitorsToday += n;
}

// --------------------------------------------------------------- theaters

void VenueSystem::theaterTick(Item::Theater * t, int minute, int day)
{
	const bool classic = t->classic();
	Movie * m = movie(t->movie);
	const int open = t->openMinute(), close = t->closeMinute();
	bool valid = m != nullptr;
	if (m && !classic)
	{
		// Theater.t2p 0x1000b930: the roadshow ends with its phase-1 run.
		const int phase = Yoot::movieLifePhase(m->def, m->state, day, std::max(0, movieBaseDay));
		if (phase != 1) t->expired = true;
		valid = !t->expired;
	}
	const int running = m && m->def.runningTime > 0 ? m->def.runningTime : 120;
	const Yoot::TheaterShow sh = valid ? Yoot::theaterShowAt(minute, open, close, running) : Yoot::TheaterShow();
	const int prev = t->status;
	const int cap = t->capacity();

	if (sh.status != Yoot::TheaterShow::Closed && t->lastOpenDay != day)
	{
		// 0x1000e640 at the day's first door opening.
		t->lastOpenDay = day;
		t->visitorsToday = 0;
		t->audienceTarget = Yoot::theaterAudienceTarget(t->audienceTarget, t->showBest,
			classic ? m->state.bestF : m->state.bestR, m->def.clipSeconds, cap, m->def.unk104);
		t->yesterdayBest = t->showBest;
		t->showBest = 0;
		t->showCount++;
	}
	// Show end: record the house, push everyone out.
	if (prev == Yoot::TheaterShow::Playing && sh.status != Yoot::TheaterShow::Playing)
	{
		t->showBest = std::max(t->showBest, t->users());
		t->leaveAt.clear();
	}
	// Customers are only collected until close - bindTime - 60 (Theater
	// 21:00, Classic 19:00: its last show draws nobody; oracle-confirmed).
	if (sh.status != Yoot::TheaterShow::Closed && reachable(t) && Yoot::theaterCollects(minute, open, close, stayOf(t, 0)))
	{
		// CollectCustomer(target - inside), spread over the door window.
		const int want = std::min(cap, std::max(0, t->audienceTarget)) - t->users();
		if (want > 0)
		{
			const int start = open + 30 + sh.show * (running + 30);
			const int left = std::max(1, start - minute);
			const int n = sh.status == Yoot::TheaterShow::Doors ? (want + left - 1) / left : want;
			const long long now = Yoot::absoluteMinute(game->time.absolute);
			for (int k = 0; k < n; ++k) t->leaveAt.push_back(now + 30 + running);
			t->visitorsToday += n;
		}
	}
	t->status = sh.status;
	t->setPattern(sh.status == Yoot::TheaterShow::Closed ? 0
		: sh.status == Yoot::TheaterShow::Playing ? 3
		: (t->users() >= cap / 2 ? 2 : 1));
	t->updateSoundtrack(sh.status == Yoot::TheaterShow::Playing);

	// [close, close+8): the day's books (DoPayTool(-CalcInMoney)) and the
	// record for ChkReMake (SetPersons at the day's end).
	if (minute >= close && t->settledDay != day && t->lastOpenDay == day)
	{
		t->settledDay = day;
		t->leaveAt.clear();
		if (m)
		{
			int & pending = classic ? m->state.pendingF : m->state.pendingR;
			const int record = classic ? m->state.bestF : m->state.bestR;
			if (t->showBest > record && t->showBest > pending) pending = t->showBest;
		}
		if (t->prototype->yoot)
			pay(t, Yoot::commercialIncome(t->visitorsToday, *t->prototype->yoot), "Income from Movie Theater just came in.");
	}
}

// ------------------------------------------------------------ event center

void VenueSystem::eventCenterTick(Item::EventCenter * e, int minute, int day)
{
	HallEventDef def;
	bool hasEvent = false;
	if (!e->today.empty())
	{
		AbstractPlugin * p = game->app.pluginManager.get(e->today);
		hasEvent = p && p->ensureLoaded() && p->getHallEventDef(def);
	}
	const int tOpen = e->openMinute(), tClose = e->closeMinute();
	const int open = hasEvent ? (int)def.openTime : tOpen;
	const int close = hasEvent ? (int)def.closeTime : tClose;
	const int cap = e->capacity();

	if (hasEvent && minute >= open - 60 && minute < close)
	{
		if (e->status == 10 && e->lastOpenDay != day)
		{
			// FUN_1000dbb0 10 -> 30: reset the counters, target = catalog value.
			e->lastOpenDay = day;
			e->visitorsToday = 0;
			std::map<std::string, int>::const_iterator v = hallValue.find(e->today);
			e->target = v != hallValue.end() ? v->second : def.unkDC;
		}
		if (e->lastOpenDay == day) e->status = 30;
		if (e->status >= 30 && reachable(e) && Yoot::hallEventCollects(minute, tOpen, tClose, 0))
		{
			const int want = std::min(cap, e->target) - e->users();
			const int n = std::max(0, std::min(want, std::max(1, want / 10)));
			const long long now = Yoot::absoluteMinute(game->time.absolute);
			for (int k = 0; k < n; ++k) e->leaveAt.push_back(now + std::max(1, def.bindTime));
			e->visitorsToday += n;
		}
	}
	else if (e->status >= 30)
	{
		e->status = 10;
		e->leaveAt.clear();
	}
	e->setPattern(e->status < 30 ? 0 : e->users() == 0 ? 1 : (e->users() >= cap / 2 ? 3 : 2));

	// [close, close+8): funds += cost - outMoney (an idle hall pays its outMoney).
	if (minute >= close && e->settledDay != day)
	{
		e->settledDay = day;
		const int dollars = Yoot::hallEventSettlement(hasEvent ? def.cost : 0, outMoneyUnits(e, 100));
		pay(e, dollars, dollars >= 0 ? "Income from Event Center just came in." : "Event Center expenses were paid.");
	}
}

// ------------------------------------------------------- generic venues

void VenueSystem::genericTick(Item::YootVenue * v, int minute)
{
	const std::string & id = v->prototype->id;
	const int day = (int)std::floor(game->time.absolute);
	const int open = v->openMinute(), close = v->closeMinute();
	const int cap = v->capacity();
	const int stay = stayOf(v, 60);

	if (id == "stadium")
	{
		// Stadium.t2p FUN_1000bf10.
		const int prev = v->status;
		v->status = Yoot::stadiumStatus(prev, minute, open, close, Yoot::stadiumSeason(dayOfYear()), rand());
		if (prev == 10 && v->status == 30) { v->visitorsToday = 0; v->lastOpenDay = day; }
		if (prev >= 30 && v->status == 10) v->status = 10;
		// CollectCustomer from 12:00 to 19:00 while there is room; visitors
		// stay until the close (EnterCustomer bind = TNDF close), so the
		// house fills over the 420-minute window.
		if (v->status >= 30 && v->status < 35 && minute < 1140)
		{
			const int before = v->users();
			collect(v, minute, 1140 - open);
			const long long leave = Yoot::absoluteMinute(game->time.absolute) + (close - minute);
			for (size_t k = before; k < v->leaveAt.size(); ++k) v->leaveAt[k] = leave;
		}
		if (v->status == 35 && prev < 35) v->leaveAt.resize(v->leaveAt.size() / 2);
		else if (v->status == 35 && minute % 8 == 0) v->leaveAt.resize(v->leaveAt.size() - v->leaveAt.size() / 4);
		if (prev >= 30 && v->status == 10)
		{
			v->leaveAt.clear();
			if (v->prototype->yoot)
				pay(v, Yoot::commercialIncome(v->visitorsToday, *v->prototype->yoot), "Income from Stadium just came in");
		}
		v->setPattern(v->status >= 30 ? 1 : 0);
		return;
	}

	bool open_ = minute >= open && minute < close;
	if (id == "yootcathedral") open_ = Yoot::cathedralOpen(game->time.isHoliday(), minute, open, close);
	bool draws = true;
	if (id == "observatory" || id == "ticketbooth")
	{
		// Observatory.t2p FUN_1000cd70: visitors come through a ticket booth.
		draws = firstBuilt<Item::YootVenue>(game, "ticketbooth") && firstBuilt<Item::YootVenue>(game, "observatory");
		if (id == "observatory" && open_ && !firstBuilt<Item::YootVenue>(game, "ticketbooth") &&
		    (minute == open || minute == 720 || minute == close - 1))
			game->timeWindow.showMessage("A Ticket Booth is required in order to admit visitors to the Observatory.");
	}
	if (id == "boatdeck") draws = false; // visitors come by ship (cruiseTick)

	if (open_)
	{
		if (v->status < 30 || v->lastOpenDay != day)
		{
			if (v->lastOpenDay != day) v->visitorsToday = 0;
			v->lastOpenDay = day;
			v->status = 30;
		}
		if (draws)
		{
			if (id == "ticketbooth")
			{
				// Ticket.t2p FUN_1000d200: the booth sells the tickets and
				// passes its customers on to the observatory.
				Item::YootVenue * obs = firstBuilt<Item::YootVenue>(game, "observatory");
				if (obs && obs->isOpen())
				{
					const int before = obs->visitorsToday;
					collect(obs, minute, stayOf(obs, 30));
					v->visitorsToday += obs->visitorsToday - before;
				}
			}

			else if (id != "observatory")
				collect(v, minute, stay);
		}
	}
	else if (v->status >= 30)
	{
		v->status = 10;
		v->leaveAt.clear();
		if (v->lastOpenDay == day && v->settledDay != day && v->prototype->yoot &&
		    (id == "attraction" || id == "ticketbooth"))
		{
			v->settledDay = day;
			pay(v, Yoot::commercialIncome(v->visitorsToday, *v->prototype->yoot),
			    id == "attraction" ? "Income from Attraction just came in" : "Income from Ticket Booth just came in");
		}
	}

	const int inside = v->users();
	if (id == "attraction")
	{
		v->setPattern(Yoot::attractionPattern(inside));
		Item::Attraction * a = dynamic_cast<Item::Attraction *>(v);
		if (a && Yoot::attractionRenovates(grade(), a->visitorsToday, a->renovated))
		{
			// Attraction.t2p FUN_1000c290.
			game->timeWindow.showMessage("The Attraction will be renovated to commemorate having over 200 visitors.");
			LOG(IMPORTANT, "Attraction renovated after %d visitors", a->visitorsToday);
			a->renovate();
			game->grantFinalGrade();
		}
	}
	else if (id == "observatory")
		v->setPattern(!open_ ? ((minute < 360 || minute >= 1080) ? 0 : 1)
			: inside == 0 ? 1 : inside < cap / 3 ? 2 : inside < 2 * cap / 3 ? 3 : 4);
	else if (id == "yootcathedral")
	{
		if (v->pattern < 2 || !wedding.active) v->setPattern(open_ ? 1 : 0);
	}
	else
		v->setPattern(v->status < 30 ? 0 : inside == 0 ? 1 : (inside >= cap / 2 ? 3 : 2));
}

// ----------------------------------------------------------------- subway

void VenueSystem::subwayTick(int minute)
{
	Item::Subway * s = firstBuilt<Item::Subway>(game, "subway");
	if (!s) return;
	const int open = s->prototype->yoot && s->prototype->yoot->closeMinute > 0 ? s->prototype->yoot->openMinute : 390;
	const int close = s->prototype->yoot && s->prototype->yoot->closeMinute > 0 ? s->prototype->yoot->closeMinute : 1439;
	s->open = Yoot::subwayOpen(minute, open, close);
	const std::vector<YootEventSpec> & ev = game->activeRules.profile().events;
	YootMetroDef metro;
	bool have = false;
	for (const YootEventSpec & e : ev)
		if (decodeYootMetro(e, metro)) { have = true; break; }
	if (!have) return;
	const std::vector<int> arrivals = Yoot::subwayArrivals(metro.trains, s->level);
	const int stopped = s->open ? Yoot::subwayStoppedTrain(arrivals, minute) : -1;
	if (stopped >= 0 && !s->trainPresent)
	{
		// Riders get off and head into the tower (they return by a later train).
		const int riders = std::min(20, capacityOf(s, 100) / 5);
		s->trainType = Yoot::subwayStoppedTrainType(metro.trains, s->level, minute);
		s->trainArrives(riders);
	}
	else if (stopped < 0 && s->trainPresent)
		s->trainLeaves();
}

bool VenueSystem::upgradeSubway(Item::Subway * s, std::string * reason)
{
	if (!s) return false;
	if (s->level >= Yoot::kSubwayMaxLevel)
	{
		if (reason) *reason = "The subway schedule is already at its maximum.";
		return false;
	}
	int cost = 500000;
	for (const YootEventSpec & e : game->activeRules.profile().events)
	{
		YootMetroDef metro;
		if (decodeYootMetro(e, metro)) { cost = metro.unk * kYootMoneyUnit; break; }
	}
	if (game->funds < cost)
	{
		if (reason) *reason = "Not enough money. You cannot expand the schedule of the subway.";
		return false;
	}
	game->transferFunds(-cost, "construction", "The subway schedule was expanded.");
	s->level++;
	return true;
}

// ----------------------------------------------------------------- cruise

void VenueSystem::cruiseTick(int minute)
{
	Item::YootVenue * deck = firstBuilt<Item::YootVenue>(game, "boatdeck");
	if (!deck || game->activeRules.profile().worldId != "hawaii") return;
	if (!cruiseInit)
	{
		// CREV (Hawai.t2p T2Cruise ctor 0x10012ac0): dock x 46, 30 minutes
		// at the dock, sailings at 07:00, 11:00, 15:00 and 19:00, grades 3+.
		cruiseInit = true;
		cruiseTimes = {420, 660, 900, 1140};
	}
	if (grade() < 3 || !deck->isOpen()) return;
	for (int t : cruiseTimes)
		if (minute == t)
		{
			// MoveToShip / CheckRouteFromPool: the ship brings pool visitors.
			const int n = game->eventSystem.people.drawVisitors(deck,
				std::max(0, std::min(deck->capacity() - deck->users(), deck->capacity() / 2)));
			const long long now = Yoot::absoluteMinute(game->time.absolute);
			for (int k = 0; k < n; ++k) deck->leaveAt.push_back(now + cruiseDockMinutes);
			deck->visitorsToday += n;
		}
}

bool VenueSystem::cruiseDocked() const
{
	Item::YootVenue * deck = firstBuilt<Item::YootVenue>(game, "boatdeck");
	if (!deck || !deck->isOpen() || grade() < 3 || game->activeRules.profile().worldId != "hawaii") return false;
	const int minute = std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
	static const int kSailings[] = {420, 660, 900, 1140};
	for (int t : kSailings)
		if (minute >= t && minute < t + cruiseDockMinutes) return true;
	return false;
}

// ------------------------------------------------------------ final items

void VenueSystem::beginBaseball()
{
	// T2Baseball::IsBeginDay 0x10011ac0.
	const bool hasStadium = firstBuilt<Item::Stadium>(game, "stadium") != nullptr;
	if (!Yoot::baseballBeginDay(baseballDone, dayOfYear(), hasStadium)) return;
	baseball.active = true;
	baseball.state = 1;
	LOG(IMPORTANT, "Baseball event begins (waiting for %d visitors)", Yoot::kBaseballVisitors);
}

void VenueSystem::baseballTick()
{
	Item::Stadium * s = firstBuilt<Item::Stadium>(game, "stadium");
	// Exec 0x10011c40 state 1: stop when the stadium is not open.
	if (!s || !s->isOpen()) { baseball = WorldEvent(); return; }
	if (s->visitorsToday < Yoot::kBaseballVisitors || baseball.state != 1) return;
	// States 2..6: the team is formed (dialog 8000) and named in dialog
	// 12006 (default "Towers"; DialogHook refuses an empty name), then the
	// memorial game is announced; state 9 calls doc vt+0x188. The sim
	// holds while the dialog is open. No fireworks.
	baseball.state = 6;
	game->timeWindow.showMessage("Your very own professional baseball team has been formed to play in the newly constructed baseball stadium.");
	game->askEventText("Baseball", "Please name your baseball team.",
		s->teamName.empty() ? "Towers" : s->teamName, [this](const std::string & name) {
			Item::Stadium * st = firstBuilt<Item::Stadium>(game, "stadium");
			if (st) st->teamName = name;
			game->timeWindow.showMessage("The memorial baseball game of " + name + " will be starting at the stadium.");
			LOG(IMPORTANT, "Baseball: the memorial game of %s", name.c_str());
			game->grantFinalGrade();
			baseballDone = true;
			baseball = WorldEvent();
		});
}

void VenueSystem::beginWedding()
{
	Item::YootVenue * c = firstBuilt<Item::YootVenue>(game, "yootcathedral");
	if (!Yoot::weddingBeginDay(weddingDone, game->time.isHoliday(), c != nullptr)) return;
	// Exec 0x10015120 / LookupCouple 0x100157a0: residents of two Hawaiian
	// condos (or one with two residents) with an estimate above 70.
	std::vector<Item::Item *> homes;
	static const char * condos[] = {"condo", "yoot_condo"};
	for (const char * kind : condos)
	{
		const Game::ItemSet & s = itemsOf(game, kind);
		for (Game::ItemSet::const_iterator i = s.begin(); i != s.end(); ++i)
		{
			Item::Item * h = *i;
			if (!built(h) || h->population <= 0) continue;
			if (h->yootEstimate != 0 && h->yootEstimate <= 70) continue;
			homes.push_back(h);
			if (h->population >= 2) homes.push_back(h);
		}
	}
	const bool couple = homes.size() >= 2;
	if (!Yoot::weddingReady(c->isOpen(), c->visitorsToday, couple))
	{
		LOG(IMPORTANT, "Wedding cancelled today (open %d, visitors %d, couple %d)", c->isOpen(), c->visitorsToday, couple);
		return;
	}
	auto room = [](Item::Item * h) { return std::to_string(h->position.y + 1) + "F"; };
	auto who = [](Item::Item * h) { return h->displayName.empty() ? h->prototype->name : h->displayName; };
	game->timeWindow.showMessage(Yoot::weddingAnnouncement(room(homes[0]), who(homes[0]), room(homes[1]), who(homes[1])));
	c->setPattern(3);
	LOG(IMPORTANT, "Wedding at the Cathedral (%d visitors)", c->visitorsToday);
	game->grantFinalGrade();
	c->setPattern(2);
	weddingDone = true;
}

void VenueSystem::weddingTick() {}

// ------------------------------------------------------------ player choices

std::vector<std::pair<std::string, std::string> > VenueSystem::choices(Item::Item * item)
{
	std::vector<std::pair<std::string, std::string> > out;
	if (!item || !enabled()) return out;
	const int day = (int)std::floor(game->time.absolute);
	if (Item::Theater * t = dynamic_cast<Item::Theater *>(item))
	{
		for (const Movie & m : movies())
		{
			const int phase = Yoot::movieLifePhase(m.def, m.state, day, std::max(0, movieBaseDay < 0 ? day : movieBaseDay));
			if (!Yoot::movieOffered(m.def, m.state, t->classic(), phase)) continue;
			char label[160];
			snprintf(label, sizeof(label), "%s ($%d)", Yoot::movieTitle(m.def, m.state, t->classic()).c_str(), m.def.unkFC * kYootMoneyUnit);
			out.push_back(std::make_pair(std::string(label), m.plugin));
		}
	}
	else if (dynamic_cast<Item::EventCenter *>(item))
	{
		out.push_back(std::make_pair(std::string("Stop the Event"), std::string()));
		for (const std::string & name : game->app.pluginManager.enumerate())
		{
			if (!startsWith(name, "Pl")) continue;
			AbstractPlugin * p = game->app.pluginManager.get(name);
			HallEventDef def;
			if (!p || !p->ensureLoaded() || !p->getHallEventDef(def)) continue;
			out.push_back(std::make_pair(def.header.title + " (" + std::to_string(def.bookingDays) + " days)", name));
		}
	}
	else if (dynamic_cast<Item::Billboard *>(item))
	{
		uint32_t host = 0;
		if (item->prototype->yoot && !item->prototype->yoot->plugin.empty())
		{
			TemplateDef td;
			AbstractPlugin * bp = game->app.pluginManager.get(item->prototype->yoot->plugin);
			if (bp && bp->ensureLoaded() && bp->getTemplateDef(td))
				for (int k = 3; k >= 0; --k) host = (host << 8) | (unsigned char)td.tool.subPluginType.c[k];
		}
		for (const std::string & name : game->app.pluginManager.enumerate())
		{
			AbstractPlugin * p = game->app.pluginManager.get(name);
			AdvertiseDef def;
			if (!p || p->container() != ResourceContainer::SP || !p->ensureLoaded() || !p->getAdvertiseDef(def)) continue;
			if (host && !Yoot::advertiseFits(def, host)) continue;
			out.push_back(std::make_pair(def.header.title + " ($" + std::to_string(def.money * kYootMoneyUnit) + ")", name));
		}
	}
	return out;
}

bool VenueSystem::choose(Item::Item * item, const std::string & plugin, std::string * reason)
{
	const int day = (int)std::floor(game->time.absolute);
	const int minute = std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
	if (Item::Theater * t = dynamic_cast<Item::Theater *>(item))
	{
		// Theater.t2p 0x1000dfa0 / 0x1000dd60.
		Movie * m = movie(plugin);
		if (!m || plugin == t->movie) return false;
		if (t->status > 10)
		{
			if (reason) *reason = "You cannot change the movie when the theater is open.";
			return false;
		}
		const int cost = m->def.unkFC * kYootMoneyUnit;
		if (game->funds < cost)
		{
			if (reason) *reason = "Not enough money.";
			return false;
		}
		t->movie = plugin;
		t->expired = false;
		t->audienceTarget = -1;
		t->showCount = 0;
		game->transferFunds(-cost, "construction", "Feature film: " + Yoot::movieTitle(m->def, m->state, t->classic()));
		return true;
	}
	if (Item::EventCenter * e = dynamic_cast<Item::EventCenter *>(item))
	{
		if (plugin.empty()) { e->event.clear(); e->span = 0; e->today.clear(); return true; }
		HallEventDef def;
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		if (!p || !p->ensureLoaded() || !p->getHallEventDef(def)) return false;
		if (!e->event.empty() && e->runsOn(day) && e->today == e->event)
		{
			if (reason) *reason = "An event is already running.";
			return false;
		}
		if (hallCooldown[plugin] > 0)
		{
			if (reason) *reason = def.header.title + " cannot be booked again yet.";
			return false;
		}
		// FUN_1000e060: today only before the event's doors open, else tomorrow.
		const int start = minute < (int)def.openTime - 60 ? day : day + 1;
		if (!Yoot::hallEventInMonth(def, (start % 12 + 12) % 12))
		{
			if (reason) *reason = def.header.title + " is out of season.";
			return false;
		}
		e->event = plugin;
		e->startDay = start;
		e->span = std::max(1, (int)def.bookingDays);
		if (start == day) e->today = plugin;
		return true;
	}
	if (Item::Billboard * b = dynamic_cast<Item::Billboard *>(item))
	{
		// Billboard.t2p FUN_1000bf40 / FUN_1000bba0.
		if (b->daysLeft > 0)
		{
			if (reason) *reason = "You cannot choose another client during the term of the contract.";
			return false;
		}
		AdvertiseDef def;
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		if (!p || !p->ensureLoaded() || !p->getAdvertiseDef(def)) return false;
		b->showAd(plugin, (int)def.days);
		b->lastDay = day;
		char msg[200];
		snprintf(msg, sizeof(msg), "%s has paid $%d to you as the advertizement charge.", def.header.title.c_str(), def.money * kYootMoneyUnit);
		game->transferFunds(def.money * kYootMoneyUnit, "entertainment_income", msg);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- clouds

sf::Texture * VenueSystem::worldTexture(int id) const
{
	std::map<int, sf::Texture>::iterator it = worldTextures.find(id);
	if (it == worldTextures.end())
	{
		sf::Texture & tex = worldTextures[id];
		YootWorld::Id w;
		if (parseYootWorldIdentifier(game->activeRules.profile().worldId, w))
			game->app.bitmaps.loadFromPlugin(yootWorldPluginFile(w), id, tex, sf::Color::White);
		it = worldTextures.find(id);
	}
	return it->second.getSize().x ? &it->second : nullptr;
}

void VenueSystem::cloudTick(long long absMinute)
{
	const RulesProfile & profile = game->activeRules.profile();
	if (!cloudsInit)
	{
		cloudsInit = true;
		for (const YootEventSpec & e : profile.events)
		{
			YootCloudDef def;
			if (decodeYootCloud(e, def)) { cloudImages = def.bitmaps; break; }
		}
		if (cloudImages.empty()) return;
		clouds.assign(Yoot::kCloudCount, Cloud());
		const int widthPx = profile.worldWidth * 8;
		for (Cloud & c : clouds)
		{
			c.image = cloudImages[rand() % cloudImages.size()];
			const sf::Texture * t = worldTexture(c.image);
			const int w = t ? (int)t->getSize().x : 64;
			c.live = true;
			c.x = (float)(rand() % std::max(1, widthPx - w));
			c.y = (float)Yoot::cloudFloor(profile.groundRow - 25, rand());
			c.speed = rand() % 4 + 2;
		}
		lastCloudMinute = absMinute;
	}
	if (clouds.empty()) return;
	// Exec 0x100130b0: one step right every 10 game minutes; a cloud that
	// leaves the world is replaced at the left edge.
	while (absMinute - lastCloudMinute >= Yoot::kCloudStepMinutes)
	{
		lastCloudMinute += Yoot::kCloudStepMinutes;
		for (Cloud & c : clouds)
		{
			c.x += c.speed;
			if (c.x > profile.worldWidth * 8)
			{
				c.image = cloudImages[rand() % cloudImages.size()];
				const sf::Texture * t = worldTexture(c.image);
				c.x = (float)(1 - (t ? (int)t->getSize().x : 64));
				c.y = (float)Yoot::cloudFloor(profile.groundRow - 25, rand());
				c.speed = rand() % 4 + 2;
			}
		}
	}
	if (absMinute < lastCloudMinute) lastCloudMinute = absMinute;
}

void VenueSystem::draw(sf::RenderTarget & target) const
{
	if (!enabled()) return;
	const RulesProfile & profile = game->activeRules.profile();
	for (const Cloud & c : clouds)
	{
		if (!c.live) continue;
		sf::Texture * t = worldTexture(c.image);
		if (!t) continue;
		Sprite s;
		s.SetImage(*t);
		// c.y rows above the ground line (QD y = (G - k) * 36 from the world top).
		s.setPosition({c.x - profile.unitOriginX * 8.f, (c.y - profile.groundRow) * 36.f});
		target.draw(s);
	}
	// T2Cruise: the ship (Hawai.t2p bitmap 8800 at 2x) sails in from the
	// left at one unit a minute, docks for 30 minutes and sails back.
	if (profile.worldId == "hawaii" && firstBuilt<Item::YootVenue>(game, "boatdeck") && grade() >= 3)
	{
		const int minute = std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
		for (int t0 : cruiseTimes)
		{
			int unit;
			if (minute >= t0 - cruiseDockX && minute < t0) unit = minute - (t0 - cruiseDockX);
			else if (minute >= t0 && minute < t0 + cruiseDockMinutes) unit = cruiseDockX;
			else if (minute >= t0 + cruiseDockMinutes && minute < t0 + cruiseDockMinutes + cruiseDockX)
				unit = cruiseDockX - (minute - t0 - cruiseDockMinutes);
			else continue;
			sf::Texture * ship = worldTexture(8800);
			if (!ship) break;
			Sprite s;
			s.SetImage(*ship);
			s.setScale({2.f, 2.f});
			s.setOrigin({(float)ship->getSize().x, (float)ship->getSize().y});
			s.setPosition({(unit - profile.unitOriginX) * 8.f, 34.f});
			target.draw(s);
		}
	}
}

void VenueSystem::drawTrains(sf::RenderTarget & target) const
{
	const YootWorldArt * art = game->yootSkinArt();
	if (!enabled() || !art) return;
	const RulesProfile & profile = game->activeRules.profile();
	YootMetroDef metro;
	bool have = false;
	for (const YootEventSpec & e : profile.events)
		if (decodeYootMetro(e, metro)) { have = true; break; }
	if (!have || profile.worldWidth <= 0) return;
	// The run follows the clock in ticks (4 a minute); the station is the
	// Subway's left edge, the world centre without one (SetStation).
	const double minutes = std::max(0.0, game->time.hour * 60.0);
	const int minute = std::min(1439, (int)std::floor(minutes));
	const int tick = std::min(3, (int)std::floor((minutes - minute) * 4.0));
	Item::Subway * s = firstBuilt<Item::Subway>(game, "subway");
	const int station = s ? s->getPosition().x + profile.unitOriginX : profile.worldWidth / 2;
	const int level = s ? s->level : 0;
	// 320x32 at the top of the bottom row (WORLDDEF bottomFloorLine), the
	// Subway's platform row; its 4 px floor strip stays visible.
	const float top = float(-profile.lowestFloor * 36);
	static const char * kParts[] = {"MET1", "MET2", "MET3", "MET4"};
	for (const Yoot::SubwayTrainPos & t : Yoot::subwayTrains(metro.trains, level, profile.worldWidth, station, minute, tick))
	{
		const YootPluginView * v = yootPluginView(game->app, yootWorldPluginFile(art->worldId()),
			sf::Color::Transparent, 8000, 8000, kParts[std::max(0, std::min(3, t.track))]);
		if (!v || v->bands.empty()) continue;
		sf::Sprite sprite(v->texture, v->band(0));
		sprite.setPosition({float((t.unit - profile.unitOriginX) * 8), top});
		target.draw(sprite);
	}
}

// ------------------------------------------------------------ persistence

void VenueSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	if (!enabled()) return;
	xml.PushAttribute("yootMovieBase", movieBaseDay);
	xml.PushAttribute("yootBaseballDone", baseballDone);
	xml.PushAttribute("yootWeddingDone", weddingDone);
	std::ostringstream m;
	for (const Movie & mv : movieList)
	{
		const Yoot::MovieState & s = mv.state;
		if (s.flags == 0 && s.bestR == -1 && s.bestF == -1 && s.pendingR == -1 && s.pendingF == -1 && s.remakeCount == 0) continue;
		m << mv.plugin << ',' << s.flags << ',' << s.bestR << ',' << s.bestF << ',' << s.pendingR << ',' << s.pendingF << ',' << s.remakeCount << ';';
	}
	xml.PushAttribute("yootMovies", m.str().c_str());
	std::ostringstream h;
	for (const auto & c : hallCooldown)
	{
		std::map<std::string, int>::const_iterator v = hallValue.find(c.first);
		h << c.first << ',' << c.second << ',' << (v != hallValue.end() ? v->second : -1) << ';';
	}
	for (const auto & v : hallValue)
		if (!hallCooldown.count(v.first)) h << v.first << ",0," << v.second << ';';
	xml.PushAttribute("yootHallEvents", h.str().c_str());
}

void VenueSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	reset();
	movieBaseDay = xml.IntAttribute("yootMovieBase", -1);
	baseballDone = xml.BoolAttribute("yootBaseballDone", false);
	weddingDone = xml.BoolAttribute("yootWeddingDone", false);
	auto records = [](const char * text) {
		std::vector<std::vector<std::string> > out;
		std::string s = text ? text : "";
		size_t pos = 0;
		while (pos < s.size())
		{
			size_t end = s.find(';', pos);
			if (end == std::string::npos) end = s.size();
			std::vector<std::string> f;
			std::stringstream ss(s.substr(pos, end - pos));
			std::string tok;
			while (std::getline(ss, tok, ',')) f.push_back(tok);
			if (!f.empty()) out.push_back(f);
			pos = end + 1;
		}
		return out;
	};
	if (game && game->activeRules.profile().yootWorld)
	{
		for (const auto & f : records(xml.Attribute("yootMovies")))
		{
			if (f.size() < 7) continue;
			Movie * mv = movie(f[0]);
			if (!mv) continue;
			mv->state.flags = atoi(f[1].c_str());
			mv->state.bestR = atoi(f[2].c_str());
			mv->state.bestF = atoi(f[3].c_str());
			mv->state.pendingR = atoi(f[4].c_str());
			mv->state.pendingF = atoi(f[5].c_str());
			mv->state.remakeCount = atoi(f[6].c_str());
		}
	}
	for (const auto & f : records(xml.Attribute("yootHallEvents")))
	{
		if (f.size() < 3) continue;
		if (atoi(f[1].c_str()) > 0) hallCooldown[f[0]] = atoi(f[1].c_str());
		if (atoi(f[2].c_str()) >= 0) hallValue[f[0]] = atoi(f[2].c_str());
	}
}

// ------------------------------------------------------------- inspector

std::string VenueSystem::describe(Item::Item * item)
{
	if (!enabled() || !item) return "";
	std::ostringstream s;
	const int day = (int)std::floor(game->time.absolute);
	if (Item::Theater * t = dynamic_cast<Item::Theater *>(item))
	{
		Movie * m = movie(t->movie);
		if (!m) s << "\nFeature title has not been selected.\n";
		else
		{
			s << "\nNow showing: " << Yoot::movieTitle(m->def, m->state, t->classic()) << "\n";
			if (t->expired) s << "The duration has passed.\n";
			s << "Produced by : " << m->def.production << "\nDirected by : " << m->def.director
			  << "\nPerformed by : " << m->def.starring << "\n";
			const int open = t->openMinute(), close = t->closeMinute();
			const int running = m->def.runningTime > 0 ? m->def.runningTime : 120;
			s << "Shows:";
			for (int k = 0; k < Yoot::theaterShowCount(open, close, running); ++k)
			{
				const int st = open + 30 + k * (running + 30);
				char b[16];
				snprintf(b, sizeof(b), " %d:%02d", st / 60, st % 60);
				s << b;
			}
			s << "\nAudience target " << t->audienceTarget << ", best show " << t->showBest << "\n";
		}
		s << "Visitors today " << t->visitorsToday << "\n";
	}
	else if (Item::EventCenter * e = dynamic_cast<Item::EventCenter *>(item))
	{
		HallEventDef def;
		AbstractPlugin * p = e->event.empty() ? nullptr : game->app.pluginManager.get(e->event);
		if (p && p->ensureLoaded() && p->getHallEventDef(def))
		{
			s << "\nBooked: " << def.header.title << "\n";
			const int left = e->startDay + e->span - day;
			if (e->startDay > day) s << "Starts tomorrow, " << e->span << " day(s)\n";
			else s << left << " day(s) left\n";
			char b[64];
			snprintf(b, sizeof(b), "%d:%02d - %d:%02d\n", def.openTime / 60, def.openTime % 60, def.closeTime / 60, def.closeTime % 60);
			s << b;
		}
		else
			s << "\nNo event booked.\n";
		s << "Visitors today " << e->visitorsToday << "\n";
	}
	else if (Item::Billboard * b = dynamic_cast<Item::Billboard *>(item))
	{
		AdvertiseDef def;
		AbstractPlugin * p = b->ad.empty() ? nullptr : game->app.pluginManager.get(b->ad);
		if (p && p->ensureLoaded() && p->getAdvertiseDef(def))
			s << "\nSponsor: " << def.header.title << ", " << b->daysLeft << " day(s) left\n";
		else
			s << "\nNo sponsor.\n";
	}
	else if (Item::Subway * w = dynamic_cast<Item::Subway *>(item))
	{
		s << "\nSchedule level " << w->level << "/" << Yoot::kSubwayMaxLevel << (w->trainPresent ? ", train in the station" : "") << "\n";
		YootMetroDef metro;
		for (const YootEventSpec & e : game->activeRules.profile().events)
			if (decodeYootMetro(e, metro))
			{
				s << "Trains:";
				for (int a : Yoot::subwayArrivals(metro.trains, w->level))
				{
					char buf[16];
					snprintf(buf, sizeof(buf), " %d:%02d", a / 60, a % 60);
					s << buf;
				}
				s << "\n";
				break;
			}
	}
	else if (Item::YootVenue * v = dynamic_cast<Item::YootVenue *>(item))
	{
		if (dynamic_cast<Item::Stadium *>(v) && !static_cast<Item::Stadium *>(v)->teamName.empty())
			s << "\nTeam: " << static_cast<Item::Stadium *>(v)->teamName << "\n";
		if (Item::Attraction * a = dynamic_cast<Item::Attraction *>(v))
			if (a->renovated) s << "\nRenovated.\n";
		s << "\n" << (v->isOpen() ? "Open" : "Closed") << ", inside " << v->users() << "/" << v->capacity()
		  << ", visitors today " << v->visitorsToday << "\n";
	}
	return s.str();
}
