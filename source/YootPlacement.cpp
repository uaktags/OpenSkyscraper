/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootPlacement.h"

namespace OT
{
	namespace Yoot
	{
		const char * buildErrorText(int code)
		{
			// T2.exe STRTABLE 9999 (75 entries), shown for code - 1.
			switch (code) {
			case 14: return "Can not place items below the subway station.";
			case kErrOnlyLobbyOnFirst: return "Only the lobby item can be placed on the first floor.";
			case kErrLobbyFirstFloorOnly: return "The lobby item can be only placed on the first floor.";
			case kErrFloorCannotRemove: return "Floor can not be removed.";
			case kErrFloorNotTop: return "Floor can only be removed if it is the top floor of the building.";
			case kErrFloorNotLowest: return "Floor can only be removed if it is the lowest floor of the building.";
			case kErrFloorHasTenant: return "This floor can not be removed because there is already a tenant on it.";
			case kErrFloorHasTransit: return "This floor can not be removed because transit systems are connected.";
			case kErrGateNotFromFirst: return "Entrances need to be connected to the first floor.";
			case 54: return "Entrances need to be placed vertically.";
			case kErrGateRemoveLot: return "Entrancse connected to parking spaces can not be removed.";
			case kErrGateRemoveOrder: return "Removing Entrances needs to be done from loweest Entrance first.";
			case kErrLotNotAdjacent: return "Parking spaces need to be placed next to an Entrance or another parking space.";
			case kErrLotRemoveOrder: return "To remove parking spaces, start with the outer spaces.(Not from in between.)";
			case kErrSubwayLowest: return "The Subway can only be placed on the lowest floor.";
			case kErrFinalTopFloor: return " can only be placed on the top floor.";
			case 67: return " can only be placed once.";
			case kErrOnlyFinalOnTop: return "Only Final item can be placed on the top floor of building.";
			case kErrFloorHasPeople: return "Can not remove the floor because there are people here.";
			case kErrSkyBridgeClear: return "To place the Sky Bridge requires at least 3 floors of free space both above and below.";
			case kErrSkyBridgeFar: return "The Sky Bridge can not be placed because the distance between the towers is too far.";
			case kErrSkyBridgeLow: return "The Sky Bridge can only be placed above the 5th floor.";
			case kErrSkyBridgeTowers: return "The Sky Bridge can only be placed between the towers.";
			case kErrBoatDeckSite: return "The Boat Deck item can only be placed at left edge of the B2 floor.";
			case kErrObservatorySite: return "The observatory can only be placed at left edge of the B32 floor.";
			default: return "";
			}
		}

		int finalItemRule(bool finalItem, int bottomFloor, int height, int highestFloor)
		{
			if (finalItem)
				return bottomFloor == highestFloor ? 0 : kErrFinalTopFloor;
			// T2TenantDef::AreaCheck: rect.top > topFloorLine, i.e. the
			// item's top floor stays below the top floor.
			return bottomFloor + height - 1 < highestFloor ? 0 : kErrOnlyFinalOnTop;
		}

		int fixedSiteRule(int leftUnit, int topRow, int wantLeft, int wantTop, int code)
		{
			return leftUnit == wantLeft && topRow == wantTop ? 0 : code;
		}

		int subwayRule(int bottomFloor, int lowestFloor)
		{
			return bottomFloor == lowestFloor - 1 ? 0 : kErrSubwayLowest;
		}

		int lobbyRule(int bottomFloor)
		{
			return bottomFloor == 0 ? 0 : kErrLobbyFirstFloorOnly;
		}

		static bool touchesSide(const ParkingPiece & a, const ParkingPiece & b)
		{
			// Same floor, edge to edge (GetTenant(top, left - 1) / (top, right)).
			return a.topFloor() == b.topFloor() && (a.maxX == b.minX || b.maxX == a.minX);
		}

		int parkingPlaceRule(const std::vector<ParkingPiece> & tower, const ParkingPiece & piece)
		{
			if (piece.gate) {
				if (piece.topFloor() == 0) return 0;
				for (const ParkingPiece & g : tower)
					if (g.gate && g.minX == piece.minX && g.floor == piece.topFloor() + 1) return 0;
				return kErrGateNotFromFirst;
			}
			for (const ParkingPiece & p : tower)
				if (touchesSide(piece, p)) return 0;
			return kErrLotNotAdjacent;
		}

		int parkingRemoveRule(const std::vector<ParkingPiece> & tower, const ParkingPiece & piece)
		{
			bool left = false, right = false, gateBelow = false;
			for (const ParkingPiece & p : tower) {
				if (piece.gate && p.gate && p.minX == piece.minX && p.topFloor() == piece.floor - 1)
					gateBelow = true; // FUN_1000bf00: GetTenant(bottom, left)
				if (p.topFloor() != piece.topFloor()) continue;
				if (p.maxX == piece.minX) left = true;
				if (p.minX == piece.maxX) right = true;
			}
			if (gateBelow) return kErrGateRemoveOrder;
			if (piece.gate) return (left || right) ? kErrGateRemoveLot : 0;
			return (left && right) ? kErrLotRemoveOrder : 0;
		}

		int floorRemoveRule(const FloorRemoval & f)
		{
			// FUN_1000ce30 order: transit, people, tenants, then the order rule.
			if (f.moverOnFloor) return kErrFloorHasTransit;
			if (f.peopleOnFloor) return kErrFloorHasPeople;
			if (f.tenantOnFloor) return kErrFloorHasTenant;
			if (f.floor > 0 && f.tenantAbove) return kErrFloorNotTop;
			if (f.floor < 0 && f.tenantBelow) return kErrFloorNotLowest;
			if (f.floor == 0) return kErrFloorCannotRemove;
			return 0;
		}
	}
}
