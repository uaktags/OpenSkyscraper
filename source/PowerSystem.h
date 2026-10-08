/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "YootEvents.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <map>
#include <string>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	namespace Item { class PowerRoom; }

	/** YootTower power supply and blackout (game-parity.md I1/V7).
	 *
	 * Supply is BOEV defSupply (5000) plus addSupply (5000) per transformer
	 * in the Power Room. Demand is the sum of every built item's
	 * consumptionPower (ToolDef +0xf0, from the plugin TemplateDef behind
	 * the item's prototype, see yootPrototypeLinks). The rules live in the
	 * pure YootPowerModel (T2BlackOut 0x1001b580..0x1001ba90); this class
	 * feeds it from the tower and does the messages, stress and darkening.
	 *
	 * Owned by EventSystem: the T2EV scheduler starts the daily BOEV check
	 * (11:00), and EventSystem drives advance/draw/persistence. Inactive
	 * (no-op) outside YootTower and in worlds without a BOEV (Kegon). */
	class PowerSystem : public GameObject
	{
	public:
		PowerSystem(Game * game);

		/// Reset state and (re)read the BOEV event; null disables power.
		void configure(const YootEventSpec * boev);
		void reset();
		bool enabled() const { return active; }

		/// Per-frame: refresh supply/demand, the shortage warning and the
		/// blackout state machine (one T2BlackOut::Exec per new minute).
		void advance();
		/// BOEV begin (11:00). Returns true when a blackout started.
		bool startCheck();
		bool isBlackOut() const { return active && model.isBlackOut(); }

		int supply() const { return model.supply(); }
		int demand() const { return model.consumption(); }
		int transformers() const;
		int transformerPriceDollars() const { return model.def.transformerPrice * 100; }
		/// PowerRoom dialog "add transformer" (FUN_1000c2f0 + FUN_1000c4a0):
		/// pays the price, adds one (max 5) and closes the breakers.
		bool addTransformer(Item::PowerRoom * room, std::string * reason = 0);
		/// Power use of one prototype (0 when it has no YootTower plugin).
		int powerFor(const std::string & prototypeId);

		/// T2Tenant::Draw 0x1005e4d0: DrawColorMesh over every tenant while
		/// IsBlackOut. Movers (elevators, stairs) stay lit.
		void drawDarkening(sf::RenderTarget & target) const;

		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

	private:
		bool active;
		YootPowerModel model;
		int lastMinute;
		int lastDemand;
		std::map<std::string, int> powerByPrototype;

		Item::PowerRoom * powerRoom() const;
		void refresh();
		void warnShortage();
	};
}
