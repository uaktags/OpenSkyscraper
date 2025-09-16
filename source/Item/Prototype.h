/* Copyright © 2012-2013 Fabian Schuiki */
/* Copyright © 2013 hoshi10 */
/* Copyright © 2025 Tim G */
#pragma once
#include <cstdio>
#include <string>
#include "../Math/Vector2D.h"
#include <tinyxml2.h>
#ifdef _DEBUG
#undef DEBUG
#endif

namespace OT {
	class Game;
	
	namespace Item {
		class Factory;
		class Item;
		
		enum ToolboxCategory {
			CAT_CONSTRUCTION = 10,
			CAT_HOUSING = 20,
			CAT_OFFICE = 30,
			CAT_HOTEL = 40,
			CAT_FOOD = 50,
			CAT_ENTERTAINMENT = 60,
			CAT_TRANSPORT = 70,
			CAT_UTILITIES = 80
		};
		
		class AbstractPrototype
		{
		public:
			std::string id;
			std::string name;
			int price;
			int2 size;
			// Minimum star rating required to unlock this prototype in the toolbox.
			// 0 means available from the start.
			int unlockRating;
			int icon;
			int entrance_offset;
			int exit_offset;
			int toolboxCategory;  // <-- Add this
			int toolboxOrder;     // <-- Add this
			
			virtual Item * make(Game * game) = 0;
			virtual ~AbstractPrototype() {}
			
			std::string desc() {
				char c[512];
				snprintf(c, 512, "Prototype '%s' $%i %s", this->id.c_str(), this->price, this->size.desc().c_str());
				return c;
			}
		};

		template <typename T>
		class Prototype : public AbstractPrototype
		{
			friend class Factory;
		protected:
				Item * make(Game * game) { return new T(game, this); }
		};
	}
}