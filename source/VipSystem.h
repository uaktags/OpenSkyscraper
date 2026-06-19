#pragma once

#include "GameObject.h"
#include <string>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	/** Periodic VIP visit system (Phase 3.2).
	 *
	 * Mirrors the role of VIP arrivals in the original SimTower
	 * (codemap.md:2653). VIPs visit the tower at random intervals once it
	 * has some momentum, evaluate overall quality (cleanliness, tenant
	 * satisfaction, facility coverage) and either reward the player or
	 * lodge a complaint. Sustained good VIP reviews are intended to feed
	 * the eventual "Tower of the Year" Cathedral ending (TODO).
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
		/// Schedule the next visit a random number of days out. Only
		/// fires once the tower is established (rating >= 1 + population
		/// threshold). No-op otherwise - nextVisitTime stays 0.
		void scheduleNextVisit();

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
