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
