/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../PluginObjmap.h"
#include "../YootCatalog.h"
#include "../YootItemArt.h"
#include "YootFacilityItems.h"

#include <algorithm>
#include <map>
#include <memory>

using namespace OT;
using namespace OT::Item;

void YootFacility::init()
{
	Item::init();
	const std::string plugin = prototype->yoot && !prototype->yoot->plugin.empty()
		? prototype->yoot->plugin : std::string(defaultPlugin());
	if (const YootPluginView * v = yootPluginView(game->app, plugin)) {
		view = &v->texture;
		bands = &v->bands;
		sprite.SetImage(v->texture);
		sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36 - interiorLift())});
		addSprite(&sprite);
	}
	updateSprite();
}

int YootFacility::interiorLift() const
{
	// Skin::Yoot: the art's bottom edge is the top of the 4 px floor strip.
	return game->skin == Skin::Yoot ? kYootInteriorLift : 0;
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
