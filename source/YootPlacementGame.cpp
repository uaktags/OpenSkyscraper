/* Copyright (c) 2026 OpenSkyscraper Project */
#include "YootPlacementGame.h"

#include "Game.h"
#include "Item/Item.h"
#include "YootCatalog.h"
#include "YootPlacement.h"
#include "YootMisc.h"
#include "Item/Floor.h"
#include "Logger.h"

#include <climits>
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
	if (!code && !isMover(proto) && proto->icon != ICON_LOBBY && proto->icon != ICON_FLOOR &&
	    proto->id != "skybridge" && !usesSpecialFootprint(game, proto) && floor != 0) {
		// A hole between two towers (Floor::holes) holds nothing up.
		const int support = floor > 0 ? floor - 1 : floor + height;
		Game::FloorItems::const_iterator slab = game.floorItems.find(support);
		if (slab != game.floorItems.end() && !slab->second->holes.empty())
			for (int cx = x; cx < x + proto->size.x; ++cx)
				if (cx >= slab->second->position.x && cx < slab->second->getRect().maxX() && !slab->second->coversCell(cx))
					return std::string("Floor ") + (floor > 0 ? "below" : "above") + " is not wide enough";
	}

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

namespace
{
	/// T2FloorInfo::GetTenant != 0 for a world cell: a floor slab (Floor
	/// is a tenant in YootTower) or any non-mover item.
	bool cellOccupied(const Game & game, int row, int unit)
	{
		const RulesProfile & p = game.activeRules.profile();
		const int floor = p.groundRow - 1 - row;
		const int x = unit - p.unitOriginX;
		Game::FloorItems::const_iterator slab = game.floorItems.find(floor);
		if (slab != game.floorItems.end() && slab->second->coversCell(x)) return true;
		Game::ItemSetByInt::const_iterator on = game.itemsByFloor.find(floor);
		if (on != game.itemsByFloor.end())
			for (Game::ItemSet::const_iterator i = on->second.begin(); i != on->second.end(); ++i)
				if (!(*i)->canHaulPeople() && x >= (*i)->position.x && x < (*i)->getRect().maxX()) return true;
		return false;
	}

	Item::Item * placeBridge(Game & game, Item::AbstractPrototype * proto, int x, int floor, int width)
	{
		Item::Item * item = game.itemFactory.make(proto, int2(x, floor));
		item->size.x = width;
		game.addItem(item);
		return item;
	}
}

bool Yoot::constructSkyBridge(Game & game, Item::AbstractPrototype * proto, int2 pos)
{
	const RulesProfile & p = game.activeRules.profile();
	const int minRating = game.activeRules.minRatingToBuild(proto->id);
	std::string reason;
	if (minRating == BuildRules::kUnavailable)
		reason = proto->name + " is not available in " + p.worldName;
	else if (minRating > game.rating)
		reason = proto->name + " unlocks at " + std::to_string(minRating + 1) + " stars";
	BuildRect r;
	if (reason.empty()) {
		r.left = p.unitForX(pos.x);
		r.right = r.left + proto->size.x;
		r.bottom = p.rowForFloor(pos.y) + 1;
		r.top = r.bottom - 1;
		BuildRect area;
		area.left = p.buildMinX == INT_MIN ? 0 : p.unitForX(p.buildMinX);
		area.right = p.buildMaxX == INT_MAX ? p.worldWidth : p.unitForX(p.buildMaxX);
		area.top = p.buildTopRow == INT_MIN ? 0 : p.buildTopRow;
		area.bottom = p.buildBottomRow == INT_MAX ? p.groundRow + 64 : p.buildBottomRow;
		const int code = skyBridgeAreaCheck(r, area, p.groundRow,
			[&game](int row, int unit) { return cellOccupied(game, row, unit); });
		if (code) reason = errorText(proto, code);
		if (!reason.empty() && reason[reason.size() - 1] == '.') reason.erase(reason.size() - 1);
	}
	if (!reason.empty()) {
		LOG(DEBUG, "cannot construct sky bridge at %ix%i: %s", pos.x, pos.y, reason.c_str());
		game.playOnce("simtower/construction/impossible");
		game.timeWindow.showMessage("Cannot place item there. " + reason + ".");
		return false;
	}
	// Like every tenant, the bridge gets Floor tenants under its cells (the
	// .bld unit map holds both ids), charged per empty cell: the oracle paid
	// $5,000 + $500 x width ($25,000 for 40 units, $20,000 for 30).
	game.extendFloor(pos.y, r.left - p.unitOriginX, r.right - p.unitOriginX);
	Item::Item * bridge = placeBridge(game, proto, r.left - p.unitOriginX, pos.y, r.right - r.left);
	game.transferFunds(-proto->price, "construction", proto->name); // CalcPayment: flat (no attr 0x2000)
	game.playOnce("simtower/construction/normal");
	LOG(INFO, "sky bridge on floor %d, x %d, %d units", pos.y, bridge->position.x, bridge->size.x);
	return true;
}

void Yoot::skyBridgeFloorBuilt(Game & game, int floor, int minX, int maxX)
{
	if (!game.activeRules.profile().yootWorld) return;
	Game::ItemSetByInt::const_iterator on = game.itemsByFloor.find(floor);
	if (on == game.itemsByFloor.end()) return;
	std::vector<Item::Item *> hit;
	for (Game::ItemSet::const_iterator i = on->second.begin(); i != on->second.end(); ++i)
		if ((*i)->prototype->id == "skybridge" && overlaps(*i, minX, maxX)) hit.push_back(*i);
	for (Item::Item * b : hit) {
		Item::AbstractPrototype * proto = b->prototype;
		const int left = b->position.x, right = b->getRect().maxX();
		const std::vector<std::pair<int, int> > runs = skyBridgeSplit(left, right,
			[minX, maxX](int x) { return x >= minX && x < maxX; });
		game.removeItem(b);
		for (const std::pair<int, int> & run : runs) placeBridge(game, proto, run.first, floor, run.second - run.first);
	}
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
