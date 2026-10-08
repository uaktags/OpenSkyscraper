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
	namespace
	{
		typedef std::tuple<std::string, uint32_t, int, int, std::string> Key;
		std::map<Key, std::unique_ptr<YootPluginView> > & viewCache()
		{
			static std::map<Key, std::unique_ptr<YootPluginView> > cache;
			return cache;
		}

		struct PaletteState
		{
			std::vector<YootRgb> palette;
			int start = 0, count = 0;
		};
		PaletteState & currentPalette()
		{
			static PaletteState s;
			return s;
		}

		void recolour(YootPluginView & v, const PaletteState & p)
		{
			if (v.indices.empty()) return;
			std::vector<YootRgb> colours;
			if (!p.palette.empty())
				for (int index : v.usedIndices)
					if (index >= p.start && index < p.start + p.count && index < (int)p.palette.size())
						colours.push_back(p.palette[index]);
			if (colours == v.applied) return;
			v.applied = colours;
			std::vector<uint8_t> rgba = v.baseRgba;
			if (!p.palette.empty())
				yootPaintAnimatedIndices(v.indices, p.palette, p.start, p.count, rgba);
			v.texture.update(rgba.data());
		}
	}

	sf::IntRect YootPluginView::band(int index) const
	{
		if (bands.empty())
			return sf::IntRect({0, 0}, {static_cast<int>(texture.getSize().x), static_cast<int>(texture.getSize().y)});
		return bands[std::max(0, std::min((int)bands.size() - 1, index))];
	}

	const YootPluginView * yootPluginView(Application & app, const std::string & plugin,
		const sf::Color & mask, int bitmapId, int objmapId, const std::string & part)
	{
		static std::map<Key, bool> missing;
		std::map<Key, std::unique_ptr<YootPluginView> > & cache = viewCache();
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

		// Keep the palette indices for the tower-palette animation.
		const WindowsPEExecutable::Resource * bmp = p ? p->getResource("2", bitmapId) : nullptr;
		YootIndexedImage img;
		if (bmp && decodeYootIndexedDib(bmp->data, bmp->length, img) &&
			img.width == (int)v->texture.getSize().x && img.height == (int)v->texture.getSize().y)
		{
			const sf::Image base = v->texture.copyToImage();
			v->baseRgba.assign(base.getPixelsPtr(), base.getPixelsPtr() + img.pixels.size() * 4);
			v->indices = img.pixels;
			bool used[256] = {false};
			for (uint8_t i : img.pixels) used[i] = true;
			for (int i = 0; i < 256; ++i)
				if (used[i]) v->usedIndices.push_back(i);
			recolour(*v, currentPalette());
		}

		YootPluginView * out = v.get();
		cache[key] = std::move(v);
		return out;
	}

	void yootRecolourPluginViews(const std::vector<YootRgb> & palette, int start, int count)
	{
		PaletteState & s = currentPalette();
		s.palette = palette;
		s.start = start;
		s.count = count;
		for (auto & entry : viewCache())
			recolour(*entry.second, s);
	}
}
