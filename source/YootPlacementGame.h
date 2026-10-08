/* Copyright (c) 2026 OpenSkyscraper Project */
/// Game-side wiring of the YootTower placement rules (YootPlacement.h,
/// plan step 13). Every function is a no-op ("" / false) outside a
/// YootTower world.
#pragma once

#include <string>

namespace OT
{
	class Game;
	namespace Item { class AbstractPrototype; class Item; }

	namespace Yoot
	{
		/// Final items and the subway may leave the floor area (see
		/// BuildRules::allowsSpecialFootprint).
		bool usesSpecialFootprint(const Game & game, const Item::AbstractPrototype * proto);
		/// L1/L2/M6/I7 checks for a new item at (x, floor) with `height`
		/// floors; "" when allowed, else the STRTABLE 9999 message.
		std::string placementError(const Game & game, const Item::AbstractPrototype * proto,
		                           int x, int floor, int height);
		/// Bulldozer: true when `item` may be removed at all in YootTower
		/// (floors become removable in order); `reason` explains a refusal.
		bool removalAllowed(const Game & game, const Item::Item * item, std::string & reason);
	}
}
