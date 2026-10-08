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
#include "YootRules.h"
#include "XmlUtil.h"

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
	double routeStress(const Route &route)
	{
		if (route.nodes.size() < 2) return 0.0;
		int walkingFloors = 0;
		double stress = 0.0;
		for (size_t i = 1; i < route.nodes.size(); ++i)
		{
			const Route::Node &previous = route.nodes[i - 1];
			const Route::Node &current = route.nodes[i];
			const int floors = std::abs(current.toFloor - previous.toFloor);
			if (floors == 0) continue;
			if (isEscalatorNode(current.item))
				stress += floors * 16.0;
			else if (isStairNode(current.item))
				stress += floors * 35.0;
			else if (!isElevatorNode(current.item))
				walkingFloors += floors;
		}
		if (walkingFloors >= 80)  stress += 30.0;
		if (walkingFloors >= 125) stress += 60.0;
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
	yootEstimate = Yoot::kPeopleEstimateStart;
	yootTrips = 0;
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

bool Person::yootRules() const
{
	return game && game->activeRules.profile().yootWorld;
}

bool Person::yootWaitTick()
{
	int s = (int)stress;
	const bool gaveUp = Yoot::waitTick(s);
	stress = s;
	if (!gaveUp) return false;
	yootEstimate = Yoot::incEstimate(yootEstimate, -Yoot::kGiveUpEstimatePenalty);
	journey.jumpToDestination();
	return true;
}

void Person::recordNoRoute(int from, int to)
{
	if (yootRules())
	{
		// T2People::SolveNoRoute 0x10048c40: +30 per failed attempt; at
		// 300 "No route to destination.", jump there and -150 estimate.
		int s = (int)stress;
		const bool exhausted = Yoot::noRouteTry(s);
		stress = s;
		pendingJourneyStress = 0.0;
		if (!exhausted) return;
		yootEstimate = Yoot::incEstimate(yootEstimate, -Yoot::kNoRouteEstimatePenalty);
		yootFinishTrip();
		std::set<FloorPair> &latched = noRouteMessages[game];
		if (latched.insert(FloorPair(from, to)).second)
			game->timeWindow.showMessage("No route to destination.");
		return;
	}
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

int Person::yootCurrentEstimate() const
{
	return Yoot::calcCurrEstimate(yootEstimate, yootTrips, (int)stress);
}

void Person::yootFinishTrip()
{
	int s = (int)stress;
	Yoot::updateEstimation(yootEstimate, yootTrips, s);
	stress = s;
	pendingJourneyStress = 0.0;
}

void Person::yootArrived(Item::Item * destination)
{
	if (game) game->eventSystem.facilities.onArrive(this, destination);
}

void Person::advance(double dt)
{
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
	if (yootTrips != 0 || yootEstimate != Yoot::kPeopleEstimateStart)
	{
		xml.PushAttribute("yootEstimate", yootEstimate);
		xml.PushAttribute("yootTrips", yootTrips);
	}
	if (yootFlags != 0) xml.PushAttribute("yootFlags", yootFlags);
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
	name    = xmlAttributeOr(xml, "name", "");
	from    = xmlAttributeOr(xml, "from", "");
	goingTo = xmlAttributeOr(xml, "goingTo", "");
	yootEstimate = Yoot::clampEstimate(xml.IntAttribute("yootEstimate", Yoot::kPeopleEstimateStart));
	yootTrips = std::max(0, std::min(Yoot::kMaxTrips, xml.IntAttribute("yootTrips", 0)));
	yootFlags = xml.IntAttribute("yootFlags", 0);
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
	while (!nodes.empty()) nodes.pop();
	for (std::vector<Route::Node>::const_iterator nit = r.nodes.begin(); nit != r.nodes.end(); ++nit)
		nodes.push(*nit);

	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		person->recordNoRoute(fromFloor, toFloor);
		return;
	}

	// Queue wait and movement penalties remain banked until final arrival.
	// YootTower stress comes from waiting and failed routes only (P1).
	if (!person->yootRules())
		person->addJourneyStress(routeStress(r));
	else if (r.nodes.front().item && r.nodes.front().item->isLobby() && person->game)
		person->game->eventSystem.facilities.onEnterTower(person); // leaves the pool
	if (person->yootRules() && person->game) person->game->eventSystem.extras.bookRoute(r); // T2TrafficInfo::Pass
	toFloor = nodes.front().toFloor;
	next();
}

void Person::Journey::next()
{
	// Remove the person from where he/she is currently at.
	if (person->at) person->at->removePerson(person);

	fromFloor = toFloor;

	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		if (person->yootRules()) person->yootFinishTrip();
		else person->deliverJourneyStress();
		return;
	}

	nodes.pop();
	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		if (person->yootRules()) person->yootFinishTrip();
		else person->deliverJourneyStress();
		return;
	}

	item    = nodes.front().item;
	toFloor = nodes.front().toFloor;
	assert(item);
	// The last node is the destination tenant, which takes the person in
	// addPerson (and may hand it straight on), so arrive first.
	if (nodes.size() == 1 && person->yootRules()) person->yootArrived(item);
	item->addPerson(person);
}

void Person::Journey::jumpToDestination()
{
	if (person->at) person->at->removePerson(person);
	while (nodes.size() > 1) nodes.pop();
	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		return;
	}
	fromFloor = toFloor;
	item    = nodes.front().item;
	toFloor = nodes.front().toFloor;
	if (item) item->addPerson(person);
}

void Person::Journey::clear()
{
	while (!nodes.empty()) nodes.pop();
	item = NULL;
	toFloor = fromFloor;
}

int Person::getWidth()
{
	return (type >= kHousekeeper ? 16 : 8);
}
