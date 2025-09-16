/* Copyright © 2012-2013 Fabian Schuiki */
/* Copyright © 2025 Tim G */
/* Copyright © 2013 hoshi10 */
#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "Factory.h"

namespace OT {
	namespace Item {
		class FastFood : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(FastFood);
			OT_ITEM_PROTOTYPE(FastFood) {
				p->id    = "fastfood";
				p->name  = "Fast Food";
				p->price = 100000;
				p->size  = int2(16,1);
				p->icon  = ICON_FASTFOOD;
				p->toolboxCategory = CAT_FOOD;
				p->toolboxOrder = 1;
			}
			virtual ~FastFood();
			
			virtual void init();
			
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			
			int variant;
			bool open;
			
			class Customer : public Person {
			public:
				Customer(FastFood * item);
				virtual ~Customer()	{ LOG(DEBUG, "fastfood customer %p killed", this); }
				double arrivalTime; //when the customer arrives at the lobby of the tower

				struct laterThan : public std::binary_function<Customer *, Customer *, bool> {
					bool operator() (const Customer * _Left, const Customer * _Right) const {
						return (_Left->arrivalTime > _Right->arrivalTime);
					}
				};
			};
			std::priority_queue<Customer *, std::deque<Customer *>, Customer::laterThan> arrivingCustomers; // Priority queue to facilitate selection of customers spawned by FastFood due to arrive at tower
			std::list<Person *> eatingCustomers; // List (in order of arrival) to facilitate selection of customers due to leave FastFood
			typedef std::set<Customer *> Customers;
			Customers customers;
			void clearCustomers();
			
			struct CustomerMetadata {
				double arrivalTime;   //when the customer arrived at the fast food
			};
			typedef std::map<Person *, CustomerMetadata> CustomerMetadataMap;
			CustomerMetadataMap customerMetadata;
			
			Sprite sprite;
			bool spriteNeedsUpdate;
			void updateSprite();
			
			virtual void advance(double dt);
			
			virtual void addPerson(Person * p);
			virtual void removePerson(Person * p);

			Path getRandomBackgroundSoundPath();
		};
	}
}