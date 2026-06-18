#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>

namespace OT {
	namespace Item {
		class Metro : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Metro);
			OT_ITEM_PROTOTYPE(Metro) {
				p->id    = "metro";
				p->name  = "Metro Station";
				p->price = 1000000;
				p->size  = int2(30,3);
				p->icon  = 19;
				p->entrance_offset = 2;
				p->exit_offset = 2;
			}
			virtual ~Metro();

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			bool open;
			bool trainPresent;

			/** A Person who arrived at the tower via the metro. The Metro item
			 *  drives the lifecycle: spawn on train arrival, send to a random
			 *  reachable commercial venue, return after a dwell period, and
			 *  delete when the next train boards waiting passengers. */
			class Visitor : public Person
			{
			public:
				Visitor(Metro *m);
				virtual ~Visitor() { LOG(DEBUG, "metro visitor %p killed", this); }

				double arrivalTime;     ///< absolute time the visitor entered the tower
				double departTime;      ///< absolute time the visitor should head back
				bool   returnedToMetro; ///< true once the visitor is back at the platform

				struct hasEarlierReturn
				{
					bool operator()(const Visitor *a, const Visitor *b) const
					{
						return (a->departTime > b->departTime);
					}
				};
			};

			std::set<Visitor *> visitors;

			Sprite station;
			Sprite platform;
			bool spriteNeedsUpdate;
			void updateSprite();

			virtual void advance(double dt);
			virtual int dailyMaintenanceCost() const override { return 5000; }

			virtual void addPerson(Person *p) override;
			virtual void removePerson(Person *p) override;

		private:
			/// Absolute time at which the next train event (arrival if the
			/// train is currently absent, departure otherwise) is due.
			double nextTrainTime;

			void spawnVisitors(int count);
			void boardReturnedVisitors();
			void clearAllVisitors();
			Item * pickDestinationFor(Visitor *v);
			void scheduleNextTrain();
		};
	}
}
