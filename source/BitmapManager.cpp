/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "Application.h"
#include "BitmapManager.h"
#include "DataManager.h"
#include "PluginBitmapLoader.h"
#include "PluginManager.h"

#include <SFML/Graphics/Image.hpp>

#include <cstring>

using namespace OT;

static sf::Vector2u fallbackBitmapSize(Path name)
{
	const std::string path = name.str();
	if (path.find("simtower/sky") == 0 || path.find("simtower\\sky") == 0)
		return {1920, 360};
	if (path.find("simtower/ui/time/bg") == 0 || path.find("simtower\\ui\\time\\bg") == 0)
		return {431, 41};
	if (path.find("simtower/ui/time/rating") == 0 || path.find("simtower\\ui\\time\\rating") == 0)
		return {108, 22};
	if (path.find("simtower/ui/toolbox/tools") == 0 || path.find("simtower\\ui\\toolbox\\tools") == 0)
		return {64, 21};
	if (path.find("simtower/ui/toolbox/items") == 0 || path.find("simtower\\ui\\toolbox\\items") == 0)
		return {800, 32};
	if (path.find("simtower/ui/toolbox/speed") == 0 || path.find("simtower\\ui\\toolbox\\speed") == 0)
		return {92, 48};
	return {2048, 512};
}

static bool loadFallbackBitmap(Path name, sf::Texture &dst)
{
	const sf::Vector2u size = fallbackBitmapSize(name);
	const std::string path = name.str();
	if (path.find("simtower/ui/toolbox/speed") == 0 || path.find("simtower\\ui\\toolbox\\speed") == 0)
	{
		sf::Image image(size, sf::Color::Transparent);
		const sf::Color face[2] = {sf::Color(214, 214, 214), sf::Color(68, 68, 68)};
		const sf::Color light[2] = {sf::Color::White, sf::Color(112, 112, 112)};
		const sf::Color shadow[2] = {sf::Color(84, 84, 84), sf::Color(18, 18, 18)};
		const sf::Color glyph[2] = {sf::Color(20, 20, 20), sf::Color::White};

		auto fillRect = [&](unsigned int x, unsigned int y, unsigned int w, unsigned int h, sf::Color color) {
			for (unsigned int py = y; py < y + h; py++)
				for (unsigned int px = x; px < x + w; px++)
					image.setPixel({px, py}, color);
		};
		auto fillTriangle = [&](int ax, int ay, int bx, int by, int cx, int cy, sf::Color color) {
			const int minX = std::min(ax, std::min(bx, cx));
			const int maxX = std::max(ax, std::max(bx, cx));
			const int minY = std::min(ay, std::min(by, cy));
			const int maxY = std::max(ay, std::max(by, cy));
			const int area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					const int w0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
					const int w1 = (cx - bx) * (y - by) - (cy - by) * (x - bx);
					const int w2 = (ax - cx) * (y - cy) - (ay - cy) * (x - cx);
					if ((area >= 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
					    (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
						image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)}, color);
				}
			}
		};
		auto drawPlay = [&](unsigned int ox, unsigned int oy, unsigned int count, sf::Color color) {
			for (unsigned int i = 0; i < count; i++)
			{
				const int x = static_cast<int>(ox + 5 + i * 5);
				const int y = static_cast<int>(oy + 5);
				fillTriangle(x, y, x, y + 14, x + 8, y + 7, color);
			}
		};

		for (unsigned int row = 0; row < 2; row++)
		{
			for (unsigned int index = 0; index < 4; index++)
			{
				const unsigned int x = index * 23;
				const unsigned int y = row * 24;
				fillRect(x, y, 23, 24, face[row]);
				fillRect(x, y, 23, 1, light[row]);
				fillRect(x, y, 1, 24, light[row]);
				fillRect(x, y + 23, 23, 1, shadow[row]);
				fillRect(x + 22, y, 1, 24, shadow[row]);

				if (index == 0)
				{
					fillRect(x + 6, y + 5, 4, 14, glyph[row]);
					fillRect(x + 13, y + 5, 4, 14, glyph[row]);
				}
				else
				{
					drawPlay(x, y, index, glyph[row]);
				}
			}
		}
		return dst.loadFromImage(image);
	}
	sf::Image image(size, sf::Color(70, 76, 84));
	unsigned int tint = 0;
	for (std::string::const_iterator i = path.begin(); i != path.end(); i++)
		tint = tint * 33 + static_cast<unsigned char>(*i);
	const sf::Color a(50 + tint % 90, 70 + (tint / 7) % 90, 90 + (tint / 17) % 90);
	const sf::Color b(120 + (tint / 3) % 80, 110 + (tint / 11) % 80, 100 + (tint / 19) % 80);
	for (unsigned int y = 0; y < size.y; y++)
	{
		for (unsigned int x = 0; x < size.x; x++)
		{
			const bool stripe = ((x / 16 + y / 16) % 2) == 0;
			image.setPixel({x, y}, stripe ? a : b);
		}
	}
	return dst.loadFromImage(image);
}

namespace
{
	/// Maps a logical item id to the OpenTower folder-plugin that provides
	/// its art. The folder plugin name is the edition/plugin path the
	/// PluginManager catalogs (see PluginManager::addPluginDirectory).
	struct EditionMapEntry { const char * logical; const char * plugin; };
	const EditionMapEntry kEditionMap[] = {
		{"office",        "OpenTower/Office"},
		{"condo",         "OpenTower/Condo"},
		{"hotel_single",  "OpenTower/Hotel-Single"},
		{"hotel_double",  "OpenTower/Hotel-Double"},
		{"hotel_suite",   "OpenTower/Hotel-Suite"},
		{"floor",         "OpenTower/Floor"},
		{"stairs",        "OpenTower/Stairs"},
		{"escalator",     "OpenTower/Escalator"},
		{"parking",       "OpenTower/Parking"},
		{"parking_ramp",  "OpenTower/Parking-Ramp"},
		{"fastfood",      "OpenTower/FastFood"},
		{"restaurant",    "OpenTower/Restaurant"},
		{"security",      "OpenTower/Security"},
		{"medicalcenter", "OpenTower/MedicalCenter"},
		{"recycling",     "OpenTower/Recycling"},
		{"partyhall",     "OpenTower/PartyHall"},
		{"housekeeping",  "OpenTower/Housekeeping"},
		{"shops",         "OpenTower/Shops"},
		{"shop",          "OpenTower/Shops"},
		{"elevator_standard", "OpenTower/Elevator-Standard"},
		{"elevator_service",  "OpenTower/Elevator-Service"},
		{"elevator_express",  "OpenTower/Elevator-Express"},
		{"elevator_narrow",   "OpenTower/Elevator-Shaft-Narrow"},
		{"elevator_wide",     "OpenTower/Elevator-Shaft-Wide"},
		{"lobby_normal",  "OpenTower/Lobby-Normal"},
		{"lobby_sky",     "OpenTower/Lobby-Sky"},
		{"lobby_high",    "OpenTower/Lobby-High"},
	};
	const char * editionPluginFor(const char * logical)
	{
		for (const EditionMapEntry & e : kEditionMap)
			if (std::strcmp(e.logical, logical) == 0)
				return e.plugin;
		return nullptr;
	}
}

bool BitmapManager::editionProvides(const char * logicalName) const
{
	const char * pluginName = editionPluginFor(logicalName);
	return pluginName && app->pluginManager.has(pluginName);
}

bool BitmapManager::editionProvidesKey(const Path & name) const
{
	std::string key = name.str();
	if (key.find(internal_path) == 0)
		key = key.substr(internal_path.size());
	return editionProvides(key.c_str());
}

bool BitmapManager::load(Path name, sf::Texture & dst)
{
	// Yoot skin drop-ins: data/bitmaps/yoottower/<stripped-name> wins when
	// present (future drop-in directory; absent today so this falls
	// through to the normal path list).
	DataManager::Paths paths;
	if (m_skin == Skin::Yoot) {
		std::string key = name.str();
		if (key.find(internal_path) == 0)
			key = key.substr(internal_path.size());
		DataManager::Paths yootPaths = app->data.paths(Path("bitmaps/yoottower") + key);
		for (const Path & p : yootPaths)
			paths.push_back(p);
	}
	// Fetch the possible locations for this bitmap. Dumped SimTower resources
	// live without the leading "simtower/" prefix and with a .png extension.
	DataManager::Paths mainPaths = app->data.paths(Path("bitmaps") + name);
	paths.insert(paths.end(), mainPaths.begin(), mainPaths.end());
	if (name.str().find(internal_path) == 0) {
		DataManager::Paths strippedPaths = app->data.paths(Path("bitmaps") + name.str().substr(internal_path.size()));
		paths.insert(paths.end(), strippedPaths.begin(), strippedPaths.end());
	}
	// Top-level data assets (e.g. splashscreen.png shipped alongside the
	// data/ dir rather than under data/bitmaps/) are searched next, so a
	// request for "splashscreen" finds data/splashscreen.png.
	{
		DataManager::Paths topLevel = app->data.paths(name);
		paths.insert(paths.end(), topLevel.begin(), topLevel.end());
	}
	paths.push_back(name);

	// Try to load the bitmap.
	bool success = false;
	for (int i = 0; i < paths.size() && !success; i++) {
		success = dst.loadFromFile(paths[i].c_str());
		// Only append .png when the filename leaf has no extension. Checking
		// the whole path is wrong: parent dirs like "../data" contain a dot.
		if (!success && paths[i].name().find('.') == std::string::npos) {
			success = dst.loadFromFile((paths[i].str() + ".png").c_str());
		}
	}

	//Return success.
	if (success) {
		LOG(DEBUG,   "loaded bitmap '%s'", name.c_str());
	} else {
		success = loadFallbackBitmap(name, dst);
		usedFallback = true;
		LOG(WARNING, "using fallback bitmap '%s'", name.c_str());
	}
	return success;
}

const sf::Texture * BitmapManager::hiresFor(const sf::Texture * base, unsigned & scale)
{
	const unsigned kScale = 4;
	if (!base || base->getSize().x == 0)
		return nullptr;
	Resources & resources = getResources();
	auto it = hires.find(base);
	if (it != hires.end()) {
		Resources::iterator r = resources.find(it->second.key);
		if (r != resources.end() && &r->second == base && it->second.baseSize == base->getSize()) {
			scale = kScale;
			return it->second.texture.get();
		}
		hires.erase(it); // stale: texture reloaded or cache cleared
	}
	// Reverse lookup of the cache key (once per texture).
	const Path * key = nullptr;
	for (Resources::iterator r = resources.begin(); r != resources.end(); ++r)
		if (&r->second == base) { key = &r->first; break; }
	if (!key)
		return nullptr;

	Hires entry{*key, base->getSize(), nullptr};
	std::string stripped = key->str();
	if (stripped.find(internal_path) == 0)
		stripped = stripped.substr(internal_path.size());
	for (const Path & p : app->data.paths(Path("bitmaps@4x") + stripped)) {
		auto tex = std::make_unique<sf::Texture>();
		bool ok = tex->loadFromFile(p.c_str());
		if (!ok && p.name().find('.') == std::string::npos)
			ok = tex->loadFromFile((p.str() + ".png").c_str());
		if (ok && tex->getSize() == base->getSize() * kScale) {
			tex->setSmooth(true);          // minified on screen: filter...
			(void)tex->generateMipmap();   // ...with mipmaps for clean detail
			entry.texture = std::move(tex);
			LOG(DEBUG, "loaded 4x twin for '%s'", key->c_str());
			break;
		}
	}
	const sf::Texture * out = entry.texture.get();
	hires.emplace(base, std::move(entry));
	if (out)
		scale = kScale;
	return out;
}

bool BitmapManager::loadFromPlugin(const std::string & pluginName, int resourceId,
                                   sf::Texture & dst,
                                   const sf::Color & maskColor)
{
	AbstractPlugin * plugin = app->pluginManager.get(pluginName);
	if (!plugin) {
		LOG(WARNING, "BitmapManager: plugin '%s' not cataloged", pluginName.c_str());
		return false;
	}
	if (!PluginBitmapLoader::loadFromPlugin(*plugin, resourceId, dst, maskColor)) {
		LOG(WARNING, "BitmapManager: RT_BITMAP id %d missing in %s",
		    resourceId, pluginName.c_str());
		return false;
	}
	LOG(DEBUG, "loaded plugin bitmap %s/%d", pluginName.c_str(), resourceId);
	return true;
}

sf::Texture & BitmapManager::editionTexture(const char * logicalName,
                                            const char * simtowerKey)
{
	// Skin decides the provider. Cached results keep the item classes'
	// existing texture keys working while the art source switches.
	Resources & resources = getResources();
	Resources::iterator existing = resources.find(Path(simtowerKey));
	if (existing != resources.end() && existing->second.getSize().x != 0 &&
	    existing->second.getSize().y != 0)
	{
		return existing->second;  // cached plugin/EXE/file result
	}
	sf::Texture & dst = resources[Path(simtowerKey)];

	// OpenTower plugin art only wins under the OpenSky skin; the Classic
	// look is canonical EXE art end-to-end, and Yoot defers to t2p art.
	if (m_skin == Skin::OpenSky) {
		const char * pluginName = editionPluginFor(logicalName);
		if (pluginName && app->pluginManager.has(pluginName) &&
		    loadFromPlugin(pluginName, 1000, dst))
		{
			return dst;
		}
	}

	// YootTower t2p art: the Condo.t2p plugin provides the empty-room
	// sheet (RT_BITMAP 1000, dark-magenta keyed) for the condo logical
	// name (same route as YootCondo.cpp).
	if (m_skin == Skin::Yoot && std::strcmp(logicalName, "condo") == 0 &&
	    loadFromPlugin("Condo.t2p", 1000, dst, sf::Color(105, 33, 104)))
	{
		return dst;
	}

	load(simtowerKey, dst);
	return dst;
}

bool BitmapManager::hasFirstPartyArt(const Path & name) const
{
	// True when first-party replacement art for this texture key ships under
	// data/bitmaps (same stripped-path convention load() uses). SimTowerLoader
	// consults this before installing an extracted EXE sheet so the file
	// edition always wins.
	std::string key = name.str();
	if (key.find(internal_path) == 0)
		key = key.substr(internal_path.size());
	return app->data.exists(Path("bitmaps") + (key + ".png"));
}
