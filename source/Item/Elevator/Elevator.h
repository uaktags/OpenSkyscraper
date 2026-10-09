#pragma once
#include "../Item.h"
#include "../../YootItemArt.h"

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
			static const int kNonExpressMaxSpan = 29;
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

				/// Skin::Yoot art of the linked mover plugin (StdElev/BigElev/
				/// ServElev...): RT_BITMAP 1000 View 0 shaft, 1 machine room,
				/// 2 pit; RT_BITMAP 200 Box 0..4 cars. Null = OpenSky art.
				const YootPluginView * yootShaft = nullptr;
				const YootPluginView * yootCars = nullptr;

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
				static int schedulePeriod(double hour);
				bool scheduleAllows(Direction direction) const;
				int scheduleThreshold() const;
				int schedulePatienceTicks() const;
				void resetSchedules();
				bool isExpress() const { return prototype->icon == 6; }
				bool isService() const { return prototype->icon == 5; }
				bool canAddCar(int floor) const;
				bool canResizeTo(int bottom, int height) const;

				/// YootTower MVDF limits (prototype->yoot, plan step 4); without
				/// a catalog entry they return the SimTower constants.
				/// maxSpan: 0 = unlimited (Large / Service Elevator).
				int maxSpan() const;
				int maxCars() const;
				/// Stop cap (Large Elevator maxStop 8); 0 = every floor.
				int maxStops() const;
				int servicedFloorCount() const;
				/// False when enabling one more stop would exceed maxStops().
				bool canAddStop() const;
				void reassignCalls(Car *car);

				/// Shaft visibility toggle ("Show: Yes/No" in the elevator
				/// dialog). When false the shaft interior is transparent.
				bool showShaft = true;

				typedef std::set<Car *, SimLess> Cars;
				Cars cars;
				void clearCars();
				void addCar(int floor);
				virtual int dailyMaintenanceCost() const override { return 100 + (int)cars.size() * 250 + size.y * 20; }


				virtual bool canHaulPeople() const { return true; }
				virtual bool connectsFloor(int floor) const;
				virtual bool isElevator() const { return true; }

				virtual void addPerson(Person * p);
				virtual void removePerson(Person * p);

			typedef std::set<Queue *, SimLess> Queues;
			Queues queues;
			Queue * getQueue(int floor, Direction dir);
			/// Lookup without creating (getQueue creates on miss).
			Queue * findQueue(int floor, Direction dir) const;
			void cleanQueues();

			void called(Queue * queue);
			void respondToCalls();
			Queue * getMostUrgentQueue();
			Car * getIdleCar(int floor);

			void decideCarDestination(Car * car);

			/// YootTower appointment dispatch (StdElev FUN_1000cf90 ->
			/// FUN_1000cda0): every waiting, unappointed call is given to
			/// the car with the least sweep cost, scanning the cars from a
			/// rotating index so equal cars share the load.
			void yootAppoint();
			int yootDispatchCursor = 0;
		};
		}
	}
}
