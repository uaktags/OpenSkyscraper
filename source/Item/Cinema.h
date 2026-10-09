#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "../Sound.h"
#include <set>

namespace OT {
	namespace Item {
		class Cinema : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Cinema);
			OT_ITEM_PROTOTYPE(Cinema) {
				p->id    = "cinema";
				p->name  = "Movie Theatre";
				p->price = 500000;
				p->size  = int2(31,2);
				p->icon  = 14;
				p->entrance_offset = 1;
			}
			virtual ~Cinema();
			virtual void evacuateForDisaster() override;
			
			virtual void init();
			
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			
			bool open;
			bool playing;
			bool intermission;
			int movieType;
			int movieAgeDays;
			int quotaMatinee;
			int quotaEvening;
			int patronsToday;
			bool eveningShow;
			bool revenueSettled;

			class Customer : public Person {
			public:
				Customer(Cinema *cinema);
				bool evening;
			};
			typedef std::set<Customer *> Customers;
			Customers customers;
			void clearCustomers();
			
			double animation;
			int animationFrame;
			Sprite hallSprite;
			Sprite screenSprite;
			bool spriteNeedsUpdate;
			void updateSprite();
			
			virtual void advance(double dt);
			virtual int dailyMaintenanceCost() const override { return 1500; }

			void addPerson(Person * p);
			void removePerson(Person * p);

			// Looping movie soundtrack: the EXE requests sound 9001+movie_id
			// at screening start (films 3 and 6 ship silent) and silences it
			// at the intermission and closing.
			Sound soundtrack;
			bool soundtrackPlaying;
			void updateSoundtrack();
			Path getRandomBackgroundSoundPath();
		};
	}
}
