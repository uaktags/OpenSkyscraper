#pragma once
#include "Item.h"
#include "../YootItemArt.h"

namespace OT {
	namespace Item {
		
		class Stairlike : public Item
		{
		public:
			Stairlike(Game * game, OT::Item::AbstractPrototype * prototype) : Item(game, prototype) {}
			
			virtual void init();
			
Sprite sprite;
int frame;
			int frameCount;
			/// Skin::Yoot: Stair.t2p / Escalator.t2p View by span (2..4
			/// storeys); the escalator's 2-storey view has 3 step patterns.
			const YootPluginView * yootView = nullptr;
			int yootPattern = 0;
			
			void updateSprite();
			virtual void advance(double dt);
			
		virtual bool canHaulPeople() const { return true; }
		virtual bool connectsFloor(int floor) const;
		virtual bool isStairlike() const { return true; }

		/// Hit-test against the actual sprite bounds (a narrow strip at
		/// the left edge of the tile footprint), not the full width.
		/// Clicks outside the sprite pass through to the tenant behind.
		virtual bool containsPoint(const double2 & pt);
			
			std::map<Person *, double, SimLess> transitionTimes;
			virtual void addPerson(Person * p);
			virtual void removePerson(Person * p);
		};
	}
}
