#pragma once
#include <string>
#include "JudgeSystem.h"

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
	 * Rating internally is 0..4 which displays as 1..5 stars.
	 *
	 * TODO(Phase 3.2): the original also requires VIP visits and a Cathedral
	 * for the "Tower of the Year" 5-star ending. Wire those up once VIP
	 * arrivals (codemap.md:2653) are implemented. */
	struct LevelUp
	{
		struct Requirements
		{
			int  population;       ///< Minimum tower population (0 = no requirement).
			bool needsSecurity;    ///< At least one Security office built.
			bool needsMedical;     ///< At least one Medical Center built.
			bool needsMetro;       ///< A Metro station built.
			const char * summary;  ///< Short human-readable description.
		};

		/// Requirements that must be met to ADVANCE from `currentRating`
		/// to `currentRating + 1`. Returns NULL if `currentRating` is maxed
		/// (i.e. already at 5 stars).
		static const Requirements * advancementRequirements(int currentRating);

		/// Minimum rating (0..4) required to BUILD the given prototype id.
		/// Items not listed return 0 (always buildable).
		static int minRatingToBuild(const std::string & prototypeId);

		/// Helper: does the given tower state satisfy the requirements?
		static bool meetsRequirements(const Requirements & req,
		                               int population,
		                               const JudgeSystem::Counts & counts);
	};
}
