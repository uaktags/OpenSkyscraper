/* Copyright © 2013 Fabian Schuiki */
/* Copyright © 2025 Tim G */
/* Copyright © 2013 hoshi10 */
#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "Factory.h"

namespace OT {
	namespace Item {
		class Metro : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Metro);
			OT_ITEM_PROTOTYPE(Metro) {
				p->id    = "metro";
				p->name  = "Metro Station";
				p->price = 1000000;
				p->size  = int2(30,3);
				p->icon  = ICON_METRO;
				p->unlockRating = 5; // unlocked at 5 stars
				p->entrance_offset = 2;
				p->exit_offset = 2;
				p->toolboxCategory = CAT_TRANSPORT;
				p->toolboxOrder = 20;
			}
			virtual ~Metro();
			
			virtual void init();
			
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			
			bool open;
			bool trainPresent;
			
			Sprite station;
			Sprite platform;
			bool spriteNeedsUpdate;
			void updateSprite();
			
			virtual void advance(double dt);
		};
	}
}