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
	// Tunables (OpenSkyWeb ISSUE-036 authentic trigger model).
	const double kVisitDurationAbs = 2.0 / 24;  // ~2 game-hours on premises
	const int    kSatisfiedReward  = 50000;     // cash bonus on positive review
	const double kGoodEvalThreshold  = 55.0;
	const double kExcellentEvalThreshold = 75.0;
	const double kRetryDays = 14.0;             // cooldown after a bad review
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

bool VipSystem::promotionNeedsVisit() const
{
	// The next promotion requires a VIP rating and everything else is
	// already satisfied — the VIP comes to inspect (OpenSkyWeb ISSUE-036).
	const LevelUp::Requirements * req = LevelUp::advancementRequirements(game->rating);
	if (!req || !req->needsVip) return false;
	const JudgeSystem::Counts & c = game->judgeSystem.counts();
	if (game->population < req->population) return false;
	if (req->needsSecurity && c.securityOffices <= 0) return false;
	if (req->needsMedical  && c.medicalCenters  <= 0) return false;
	if (req->needsMetro    && c.metros         <= 0) return false;
	return true;
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

	// A disappointed VIP needs time before re-inspecting (14-day retry
	// cooldown); a satisfied one completed the promotion gate, which
	// ratingMayIncrease() consumes below.
	nextVisitTime = (score < kGoodEvalThreshold)
		? game->time.absolute + kRetryDays
		: 0;

	game->ratingMayIncrease();
}

void VipSystem::advance(double dt)
{
	if (visiting)
	{
		if (game->time.absolute >= visitEndTime)
		{
			endVisit();
		}
		return;
	}

	// Pending forced visit (scheduleNow) or post-bad-review cooldown.
	if (nextVisitTime > 0)
	{
		if (game->time.absolute >= nextVisitTime)
		{
			nextVisitTime = 0;
			beginVisit();
		}
		return;
	}

	// Promotion gate: the next star needs a VIP rating and the tower
	// already satisfies every other requirement.
	if (promotionNeedsVisit())
	{
		beginVisit();
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
