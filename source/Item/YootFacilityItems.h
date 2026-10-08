/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include <vector>
#include "../Sprite.h"
#include "../YootFacilities.h"
#include "Item.h"

namespace OT {
	namespace Item {
		/** Common base of the YootTower building facilities (plan step 12):
		 * a plugin-art tenant whose view pattern follows its occupancy.
		 * FacilitySystem drives them; these classes only hold state and
		 * draw. YootTower-only: BuildRules hides them in the other rulesets.
		 *
		 * Art: RT_BITMAP 1000 of the linked plugin sliced by its OBJMAP 1000
		 * "View" rects (pattern n = n-th rect), bottom-aligned on the item's
		 * floor like the Power Room. */
		class YootFacility : public Item
		{
		public:
			YootFacility(Game * game, AbstractPrototype * prototype) : Item(game, prototype) {}
			virtual ~YootFacility() {}

			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			int pattern = 0;            ///< T2Tenant +0x3c
			/// Occupants (restroom users / patients) and the absolute game
			/// minute each one leaves (bindMinutes after entering).
			std::vector<long long> leaveAt;
			int users() const { return (int)leaveAt.size(); }
			int capacity() const;
			int stayMinutes() const;

			void setPattern(int p);
			/// Pixels the art sits above the row's bottom edge.
			int interiorLift() const;
			/// The plugin's DrawProc paints the whole unit rect, not the
			/// interior (Stadium.t2p).
			virtual bool yootFullRect() const { return false; }
			virtual bool yootCoversFloor() const { return yootFullRect() && game->skin == Skin::Yoot; }

		protected:
			/// Plugin file when no catalog entry is loaded.
			virtual const char * defaultPlugin() const = 0;

			Sprite sprite;
			const sf::Texture * view = nullptr;
			/// OBJMAP 1000 "View" rects in pattern order.
			const std::vector<sf::IntRect> * bands = nullptr;
			void updateSprite();
		};

		/// TrashFacility.t2p (KTrashFacility.t2p in Kegon): ATTR 1542,
		/// grade 3, TNDF 25x2 units, capacity 50, open 07:30, close 07:00.
		/// Patterns 0 empty .. 5 full, 6 emptying (200x60 bands).
		class TrashFacility : public YootFacility
		{
		public:
			TrashFacility(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(TrashFacility) {
				p->id = "trashfacility";
				p->name = "Trash Facility";
				p->price = 500000;
				p->size = int2(25, 2);
				p->icon = 18;
			}
			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			Yoot::TrashFacilityModel model;

		protected:
			const char * defaultPlugin() const { return "TrashFacility.t2p"; }
		};

		/// Restroom.t2p (KRestroom.t2p in Kegon): ATTR 1500, grade 1,
		/// TNDF 9x1, capacity 5, stay 15 minutes, registry 7 (P5).
		class Restroom : public YootFacility
		{
		public:
			Restroom(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(Restroom) {
				p->id = "restroom";
				p->name = "Restroom";
				p->price = 10000;
				p->size = int2(9, 1);
				p->icon = 18;
			}
		protected:
			const char * defaultPlugin() const { return "Restroom.t2p"; }
		};

		/// Doctor's.t2p: ATTR 1544, grade 3, TNDF 26x1, capacity 20,
		/// open 08:00-20:00, stay 30 minutes, registry 4 (P6). Pattern 0
		/// closed, 1 open, 2 with patients, 3 from half capacity.
		class DoctorsOffice : public YootFacility
		{
		public:
			DoctorsOffice(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(DoctorsOffice) {
				p->id = "doctor";
				p->name = "Doctor's Office";
				p->price = 500000;
				p->size = int2(26, 1);
				p->icon = 18;
			}
			bool isOpen(int minuteOfDay) const;
		protected:
			const char * defaultPlugin() const { return "Doctor's.t2p"; }
		};
	}
}
