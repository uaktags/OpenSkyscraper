#pragma once

#include <map>
#include <vector>

namespace OT
{
	class Game;
	class Person;

	namespace Item { class Item; }

	/** Tenant judge (batch 5, J1/J9). Computes the EXE tenant category
	 * (0 stressed, 1 OK, 2 happy, 255 unjudged) from the per-member banked
	 * waits stored on Item::tenant, runs the per-floor daily judge / move-out /
	 * arm passes, immediate rescue on a rent-class change and the noise scan.
	 * Item::evaluation is now only a display value derived from the category.
	 * The pure arithmetic lives in TenantRules.h (unit-tested). */
	class JudgeSystem
	{
	public:
		JudgeSystem();

		/// Refresh the facility counters (cheap; also used by LevelUp).
		void evaluateAll(Game * game);

		/// Daily judge at the 04:59 settlement: one pass per floor (basement
		/// first) judging, then on quarter days move-out + settle, otherwise
		/// the arm pass. Replaces the old tower-wide scoring.
		void judgeDaily(Game * game, bool isQuarterDay);

		/// Judge a single tenant item and store its category (T3/T7/T8).
		/// Returns the stored category.
		int judgeOne(Item::Item * item);

		/// Rejudge after a rent-class change; arms a vacant disarmed unit
		/// whose category is not 0 (T11). Never moves anybody out.
		void rescueOne(Item::Item * item);

		/// Tenant metric after class adjustment, noise and clamp (T3); -1
		/// when the unit is not eligible to be judged.
		int tenantMetric(Item::Item * item);

		/// First noisy neighbour of `item` (T4), or nullptr.
		Item::Item * noiseSource(Item::Item * item);

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
			int criticalTenants; ///< tenants with category 0 (stressed)

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
		bool moveOut(Game * game, Item::Item * item, const std::vector<Item::Item *> & floorTenants);

		/// Per-item scoring helpers (YootTower ruleset). Each returns a
		/// value in [0, 100].
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
