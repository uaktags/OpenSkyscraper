#include <algorithm>
#include <cassert>
#include <cmath>
#include "../../Game.h"
#include "../../YootCatalog.h"
#include "Car.h"
#include "Queue.h"
#include "../Lobby.h"

using namespace OT;
using namespace OT::Item::Elevator;

const static double kDoorPeriod    = 0.1;  //seconds it takes the door to open/close
const static double kWaitTime      = 0.15; //seconds the elevator waits before closing doors
const static double kMountPeriod   = 0.05; //seconds it takes for one person to get on the elevator
const static double kUnmountPeriod = 0.05; //ditto, but off the elevator


void Car::init()
{
	const std::string carKey = elevator->carBitmap;
	const char *edition = carKey.find("express") != std::string::npos ? "elevator_express" :
	                      carKey.find("service") != std::string::npos ? "elevator_service" :
	                                                                    "elevator_standard";
	sprite.setTexture(app->bitmaps.editionTexture(edition, elevator->carBitmap.c_str()));
	sprite.setOrigin({0.f, 30.f});
	if (elevator->yootCars) {
		// Skin::Yoot: the plugin's 32 px car boxes, 2 px above the floor line.
		sprite.setTexture(elevator->yootCars->texture);
		sprite.setOrigin({0.f, elevator->yootCars->band(0).size.y + 2.f});
	}
	updateSprite();

	arrivingSound.setBuffer(app->sounds["simtower/elevator/arriving"]);
	departingSound.setBuffer(app->sounds["simtower/elevator/departing"]);
	arrivingPlayed = false;
	departingPlayed = false;

	direction = Elevator::kNone;
	startAltitude = altitude;
	destinationFloor = (int)altitude;
	homeFloor = (int)altitude;
	journeyTime = 0;
	doorTicks = 0;
	holdTicks = 0;
	moveTicks = 0;
	yootGear = 0;
	yootOffset = 0;
	state = kIdle;
}

void Car::setAltitude(double a)
{
	if (altitude != a) {
		altitude = a;
		reposition();
	}
}

void Car::reposition()
{
	//2 and 4 were determined experimentally to center the car in the elevator.
	//A better method should probably be determined.
	sprite.setPosition({static_cast<float>(elevator->position.x * 8 + 2), static_cast<float>(-altitude * 36)});
}

void Car::updateSprite()
{
	// The 5 texture frames represent door animation phases:
	// 0: fully closed doors
	// 1: 25% open
	// 2: 50% open
	// 3: 75% open
	// 4: 100% open (showing cabin interior)
	int index = 0;
	if (state == kMoving || state == kIdle)
	{
		// Elevators must always keep doors closed while traveling or waiting idle
		index = 0;
	}
	else if (state == kHauling)
	{
		if (doorTicks >= 5)
			index = 2; // Opening doors
		else if (doorTicks == 1)
			index = 2; // Closing doors
		else if (doorTicks == 0)
			index = 0; // Closed
		else
			index = 4; // Fully open while passengers transfer
	}
	else if (state == kOpeningDoors)
	{
		index = 2;
	}
	else if (state == kClosingDoors)
	{
		index = 2;
	}

	if (elevator->yootCars) {
		// Box patterns: 0 empty, 1 one rider, 2 some, 3 crowded (4 = fire).
		const int n = (int)passengers.size();
		const int cap = std::max(1, (int)elevator->maxCarCapacity);
		sprite.setTextureRect(elevator->yootCars->band(n == 0 ? 0 : n == 1 ? 1 : n < cap / 2 ? 2 : 3));
		reposition();
		return;
	}
	int w = sprite.getTexture().getSize().x / 5;
	int h = sprite.getTexture().getSize().y;
	sprite.setTextureRect(sf::IntRect(sf::Vector2i(index*w, 0), sf::Vector2i(w, h)));
	reposition();
}

void Car::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
	render(target);
}

void Car::render(sf::RenderTarget & target) const
{
	sf::Color statusTint = sf::Color::White;
	if (game->statusMode == Game::kHotel)
	{
		statusTint = sf::Color(110, 110, 110, 160);
	}

	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255) || statusTint != sf::Color::White);

	if (tinted) {
		Sprite tmp = sprite;
		sf::Color composed = game->lighting.compose(sprite.getColor());
		if (statusTint != sf::Color::White)
		{
			composed.r = (composed.r * statusTint.r) / 255;
			composed.g = (composed.g * statusTint.g) / 255;
			composed.b = (composed.b * statusTint.b) / 255;
			composed.a = (composed.a * statusTint.a) / 255;
		}
		tmp.setColor(composed);
		target.draw(tmp);
	} else {
		target.draw(sprite);
	}

	//Draw the people stepping out of the elevator.
	Person *p = NULL;
	if (state == kHauling && (p = nextPassengerToUnmount())) {
		Sprite s;
		s.SetImage(app->bitmaps["simtower/elevator/people"]);
		s.setOrigin({static_cast<float>(direction == Elevator::kUp ? -elevator->shaft.getSize().x : 16), 24.f});

		//Calculate the texture subrect for the person stepping out of the car.
		int type = p->type;
		sf::IntRect sr;
		sr.position.x = type * 32;
		sr.size.x     = 16;
		sr.position.y = 24;
		sr.size.y     = 24;
		if (direction == Elevator::kUp) {
			sr.position.x += 16;
		}

		//Draw the person.
		sf::Color bodyColor = sf::Color::Black;
		if (statusTint != sf::Color::White)
		{
			bodyColor.r = (bodyColor.r * statusTint.r) / 255;
			bodyColor.g = (bodyColor.g * statusTint.g) / 255;
			bodyColor.b = (bodyColor.b * statusTint.b) / 255;
			bodyColor.a = (bodyColor.a * statusTint.a) / 255;
		}
		s.setColor(bodyColor);
		s.setTextureRect(sr);
		s.setPosition({elevator->shaft.getPosition().x, sprite.getPosition().y});
		target.draw(s);
	}
}

void Car::encodeXML(tinyxml2::XMLPrinter& xml)
{
	xml.PushAttribute("altitude", altitude);
	xml.PushAttribute("homeFloor", homeFloor);
}

void Car::decodeXML(tinyxml2::XMLElement& xml)
{
	setAltitude(xml.DoubleAttribute("altitude"));
	startAltitude = altitude;
	destinationFloor = altitude;
	if (xml.QueryIntAttribute("homeFloor", &homeFloor) != tinyxml2::XML_SUCCESS) {
		homeFloor = (int)altitude;
	}
}

void Car::setState(State s)
{
	if (state != s) {
		state = s;
		if (state == kIdle) {
			// YootTower keeps the sweep direction: the module's direction
			// only clears when it parks at home (FUN_1000c770 ->
			// StartToHomePos at the home floor), not on every idle.
			if (!elevator->prototype->yoot) direction = Elevator::kNone;
			elevator->respondToCalls();
		}
		updateSprite();
	}
}

int Car::CalcMoveSpeed(bool express, int distanceToTarget,
                       int distanceFromStart, int *stride)
{
	if (stride) *stride = 1;
	const bool atEnd = distanceToTarget <= 1 || distanceFromStart <= 1;
	if (atEnd) return 5; // crawl: one floor plus five-tick docking tail
	if (express && distanceToTarget > 4 && distanceFromStart > 4)
	{
		if (stride) *stride = 3;
		return 0; // express turbo, three floors per tick
	}
	if (!express && (distanceToTarget <= 3 || distanceFromStart <= 3))
		return 2; // standard/service middle gear
	return 0; // one floor per tick cruise
}

void Car::advance(double dt)
{
	const int passengersBefore = (int)passengers.size();
	const int doorTicksBefore = doorTicks;
	const State stateBefore = state;
// YootTower movers idle on every T2 sub-tick, 4 a game minute (T2.exe
	// 0x41dc40 runs T2MoverArrayList::DispatchIdle after each
	// IsIncDateTime): StdElev's crawl of 6 idles a floor is this car's
	// 1 + 5 frames. SimTower keeps its EXE frame clock (2600/day).
	const bool yoot = elevator->prototype->yoot != nullptr;
	const double frameDelta = yoot ? game->time.dta * 1440.0 * 4.0 : game->time.dta * Time::kSimFramesPerDay;
	journeyTime += frameDelta;
	if (journeyTime + 1e-7 < 1.0) return;
	int steps = (int)std::floor(journeyTime + 1e-7);
	journeyTime -= steps;
	if (steps > (yoot ? 1024 : 64)) steps = yoot ? 1024 : 64; // hitches cannot starve the door state machine
	for (int tick = 0; tick < steps; ++tick)
	{
		if (doorTicks > 0)
		{
			if (doorTicks == 5)
			{
				elevator->reassignCalls(this); // open doors steal calls at this floor
				while (Person *p = nextPassengerToUnmount())
				{
					passengers.erase(p);
					p->journey.next();
				}
				// YootTower: every stop holds defaultWait idles after the
				// boarding (StdElev FUN_1000bf30 -> state 4, FUN_1000c1b0).
				holdTicks = yoot ? elevator->prototype->yoot->defaultWait : elevator->schedulePatienceTicks();
				arrivingSound.Play(game);
			}
			if (doorTicks & 1)
			{
				Queue *q = yoot ? yootServedQueue()
				                : elevator->getQueue(destinationFloor, direction);
				if (doorTicks == 1)
				{
					while (!isFull() && q && !q->people.empty()) passengers.insert(q->popPerson());
				}
				else if (!isFull() && q && !q->people.empty())
					passengers.insert(q->popPerson());
				if (q && q->people.empty())
				{
					q->steppingInside = false;
					q->called = false;
					q->answered = false;
				}
				if (!yoot && elevator->scheduleMode() != kScheduleFull)
				{
					// Express schedules board the reverse direction too (ours).
					Queue *q2 = elevator->getQueue(destinationFloor, direction == Elevator::kUp ? Elevator::kDown : Elevator::kUp);
					if (doorTicks == 1)
					{
						while (!isFull() && q2 && !q2->people.empty()) passengers.insert(q2->popPerson());
					}
					else if (!isFull() && q2 && !q2->people.empty())
						passengers.insert(q2->popPerson());
				}
			}
			if (isFull())
			{
				doorTicks = 0; holdTicks = 0;
				elevator->finishStop(this);
				if (!passengers.empty()) setState(kClosingDoors);
				else setState(kIdle);
				if (yoot) { yootEndStop(); yootDecide(); }
				else elevator->decideCarDestination(this);
				continue;
			}
			if (yoot && doorTicks == 1 && holdTicks > 0)
			{
				// Hold (FUN_1000c1b0): someone new waiting reopens the
				// boarding window (state 3, counter 6), then holds again.
				Queue *q = yootServedQueue();
				if (q && !q->people.empty()) { doorTicks = 5; holdTicks = elevator->prototype->yoot->defaultWait; continue; }
				--holdTicks;
				continue;
			}
			if (!yoot && doorTicks == 1 && holdTicks > 0 && !isFull() &&
				(fabs(altitude - homeFloor) < 0.01 || destinationFloor == 0 || OT::Item::Lobby::isSkyLobbyFloor(destinationFloor) ||
				 (game->mainLobby && destinationFloor >= game->mainLobby->position.y &&
				  destinationFloor < game->mainLobby->position.y + game->mainLobby->size.y)))
			{
				--holdTicks;
				continue;
			}
			--doorTicks;
			if (doorTicks == 0)
			{
				if (yoot)
				{
					yootEndStop();
					if (passengers.empty()) { setState(kIdle); continue; }
					yootDecide();
				}
				else
				{
					elevator->finishStop(this);
					elevator->decideCarDestination(this);
					elevator->respondToCalls();
				}
			}
			continue;
		}

		if (state == kMoving && destinationFloor == (int)std::lround(altitude) && elevator->prototype->yoot)
		{
			// A moving car sent to the floor it stands on (respondToCalls on
			// a car that has just set off) would wait there forever with its
			// passengers: YootTower opens the doors instead.
			doorTicks = 5;
			setState(kHauling);
			continue;
		}
		if (state == kMoving && destinationFloor != (int)std::lround(altitude))
		{
			if (yoot)
			{
				yootDrive();
				continue;
			}
			if (moveTicks > 0) { --moveTicks; continue; }
			const int current = (int)std::lround(altitude);
			const int targetDistance = std::abs(destinationFloor - current);
			const int runDistance = std::abs(current - (int)std::lround(startAltitude));
			int stride = 1;
			moveTicks = CalcMoveSpeed(elevator->isExpress(), targetDistance, runDistance, &stride);
			const int left = targetDistance;
			if (stride > left) stride = left;
			setAltitude(current + (direction == Elevator::kUp ? stride : -stride));
			if ((int)std::lround(altitude) == destinationFloor)
			{
				if (!elevator->hasWork(this)) { doorTicks = 0; setState(kIdle); continue; }
				if (elevator->scheduleMode() != kScheduleFull) {
					const int mode = elevator->scheduleMode();
					const int extreme = mode == kScheduleExpressUp ? elevator->position.y + elevator->size.y - 1 : elevator->position.y;
					if (destinationFloor == extreme) shuttleReturning = true;
					direction = mode == kScheduleExpressUp ? Elevator::kDown : Elevator::kUp;
				} else {
					// A reversal hall stop boards the direction of its owned call.
					bool ahead = false;
					for (auto *p : passengers) if ((p->journey.toFloor - destinationFloor) * direction > 0) ahead = true;
					for (auto *q : elevator->queues) if (q->assignedCar == this && !q->people.empty() &&
					    (q->floor - destinationFloor) * direction > 0) ahead = true;
					if (!ahead) for (auto *q : elevator->queues) if (q->assignedCar == this &&
					    q->floor == destinationFloor && !q->people.empty()) { direction = q->direction; break; }
				}
				doorTicks = 5;
				setState(kHauling);
			}
			continue;
		}

		if (state == kIdle)
		{
			if (yoot)
			{
				// FUN_1000bd50/FUN_1000c770: a stopped car with stops
				// sweeps to them; an empty one goes home.
				if (!yootPickups.empty() || !passengers.empty()) { yootDecide(); continue; }
				if (std::abs((int)std::lround(altitude) - homeFloor) > 0)
				{
					direction = homeFloor > altitude ? Elevator::kUp : Elevator::kDown;
					moveTo(homeFloor);
				}
				continue;
			}
			Queue *up = elevator->getQueue((int)std::lround(altitude), Elevator::kUp);
			Queue *down = elevator->getQueue((int)std::lround(altitude), Elevator::kDown);
			if ((up && !up->people.empty() && (!up->assignedCar || up->assignedCar == this)) ||
			    (down && !down->people.empty() && (!down->assignedCar || down->assignedCar == this)))
			{
				direction = (up && !up->people.empty() && (!up->assignedCar || up->assignedCar == this)) ? Elevator::kUp : Elevator::kDown;
				destinationFloor = (int)std::lround(altitude);
				doorTicks = 5;
				setState(kHauling);
				continue;
			}
			if (std::abs((int)std::lround(altitude) - homeFloor) > 0)
			{
				direction = homeFloor > altitude ? Elevator::kUp : Elevator::kDown;
				moveTo(homeFloor);
			}
		}
	}

	if (passengersBefore != (int)passengers.size() || doorTicksBefore != doorTicks || stateBefore != state)
		updateSprite();
}

bool Car::isFull()
{
	return (passengers.size() >= elevator->maxCarCapacity);
}

Person * Car::nextPassengerToUnmount() const
{
	//Iterate through all passengers and return the first one we find that has the current floor
	//as its end floor, i.e. that needs to get off on this floor.
	for (Item::People::const_iterator ip = passengers.begin(); ip != passengers.end(); ip++)
		if ((*ip)->journey.toFloor == destinationFloor)
			return *ip;
	return NULL;
}

/**
 * One mover idle of a moving YootTower car (StdElev.dll; BigElev and
 * ServElev run the same code). The car gains speed[gear] x 6 units of a
 * 36-unit floor per idle: gear 0 (speed 1) 6 idles a floor, gear 1
 * (speed 3) 2 idles a floor, gear 2 (the Large Elevator's 12) 2 floors an
 * idle. At each floor boundary it shifts up while the floors left reach
 * T(n) = sum over i < n of (speed[i] + 5) / 6 and down below it; it only
 * stops from gear 0 (FUN_1000c200 / c3b0 / c590, T(n) FUN_1000cad0).
 */
void Car::yootDrive()
{
	const std::array<int, 3> & speed = elevator->prototype->yoot->speed;
	auto threshold = [&](int n) { int t = 0; for (int i = 0; i < n; ++i) t += (speed[i] + 5) / 6; return t; };
	const int current = (int)std::lround(altitude);
	int left = std::abs(destinationFloor - current);
	yootOffset += std::max(1, speed[yootGear]) * 6;
	int floors = std::min(left, yootOffset / 36);
	yootOffset %= 36;
	if (floors > 0)
		setAltitude(current + (direction == Elevator::kUp ? floors : -floors));
	if (yootOffset != 0) return;
	left -= floors;
	if (left == 0)
	{
		yootGear = 0;
		doorTicks = 5;
		setState(kHauling);
		return;
	}
	if (yootGear == 0 && speed[1] > 0 && left >= threshold(2)) yootGear = 1;
	else if (yootGear == 1 && left < threshold(2)) yootGear = 0;
	else if (yootGear == 1 && speed[2] > 0 && left >= threshold(3)) yootGear = 2;
	else if (yootGear == 2 && left < threshold(3)) yootGear = 1;
}

void Car::moveTo(int floor)
{
	if (state == kMoving && destinationFloor == floor && std::abs(altitude - floor) > 0.01) return;
	destinationFloor = floor;
	if (std::abs(altitude - floor) > 0.01)
	{
		if (state != kMoving) { yootGear = 0; yootOffset = 0; }
		destinationFloor = floor;
		startAltitude = altitude;
		moveTicks = 0;
		arrivingPlayed = false;
		departingPlayed = false;
		direction = floor > altitude ? Elevator::kUp : Elevator::kDown;
		setState(kMoving);
	}
	else
	{
		doorTicks = 5;
		setState(kHauling);
	}
}

/** Removes the given passenger from this car. Does nothing if the person is not in the car. */
void Car::removePassenger(Person *p)
{
	passengers.erase(p);
}

/**
 * YootTower: the queue this car is serving at its current stop — the
 * request appointed to it (GetAppointRequest 0x1002e100), matched by
 * appointment rather than by the car's direction so a car that was sent
 * home or just closed its doors still boards the call that owns it.
 */
Queue *Car::yootServedQueue()
{
	Queue *q = NULL;
	if (direction == Elevator::kUp || direction == Elevator::kDown)
		if ((q = elevator->findQueue(destinationFloor, direction)) && q->appointed == this) return q;
	if ((q = elevator->findQueue(destinationFloor, Elevator::kUp)) && q->appointed == this) return q;
	if ((q = elevator->findQueue(destinationFloor, Elevator::kDown)) && q->appointed == this) return q;
	return NULL;
}

/**
 * YootTower: the cost for this car to serve a call at `floor` going `dir`
 * (StdElev FUN_1000d070). A stopped car pays the plain distance. A moving
 * car pays the plain distance only when the call continues its sweep past
 * the current destination; otherwise it pays the full turnaround through
 * the destination, which keeps idle cars competitive and spreads the cars
 * over the calls (the original's formulas scale the non-ahead cases by
 * the sweep extent, making them strictly larger).
 */
int Car::yootSweepCost(int floor, Elevator::Direction dir) const
{
	const int cur = (int)std::lround(altitude);
	if (state != kMoving || direction == Elevator::kNone)
		return std::abs(floor - cur);
	const int s = direction == Elevator::kUp ? 1 : -1;
	if (dir == direction && (floor - destinationFloor) * s >= 0)
		return std::abs(floor - destinationFloor);
	return std::abs(destinationFloor - cur) + std::abs(floor - destinationFloor);
}

/**
 * YootTower: pick the next stop of the sweep (T2ElevatorModule::
 * SetNextStop 0x1002df50 + FUN_1000c770): the nearest floor with an
 * appointed pickup or a rider's destination in the current direction,
 * turning around when the sweep only has floors behind, and going home
 * (StartToHomePos 0x1002e0b0) when nothing is left.
 */
void Car::yootDecide()
{
	std::set<int> stops = yootPickups;
	for (Item::People::iterator ip = passengers.begin(); ip != passengers.end(); ++ip)
		stops.insert((*ip)->journey.toFloor);
	const int cur = (int)std::lround(altitude);
	int next = INT_MAX;
	if (direction == Elevator::kNone)
	{
		for (std::set<int>::iterator it = stops.begin(); it != stops.end(); ++it)
			if (next == INT_MAX || std::abs(*it - cur) < std::abs(next - cur)) next = *it;
	}
	else
	{
		const int s = direction == Elevator::kUp ? 1 : -1;
		for (std::set<int>::iterator it = stops.begin(); it != stops.end(); ++it)
			if ((*it - cur) * s > 0 && (next == INT_MAX || std::abs(*it - cur) < std::abs(next - cur))) next = *it;
		if (next == INT_MAX && !stops.empty())
		{
			// ChangeTurnPos: the sweep reverses at its extent.
			direction = direction == Elevator::kUp ? Elevator::kDown : Elevator::kUp;
			const int s2 = direction == Elevator::kUp ? 1 : -1;
			for (std::set<int>::iterator it = stops.begin(); it != stops.end(); ++it)
				if ((*it - cur) * s2 > 0 && (next == INT_MAX || std::abs(*it - cur) < std::abs(next - cur))) next = *it;
		}
	}
	if (next != INT_MAX)
	{
		LOG(DEBUG, "car %p decide: floor %d dir %d -> stop %d (pickups %zu, pax %zu)", (void*)this, cur, (int)direction, next, yootPickups.size(), passengers.size());
		moveTo(next);
		return;
	}
	if (cur != homeFloor)
	{
		direction = homeFloor > cur ? Elevator::kUp : Elevator::kDown;
		moveTo(homeFloor);
	}
	else
	{
		direction = Elevator::kNone;
		setState(kIdle);
	}
}

/**
 * YootTower: the boarding window at this floor is over (ClearOnStop
 * 0x1002dee0): the served queue loses its appointment (T2Request +0x24
 * back to -1) so the remaining waiters are re-appointed to the best car
 * on the next dispatch pass.
 */
void Car::yootEndStop()
{
	yootPickups.erase(destinationFloor);
	const Elevator::Direction dirs[2] = { Elevator::kUp, Elevator::kDown };
	for (int d = 0; d < 2; ++d)
	{
		Queue *q = elevator->findQueue(destinationFloor, dirs[d]);
		if (q && q->appointed == this) q->appointed = NULL;
	}
}
