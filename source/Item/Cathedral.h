#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>

namespace OT {
	namespace Item {
		/** Cathedral — wedding church / "Tower of the Year" endgame venue
		 * (original ChurchT).
		 *
		 * EXE-faithful role (concilia-tower decode of ChurchT seg_1040,
		 * adopted 2026-09-14, TODO.md rows 23): a completed cathedral
		 *  - retires fires forever (read Cathedral::towerHasCathedral()),
		 *  - hosts the 5-star -> TOWER wedding: on an eligible weekend
		 *    OpenChurch (7:00) summons 40 guests; if all 40 are inside
		 *    before 12:30, InChurchPeple fires StartMarry — bells ring,
		 *    the couple animation (resource 0x8828) runs, the camera pans
		 *    to the church, and the TOWER promotion is evaluated
		 *    mid-ceremony (Game::ratingMayIncrease -> LevelUp's
		 *    cathedral+15000+weddingCompleted row). CloseChurch (13:00)
		 *    sends everyone home whether or not the ceremony fired.
		 *
		 * Completing construction no longer crowns the tower by itself.
		 */
		class Cathedral : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Cathedral);
			OT_ITEM_PROTOTYPE(Cathedral) {
				p->id    = "cathedral";
				p->name  = "Cathedral";
				p->price = 3000000;
				p->size  = int2(48, 4);
				// The EXE toolbox has no dedicated cathedral cell; reuse the
				// party hall button art, matching the web port's sprite
				// reuse (simtower/cathedral/main = composed partyhall).
				p->icon  = 15;
			}
			virtual ~Cathedral();

			virtual void init();
			virtual void advance(double dt);

			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			virtual int dailyMaintenanceCost() const override { return 1000; }

			/// ChurchT wedding state for the current day.
			enum WeddingPhase {
				kWeddingIdle = 0,        ///< no event today
				kWeddingGuestsArriving, ///< OpenChurch 7:00: the 40 guests are traveling
				kWeddingCeremony        ///< StartMarry: couple animation running
			};
			WeddingPhase weddingPhase;
			bool  weddingDone;   ///< a wedding has been held (latched for good)
			int   weddingCounter;///< AnimateWeddingCouple script step (EXE event dword 0xB40C)

			/// True when the tower has a completed cathedral. Consumed by
			/// the EventSystem fire scheduler: fires retire for good.
			static bool towerHasCathedral(Game * game);

			/// True when a cathedral wedding has been held. Consumed by
			/// LevelUp's 5-star -> TOWER gate.
			static bool weddingCompleted(Game * game);

			/// The cathedral hosting today's wedding event, or NULL when
			/// no cathedral is in a wedding phase.
			static Cathedral * weddingVenue(Game * game);

			/// A wedding guest summoned by OpenChurch; walks in from the
			/// lobby like any venue crowd and holds their pew until the
			/// 13:00 CloseChurch dismissal.
			class Guest : public Person {
			public:
				Guest(Cathedral * church);
			};
			typedef std::set<Guest *, SimLess> Guests;
			Guests guests;
			void clearGuests();

			// Declaration order == draw order (the sprite set is pointer-
			// ordered): exterior, then the ceremony strips the cathedral
			// wears while the wedding runs, then the couple on top.
			Sprite sprite;
			Sprite ceremonyStrips[6]; ///< simtower/church/ceremony0..5 (0 = topmost banner strip)
			Sprite coupleA;           ///< simtower/church/couple (0x8828): bride / ceremony pose
			Sprite coupleB;           ///< groom during the walk-in

		private:
			void startWedding();      ///< StartMarry (1040:02B5)
			void endWeddingEvent();   ///< CloseChurch (1040:0179)
			void updateCoupleSprites();

			double weddingFrameAccumulator; ///< EXE frames since the last couple step
			double lastAdvancedAt;          ///< game->time.absolute at the previous advance
			unsigned coupleCellWidth;       ///< 0x8828 sheet width / 8 cells
			bool   coupleArtReady;          ///< couple sheet loaded at sheet size (not a placeholder)
		};
	}
}
