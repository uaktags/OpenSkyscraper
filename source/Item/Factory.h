/* Copyright © 2012 Fabian Schuiki */
/* Copyright © 2016 Matthew Harvey */
/* Copyright © 2025 Tim G */
#pragma once
#include <map>
#include <string>
#include <tinyxml2.h>
#include <vector>
#include "../GameObject.h"
#include "Prototype.h"

namespace OT {

	/*
	 * The Game.cpp refers to the different objects
	 * by the icon number, so this is an enum to
	 * give readable names to the icon numbers.
	 */
	enum IconNumbers
	{
		ICON_LOBBY = 0,
		ICON_FLOOR = 1,
		ICON_STAIRS = 2,
		ICON_ESCALATOR = 3,
		ICON_ELEVATOR = 4,
		ICON_SERVICE_ELEVATOR = 5,
		ICON_EXPRESS_ELEVATOR = 6,
		ICON_OFFICE = 7,
		ICON_HOTEL_ROOM = 8,
		ICON_DELUX_HOTEL_ROOM = 9,
		ICON_HOTEL_SUITE = 10,
		ICON_FASTFOOD = 11,
		ICON_RESTAURANT = 12,
		ICON_SHOPS = 13,
		ICON_CINEMA = 14,
		ICON_PARTYHALL = 15,
		ICON_PARKING_FLOOR = 16,
		ICON_PARKING_GARAGE = 17,
		ICON_RECYCLING_CENTER = 18,
		ICON_METRO = 19,
		ICON_CATHEDRAL = 20,
		ICON_SECURITY = 21,
		ICON_MEDICAL_CENTER = 22,
		ICON_HOUSEKEEPING = 23,
		ICON_CONDO = 24,
		ICON_SECOM = 25,
	};

	namespace Item {
		
		class Factory : public GameObject
		{
		public:
			Factory(Game * game) : GameObject(game) {}
			~Factory();
			
			std::vector<AbstractPrototype *> prototypes;
			std::map<std::string, AbstractPrototype *> prototypesById;
			void loadPrototypes();
			
			Item * make(AbstractPrototype * prototype, int2 position = int2());
			Item * make(std::string prototypeID, int2 position = int2());
			Item * make(tinyxml2::XMLElement & xml);
		};
	}
}