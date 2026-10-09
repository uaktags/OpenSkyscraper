#pragma once
#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <tinyxml2.h>
#include "GameObject.h"
#include "Route.h"

namespace OT {
	namespace Item { class Item; }

	class Person : public GameObject
	{
	public:
		typedef enum {
			kMan = 0,
			kSalesman,
			kWoman1,
			kChild,
			kWoman2,
			kHousekeeper,
			kWomanWithChild1,
			kWomanWithChild2,
			kSecurity
		} Type;

		/// Activity state of a Person. The state machine drives daily routines:
		/// a condo occupant cycles kHome -> kCommuting -> kWorking -> kReturning
		/// -> kHome; an office worker omits kHome. Subclasses advance() drive the
		/// transitions; Person::advance() handles journey retries and recovery.
		typedef enum {
			kWandering = 0,
			kHome,
			kCommuting,
			kWorking,
			kLunch,
			kShopping,
			kReturning,
			kResting,
			kIdle,
			kPlanning
		} State;

		Person(Game * game, Type type = kMan);
		virtual ~Person();

		/// Subclass hook called every frame. Default recovers stress when
		/// resting or at home; subclasses override to drive state transitions.
		virtual void advance(double dt);

		/// Felt-wait stress is a bounded bank in the original 0..300 scale.
		/// Keep the field public for existing inspector/evaluation consumers,
		/// but funnel mutations through this clamp.
		static const double kMaxStress;
		void addStress(double amount);
		void recoverStress(double amount) { addStress(-amount); }

		/// Queue and movement penalties are accumulated for the active trip and
		/// deposited once when Journey reaches its final node.
		void addJourneyStress(double amount);
		void deliverJourneyStress();
		/// Bank the finished trip's pending wait (batch 5 T2): count++ / total +=
		/// round(pending) with EXE int8/int16 wrap, written through to the
		/// owner's tenant record. Housekeepers and security never bank.
		void bankTrip();
		/// Individual mean of banked trips (signed, truncated), 0 without trips.
		int bankedMean() const;
		int8_t  bankCount = 0;
		int16_t bankTotal = 0;
		Item::Item * home = nullptr; ///< owning tenant (null for visitors/staff)
		int8_t member = -1;          ///< member slot on the owner record
		double journeyStress() const { return pendingJourneyStress; }
		bool isServicePerson() const { return type == kHousekeeper || type == kSecurity; }
		double waitStartFrame = 0;

		/// Record an unreachable destination. The stress hit is immediate; the
		/// diagnostic itself is latched per game/floor pair.
		void recordNoRoute(int fromFloor, int toFloor);

		/// True under the YootTower profile (stress model P1/P2).
		bool yootRules() const;
		/// T2People::IdleWaitMover 0x10048880 for one people-idle tick (2
		/// game minutes) in a mover queue: +5 stress; at 300 the person
		/// gives up, jumps to the destination and loses 50 estimate.
		/// Returns true when the person gave up.
		bool yootWaitTick();

		virtual void encodeXML(tinyxml2::XMLPrinter &xml);
		virtual void decodeXML(tinyxml2::XMLElement &xml);

		Item::Item * at;

		class Journey
		{
		public:
			Journey(Person * p);

			void set(const Route & r);

			void next();
			void retry();
			void queueRejected();
			void queueAccepted();
			void failWaiting();
			/// True once the person stands in the final non-transport node of the
			/// current route (the trip is complete and already banked).
			bool atFinalNode() const;
			/// A journey banks exactly once (F3).
			bool banked = false;
			Item::Item *waitingAt = nullptr;
			bool retryPending = false;
			double retryFrame = 0;
			State retryState = kWandering;
			void cancel();
			bool references(const Item::Item * target) const;
			/// T2People::JumpToDestination: leave the current mover and
			/// arrive at the final node directly.
			void jumpToDestination();
			/// Drop the remaining nodes without arriving anywhere.
			void clear();
			/// The next node is the end of the route.
			bool atLastNode() const { return nodes.size() == 1; }
			int fromFloor;
			Item::Item * item;
			int toFloor;

		private:
			Person * const person;
			std::queue<Route::Node> nodes;
			Item::Item *destination = nullptr;
			int destinationFloor = 0;
			bool replanPending = false;
		};
		Journey journey;
		double stress;
		double eval;
		/// YootTower people estimate (T2People +0x84, 1..300) and completed
		/// trips (+0x80, max 4); plan step 8 (P2).
		int yootEstimate;
		int yootTrips;
		/// T2People::CalcCurrEstimate with the current stress.
		int yootCurrentEstimate() const;
		/// T2People::UpdateEstimation at the end of a trip.
		void yootFinishTrip();
		/// T2People special flags (+0x78): Yoot::kPeopleNeedsToilet,
		/// Yoot::kPeopleSick (plan step 12, P5/P6).
		int yootFlags = 0;
		/// Arrival at the end of a trip (T2Tenant::EnterTenant hooks).
		void yootArrived(Item::Item * destination);
		/// YootTower visitors walk along `floor` to the next route node
		/// before they enter it (T2People::IsReachDestination); true when
		/// the person walks and enters the node on arrival.
		virtual bool yootWalkTo(Item::Item * next, int floor) { (void)next; (void)floor; return false; }
		double pendingJourneyStress;
		Type type;
		State state;
		std::string name;
		std::string from;
		std::string goingTo;

		/// Random per-person offset so walking animations don't sync.
		double animOffset;

		int getWidth();
	};
}
