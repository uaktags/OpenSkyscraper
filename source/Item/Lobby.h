#pragma once
#include <map>
#include "../Sprite.h"
#include "Item.h"
namespace OT {
	namespace Item {
		class Lobby : public Item
		{
		public:
			static bool isSkyLobbyFloor(int y) { return y > 0 && y % 15 == 14; }
			OT_ITEM_CONSTRUCTOR(Lobby);
			OT_ITEM_PROTOTYPE(Lobby) {
				p->id    = "lobby";
				p->name  = "Lobby";
				p->price = 5000;
				p->size  = int2(4,1);
				p->icon  = 0;
			}
			virtual ~Lobby();

			virtual void init();
			virtual void advance(double dt);

			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			Sprite background;
			Sprite overlay;
			Sprite entrances[2];
			void updateSprite();
			virtual void render(sf::RenderTarget & target) const;
			virtual void addPerson(Person * p);
			virtual void removePerson(Person * p);
			virtual bool isLobby() const override { return true; }

			/// Rendering-only grouping of imported 1-story lobby slices: the
			/// TDT stores a grand lobby as one record per story. Returns this
			/// slice's story `index` (0 = lowest) inside a stack of identical-
			/// extent slices on consecutive floors, and the stack's story
			/// `total` (at most 3). A natively built tall lobby reports
			/// (0, size.y). Routing and simulation keep the slices separate.
			void stackPosition(int & index, int & total) const;

			/// Per-person atrium walk timer (see advance); mirrors Stairlike.
			std::map<Person *, double, SimLess> transitionTimes;
		};
	}
}
