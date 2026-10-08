/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootCatalog.h"

#include "AbstractPlugin.h"
#include "PluginManager.h"
#include "YootWorld.h"

#include <algorithm>

namespace OT
{
	int YootItemStats::inMoneyFor(int selection) const
	{
		if (inMoney.empty()) return 0;
		selection = std::max(0, std::min((int)inMoney.size() - 1, selection));
		return inMoney[selection];
	}

	bool yootItemStatsFromTemplate(const TemplateDef & def, int variation, YootItemStats & out)
	{
		if (def.kind == TemplateDef::OuterObj || def.tool.variations.empty())
			return false;
		variation = std::max(0, std::min((int)def.tool.variations.size() - 1, variation));
		const ToolVariation & v = def.tool.variations[variation];

		YootItemStats s;
		s.kind = def.kind;
		s.variation = variation;
		s.name = v.name;
		s.attribute = def.tool.attribute;
		s.category = def.tool.category;
		s.price = v.price * kYootMoneyUnit;
		s.outMoney = v.outMoney * kYootMoneyUnit;
		s.power = v.consumptionPower;
		s.capacity = def.equip.capacity;
		for (size_t i = 0; i < def.equip.inMoney.size(); ++i)
			s.inMoney.push_back(def.equip.inMoney[i] * kYootMoneyUnit);

		if (def.kind == TemplateDef::Tenant)
		{
			s.senseOfNoise = def.tenant.senseOfNoise;
			s.openMinute = def.tenant.openTime;
			s.closeMinute = def.tenant.closeTime;
			s.bindMinutes = def.tenant.bindTime;
			s.maxBuild = def.tenant.maxBuildCount;
		}
		else
		{
			s.mover = true;
			s.maxLength = def.mover.maxLength;
			s.minLength = def.mover.minLength;
			s.maxStops = def.mover.maxStop;
			s.routeScore = def.mover.score;
			s.maxCars = def.mover.maxModule;
			s.carPrice = def.mover.moduleParam2 * kYootMoneyUnit;
			s.speed = def.mover.speed;
		}
		out = s;
		return true;
	}

	bool loadYootItemStats(const AbstractPlugin & plugin, int variation, YootItemStats & out)
	{
		TemplateDef def;
		if (!plugin.getTemplateDef(def)) return false;
		if (!yootItemStatsFromTemplate(def, variation, out)) return false;
		out.plugin = plugin.name();
		return true;
	}

	int yootPrototypeVariation(const std::string & prototypeId)
	{
		if (prototypeId == "lobby") return 1;    // Floor.t2p: Floor, Lobby
		if (prototypeId == "parking") return 1;  // Parking.t2p: lot, gate (OpenSky ramp)
		return 0;
	}

	std::map<std::string, YootItemStats> resolveYootCatalog(const PluginManager & plugins, uint32_t worldMask)
	{
		std::map<std::string, YootItemStats> out;
		for (const YootPrototypeLink & link : yootPrototypeLinks())
		{
			for (int k = 0; k < 5 && link.plugins[k]; ++k)
			{
				AbstractPlugin * p = plugins.get(link.plugins[k]);
				if (!p || !p->ensureLoaded()) continue;
				YootPluginIdentity ident;
				if (!readYootPluginIdentity(*p, ident)) continue;
				// World part of the toolbox filter (FUN_0041ca80); grade is
				// handled by the unlock table.
				if (!yootPluginAvailable(ident, worldMask, ident.gameLevel)) continue;
				YootItemStats s;
				if (!loadYootItemStats(*p, yootPrototypeVariation(link.prototypeId), s)) continue;
				out[link.prototypeId] = s;
				break;
			}
		}
		return out;
	}
}
