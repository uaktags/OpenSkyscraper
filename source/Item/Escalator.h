/* Copyright © 2012 Fabian Schuiki */
/* Copyright © 2025 Tim G */
#pragma once
#include "Stairlike.h"
#include "Factory.h"

namespace OT {
	namespace Item {
		
		class Escalator : public Stairlike
		{
		public:
			Escalator(Game * game, OT::Item::AbstractPrototype * prototype) : Stairlike(game, prototype) {}
			OT_ITEM_PROTOTYPE(Escalator) {
				p->id    = "escalator";
				p->name  = "Escalator";
				p->price = 20000;
				p->size  = int2(8,2);
				p->icon  = ICON_ESCALATOR;
				p->unlockRating = 3;
				p->toolboxCategory = CAT_TRANSPORT;
				p->toolboxOrder = 2;
			}
			
			virtual void init();
		};
	}
}