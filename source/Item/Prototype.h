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
	struct YootItemStats;
	
	namespace Item {
		class Factory;
		class Item;
		
		class AbstractPrototype
		{
		public:
			std::string id;
			std::string name;
			int price;
			int2 size;
			int icon;
			int variant;
			int entrance_offset;
			int exit_offset;
			int carCost = 0; ///< Optional per-car cost for elevator shafts (0 = none).
			/// YootTower catalog entry (TNDF/MVDF data, plan step 4); null
			/// outside YootTower mode. Owned by Game::yootCatalog, which
			/// also overlays price and carCost while it is set.
			const YootItemStats * yoot = nullptr;
			
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