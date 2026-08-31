#pragma once

#include "GameObject.h"
#include <string>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	/** VIP visit system (Phase 3.2).
	 *
	 * Scheduling retuned to the authentic trigger model (OpenSkyWeb
	 * ISSUE-036, docs/research/event-triggers.md): VIP visits are NOT
	 * random. The VIP arrives when the next star promotion requires a VIP
	 * rating and every other requirement is already met. A good review
	 * completes the promotion gate (consumed by Game::ratingMayIncrease);
	 * a bad review schedules a 14-day retry cooldown.
	 *
	 * The visit itself is abstract - we don't spawn a walking Person for
	 * the VIP, just bookkeep a scheduled window. This keeps the system
	 * decoupled from the pathfinder and lets it run even mid-construction.
	 */
	class VipSystem : public GameObject
	{
	public:
		VipSystem(Game * game);

		/// Per-frame hook. Handles countdown to the next scheduled visit
		/// and ends any in-progress visit. Called from Game::advance().
		void advance(double dt);

		/// Force a visit as soon as possible (used by dev keys / tests).
		void scheduleNow();

		/// Reset all state. Called from Game::clearWorld().
		void reset();

		/// Is a VIP currently on the premises?
		bool isVisiting() const { return visiting; }

		/// Absolute-time at which the next visit will trigger. 0 if no
		/// visit is scheduled yet (e.g. fresh tower below the threshold).
		double nextVisitAt() const { return nextVisitTime; }

		/// Persist / restore across save-reload. Called from Game's
		/// encodeXML/decodeXML.
		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		/// Running count of positive reviews. Intended to gate the
		/// Cathedral "Tower of the Year" ending once that lands.
		int positiveReviews() const { return goodReviews; }

	private:
		/// True when the next promotion requires a VIP rating and every
		/// other requirement (population, facilities) is already met — the
		/// condition under which the VIP comes to inspect.
		bool promotionNeedsVisit() const;

		/// Begin a visit: mark visiting, capture the end time, emit an
		/// arrival message.
		void beginVisit();

		/// End a visit: evaluate tower state, emit a result message,
		/// grant any reward, bump goodReviews / badReviews.
		void endVisit();

		double nextVisitTime;   ///< absolute time of next scheduled visit; 0 = none
		double visitEndTime;    ///< absolute time when current visit ends
		bool   visiting;        ///< is a VIP currently on premises?
		int    goodReviews;     ///< lifetime positive reviews
		int    badReviews;      ///< lifetime negative reviews
	};
}
