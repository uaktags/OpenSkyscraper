/* Copyright (c) 2026 OpenSkyscraper Project */
#include "YootPlacementGame.h"

#include "Game.h"
#include "Item/Item.h"
#include "YootCatalog.h"
#include "YootPlacement.h"

#include <vector>

using namespace OT;

namespace
{
	bool isMover(const Item::AbstractPrototype * proto)
	{
		return proto->id.find("elevator") == 0 || proto->id == "stairs" || proto->id == "escalator";
	}

	std::string errorText(const Item::AbstractPrototype * proto, int code)
	{
		const std::string text = Yoot::buildErrorText(code);
		// Entries starting with a space take the item name (BuildErr's
		// T2EquipDef argument).
		if (!text.empty() && text[0] == ' ' && proto) return proto->name + text;
		return text;
	}

	bool overlaps(const Item::Item * item, int minX, int maxX)
	{
		return item->position.x < maxX && item->getRect().maxX() > minX;
	}

	/// Parking lots and gates (Parking.t2p variations 0 / 1, plan step 14).
	std::vector<Yoot::ParkingPiece> parkingPieces(const Game & game, const Item::Item * except)
	{
		std::vector<Yoot::ParkingPiece> out;
		static const char * kinds[] = {"parkinglot", "parkinggate"};
		for (const char * kind : kinds) {
			Game::ItemSetByString::const_iterator it = game.itemsByType.find(kind);
			if (it == game.itemsByType.end()) continue;
			for (Game::ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i) {
				if (*i == except) continue;
				Yoot::ParkingPiece p;
				p.floor = (*i)->position.y;
				p.height = (*i)->size.y;
				p.minX = (*i)->position.x;
				p.maxX = (*i)->getRect().maxX();
				p.gate = std::string(kind) == "parkinggate";
				out.push_back(p);
			}
		}
		return out;
	}
}

bool Yoot::usesSpecialFootprint(const Game & game, const Item::AbstractPrototype * proto)
{
	if (!game.activeRules.profile().yootWorld || !proto) return false;
	if (proto->icon == ICON_METRO) return true;
	return proto->yoot && proto->yoot->finalItem();
}

std::string Yoot::placementError(const Game & game, const Item::AbstractPrototype * proto,
                                 int x, int floor, int height)
{
	const RulesProfile & profile = game.activeRules.profile();
	if (!profile.yootWorld || !proto) return std::string();

	int code = 0;
	if (proto->icon == ICON_METRO)
		code = subwayRule(floor, profile.lowestFloor);
	else if (proto->icon == ICON_LOBBY)
		code = lobbyRule(floor);
	else if (proto->id == "boatdeck")
		code = fixedSiteRule(profile.unitForX(x), profile.rowForFloor(floor + height - 1), 56, 55, kErrBoatDeckSite);
	else if (proto->id == "observatory")
		code = fixedSiteRule(profile.unitForX(x), profile.rowForFloor(floor + height - 1), 81, 40, kErrObservatorySite);
	if (!code && !isMover(proto))
		code = finalItemRule(proto->yoot && proto->yoot->finalItem(), floor, height, profile.highestFloor);

	// Parking.t2p AreaCheck / Entrance rules (#52, #56), live since the
	// Parking Gate item exists (step 14).
	if (!code && (proto->id == "parkinglot" || proto->id == "parkinggate")) {
		ParkingPiece piece;
		piece.floor = floor;
		piece.height = height;
		piece.minX = x;
		piece.maxX = x + proto->size.x;
		piece.gate = proto->id == "parkinggate";
		code = parkingPlaceRule(parkingPieces(game, nullptr), piece);
	}
	return code ? errorText(proto, code) : std::string();
}

bool Yoot::removalAllowed(const Game & game, const Item::Item * item, std::string & reason)
{
	reason.clear();
	if (!game.activeRules.profile().yootWorld || !item) return true;
	const Item::AbstractPrototype * proto = item->prototype;
	int code = 0;

	if (proto->icon == ICON_FLOOR) {
		// Floor.t2p DoDestructProc FUN_1000ce30 over the whole slab.
		FloorRemoval f;
		f.floor = item->position.y;
		const int minX = item->position.x, maxX = item->getRect().maxX();
		f.peopleOnFloor = !item->people.empty();
		Game::ItemSetByInt::const_iterator on = game.itemsByFloor.find(f.floor);
		if (on != game.itemsByFloor.end())
			for (Game::ItemSet::const_iterator i = on->second.begin(); i != on->second.end(); ++i) {
				if (!overlaps(*i, minX, maxX)) continue;
				if ((*i)->canHaulPeople()) f.moverOnFloor = true;
				else f.tenantOnFloor = true;
				if (!(*i)->people.empty()) f.peopleOnFloor = true;
			}
		const int next = f.floor > 0 ? f.floor + 1 : f.floor - 1;
		bool beyond = false;
		Game::ItemSetByInt::const_iterator nb = game.itemsByFloor.find(next);
		if (nb != game.itemsByFloor.end())
			for (Game::ItemSet::const_iterator i = nb->second.begin(); i != nb->second.end(); ++i)
				if (overlaps(*i, minX, maxX)) beyond = true;
		Game::FloorItems::const_iterator slab = game.floorItems.find(next);
		if (slab != game.floorItems.end() && overlaps(slab->second, minX, maxX)) beyond = true;
		f.tenantAbove = f.floor > 0 && beyond;
		f.tenantBelow = f.floor < 0 && beyond;
		code = floorRemoveRule(f);
	} else if (proto->id == "parkinglot" || proto->id == "parkinggate") {
		// Parking.t2p DoDestructProc FUN_1000bf00.
		ParkingPiece piece;
		piece.floor = item->position.y;
		piece.height = item->size.y;
		piece.minX = item->position.x;
		piece.maxX = item->getRect().maxX();
		piece.gate = proto->id == "parkinggate";
		code = parkingRemoveRule(parkingPieces(game, item), piece);
	}
	if (!code) return true;
	reason = errorText(proto, code);
	return false;
}
