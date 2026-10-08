/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Wave E - EXE-shaped VIP suite guest and approval system. */
#include "VipSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Hotel.h"
#include "Item/Parking.h"
#include "JudgeSystem.h"
#include "LevelUp.h"
#include "Person.h"
#include "Time.h"
#include "TimeWindow.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace OT;

namespace
{
	// Game::rating is zero-based: displayed 3 stars is rating 2. The EXE
	// arms only at displayed star 3, and the approval is read by the 3->4 gate.
	const int kVipRating = 2;
	const int kVipScheduleDay = 3;       // day % 9 == 3 (VipT 1240)
	const double kVipArrivalHour = 17.0;
	const double kVipTimeoutHour = 12.0;
	const double kVipParkingGraceHours = 0.25;
	const double kVipApprovalStress = 150.0; // tuning judge-moderate bar
}

VipSystem::VipSystem(Game * game)
: GameObject(game)
{
	reset();
}

void VipSystem::reset()
{
	nextVisitTime = 0.0;
	visitEndTime = 0.0;
	visiting = false;
	forcedVisit = false;
	guestArrived = false;
	guestCheckedIn = false;
	visitVoided = false;
	armedDay = -1;
	suiteFloor = 0;
	stressTotal = 0.0;
	stressTrips = 0;
	bookedSuite = NULL;
	vipGuest = NULL;
	goodReviews = 0;
	badReviews = 0;
	yootPendingVip = -1;
	yootArrivalTime = 0.0;
	yootLastDay = -1;
	yootDestination = NULL;
}

void VipSystem::clearApproval()
{
	// Consume exactly one favorable verdict. Lifetime bad reviews and any
	// future approval remain untouched (concilia LevelUp 1148:004f).
	if (goodReviews > 0)
		--goodReviews;
}

void VipSystem::scheduleNow()
{
	// Dev/test hook: retain the same arrival, parking and stress state machine,
	// but bypass the calendar and star gate.
	forcedVisit = true;
	nextVisitTime = game->time.absolute;
	armedDay = static_cast<int>(std::floor(game->time.absolute));
}

bool VipSystem::promotionNeedsVisit() const
{
	// VIP is a 3->4 prerequisite only. Once the favorable latch exists, the
	// day cadence must not arm another guest.
	return game->rating == kVipRating && goodReviews == 0;
}

Item::Hotel * VipSystem::firstBookableSuite() const
{
	Game::ItemSetByString::const_iterator bucket = game->itemsByType.find("hotel_suite");
	if (bucket == game->itemsByType.end())
		return NULL;

	std::vector<Item::Hotel *> suites;
	for (Game::ItemSet::const_iterator it = bucket->second.begin(); it != bucket->second.end(); ++it)
	{
		Item::Hotel *suite = dynamic_cast<Item::Hotel *>(*it);
		if (!suite || suite->underConstruction || suite->roomState != Item::Hotel::kClean)
			continue;
		// A route not computed yet is still bookable; the lazy route pump may
		// finish it between reservation and the 17:00 arrival window.
		if (suite->lobbyRouteComputed && suite->lobbyRoute.empty())
			continue;
		suites.push_back(suite);
	}
	std::sort(suites.begin(), suites.end(), [](Item::Hotel *a, Item::Hotel *b) {
		if (a->position.y != b->position.y) return a->position.y < b->position.y;
		return a->position.x < b->position.x;
	});
	return suites.empty() ? NULL : suites.front();
}

bool VipSystem::liveSuite(Item::Hotel *suite) const
{
	if (!suite)
		return false;
	for (Game::ItemSet::const_iterator it = game->items.begin(); it != game->items.end(); ++it)
		if (*it == suite)
			return true;
	return false;
}

bool VipSystem::liveGuest(Person *guest) const
{
	if (!guest)
		return false;
	for (Game::PersonSet::const_iterator it = game->people.begin(); it != game->people.end(); ++it)
		if (*it == guest)
			return true;
	return false;
}

void VipSystem::armVisit()
{
	if (visiting || nextVisitTime > 0.0 || !promotionNeedsVisit())
		return;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	if (day % 9 != kVipScheduleDay)
		return;
	if (armedDay == day)
		return;

	armedDay = day;
	bookedSuite = firstBookableSuite();
	if (!bookedSuite)
	{
		// A VIP cannot be reserved without a suite. Leave the day consumed;
		// the next deterministic day-3 cadence will retry.
		suiteFloor = 0;
		return;
	}
	suiteFloor = bookedSuite->position.y;
	nextVisitTime = std::floor(game->time.absolute) + Time::hourToAbsolute(kVipArrivalHour);
	game->timeWindow.showMessage("VIP reservation: Hotel Suite on floor " + std::to_string(suiteFloor));
	LOG(IMPORTANT, "VIP reservation armed for suite floor %d on day %d", suiteFloor, day);
}

void VipSystem::beginVisit()
{
	if (visiting)
		return;

	// Re-resolve the reservation because construction/demolition may have
	// changed the item set since the 17:00 announcement.
	Item::Hotel *suite = liveSuite(bookedSuite) ? bookedSuite : firstBookableSuite();
	if (!suite || suite->underConstruction || suite->roomState != Item::Hotel::kClean)
	{
		nextVisitTime = 0.0;
		forcedVisit = false;
		bookedSuite = NULL;
		return;
	}

	bookedSuite = suite;
	suiteFloor = suite->position.y;
	visiting = true;
	guestArrived = false;
	guestCheckedIn = false;
	visitVoided = false;
	stressTotal = 0.0;
	stressTrips = 0;
	vipGuest = NULL;
	visitEndTime = std::floor(game->time.absolute) + 1.0 + Time::hourToAbsolute(kVipTimeoutHour);
	nextVisitTime = 0.0;
	forcedVisit = false;
	game->timeWindow.showMessage("A VIP is arriving by car at the Hotel Suite on floor " + std::to_string(suiteFloor));
	LOG(IMPORTANT, "VIP suite visit begins on floor %d at abs=%.4f", suiteFloor, game->time.absolute);
}

void VipSystem::endVisit(bool favorable)
{
	if (!visiting)
		return;
	visiting = false;
	nextVisitTime = 0.0;
	forcedVisit = false;
	bookedSuite = NULL;
	vipGuest = NULL;
	guestArrived = false;
	guestCheckedIn = false;

	if (favorable && !visitVoided && stressTotal <= kVipApprovalStress)
	{
		// Exactly one consumable approval per favorable checkout. No cash is
		// awarded by the EXE VIP path.
		++goodReviews;
		game->timeWindow.showMessage("VIP approved the tower's Hotel Suite.");
		LOG(IMPORTANT, "VIP verdict: favorable (banked stress=%.1f)", stressTotal);
		game->ratingMayIncrease();
	}
	else
	{
		++badReviews;
		game->timeWindow.showMessage(visitVoided
			? "VIP visit voided: the suite guest could not park."
			: "VIP was not satisfied with the tower.");
		LOG(IMPORTANT, "VIP verdict: unfavorable (void=%d, banked stress=%.1f)", visitVoided ? 1 : 0, stressTotal);
	}
	visitVoided = false;
}

void VipSystem::advanceGuest()
{
	if (!liveSuite(bookedSuite))
	{
		visitVoided = true;
		endVisit(false);
		return;
	}

	const double now = game->time.absolute;
	if (!guestArrived)
	{
		Item::Hotel::Guest *candidate = NULL;
		double latestArrival = 0.0;
		for (std::set<Item::Hotel::Guest *>::const_iterator it = bookedSuite->guests.begin(); it != bookedSuite->guests.end(); ++it)
		{
			Item::Hotel::Guest *guest = *it;
			latestArrival = std::max(latestArrival, guest->arrivalTime);
			if (guest->arrivalTime <= now && guest->parkingUsed &&
				(!candidate || guest->arrivalTime < candidate->arrivalTime))
				candidate = guest;
		}
		if (candidate)
		{
			vipGuest = candidate;
			vipGuest->name = "VIP";
			guestArrived = true;
			game->timeWindow.showMessage("VIP arrived by car.");
		}
		else if (!bookedSuite->guests.empty() && latestArrival > 0.0 &&
		         now >= latestArrival + Time::hourToAbsolute(kVipParkingGraceHours))
		{
			const bool parkingAvailable = Item::Parking::canPark(game, Item::Parking::kSuiteCar);
			visitVoided = true;
			LOG(DEBUG, "VIP arrival voided: %s",
			    parkingAvailable ? "suite guest did not reach the hotel" : "no usable parking bay");
			endVisit(false);
			return;
		}
	}

	if (!guestArrived || !liveGuest(vipGuest))
	{
		if (now >= visitEndTime)
		{
			visitVoided = true;
			endVisit(false);
		}
		return;
	}
	Item::Hotel::Guest *hotelGuest = dynamic_cast<Item::Hotel::Guest *>(vipGuest);
	if (!hotelGuest)
	{
		visitVoided = true;
		endVisit(false);
		return;
	}


	if (!guestCheckedIn)
	{
		if (vipGuest->at == bookedSuite)
			guestCheckedIn = true;
		else if (now >= hotelGuest->checkoutTime)
		{
			visitVoided = true;
			endVisit(false);
			return;
		}
	}
	if (!guestCheckedIn)
		return;

	// Person::stress is the current port's banked felt-wait value. Keep the
	// largest observed bank so temporary recovery cannot improve the verdict.
	stressTotal = std::max(stressTotal, vipGuest->stress);
	stressTrips = 1;
	if (!vipGuest->at && now >= hotelGuest->checkoutTime)
	{
		const bool favorable = !visitVoided && stressTrips > 0 && stressTotal <= kVipApprovalStress;
		endVisit(favorable);
	}
	else if (now >= visitEndTime)
	{
		visitVoided = true;
		endVisit(false);
	}
}

bool VipSystem::liveItem(Item::Item *item) const
{
	if (!item)
		return false;
	return game->items.find(item) != game->items.end();
}

Item::Item * VipSystem::findYootDestination(const std::vector<std::string> & prototypes) const
{
	for (size_t i = 0; i < prototypes.size(); ++i)
	{
		Game::ItemSetByString::const_iterator bucket = game->itemsByType.find(prototypes[i]);
		if (bucket == game->itemsByType.end())
			continue;
		for (Game::ItemSet::const_iterator it = bucket->second.begin(); it != bucket->second.end(); ++it)
			if (!(*it)->underConstruction)
				return *it;
	}
	return NULL;
}

int VipSystem::yootVisitEstimate(Item::Item *dest) const
{
	// T2People::VIPEstimation: the VIP's CalcCurrEstimate > 200. No VIP
	// person is spawned, so use the people-estimates (P2) of the people at
	// the destination, then its 1..300 tenant estimate (J1), and only as a
	// last resort the 0..100 evaluation x 3.
	int sum = 0, n = 0;
	for (Item::Item::People::const_iterator p = dest->people.begin(); p != dest->people.end(); ++p)
	{
		sum += (*p)->yootCurrentEstimate();
		++n;
	}
	if (n > 0)
		return sum / n;
	if (dest->yootEstimate > 0)
		return dest->yootEstimate;
	return LevelUp::yootEstimateFromEvaluation(dest->getEvaluation());
}

void VipSystem::resolveYootVisit()
{
	const std::vector<RulesProfile::Vip> & vips = game->activeRules.profile().vips;
	const int index = yootPendingVip;
	Item::Item *dest = yootDestination;
	yootPendingVip = -1;
	yootDestination = NULL;
	if (index < 0 || index >= (int)vips.size())
		return;
	const RulesProfile::Vip & vip = vips[index];
	if (!liveItem(dest))
	{
		++badReviews;
		game->timeWindow.showMessage("The VIP's destination is gone; the visit was cancelled.");
		return;
	}
	const int estimate = yootVisitEstimate(dest);
	if (LevelUp::yootVipSucceeds(estimate))
	{
		++goodReviews;
		game->yootVipFlags |= vip.vipFlag;
		game->timeWindow.showMessage(vip.goodMsg.empty() ? std::string("The VIP enjoyed the visit.") : vip.goodMsg);
		LOG(IMPORTANT, "YootTower VIP (grade %d) favourable: estimate %d, flag 0x%x", vip.grade, estimate, (unsigned)vip.vipFlag);
		game->ratingMayIncrease();
	}
	else
	{
		++badReviews;
		game->timeWindow.showMessage(vip.badMsg.empty() ? std::string("The VIP was not satisfied.") : vip.badMsg);
		LOG(IMPORTANT, "YootTower VIP (grade %d) unfavourable: estimate %d", vip.grade, estimate);
	}
}

void VipSystem::advanceYoot()
{
	if (yootPendingVip >= 0)
	{
		if (game->time.absolute >= yootArrivalTime)
			resolveYootVisit();
		return;
	}

	const RulesProfile & profile = game->activeRules.profile();
	if (profile.vips.empty())
		return;
	// T2VisitVIP::Init forces cycleDay to 1 and IsBeginDay admits the first
	// call of each day; IsBeginTime matches the VIEV begin minute (10:00).
	const int day = static_cast<int>(std::floor(game->time.absolute));
	if (day == yootLastDay || game->time.hour * 60.0 < profile.vips.front().beginMinute)
		return;
	yootLastDay = day;

	const int grade = game->rating + 1;
	const int numGrades = (int)profile.grades.size();
	for (size_t i = 0; i < profile.vips.size(); ++i)
	{
		const RulesProfile::Vip & vip = profile.vips[i];
		if (vip.grade < 1 || vip.grade > numGrades)
			continue;
		const RulesProfile::Grade & row = profile.grades[vip.grade - 1];
		if (!LevelUp::yootVipMayStart(vip.grade, grade, numGrades, vip.vipFlag, row.vipFlag,
		                              row.population, game->yootVipFlags, game->population))
			continue;

		if (vip.destPrototypes.empty())
		{
			// No OpenSky counterpart for the destination plugin: like the
			// engine's "plugin not installed" branch, grant the flag outright.
			game->yootVipFlags |= vip.vipFlag;
			LOG(IMPORTANT, "YootTower VIP (grade %d) has no destination counterpart; flag 0x%x granted", vip.grade, (unsigned)vip.vipFlag);
			game->ratingMayIncrease();
			return;
		}
		Item::Item *dest = findYootDestination(vip.destPrototypes);
		if (!dest)
		{
			const std::string name = game->itemFactory.prototypesById.count(vip.destPrototypes.front())
				? game->itemFactory.prototypesById[vip.destPrototypes.front()]->name
				: vip.destPrototypes.front();
			const char c = name.empty() ? 'x' : (char)std::tolower(name[0]);
			const bool vowel = c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
			game->timeWindow.showMessage(std::string("To invite a VIP, ") + (vowel ? "an " : "a ") + name + " needs to be built.");
			return;
		}
		yootPendingVip = (int)i;
		yootDestination = dest;
		yootArrivalTime = std::floor(game->time.absolute) + Time::hourToAbsolute(vip.arrivalMinute / 60.0);
		const int h = vip.arrivalMinute / 60, m = vip.arrivalMinute % 60;
		char when[32];
		std::snprintf(when, sizeof(when), "%d:%02d %s", (h % 12) ? h % 12 : 12, m, h < 12 ? "a.m." : "p.m.");
		game->timeWindow.showMessage("A VIP has made a reservation to visit your " + dest->prototype->name +
			". He will be arriving in the lobby at " + when + ".");
		LOG(IMPORTANT, "YootTower VIP (grade %d) reserved %s, arriving %s", vip.grade, dest->prototype->name.c_str(), when);
		return;
	}
}

void VipSystem::advance(double dt)
{
	(void)dt;
	if (game->activeRules.profile().yootWorld)
	{
		advanceYoot();
		return;
	}
	if (visiting)
	{
		advanceGuest();
		return;
	}
	if (nextVisitTime > 0.0 && game->time.absolute >= nextVisitTime)
	{
		beginVisit();
		return;
	}
	if (forcedVisit)
	{
		// Absolute time zero is a valid fresh-tower clock value; do not use
		// nextVisitTime's zero sentinel to strand scheduleNow().
		beginVisit();
		return;
	}
	armVisit();
}

void VipSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	xml.PushAttribute("vipNextVisit", nextVisitTime);
	xml.PushAttribute("vipVisiting", visiting);
	xml.PushAttribute("vipVisitEnd", visitEndTime);
	xml.PushAttribute("vipGoodReviews", goodReviews);
	xml.PushAttribute("vipBadReviews", badReviews);
	xml.PushAttribute("vipArmedDay", armedDay);
	xml.PushAttribute("vipSuiteFloor", suiteFloor);
	xml.PushAttribute("vipStressTotal", stressTotal);
	xml.PushAttribute("vipStressTrips", stressTrips);
	xml.PushAttribute("yootVipLastDay", yootLastDay);
}

void VipSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	nextVisitTime = xml.DoubleAttribute("vipNextVisit", 0.0);
	visitEndTime = xml.DoubleAttribute("vipVisitEnd", 0.0);
	goodReviews = std::max(0, xml.IntAttribute("vipGoodReviews", 0));
	badReviews = std::max(0, xml.IntAttribute("vipBadReviews", 0));
	armedDay = xml.IntAttribute("vipArmedDay", -1);
	suiteFloor = xml.IntAttribute("vipSuiteFloor", 0);
	stressTotal = std::max(0.0, xml.DoubleAttribute("vipStressTotal", 0.0));
	stressTrips = std::max(0, xml.IntAttribute("vipStressTrips", 0));
	// A pending YootTower reservation is transient (item pointer); drop it.
	yootPendingVip = -1;
	yootDestination = NULL;
	yootArrivalTime = 0.0;
	yootLastDay = xml.IntAttribute("yootVipLastDay", -1);

	// Guest pointers are transient. A mid-stay reload voids the EXE visit;
	// preserve the approval/review counters and let the next cadence retry.
	const bool savedVisiting = xml.BoolAttribute("vipVisiting", false);
	visiting = false;
	forcedVisit = false;
	guestArrived = false;
	guestCheckedIn = false;
	visitVoided = savedVisiting;
	bookedSuite = NULL;
	vipGuest = NULL;
	if (savedVisiting)
	{
		nextVisitTime = 0.0;
		armedDay = static_cast<int>(std::floor(game->time.absolute));
	}
}
