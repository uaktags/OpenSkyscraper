/* Copyright (c) 2026 OpenSkyscraper Project */
#include "BldImport.h"

#include "BldExport.h"
#include "BldLoader.h"
#include "Game.h"
#include "Logger.h"
#include "tinyxml2.h"

namespace OT
{
	bool importBldSave(Game & game, const std::string & path)
	{
		Bld::BldData save;
		std::string error;
		if (!Bld::loadFile(path, save, error))
		{
			LOG(ERROR, "Could not import YootTower save %s: %s", path.c_str(), error.c_str());
			return false;
		}
		Bld::PrototypeLookup lookup = [&game](const std::string & id, int & w, int & h)
		{
			std::map<std::string, Item::AbstractPrototype *>::const_iterator p = game.itemFactory.prototypesById.find(id);
			if (p == game.itemFactory.prototypesById.end() || !p->second) return false;
			w = p->second->size.x;
			h = p->second->size.y;
			return true;
		};
		std::string xmlText;
		Bld::ImportReport report;
		if (!Bld::toTowerXml(save, lookup, xmlText, report, error, Bld::subPluginLookup(game.app.pluginManager), Bld::hallEventLookup(game.app.pluginManager)))
		{
			LOG(ERROR, "Could not import YootTower save %s: %s", path.c_str(), error.c_str());
			return false;
		}
		tinyxml2::XMLDocument xml;
		if (xml.Parse(xmlText.c_str(), xmlText.size()) != tinyxml2::XML_SUCCESS)
		{
			LOG(ERROR, "BLD import produced invalid XML for %s", path.c_str());
			return false;
		}
		// The XML names ruleset yoottower and the save's world; decodeXML
		// selects both unless --ruleset/--world override them.
		game.decodeXML(xml);
		game.reloadGUI();

		std::string nativeName = path;
		if (nativeName.size() >= 4)
		{
			std::string ext = nativeName.substr(nativeName.size() - 4);
			for (char & c : ext) c = (char)tolower((unsigned char)c);
			if (ext == ".bld") nativeName = nativeName.substr(0, nativeName.size() - 4);
		}
		game.saveFilename = nativeName + ".tower";
		game.isDirty = false;

		int imported = 0, dropped = 0;
		for (const auto & i : report.imported)
		{
			LOG(INFO, "BLD import: %d x %s", i.second, i.first.c_str());
			imported += i.second;
		}
		for (const auto & d : report.dropped)
		{
			LOG(WARNING, "BLD import: dropped %d x %s", d.second, d.first.c_str());
			dropped += d.second;
		}
		for (const std::string & n : report.notes)
			LOG(INFO, "BLD import: %s", n.c_str());
		std::map<std::string, int> inGame;
		for (Item::Item * item : game.items) inGame[item->prototype->id]++;
		std::string census;
		for (const auto & i : inGame)
			census += (census.empty() ? "" : ", ") + std::to_string(i.second) + " " + i.first;
		LOG(INFO, "BLD import: game now holds %s", census.c_str());
		LOG(IMPORTANT, "YootTower save imported from %s: %s, %d star(s), $%lld, year %d month %d %02d:%02d; %d item(s) placed, %d record(s) dropped; %zu items in game",
			path.c_str(), report.worldId.c_str(), (int)save.level, save.fundsDollars(), (int)save.year, (int)save.month0 + 1,
			save.minutes / 60, save.minutes % 60, imported, dropped, game.items.size());
		return true;
	}
}
