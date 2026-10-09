#include <cassert>
#include <sstream>
#include "../Application.h"
#include "../Game.h"
#include "Elevator/Express.h"
#include "Elevator/Service.h"
#include "Elevator/Standard.h"
#include "Escalator.h"
#include "Factory.h"
#include "FastFood.h"
#include "Floor.h"
#include "Hotel.h"
#include "Lobby.h"
#include "Restaurant.h"
#include "Shop.h"
#include "Stairs.h"
#include "PartyHall.h"
#include "Cinema.h"
#include "Metro.h"
#include "Office.h"
#include "Parking.h"
#include "ParkingRamp.h"
#include "Housekeeping.h"
#include "Condo.h"
#include "YootCondo.h"
#include "Security.h"
#include "MedicalCenter.h"
#include "Recycling.h"
#include "Cathedral.h"

using namespace OT;
using namespace Item;


Factory::~Factory()
{
	for (int i = 0; i < prototypes.size(); i++) {
		delete prototypes[i];
	}
	prototypes.clear();
	prototypesById.clear();
}

void Factory::loadPrototypes()
{
	assert(prototypes.empty() && "prototypes mustn't be loaded multiple times");
	
	#define register_item(cls) prototypes.push_back(cls::makePrototype());
	register_item(Lobby);
	register_item(Floor);
	register_item(Stairs);
	register_item(Escalator);
	register_item(Elevator::Express);
	register_item(Elevator::Service);
	register_item(Elevator::Standard);
	register_item(FastFood);
	register_item(Restaurant);
	register_item(Housekeeping);
	register_item(Shop);
	prototypes.push_back(Hotel::makeSinglePrototype());
	prototypes.push_back(Hotel::makeDoublePrototype());
	prototypes.push_back(Hotel::makeSuitePrototype());
	register_item(PartyHall);
	register_item(Cinema);
	register_item(Metro);
	register_item(Office);
	register_item(Parking);
	register_item(ParkingRamp);
	register_item(Condo);
	register_item(YootCondo);
	register_item(Security);
	prototypes.push_back(Security::makeSecomPrototype());
	register_item(MedicalCenter);
	register_item(Recycling);
	register_item(Cathedral);
	#undef register_item
	
	for (int i = 0; i < prototypes.size(); i++) {
		prototypesById[prototypes[i]->id] = prototypes[i];
	}
}

OT::Item::Item * Factory::make(AbstractPrototype * prototype, int2 position)
{
	assert(prototype);
	Item * item = prototype->make(game);
	item->setPosition(position);
	item->init();
	// Mark fresh placements as under construction. Saved games call
	// make() too, but their decodeXML() run immediately afterwards and
	// restore the persisted underConstruction flag, overriding this.
	double duration = item->constructionDuration();
	if (duration > 0.0)
	{
		item->underConstruction = true;
		item->constructionEndTime = game->time.absolute + duration;
	}
	else
	{
		item->underConstruction = false;
		item->constructionEndTime = 0.0;
	}
	return item;
}

OT::Item::Item * Factory::make(std::string prototypeID, int2 position)
{
	AbstractPrototype * prototype = prototypesById[prototypeID];
	return make(prototype, position);
}

OT::Item::Item * Factory::make(tinyxml2::XMLElement & xml)
{
	int2 position(xml.IntAttribute("x"), xml.IntAttribute("y"));
	std::string type = xml.Attribute("type") ? xml.Attribute("type") : "";
	// Legacy migration: older saves stored every hotel room under the single
	// "hotel" id and disambiguated the room type with a "variant" attribute.
	// Redirect those entries to the new per-variant prototypes so the correct
	// size, icon and behaviour are restored.
	if (type == "hotel")
	{
		switch (xml.IntAttribute("variant", 0))
		{
			case 1:  type = "hotel_double"; break;
			case 2:  type = "hotel_suite";  break;
			default: type = "hotel_single"; break;
		}
	}
	// Fixed-size rooms did not serialize dimensions in older native saves.
	// Reconstruct legacy 48x4 cathedrals as 28x5 before init()/addItem(), so
	// the sprite and spatial indexes share the preserved bottom-left anchor.
	if (type == "cathedral")
		position.x = game->activeRules.clampHorizontalPosition(position.x, prototypesById.at(type)->size.x);
	Item * item = make(type, position);
	assert(item);
	item->decodeXML(xml);
	return item;
}
