/* Copyright © 2012-2015 Fabian Schuiki */
/* Copyright © 2025 Tim G */
#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "Factory.h"

namespace OT {
	namespace Item {
		class Lobby : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Lobby);
			OT_ITEM_PROTOTYPE(Lobby) {
				p->id    = "lobby";
				p->name  = "Lobby";
				p->price = 20000;
				p->size  = int2(4,1);
				p->icon  = ICON_LOBBY;
				p->toolboxCategory = CAT_CONSTRUCTION;
				p->toolboxOrder = 1;
			}
			virtual ~Lobby();

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			Sprite background;
			Sprite overlay;
			Sprite entrances[2];
			void updateSprite();

			virtual void render(sf::RenderTarget & target) const;
		};
	}
}