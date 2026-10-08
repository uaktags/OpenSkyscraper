/* Copyright © 2024 */
#include "../Game.h"
#include "../YootRules.h"
#include "../Math/Rand.h"
#include "../Time.h"
#include "Factory.h"
#include "Hotel.h"
#include "Parking.h"
#include <cmath>
#include <string>

using namespace OT;
using namespace OT::Item;
// Shared rent-table writer implemented by Game.cpp (resource 0x7f0b/0x3e9).
extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);

// Find a reachable Parking with a free slot and assign it. Mirrors the
// FastFood/Restaurant iteration pattern. Returns the claimed Parking or NULL.
static Parking * claimReachableParking(Game * game, OT::Item::Item * origin)
{
	const Game::ItemSet &parkings = game->itemsByType["parking"];
	for (Game::ItemSet::const_iterator it = parkings.begin(); it != parkings.end(); ++it)
	{
		Parking *park = dynamic_cast<Parking *>(*it);
		if (!park || !park->hasSpace()) continue;
		if (game->findRoute(origin, park).empty()) continue;
		if (park->assignSpace())
		{
			park->spriteNeedsUpdate = true;
			return park;
		}
	}
	return NULL;
}

static void releaseParking(Parking * park)
{
	if (!park) return;
	park->freeSpace();
	park->spriteNeedsUpdate = true;
}

Hotel::~Hotel()
{
	clearAll();
}

int Hotel::capacity() const
{
	switch (variant)
	{
		case kSingle: return 1;
		case kDouble: return 2;
		case kSuite:  return 3;
	}
	return 1;
}

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
	p->size           = int2(7, 1);
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
		case kSuite:  sz = int2(7, 1); tex = "simtower/suite";  edition = "hotel_suite";  frameWidth = 80; break;
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
	housekeeper = NULL;

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

void Hotel::scheduleGuest(Guest *g)
{
	double today = floor(game->time.absolute);

	// Arrival between 17:00 and 18:00.
	double arrivalHour = Math::randd(Time::hourToAbsolute(17), Time::hourToAbsolute(18));
	g->arrivalTime = today + arrivalHour;

	// Dinner: leave ~0.5h after arrival, return ~1h later.
	g->dinnerLeaveTime = g->arrivalTime + Time::hourToAbsolute(0.5);
	g->dinnerReturnTime = g->dinnerLeaveTime + Time::hourToAbsolute(1.0);

	// Sleep: between 23:00 and 1:30 next day.
	double nextDay = today + 1.0;
	g->sleepTime = today + Math::randd(Time::hourToAbsolute(23), 1.0 + Time::hourToAbsolute(1.5));

	// Wake between 6:00 and 8:00 next morning.
	g->wakeTime = nextDay + Math::randd(Time::hourToAbsolute(6), Time::hourToAbsolute(8));

	// Checkout between 8:00 and 10:00 next morning.
	g->checkoutTime = nextDay + Math::randd(Time::hourToAbsolute(8), Time::hourToAbsolute(10));

	g->atHotel = false;
}
void Hotel::clearAll()
{
	for (std::set<Guest *>::iterator it = guests.begin(); it != guests.end(); ++it)
	{
		Guest *guest = *it;
		releaseParking(guest->parkingUsed);
		guest->parkingUsed = NULL;
		if (guest->at == this)
			Item::removePerson(guest);
		delete guest;
	}
	guests.clear();
	while (!arrivingGuests.empty())
		arrivingGuests.pop();
	if (housekeeper)
	{
		if (housekeeper->at == this)
			Item::removePerson(housekeeper);
		delete housekeeper;
		housekeeper = NULL;
	}
	roomState = kClean;
}

void Hotel::advance(double dt)
{
	// Open at 17:00 — clean up yesterday's leftovers and spawn new guests.
	if (game->time.checkHour(17))
	{
		clearAll();
		roomState = kClean;
		spriteNeedsUpdate = true;

		if (!lobbyRoute.empty())
		{
			int n = capacity();
			for (int i = 0; i < n; i++)
			{
				Guest *g = new Guest(this);
				scheduleGuest(g);
				guests.insert(g);
				arrivingGuests.push(g);
			}
		}
	}

	// Dispatch arriving guests whose time has come.
	while (!arrivingGuests.empty())
	{
		Guest *g = arrivingGuests.top();
		if (game->time.absolute > g->arrivalTime && !lobbyRoute.empty())
		{
			arrivingGuests.pop();
			g->journey.set(lobbyRoute);
			// Drive in: claim a reachable parking slot for the car.
			if (!g->parkingUsed)
				g->parkingUsed = claimReachableParking(game, this);
		}
		else
			break;
	}

	// Process guest schedules.
	double t = game->time.absolute;
	for (std::set<Guest *>::iterator it = guests.begin(); it != guests.end(); )
	{
		Guest *g = *it;

		// Return from dinner.
		if (!g->atHotel && t >= g->dinnerReturnTime && t < g->sleepTime)
		{
			if (g->at == NULL)
			{
				Item::addPerson(g);
				g->atHotel = true;
				g->state = Person::kHome;
				g->eval = 50;
				roomState = kOccupied;
				spriteNeedsUpdate = true;
			}
		}
		// Leave for dinner.
		else if (g->atHotel && g->state == Person::kHome && t >= g->dinnerLeaveTime && t < g->dinnerReturnTime)
		{
			Item::removePerson(g);
			g->atHotel = false;
			g->state = Person::kShopping;
			g->from = prototype->name;
			g->goingTo = "Restaurant";

			Route bestRoute;
			int bestScore = 0;
			const Game::ItemSet &restaurants = game->itemsByType["restaurant"];
			for (Game::ItemSet::const_iterator i = restaurants.begin(); i != restaurants.end(); i++)
			{
				Route candidate = game->findRoute(this, *i);
				if (!candidate.empty() && (bestRoute.empty() || candidate.score() < bestScore))
				{
					bestRoute = candidate;
					bestScore = candidate.score();
				}
			}
			if (!bestRoute.empty())
			{
				g->journey.set(bestRoute);
			}
			else
			{
				// No restaurant reachable - they leave the tower
				const Route &r = game->findRoute(this, game->mainLobby);
				if (!r.empty())
				{
					g->state = Person::kReturning;
					g->goingTo = "Exit";
					g->journey.set(r);
				}
			}
		}
		// Go to sleep.
		else if (g->atHotel && g->state == Person::kHome && t >= g->sleepTime && t < g->wakeTime)
		{
			g->state = Person::kResting;
		}
		// Wake up.
		else if (g->atHotel && g->state == Person::kResting &&
		         t >= g->wakeTime && t < g->checkoutTime)
		{
			g->state = Person::kHome;
		}
		// Checkout is the hotel's revenue boundary. Charge exactly once even
		// if the guest is already out at dinner or a route is temporarily
		// unavailable; a negative checkoutTime is the durable latch.
		else if (g->checkoutTime >= 0 && t >= g->checkoutTime)
		{
			if (prototype->yoot) {
				// YootTower E7 (T2GuestroomTable::CalcInMoney 0x10037a00):
				// the room's inMoney at its price selection.
				game->yootBook(Yoot::hotelRoomIncome(*prototype->yoot, game->yootPriceSelection(this)),
				               "hotel_income", "Hotel room checkout");
			} else {
				const int stay = openSkyQuarterlyRent(prototype->id, 0);
				game->transferFunds(stay, "hotel_income", "Hotel room checkout");
			}
			g->checkoutTime = -1;
			const Route &r = game->findRoute(this, game->mainLobby);
			if (g->at == this)
				Item::removePerson(g);
			g->atHotel = false;
			g->state = Person::kReturning;
			g->from = prototype->name;
			g->goingTo = "Exit";
			if (!r.empty())
				g->journey.set(r);
			releaseParking(g->parkingUsed);
			g->parkingUsed = NULL;
			roomState = kDirty;
			dirtySince = t;
			spriteNeedsUpdate = true;
		}
		// Retire checked-out guests once their exit journey reaches the lobby.
		// Keeping them in the room set would make the hotel population grow
		// without bound and would retain stale evaluation records.
		if (g->checkoutTime < 0 && !g->atHotel && !g->at)
		{
			it = guests.erase(it);
			delete g;
		}
		else
		{
			++it;
		}
	}

	// Housekeepers are service staff: they must use service elevators and
	// stairs-only paths rather than the passenger lobby route.
	if (roomState == kDirty && housekeeper == NULL && !lobbyRoute.empty())
	{
		Route serviceRoute = game->findRoute(game->mainLobby, this, true);
		if (!serviceRoute.empty())
		{
			Housekeeper *h = new Housekeeper(this);
			housekeeper = h;
			h->journey.set(serviceRoute);
		}
	}

	// Housekeeper finished cleaning?
	if (housekeeper && housekeeper->at == this && housekeeper->cleaning)
	{
		if (game->time.absolute >= housekeeper->cleaningUntil)
		{
			roomState = kClean;
			spriteNeedsUpdate = true;
			Route r = game->findRoute(this, game->mainLobby, true);
			if (!r.empty())
			{
				Item::removePerson(housekeeper);
				housekeeper->state = Person::kReturning;
				housekeeper->from = prototype->name;
				housekeeper->goingTo = "Exit";
				housekeeper->journey.set(r);
			}
			// Keep the pointer until the journey completes; it will be
			// deleted by clearAll() on the next opening or when the item is
			// destroyed.
		}
	}

	if (spriteNeedsUpdate || roomFrameIndex != hotelRoomFrame(roomState, game->time.hour))
		updateSprite();
}

void Hotel::addPerson(Person *p)
{
	Item::addPerson(p);

	Guest *g = dynamic_cast<Guest *>(p);
	if (g)
	{
		g->atHotel = true;
		g->state = Person::kHome;
		g->eval = 50;
		roomState = kOccupied;
		spriteNeedsUpdate = true;
		return;
	}

	Housekeeper *h = dynamic_cast<Housekeeper *>(p);
	if (h)
	{
		h->state = Person::kWorking;
		h->cleaning = true;
		h->cleaningUntil = game->time.absolute + Time::hourToAbsolute(0.5);
		spriteNeedsUpdate = true;
		return;
	}
}

void Hotel::removePerson(Person *p)
{
	Item::removePerson(p);

	Guest *g = dynamic_cast<Guest *>(p);
	if (g)
	{
		g->atHotel = false;
	}

	Housekeeper *h = dynamic_cast<Housekeeper *>(p);
	if (h)
	{
		h->cleaning = false;
	}

	spriteNeedsUpdate = true;
}

void Hotel::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("subVariant", subVariant);
	xml.PushAttribute("roomState", roomState);
	xml.PushAttribute("dirtySince", dirtySince);

	for (std::set<Guest *>::iterator it = guests.begin(); it != guests.end(); ++it)
	{
		Guest *g = *it;
		xml.OpenElement("guest");
		xml.PushAttribute("arrivalTime", g->arrivalTime);
		xml.PushAttribute("dinnerLeaveTime", g->dinnerLeaveTime);
		xml.PushAttribute("dinnerReturnTime", g->dinnerReturnTime);
		xml.PushAttribute("sleepTime", g->sleepTime);
		xml.PushAttribute("wakeTime", g->wakeTime);
		xml.PushAttribute("checkoutTime", g->checkoutTime);
		xml.PushAttribute("atHotel", g->atHotel);
		xml.PushAttribute("type", g->type);
		xml.PushAttribute("state", g->state);
		xml.PushAttribute("stress", g->stress);
		xml.PushAttribute("eval", g->eval);
		xml.PushAttribute("name", g->name.c_str());
		xml.PushAttribute("from", g->from.c_str());
		xml.PushAttribute("goingTo", g->goingTo.c_str());
		xml.CloseElement();
	}

	if (housekeeper)
	{
		xml.OpenElement("housekeeper");
		xml.PushAttribute("cleaningUntil", housekeeper->cleaningUntil);
		xml.PushAttribute("cleaning", housekeeper->cleaning);
		xml.PushAttribute("atHotel", (housekeeper->at == this));
		xml.PushAttribute("state", housekeeper->state);
		xml.PushAttribute("stress", housekeeper->stress);
		xml.PushAttribute("eval", housekeeper->eval);
		xml.PushAttribute("name", housekeeper->name.c_str());
		xml.PushAttribute("from", housekeeper->from.c_str());
		xml.PushAttribute("goingTo", housekeeper->goingTo.c_str());
		xml.CloseElement();
	}
}

void Hotel::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant", prototype->variant);
	subVariant = xml.IntAttribute("subVariant", rand() % 2);
	roomState = xml.IntAttribute("roomState", kClean);
	dirtySince = xml.DoubleAttribute("dirtySince", 0.0);
	clearAll();
	applyVariant();

	for (tinyxml2::XMLElement *e = xml.FirstChildElement("guest"); e; e = e->NextSiblingElement("guest"))
	{
		Guest *g = new Guest(this);
		g->arrivalTime = e->DoubleAttribute("arrivalTime");
		g->dinnerLeaveTime = e->DoubleAttribute("dinnerLeaveTime");
		g->dinnerReturnTime = e->DoubleAttribute("dinnerReturnTime");
		g->sleepTime = e->DoubleAttribute("sleepTime");
		g->wakeTime = e->DoubleAttribute("wakeTime");
		g->checkoutTime = e->DoubleAttribute("checkoutTime");
		g->atHotel = e->BoolAttribute("atHotel", false);
		g->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		g->state = (Person::State)e->IntAttribute("state", Person::kWandering);
		g->stress = e->DoubleAttribute("stress", 0.0);
		g->eval = e->DoubleAttribute("eval", 0.0);
		g->name = e->Attribute("name", "");
		g->from = e->Attribute("from", "");
		g->goingTo = e->Attribute("goingTo", "");
		guests.insert(g);
		if (g->atHotel)
		{
			Item::addPerson(g);
			g->atHotel = true;
			// NOTE(Phase 2.2): parkingUsed is not persisted across save/reload.
			// Parking::used IS persisted, so the tower-wide slot count stays
			// correct, but the per-guest back-pointer is lost. This means a
			// guest who checks out post-reload won't call freeSpace() on their
			// slot, leaving it claimed until the parking is rebuilt. Proper
			// fix: persist the parking item's position as a guest attribute
			// and re-link the pointer here without calling assignSpace().
		}
		else
		{
			arrivingGuests.push(g);
		}
	}

	if (tinyxml2::XMLElement *e = xml.FirstChildElement("housekeeper"))
	{
		Housekeeper *h = new Housekeeper(this);
		h->cleaningUntil = e->DoubleAttribute("cleaningUntil");
		h->cleaning = e->BoolAttribute("cleaning", false);
		h->state = (Person::State)e->IntAttribute("state", Person::kWandering);
		h->stress = e->DoubleAttribute("stress", 0.0);
		h->eval = e->DoubleAttribute("eval", 0.0);
		h->name = e->Attribute("name", "");
		h->from = e->Attribute("from", "");
		h->goingTo = e->Attribute("goingTo", "");
		housekeeper = h;
		if (e->BoolAttribute("atHotel", false))
		{
			Item::addPerson(h);
		}
	}

	updateSprite();
}

Path Hotel::getRandomBackgroundSoundPath()
{
	return "";
}

Hotel::Guest::Guest(Hotel *item)
	: Person(item->game), hotel(item)
{
	arrivalTime = 0;
	dinnerLeaveTime = 0;
	dinnerReturnTime = 0;
	sleepTime = 0;
	wakeTime = 0;
	checkoutTime = 0;
	atHotel = false;
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	from = "City";
	goingTo = item->prototype->name;
}

Hotel::Housekeeper::Housekeeper(Hotel *item)
	: Person(item->game, kHousekeeper)
{
	cleaningUntil = 0;
	cleaning = false;
	from = "Hotel Staff";
	goingTo = item->prototype->name;
}
