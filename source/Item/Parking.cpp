#include "../Game.h"
#include "Parking.h"
#include <algorithm>
#include <map>
#include <set>

using namespace OT;
using namespace OT::Item;

// =============================================================================
// EXE-verified parking constants (ground-truth.md; concilia-tower reference)
// =============================================================================

// Merged parking sheet "simtower/parking/space" (0x86A8 + 0x86A9, merged by
// SimTowerLoader as one 15-frame strip): frame 0 = empty bay, frame 1 =
// red X (unusable space), frames 2-14 = the 13 parked-car variants.
static const int kParkingFrameEmpty    = 0;
static const int kParkingFrameCrossed  = 1;
static const int kParkingFrameFirstCar = 2;
static const int kParkingCarVariants   = 13;   // frames 2..14

// One parking space per 4 cells (32 px) - the EXE "Parking Space" record
// width; our 8-cell item renders and admits two spaces.
static const int kBayWidthCells = 4;
static const int kBayWidthPx    = 32;
static const int kBayHeightPx   = 24;

// A bare-deck run of this many cells between the ramp and a space cuts the
// drive path (ParkingT CheckAllParking sweep; concilia game.c
// game_parking_recompute).
static const int kRampGapLimitCells = 4;

// Quarterly upkeep PER RAMP: maintenance table 0x3EA item 44 = 10000 x $100
// (byte-verified). Spaces and cars cost nothing - ParkingT InParkingCar
// (1198:031a) makes zero MoneyT calls, which is why
// dailyMaintenanceCost() returns 0 and no income is ever booked here.
const int Parking::kRampUpkeepPerQuarter = 10000;

// =============================================================================
// Ramp chain + usability sweep (ParkingT CheckAllParking)
// =============================================================================

namespace {

struct RampRec {
	int x;      ///< left cell of the ramp strip
	int width;  ///< strip width in cells
};

typedef std::map<int, RampRec> RampByFloor;      ///< basement floor -> ramp
typedef std::map<int, std::map<int, int> > FloorCellKinds; ///< floor -> cell -> kind

// Cell kinds for the drive sweep (SetParkingUsable 1198:09ce): 0 = bare deck
// (counts toward the gap limit), 1 = parking (continues the path), 2 = any
// other record (kills the path). Elevator shafts are no floor-map record in
// the EXE, so they read as bare deck here.
enum {
	kCellBare    = 0,
	kCellParking = 1,
	kCellBlocker = 2
};

const Game::ItemSet * itemsOfType(Game * game, const char * type)
{
	Game::ItemSetByString::const_iterator it = game->itemsByType.find(type);
	return (it == game->itemsByType.end()) ? NULL : &it->second;
}

// Comparator for the fill-ordinal pass: B1 first, then left to right, then
// deeper basements (deterministic tower-wide bay order).
bool parkOrdinalLess(const Parking * a, const Parking * b)
{
	if (a->position.y != b->position.y) return a->position.y > b->position.y; // B1 before B2...
	return a->position.x < b->position.x;
}

// Layout signature for cheap change detection in Parking::advance. The EXE
// recomputes the chain on build/demolish plus a daily 7AM sweep; we watch
// the item counts (any build/demolish shifts them) and the sim hour (which
// also catches construction completions, at finer granularity than 7AM).
struct LayoutSignature {
	size_t items, ramps, parks;
	long hourStamp;
	bool valid;
	LayoutSignature() : items(0), ramps(0), parks(0), hourStamp(0), valid(false) {}
};

std::map<Game *, LayoutSignature> s_layoutSignatures;

} // namespace

// Rebuild the ramp chain and every Parking item's bay usability. A floor is
// chained iff B1 has a ramp and each deeper basement floor has a ramp at the
// SAME x as the floor above it (ramps stack vertically into one column;
// concilia game.c game_parking_recompute, byte-verified 2026-07-11).
void Parking::recomputeAllChains(Game * game)
{
	// 1. Collect completed ramps by floor. The EXE build gate allows only
	//    one ramp per floor; if several exist here anyway, the leftmost
	//    anchors the chain (deterministic).
	RampByFloor ramps;
	if (const Game::ItemSet * rampItems = itemsOfType(game, "parkingramp")) {
		for (Game::ItemSet::const_iterator it = rampItems->begin(); it != rampItems->end(); ++it) {
			const Item * item = *it;
			if (item->underConstruction) continue; // construction sites never chain
			RampRec rec = { item->position.x, item->size.x };
			std::pair<RampByFloor::iterator, bool> ins =
				ramps.insert(std::make_pair(item->position.y, rec));
			if (!ins.second && rec.x < ins.first->second.x) ins.first->second = rec;
		}
	}

	// 2. Chain the floors downward from the B1 anchor (-1): a floor without
	//    a ramp, or one whose ramp sits at a different x than the chained
	//    floor above, ends the chain.
	std::set<int> chained;
	for (int floor = -1; ramps.count(floor) != 0; --floor) {
		if (floor < -1) {
			if (chained.count(floor + 1) == 0) break;
			if (ramps[floor].x != ramps[floor + 1].x) break; // not same-x consecutive
		}
		chained.insert(floor);
	}

	// 3. Cell-kind map for the drive sweep, basement floors only.
	FloorCellKinds kinds;
	for (Game::ItemSetByInt::const_iterator fi = game->itemsByFloor.begin();
	     fi != game->itemsByFloor.end(); ++fi) {
		const int floor = fi->first;
		if (floor >= 0) continue; // parking lives underground only
		std::map<int, int> & row = kinds[floor];
		for (Game::ItemSet::const_iterator it = fi->second.begin(); it != fi->second.end(); ++it) {
			const Item * item = *it;
			int kind;
			if (item->prototype->id == "parking") kind = kCellParking;
			else if (item->isElevator())         kind = kCellBare; // shafts: no floor-map record
			else                                 kind = kCellBlocker;
			for (int cx = item->position.x; cx < item->position.x + item->size.x; ++cx) {
				std::map<int, int>::iterator k = row.find(cx);
				if (k == row.end()) row[cx] = kind;
				else if (k->second < kind) k->second = kind; // blocker > parking > bare
			}
		}
	}

	// 4. Recompute every Parking item's bays, ordered B1-first then left to
	//    right, assigning tower-wide ordinals so cars fill by ordinal.
	std::vector<Parking *> parks;
	if (const Game::ItemSet * parkingItems = itemsOfType(game, "parking")) {
		for (Game::ItemSet::const_iterator it = parkingItems->begin(); it != parkingItems->end(); ++it) {
			Parking * park = dynamic_cast<Parking *>(*it);
			if (park) parks.push_back(park);
		}
	}
	std::sort(parks.begin(), parks.end(), parkOrdinalLess);

	int ordinal = 0;
	for (std::vector<Parking *>::iterator pi = parks.begin(); pi != parks.end(); ++pi) {
		Parking * park = *pi;
		const int floor = park->position.y;
		const bool floorChained = (floor < 0) && chained.find(floor) != chained.end();
		RampByFloor::const_iterator rampIt = floorChained ? ramps.find(floor) : ramps.end();
		FloorCellKinds::iterator rowIt = floorChained ? kinds.find(floor) : kinds.end();
		const RampRec * ramp = (rampIt != ramps.end()) ? &rampIt->second : NULL;
		std::map<int, int> * row = (rowIt != kinds.end()) ? &rowIt->second : NULL;

		for (int b = 0; b < (int)park->bayList.size(); ++b) {
			Bay & bay = park->bayList[b];
			bool ok = floorChained && ramp && row && !park->underConstruction;
			if (ok) {
				// Drive sweep from the ramp's edge to the bay's first cell:
				// parking continues the path, a >= kRampGapLimitCells bare
				// run severs it, any other record blocks it outright.
				const int bayX = park->position.x + b * kBayWidthCells;
				const int step = (bayX > ramp->x) ? 1 : -1;
				int cx = (step > 0) ? ramp->x + ramp->width : ramp->x - 1;
				int gap = 0;
				for (; ok && cx != bayX; cx += step) {
					std::map<int, int>::const_iterator k = row->find(cx);
					const int kind = (k == row->end()) ? kCellBare : k->second;
					if (kind == kCellParking) gap = 0;
					else if (kind == kCellBare) { if (++gap >= kRampGapLimitCells) ok = false; }
					else ok = false;
				}
			}
			bay.usable = ok;
			bay.ordinal = ok ? ordinal++ : 0;
			// An occupied bay stranded off-chain keeps its car (the driver
			// frees it when they leave); it just draws the red X.
		}
		park->spriteNeedsUpdate = true;
	}
}


// =============================================================================
// Parking
// =============================================================================

Parking::~Parking() {}

void Parking::init()
{
	Item::init();

	// Fresh placements are always marked underConstruction right after
	// init() by Factory::make; remember that so the first advance() after
	// completion forces a chain recompute (this item just came online).
	wasUnderConstruction = true;

	ensureBays();
	updateSprite();
}

int Parking::totalSpaces() const
{
	return std::max(1, (size.x + kBayWidthCells - 1) / kBayWidthCells);
}

int Parking::usedSpaces() const
{
	int used = 0;
	for (std::vector<Bay>::const_iterator b = bayList.begin(); b != bayList.end(); ++b)
		if (b->occupied) ++used;
	return used;
}

int Parking::usableSpaces() const
{
	int usable = 0;
	for (std::vector<Bay>::const_iterator b = bayList.begin(); b != bayList.end(); ++b)
		if (b->usable) ++usable;
	return usable;
}

bool Parking::hasSpace() const
{
	for (std::vector<Bay>::const_iterator b = bayList.begin(); b != bayList.end(); ++b)
		if (b->usable && !b->occupied) return true;
	return false;
}

bool Parking::assignSpace(Person * driver, CarCategory cat)
{
	if (!hasSpace()) return false;
	// EXE admission valve: the category quota of 2 x usable spaces
	// tower-wide (ParkingT SetupGateCount). With the B2 fix the free-bay
	// check above is usually the tighter constraint.
	if (cat != kNoCar && carsParked(game, cat) >= carQuota(game)) return false;
	for (int b = 0; b < (int)bayList.size(); ++b) {
		Bay & bay = bayList[b];
		// B2 divergence: an occupied bay (person != none) is never
		// assignable - the EXE would double-park the physical space.
		if (!bay.usable || bay.occupied) continue;
		bay.occupied = true;
		bay.person = driver;
		bay.category = cat;
		spriteNeedsUpdate = true;
		return true;
	}
	return false;
}

void Parking::freeSpace(Person * driver)
{
	for (int b = 0; b < (int)bayList.size(); ++b) {
		Bay & bay = bayList[b];
		if (!bay.occupied) continue;
		if (driver && bay.person != driver) continue;
		bay.occupied = false;
		bay.person = NULL;
		bay.category = kNoCar;
		spriteNeedsUpdate = true;
		return;
	}
}

int Parking::usableSpaceCount(Game * game)
{
	int usable = 0;
	if (const Game::ItemSet * parkingItems = itemsOfType(game, "parking")) {
		for (Game::ItemSet::const_iterator it = parkingItems->begin(); it != parkingItems->end(); ++it) {
			const Parking * park = dynamic_cast<const Parking *>(*it);
			if (park && !park->underConstruction) usable += park->usableSpaces();
		}
	}
	return usable;
}

int Parking::carQuota(Game * game)
{
	// 2 x usable spaces PER CATEGORY (office and suite each; concilia
	// people.c people_parking_assign, EXE SetupGateCount quota table).
	return 2 * usableSpaceCount(game);
}

int Parking::carsParked(Game * game, CarCategory cat)
{
	int cars = 0;
	if (const Game::ItemSet * parkingItems = itemsOfType(game, "parking")) {
		for (Game::ItemSet::const_iterator it = parkingItems->begin(); it != parkingItems->end(); ++it) {
			const Parking * park = dynamic_cast<const Parking *>(*it);
			if (!park) continue;
			const std::vector<Bay> & bays = park->bays();
			for (std::vector<Bay>::const_iterator b = bays.begin(); b != bays.end(); ++b)
				if (b->occupied && (cat == kNoCar || b->category == cat)) ++cars;
		}
	}
	return cars;
}

bool Parking::canPark(Game * game, CarCategory cat)
{
	if (cat == kNoCar) return false;
	// Quota room AND a physical free usable bay (the B2 fix makes the bay
	// the binding scarcity once a category fills the usable spaces 1:1).
	if (carsParked(game, cat) >= carQuota(game)) return false;
	if (const Game::ItemSet * parkingItems = itemsOfType(game, "parking")) {
		for (Game::ItemSet::const_iterator it = parkingItems->begin(); it != parkingItems->end(); ++it) {
			const Parking * park = dynamic_cast<const Parking *>(*it);
			if (park && !park->underConstruction && park->hasSpace()) return true;
		}
	}
	return false;
}

void Parking::advance(double dt)
{
	// Recompute the ramp chain when the layout changed (any build/demolish
	// shifts an item count) or on the sim-hour tick (catches construction
	// completions and mirrors the EXE's daily CheckAllParking sweep).
	LayoutSignature sig;
	sig.valid = true;
	sig.items = game->items.size();
	if (const Game::ItemSet * rampItems = itemsOfType(game, "parkingramp"))
		sig.ramps = rampItems->size();
	if (const Game::ItemSet * parkingItems = itemsOfType(game, "parking"))
		sig.parks = parkingItems->size();
	sig.hourStamp = (long)game->time.day * 100 + (long)game->time.hour;

	const bool justBuilt = wasUnderConstruction && !underConstruction;
	wasUnderConstruction = underConstruction;

	LayoutSignature & cached = s_layoutSignatures[game];
	if (justBuilt || !cached.valid || cached.items != sig.items ||
	    cached.ramps != sig.ramps || cached.parks != sig.parks ||
	    cached.hourStamp != sig.hourStamp) {
		recomputeAllChains(game);
		cached = sig;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

void Parking::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("used", usedSpaces());
}

void Parking::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	ensureBays();
	// Legacy "used": older saves stored an anonymous occupied-slot count
	// against the retired 2-spaces-per-tile model. Clamp to the bay count
	// and park that many cars; the driver handle is lost with the old
	// model, so the bays are released by the ordinal fallback in
	// freeSpace(NULL). Category is unknown - count them as office cars.
	int used = xml.IntAttribute("used");
	if (used > (int)bayList.size()) used = (int)bayList.size();
	for (int b = 0; b < used; ++b) {
		bayList[b].occupied = true;
		bayList[b].person = NULL;
		bayList[b].category = kOfficeCar;
	}
	updateSprite();
}

void Parking::ensureBays()
{
	const int count = totalSpaces();
	if ((int)bayList.size() == count && (int)baySprites.size() == count) return;
	bayList.assign(count, Bay());
	for (int b = 0; b < count; ++b) {
		// Deterministic per-space id: the bay's physical cell anchor (x,
		// floor). Stable across save/load, unlike a creation counter.
		const long spaceId = (long)(position.x + b * kBayWidthCells) * 128 - (long)position.y;
		const int variant = (int)(((spaceId % kParkingCarVariants) + kParkingCarVariants) % kParkingCarVariants);
		bayList[b].variant = kParkingFrameFirstCar + variant; // 2 + id % 13
	}
	rebuildBaySprites();
}

void Parking::rebuildBaySprites()
{
	for (std::vector<Sprite>::iterator s = baySprites.begin(); s != baySprites.end(); ++s)
		removeSprite(&*s);
	baySprites.clear();
	baySprites.resize(bayList.size());
	sf::Texture & texture = App->bitmaps.editionTexture("parking", "simtower/parking/space");
	for (std::vector<Sprite>::iterator s = baySprites.begin(); s != baySprites.end(); ++s) {
		s->setTexture(texture);
		s->setOrigin({0.f, 24.f});
		addSprite(&*s);
	}
	spriteNeedsUpdate = true;
}

void Parking::updateSprite()
{
	ensureBays();
	spriteNeedsUpdate = false;
	for (int b = 0; b < (int)bayList.size() && b < (int)baySprites.size(); ++b) {
		const Bay & bay = bayList[b];
		// Frame pick (concilia main.c, ITEM_PARKING render): red X when
		// off the chain, else the car variant while occupied, else the
		// empty bay.
		const int frame = !bay.usable ? kParkingFrameCrossed
		                : bay.occupied ? bay.variant
		                : kParkingFrameEmpty;
		baySprites[b].setTextureRect(sf::IntRect({frame * kBayWidthPx, 0}, {kBayWidthPx, kBayHeightPx}));
		baySprites[b].setPosition({static_cast<float>((position.x + b * kBayWidthCells) * 8),
		                           static_cast<float>(position.y * -36)});
	}
}

void Parking::evacuateForDisaster()
{
 for (Bay &bay : bayList) {
  bay.occupied = false; bay.person = NULL; bay.category = kNoCar;
 }
 spriteNeedsUpdate = true;
 updateSprite();
}
