#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>

namespace OT {
	namespace Item {
		class PartyHall : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(PartyHall);
			OT_ITEM_PROTOTYPE(PartyHall) {
				p->id    = "partyhall";
				p->name  = "Party Hall";
				p->price = 500000;
				p->size  = int2(24,2);
				p->icon  = 15;
			}
			virtual ~PartyHall();

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			bool open;

			/** A Person who arrives for a party. Spawned when the hall opens,
			 *  removed when the hall closes (with attendance-based income). */
			class Visitor : public Person {
			public:
				Visitor(PartyHall *hall);
			};
			typedef std::set<Visitor *> Visitors;
			Visitors visitors;
			void clearVisitors();

			Sprite sprite;
			bool spriteNeedsUpdate;
			void updateSprite();

			virtual void advance(double dt);
			virtual int dailyMaintenanceCost() const override { return 1000; }

			virtual void addPerson(Person * p);
			virtual void removePerson(Person * p);

			Path getRandomBackgroundSoundPath();
		};
	}
}
