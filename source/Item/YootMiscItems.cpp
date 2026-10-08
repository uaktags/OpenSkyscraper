/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../YootCatalog.h"
#include "../YootItemArt.h"
#include "../YootWorldArt.h"
#include "YootMiscItems.h"

#include <algorithm>
#include <map>

using namespace OT;
using namespace OT::Item;

// ----------------------------------------------------------------- school

void School::init()
{
	YootVenue::init();
	model = Yoot::SchoolModel();
	model.capacity = capacity() > 0 ? capacity() : 60;
}

void School::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootVenue::encodeXML(xml);
	xml.PushAttribute("schoolStatus", model.status);
}

void School::decodeXML(tinyxml2::XMLElement & xml)
{
	YootVenue::decodeXML(xml);
	model = Yoot::SchoolModel();
	model.capacity = capacity() > 0 ? capacity() : 60;
	// Students are transient: a reloaded school past hiring waits closed.
	const int s = xml.IntAttribute("schoolStatus", Yoot::SchoolModel::Built);
	model.status = s <= Yoot::SchoolModel::Hiring ? s : Yoot::SchoolModel::Closed;
	model.pattern = 0;
	setPattern(0);
}

// ------------------------------------------------------------- sky bridge

void SkyBridge::init()
{
	Item::init();
	static std::map<int, sf::Texture> cache;
	// BuildFinish FUN_1000c3f0 sets pattern 3; OBJMAP 1000 View pattern
	// 100 + n uses RT_BITMAP 1000 + n. Fall back to 1000.
	for (int id : {1003, 1000})
	{
		sf::Texture & t = cache[id];
		if (!t.getSize().x) game->app.bitmaps.loadFromPlugin("SkyBrdge.t2p", id, t);
		if (t.getSize().x) { t.setRepeated(true); texture = &t; break; }
	}
}

void SkyBridge::render(sf::RenderTarget & target) const
{
	const float left = position.x * 8.0f;
	const float top = -(position.y + 1) * 36.0f;
	const float width = size.x * 8.0f;
	// Skin::Yoot: the View loop of RT_BITMAP 1000 (64x24, windows keyed so
	// the sky shows through; 1001..1003 are the Outside view facades) over
	// the world's roof and floor strips, like a floor row.
	const YootWorldArt * yoot = game->yootSkinArt();
	const YootPluginView * view = yoot ? yootPluginView(game->app, "SkyBrdge.t2p", kYootWindowKey) : nullptr;
	if (view && !view->bands.empty() && yoot->hasSlab())
	{
		const sf::Color tint = game->lighting.compose(sf::Color::White);
		yoot->drawRoof(target, left, left + width, top, tint);
		yootDrawLoop(target, *view, view->band(3), left, top + 8.0f, width, tint);
		yoot->drawFloorStrip(target, left, left + width, top + 32.0f, tint);
		game->drawnSprites++;
		return;
	}
	if (texture)
	{
		const int h = std::min(24, (int)texture->getSize().y);
		sf::Sprite s(*texture);
		s.setTextureRect(sf::IntRect({0, 0}, {(int)width, h}));
		s.setPosition({left, top + 36.0f - 4.0f - h});
		const sf::Color tint = game->lighting.tint();
		if (tint != sf::Color(255, 255, 255, 255)) s.setColor(game->lighting.compose(sf::Color::White));
		target.draw(s);
	}
	else
	{
		sf::RectangleShape deck({width, 6.0f});
		deck.setPosition({left, top + 26.0f});
		deck.setFillColor(sf::Color(150, 150, 160));
		target.draw(deck);
	}
	game->drawnSprites++;
}

void SkyBridge::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("width", size.x);
}

void SkyBridge::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	size.x = std::max(1, xml.IntAttribute("width", prototype->size.x));
}
