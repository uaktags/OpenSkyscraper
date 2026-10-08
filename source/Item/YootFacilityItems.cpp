/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../PluginObjmap.h"
#include "../YootCatalog.h"
#include "YootFacilityItems.h"

#include <algorithm>
#include <map>
#include <memory>

using namespace OT;
using namespace OT::Item;

namespace
{
	struct PluginView
	{
		bool loaded = false;
		sf::Texture texture;
		std::vector<sf::IntRect> bands;
	};

	/// RT_BITMAP 1000 + OBJMAP 1000 "View" rects of one plugin, cached.
	PluginView & pluginView(Game * game, const std::string & plugin)
	{
		static std::map<std::string, std::unique_ptr<PluginView> > cache;
		std::unique_ptr<PluginView> & slot = cache[plugin];
		if (slot) return *slot;
		slot.reset(new PluginView);
		PluginView & v = *slot;
		v.loaded = game->app.bitmaps.loadFromPlugin(plugin, 1000, v.texture);
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		const WindowsPEExecutable::Resource * om = (p && p->ensureLoaded()) ? p->getResource("OBJMAP", 1000) : nullptr;
		std::vector<ObjmapBlock> blocks;
		if (om && parsePluginObjmap(om->data, om->length, blocks)) {
			std::vector<std::pair<int, sf::IntRect> > views;
			for (const ObjmapBlock & b : blocks)
				if (b.name == "View")
					views.push_back(std::make_pair(b.pattern, sf::IntRect({b.rect.left, b.rect.top}, {b.rect.width(), b.rect.height()})));
			std::stable_sort(views.begin(), views.end(),
				[](const std::pair<int, sf::IntRect> & a, const std::pair<int, sf::IntRect> & b) { return a.first < b.first; });
			for (const auto & view : views) v.bands.push_back(view.second);
		}
		return v;
	}
}

void YootFacility::init()
{
	Item::init();
	const std::string plugin = prototype->yoot && !prototype->yoot->plugin.empty()
		? prototype->yoot->plugin : std::string(defaultPlugin());
	PluginView & v = pluginView(game, plugin);
	if (v.loaded) {
		view = &v.texture;
		bands = &v.bands;
		sprite.SetImage(v.texture);
		sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
		addSprite(&sprite);
	}
	updateSprite();
}

int YootFacility::capacity() const
{
	if (prototype->yoot && prototype->yoot->capacity > 0) return prototype->yoot->capacity;
	return 5;
}

int YootFacility::stayMinutes() const
{
	if (prototype->yoot && prototype->yoot->bindMinutes > 0) return prototype->yoot->bindMinutes;
	return 15;
}

void YootFacility::setPattern(int p)
{
	if (p == pattern) return;
	pattern = p;
	updateSprite();
}

void YootFacility::updateSprite()
{
	if (!view) return;
	sf::IntRect band({0, 0}, {static_cast<int>(view->getSize().x), static_cast<int>(view->getSize().y)});
	if (bands && !bands->empty())
		band = (*bands)[std::max(0, std::min((int)bands->size() - 1, pattern))];
	sprite.setTextureRect(band);
	sprite.setOrigin({0.f, static_cast<float>(band.size.y)});
}

void YootFacility::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("pattern", pattern);
}

void YootFacility::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	// Occupants are transient (they are not saved as people either).
	leaveAt.clear();
	pattern = std::max(0, xml.IntAttribute("pattern", 0));
	updateSprite();
}

void TrashFacility::init()
{
	model = Yoot::TrashFacilityModel();
	model.capacity = capacity();
	YootFacility::init();
}

void TrashFacility::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootFacility::encodeXML(xml);
	xml.PushAttribute("garbage", model.garbage);
	xml.PushAttribute("trashStatus", model.status);
}

void TrashFacility::decodeXML(tinyxml2::XMLElement & xml)
{
	YootFacility::decodeXML(xml);
	model.capacity = capacity();
	model.garbage = std::max(0, std::min(model.capacity, xml.IntAttribute("garbage", 0)));
	model.status = xml.IntAttribute("trashStatus", Yoot::TrashFacilityModel::kOpen);
	model.pattern = pattern;
}

bool DoctorsOffice::isOpen(int minute) const
{
	int open = 480, close = 1200; // Doctor's.t2p TNDF +0x174 / +0x178
	if (prototype->yoot && prototype->yoot->closeMinute > 0) {
		open = prototype->yoot->openMinute;
		close = prototype->yoot->closeMinute;
	}
	return minute >= open && minute < close;
}
