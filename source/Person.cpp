#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include "Game.h"
#include "Item/Item.h"
#include "NameManager.h"
#include "Person.h"

using namespace OT;
const double Person::kMaxStress = 300.0;

namespace
{
	typedef std::pair<int, int> FloorPair;
	std::map<Game *, std::set<FloorPair> > noRouteMessages;

	bool isElevatorNode(Item::Item *item)
	{
		return item && item->isElevator();
	}

	bool isStairNode(Item::Item *item)
	{
		return item && item->prototype->id == "stairs";
	}

	bool isEscalatorNode(Item::Item *item)
	{
		return item && item->prototype->id == "escalator";
	}

	/// Route nodes describe the transport used by the leg ending at that node.
	double routeStress(const Route &route, const Person &person)
	{
		if (person.isServicePerson() || route.nodes.size() < 2) return 0.0;
		const Item::Item *origin = person.at ? person.at : route.nodes.front().item;
		const double originX = origin ? origin->position.x + origin->size.x / 2.0 : 0.0;
		double stress = 0.0;
		bool chargedWalk = false;
		for (size_t i = 1; i < route.nodes.size(); ++i)
		{
			const Route::Node &previous = route.nodes[i - 1];
			const Route::Node &current = route.nodes[i];
			const int floors = std::abs(current.toFloor - previous.toFloor);
			if (isEscalatorNode(current.item)) stress += floors * 16.0;
			else if (isStairNode(current.item)) stress += floors * 35.0;
			if (!chargedWalk && current.item &&
			    (current.item->isStairlike() || isElevatorNode(current.item)))
			{
				chargedWalk = true;
				if (current.item->prototype->icon != 6)
				{
					const double distance = std::abs(current.item->position.x - originX);
					stress += distance >= 125 ? 60.0 : distance >= 80 ? 30.0 : 0.0;
				}
			}
		}
		return stress;
	}
}

Person::Person(Game * game, Type type)
: GameObject(game),
  journey(this),
  type(type)
{
	at     = NULL;
	state  = kWandering;
	stress = 0.0;
	eval   = 0.0;
	pendingJourneyStress = 0.0;
	animOffset = (double)(rand() % 1000) / 1000.0;
	name   = NameManager::makeName((int)type);
	if (game) game->people.insert(this);
}

Person::~Person()
{
	if (at) {
		LOG(DEBUG, "forcefully removed from %s", at->desc().c_str());
		at->removePerson(this);
	}
	if (game) game->people.erase(this);
}

void Person::addStress(double amount)
{
	if (!std::isfinite(amount)) return;
	stress = std::max(0.0, std::min(kMaxStress, stress + amount));
}

void Person::addJourneyStress(double amount)
{
	if (!std::isfinite(amount) || amount <= 0.0) return;
	pendingJourneyStress = std::max(0.0,
		std::min(kMaxStress, pendingJourneyStress + amount));
}

void Person::deliverJourneyStress()
{
	if (pendingJourneyStress <= 0.0) return;
	addStress(pendingJourneyStress);
	pendingJourneyStress = 0.0;
}

void Person::recordNoRoute(int from, int to)
{
	addStress(kMaxStress);
	pendingJourneyStress = 0.0;
	if (!game) return;
	std::set<FloorPair> &latched = noRouteMessages[game];
	if (latched.insert(FloorPair(from, to)).second)
	{
		char message[128];
		std::snprintf(message, sizeof(message),
			"People on Floor %d need a path to Floor %d", from, to);
		game->timeWindow.showMessage(message);
	}
}

void Person::advance(double dt)
{
	if (journey.retryPending) journey.retry();
	if (state == kHome || state == kResting)
		recoverStress(dt * 0.5);
}

void Person::encodeXML(tinyxml2::XMLPrinter &xml)
{
	xml.PushAttribute("type", type);
	xml.PushAttribute("state", state);
	xml.PushAttribute("stress", stress);
	xml.PushAttribute("eval", eval);
	xml.PushAttribute("name", name.c_str());
	xml.PushAttribute("from", from.c_str());
	xml.PushAttribute("goingTo", goingTo.c_str());
}

void Person::decodeXML(tinyxml2::XMLElement &xml)
{
	type    = (Type)xml.IntAttribute("type", kMan);
	state   = (State)xml.IntAttribute("state", kWandering);
	stress  = xml.DoubleAttribute("stress", 0.0);
	if (!std::isfinite(stress)) stress = 0.0;
	stress  = std::max(0.0, std::min(kMaxStress, stress));
	eval    = xml.DoubleAttribute("eval", 0.0);
	pendingJourneyStress = 0.0;
	name    = xml.Attribute("name", "");
	from    = xml.Attribute("from", "");
	goingTo = xml.Attribute("goingTo", "");
}

Person::Journey::Journey(Person * p)
: person(p)
{
	fromFloor = 0;
	item = NULL;
	toFloor = 0;
}

void Person::Journey::set(const Route & r)
{
	retryPending = replanPending = false;
	waitingAt = nullptr;
	destination = r.empty() ? nullptr : r.nodes.back().item;
	destinationFloor = r.empty() ? fromFloor : r.nodes.back().toFloor;
	while (!nodes.empty()) nodes.pop();
	for (std::vector<Route::Node>::const_iterator nit = r.nodes.begin(); nit != r.nodes.end(); ++nit)
		nodes.push(*nit);

	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		person->recordNoRoute(fromFloor, toFloor);
		person->state = Person::kResting;
		return;
	}

	// Queue wait and movement penalties remain banked until final arrival.
	person->addJourneyStress(routeStress(r, *person));
	toFloor = nodes.front().toFloor;
	next();
}

void Person::Journey::next()
{
	retryPending = false;
	waitingAt = person->at ? person->at : nodes.empty() ? nullptr : nodes.front().item;
	fromFloor = toFloor;
	if (!nodes.empty()) nodes.pop();
	if (nodes.empty()) {
		if (person->at) person->at->removePerson(person);
		item = nullptr;
		waitingAt = nullptr;
		destination = nullptr;
		replanPending = false;
		toFloor = fromFloor;
		person->deliverJourneyStress();
		return;
	}
	item = nodes.front().item;
	toFloor = nodes.front().toFloor;
	assert(item);
	// A rejected elevator rider stays in the origin item, with this leg
	// intact. Only a successful admission detaches them from that item.
	if (item->isElevator() && waitingAt && waitingAt->canHaulPeople()) {
		if (person->at) person->at->removePerson(person);
		auto landing = person->game->floorItems.find(fromFloor);
		waitingAt = landing == person->game->floorItems.end() ? nullptr : landing->second;
		if (waitingAt) waitingAt->Item::Item::addPerson(person);
	}
	if (!item->isElevator() && person->at) person->at->removePerson(person);
	item->addPerson(person);
}

void Person::Journey::queueRejected()
{
	if (!retryPending) retryState = person->state;
	retryPending = true;
	retryFrame = person->game->time.absolute * Time::kSimFramesPerDay + 16;
	person->state = Person::kPlanning;
	if (!person->at && waitingAt && !waitingAt->canHaulPeople())
		waitingAt->Item::Item::addPerson(person);
}

void Person::Journey::queueAccepted()
{
	if (retryPending) person->state = retryState;
	retryPending = false;
}

void Person::Journey::retry()
{
	if (!retryPending ||
	    person->game->time.absolute * Time::kSimFramesPerDay + 1e-7 < retryFrame) return;
	if (replanPending) {
		retryFrame = person->game->time.absolute * Time::kSimFramesPerDay + 16;
		const Route route = person->game->findRoute(person->at, destination,
		                                          person->isServicePerson());
		if (route.empty()) return;
		person->state = retryState;
		set(route);
	} else if (item) {
		item->addPerson(person);
	}
}

void Person::Journey::failWaiting()
{
	const int floor = fromFloor;
	Item::Item *restAt = waitingAt;
	if (person->at) person->at->removePerson(person);
	while (!nodes.empty()) nodes.pop();
	item = nullptr;
	if (!retryPending) retryState = person->state;
	retryPending = replanPending = true;
	retryFrame = person->game->time.absolute * Time::kSimFramesPerDay + 16;
	waitingAt = nullptr;
	fromFloor = floor;
	if (destination) toFloor = destinationFloor;
	person->deliverJourneyStress();
	person->state = Person::kPlanning;
	// Keep the intended destination and re-plan from the landing until a
	// route becomes available or the owning item sets a new journey.
	if (!restAt || restAt->canHaulPeople()) {
		auto it = person->game->floorItems.find(floor);
		restAt = it == person->game->floorItems.end() ? nullptr : it->second;
	}
	if (restAt) restAt->Item::Item::addPerson(person);
}

int Person::getWidth()
{
	return (type >= kHousekeeper ? 16 : 8);
}

// Cancel without delivering wait/movement stress. Virtual detachment clears
// elevator queues/cars and stair timers before any referenced item is deleted.
void Person::Journey::cancel()
{
 if (person->at) person->at->removePerson(person);
 while (!nodes.empty()) nodes.pop();
 item = NULL;
 waitingAt = nullptr;
 retryPending = replanPending = false;
 destination = nullptr;
 destinationFloor = 0;
 fromFloor = toFloor = 0;
 person->pendingJourneyStress = 0;
 person->state = Person::kIdle;
 person->from.clear();
 person->goingTo.clear();
}

bool Person::Journey::references(const Item::Item * target) const
{
 if (item == target || person->at == target || waitingAt == target || destination == target) return true;
 std::queue<Route::Node> remaining = nodes;
 while (!remaining.empty()) {
  if (remaining.front().item == target) return true;
  remaining.pop();
 }
 return false;
}
