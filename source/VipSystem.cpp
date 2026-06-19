/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Phase 3.2 - VIP visits system. */
#include "VipSystem.h"

#include "Application.h"
#include "Game.h"
#include "JudgeSystem.h"
#include "LevelUp.h"
#include "Money.h"
#include "Person.h"
#include "Time.h"
#include "TimeWindow.h"

#include <cstdio>
#include <cstdlib>

using namespace OT;

namespace
{
	// Tunables. Keep these in one place so they're easy to find when
	// balancing the system against the SimTower original.
	const double kMinPopForVip     = 100;       // tower needs some momentum
	const int    kMinRatingForVip  = 1;         // 2-star tower minimum
	const double kVisitGapMinDays  = 3.0;       // earliest gap between visits
	const double kVisitGapMaxDays  = 8.0;       // latest gap
	const double kVisitDurationAbs = 2.0 / 24;  // ~2 game-hours on premises
	const int    kSatisfiedReward  = 50000;     // cash bonus on positive review
	const double kGoodEvalThreshold  = 55.0;    // avg eval above this = positive
	const double kExcellentEvalThreshold = 75.0;

	double randDays(double lo, double hi)
	{
		const double r = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
		return lo + r * (hi - lo);
	}
}

VipSystem::VipSystem(Game * game)
: GameObject(game)
{
	reset();
}

void VipSystem::reset()
{
	nextVisitTime = 0;
	visitEndTime  = 0;
	visiting      = false;
	goodReviews   = 0;
	badReviews    = 0;
}

void VipSystem::scheduleNow()
{
	// Used by dev keys / tests to force a visit on the next advance().
	nextVisitTime = game->time.absolute;
}

void VipSystem::scheduleNextVisit()
{
	// Only schedule once the tower is established. This avoids VIP
	// arrivals on a fresh 1-star lobby-only tower.
	if (game->rating < kMinRatingForVip)    { nextVisitTime = 0; return; }
	if (game->population < kMinPopForVip)   { nextVisitTime = 0; return; }

	const double gapAbs = Time::hourToAbsolute(24.0 * randDays(kVisitGapMinDays, kVisitGapMaxDays));
	nextVisitTime = game->time.absolute + gapAbs;
}

void VipSystem::beginVisit()
{
	visiting     = true;
	visitEndTime = game->time.absolute + kVisitDurationAbs;
	game->timeWindow.showMessage("A VIP is visiting the tower!");
	LOG(IMPORTANT, "VIP visit begins at abs=%.4f", game->time.absolute);
}

void VipSystem::endVisit()
{
	visiting = false;

	// Evaluate the tower state from the most recent judge pass. We don't
	// trigger a fresh evaluateAll here - the daily one is recent enough
	// and forcing one mid-frame risks reentrancy with settleDailyAccounting.
	const JudgeSystem::Counts & c = game->judgeSystem.counts();
	const double avgEval = (c.hotelAvgEval > 0.0)
		? (c.hotelAvgEval * 0.4 + 50.0 * 0.6)  // weight hotels slightly
		: 50.0;

	// Penalty for dirty rooms and critical tenants; bonus for good
	// facility coverage and high population (signals a thriving tower).
	double score = avgEval;
	score -= c.hotelsDirty * 3.0;
	score -= c.criticalTenants * 2.0;
	if (c.securityOffices > 0) score += 3;
	if (c.medicalCenters  > 0) score += 3;
	if (c.foodOutlets     > 0) score += 2;

	char buf[192];
	if (score >= kExcellentEvalThreshold)
	{
		++goodReviews;
		game->transferFunds(kSatisfiedReward, "vip", "VIP impressed - bonus granted");
		snprintf(buf, sizeof(buf), "VIP delighted! Bonus $%d granted.", kSatisfiedReward);
		game->timeWindow.showMessage(buf);
		LOG(IMPORTANT, "VIP review: EXCELLENT (score=%.1f) bonus=$%d", score, kSatisfiedReward);
	}
	else if (score >= kGoodEvalThreshold)
	{
		++goodReviews;
		// Half-bonus for "OK" reviews - acknowledge without over-rewarding.
		const int reward = kSatisfiedReward / 2;
		game->transferFunds(reward, "vip", "VIP content - small bonus");
		snprintf(buf, sizeof(buf), "VIP content with the tower. $%d bonus.", reward);
		game->timeWindow.showMessage(buf);
		LOG(IMPORTANT, "VIP review: OK (score=%.1f) bonus=$%d", score, reward);
	}
	else
	{
		++badReviews;
		snprintf(buf, sizeof(buf), "VIP unimpressed - address tenant complaints to improve");
		game->timeWindow.showMessage(buf);
		LOG(IMPORTANT, "VIP review: POOR (score=%.1f)", score);
	}

	// Schedule the next visit. A bad review pushes the next VIP further out
	// so the player has time to fix issues before being re-judged.
	const double baseGapDays = randDays(kVisitGapMinDays, kVisitGapMaxDays);
	const double extraGapDays = (score < kGoodEvalThreshold) ? 3.0 : 0.0;
	nextVisitTime = game->time.absolute + Time::hourToAbsolute(24.0 * (baseGapDays + extraGapDays));
}

void VipSystem::advance(double dt)
{
	// If we have no visit scheduled, try to schedule one (no-op until the
	// tower meets the population/rating thresholds).
	if (nextVisitTime == 0 && !visiting)
	{
		scheduleNextVisit();
	}

	if (visiting)
	{
		if (game->time.absolute >= visitEndTime)
		{
			endVisit();
		}
	}
	else if (nextVisitTime > 0 && game->time.absolute >= nextVisitTime)
	{
		// Threshold check again at fire time - the player may have dropped
		// below the population threshold (e.g. tenants fled) since we
		// scheduled.
		if (game->rating >= kMinRatingForVip && game->population >= kMinPopForVip)
			beginVisit();
		else
			scheduleNextVisit(); // try again later
	}
}

void VipSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	xml.PushAttribute("vipNextVisit",   nextVisitTime);
	xml.PushAttribute("vipVisiting",    visiting);
	xml.PushAttribute("vipVisitEnd",    visitEndTime);
	xml.PushAttribute("vipGoodReviews", goodReviews);
	xml.PushAttribute("vipBadReviews",  badReviews);
}

void VipSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	nextVisitTime = xml.DoubleAttribute("vipNextVisit", 0.0);
	visitEndTime  = xml.DoubleAttribute("vipVisitEnd",  0.0);
	visiting      = xml.BoolAttribute("vipVisiting",    false);
	goodReviews   = xml.IntAttribute("vipGoodReviews",  0);
	badReviews    = xml.IntAttribute("vipBadReviews",   0);
}
