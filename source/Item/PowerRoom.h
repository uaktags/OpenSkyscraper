/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include "../Sprite.h"
#include "Item.h"

namespace OT {
	namespace Item {
		/** YootTower Power Room (PowerRoom.t2p: ATTR id 1541, grade 3; TNDF
		 * 23x2 units, price 5000 = $500,000, outMoney 500, power 0).
		 *
		 * It holds up to five transformers. The first comes with the room:
		 * T2TowerEvent::SetLatestBuild (0x100692a0) calls AddSupplyPower(1)
		 * when the 'HEND' plugin is built. Each further transformer costs
		 * BOEV's transformer price (PowerRoom.t2p FUN_1000c2f0 and
		 * FUN_1000c4a0). Supply is defSupply + addSupply x transformers; see
		 * PowerSystem.
		 *
		 * YootTower-only: BuildRules::minRatingToBuild hides it in the
		 * SimTower and OpenSky rulesets. */
		class PowerRoom : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(PowerRoom);
			OT_ITEM_PROTOTYPE(PowerRoom) {
				p->id = "powerroom";
				p->name = "Power Room";
				p->price = 500000;
				p->size = int2(23, 2);
				p->icon = 18;
			}
			virtual ~PowerRoom() {}

			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			static const int kMaxTransformers = 5;
			int transformers; ///< 1..5 installed (FUN_1000c1d0 starts with one)

			Sprite sprite;
		};
	}
}
