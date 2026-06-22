#pragma once
#include "../Item.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Car;
			class Queue;

			class Elevator : public Item
			{
			public:
				Elevator(Game * game, AbstractPrototype * prototype) : Item(game, prototype) {}
				virtual ~Elevator();

				virtual void init();

				std::string shaftBitmap;
				std::string carBitmap;
				double maxCarAcceleration;
				double maxCarSpeed;
				unsigned int maxCarCapacity;

				double animation;
				int frame;

				Sprite shaft;
				Sprite topMotor;
				Sprite bottomMotor;

				void updateSprite();
				virtual void render(sf::RenderTarget & target) const;
				virtual void advance(double dt);

				virtual void encodeXML(tinyxml2::XMLPrinter& xml);
				virtual void decodeXML(tinyxml2::XMLElement& xml);

			virtual rectd getMouseRegion() override;
			virtual bool containsPoint(const double2 & pt) override;
			bool repositionMotor(int motor, int y);
			std::set<int> unservicedFloors;

			/// Shaft visibility toggle ("Show: Yes/No" in the elevator
			/// dialog). When false the shaft interior is transparent —
			/// only the left/right border lines, the cars, and the motor
			/// gearboxes are drawn, so tenants behind the shaft show
			/// through. Mirrors SimTower's elevator Show toggle.
			bool showShaft = true;

				typedef std::set<Car *> Cars;
			Cars cars;
			void clearCars();
			void addCar(int floor);
			virtual int dailyMaintenanceCost() const override { return 100 + (int)cars.size() * 250 + size.y * 20; }

			typedef enum {
					kUp   = 1,
					kNone = 0,
					kDown = -1
				} Direction;

				virtual bool canHaulPeople() const { return true; }
				virtual bool connectsFloor(int floor) const;
				virtual bool isElevator() const { return true; }

				virtual void addPerson(Person * p);
				virtual void removePerson(Person * p);

				typedef std::set<Queue *> Queues;
				Queues queues;
				Queue * getQueue(int floor, Direction dir);
				void cleanQueues();

				void called(Queue * queue);
				void respondToCalls();
				Queue * getMostUrgentQueue();
				Car * getIdleCar(int floor);

				void decideCarDestination(Car * car);
			};
		}
	}
}
