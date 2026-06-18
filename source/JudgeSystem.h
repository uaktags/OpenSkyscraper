#pragma once

namespace OT
{
	class Game;

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
		struct Counts
		{
			int offices;
			int condos;
			int hotels;
			int hotelsDirty;
			int foodOutlets;     ///< fast food + restaurant
			int securityOffices;
			int medicalCenters;
			int population;
		};
		const Counts & counts() const { return lastCounts; }

	private:
		/// Per-item scoring helpers. Each returns a value in [0, 100].
		double scoreOffice(Game * game, Item::Item * item);
		double scoreCondo(Game * game, Item::Item * item);
		double scoreHotel(Game * game, Item::Item * item);
		double scoreCommercial(Game * game, Item::Item * item);

		/// Shared metrics.
		static double clampScore(double v);

		Counts lastCounts;
	};
}
