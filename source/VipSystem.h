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
		bool isVisiting() const { return visiting != 0; }

		/// Absolute-time at which the next visit will trigger. 0 if none.
		double nextVisitAt() const { return nextVisitTime; }

		/// Persist / restore across save-reload.
		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		/// Favorable verdicts on hand, consumed by promotion ceremonies.
		int positiveReviews() const { return gameIsYoot() ? goodReviews : (favorable ? 1 : 0); }

		/// Consume exactly one favorable verdict without clearing history.
		void clearApproval();

	private:
		/// YootTower VIEV visits (T2VisitVIP): one reservation attempt per
		/// day at the VIEV begin time, arrival at the VIP's +0x38 minute,
		/// success when the estimate exceeds 200; success sets the VIP's
		/// flag in Game::yootVipFlags.
		void advanceYoot();
		void resolveYootVisit();
		Item::Item * findYootDestination(const std::vector<std::string> & prototypes) const;
		bool liveItem(Item::Item * item) const;

        bool gameIsYoot() const;
        void armVisit();
        bool liveGuest(Person*) const;
        int visiting = 0;
        int lastDay = -1;
        bool watch = false;
        Person *vipGuest = nullptr;
        int vipTotal = 0, vipTrips = 0, result = 0;
        int goodReviews = 0, badReviews = 0; // Yoot path only
        double nextVisitTime = 0;
        int pendingHomeX = 0, pendingHomeY = 0, pendingMember = -1;
    public:
        bool favorable = false;
        void bankTrip(Person*, int wait);
        void bindGuest(Person*);
        void checkout(Person*, bool normal);
        void voidPerson(Person*);
        /// Clear the bound guest if it is this (about to be destroyed) person.
        void forgetPerson(Person*);
        /// Arm the VIP visit from the shared daily judge tick.
        void armDaily();
        void dayCheck();
        /// Re-find the bound guest once items have been decoded.
        void relink();
    private:

		// --- YootTower VIP state ---
		int    yootPendingVip;    ///< index into RulesProfile::vips, -1 = none
		double yootArrivalTime;   ///< absolute time the pending VIP is judged
		int    yootLastDay;       ///< day of the last reservation attempt
		Item::Item * yootDestination;
	};
}
