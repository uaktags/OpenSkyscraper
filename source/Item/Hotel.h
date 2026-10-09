#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "../YootItemArt.h"
#include <queue>
#include <set>

namespace OT
{
	namespace Item
	{
		class Parking;

		class Hotel : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Hotel);
			static AbstractPrototype * makeSinglePrototype();
			static AbstractPrototype * makeDoublePrototype();
			static AbstractPrototype * makeSuitePrototype();
			virtual ~Hotel();

			enum RoomType { kSingle = 0, kDouble = 1, kSuite = 2 };
			enum RoomState { kClean = 0, kOccupied = 1, kDirty = 2, kInfested = 3 };
			// kInfested: cockroach frames (sheet cols 7/8); the hotel
			// lifecycle arms it (wave S) — render support only here.

			int variant;
			int subVariant;
			int yootKind = 0; ///< Skin::Yoot View kind roll (Yoot::roomKindRoll)
			int roomState;
			double dirtySince;
			int capacity() const;

			class Guest : public Person
			{
			public:
				Guest(Hotel *item);
				virtual ~Guest() { LOG(DEBUG, "hotel guest %p killed", this); }

				Hotel *hotel;
				double arrivalTime;
				double dinnerLeaveTime;
				double dinnerReturnTime;
				double sleepTime;
				double wakeTime;
				double checkoutTime;
				bool atHotel;

				/// Parking slot the guest's car occupies during their
				/// stay (NULL if no reachable parking had space). Freed
				/// on checkout.
				Parking * parkingUsed = nullptr;

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
			std::set<Guest *, SimLess> guests;
			Housekeeper *housekeeper;

			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter &xml) override;
			virtual void decodeXML(tinyxml2::XMLElement &xml) override;
			virtual void advance(double dt) override;
			virtual int dailyMaintenanceCost() const override { return 600; }

			virtual void addPerson(Person *p) override;
			virtual void removePerson(Person *p) override;

			/// YootTower (plan step 14): the stay's check-in is booked on the
			/// linked Front Desk once; dirty rooms wait for a Housekeeping maid.
			bool yootStayBooked = false;
			/// A saved stay (yootStayStatus / yootStayParty) read by decodeXML and
			/// restored by PeopleSystem::roomIdle on the first idle tick after load.
			int pendingStayStatus = 0;
			std::string pendingStayParty;
			bool yootSendHousekeeper(Item * from);

			Sprite sprite;
			bool spriteNeedsUpdate;
			// Last frame column updateSprite() selected (detects the
			// day/night and 4AM lights-out re-poses).
			int roomFrameIndex;
			void updateSprite();
			/// Skin::Yoot: the room plugin's View sheet (Singleroom, Twinroom,
			/// Suiteroom; Smallroom/Largeroom in Kegon), null = OpenSky art.
			const YootPluginView * yootView = nullptr;
			/// Frame updateSprite() would pick now (sheet column or View band).
			int currentFrame() const;

			Path getRandomBackgroundSoundPath() override;

		protected:
			void scheduleGuest(Guest *g);
			void clearAll();
			void applyVariant();
		};
	}
}
