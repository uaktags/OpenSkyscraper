#include "LevelUp.h"

using namespace OT;

namespace
{
	// Requirements to advance FROM rating N TO rating N+1.
	// Index 0 = advancing from 1 star to 2 stars (rating 0 -> 1).
	// Population thresholds and facility gates aligned to the authentic
	// SimTower progression (SimTower wiki; OpenSkyWeb commit 46372a2):
	// 2★ 300 pop, 3★ 1000 + Security, 4★ 5000 + Medical & VIP approval,
	// 5★ 10000 + Metro. The original's 15000 + Cathedral "Tower of the
	// Year" step needs the Cathedral item (not yet implemented).
	const LevelUp::Requirements kAdvancement[] = {
		{   0, false, false, false, false, "Starting tower"                         }, // rating 0 (1 star) - always
		{ 300, false, false, false, false, "Reach 300 population"                   }, // 1 -> 2 stars
		{1000, true,  false, false, false, "1000 population + Security Office"      }, // 2 -> 3 stars
		{5000, true,  true,  false, true,  "5000 population + Medical & VIP Approval" }, // 3 -> 4 stars
		{10000,true,  true,  true,  false, "10000 population + Metro Station"       }, // 4 -> 5 stars
		{15000,true,  true,  true,  false, "15,000 population + Cathedral (TOWER rating)" }, // 5 -> TOWER
	};

	struct ItemLock { const char * id; int minRating; };

	// Prototype id -> minimum star rating (0..4) required to build.
	// Items absent from this list are always buildable (min rating 0).
	// Mapping aligned to the authentic SimTower toolbox (SimTower wiki;
	// OpenSkyWeb commit 46372a2).
	const ItemLock kItemLocks[] = {
		// 2 stars (rating >= 1)
		{ "elevator-service", 1 },
		{ "hotel_single",     1 },
		{ "security",         1 },
		// 3 stars (rating >= 2)
		{ "escalator",       2 },
		{ "elevator-express",2 },
		{ "restaurant",      2 },
		{ "cinema",          2 },
		{ "partyhall",       2 },
		{ "hotel_double",    2 },
		{ "hotel_suite",     2 },
		{ "medicalcenter",   2 },
		{ "recycling",       2 },
		{ "parking",         2 },
		// 4 stars (rating >= 3)
		{ "metro",          3 },
		// 3 stars (rating >= 2): premium automated security (Tim: above the
		// manual Security Office; web defaulted it to 2 stars with no
		// authentic gate evidence).
		{ "secom",           2 },
		// 5 stars (rating >= 4)
		{ "cathedral",      4 },
	};
	const size_t kNumItemLocks = sizeof(kItemLocks) / sizeof(kItemLocks[0]);
}

const LevelUp::Requirements * LevelUp::advancementRequirements(int currentRating)
{
	if (currentRating < 0 || currentRating >= static_cast<int>(sizeof(kAdvancement) / sizeof(kAdvancement[0])) - 1)
		return NULL;
	return &kAdvancement[currentRating + 1];
}

int LevelUp::minRatingToBuild(const std::string & prototypeId)
{
	for (size_t i = 0; i < kNumItemLocks; ++i)
	{
		if (prototypeId == kItemLocks[i].id)
			return kItemLocks[i].minRating;
	}
	return 0;
}

bool LevelUp::meetsRequirements(const Requirements & req,
                                int population,
                                const JudgeSystem::Counts & counts,
                                int vipReviews)
{
	if (population < req.population) return false;
	if (req.needsSecurity && counts.securityOffices <= 0) return false;
	if (req.needsMedical  && counts.medicalCenters <= 0)  return false;
	if (req.needsMetro    && counts.metros <= 0)          return false;
	if (req.needsVip      && vipReviews <= 0)             return false;
	return true;
}

std::string LevelUp::starRewardMessage(int newStars)
{
	switch (newStars)
	{
		case 2:  return "Your tower has been given a Two-Star Award!";
		case 3:  return "Your tower has been given a Three-Star Award!";
		case 4:  return "Your tower has been given a Four-Star Award!";
		case 5:  return "Your tower has been given a Five-Star Award!";
		default: return "";
	}
}
