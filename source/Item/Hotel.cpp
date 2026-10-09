/* Copyright © 2024 */
#include "../Game.h"
#include "../Math/Rand.h"
#include "../Time.h"
#include "Factory.h"
#include "Hotel.h"
#include "Parking.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

using namespace OT;
using namespace OT::Item;
// Shared rent-table writer implemented by Game.cpp (resource 0x7f0b/0x3e9).
extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);
extern int openSkyDefaultRentClass(const std::string &prototypeId);

Hotel::~Hotel() { clearAll(false); } // Game::removeItem already took the hosted CountT contribution
int Hotel::capacity() const { return variant == kSingle ? 1 : 2; }
void Hotel::setRentClass(int c) { tenant.rentClass = std::max(0, std::min(3, c)); }

AbstractPrototype * Hotel::makeSinglePrototype()
{
	AbstractPrototype * p = new Prototype<Hotel>;
	p->id             = "hotel_single";
	p->name           = "Single Hotel Room";
	p->price          = 20000;
	p->size           = int2(4, 1);
	p->icon           = ICON_HOTEL_SINGLE;
	p->variant        = kSingle;
	p->entrance_offset = 0;
	p->exit_offset    = 0;
	return p;
}

AbstractPrototype * Hotel::makeDoublePrototype()
{
	AbstractPrototype * p = new Prototype<Hotel>;
	p->id             = "hotel_double";
	p->name           = "Double Hotel Room";
	p->price          = 50000;
	p->size           = int2(6, 1);
	p->icon           = ICON_HOTEL_DOUBLE;
	p->variant        = kDouble;
	p->entrance_offset = 0;
	p->exit_offset    = 0;
	return p;
}

AbstractPrototype * Hotel::makeSuitePrototype()
{
	AbstractPrototype * p = new Prototype<Hotel>;
	p->id             = "hotel_suite";
	p->name           = "Hotel Suite";
	p->price          = 100000;
	p->size           = int2(10, 1);
	p->icon           = ICON_HOTEL_SUITE;
	p->variant        = kSuite;
	p->entrance_offset = 0;
	p->exit_offset    = 0;
	return p;
}

void Hotel::applyVariant()
{
	int2 sz(8, 1);
	const char *tex = "simtower/double";
	const char *edition = "hotel_double";
	int frameWidth = 48;
	switch (variant)
	{
		case kSingle: sz = int2(4, 1); tex = "simtower/single"; edition = "hotel_single"; frameWidth = 32; break;
		case kDouble: sz = int2(6, 1); tex = "simtower/double"; edition = "hotel_double"; frameWidth = 48; break;
		case kSuite:  sz = int2(10, 1); tex = "simtower/suite";  edition = "hotel_suite";  frameWidth = 80; break;
	}
	size = sz;
	sprite.setTexture(App->bitmaps.editionTexture(edition, tex));
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	(void)frameWidth;
}

void Hotel::init()
{
	Item::init();

	variant = prototype->variant;
	subVariant = rand() % 2;
	roomState = kClean;
	dirtySince = 0;
	hosted = 0;
	dinnerSent = evacuated = false;

	applyVariant();
	addSprite(&sprite);
	spriteNeedsUpdate = false;
	roomFrameIndex = -1;

	updateSprite();
}

// EXE hotel sheet: door (col 0) + 8 room frames as state x day/night
// pairs — 1/2 occupied, 3/4 clean, 5/6 dirty, 7/8 infested (status byte
// bands 0x10..0x40; day frame first, night second). Night art runs from
// 17:00 to 5:00. Clean vacant rooms clamp to the lights-out band
// (occupied-night art) between 4AM and 7AM (<0x18 -> 0x10).
static int hotelRoomFrame(int roomState, double hour)
{
	bool night = (hour >= 17 || hour < 5);
	switch (roomState)
	{
		case Hotel::kInfested: return night ? 8 : 7; // cockroaches
		case Hotel::kDirty:    return night ? 6 : 5; // needs housekeeping
		case Hotel::kOccupied: return night ? 2 : 1;
	}
	if (hour >= 4 && hour < 7) return 2; // 4AM lights-out clamp
	return night ? 4 : 3;
}

void Hotel::updateSprite()
{
	spriteNeedsUpdate = false;
	int frameWidth = 48;
	switch (variant)
	{
		case kSingle: frameWidth = 32; break;
		case kDouble: frameWidth = 48; break;
		case kSuite:  frameWidth = 80; break; // 9 cols: door + 8 state frames
	}

	// Sheet rows are style variants (single 2, double 4 = two style
	// families, suite base + style); the room state selects the column.
	int row = subVariant;
	if (variant == kDouble)
		row *= 2;

	int col = hotelRoomFrame(roomState, game->time.hour);
	roomFrameIndex = col;
	sprite.setTextureRect(sf::IntRect({col * frameWidth, row * 24}, {frameWidth, 24}));
}

void Hotel::resetBanks()
{
 for (auto &b : tenant.bank) { b.count = 0; b.total = 0; }
 for (Guest *g : guests) g->bankCount = g->bankTotal = 0;
}
void Hotel::clearAll(bool adjustPopulation)
{
 if (hosted) { if (adjustPopulation) game->addPopulation(variant + 1, -hosted); hosted = population = 0; }
 for (Guest *g : guests) {
  game->vipSystem.voidPerson(g);
  if (g->parkingUsed) g->parkingUsed->freeSpace(g);
  g->journey.cancel(); delete g;
 }
 guests.clear();
}

bool Hotel::checkIn()
{
 if (!tenant.armed || roomState != kClean || hosted || evacuated || !guests.empty()) return false;
 std::vector<Guest*> arrivals;
 for (int k=0; k<capacity(); ++k) {
  Guest *g = new Guest(this, k);
  arrivals.push_back(g);
  g->entry = game->mainLobby;
  if (variant == kSuite && game->rating >= 2) {
   g->parkingUsed = Parking::claim(game, this, g, Parking::kSuiteCar);
   if (g->parkingUsed) g->entry = g->parkingUsed;
   else {
    for (Guest *a : arrivals) { if (a->parkingUsed) a->parkingUsed->freeSpace(a); delete a; }
    return false;
   }
  }
  Route route = game->findRoute(g->entry, this);
  if (route.empty()) {
   g->recordNoRoute(g->entry ? g->entry->position.y : 0, position.y);
   for (Guest *a : arrivals) { if (a->parkingUsed) a->parkingUsed->freeSpace(a); delete a; }
   return false;
  }
 }
 dinnerSent = false;
 for (Guest *g : arrivals) guests.insert(g);
 // Bind before the first completed inbound trip is banked.
 for (Guest *g : arrivals) {
  game->vipSystem.bindGuest(g);
  g->state = Person::kCommuting;
  g->journey.set(game->findRoute(g->entry, this));
 }
 return true;
}

void Hotel::checkout(bool paid)
{
 if (hosted) {
  game->addPopulation(variant + 1, -hosted);
  hosted = population = 0;
  if (paid) game->transferFunds(openSkyQuarterlyRent(prototype->id, tenant.rentClass), "hotel_income", "Hotel room checkout");
 }
 tenant.armed = false;
 roomState = kDirty;
 dirtySince = game->time.absolute;
 spriteNeedsUpdate = true;
 for (Guest *g : guests) {
  if (g->leaving) continue;
  game->vipSystem.checkout(g, paid);
  Item *origin = g->at ? g->at : this;
  Item *exit = g->entry && game->items.count(g->entry) ? g->entry : game->mainLobby;
  Route route = game->findRoute(origin, exit);
  g->journey.cancel(); g->leaving = true; g->atHotel = false;
  g->state = Person::kReturning; g->goingTo = g->parkingUsed ? "Parking Space" : "Lobby to go home";
  if (!route.empty()) g->journey.set(route);
  else {
   // No way out: the guest still leaves, so the bay must not leak (F7).
   if (g->parkingUsed) { g->parkingUsed->freeSpace(g); g->parkingUsed = nullptr; }
   game->vipSystem.voidPerson(g);
  }
 }
}

void Hotel::stampInfested()
{
 if (hosted || !guests.empty()) { checkout(false); clearAll(); }
 roomState = kInfested; tenant.category = 255; tenant.armed = false;
 tenant.redraw = true; spriteNeedsUpdate = true;
}

namespace {
std::vector<OT::Item::Item*> hotelSlots(const Game::ItemSet &items)
{
 std::vector<OT::Item::Item*> slots;
 for (auto *item : items) if (!item->isElevator()) slots.push_back(item);
 std::sort(slots.begin(), slots.end(), [](auto *a, auto *b) {
  if (a->position.x != b->position.x) return a->position.x < b->position.x;
  return a->prototype->id < b->prototype->id;
 });
 return slots;
}
}
void Hotel::spreadRoaches(Game *game)
{
 for (auto &floor : game->itemsByFloor) {
  auto slots = hotelSlots(floor.second);
  for (size_t i=0; i<slots.size();) {
   auto *h = dynamic_cast<Hotel*>(slots[i]);
   if (!h || h->roomState < kInfested) { ++i; continue; }
   if (i && dynamic_cast<Hotel*>(slots[i-1])) static_cast<Hotel*>(slots[i-1])->stampInfested();
   ++i;
   if (i >= slots.size()) break;
   auto *next = dynamic_cast<Hotel*>(slots[i]);
   if (!next) ++i;
   else if (next->roomState < kInfested) { next->stampInfested(); ++i; }
  }
 }
}
void Hotel::judgeAll(Game *game)
{
 for (auto &floor : game->itemsByFloor) {
  auto slots = hotelSlots(floor.second);
  std::vector<Hotel*> hotels;
  for (auto *item : slots) if (auto *h = dynamic_cast<Hotel*>(item)) if (!h->underConstruction) hotels.push_back(h);
  for (auto *h : hotels) game->judgeSystem.judgeOne(h);
  for (auto *h : hotels) if (h->roomState >= kDirty) {
   if (h->tenant.armed) { h->tenant.armed = false; h->tenant.category = 0; h->tenant.tenure = 0; }
   else h->tenant.tenure = static_cast<unsigned char>(h->tenant.tenure + 1);
   if (h->tenant.tenure == 3) h->stampInfested();
  }
  for (auto *h : hotels) if (h->roomState < kDirty) {
   if (static_cast<signed char>(h->tenant.category) >= 1) { h->tenant.armed = true; h->resetBanks(); }
   else if (h->tenant.category == 0) {
    Hotel *peer = nullptr;
    for (auto *p : hotels) if (p != h && p->prototype->id == h->prototype->id && p->tenant.category == 2) { peer=p; break; }
    h->tenant.armed = peer != nullptr;
    if (peer) { h->tenant.category = peer->tenant.category = 1; h->resetBanks(); }
   }
  }
 }
}
void Hotel::advance(double)
{
 if ((game->time.checkHour(5) || (game->time.hour >= 5 && game->time.hour < 7)) && hosted) checkout(true);
 const long visits = visitsThisTick(game, 16, 0);
 for (long v=0; v<visits; ++v) {
  if (game->time.hour >= 17 && game->time.hour < 21) {
   if (tenant.armed && roomState == kClean && hosted == 0 && guests.empty() && !evacuated && rand()%8 == 0) checkIn();
   if (hosted && !dinnerSent && game->tenantSlotIndex(this)%2 == 0 && rand()%6 == 0) {
    dinnerSent = true;
    for (Guest *g : guests) if (g->at == this && !g->leaving) {
     Item *venue = game->pickVenue(position.y, 1);
     if (venue) {
      Route r = game->findRoute(this, venue);
      if (!r.empty()) { g->state = Person::kShopping; g->goingTo="Restaurant"; g->journey.set(r); }
     }
     break;
    }
   }
  }
 }
 for (auto it=guests.begin(); it!=guests.end();) {
  Guest *g = *it;
  if (g->leaving && (!g->at || g->at == g->entry) && (!g->journey.item || g->journey.atFinalNode())) {
   if (g->parkingUsed) g->parkingUsed->freeSpace(g);
   g->journey.cancel(); it=guests.erase(it); delete g;
  } else {
   if (!g->leaving && g->at == this) g->state = (game->time.hour >= 23 || game->time.hour < 6) ? Person::kResting : Person::kHome;
   ++it;
  }
 }
 if (spriteNeedsUpdate || roomFrameIndex != hotelRoomFrame(roomState, game->time.hour)) updateSprite();
}
void Hotel::addPerson(Person *p)
{
 Item::addPerson(p);
 if (auto *g = dynamic_cast<Guest*>(p)) {
  if (g->hotel != this || g->leaving) return;
  g->atHotel=true; g->state=Person::kHome;
  if (!hosted) { hosted=capacity(); population=hosted; game->addPopulation(variant+1, hosted); }
  roomState=kOccupied; spriteNeedsUpdate=true;
 }
}
void Hotel::removePerson(Person *p)
{
 Item::removePerson(p);
 if (auto *g=dynamic_cast<Guest*>(p)) g->atHotel=false;
 spriteNeedsUpdate=true;
}
void Hotel::encodeXML(tinyxml2::XMLPrinter &xml)
{
 Item::encodeXML(xml);
 xml.PushAttribute("variant",variant); xml.PushAttribute("subVariant",subVariant);
 xml.PushAttribute("roomState",roomState); xml.PushAttribute("hosted",hosted);
 xml.PushAttribute("dirtySince",dirtySince); xml.PushAttribute("dinnerSent",dinnerSent);
 for (Guest *g : guests) {
  xml.OpenElement("guest"); g->encodeXML(xml);
  xml.PushAttribute("member",g->member); xml.PushAttribute("atHotel",g->atHotel);
  xml.PushAttribute("leaving",g->leaving);
  xml.CloseElement();
 }
}
void Hotel::decodeXML(tinyxml2::XMLElement &xml)
{
 clearAll(); Item::decodeXML(xml);
 variant=xml.IntAttribute("variant",prototype->variant); subVariant=xml.IntAttribute("subVariant",0);
 roomState=xml.IntAttribute("roomState",kClean); hosted=xml.IntAttribute("hosted",0); population=hosted;
 dirtySince=xml.DoubleAttribute("dirtySince",0); dinnerSent=xml.BoolAttribute("dinnerSent",false);
 applyVariant();
 int k=0;
 for (auto *e=xml.FirstChildElement("guest"); e; e=e->NextSiblingElement("guest")) {
  Guest *g=new Guest(this,e->IntAttribute("member",k++)); g->decodeXML(*e);
  g->atHotel=e->BoolAttribute("atHotel",false); g->leaving=e->BoolAttribute("leaving",false);
  g->entry=game->mainLobby;
  guests.insert(g);
  if (g->atHotel) Item::addPerson(g);
 }
 spriteNeedsUpdate=true; updateSprite();
}
Hotel::Guest::Guest(Hotel *item, int memberIndex) : Person(item->game, kMan), hotel(item)
{
 home=item; member=memberIndex;
 bankCount=item->tenant.bank[memberIndex].count; bankTotal=item->tenant.bank[memberIndex].total;
 from="City"; goingTo=item->prototype->name;
}
Path Hotel::getRandomBackgroundSoundPath()
{
 return roomState == kOccupied ? Path("simtower/hotel") : Path("");
}
void Hotel::evacuateForDisaster()
{
 const bool occupied=hosted != 0;
 clearAll(); evacuated=true;
 if (occupied) { roomState=kDirty; dirtySince=game->time.absolute; }
 spriteNeedsUpdate=true; updateSprite();
}
