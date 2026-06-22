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
			virtual int dailyMaintenanceCost() const override { return 0; }
			virtual bool isOccupied() const override { return occupied; }

			virtual void addPerson(Person *p) override;
			virtual void removePerson(Person *p) override;

		public:
			static bool loadPEBitmap(const std::string &pluginPath, int resourceId, sf::Texture &texture, const sf::Color &maskColor = sf::Color::Transparent);

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

			mutable Sprite baseSprite;
			mutable Sprite residentSprite;
			bool spriteNeedsUpdate;

			std::set<YootCondoOccupant *> occupants;
			std::priority_queue<YootCondoOccupant *, std::deque<YootCondoOccupant *>, YootCondoOccupant::departsLaterThan> departureQueue;
			std::priority_queue<YootCondoOccupant *, std::deque<YootCondoOccupant *>, YootCondoOccupant::returnsLaterThan> returnQueue;

			// Static textures shared among instances
			static sf::Texture emptyCondoTexture;
			static sf::Texture residentTexture;
			static bool texturesLoaded;

			static bool loadTextures(Game *game);
		};
	}
}
