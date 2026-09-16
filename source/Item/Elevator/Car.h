#pragma once
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

				/// Door timer is the authentic five-tick countdown. The
				/// legacy State values remain serialized-compatible.
				int doorTicks;
				int holdTicks;
				int moveTicks;

			};
		}
	}
}
