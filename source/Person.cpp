#include <cassert>
#include "Item/Item.h"
#include "Person.h"

using namespace OT;


Person::Person(Game * game, Type type)
:	GameObject(game),
	journey(this),
	type(type)
{
	at     = NULL;
	state  = kWandering;
	stress = 0.0;
	eval   = 0.0;
}

Person::~Person()
{
	if (at) {
		LOG(DEBUG, "forcefully removed from %s", at->desc().c_str());
		at->removePerson(this);
	}
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
	eval    = xml.DoubleAttribute("eval", 0.0);
	name    = xml.Attribute("name", "");
	from    = xml.Attribute("from", "");
	goingTo = xml.Attribute("goingTo", "");
}

Person::Journey::Journey(Person * p)
:	person(p)
{
	fromFloor = 0;
	item = NULL;
	toFloor = 0;
}

void Person::Journey::set(const Route & r)
{
	while (!nodes.empty()) nodes.pop();
	for (std::vector<Route::Node>::const_iterator nit = r.nodes.begin(); nit != r.nodes.end(); nit++) {
		nodes.push(*nit);
	}
	// If the route is empty, the person is stuck; bail out before touching toFloor/next().
	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		return;
	}
	toFloor = nodes.front().toFloor;
	next();
}

void Person::Journey::next()
{
	//Remove the person from where he/she is currently at.
	if (person->at) person->at->removePerson(person);

	//Keep the current floor around.
	fromFloor = toFloor;

	//Guard against an empty route - this can happen when a Person has no valid
	//path to their destination. Leave them where they are rather than crashing.
	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		return;
	}

	//Jump to next node.
	nodes.pop();

	//If we've consumed the only node, the journey is complete - keep the person
	//where they are without adding them to a (non-existent) next item.
	if (nodes.empty()) {
		item = NULL;
		toFloor = fromFloor;
		return;
	}

	//Add the person to the node's item.
	item    = nodes.front().item;
	toFloor = nodes.front().toFloor;
	assert(item);
	item->addPerson(person);
}

int Person::getWidth()
{
	return (type >= kHousekeeper ? 16 : 8);
}
