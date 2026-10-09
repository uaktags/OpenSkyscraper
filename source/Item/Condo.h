#pragma once
#include "../Sprite.h"
#include "../YootItemArt.h"
#include "Item.h"

namespace OT
{
	namespace Item
	{
		class Condo : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Condo);
			OT_ITEM_PROTOTYPE(Condo)
			{
				p->id = "condo";
				p->name = "Condo";
				p->price = 80000;
				p->size = int2(16, 1);
				p->icon = 24;
			}
			virtual ~Condo();
			virtual int disasterSaleReversal() const override { return occupied ? rentDeposit : 0; }
			virtual int demolitionBuyout() const override;
			virtual void evacuateForDisaster() override;

			enum LightingConditions
			{
				NIGHT,
				LIT,
				DAYTIME
			};

			/**
			 * A Person living in a Condo item (T36). Every condo has exactly
			 * three members: 0 adult (Man), 1 homebody (Mother with Baby),
			 * 2 kid (Child). The extra arguments of the constructor are kept
			 * for source compatibility; the day cycle is rolled per visit.
			 */
			class CondoOccupant : public Person
			{
			public:
				CondoOccupant(Condo *item, Person::Type type, double depart = 0, double ret = 0, int k = 0);

				double departureTime;
				double returnTime;
				/// True if the person is currently in the condo
				bool atHome;
				/// Outside the condo (commute, shopping, dining).
				bool out = false;
				bool asleep = false;
				bool leftToday = false;
				/// Venue phase bits: shopping/dining dispatched in this phase.
				int shopPhase = -1;
			};

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter &xml) override;
			virtual void decodeXML(tinyxml2::XMLElement &xml) override;
			virtual void advance(double dt);
			virtual bool isOccupied() const override { return occupied; }

			virtual void addPerson(Person *p);
			virtual void removePerson(Person *p);

			/// Quarter-day move-out (T6): family retires, unit buys back.
			void vacate();
			/// Mover arrival (T9): sale credited, family created.
			void moveIn();
			virtual void resetMemberBanks() override;
			virtual int populationCategory() const override { return 9; }
			virtual int populationContribution() const override { return occupied ? 3 : 0; }

			/// Night-frame gate used by the AnimPeple suppression in
			/// Item::renderPeople (OpenSkyWeb ISSUE-037 lighting rules).
			bool isNightLighting() const { return lighting == NIGHT; }

		protected:
			void visit();
			void createOccupants();
			void removeOccupants();
			bool updateLighting(double time);
			void updateSprite();

			int rent;
			int rentDeposit;
			int variant;
			LightingConditions lighting;
			bool occupied;
			int diningPhase = -1;
			int diningSent = 0;

			Sprite sprite;
			/// Skin::Yoot plugin art (YootItemArt.h), null = OpenSky art.
			const YootPluginView * yootView = nullptr;
			bool spriteNeedsUpdate;
			std::set<CondoOccupant *, SimLess> occupants;
			CondoOccupant *family[3] = {nullptr, nullptr, nullptr};
		};
	}
}
