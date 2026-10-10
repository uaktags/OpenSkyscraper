/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower power: T2BlackOut (T2DLL 0x1001b580..0x1001ba90), the BOEV
 * event, T2TowerEvent::SetLatestBuild (0x100692a0) and the PowerRoom.t2p
 * transformer dialog. game-parity.md I1/V7. */
#include "PowerSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "Item/PowerRoom.h"
#include "Logger.h"
#include "Person.h"
#include "PluginDefs.h"
#include "YootWorld.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <cmath>
#include <tinyxml2.h>

using namespace OT;

namespace
{
	// T2DLL string table (strings.tsv 100c72dc..100c7440).
	const char * kPowerDown       = "Power down.";
	const char * kExceededNoRoom  = "The building's power supply exceeded its limit. Please immediately add a power room.";
	const char * kAddTransformer  = "Please add a transformer to the power room.";
	const char * kOrRemove        = "Or please remove unnecessary tenants.";
	const char * kEscaped         = "The residents have escaped through the emergency stairs.";
	const char * kShortage        = "Shortage of power.";
	const char * kWarnTransformer = "Please add a transformer to the power room."; // EXE: "atransformer"
	const char * kWarnRoom        = "Please add a power room.";
	const char * kWarnRemove      = "Please remove unnecessary tenants.";
}

PowerSystem::PowerSystem(Game * game)
: GameObject(game)
, active(false)
, lastMinute(-1)
, lastDemand(-1)
{
}

void PowerSystem::configure(const YootEventSpec * boev)
{
	active = boev != 0;
	model = YootPowerModel();
	if (boev) {
		decodeYootBlackout(*boev, model.def);
		model.endMin = boev->endMin;
	}
	powerByPrototype.clear();
	reset();
}

void PowerSystem::reset()
{
	model.reset();
	lastMinute = -1;
	lastDemand = -1; // no warning for the tower as found
}

Item::PowerRoom * PowerSystem::powerRoom() const
{
	Game::ItemSetByString::const_iterator it = game->itemsByType.find("powerroom");
	if (it == game->itemsByType.end() || it->second.empty()) return 0;
	return dynamic_cast<Item::PowerRoom *>(*it->second.begin());
}

int PowerSystem::transformers() const
{
	Item::PowerRoom * room = powerRoom();
	return room ? room->transformers : 0;
}

int PowerSystem::powerFor(const std::string & prototypeId)
{
	std::map<std::string, int>::const_iterator cached = powerByPrototype.find(prototypeId);
	if (cached != powerByPrototype.end()) return cached->second;

	// ToolDef +0xf0 GetConsumptionPower of the first linked plugin that
	// exists in this world (the same plugin choice as the unlock table).
	int power = 0;
	const uint32_t worldMask = game->activeRules.profile().worldMask;
	for (const YootPrototypeLink & link : yootPrototypeLinks()) {
		if (prototypeId != link.prototypeId) continue;
		for (int k = 0; k < 5 && link.plugins[k]; ++k) {
			AbstractPlugin * plugin = app->pluginManager.get(link.plugins[k]);
			if (!plugin || !plugin->ensureLoaded()) continue;
			YootPluginIdentity ident;
			if (worldMask && readYootPluginIdentity(*plugin, ident) &&
			    (ident.attrib & worldMask) == 0)
				continue;
			TemplateDef def;
			if (!plugin->getTemplateDef(def) || def.tool.variations.empty()) continue;
			power = def.tool.variations[0].consumptionPower;
			break;
		}
		break;
	}
	powerByPrototype[prototypeId] = power;
	return power;
}

void PowerSystem::refresh()
{
	model.setTransformers(transformers());
	int demand = 0;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); ++i)
		demand += powerFor((*i)->prototype->id);
	model.setConsumption(demand);
	// AddConsumptionPower warns on every build that leaves less than 50 spare.
	if (lastDemand >= 0 && demand > lastDemand && model.shortage()) warnShortage();
	lastDemand = demand;
}

void PowerSystem::warnShortage()
{
	// AddConsumptionPower 0x1001b8b0: suffix by state of the power room.
	const char * suffix;
	if (model.supply() > model.def.defSupply) suffix = kWarnTransformer;
	else if (app->pluginManager.has("PowerRoom.t2p")) suffix = kWarnRoom;
	else suffix = kWarnRemove;
	game->timeWindow.showMessage(std::string(kShortage) + " " + suffix);
}

void PowerSystem::advance()
{
	if (!active) return;
	const int minute = (int)std::floor(game->time.hour * 60.0);
	if (minute == lastMinute) return;
	lastMinute = minute;
	refresh();
	if (!model.isBlackOut()) return;
	switch (model.minute(minute)) {
	case YootPowerModel::Escape:
		game->eventSystem.people.blackoutEscape();
		game->timeWindow.showMessage(kEscaped);
		for (Game::PersonSet::iterator p = game->people.begin(); p != game->people.end(); ++p)
			(*p)->addStress(YootPowerModel::kEscapeStress);
		LOG(IMPORTANT, "Blackout: residents escaped (+%d stress)", YootPowerModel::kEscapeStress);
		break;
	case YootPowerModel::Ended:
		LOG(IMPORTANT, "Blackout over (supply %d, demand %d)", model.supply(), model.consumption());
		break;
	default:
		break;
	}
}

bool PowerSystem::startCheck()
{
	if (!active) return false;
	refresh();
	if (model.start() != YootPowerModel::Started) return false;
	// T2BlackOut::Start 0x1001b620.
	std::string msg = std::string(kPowerDown) + " " +
		(model.supply() == model.def.defSupply ? kExceededNoRoom : kAddTransformer) + " " + kOrRemove;
	game->timeWindow.showMessage(msg);
	for (Game::PersonSet::iterator p = game->people.begin(); p != game->people.end(); ++p)
		(*p)->addStress(YootPowerModel::kStartStress);
	LOG(IMPORTANT, "Blackout: supply %d < demand %d", model.supply(), model.consumption());
	return true;
}

bool PowerSystem::addTransformer(Item::PowerRoom * room, std::string * reason)
{
	if (!active || !room) return false;
	if (room->transformers >= Item::PowerRoom::kMaxTransformers) {
		if (reason) *reason = "The power room is full.";
		return false;
	}
	const int price = transformerPriceDollars();
	if (game->funds < price) {
		// PowerRoom.t2p FUN_1000c2f0 string 0x10021068.
		if (reason) *reason = "Not enough money! You cannot add a transformer.";
		return false;
	}
	room->transformers++;
	game->transferFunds(-price, "construction", "Transformer added");
	// FUN_1000c4a0: AddSupplyPower(on), then BreakerOn when every
	// installed transformer is switched on (always here: no switches).
	model.setTransformers(room->transformers);
	model.breakerOn();
	return true;
}

void PowerSystem::drawDarkening(sf::RenderTarget & target) const
{
	if (!isBlackOut()) return;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); ++i) {
		const Item::Item * item = *i;
		if (item->isElevator() || item->isStairlike()) continue;
		const int2 px = item->getPositionPixels();
		const sf::Vector2u sz = item->getSizePixels();
		sf::RectangleShape s({static_cast<float>(sz.x), static_cast<float>(sz.y)});
		s.setPosition({static_cast<float>(px.x), static_cast<float>(-(px.y + static_cast<int>(sz.y)))});
		s.setFillColor(sf::Color(0, 0, 24, 150));
		target.draw(s);
	}
}

void PowerSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	if (!active) return;
	// T2BlackOut::Write 0x1001ba50 keeps status, consumption and supply;
	// the latter two are recomputed from the tower here.
	xml.PushAttribute("powerBlackout", (int)model.status());
}

void PowerSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	model.restore(active ? xml.IntAttribute("powerBlackout", 0) : 0);
	lastMinute = -1;
	lastDemand = -1;
}
