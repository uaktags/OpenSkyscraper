#pragma once
#include <cstddef>
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
		/// transitions; Person::advance() is the no-op default.
		typedef enum {
			kWandering = 0,
			kHome,
			kCommuting,
			kWorking,
			kLunch,
			kShopping,
			kReturning,
			kResting,
			kIdle
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
		double journeyStress() const { return pendingJourneyStress; }

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
			/// T2People::JumpToDestination: leave the current mover and
			/// arrive at the final node directly.
			void jumpToDestination();
			int fromFloor;
			Item::Item * item;
			int toFloor;

		private:
			Person * const person;
			std::queue<Route::Node> nodes;
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
