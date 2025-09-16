/* Copyright © 2012 Fabian Schuiki */
/* Copyright © 2025 Tim G */
#pragma once
#include "Stairlike.h"
#include "Factory.h"

namespace OT {
	namespace Item {
		
		class Stairs : public Stairlike
		{
		public:
			Stairs(Game * game, OT::Item::AbstractPrototype * prototype) : Stairlike(game, prototype) {}
			OT_ITEM_PROTOTYPE(Stairs) {
				p->id    = "stairs";
				p->name  = "Stairs";
				p->price = 5000;
				p->size  = int2(8,2);
				p->icon  = ICON_STAIRS;
				p->unlockRating = 0;
				p->toolboxCategory = CAT_TRANSPORT;
				p->toolboxOrder = 1;
			}
			
			virtual void init();
		};
	}
}