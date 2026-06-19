/* Copyright © 2013 Fabian Schuiki */
#include "Office.h"
#include "../Game.h"
#include "../Math/Rand.h"
#include "../Time.h"
#include "Parking.h"

using OT::Path;
using OT::Route;
using OT::Time;
using OT::Item::Office;
using OT::Item::Parking;
using OT::Math::randd;

// Find a reachable Parking with a free slot and assign it to the worker.
// Mirrors the FastFood/Restaurant pattern of iterating game->itemsByType
// and filtering by findRoute. Returns the claimed Parking or NULL.
static Parking * claimReachableParking(OT::Game * game, OT::Item::Item * origin)
{
	const OT::Game::ItemSet &parkings = game->itemsByType["parking"];
	for (OT::Game::ItemSet::const_iterator it = parkings.begin(); it != parkings.end(); ++it)
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

Office::~Office()
{
	// Get rid of the workers.
	for (Workers::iterator c = workers.begin(); c != workers.end(); c++)
	{
		if ((*c)->parkingUsed)
		{
			(*c)->parkingUsed->freeSpace();
			(*c)->parkingUsed->spriteNeedsUpdate = true;
			(*c)->parkingUsed = NULL;
		}
		delete *c;
	}
}

void Office::init()
{
	Item::init();

	variant = 0;
	occupied = false;
	lit = false;
	rent = 10000;
	rentDeposit = rent;

	sprite.setTexture(App->bitmaps["simtower/office"]);
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(position.y * -36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;

	// Create the workers.
	Person::Type types[] = {
		Person::kSalesman,
		Person::kSalesman,
		Person::kMan,
		Person::kMan,
		Person::kWoman1,
		Person::kWoman2};
	for (int i = 0; i < 6; i++)
	{
		workers.insert(new Worker(this, types[i]));
	}
	rescheduleWorkers();

	// defaultCeiling();

	updateSprite();
}

void Office::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("rent", rent);
	xml.PushAttribute("rentDeposit", rentDeposit);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("occupied", occupied);
	xml.PushAttribute("lit", lit);
}

void Office::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	rent = xml.IntAttribute("rent");
	rentDeposit = xml.IntAttribute("rentDeposit");
	variant = xml.IntAttribute("variant");
	occupied = xml.BoolAttribute("occupied");
	lit = xml.BoolAttribute("lit");
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

void Office::advance(double dt)
{
	// Occupy the office if it is attractive enough.
	if (!occupied && game->time.day != 2 && game->time.hour >= 7 && game->time.hour < 17 && isAttractive())
	{
		// The item has a 10% chance of being occupied every 1/500th of a day. This means
		// that on average, an item is occupied after 1/250th of a day.
		if (game->time.checkTick(0.002) && (rand() % 10) == 0)
		{
			occupied = true;
			variant = rand() % 6;
			lit = true;
			spriteNeedsUpdate = true;
			rentDeposit = rent;
			population = workers.size();
			game->populationNeedsUpdate = true;
			game->transferFunds(rentDeposit, "deposit_income", "Occupied Office's rent deposit");
		}
	}

	// Monday 5:00 needs special treatment as this is where rent will be paid and tenants vacate
	// unattractive items.
	if (occupied && game->time.checkHour(5) && game->time.day == 0)
	{
		// Vacate unattractive offices.
		if (!isAttractive())
		{
			occupied = false;
			spriteNeedsUpdate = true;
			population = 0;
			game->populationNeedsUpdate = true;
			game->transferFunds(-rentDeposit, "deposit_refund", "Vacated Office's rent deposit paid back");
		}

		// Pay rent for the others.
		else
		{
			game->transferFunds(rent, "rent_income", "Income from Office rent");
		}
	}

	// Reset worker schedules at 5:00 if the office is occupied.
	if (occupied && game->time.checkHour(5) && game->time.day != 2)
	{
		rescheduleWorkers();
	}

	// Animate workers if occupied.
	if (occupied)
	{
		// Make workers arrive.
		while (!arrivalQueue.empty())
		{
			Worker *c = arrivalQueue.top();
			if (game->time.hour > c->arrivalTime && !lobbyRoute.empty())
			{
				arrivalQueue.pop();
				c->state = Person::kWorking;
				c->journey.set(lobbyRoute);
				// Drive in: claim a reachable parking slot for the car.
				if (!c->parkingUsed)
					c->parkingUsed = claimReachableParking(game, this);
			}
			else
				break;
		}

		// Make workers leave.
		while (!departureQueue.empty())
		{
			Worker *c = departureQueue.top();
			if (game->time.hour > c->departureTime)
			{
				departureQueue.pop();
				// Find a way home for the worker.
				const Route &r = game->findRoute(this, game->mainLobby);
				if (r.empty())
				{
					LOG(DEBUG, "%p has no route to leave", c);
				}
				else
				{
					LOG(DEBUG, "%p leaving office", c);
					c->state = Person::kReturning;
					c->journey.set(r);
					// Drive home: free the parking slot.
					releaseParking(c->parkingUsed);
					c->parkingUsed = NULL;
				}
			}
			else
				break;
		}

		while (!lunchQueue.empty())
		{
			Worker *w = lunchQueue.top();
			if (game->time.hour > w->lunchTime)
			{
				lunchQueue.pop();
				Route route;
				if (w->at == this && findLunchRoute(route))
				{
					LOG(DEBUG, "Worker %p leaving office for lunch", w);
					w->state = Person::kLunch;
					w->journey.set(route);
					w->lunchReturnTime = game->time.absolute + 10 * Time::kBaseSpeed;
					lunchReturnQueue.push(w);
				}
				else
				{
					// A missed lunch is a meaningful stress hit - enough that
					// workers with no reachable food will eventually flee for
					// the day once stress crosses the flee threshold.
					w->addStress(15.0);
					LOG(DEBUG, "Worker %p has no route to lunch", w);
				}
			}
			else
				break;
		}

		while (!lunchReturnQueue.empty())
		{
			Worker *w = lunchReturnQueue.top();
			if (game->time.check(w->lunchReturnTime))
			{
				lunchReturnQueue.pop();
				if (w->at && w->at != this)
				{
					const Route &r = game->findRoute(w->at, this);
				if (r.empty())
				{
					w->addStress(0.1);
					LOG(DEBUG, "Worker %p has no route back from lunch", w);
				}
					else
					{
						LOG(DEBUG, "Worker %p returning from lunch", w);
						w->state = Person::kWorking;
						w->journey.set(r);
					}
				}
			}
			else
				break;
		}

		// Make salesmen leave.
		while (!salesLeaveQueue.empty())
		{
			Worker *w = salesLeaveQueue.top();
			if (game->time.check(w->leaveForSalesTime) && !lobbyRoute.empty())
			{
				salesLeaveQueue.pop();
				const Route &r = game->findRoute(this, game->mainLobby);
				if (r.empty())
				{
					LOG(DEBUG, "%p has no route to leave", w);
				}
				else
				{
					LOG(DEBUG, "Salesman %p leaving office", w);
					w->state = Person::kWandering;
					w->journey.set(r);
					// Driving off for the sales trip - free the slot; it
					// will be re-claimed when the salesman returns.
					releaseParking(w->parkingUsed);
					w->parkingUsed = NULL;
				}
			}
			else
				break;
		}

		// Make salesmen return.
		while (!salesReturnQueue.empty())
		{
			Worker *w = salesReturnQueue.top();
			if (game->time.check(w->returnFromSalesTime) && !lobbyRoute.empty())
			{
				salesReturnQueue.pop();
				LOG(DEBUG, "Calling back salesman %p", w);
				w->state = Person::kWorking;
				w->journey.set(lobbyRoute);
				// Drive back in - try to claim a fresh slot.
				if (!w->parkingUsed)
					w->parkingUsed = claimReachableParking(game, this);
			}
			else
				break;
		}

		// Stress-flee: any worker currently at the office whose stress has
		// crossed the flee threshold heads home for the day early. Salesmen
		// are excluded - they're already in and out on sales trips.
		const double kStressFleeThreshold = 80.0;
		const Route &homeRoute = game->findRoute(this, game->mainLobby);
		for (Workers::iterator iw = workers.begin(); iw != workers.end(); ++iw)
		{
			Worker *w = *iw;
			if (w->at == this && w->state == Person::kWorking &&
			    w->stress > kStressFleeThreshold && !homeRoute.empty())
			{
				LOG(DEBUG, "Worker %p fleeing office (stress %.1f)", w, w->stress);
				w->state = Person::kReturning;
				w->goingTo = "Home (stressed)";
				w->journey.set(homeRoute);
				w->addStress(-10.0); // recovering on the way home
				releaseParking(w->parkingUsed);
				w->parkingUsed = NULL;
				game->timeWindow.showMessage("Worker leaves a stressful office");
			}
		}
	}

	// Turn on the office lights.
	bool shouldBeLit = (game->time.day != 2 && game->time.hour >= 7 && game->time.hour < 17) || !people.empty();
	if (lit != shouldBeLit)
	{
		lit = shouldBeLit;
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

/** Returns whether the item will be vacated at the next month. */
bool Office::isAttractive()
{
	// Need a route to the lobby, and a non-critical evaluation score.
	// JudgeSystem refreshes evaluation daily; the default of 50 keeps fresh
	// offices attractive before the first evaluation pass.
	return !lobbyRoute.empty() && evaluation >= 30.0;
}

void Office::addPerson(Person *p)
{
	Item::addPerson(p);

	p->state = Person::kWorking;

	// Reduce the person's stress a bit, just for the time being.
	p->stress *= 0.5;

	// If this was a salesman, set a sales leave and return time for him.
	if (p->type == Person::kSalesman)
	{
		Worker *w = (Worker *)p;
		if (w->leaveForSalesTime < 0)
		{
			w->leaveForSalesTime = game->time.absolute + randd(0.01, 0.02);
			LOG(DEBUG, "Salesman %p will leave at %f", w, w->leaveForSalesTime);
			salesLeaveQueue.push(w);
		}
		if (w->returnFromSalesTime < 0)
		{
			w->returnFromSalesTime = floor(game->time.absolute) + randd(Time::hourToAbsolute(13), Time::hourToAbsolute(15));
			LOG(DEBUG, "Salesman %p will return at %f", w, w->returnFromSalesTime);
			salesReturnQueue.push(w);
		}
	}
}

bool Office::findLunchRoute(Route &route)
{
	int bestScore = 0;
	const Game::ItemSet &foodItems = game->itemsByType["fastfood"];
	for (Game::ItemSet::const_iterator i = foodItems.begin(); i != foodItems.end(); i++)
	{
		Route candidate = game->findRoute(this, *i);
		if (!candidate.empty() && (route.empty() || candidate.score() < bestScore))
		{
			route = candidate;
			bestScore = candidate.score();
		}
	}
	return !route.empty();
}

void Office::prepareLunchQa(double lunchHour)
{
	occupied = true;
	lit = true;
	spriteNeedsUpdate = true;
	while (!arrivalQueue.empty())
		arrivalQueue.pop();
	while (!departureQueue.empty())
		departureQueue.pop();
	while (!lunchQueue.empty())
		lunchQueue.pop();
	while (!lunchReturnQueue.empty())
		lunchReturnQueue.pop();
	while (!salesLeaveQueue.empty())
		salesLeaveQueue.pop();
	while (!salesReturnQueue.empty())
		salesReturnQueue.pop();
	for (Workers::iterator iw = workers.begin(); iw != workers.end(); iw++)
	{
		Worker *w = *iw;
		w->stress = 0;
		w->leaveForSalesTime = -1;
		w->returnFromSalesTime = -1;
		w->lunchReturnTime = -1;
		if (!w->at)
			Item::addPerson(w);
		if (w->type != Person::kSalesman)
		{
			w->lunchTime = lunchHour;
			lunchQueue.push(w);
		}
	}
	population = workers.size();
	game->populationNeedsUpdate = true;
	updateSprite();
}

Path Office::getRandomBackgroundSoundPath()
{
	if (!lit || !occupied)
		return "";
	return "simtower/office";
}

/**
 * Shuffles the schedule of all office workers for the current day. Also sets
 * up all the queues appropriately.
 */
void Office::rescheduleWorkers()
{
	LOG(DEBUG, "Rescheduling workers.");

	// Clear the current queues.
	while (!arrivalQueue.empty())
		arrivalQueue.pop();
	while (!departureQueue.empty())
		departureQueue.pop();
	while (!lunchQueue.empty())
		lunchQueue.pop();
	while (!lunchReturnQueue.empty())
		lunchReturnQueue.pop();
	while (!salesLeaveQueue.empty())
		salesLeaveQueue.pop();
	while (!salesReturnQueue.empty())
		salesReturnQueue.pop();

	// For each worker, decide on an arrival, departure and lunch time.
	for (Workers::iterator iw = workers.begin(); iw != workers.end(); iw++)
	{
		Worker *w = *iw;
		w->arrivalTime = randd(7, 8);
		w->departureTime = randd(17, 19);
		w->lunchTime = randd(12, 12.2);
		w->lunchReturnTime = -1;
		w->stress = 0;
		w->leaveForSalesTime = -1;
		w->returnFromSalesTime = -1;

		arrivalQueue.push(w);
		departureQueue.push(w);
		if (w->type != Person::kSalesman)
			lunchQueue.push(w);
	}
}
