#pragma once
#include <list>
#include <SFML/Graphics/Drawable.hpp>
#include "../../GameObject.h"
#include "../../Math/Rect.h"
#include "Elevator.h"

namespace OT {
	class Person;

	namespace Item {
		namespace Elevator {
			class Elevator;
			class Queue : public GameObject, public sf::Drawable
			{
			public:
				Elevator * const elevator;

				Queue(Elevator * e);
				~Queue();

				int floor;
				Elevator::Direction direction;
				int width;

				static const int kQueueCapacity = 40;
				static const int kQueueFullPenalty = 5;
				static const int kQueueFullRetryFrames = 16;
				static const int kQueueWatchdogFrames = 300;

				bool called;
				double callTime;
				bool answered;
				double getWaitDuration();

				typedef std::list<Person *> People;
				People people;
				void addPerson(Person * p);
				void removePerson(Person * p);
				Person * popPerson();
				void callElevator();

				bool steppingInside;

				virtual void advance(double dt);
				void draw(sf::RenderTarget & target, sf::RenderStates states) const;
				virtual void render(sf::RenderTarget & target) const;

			private:
				double fullRetryFrame;
				double waitStartFrame;
				/// YootTower: last people-idle tick (absolute minute / 2) that
				/// charged wait stress.
				long long yootIdleTick = -1;
				void applyWatchdog();
			};
		}
	}
}
