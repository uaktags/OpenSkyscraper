/* Copyright (c) 2026 OpenSkyscraper Project */
#include "YootPeopleArt.h"

#include "AbstractPlugin.h"
#include "Game.h"
#include "PeopleSystem.h"
#include "PluginObjmap.h"
#include "YootWorld.h"

#include <map>
#include <memory>

using namespace OT;

namespace
{
	struct WorldSilhouettes
	{
		bool loaded = false;
		std::map<int, int> peopleToSilhouette;     ///< PPDF
		std::map<int, SilhouetteDef> silhouettes;  ///< SLDF by type
		std::map<int, std::unique_ptr<sf::Texture> > textures;  ///< by bitmap id
	};

	WorldSilhouettes & worldSilhouettes(Game * game, const std::string & plugin)
	{
		static std::map<std::string, std::unique_ptr<WorldSilhouettes> > cache;
		std::unique_ptr<WorldSilhouettes> & slot = cache[plugin];
		if (slot) return *slot;
		slot.reset(new WorldSilhouettes);
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		PeopleList people;
		SilhouetteList sils;
		if (p && p->ensureLoaded() && p->getPeopleDefs(people) && p->getSilhouettes(sils))
		{
			slot->loaded = true;
			for (const PeopleDef & d : people) slot->peopleToSilhouette[d.peopleType] = d.silhouetteType;
			// T2PeopleLoader::LoadSilhouette keeps the first of duplicate types.
			for (const SilhouetteDef & s : sils)
				if (!slot->silhouettes.count(s.silhouetteType)) slot->silhouettes[s.silhouetteType] = s;
		}
		return *slot;
	}

	/// People type for OpenSky people that are not pool visitors.
	int peopleTypeFor(const Person * p)
	{
		switch (p->type)
		{
		case Person::kSalesman: return 24;          // Sales Person
		case Person::kWoman1: return 19;            // Working student
		case Person::kChild: return 8;              // Elementary school student
		case Person::kWoman2: return 51;            // Mother
		case Person::kWomanWithChild1:
		case Person::kWomanWithChild2: return 37;   // Shopper with child
		case Person::kHousekeeper: return 19;
		case Person::kSecurity: return 40;          // Business Person
		default: return 25;                         // Office worker
		}
	}
}

YootSilhouetteFrame OT::yootPersonSilhouette(Game * game, const Person * person)
{
	YootSilhouetteFrame f;
	if (!game || !person || !game->yootSkinArt()) return f;
	YootWorld::Id wid;
	if (!parseYootWorldIdentifier(game->activeRules.profile().worldId, wid)) return f;
	const std::string plugin = yootWorldPluginFile(wid);
	WorldSilhouettes & w = worldSilhouettes(game, plugin);
	if (!w.loaded) return f;
	const YootVisitor * v = dynamic_cast<const YootVisitor *>(person);
	const int peopleType = v ? v->peopleType.silhouette : peopleTypeFor(person);
	std::map<int, int>::const_iterator pt = w.peopleToSilhouette.find(peopleType);
	// The VIPs carry their own SLDF id (style 1 = the next sheet).
	const int sil = v && v->silhouette >= 0 ? v->silhouette + v->style : pt != w.peopleToSilhouette.end() ? pt->second : -1;
	std::map<int, SilhouetteDef>::const_iterator s = w.silhouettes.find(sil);
	if (s == w.silhouettes.end()) return f;
	std::unique_ptr<sf::Texture> & tex = w.textures[s->second.bitmapID];
	if (!tex)
	{
		tex.reset(new sf::Texture);
		if (!game->app.bitmaps.loadFromPlugin(plugin, s->second.bitmapID, *tex, sf::Color(255, 255, 255)))
			tex.reset(new sf::Texture);
	}
	if (tex->getSize().x == 0) return f;
	f.texture = tex.get();
	f.width = std::max(1, (int)s->second.width);
	return f;
}

sf::IntRect OT::yootSilhouetteRect(int row)
{
	return sf::IntRect({0, std::max(0, std::min(5, row)) * 36}, {16, 36});
}

const YootTenantPeopleArt & OT::yootTenantPeopleArt(Game * game, const std::string & plugin)
{
	static std::map<std::string, std::unique_ptr<YootTenantPeopleArt> > cache;
	std::unique_ptr<YootTenantPeopleArt> & slot = cache[plugin];
	if (slot) return *slot;
	slot.reset(new YootTenantPeopleArt);
	YootTenantPeopleArt & a = *slot;
	AbstractPlugin * p = game->app.pluginManager.get(plugin);
	if (!p || !p->ensureLoaded()) return a;
	a.hasAnime = p->getPeopleAnimeDef(a.anime, 1);
	const WindowsPEExecutable::Resource * om = p->getResource("OBJMAP", 1100);
	std::vector<ObjmapBlock> blocks;
	if (om && parsePluginObjmap(om->data, om->length, blocks))
		for (const ObjmapBlock & b : blocks)
		{
			if (b.name != "SHIL" || b.pattern < 0) continue;
			if ((int)a.shil.size() <= b.pattern) a.shil.resize(b.pattern + 1);
			a.shil[b.pattern] = sf::IntRect({b.rect.left, b.rect.top}, {b.rect.width(), b.rect.height()});
		}
	a.loaded = !a.shil.empty() && game->app.bitmaps.loadFromPlugin(plugin, 1100, a.texture, sf::Color(255, 255, 255));
	return a;
}
