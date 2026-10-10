#pragma once
#include <set>
#include "../../GameObject.h"
#include "../../Sound.h"
#include "Elevator.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Queue;
			class Car : public GameObject, public sf::Drawable
			{
				sf::Vector2f position;

			public:
				Elevator * const elevator;
				Car(Elevator * elevator) : GameObject(elevator->game), elevator(elevator) { altitude = elevator->position.y; init(); }

				virtual void init();

				double altitude;
				void setAltitude(double a);
				void reposition();

				void setPosition(float x, float y) { position = sf::Vector2f(x,y); }

				Sprite sprite;
				void updateSprite();

				Item::People passengers;
				void removePassenger(Person *p);

				void draw(sf::RenderTarget & target, sf::RenderStates states) const;
				virtual void render(sf::RenderTarget & target) const;

				virtual void encodeXML(tinyxml2::XMLPrinter& xml);
				virtual void decodeXML(tinyxml2::XMLElement& xml);

				virtual void advance(double dt);

				Elevator::Direction direction;
				int destinationFloor;
				int homeFloor;
				int index = 0; // stable creation order for dispatch ties
				int shuttleMode = 0;
				bool shuttleReturning = false;

				typedef enum {
					kIdle,
					kMoving,
					kOpeningDoors,
					kHauling,
					kClosingDoors
				} State;
				State state;
				void setState(State s);

				double startAltitude;
				double journeyTime;

				/// Returns the raw tail delay and the number of floors to
				/// advance for one simulation tick (CalcMoveSpeed).
				static int CalcMoveSpeed(bool express, int distanceToTarget,
				                         int distanceFromStart, int *stride);

				Sound arrivingSound;
				Sound departingSound;
				bool arrivingPlayed;
				bool departingPlayed;

				bool isFull();
				Person * nextPassengerToUnmount() const;

			void moveTo(int floor);
			void yootDrive();
			/// YootTower dispatch (StdElev FUN_1000cf90/cda0/d070): the
			/// floors this car is appointed to pick up (SetOnStop), the
			/// sweep cost of serving a call, the sweep decision
			/// (SetNextStop/ChangeTurnPos) and the end-of-window unappoint
			/// (ClearOnStop).
			std::set<int> yootPickups;
			int yootSweepCost(int floor, Elevator::Direction dir) const;
			void yootDecide();
			void yootEndStop();
			Queue *yootServedQueue();

				/// Door timer is the authentic five-tick countdown. The
				/// legacy State values remain serialized-compatible.
				int doorTicks;
				int holdTicks;
				int moveTicks;
				/// YootTower StdElev/BigElev drive (FUN_1000c200/c3b0/c590):
				/// the gear 0..2 (MVDF speed index) and the car's offset
				/// inside a floor, 36 units a floor.
				int yootGear;
				int yootOffset;
				/// YootTower door cycle (StdElev/BigElev module states 1-4, FUN_1000c150 /
				/// c2c0 / c540): 0 none, 1 unload (state 1/2), 2 board (state 3), 3 hold
				/// (state 4), each with its idle counter (6 on entry, the hold counts
				/// down from the mover's waitCount, which runs as 6).
				int yootPhase = 0;
				int yootCounter = 0;
				void yootArrive();
				void yootDoorStep();
				void yootDepart();

			};
		}
	}
}
