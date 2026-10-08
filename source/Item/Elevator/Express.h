#pragma once
#include "Elevator.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Express : public Elevator {
			public:
				Express(Game * game, AbstractPrototype * prototype) : Elevator(game, prototype) {}
				OT_ITEM_PROTOTYPE(Express) {
					p->id    = "elevator-express";
					p->name  = "Express Elevator";
					p->price = 400000;
					p->carCost = 150000;
					p->size  = int2(6,1);
					p->icon  = 6;
				}
				
				virtual void init()
				{
					shaftBitmap = "simtower/elevator/wide";
					carBitmap   = "simtower/elevator/express";
					Elevator::init();
					maxCarAcceleration = 20;
					if (!prototype->yoot) maxCarSpeed = 30;
				}
				
				virtual bool connectsFloor(int floor) const
				{
					// YootTower Large Elevator: no 15-floor sky-lobby rule; it
					// stops where the player set stops (capped by maxStops()).
					if (prototype->yoot) return Elevator::connectsFloor(floor);
					if(floor < 0) return true;
					else if ((floor % 15) != 0) return false;
					return Elevator::connectsFloor(floor);
				}
			};
		}
	}
}