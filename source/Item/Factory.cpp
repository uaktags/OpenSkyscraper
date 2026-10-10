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
#include "PowerRoom.h"
#include "YootFacilityItems.h"
#include "YootVenueItems.h"
#include "YootTenant.h"
#include "YootSpecialItems.h"
#include "YootMiscItems.h"

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
	register_item(PowerRoom); // YootTower-only (BuildRules hides it elsewhere)
	register_item(TrashFacility); // YootTower-only facilities (plan step 12)
	register_item(Restroom);
	register_item(DoctorsOffice);
	register_item(Theater); // YootTower-only venues (plan step 14 part B)
	register_item(ClassicTheater);
	register_item(EventCenter);
	register_item(Billboard);
	register_item(Stadium);
	register_item(YootCathedral);
	register_item(Attraction);
	register_item(Observatory);
	register_item(TicketBooth);
	register_item(GatheringArea);
	register_item(BoatDeck);
	register_item(Subway);
	register_item(FrontDesk); // YootTower-only special items (plan step 14)
	register_item(ParkingGate);
	register_item(ParkingLot);
	register_item(FireDepartment);
	register_item(Apartment);
	register_item(School); // YootTower-only, closed after step 14
	register_item(SkyBridge);
	#undef register_item
	// YootTower-only: one prototype per shop / restaurant / vending /
	// service tenant plugin found in the catalog (YootTenants.h).
	registerYootTenantPrototypes(prototypes, game->app.pluginManager);
	
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
	// YootTower InitEquip: build day = CalcTotalDays (saved items restore theirs).
	if (game->activeRules.profile().yootWorld)
		item->yootBuildDay = (int)floor(game->time.absolute);
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
	if (!prototype) LOG(ERROR, "Factory::make: no prototype registered for id '%s'", prototypeID.c_str());
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
	// WP7b: generalised. Any fixed-size type (its element carries no "width",
	// so the footprint always comes from the current prototype) is rebuilt at
	// the prototype size with its bottom-left anchor kept, clamped to the lot.
	// Variable-width items (floor, lobby, sky bridge...) save their width and
	// are left alone; YootTower saves keep their own footprints, except the
	// cathedral which is always migrated.
	{
		auto proto = prototypesById.find(type);
		const bool migrate = type == "cathedral" ||
			(!game->activeRules.profile().yootWorld && !xml.Attribute("width"));
		if (proto != prototypesById.end() && migrate)
			position.x = game->activeRules.clampHorizontalPosition(position.x, proto->second->size.x);
	}
	// YootTower-only prototypes stay registered for every ruleset (the
	// build UI hides them via minRatingToBuild), but a save from another
	// ruleset must not resurrect them: their advance()/decode paths run
	// ungated YootTower logic. Reject and let the caller skip the element.
	// Housekeeping is the exception: SimTower owns the type-15 rooms (maid pool,
	// 1180:0bbe bills row 15 per room), so a SimTower save or .TDT import keeps it.
	if (game->activeRules.ruleset() != BuildRules::Ruleset::YootTower &&
	    type != "housekeeping" && BuildRules::isYootOnlyPrototype(type))
	{
		LOG(WARNING, "skipping YootTower-only item '%s' in a %s save",
		    type.c_str(), game->activeRules.displayName());
		return NULL;
	}
	Item * item = make(type, position);
	assert(item);
	item->decodeXML(xml);
	return item;
}
