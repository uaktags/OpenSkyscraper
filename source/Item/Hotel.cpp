/* Copyright © 2024 */
#include "../Game.h"
#include "../Math/Rand.h"
#include "../Time.h"
#include "Factory.h"
#include "Hotel.h"
#include <cmath>
#include <string>

using namespace OT;
using namespace OT::Item;

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
	p->price          = 50000;
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
	p->price          = 100000;
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
	p->price          = 200000;
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
	int frameWidth = 48;
	switch (variant)
	{
		case kSingle: sz = int2(4, 1); tex = "simtower/single"; frameWidth = 32; break;
		case kDouble: sz = int2(6, 1); tex = "simtower/double"; frameWidth = 48; break;
		case kSuite:  sz = int2(7, 1); tex = "simtower/suite";  frameWidth = 56; break;
	}
	size = sz;
	sprite.setTexture(App->bitmaps[tex]);
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	(void)frameWidth;
}

void Hotel::init()
{
	Item::init();

	variant = prototype->variant;
	roomState = kClean;
	dirtySince = 0;
	housekeeper = NULL;

	applyVariant();
	addSprite(&sprite);
	spriteNeedsUpdate = false;

	updateSprite();
}

void Hotel::updateSprite()
{
	spriteNeedsUpdate = false;
	int frameWidth = 48;
	switch (variant)
	{
		case kSingle: frameWidth = 32; break;
		case kDouble: frameWidth = 48; break;
		case kSuite:  frameWidth = 56; break;
	}
	int row = (roomState == kDirty) ? 1 : 0;
	sprite.setTextureRect(sf::IntRect({0, row * 24}, {frameWidth, 24}));
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
		delete *it;
	guests.clear();
	while (!arrivingGuests.empty())
		arrivingGuests.pop();
	if (housekeeper)
	{
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
		}
		else
			break;
	}

	// Process guest schedules.
	double t = game->time.absolute;
	for (std::set<Guest *>::iterator it = guests.begin(); it != guests.end(); ++it)
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
		}
		// Go to sleep.
		else if (g->atHotel && g->state == Person::kHome && t >= g->sleepTime && t < g->wakeTime)
		{
			g->state = Person::kResting;
		}
		// Wake up.
		else if (g->atHotel && g->state == Person::kResting && t >= g->wakeTime && t < g->checkoutTime)
		{
			g->state = Person::kHome;
		}
		// Checkout — room becomes dirty.
		else if (g->atHotel && t >= g->checkoutTime)
		{
			const Route &r = game->findRoute(this, game->mainLobby);
			if (!r.empty())
			{
				Item::removePerson(g);
				g->atHotel = false;
				g->state = Person::kReturning;
				g->from = prototype->name;
				g->goingTo = "Exit";
				g->journey.set(r);
			}
			roomState = kDirty;
			dirtySince = t;
			spriteNeedsUpdate = true;
		}
	}

	// Dispatch a housekeeper when the room is dirty.
	if (roomState == kDirty && housekeeper == NULL && !lobbyRoute.empty())
	{
		Housekeeper *h = new Housekeeper(this);
		housekeeper = h;
		h->journey.set(lobbyRoute);
	}

	// Housekeeper finished cleaning?
	if (housekeeper && housekeeper->at == this && housekeeper->cleaning)
	{
		if (game->time.absolute >= housekeeper->cleaningUntil)
		{
			roomState = kClean;
			spriteNeedsUpdate = true;
			const Route &r = game->findRoute(this, game->mainLobby);
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

	if (spriteNeedsUpdate)
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
		g->addStress(-5);
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
	: Person(item->game)
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
