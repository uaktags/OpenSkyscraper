#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	/** Star-rating progression rules.
	 *
	 * Encodes the original SimTower level system (LevelUpT.h/c, LevelT.h/c
	 * in the Yoot source):
	 *   - the population and facility requirements to advance from one
	 *     star to the next, and
	 *   - the minimum star rating required to build each prototype.
	 *
	 * `Game::ratingMayIncrease()` consults `advancementRequirements()` and
	 * the construction handler in `Game::handleEvent` plus `ToolboxWindow`
	 * consult `minRatingToBuild()`.
	 *
	 * Rating internally is 0..4 which displays as 1..5 stars. The 5-star ->
	 * TOWER (rating 5) step is the cathedral wedding special event: the EXE
	 * never promotes from 5 stars in LevelUp (1150:014d-0154), so the row is
	 * held shut by needsWedding until Cathedral::startWedding's mid-ceremony
	 * ratingMayIncrease() crowns the tower (mirrors the web port's
	 * Requirements.needsWedding / meetsRequirements(..., weddingCompleted)).
	 *
	 * The gates are ported from the EXE's LevelUp (seg_1148 FUN_1148_0000,
	 * byte-verified 2026-07-09) via concilia-tower's scan_promotion_flags /
	 * game_check_promotion. This class is a *pure* seam module: it takes
	 * plain data (PromotionContext) and never touches Game, so it stays
	 * linkable by the OpenSkyTests seam subset. */
	struct LevelUp
	{
		struct Requirements
		{
			int  population;       ///< Minimum tower population (0 = no requirement).
			bool needsSecurity;    ///< At least one Security office built.
			bool needsMedical;     ///< Medical coverage adequate (3->4, 4->5).
			bool needsMetro;       ///< A Metro station built.
			bool needsVip;         ///< A positive VIP review received.
			bool needsSuite;       ///< At least one hotel suite built (3->4).
			bool needsRecycling;   ///< Recycling adequate for the population (3->4, 4->5).
			bool needsWindow;      ///< Only lands in the weekday-evening window (3->4, 4->5).
			bool needsWedding;     ///< A cathedral wedding has been held (the 5->TOWER latch).
			const char * summary;  ///< Short human-readable description.
		};

		/// Tower-wide state the promotion gate reads. Assembled by Game from
		/// the live judge counts, a facility scan and the clock; kept as plain
		/// data so the gate itself stays pure and unit-testable.
		struct PromotionContext
		{
			int    population        = 0;     ///< Standing tower population.
			int    securityOffices   = 0;     ///< Security offices built.
			int    hotelSuites       = 0;     ///< Hotel suites built (0xB92B).
			int    metros            = 0;     ///< Metro stations built.
			bool   recyclingAdequate = false; ///< TrashT capacity keeps up (0xB92C).
			bool   medicalAdequate   = false; ///< 15-floor medical coverage (0xB92D).
			int    vipReviews        = 0;     ///< Favorable VIP verdicts on hand (0xB923).
			double hour              = 0.0;   ///< Game-clock hour, 0..24.
			int    day               = 0;     ///< Absolute day; the weekend is day % 3 == 2.
			bool   weddingCompleted  = false; ///< A cathedral wedding has been held (Cathedral::weddingCompleted).
		};

		/// Requirements that must be met to ADVANCE from `currentRating`
		/// to `currentRating + 1`. Returns NULL if `currentRating` is maxed
		/// (i.e. already at TOWER).
		static const Requirements * advancementRequirements(int currentRating);

		/// Minimum rating (0..4) required to BUILD the given prototype id.
		/// Items not listed return 0 (always buildable).
		static int minRatingToBuild(const std::string & prototypeId);

		/// The full authentic gate: population, facility presence, adequacy
		/// and the weekday-evening landing window (concilia LevelUp
		/// 1148:0000 / game_check_promotion).
		static bool meetsRequirements(const Requirements & req,
		                              const PromotionContext & ctx);

		/// Recycling is adequate while population stays under 2500 per
		/// recycling center; above that the trucks stop and stars 4/5 stay
		/// blocked (concilia trash_required_level, TrashT seg_1088).
		static bool recyclingAdequate(int population, int recyclingCenters);

		/// The 15-floor band a floor's medical coverage resolves in
		/// (concilia medical_band: floor <= 0 -> band 0, the ground zone).
		static int medicalBand(int floor);

		/// True when every floor in `servedFloors` (offices/condos) has a
		/// medical center in its own band, or a band-0 (ground-zone) center
		/// is built — the band-0 fallback rule from concilia game_medical_seek
		/// (MedicalT seg_1170). No offices/condos at all is adequate.
		static bool medicalAdequate(const std::vector<int> & servedFloors,
		                            const std::vector<int> & medicalFloors);

		/// The 3->4 and 4->5 promotions only land 5PM through the 7AM reset
		/// on weekdays; 2->3 has no clock gate (concilia promotion_window_open,
		/// LevelUp @00de/@00e5 and @011e/@0125).
		static bool promotionWindowOpen(double hour, int day);

		/// Landing-window bounds: opens at 5PM (17:00) and runs through the
		/// 7AM reset; the weekend closes it entirely.
		static const double kPromotionWindowStartHour;
		static const double kPromotionWindowEndHour;

		/// Promotion award gift message for reaching `newStars` (1-based).
		/// ALWAYS returns empty: the EXE pays no cash on a promotion — the
		/// errata's "$200k/$300k/$500k bonus" was the buried-treasure payout
		/// misfiled (Phase 1a adjudication, 2026-09-14). API kept for callers.
		static std::string starRewardMessage(int newStars);

		// --- YootTower grade rules (game-parity.md R1-R4) ---

		/// CheckGameLevel 0x10073290 for one grade row: the tower's VIP bits
		/// must cover the row's flag and population must reach the row's
		/// threshold. Nothing else gates a YootTower promotion.
		static bool yootPromotionReady(uint32_t rowVipFlag, int rowPopulation,
		                               uint32_t towerVipFlags, int population);

		/// T2VisitVIP::Start 0x1006cd90 gate: the VIP's grade is the current
		/// grade (and within the table), its flag is part of that grade row's
		/// flag, still unset on the tower, and population > row population / 2.
		static bool yootVipMayStart(int vipGrade, int currentGrade, int numGrades,
		                            uint32_t vipFlag, uint32_t rowVipFlag, int rowPopulation,
		                            uint32_t towerVipFlags, int population);

		/// T2People::VIPEstimation 0x10049990: favourable when the estimate
		/// (1..300 scale) is above 200.
		static bool yootVipSucceeds(int estimate);

		/// Estimate proxy until per-person estimates exist (P2): OpenSky's
		/// 0..100 tenant evaluation scaled onto the 1..300 estimate scale.
		static int yootEstimateFromEvaluation(double evaluation);

		/// T2.exe FUN_00422670 (doc vtable +0x188, the final-item event):
		/// only at grade == numGrades, OR that grade's flag into the tower
		/// bits. Returns false (bits untouched) at any other grade.
		static bool yootGrantFinalGrade(int grade, int numGrades, uint32_t rowVipFlag,
		                                uint32_t & towerVipFlags);
	};
}
