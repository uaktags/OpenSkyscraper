#include "LevelUp.h"

#include <set>

using namespace OT;

namespace
{
	// --- Promotion constants (concilia adoption row 21) ---

	// Weekend is the EXE's every-3rd-day flag (0xB3A0): day % 3 == 2. The
	// whole tower uses this convention (EventSystem, Cathedral, Item anim).
	const int kWeekendModulus = 3;
	const int kWeekendDay     = 2;

	// TrashT (seg_1088): capacity keeps up only while population stays under
	// 2500 per recycling center; above that the trucks stop and the star 4/5
	// gate stays shut (concilia trash_required_level, game.h 0xB92C).
	const int kRecyclingPopPerCenter = 2500;

	// MedicalT (seg_1170): a sick worker looks in his own 15-floor band
	// first, then any band-0 (ground-zone) center (concilia medical_band /
	// game_medical_seek).
	const int kMedicalBandFloors = 15;

	// Requirements to advance FROM rating N TO rating N+1.
	// Index 0 = advancing from 1 star to 2 stars (rating 0 -> 1).
	// Population thresholds stay authentic (300/1000/5000/10000/15000).
	// Field order: population, needsSecurity, needsMedical, needsMetro,
	// needsVip, needsSuite, needsRecycling, needsWindow, summary.
	// 2->3: security only, any hour. 3->4: suite + recycling + medical +
	// VIP, weekday evening. 4->5: metro + recycling + medical, weekday
	// evening (no VIP/suite re-check — the EXE reads neither 0xB923 nor
	// 0xB92B there). 5->TOWER is the cathedral wedding special event, not
	// granted by this table; its row is retained for the lock/UI lookups.
	const LevelUp::Requirements kAdvancement[] = {
		{    0, false, false, false, false, false, false, false, "Starting tower"                        }, // rating 0 (1 star) - always
		{  300, false, false, false, false, false, false, false, "Reach 300 population"                  }, // 1 -> 2 stars
		{ 1000, true,  false, false, false, false, false, false, "1000 population + Security Office"     }, // 2 -> 3 stars
		{ 5000, false, true,  false, true,  true,  true,  true,  "5000 population + Suite, Recycling & Medical adequacy + VIP approval (weekday evening)" }, // 3 -> 4 stars
		{10000, false, true,  true,  false, false, true,  true,  "10000 population + Metro, Recycling & Medical adequacy (weekday evening)"             }, // 4 -> 5 stars
		{15000, true,  true,  true,  false, false, false, false, "15,000 population + Cathedral (TOWER rating)" }, // 5 -> TOWER (wedding event; delegated)
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

// The weekday-evening landing window (concilia LevelUp @00de/@00e5 +
// promotion_window_open): opens at 5PM, runs through the 7AM reset.
const double LevelUp::kPromotionWindowStartHour = 17.0;
const double LevelUp::kPromotionWindowEndHour   = 7.0;

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

bool LevelUp::recyclingAdequate(int population, int recyclingCenters)
{
	if (recyclingCenters <= 0) return false;
	return population / recyclingCenters < kRecyclingPopPerCenter;
}

int LevelUp::medicalBand(int floor)
{
	return floor <= 0 ? 0 : floor / kMedicalBandFloors;
}

bool LevelUp::medicalAdequate(const std::vector<int> & servedFloors,
                              const std::vector<int> & medicalFloors)
{
	if (medicalFloors.empty()) return false;

	std::set<int> bands;
	bool hasGroundBand = false;
	for (size_t i = 0; i < medicalFloors.size(); ++i)
	{
		const int band = medicalBand(medicalFloors[i]);
		if (band == 0) hasGroundBand = true;
		bands.insert(band);
	}

	// A band-0 center is the universal fallback, so while one exists every
	// floor is served regardless of its band.
	if (hasGroundBand) return true;

	for (size_t i = 0; i < servedFloors.size(); ++i)
	{
		if (bands.find(medicalBand(servedFloors[i])) == bands.end())
			return false;
	}
	return true;
}

bool LevelUp::promotionWindowOpen(double hour, int day)
{
	const bool eveningOrLater = (hour >= kPromotionWindowStartHour ||
	                             hour < kPromotionWindowEndHour);
	return eveningOrLater && (day % kWeekendModulus != kWeekendDay);
}

bool LevelUp::meetsRequirements(const Requirements & req,
                                const PromotionContext & ctx)
{
	if (ctx.population < req.population) return false;
	if (req.needsSecurity  && ctx.securityOffices <= 0)  return false;
	if (req.needsSuite     && ctx.hotelSuites     <= 0)  return false;
	if (req.needsRecycling && !ctx.recyclingAdequate)    return false;
	if (req.needsMedical   && !ctx.medicalAdequate)      return false;
	if (req.needsMetro     && ctx.metros          <= 0)  return false;
	if (req.needsVip       && ctx.vipReviews      <= 0)  return false;
	if (req.needsWindow    && !promotionWindowOpen(ctx.hour, ctx.day)) return false;
	return true;
}

std::string LevelUp::starRewardMessage(int newStars)
{
	// No promotion cash. The errata's "$200k/$300k/$500k star-up bonus" was
	// the buried-treasure payout misfiled: LevelUp 1148:020f is the treasure
	// condition's epilogue and AwardMoney 1178:076f is the dig payout
	// (Phase 1a adjudication, 2026-09-14). The API is retained so callers and
	// tests keep a stable seam, but it never returns a gift.
	(void)newStars;
	return std::string();
}
