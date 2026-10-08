/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootItemArt.h"

#include "Application.h"
#include "PluginObjmap.h"

#include <algorithm>
#include <map>
#include <memory>
#include <tuple>

namespace OT
{
	sf::IntRect YootPluginView::band(int index) const
	{
		if (bands.empty())
			return sf::IntRect({0, 0}, {static_cast<int>(texture.getSize().x), static_cast<int>(texture.getSize().y)});
		return bands[std::max(0, std::min((int)bands.size() - 1, index))];
	}

	const YootPluginView * yootPluginView(Application & app, const std::string & plugin,
		const sf::Color & mask, int bitmapId, int objmapId, const std::string & part)
	{
		typedef std::tuple<std::string, uint32_t, int, int, std::string> Key;
		static std::map<Key, std::unique_ptr<YootPluginView> > cache;
		static std::map<Key, bool> missing;
		const Key key(plugin, mask.toInteger(), bitmapId, objmapId, part);
		auto hit = cache.find(key);
		if (hit != cache.end()) return hit->second.get();
		if (missing.count(key)) return nullptr;

		std::unique_ptr<YootPluginView> v(new YootPluginView);
		if (!app.pluginManager.has(plugin) || !app.bitmaps.loadFromPlugin(plugin, bitmapId, v->texture, mask))
		{
			missing[key] = true;
			return nullptr;
		}
		AbstractPlugin * p = app.pluginManager.get(plugin);
		const WindowsPEExecutable::Resource * om = (p && p->ensureLoaded()) ? p->getResource("OBJMAP", objmapId) : nullptr;
		std::vector<ObjmapBlock> blocks;
		if (om && parsePluginObjmap(om->data, om->length, blocks))
		{
			std::vector<std::pair<int, sf::IntRect> > parts;
			for (const ObjmapBlock & b : blocks)
				if (b.name == part && b.rect.width() > 0 && b.rect.height() > 0)
					parts.push_back(std::make_pair(b.pattern, sf::IntRect({b.rect.left, b.rect.top}, {b.rect.width(), b.rect.height()})));
			std::stable_sort(parts.begin(), parts.end(),
				[](const std::pair<int, sf::IntRect> & a, const std::pair<int, sf::IntRect> & b) { return a.first < b.first; });
			for (const auto & q : parts)
			{
				v->patterns.push_back(q.first);
				v->bands.push_back(q.second);
			}
		}
		YootPluginView * out = v.get();
		cache[key] = std::move(v);
		return out;
	}
}
