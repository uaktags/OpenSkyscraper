#pragma once
#include "Elevator.h"
#include "../Lobby.h"

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
					maxCarSpeed = 30;
				}
				
				virtual bool connectsFloor(int floor) const
				{
					if(floor < 0) return true;
					else if (floor > 0 && !Lobby::isSkyLobbyFloor(floor)) return false;
					return Elevator::connectsFloor(floor);
				}
			};
		}
	}
}