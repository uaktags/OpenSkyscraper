/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower placement and removal rules (game-parity.md L1, L2, M6, I7;
/// plan step 13).
///
/// The engine reports a failed check with T2TowerMessage::BuildErr(code),
/// which shows T2.exe STRTABLE 9999 entry code - 1. Each predicate below is
/// a pure port of the AreaCheck / DoDestructProc that raises its code and
/// returns that code (0 = allowed). Floors are OpenSky floor numbers
/// (0 = 1F, negative = basements); spans are [minX, maxX).
#pragma once

#include <vector>

namespace OT
{
	namespace Yoot
	{
		// BuildErr codes (STRTABLE 9999 index + 1).
		const int kErrOnlyLobbyOnFirst = 15;     ///< "Only the lobby item can be placed on the first floor."
		const int kErrLobbyFirstFloorOnly = 16;  ///< "The lobby item can be only placed on the first floor."
		const int kErrFloorCannotRemove = 18;    ///< "Floor can not be removed."
		const int kErrFloorNotTop = 19;          ///< "Floor can only be removed if it is the top floor of the building."
		const int kErrFloorNotLowest = 20;       ///< "... if it is the lowest floor of the building."
		const int kErrFloorHasTenant = 21;       ///< "... because there is already a tenant on it."
		const int kErrFloorHasTransit = 22;      ///< "... because transit systems are connected."
		const int kErrGateNotFromFirst = 53;     ///< "Entrances need to be connected to the first floor."
		const int kErrGateRemoveLot = 55;        ///< "Entrances connected to parking spaces can not be removed."
		const int kErrGateRemoveOrder = 56;      ///< "Removing Entrances needs to be done from loweest Entrance first."
		const int kErrLotNotAdjacent = 57;       ///< "Parking spaces need to be placed next to an Entrance or another parking space."
		const int kErrLotRemoveOrder = 58;       ///< "To remove parking spaces, start with the outer spaces."
		const int kErrSubwayLowest = 64;         ///< "The Subway can only be placed on the lowest floor."
		const int kErrFinalTopFloor = 66;        ///< "<item> can only be placed on the top floor."
		const int kErrOnlyFinalOnTop = 68;       ///< "Only Final item can be placed on the top floor of building."
		const int kErrFloorHasPeople = 73;       ///< "Can not remove the floor because there are people here."
		const int kErrSkyBridgeClear = 59;       ///< "To place the Sky Bridge requires at least 3 floors of free space ..."
		const int kErrSkyBridgeFar = 60;         ///< "... the distance between the towers is too far."
		const int kErrSkyBridgeLow = 62;         ///< "The Sky Bridge can only be placed above the 5th floor."
		const int kErrSkyBridgeTowers = 63;      ///< "The Sky Bridge can only be placed between the towers."
		const int kErrBoatDeckSite = 74;         ///< "The Boat Deck item can only be placed at left edge of the B2 floor."
		const int kErrObservatorySite = 75;      ///< "The observatory can only be placed at left edge of the B32 floor."

		/// Boatdeck.t2p AreaCheck FUN_1000bef0 (rect.left == 56, rect.top ==
		/// 55: Hawaii B2, left edge of the build area) and Observatory.t2p
		/// FUN_1000cc70 (left 81, top 40: Kegon B32). World units and rows.
		int fixedSiteRule(int leftUnit, int topRow, int wantLeft, int wantTop, int code);

		/// T2.exe STRTABLE 9999 text for a BuildErr code ("" if unknown).
		/// Codes 37, 41, 66 and 67 take the item name as a prefix.
		const char * buildErrorText(int code);

		/// L2 Stadium/Cathedral/Attraction.t2p AreaCheck (0x1000c230 /
		/// 0x1000c390): a final item (TNDF category 100) must stand with its
		/// bottom row on the world's top floor (rect.bottom ==
		/// topFloorLine + 1), so it rises above the floor area. T2TenantDef::
		/// AreaCheck 0x10062a90: every other tenant must stay below that
		/// row (rect.top > topFloorLine). `highestFloor` is the top floor.
		int finalItemRule(bool finalItem, int bottomFloor, int height, int highestFloor);

		/// I7 Subway.t2p AreaCheck FUN_1000d5f0: rect.bottom ==
		/// bottomFloorLine + 1, i.e. the subway's bottom row is the row just
		/// below the lowest floor (Tokyo B10 with floors down to B9).
		int subwayRule(int bottomFloor, int lowestFloor);

		/// M6: there is no 15-floor sky-lobby rule; the lobby goes on 1F only
		/// (STRTABLE 9999 #14/#15).
		int lobbyRule(int bottomFloor);

		/// One parking piece already in the tower.
		struct ParkingPiece
		{
			int floor = 0;     ///< bottom floor
			int height = 1;
			int minX = 0, maxX = 0;
			bool gate = false; ///< Parking Gate (Entrance), else a lot
			int topFloor() const { return floor + height - 1; }
		};
		/// L1 parking placement (STRTABLE 9999 #52, #56): an entrance
		/// (gate) must reach 1F or hang directly below another gate at the
		/// same x (gates stack down from the first floor, Parking.t2p idle
		/// FUN_1000bd20); a lot must touch a gate or a lot on its floor at
		/// its left or right edge.
		int parkingPlaceRule(const std::vector<ParkingPiece> & tower, const ParkingPiece & piece);
		/// L1 Parking.t2p DoDestructProc FUN_1000bf00: a gate cannot go
		/// while a lot touches it (#54) or another gate hangs below it (#55);
		/// a lot with lots on both sides is an inner space (#57).
		int parkingRemoveRule(const std::vector<ParkingPiece> & tower, const ParkingPiece & piece);

		/// What Floor.t2p's DoDestructProc FUN_1000ce30 sees around a floor.
		struct FloorRemoval
		{
			int floor = 0;
			bool moverOnFloor = false;  ///< a mover body intersects it (#21)
			bool peopleOnFloor = false; ///< T2Tenant +0x60 people (#72)
			bool tenantOnFloor = false; ///< any non-floor tenant (attr 0x2000 clear) (#20)
			bool tenantAbove = false;   ///< something on floor + 1 over the span
			bool tenantBelow = false;   ///< something on floor - 1 over the span
		};
		/// L1 floor removal order: above ground only the top floor (nothing
		/// above it, #18), underground only the lowest (nothing below, #19).
		/// The lobby floor (0) is never removable here.
		int floorRemoveRule(const FloorRemoval & f);
	}
}
