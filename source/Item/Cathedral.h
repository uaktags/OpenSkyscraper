#pragma once
#include "../Sprite.h"
#include "Item.h"

namespace OT {
	namespace Item {
		/** Cathedral / "Tower of the Year" endgame monument (OpenSkyWeb
		 * ISSUE-010, original ChurchT.h/c).
		 *
		 * A 48x4 rooftop monument unlockable at 5 stars. When construction
		 * completes, church bells ring and the tower is awarded the
		 * TOWER rating (internal rating 5) with the authentic award
		 * message. The ceremony flag persists so reloads don't replay it.
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

			bool ceremonyTriggered;

			Sprite sprite;
		};
	}
}
