#pragma once
#include "Elevator.h"
#include "../Factory.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Standard : public Elevator {
			public:
				Standard(Game * game, AbstractPrototype * prototype) : Elevator(game, prototype) {}
				OT_ITEM_PROTOTYPE(Standard) {
					p->id    = "elevator-standard";
					p->name  = "Standard Elevator";
					p->price = 100000;
					p->size  = int2(4,1);
					p->icon  = ICON_ELEVATOR;
					p->unlockRating = 0;
					p->toolboxCategory = CAT_TRANSPORT;
					p->toolboxOrder = 10;
				}
				
				virtual void init()
				{
					shaftBitmap = "simtower/elevator/narrow";
					carBitmap   = "simtower/elevator/standard";
					Elevator::init();
				}
			};
		}
	}
}