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

			/// Per-person atrium walk timer (see advance); mirrors Stairlike.
			std::map<Person *, double, SimLess> transitionTimes;
		};
	}
}
