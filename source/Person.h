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

		/// Subclass hook called every frame. Default is a no-op; subclasses
		/// override to drive state transitions and decide where to go next.
		virtual void advance(double dt) {}

		virtual void encodeXML(tinyxml2::XMLPrinter &xml);
		virtual void decodeXML(tinyxml2::XMLElement &xml);

		Item::Item * at;

		class Journey
		{
		public:
			Journey(Person * p);

			void set(const Route & r);

			void next();
			int fromFloor;
			Item::Item * item;
			int toFloor;

		private:
			Person * const person;
			std::queue<Route::Node> nodes;
		};
		Journey journey;
		Type type;
		State state;
		std::string name;
		std::string from;
		std::string goingTo;
		double stress;
		double eval;

		int getWidth();
	};
}
