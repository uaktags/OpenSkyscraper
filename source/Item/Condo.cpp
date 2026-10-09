#include "../Game.h"
#include "../Math/Rand.h"
#include "Condo.h"

using namespace OT;
using namespace OT::Item;
// Shared rent-table writer implemented by Game.cpp (resource 0x7f0b/0x3e9).
#include <algorithm>
extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);

Condo::CondoOccupant::CondoOccupant(Condo *item, Person::Type type, double depart, double ret, int k)
	: Person(item->game, type),
	  departureTime(depart),
	  returnTime(ret),
	  atHome(true)
{
	home = item;
	member = (int8_t)k;
	if (k >= 0 && k < 3)
	{
		bankCount = item->tenant.bank[k].count;
		bankTotal = item->tenant.bank[k].total;
	}
}

Condo::~Condo()
{
	removeOccupants();
}

void Condo::init()
{
	Item::init();

	variant = rand() % 3;
	occupied = false;
	lighting = NIGHT;
	rent = 150000;
	rentDeposit = rent;
	tenant.rentClass = 1;

	sprite.setTexture(App->bitmaps.editionTexture("condo", "simtower/condo"));
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});
	addSprite(&sprite);
	updateSprite();
}

void Condo::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("rent", rent);
	xml.PushAttribute("rentDeposit", rentDeposit);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("lighting", lighting);
	xml.PushAttribute("occupied", occupied);
	xml.PushAttribute("population", population);
}

void Condo::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	rent = xml.IntAttribute("rent", 150000);
	rentDeposit = xml.IntAttribute("rentDeposit", xml.BoolAttribute("occupied") ? 150000 : rent);
	variant = xml.IntAttribute("variant");
	lighting = (LightingConditions)xml.IntAttribute("lighting");
	occupied = xml.BoolAttribute("occupied");
	if (!xml.FindAttribute("armed") && occupied) tenant.armed = true;
	removeOccupants();
	if (occupied)
	{
		createOccupants();
		population = 3;
	}
	else
	{
		population = 0;
	}
	updateSprite();
}

void Condo::updateSprite()
{
	spriteNeedsUpdate = false;
	int index = 2;
	if (occupied)
	{
		switch (lighting)
		{
		case NIGHT:
			index = 2;
			break;
		case LIT:
			index = 1;
			break;
		case DAYTIME:
			index = 0;
			break;
		}
	}
	else
	{
		switch (lighting)
		{
		case NIGHT:
		case LIT:
			index = 4;
			break;
		case DAYTIME:
			index = 3;
			break;
		}
	}
	sprite.setTextureRect(sf::IntRect({index * 128, variant * 24}, {128, 24}));
}

void Condo::advance(double dt)
{
	(void)dt;
	const int visits = visitsThisTick(game);
	const double h = game->time.hour;

	// Movers (T9): armed vacant condos are re-let every morning, 1/12 per
	// visit. Unarmed units are never re-let.
	if (!occupied && tenant.armed && h >= 7.0 && h < 12.0)
	{
		for (int v = 0; v < visits; ++v)
			if (rand() % 12 == 0) { moveIn(); break; }
	}

	if (occupied)
	{
		if (game->time.checkHour(5))
			for (CondoOccupant *c : occupants) c->leftToday = false;
		for (int v = 0; v < visits; ++v) visit();
	}

	if (updateLighting(h))
	{
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

/// One 16-frame visit of the family day cycle (T36).
void Condo::visit()
{
	const double h = game->time.hour;
	const bool weekday = !game->time.isHoliday();
	const int slot = game->tenantSlotIndex(this);
	const int phase = h >= 17.0 ? 2 : h >= 12.0 ? 1 : 0;
	bool inboundRolled = false;

	for (int k = 0; k < 3; ++k)
	{
		CondoOccupant *c = family[k];
		if (!c) continue;
		const bool inside = c->at == this;
		const bool away = !c->at && !c->journey.item && c->out;
		const bool nightHours = h >= 21.0 || h < 6.0;

		// Bedtime and waking.
		if (inside)
		{
			if (!c->asleep && nightHours)
			{
				bool sleep;
				if (k == 2) sleep = true;
				else if (h >= 21.0 || h < 1.0) sleep = rand() % 12 == 0;
				else sleep = true;
				if (sleep) c->asleep = true;
			}
			else if (c->asleep && !nightHours)
			{
				if (h >= 7.0 || rand() % 12 == 0) c->asleep = false;
			}
			if (c->asleep) continue;
		}

		// Away: coming home.
		if (away)
		{
			bool eligible = false;
			bool certain = false;
			if (h >= 21.0 || h < 5.0) { eligible = true; certain = true; }
			else if (!weekday) eligible = true;
			else if (k == 1) eligible = true;
			else if (k == 2) eligible = h >= 13.0 && h < 17.0;
			else eligible = h >= 17.0 && h < 21.0;
			if (eligible && !lobbyRoute.empty() && !inboundRolled && (certain || rand() % 8 == 0))
			{
				inboundRolled = true;
				c->out = false;
				c->state = Person::kReturning;
				c->goingTo = "Condo";
				c->journey.set(lobbyRoute);
			}
			continue;
		}
		if (!inside) continue;

		// Weekday: the awake residents (adult and kid) leave 07:00-12:00.
		if (weekday && k != 1 && h >= 7.0 && h < 12.0 && !c->leftToday && rand() % 12 == 0)
		{
			const Route r = game->findRoute(this, game->mainLobby);
			if (!r.empty())
			{
				c->leftToday = true;
				c->out = true;
				c->state = Person::kCommuting;
				c->goingTo = "Lobby to leave";
				c->journey.set(r);
			}
			continue;
		}

		// Homebody shopping, 10:00-21:00, once per phase.
		if (k == 1 && h >= 10.0 && h < 21.0 && c->shopPhase != phase && rand() % 12 == 0)
		{
			c->shopPhase = phase;
			Item *shop = game->pickVenue(position.y, 0);
			Route r = shop ? game->findRoute(this, shop) : Route();
			if (r.empty()) r = game->findRoute(this, game->mainLobby);
			if (!r.empty())
			{
				c->out = true;
				c->state = Person::kShopping;
				c->goingTo = "Shopping";
				c->journey.set(r);
			}
			continue;
		}

		// Weekend dining: every fourth condo slot, two dispatches per phase.
		if (!weekday && slot % 4 == 0 && h >= 10.0 && h < 21.0)
		{
			if (diningPhase != phase) { diningPhase = phase; diningSent = 0; }
			if (diningSent < 2)
			{
				const bool evening = h >= 17.0;
				if (rand() % (evening ? 6 : 12) == 0)
				{
					Item *venue = game->pickVenue(position.y, evening ? 1 : 2);
					const Route r = venue ? game->findRoute(this, venue) : Route();
					if (!r.empty())
					{
						++diningSent;
						c->out = true;
						c->state = Person::kLunch;
						c->goingTo = evening ? "Restaurant" : "Fast Food";
						c->journey.set(r);
					}
				}
			}
		}
	}
}

bool Condo::updateLighting(double time)
{
	LightingConditions newLighting = lighting;
	if ((time < 7.0) || (time > 22.0))
	{
		// Lights stay on while somebody is awake at home.
		bool awake = false;
		for (CondoOccupant *c : occupants)
			if (c->at == this && !c->asleep) awake = true;
		newLighting = awake ? LIT : NIGHT;
	}
	else if (time < 19.0)
	{
		newLighting = DAYTIME;
	}
	else
	{
		newLighting = LIT;
	}

	bool retval = (newLighting != lighting);
	lighting = newLighting;
	return retval;
}

void Condo::addPerson(Person *p)
{
	Item::addPerson(p);
	p->state = Person::kHome;
	if (CondoOccupant *c = dynamic_cast<CondoOccupant *>(p))
	{
		c->out = false;
		c->goingTo.clear();
	}
}

void Condo::createOccupants()
{
	const Person::Type types[3] = {Person::kMan, Person::kWomanWithChild1, Person::kChild};
	for (int k = 0; k < 3; ++k)
	{
		CondoOccupant *c = new CondoOccupant(this, types[k], 0, 0, k);
		family[k] = c;
		occupants.insert(c);
		Item::addPerson(c);
		c->state = Person::kHome;
	}
	population = 3;
}

void Condo::removeOccupants()
{
	// Erase before deleting so a subsequent re-let cannot retain dangling
	// occupant pointers (and Item::removePerson remains assertion-safe).
	while (!occupants.empty())
	{
		std::set<CondoOccupant *>::iterator it = occupants.begin();
		CondoOccupant *occupant = *it;
		occupants.erase(it);
		if (occupant->at == this)
			Item::removePerson(occupant);
		delete occupant;
	}
	family[0] = family[1] = family[2] = nullptr;
}

void Condo::removePerson(Person *p)
{
	assert(p);
	Item::removePerson(p);
	// occupants owns the family, even when they leave the room or evacuate.
	// Only removeOccupants deletes these records and changes tenancy.
}

void Condo::moveIn()
{
	occupied = true;
	variant = rand() % 3;
	spriteNeedsUpdate = true;
	tenant.tenure = 0;
	tenant.category = TenantRules::kUnjudged;
	tenant.redraw = true;
	refreshEvaluation();
	resetTenantBanks();
	rentDeposit = openSkyQuarterlyRent("condo", tenant.rentClass);
	game->transferFunds(rentDeposit, "condo_sale", "Income from Condo sale");
	removeOccupants();
	createOccupants();
	game->addPopulation(TenantRules::kCountCondo, 3);
}

void Condo::vacate()
{
	if (!occupied) return;
	occupied = false;
	removeOccupants();
	spriteNeedsUpdate = true;
	population = 0;
	game->addPopulation(TenantRules::kCountCondo, -3);
	const int buyback = openSkyQuarterlyRent("condo", tenant.rentClass);
	game->transferFunds(-buyback, "condo_buyback", "Repurchased vacated Condo");
	updateSprite();
}

void Condo::resetMemberBanks()
{
	resetTenantBanks();
	for (CondoOccupant *c : occupants)
	{
		c->bankCount = 0;
		c->bankTotal = 0;
	}
}

void Condo::evacuateForDisaster()
{
 for (auto *p : occupants) {
  // Journeys were cancelled: residents go back inside their unit.
  if (!p->at) Item::addPerson(p);
  p->out = false; p->state = Person::kIdle; p->atHome = false;
 }
 lighting = DAYTIME;
 spriteNeedsUpdate = true;
 updateSprite();
}
