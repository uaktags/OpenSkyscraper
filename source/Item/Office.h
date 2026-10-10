/* Copyright © 2013 Fabian Schuiki */
#pragma once
#include "Item.h"
#include "../YootItemArt.h"
#include <queue>
#include <vector>
#include <set>

namespace OT
{
	namespace Item
	{
		class Parking;

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
			virtual void evacuateForDisaster() override;

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter &xml) override;
			virtual void decodeXML(tinyxml2::XMLElement &xml) override;

			virtual void advance(double dt) override;
			virtual bool isOccupied() const override { return occupied; }
			/// Lit-state + worker access for the AnimPeple overlay in
			/// Item::renderPeople (OpenSkyWeb office population rules).
			bool isLit() const { return lit; }
			std::vector<Person *> getWorkerPeople(int maxCount) const
			{
				std::vector<Person *> out;
				for (Workers::const_iterator w = workers.begin(); w != workers.end() && (int)out.size() < maxCount; ++w)
					out.push_back(*w);
				return out;
			}

			virtual void addPerson(Person *p) override;
			void prepareLunchQa(double lunchHour);

			/// Tenant leaves (quarter-day move-out, T6): workers go away, the
			/// unit turns vacant and CountT drops by 6.
			void vacate();
			/// An armed vacant unit is re-let by a mover (T9).
			void moveIn();
			virtual void resetMemberBanks() override;
			virtual int populationCategory() const override { return 0; }
			virtual int populationContribution() const override { return occupied ? 6 : 0; }

			Path getRandomBackgroundSoundPath() override;

			/**
			 * A Person working in an Office item (member k = 0..5). Members 0
			 * and 1 are salesmen; worker 0 makes the sales trip, 1..5 lunch.
			 */
			class Worker : public Person
			{
			public:
				Worker(Office *item, Person::Type type, int k = 0);

				/// Day flags, cleared at 05:00.
				bool arrived = false;
				bool lunchSent = false;
				bool salesSent = false;
				bool salesBack = false;
				bool sickSent = false;
				/// Absolute time a lunch visit ends (0 = not at lunch).
				double lunchReturnTime = 0;

				/// Parking slot the worker's car currently occupies (NULL if
				/// they arrived on foot or no reachable parking had space).
				Parking * parkingUsed = nullptr;
			};

		protected:
			void updateSprite();
			void newDay();
			void visit();
			void sendHome(Worker *w);
			void arrive(Worker *w);
			void releaseCar(Worker *w);
			bool drives(Worker *w) const;
			void syncWorkersFromRecord();
		public:
			/// Worker k (0..5) of this office, or NULL.
			Worker * crewMember(int k) const { return (k >= 0 && k < 6) ? crew[k] : nullptr; }
		protected:

			bool occupied;
			int variant;
			bool lit;

			Sprite sprite;
			bool spriteNeedsUpdate;
			/// Skin::Yoot: Office.t2p art (YootItemArt.h), null = OpenSky art.
			const YootPluginView * yootView = nullptr;
			typedef std::set<Worker *, SimLess> Workers;
			Workers workers;
			Worker *crew[6];
		};
	}
}
