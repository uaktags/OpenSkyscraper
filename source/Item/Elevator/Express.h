/* Copyright © 2012 Fabian Schuiki */
/* Copyright © 2025 Tim G */
/* Copyright © 2013 globemaster */
#pragma once
#include "Elevator.h"
#include "../Factory.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Express : public Elevator {
			public:
				Express(Game * game, AbstractPrototype * prototype) : Elevator(game, prototype) {}
				OT_ITEM_PROTOTYPE(Express) {
					p->id    = "elevator-express";
					p->name  = "Express Elevator";
					p->price = 1000000;
					p->size  = int2(6,1);
					p->icon  = ICON_EXPRESS_ELEVATOR;
					p->unlockRating = 3;
					p->toolboxCategory = CAT_TRANSPORT;
					p->toolboxOrder = 12;
				}
				
				virtual void init()
				{
					shaftBitmap = "simtower/elevator/wide";
					carBitmap   = "simtower/elevator/express";
					Elevator::init();
					maxCarAcceleration = 20;
					maxCarSpeed = 30;
				}
				
				virtual bool connectsFloor(int floor) const
				{
					if(floor < 0) return true;
					else if ((floor % 15) != 0) return false;
					return Elevator::connectsFloor(floor);
				}
			};
		}
	}
}