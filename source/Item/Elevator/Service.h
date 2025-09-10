#pragma once
#include "Elevator.h"
#include "../Factory.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Service : public Elevator {
			public:
				Service(Game * game, AbstractPrototype * prototype) : Elevator(game, prototype) {}
				OT_ITEM_PROTOTYPE(Service) {
					p->id    = "elevator-service";
					p->name  = "Service Elevator";
					p->price = 80000;
					p->size  = int2(4,1);
					p->icon  = ICON_SERVICE_ELEVATOR;
					p->unlockRating = 2;
					p->toolboxCategory = CAT_TRANSPORT;
					p->toolboxOrder = 11;
				}
				
				virtual void init()
				{
					shaftBitmap = "simtower/elevator/narrow";
					carBitmap   = "simtower/elevator/service";
					Elevator::init();
				}
			};
		}
	}
}