#pragma once

#include "GameObject.h"
#include <string>
#include <vector>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	namespace Item { class Hotel; class Item; }
	class Person;

	/** Authentic VIP visit state machine.
	 *
	 * At exactly three stars, every ninth-day schedule arms a real suite
	 * guest.  The guest must arrive in a parked car; the guest's own banked
	 * stress is judged at checkout and a favorable result supplies one
	 * consumable 3->4 approval.
	 */
	class VipSystem : public GameObject
	{
	public:
		VipSystem(Game * game);

		/// Per-frame hook. Handles deterministic arming, arrival and checkout.
		void advance(double dt);

		/// Force a visit as soon as possible (used by dev keys / tests).
		void scheduleNow();

		/// Reset all state. Called from Game::clearWorld().
		void reset();

		/// Is a VIP currently on the premises?
		bool isVisiting() const { return visiting; }

		/// Absolute-time at which the next visit will trigger. 0 if none.
		double nextVisitAt() const { return nextVisitTime; }

		/// Persist / restore across save-reload.
		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		/// Favorable verdicts on hand, consumed by promotion ceremonies.
		int positiveReviews() const { return goodReviews; }

		/// Consume exactly one favorable verdict without clearing history.
		void clearApproval();

	private:
		/// YootTower VIEV visits (T2VisitVIP): one reservation attempt per
		/// day at the VIEV begin time, arrival at the VIP's +0x38 minute,
		/// success when the estimate exceeds 200; success sets the VIP's
		/// flag in Game::yootVipFlags.
		void advanceYoot();
		void resolveYootVisit();
		/// 1..300 estimate the VIP verdict compares with 200.
		int yootVisitEstimate(Item::Item * dest) const;
		Item::Item * findYootDestination(const std::vector<std::string> & prototypes) const;
		bool liveItem(Item::Item * item) const;

		bool promotionNeedsVisit() const;
		void armVisit();
		void beginVisit();
		void advanceGuest();
		void endVisit(bool favorable);
		Item::Hotel * firstBookableSuite() const;
		bool liveSuite(Item::Hotel * suite) const;
		bool liveGuest(Person * guest) const;

		double nextVisitTime;   ///< absolute time of the next 17:00 arm
		double visitEndTime;    ///< fallback timeout for a malformed stay
		bool   visiting;        ///< a scheduled VIP stay is active
		bool   forcedVisit;     ///< scheduleNow() bypasses the calendar gate
		bool   guestArrived;    ///< a suite guest claimed a car space
		bool   guestCheckedIn;  ///< the selected guest reached the suite
		bool   visitVoided;     ///< failed parking/route voided this stay
		int    armedDay;        ///< day already armed; prevents per-frame re-arm
		int    suiteFloor;      ///< reservation dialog floor, 0 when unavailable
		double stressTotal;     ///< selected guest's banked felt-wait stress
		int    stressTrips;     ///< number of banked stress samples
		Item::Hotel * bookedSuite;
		Person * vipGuest;
		int    goodReviews;
		int    badReviews;

		// --- YootTower VIP state ---
		int    yootPendingVip;    ///< index into RulesProfile::vips, -1 = none
		double yootArrivalTime;   ///< absolute time the pending VIP is judged
		int    yootLastDay;       ///< day of the last reservation attempt
		Item::Item * yootDestination;
		/// The saved reservation's destination (prototype id and cell). Set
		/// with yootDestination; a loaded reservation resolves it once.
		std::string yootDestType;
		int    yootDestX, yootDestY;
		void resolveSavedDestination();
	};
}
