#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <queue>
#include <set>

namespace OT
{
	namespace Item
	{
		class Hotel : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Hotel);
			static AbstractPrototype * makeSinglePrototype();
			static AbstractPrototype * makeDoublePrototype();
			static AbstractPrototype * makeSuitePrototype();
			virtual ~Hotel();

			enum RoomType { kSingle = 0, kDouble = 1, kSuite = 2 };
			enum RoomState { kClean = 0, kOccupied = 1, kDirty = 2 };

			int variant;
			int roomState;
			double dirtySince;
			int capacity() const;

			class Guest : public Person
			{
			public:
				Guest(Hotel *item);
				virtual ~Guest() { LOG(DEBUG, "hotel guest %p killed", this); }

				double arrivalTime;
				double dinnerLeaveTime;
				double dinnerReturnTime;
				double sleepTime;
				double wakeTime;
				double checkoutTime;
				bool atHotel;

				struct laterThan
				{
					bool operator()(const Guest *a, const Guest *b) const
					{
						return (a->arrivalTime > b->arrivalTime);
					}
				};
			};

			class Housekeeper : public Person
			{
			public:
				Housekeeper(Hotel *item);
				virtual ~Housekeeper() { LOG(DEBUG, "hotel housekeeper %p killed", this); }

				double cleaningUntil;
				bool cleaning;
			};

			std::priority_queue<Guest *, std::deque<Guest *>, Guest::laterThan> arrivingGuests;
			std::set<Guest *> guests;
			Housekeeper *housekeeper;

			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter &xml) override;
			virtual void decodeXML(tinyxml2::XMLElement &xml) override;
			virtual void advance(double dt) override;
			virtual int dailyMaintenanceCost() const override { return 600; }

			virtual void addPerson(Person *p) override;
			virtual void removePerson(Person *p) override;

			Sprite sprite;
			bool spriteNeedsUpdate;
			void updateSprite();

			Path getRandomBackgroundSoundPath() override;

		protected:
			void scheduleGuest(Guest *g);
			void clearAll();
			void applyVariant();
		};
	}
}
