#pragma once
#include "../Sprite.h"
#include "Item.h"

namespace OT
{
	namespace Item
	{
		/// Retail shop tenant (concilia-tower adoption row 3). The 11 visual
		/// variants live on sheets 0x8668-0x8672 (three 96x24 fill frames
		/// each); the shared single-frame 0x8673 "For RENT" storefront shows
		/// while un-let and 0x8674 shutters outside the 10:00-21:00 opening
		/// window (CloseOneVenue seg 11a8:06ca). This item only renders the
		/// state; the walk-in quota economy consuming these hooks lands in
		/// wave S (adoption row 38).
		class Shop : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Shop);
			OT_ITEM_PROTOTYPE(Shop)
			{
				p->id = "shop";
				p->name = "Retail Shop";
				p->price = 100000;
				p->size = int2(12, 1);
				p->icon = 13;
			}
			virtual ~Shop();
			virtual void evacuateForDisaster() override;

			virtual void init();

			virtual void encodeXML(tinyxml2::XMLPrinter &xml);
			virtual void decodeXML(tinyxml2::XMLElement &xml);

			int variant;
			/// Cosmetic tenant identity (SIMTOWER.EXE string table res
			/// 0x2cc), indexed by the sprite `variant`.
			static const char * const kNameVariants[11];

			/// EXE retail record +0x02 state byte (sprite-state referee
			/// 2026-07-18): 0x00 empty (0 patrons), 0x01 1-9 inside, 0x02
			/// 10+ packed, 0x03 closed, 0xFF un-let. `let`, `open` and
			/// `patrons` are the plain hooks for that byte; this class only
			/// renders them.
			bool let;  ///< Tenant present (false = the 0xFF un-let storefront).
			bool open; ///< Inside the 10:00-21:00 window (false = shutters).
			int patrons; ///< Current patron count driving the fill tier.
			// Retail record fields used by the daily street pool.  Quarterly
			// rent is settled by Game, not by this item.
			int rentClass;
			int serviceScore[3];
			int dailyQuota;
			int quotaRemaining;
			int walkinsToday;
			int walkinsYesterday;
			int customersToday;
			int lastDemandDay;
			void setLet(bool l);
			void setOpen(bool o);
			void setPatrons(int p);
			/// The EXE retail state byte the current fields map to.
			int stateByte() const;

			Sprite sprite;
			bool spriteNeedsUpdate;
			void updateSprite();

			virtual void advance(double dt);

			virtual Path getRandomBackgroundSoundPath();
		};
	}
}
