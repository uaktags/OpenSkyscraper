#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <queue>
#include <set>

namespace OT
{
	namespace Item
	{
		class YootCondo : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(YootCondo);
			OT_ITEM_PROTOTYPE(YootCondo)
			{
				p->id = "yoot_condo";
				p->name = "Yoot Condo";
				p->price = 100000;
				p->size = int2(16, 1);
				p->icon = 24;
			}
			virtual ~YootCondo();
			virtual int disasterSaleReversal() const override { return occupied ? rentDeposit : 0; }
			virtual int demolitionBuyout() const override { return occupied ? rentDeposit : 0; }
			virtual void evacuateForDisaster() override;

			enum LightingConditions
			{
				NIGHT,
				LIT,
				DAYTIME
			};

			class YootCondoOccupant : public Person
			{
			public:
				YootCondoOccupant(YootCondo *item, Person::Type type, double depart, double ret)
					: Person(item->game, type),
					  departureTime(depart),
					  returnTime(ret),
					  departureJitter(0.0),
					  returnJitter(0.0),
					  posX(10 + rand() % 100),
					  animOffset(rand() % 40)
				{
				}

				double departureTime;
				double returnTime;
				double departureJitter;
				double returnJitter;
				int posX;
				int animOffset;

				struct departsLaterThan
				{
					bool operator()(const YootCondoOccupant *a, const YootCondoOccupant *b) const
					{
						return (a->actualDepartureTime() > b->actualDepartureTime());
					}
				};
				struct returnsLaterThan
				{
					bool operator()(const YootCondoOccupant *a, const YootCondoOccupant *b) const
					{
						return (a->actualReturnTime() > b->actualReturnTime());
					}
				};

				double actualReturnTime() const;
				double actualDepartureTime() const;
			};

			virtual void init() override;
			virtual void render(sf::RenderTarget &target) const override;

			virtual void encodeXML(tinyxml2::XMLPrinter &xml) override;
			virtual void decodeXML(tinyxml2::XMLElement &xml) override;
			virtual void advance(double dt) override;
			virtual bool isOccupied() const override { return occupied; }

			virtual void addPerson(Person *p) override;
			virtual void removePerson(Person *p) override;

			/// Night-frame gate used by the AnimPeple suppression in
			/// Item::renderPeople (OpenSkyWeb ISSUE-037 lighting rules).
			bool isNightLighting() const { return lighting == NIGHT; }
			
		protected:
			void generateJitters();
			void moveOccupants();
			void createOccupants();
			void removeOccupants();
			bool updateLighting(double time);
			void updateSprite();
			bool isAttractive();

			int rent;
			int rentDeposit;
			int variant;
			LightingConditions lighting;
			bool occupied;
			long long yootIdleMinute = -1;   ///< last game minute whose idle slot was checked

			mutable Sprite baseSprite;
			mutable Sprite residentSprite;
			bool spriteNeedsUpdate;

			std::set<YootCondoOccupant *, SimLess> occupants;
			std::priority_queue<YootCondoOccupant *, std::deque<YootCondoOccupant *>, YootCondoOccupant::departsLaterThan> departureQueue;
			std::priority_queue<YootCondoOccupant *, std::deque<YootCondoOccupant *>, YootCondoOccupant::returnsLaterThan> returnQueue;

			/// Plugin art: Condo.t2p, or the world's plugin through the
			/// catalog (HawaianCondo.t2p in Hawaii); Apartment.t2p overrides.
			virtual const char * condoPlugin() const;
			/// Becoming occupied / vacated: sale and buy-back (YootCondo).
			virtual void moveIn();
			virtual void moveOut();
			virtual int residentsToCreate() const;
			virtual bool hasFamilies() const { return true; }

			// Textures shared among instances, cached per plugin.
			const sf::Texture * emptyCondoTexture = nullptr;
			const sf::Texture * residentTexture = nullptr;
			bool loadTextures();
		};
	}
}
