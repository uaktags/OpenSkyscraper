#pragma once
#include "../Sprite.h"
#include "Item.h"

namespace OT {
	namespace Item {
		/** Parking structure item.
		 *
		 * Provides parking spaces as a tower-wide resource. The JudgeSystem
		 * tallies total spaces across all Parking instances and computes
		 * coverage against the original's requirement of 1 space per hotel
		 * room and 1 per 4 offices; under-covered towers penalise office and
		 * hotel evaluation.
		 *
		 * TODO(Phase 2.2): gate integration (SetupAllGate equivalent) and
		 * visual car sprites on individual tiles. */
		class Parking : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Parking);
			OT_ITEM_PROTOTYPE(Parking) {
				p->id    = "parking";
				p->name  = "Parking";
				p->price = 5000;
				p->size  = int2(8, 1);
				p->icon  = 20;
			}
			virtual ~Parking();

			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter &xml);
			virtual void decodeXML(tinyxml2::XMLElement &xml);
			virtual void advance(double dt);
			virtual int dailyMaintenanceCost() const override { return 50; }

			/// Spaces available on this tile, derived from width.
			int totalSpaces() const;
			int usedSpaces() const { return used; }
			bool hasSpace() const  { return used < totalSpaces(); }
			bool assignSpace();    ///< @return true if a space was claimed.
			void freeSpace();

			Sprite sprite;
			bool spriteNeedsUpdate;
			void updateSprite();

		private:
			int used;
		};
	}
}
