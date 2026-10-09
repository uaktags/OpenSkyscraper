#pragma once
#include "../Item.h"

namespace OT {
	namespace Item {
		namespace Elevator {
			class Car;
			class Queue;


			enum ScheduleMode {
				kScheduleFull = 0,
				kScheduleExpressUp = 1,
				kScheduleExpressDown = 2
			};

			struct SchedulePeriod {
				int mode;
				int threshold;
				int patience;
			};
			class Elevator : public Item
			{
			public:
			/// Elevator groups and cars are bounded by the original
			/// ElevatorsT tables and MakeElevator checks.
			static const int kMaxShafts = 24;
			static const int kMaxCarsPerShaft = 8;
			static const int kMaxElevatorShafts = kMaxShafts;
			static const int kNonExpressMaxSpan = 30;
			static const int kShaftClearanceCells = 8;
			static const int kMetroClearanceFloors = 1;
			static const int kScheduleDays = 2;
			static const int kSchedulePeriods = 6;
			static const int kSchedulePeriodCount = 7; // sixth editable + hidden night slot
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

				typedef enum {
					kUp   = 1,
					kNone = 0,
					kDown = -1
				} Direction;

				/// Per-group [weekday/weekend][period] dispatch settings.
				/// The seventh slot is the hidden night period retained for
				/// compatibility with the original table.
				SchedulePeriod schedules[kScheduleDays][kSchedulePeriodCount];
				static int schedulePeriod(double absolute);
				int scheduleMode() const;
				bool hasWork(const Car *car) const;
				void finishStop(Car *car);
				int scheduleThreshold() const;
				int schedulePatienceTicks() const;
				void resetSchedules();
				bool isExpress() const { return prototype->icon == 6; }
				bool isService() const { return prototype->icon == 5; }
				bool canAddCar(int floor) const;
				bool canResizeTo(int bottom, int height) const;
				void reassignCalls(Car *car);

				/// Shaft visibility toggle ("Show: Yes/No" in the elevator
				/// dialog). When false the shaft interior is transparent.
				bool showShaft = true;

				typedef std::set<Car *> Cars;
				Cars cars;
				void clearCars();
				virtual void evacuateForDisaster() override;
				void addCar(int floor);


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
