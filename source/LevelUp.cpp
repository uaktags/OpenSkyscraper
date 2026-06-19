#include "LevelUp.h"

using namespace OT;

namespace
{
	// Requirements to advance FROM rating N TO rating N+1.
	// Index 0 = advancing from 1 star to 2 stars (rating 0 -> 1).
	// Index 4 is unused (5 stars is the maximum).
	const LevelUp::Requirements kAdvancement[] = {
		{   0, false, false, false, false, "Starting tower"               }, // rating 0 (1 star) - always
		{ 300, false, false, false, false, "Reach 300 population"         }, // 1 -> 2 stars
		{1000, true,  false, false, true,  "1000 population + Security + VIP Approval" }, // 2 -> 3 stars
		{2000, true,  true,  false, false, "2000 population + Medical"    }, // 3 -> 4 stars
		{3000, true,  true,  true,  true,  "3000 population + Metro + VIP Approval" }, // 4 -> 5 stars
	};

	struct ItemLock { const char * id; int minRating; };

	// Prototype id -> minimum star rating (0..4) required to build.
	// Items absent from this list are always buildable (min rating 0).
	const ItemLock kItemLocks[] = {
		// 2 stars (rating >= 1): basic retail + infra
		{ "fastfood",    1 },
		{ "parking",     1 },
		{ "recycling",   1 },
		// 3 stars (rating >= 2): security, restaurants, express elevators
		{ "security",          2 },
		{ "restaurant",        2 },
		{ "elevator_express",  2 },
		{ "elevator_service",  2 },
		// 4 stars (rating >= 3): hotel + medical
		{ "hotel_single",  3 },
		{ "hotel_double",  3 },
		{ "hotel_suite",   3 },
		{ "medicalcenter", 3 },
		// 5 stars (rating >= 4): cinema + metro
		{ "cinema",     4 },
		{ "partyhall",  4 },
		{ "metro",      4 },
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
