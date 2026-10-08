#include <algorithm>
#include <cassert>
#include <cmath>
#include "../../Game.h"
#include "Car.h"
#include "Queue.h"

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
		journeyTime = 0;
		if (state == kIdle) {
			direction = Elevator::kNone;
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
	const double frameDelta = game->time.dta / Time::kBaseSpeed;
	journeyTime += frameDelta;
	if (journeyTime < 1.0) return;
	int steps = (int)journeyTime;
	journeyTime -= steps;
	if (steps > 64) steps = 64; // hitches cannot starve the door state machine

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
				holdTicks = elevator->schedulePatienceTicks();
				arrivingSound.Play(game);
			}
			if (doorTicks & 1)
			{
				Queue *q = elevator->getQueue(destinationFloor, direction);
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
			}
			if (isFull())
			{
				doorTicks = 0; holdTicks = 0;
				if (!passengers.empty()) setState(kClosingDoors);
				else setState(kIdle);
				elevator->decideCarDestination(this);
				continue;
			}
			if (doorTicks == 1 && holdTicks > 0 && !isFull() &&
				(fabs(altitude - homeFloor) < 0.01 || destinationFloor == 0 ||
				 (game->mainLobby && destinationFloor >= game->mainLobby->position.y &&
				  destinationFloor < game->mainLobby->position.y + game->mainLobby->size.y)))
			{
				--holdTicks;
				continue;
			}
			--doorTicks;
			if (doorTicks == 0)
			{
				if (passengers.empty()) { setState(kIdle); continue; }
				elevator->decideCarDestination(this);
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
				doorTicks = 5;
				setState(kHauling);
			}
			continue;
		}

		if (state == kIdle)
		{
			Queue *up = elevator->getQueue((int)std::lround(altitude), Elevator::kUp);
			Queue *down = elevator->getQueue((int)std::lround(altitude), Elevator::kDown);
			if ((up && up->called && elevator->scheduleAllows(Elevator::kUp)) ||
			    (down && down->called && elevator->scheduleAllows(Elevator::kDown)))
			{
				direction = (up && up->called && elevator->scheduleAllows(Elevator::kUp)) ? Elevator::kUp : Elevator::kDown;
				destinationFloor = (int)std::lround(altitude);
				doorTicks = 1;
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

void Car::moveTo(int floor)
{
	if (destinationFloor != floor)
	{
		destinationFloor = floor;
		startAltitude = altitude;
		journeyTime = 0;
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
