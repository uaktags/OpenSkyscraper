#include "../Game.h"
#include "Parking.h"
#include "Office.h"
#include "Hotel.h"
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
// no income is ever booked here.
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
			// Shafts, stairs and escalators have no floor-map record (the EXE keeps
			// them in their own tables), so they never interrupt the parking
			// records underneath: only a genuinely bare run or another tenant does.
			else if (item->canHaulPeople() || item->prototype->id == "floor")      kind = kCellBare;
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

namespace {
// Parking items that currently hold at least one driver. A person's
// destructor consults only this live set, never Game::itemsByType (which
// can still list already-deleted items while the world is torn down).
std::set<Parking *> & activeParkings() { static std::set<Parking *> s; return s; }
}

Parking::~Parking() { activeParkings().erase(this); }

void Parking::forgetDriver(Person * driver)
{
 if (!driver) return;
 std::vector<Parking *> live(activeParkings().begin(), activeParkings().end());
 for (Parking * p : live) p->freeSpace(driver);
}

Parking::CarCategory Parking::driverCategory(Person * driver) const
{
 std::map<Person*, CarCategory>::const_iterator it = drivers.find(driver);
 return it == drivers.end() ? kNoCar : it->second;
}

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

int Parking::totalSpaces() const { return 1; }
int Parking::usedSpaces() const { return static_cast<int>(drivers.size()); }
int Parking::usableSpaces() const { return !bayList.empty() && bayList[0].usable ? 1 : 0; }
bool Parking::hasSpace() const { return usableSpaces() != 0; }
bool Parking::assignSpace(Person *driver, CarCategory cat)
{
 if (!driver || !hasSpace() || !canPark(game,cat)) return false;
 if (drivers.count(driver)) return true;
 drivers[driver]=cat; activeParkings().insert(this);
 Bay &bay=bayList[0]; bay.occupied=true; bay.person=driver; bay.category=cat;
 spriteNeedsUpdate=true; return true;
}
void Parking::freeSpace(Person *driver)
{
 if (!driver || !drivers.erase(driver)) return;
 Bay &bay=bayList[0];
 // An overwritten bay belongs to its latest driver. Earlier owners only
 // release their own category count, never somebody else's displayed car.
 if (bay.person == driver) { bay.occupied=false; bay.person=nullptr; bay.category=kNoCar; }
 if (drivers.empty()) activeParkings().erase(this);
 spriteNeedsUpdate=true;
}
int Parking::usableSpaceCount(Game *game)
{
 int n=0;
 for (auto *item : game->itemsByType["parking"]) if (auto *p=dynamic_cast<Parking*>(item)) if (!p->underConstruction) n+=p->usableSpaces();
 return n;
}
int Parking::carQuota(Game *game, CarCategory cat)
{
 return usableSpaceCount(game) * (cat == kOfficeCar || cat == kSuiteCar ? 2 : 1);
}
int Parking::carsParked(Game *game, CarCategory cat)
{
 int n=0;
 for (auto *item : game->itemsByType["parking"]) if (auto *p=dynamic_cast<Parking*>(item))
  for (auto &driver : p->drivers) if (cat == kNoCar || driver.second == cat) ++n;
 return n;
}
bool Parking::canPark(Game *game, CarCategory cat)
{
 return cat != kNoCar && usableSpaceCount(game)>0 && carsParked(game,cat)<carQuota(game,cat);
}
Parking *Parking::claim(Game *game, Item *destination, Person *driver, CarCategory cat)
{
 recomputeAllChains(game);
 if (!canPark(game,cat)) return nullptr;
 std::vector<Parking*> candidates;
 for (auto *item : game->itemsByType["parking"]) if (auto *p=dynamic_cast<Parking*>(item)) {
  if (!p->underConstruction && p->hasSpace() && game->mainLobby &&
      !game->findRoute(p,game->mainLobby).empty() && !game->findRoute(p,destination).empty()) candidates.push_back(p);
 }
 if (candidates.empty()) return nullptr;
 Parking *p=candidates[rand()%candidates.size()];
 return p->assignSpace(driver,cat) ? p : nullptr;
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
	// Driver association (F4): the owning item's cell + member + category.
	for (std::map<Person*, CarCategory>::const_iterator d = drivers.begin(); d != drivers.end(); ++d) {
		Person * p = d->first;
		if (!p || !p->home) continue;
		xml.OpenElement("car");
		xml.PushAttribute("x", p->home->position.x);
		xml.PushAttribute("y", p->home->position.y);
		xml.PushAttribute("member", p->member);
		xml.PushAttribute("category", (int)d->second);
		xml.CloseElement();
	}
}

void Parking::relinkAll(Game * game)
{
	if (!game) return;
	std::vector<Item *> parks(game->itemsByType["parking"].begin(), game->itemsByType["parking"].end());
	for (Item * item : parks) {
		Parking * park = dynamic_cast<Parking *>(item);
		if (!park) continue;
		for (const PendingCar & car : park->pendingCars) {
			Person * driver = NULL;
			if (car.category == kOfficeCar) {
				for (Item * o : game->itemsByType["office"])
					if (o->position.x == car.x && o->position.y == car.y)
						if (Office * office = dynamic_cast<Office *>(o))
							driver = office->crewMember(car.member);
				if (Office::Worker * w = dynamic_cast<Office::Worker *>(driver)) w->parkingUsed = park;
			} else if (car.category == kSuiteCar) {
				for (Item * h : game->itemsByType["hotel_suite"])
					if (h->position.x == car.x && h->position.y == car.y)
						if (Hotel * hotel = dynamic_cast<Hotel *>(h))
							for (Hotel::Guest * g : hotel->guests)
								if (g->member == car.member) { driver = g; g->parkingUsed = park; g->entry = park; }
			}
			if (!driver || park->drivers.count(driver)) continue;
			park->drivers[driver] = car.category;
			activeParkings().insert(park);
			Bay & bay = park->bayList[0];
			bay.occupied = true; bay.person = driver; bay.category = car.category;
		}
		park->pendingCars.clear();
		park->spriteNeedsUpdate = true;
	}
}

void Parking::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	ensureBays();
	// Saved cars are re-bound to their drivers by relinkAll() once the items
	// and people exist.
 drivers.clear(); activeParkings().erase(this);
 for (auto &bay : bayList) { bay.occupied=false; bay.person=nullptr; bay.category=kNoCar; }
	pendingCars.clear();
	for (tinyxml2::XMLElement * e = xml.FirstChildElement("car"); e; e = e->NextSiblingElement("car")) {
		PendingCar car;
		car.x = e->IntAttribute("x"); car.y = e->IntAttribute("y");
		car.member = e->IntAttribute("member");
		car.category = (CarCategory)e->IntAttribute("category", kOfficeCar);
		pendingCars.push_back(car);
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
 drivers.clear(); activeParkings().erase(this);
 for (Bay &bay : bayList) {
  bay.occupied = false; bay.person = NULL; bay.category = kNoCar;
 }
 spriteNeedsUpdate = true;
 updateSprite();
}
