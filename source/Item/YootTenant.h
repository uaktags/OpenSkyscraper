/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include <vector>
#include "../Sprite.h"
#include "../YootTenants.h"
#include "Item.h"

namespace OT {
	class PluginManager;

	namespace Item {
		/** Prototype of one plugin-backed YootTower tenant ("yt_<stem>"):
		 * identity, footprint, price and art come from the plugin's ATTR,
		 * TNDF and OBJMAP (YootTenantDef). make() picks the family class.
		 * YootTower-only: BuildRules hides these prototypes elsewhere. */
		class YootTenantPrototype : public AbstractPrototype
		{
		public:
			YootTenantDef def;
			Item * make(Game * game);
		};

		/// Registers one prototype per covered tenant plugin in the catalog.
		void registerYootTenantPrototypes(std::vector<AbstractPrototype *> & prototypes, const PluginManager & plugins);

		/** Common base of the plugin-backed tenant families. The behaviour is
		 * the pure Yoot::TenantModel (YootTenants.h), stepped once per game
		 * minute with an idle every 8 minutes; this class feeds it the
		 * clock, books the income and draws RT_BITMAP 1000 sliced by the
		 * OBJMAP 1000 `View` patterns (pattern = numOfPattern x kind + p). */
		class YootTenant : public Item
		{
		public:
			YootTenant(Game * game, AbstractPrototype * prototype);
			virtual ~YootTenant() {}

			const YootTenantDef & def() const;

			virtual void init();
			virtual void advance(double dt);
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			virtual bool isOccupied() const { return true; }
			bool isOpen() const { return model.status >= Yoot::kStatusOpen && model.status < 10000; }

			Yoot::TenantModel model;
			long long lastMinute = -1;
			int idlePhase = 0;          ///< minute (mod 8) of this tenant's idle slot
			bool reachable = true;      ///< last known route from the lobby

		protected:
			/// Settlement category and info-bar text of the income.
			virtual const char * incomeCategory() const { return "retail_income"; }
			virtual const char * incomeMessage() const { return nullptr; }

			Sprite sprite;
			const sf::Texture * view = nullptr;
			const std::vector<sf::IntRect> * bands = nullptr;
			void updateSprite();
		};

		/// Shops (Barber.t2p family, 28 plugins + Spa).
		class YootShop : public YootTenant
		{
		public:
			YootShop(Game * game, AbstractPrototype * prototype) : YootTenant(game, prototype) {}
		protected:
			const char * incomeMessage() const;
		};

		/// Restaurants, fast food and the food court (Burger.t2p family, 22).
		class YootRestaurant : public YootTenant
		{
		public:
			YootRestaurant(Game * game, AbstractPrototype * prototype) : YootTenant(game, prototype) {}
		protected:
			const char * incomeMessage() const { return "Income from restaurants just came in."; }
		};

		/// Vending machines (Drink.t2p family, 5): pay silently (DoPay).
		class YootVending : public YootTenant
		{
		public:
			YootVending(Game * game, AbstractPrototype * prototype) : YootTenant(game, prototype) {}
		};
	}
}
