/* Copyright © 2013 Fabian Schuiki */
#include "Office.h"
#include "../Game.h"
#include "../PeopleSystem.h"
#include "../YootRules.h"
#include "../Math/Rand.h"
#include "../Time.h"
#include "MedicalCenter.h"
#include "Parking.h"
#include <algorithm>

extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);

using OT::Path;
using OT::Route;
using OT::Time;
using OT::Item::Office;
using OT::Item::Parking;
using OT::Item::MedicalCenter;

Office::Worker::Worker(Office *item, Person::Type type, int k)
	: Person(item->game, type)
{
	home = item;
	member = (int8_t)k;
	if (k >= 0 && k < 6)
	{
		bankCount = item->tenant.bank[k].count;
		bankTotal = item->tenant.bank[k].total;
	}
}

Office::~Office()
{
	// Remove worker memberships before deleting records; Item::removePerson
	// asserts ownership and keeps Game::people free of dangling pointers.
	for (Workers::iterator c = workers.begin(); c != workers.end(); c++)
	{
		releaseCar(*c);
		if ((*c)->at == this)
			Item::removePerson(*c);
		delete *c;
	}
}

void Office::init()
{
	Item::init();

	variant = 0;
	occupied = false;
	lit = false;
	tenant.rentClass = 1;

	sprite.setTexture(App->bitmaps.editionTexture("office", "simtower/office"));
	// Skin::Yoot: the Office.t2p sheet, its windows keyed to the backdrop.
	yootView = game->skin == Skin::Yoot
		? yootPluginView(game->app, prototype->yoot ? prototype->yoot->plugin : "Office.t2p", kYootWindowKey) : nullptr;
	if (yootView) sprite.setTexture(yootView->texture);
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(position.y * -36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;

	// Create the workers (members 0 and 1 salesmen, 2/3 men, 4/5 women).
	Person::Type types[] = {
		Person::kSalesman,
		Person::kSalesman,
		Person::kMan,
		Person::kMan,
		Person::kWoman1,
		Person::kWoman2};
	for (int i = 0; i < 6; i++)
	{
		crew[i] = new Worker(this, types[i], i);
		workers.insert(crew[i]);
	}

	updateSprite();
}

void Office::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("occupied", occupied);
	xml.PushAttribute("lit", lit);
	xml.PushAttribute("population", population);
}

void Office::syncWorkersFromRecord()
{
	for (int k = 0; k < 6; ++k)
	{
		if (!crew[k]) continue;
		crew[k]->bankCount = tenant.bank[k].count;
		crew[k]->bankTotal = tenant.bank[k].total;
	}
}

void Office::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant");
	occupied = xml.BoolAttribute("occupied");
	lit = xml.BoolAttribute("lit");
	// Old saves: `armed` was never stored; an occupied office is a tenant.
	if (!xml.FindAttribute("armed") && occupied) tenant.armed = true;
	population = xml.IntAttribute("population", occupied ? 6 : 0);
	syncWorkersFromRecord();
	updateSprite();
}

void Office::updateSprite()
{
	spriteNeedsUpdate = false;
	int index_x = (lit ? 0 : 1);
	int index_y = (occupied ? variant : 6);
	sprite.setTextureRect(sf::IntRect({index_x * 72, index_y * 24}, {72, 24}));
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(position.y * -36)});
}

void Office::releaseCar(Worker *w)
{
	if (!w || !w->parkingUsed) return;
	w->parkingUsed->freeSpace(w);
	w->parkingUsed->spriteNeedsUpdate = true;
	w->parkingUsed = NULL;
	if (prototype->yoot) game->eventSystem.specials.driveOut(w);
}

/// Office member 2 drives when (floor + tenant slot) & 3 == 1 at 3+ stars (T40).
bool Office::drives(Worker *w) const
{
	if (!w || w->member != 2 || game->rating < 2) return false;
	const int x = position.y + game->tenantSlotIndex(this);
	return (((x % 4) + 4) % 4) == 1;
}

void Office::arrive(Worker *w)
{
	w->arrived = true;
	w->state = Person::kWorking;
	Route route = lobbyRoute;
	if (drives(w) && !w->parkingUsed)
	{
		Parking *park = Parking::claim(game, this, w, Parking::kOfficeCar);
		if (park)
		{
			w->parkingUsed = park;
			park->spriteNeedsUpdate = true;
			if (prototype->yoot) game->eventSystem.specials.driveIn(w, this); // YootTower lots (I6)
			const Route fromCar = game->findRoute(park, this);
			if (!fromCar.empty()) route = fromCar;
		}
	}
	w->journey.set(route);
}

void Office::sendHome(Worker *w)
{
	Route r;
	if (w->parkingUsed) r = game->findRoute(this, w->parkingUsed);
	if (r.empty()) r = game->findRoute(this, game->mainLobby);
	if (r.empty()) return; // retry at the next visit
	w->state = Person::kReturning;
	// The car stays in its bay while the driver is still inside the tower;
	// advance() frees it when the driver reaches the parking item.
	if (w->parkingUsed && !game->routeExists(this, w->parkingUsed))
		releaseCar(w); // no way to the car: the worker leaves via the lobby on foot
	w->goingTo = w->parkingUsed ? "Parking Space" : "Lobby to go home";
	w->journey.set(r);
}

void Office::newDay()
{
	for (Workers::iterator iw = workers.begin(); iw != workers.end(); ++iw)
	{
		Worker *w = *iw;
		w->arrived = w->lunchSent = w->salesSent = w->salesBack = w->sickSent = false;
		w->lunchReturnTime = 0;
	}
}

/// One 16-frame visit of the office day cycle (T32/T33).
void Office::visit()
{
	const bool weekday = !game->time.isHoliday();
	const double h = game->time.hour;

	// Morning: one 1/8 roll per tenant until all six have arrived.
	if (weekday && h >= 7.0 && h < 12.0 && !lobbyRoute.empty())
	{
		for (int k = 0; k < 6; ++k)
		{
			Worker *w = crew[k];
			if (w->arrived) continue;
			if (rand() % 8 == 0) arrive(w);
			break;
		}
	}

	for (int k = 0; k < 6; ++k)
	{
		Worker *w = crew[k];
		const bool atDesk = w->at == this && w->state == Person::kWorking;

		// Lunch return.
		if (w->lunchReturnTime > 0 && game->time.absolute >= w->lunchReturnTime)
		{
			if (w->at && w->at != this)
			{
				const Route r = game->findRoute(w->at, this);
				if (!r.empty())
				{
					w->state = Person::kWorking;
					w->goingTo.clear();
					w->lunchReturnTime = 0;
					w->journey.set(r);
				}
			}
			else
				w->lunchReturnTime = 0;
			continue;
		}

		// Evening: everybody at a desk goes home.
		if (atDesk && (h >= 17.0 || h < 5.0))
		{
			sendHome(w);
			continue;
		}

		if (atDesk && weekday && h >= 7.0 && h < 17.0)
		{
			if (k == 0 && !w->salesSent)
			{
				// Sales trip: 1/12 per visit until noon, certain afterwards.
				const bool go = h < 12.0 ? (rand() % 12 == 0) : true;
				if (go)
				{
					const Route r = game->findRoute(this, game->mainLobby);
					if (!r.empty())
					{
						w->salesSent = true;
						w->state = Person::kWandering;
						w->goingTo = "Lobby";
						w->journey.set(r);
						continue;
					}
				}
			}
			else if (k >= 1 && !w->lunchSent && h >= 12.0)
			{
				const bool go = h < 13.0 ? (rand() % 12 == 0) : true;
				Item *venue = go ? game->pickVenue(position.y, 2) : nullptr;
				if (venue)
				{
					// One pick per worker per day: a closed/full venue bounces
					// the visitor home on arrival, never a re-roll (T28).
					w->lunchSent = true;
					const Route r = game->findRoute(this, venue);
					if (!r.empty())
					{
						w->state = Person::kLunch;
						w->goingTo = "Lobby to eat";
						w->lunchReturnTime = game->time.absolute + 10 * Time::kBaseSpeed;
						w->journey.set(r);
						continue;
					}
				}
			}

			// Sick roll (3+ stars): members 2..5 may be sent to a medical center.
			if (k >= 2 && game->rating >= 2 && rand() % 10 == 0)
			{
				Item *center = game->pickMedicalCenter(position.y);
				if (!center)
					game->medicalAdequate = false;
				else if (MedicalCenter *mc = dynamic_cast<MedicalCenter *>(center))
				{
					mc->admitPatient(w);
				}
			}
		}

		// Worker 0 returns from the sales call: 1/12 per visit from 13:00 to 17:00.
		if (k == 0 && w->salesSent && !w->salesBack && !w->at && !w->journey.item &&
		    weekday && h >= 13.0 && h < 17.0 && !lobbyRoute.empty() && rand() % 12 == 0)
		{
			w->salesBack = true;
			arrive(w);
		}
	}
}

void Office::advance(double dt)
{
	// Tenant idles are frozen during the YootTower blackout (EventSystem::tenantIdleFrozen).
	if (prototype->yoot && game->eventSystem.tenantIdleFrozen()) return;

	const int visits = visitsThisTick(game);
	const bool weekday = !game->time.isHoliday();
	const double h = game->time.hour;
	// YootTower (Office.t2p FUN_1000c0c0, status 10/11): a vacant office
	// tries CollectEmployee in the first 8 minutes of each hour from 08:00
	// to 16:59, on any day.
	const bool yootHireWindow = prototype->yoot && game->time.hour >= 8 && game->time.hour < 17 &&
		std::fmod(game->time.hour, 1.0) * 60.0 < 8.0;

	// YootTower: the workers come out of the pool (Office.t2p TMDF 1,
	// CollectEmployee 0x1005f5a0); no workers, no tenant. SetKind pays
	// inMoney once on occupancy (oracle: 9 offices paid 9 x $3,000).
	if (!occupied && yootHireWindow && game->eventSystem.people.enabled() &&
	    !game->eventSystem.people.pool.empty() &&
	    yootKeepsTenants(false) && game->eventSystem.people.belongOf(this).empty() &&
	    game->eventSystem.people.hireBelong(this))
	{
		moveIn();
	}
	// Movers (T9): an armed vacant unit is re-let on weekday mornings,
	// 1/12 per visit. Unarmed units are never re-let.
	else if (!occupied && !prototype->yoot && tenant.armed && weekday && h >= 7.0 && h < 12.0)
	{
		for (int v = 0; v < visits; ++v)
			if (rand() % 12 == 0) { moveIn(); break; }
	}

	// YootTower: a let office with nobody hired yet (a .bld office of
	// status 10/11, vacant in Office.t2p, with or without a kind) hires at
	// the next window and pays SetKind's inMoney once, like a vacant one.
	if (occupied && population == 0 && yootHireWindow && game->eventSystem.people.enabled() &&
	    !game->eventSystem.people.pool.empty() &&
	    yootKeepsTenants(false) && game->eventSystem.people.belongOf(this).empty() &&
	    game->eventSystem.people.hireBelong(this))
	{
		population = workers.size();
		game->populationNeedsUpdate = true;
		game->transferFunds(prototype->yoot->inMoneyFor(game->yootPriceSelection(this)),
			"deposit_income", "Income from office rentals just came in.");
	}

	if (occupied)
	{
		// A driver who has walked home to the parking item leaves the tower:
		// only now does the bay free up (never on in-tower errands).
		for (Worker * w : workers)
			if (w->parkingUsed && w->state == Person::kReturning &&
			    w->at == w->parkingUsed && w->journey.atFinalNode())
				releaseCar(w);
		if (game->time.checkHour(5)) newDay();
		for (int v = 0; v < visits; ++v) visit();
	}

	// Turn on the office lights.
	bool shouldBeLit = (weekday && h >= 7.0 && h < 17.0) || !people.empty();
	if (lit != shouldBeLit)
	{
		lit = shouldBeLit;
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

void Office::moveIn()
{
	occupied = true;
	variant = rand() % 6;
	lit = true;
	spriteNeedsUpdate = true;
	tenant.tenure = 0;
	tenant.category = TenantRules::kUnjudged;
	tenant.redraw = true;
	refreshEvaluation();
	resetMemberBanks();
	newDay();
	population = 6;
	game->addPopulation(TenantRules::kCountOffice, 6);
	if (prototype->yoot)
	{
		// YootTower: SetKind pays inMoney once on occupancy (Office.t2p
		// FUN_1000c0c0; oracle: 9 offices paid 9 x $3,000).
		game->transferFunds(prototype->yoot->inMoneyFor(game->yootPriceSelection(this)),
			"deposit_income", "Income from office rentals just came in.");
	}
	else
		game->transferFunds(openSkyQuarterlyRent("office", tenant.rentClass), "rent_income", "Office rent");
}

void Office::vacate()
{
	if (!occupied) return;
	for (Workers::iterator iw = workers.begin(); iw != workers.end(); ++iw)
	{
		Worker *w = *iw;
		releaseCar(w);
		w->journey.cancel();
		w->state = Person::kIdle;
	}
	occupied = false;
	spriteNeedsUpdate = true;
	population = 0;
	game->addPopulation(TenantRules::kCountOffice, -6);
	if (prototype->yoot) game->eventSystem.people.releaseBelong(this);
	updateSprite();
}

void Office::resetMemberBanks()
{
	resetTenantBanks();
	for (Workers::iterator iw = workers.begin(); iw != workers.end(); ++iw)
	{
		(*iw)->bankCount = 0;
		(*iw)->bankTotal = 0;
	}
}

void Office::addPerson(Person *p)
{
	Item::addPerson(p);
	p->state = Person::kWorking;
}

void Office::prepareLunchQa(double lunchHour)
{
	(void)lunchHour;
	if (!occupied)
	{
		game->addPopulation(TenantRules::kCountOffice, 6);
		population = 6;
	}
	occupied = true;
	tenant.armed = true;
	lit = true;
	spriteNeedsUpdate = true;
	newDay();
	for (Workers::iterator iw = workers.begin(); iw != workers.end(); iw++)
	{
		Worker *w = *iw;
		w->stress = 0;
		w->arrived = true;
		if (!w->at)
			Item::addPerson(w);
		w->state = Person::kWorking;
	}
	updateSprite();
}

Path Office::getRandomBackgroundSoundPath()
{
	if (!lit || !occupied)
		return "";
	return "simtower/office";
}

void Office::evacuateForDisaster()
{
	// occupied is tenancy, not arrival state; preserve it and standing population.
	for (Worker *w : workers) {
		releaseCar(w);
		w->state = Person::kIdle;
		w->lunchReturnTime = 0;
	}
	lit = false;
	spriteNeedsUpdate = true;
	updateSprite();
}
