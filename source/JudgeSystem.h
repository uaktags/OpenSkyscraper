#pragma once

#include <map>

namespace OT
{
	class Game;
	class Person;

	namespace Item { class Item; }

	/** Daily tenant & hotel evaluation engine.
	 *
	 * Computes a 0..100 satisfaction score for each tenant item based on
	 * route quality, occupant stress and amenity coverage, then writes the
	 * result back into Item::evaluation. Office::isAttractive() and
	 * Condo::isAttractive() consult this score when deciding move-in and
	 * vacation, and the per-person eval field is refreshed so the inspector
	 * (Phase 4.1) and status overlays (Phase 4.3) have data to show.
	 *
	 * Mirrors the role of JudgeT.h/c in the original Yoot Tower source
	 * (JudgeAllTenant, JudgeAllHotel, JudgeOneTenant). The Counts struct
	 * approximates CountT.h/c and is intended to feed star progression
	 * (Phase 3.2) and the future minimap overlays.
	 */
	class JudgeSystem
	{
	public:
		JudgeSystem();

		/// Run a full evaluation pass. Intended to be called once per game
		/// day from Game::settleDailyAccounting().
		void evaluateAll(Game * game);

		/// Tallies accumulated during the last evaluateAll() pass.
		/// Approximates the per-type counters (CountPC / CountVC /
		/// CountIn / CountDay) from CountT.h/c in the Yoot source.
		struct Counts
		{
			int offices;
			int condos;
			int hotels;
			int hotelsDirty;
			int hotelsOccupied;   ///< hotel rooms currently kOccupied
			int foodOutlets;     ///< fast food + restaurant
			int securityOffices;
			int medicalCenters;
			int metros;
			int population;
			int criticalTenants; ///< tenants with evaluation < 25 (complaint risk)

			/// Granular capacity/occupancy metrics. Capacity is derived from
			/// the live item model; visitor pools are current occupants by
			/// category and dailyArrivals is deduplicated per simulation day.
			int populationCapacity; ///< sum of worker/resident capacity
			int visitorCapacity;    ///< hotel + commercial capacity
			int currentOccupants;   ///< people currently at an item
			int visitorPool;        ///< all hotel/commercial visitors present
			int hotelVisitorPool;   ///< hotel guests present
			int foodVisitorPool;    ///< fast-food/restaurant visitors present
			int shopVisitorPool;    ///< retail visitors present
			int leisureVisitorPool; ///< cinema/party visitors present
			int dailyArrivals;      ///< visitor arrivals during the current day
			int dailyHotelArrivals;
			int dailyFoodArrivals;
			int dailyShopArrivals;
			int dailyLeisureArrivals;
			int visitorDay;         ///< floor(time.absolute), for pass consumers
			double hotelAvgEval;    ///< mean evaluation across hotels, 0 if none
		};
		const Counts & counts() const { return lastCounts; }

		/// Drop tracking entries for an object that is being destroyed:
		/// the maps are keyed by pointer and SimLess dereferences keys in
		/// deterministic runs, so stale entries would be use-after-free.
		void forget(const Person * p);
		void forget(const Item::Item * item);

	private:
		/// Per-item scoring helpers. Each returns a value in [0, 100].
		double scoreOffice(Game * game, Item::Item * item);
		double scoreCondo(Game * game, Item::Item * item);
		double scoreHotel(Game * game, Item::Item * item);
		double scoreCommercial(Game * game, Item::Item * item);

		/// YootTower 1..300 estimate (plan step 8, J1-J3); 0 when the item
		/// is not a YootTower-evaluated tenant.
		int yootEstimate(Game * game, Item::Item * item);
		/// T2TrafficInfo::GetRange stand-in (0..3) at a tenant's entrance.
		static int yootTrafficRange(Game * game, Item::Item * item);

		/// Aggregate hotel review (JudgeAllHotel equivalent). Returns
		/// true if hotels are underperforming enough to surface a daily
		/// complaint. Also updates lastCounts.hotelAvgEval.
		bool reviewHotels(Game * game);

		/// Per-tenant bad-day tracking (ExpandoBadHotel equivalent).
		/// Bumps a counter for each tenant with evaluation < 25, decays
		/// it otherwise, and returns the list of tenants that have been
		/// unhappy long enough to warrant a targeted complaint.
		void reviewUnderperformers(Game * game);

		/// Shared metrics.
		static double clampScore(double v);

		/// Tower-wide parking coverage in [0, 1+]. Cached per evaluateAll()
		/// pass so the per-item scorers can refer to it cheaply.
		void computeParkingCoverage(Game * game);
		double parkingCoverage;

		Counts lastCounts;

		/// Visitor pass de-duplication: a person contributes once per day even
		/// when evaluateAll is also triggered by a rating/facility change.
		std::map<Person *, int, SimLess> visitorDaySeen;
		/// Item pointer -> consecutive bad days. Used by
		/// reviewUnderperformers to only complain about persistently
		/// unhappy tenants rather than transient dips.
		std::map<Item::Item *, int, SimLess> badDayStreak;
	};
}
