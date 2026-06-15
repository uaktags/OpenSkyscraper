/* Copyright © 2013 Fabian Schuiki */
#pragma once
#include "Item.h"
#include <queue>
#include <set>

namespace OT
{
	namespace Item
	{
		class Office : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Office);
			OT_ITEM_PROTOTYPE(Office)
			{
				p->id = "office";
				p->name = "Office";
				p->price = 40000;
				p->size = int2(9, 1);
				p->icon = 7;
			}
			virtual ~Office();

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter &xml) override;
			virtual void decodeXML(tinyxml2::XMLElement &xml) override;

			virtual void advance(double dt) override;
			virtual int dailyMaintenanceCost() const override { return 250; }
			bool isAttractive();

			virtual void addPerson(Person *p) override;
			void prepareLunchQa(double lunchHour);

			Path getRandomBackgroundSoundPath() override;

			/**
			 * A Person working in an Office item. The class maintains
			 * additional information of the worker, such as the day's schedule
			 * and the like.
			 */
			class Worker : public Person
			{
			public:
				Worker(Office *item, Person::Type type) : Person(item->game, type) {}

				/// When the worker is supposed to arrive at the tower in the morning.
				double arrivalTime;
				/// When the worker is supposed to leave the tower in the evening.
				double departureTime;

				/// In case of a salesman, stores when he will leave the tower
				/// to go on his sales trip.
				double leaveForSalesTime; // absolute
				/// In case of a salesman, stores when he will return from his
				/// sales trip.
				double returnFromSalesTime; // absolute

				/// In case of a regular worker, stores when the worker is
				/// supposed to have lunch.
				double lunchTime;
				double lunchReturnTime;

				struct arrivesLaterThan
				{
					bool operator()(const Worker *a, const Worker *b) const
					{
						return (a->arrivalTime > b->arrivalTime);
					}
				};
				struct departsLaterThan
				{
					bool operator()(const Worker *a, const Worker *b) const
					{
						return (a->departureTime > b->departureTime);
					}
				};
				struct lunchLaterThan
				{
					bool operator()(const Worker *a, const Worker *b) const
					{
						return (a->lunchTime > b->lunchTime);
					}
				};
				struct lunchReturnLaterThan
				{
					bool operator()(const Worker *a, const Worker *b) const
					{
						return (a->lunchReturnTime > b->lunchReturnTime);
					}
				};
				struct leaveForSalesLaterThan
				{
					bool operator()(const Worker *a, const Worker *b) const
					{
						return (a->leaveForSalesTime > b->leaveForSalesTime);
					}
				};
				struct returnFromSalesLaterThan
				{
					bool operator()(const Worker *a, const Worker *b) const
					{
						return (a->returnFromSalesTime > b->returnFromSalesTime);
					}
				};
			};

		protected:
			void updateSprite();
			void rescheduleWorkers();
			bool findLunchRoute(Route &route);

			int rent;
			int rentDeposit;
			bool occupied;
			int variant;
			bool lit;

			Sprite sprite;
			bool spriteNeedsUpdate;
			typedef std::set<Worker *> Workers;
			Workers workers;

			std::priority_queue<Worker *, std::deque<Worker *>, Worker::arrivesLaterThan> arrivalQueue;
			std::priority_queue<Worker *, std::deque<Worker *>, Worker::departsLaterThan> departureQueue;
			std::priority_queue<Worker *, std::deque<Worker *>, Worker::lunchLaterThan> lunchQueue;
			std::priority_queue<Worker *, std::deque<Worker *>, Worker::lunchReturnLaterThan> lunchReturnQueue;
			std::priority_queue<Worker *, std::deque<Worker *>, Worker::leaveForSalesLaterThan> salesLeaveQueue;
			std::priority_queue<Worker *, std::deque<Worker *>, Worker::returnFromSalesLaterThan> salesReturnQueue;

			Workers workersToArrive;
			Workers workersToDepart;
			Workers workersToHaveLunch;
		};
	}
}
