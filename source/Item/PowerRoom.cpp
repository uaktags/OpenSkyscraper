/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "PowerRoom.h"

using namespace OT;
using namespace OT::Item;

sf::Texture PowerRoom::viewTexture;
bool PowerRoom::viewLoaded = false;
const int PowerRoom::kMaxTransformers;

void PowerRoom::init()
{
	Item::init();
	transformers = 1;

	// PowerRoom.t2p RT_BITMAP 1000 is the 184x60 room view (OBJMAP 1000
	// "View", pattern 200), bottom-aligned like the other two-floor rooms.
	if (!viewLoaded)
		viewLoaded = game->app.bitmaps.loadFromPlugin("PowerRoom.t2p", 1000, viewTexture);
	if (viewLoaded) {
		sprite.SetImage(viewTexture);
		sprite.setOrigin({0.f, 60.f});
		sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
		addSprite(&sprite);
	}
}

void PowerRoom::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("transformers", transformers);
}

void PowerRoom::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	transformers = xml.IntAttribute("transformers", 1);
	if (transformers < 1) transformers = 1;
	if (transformers > kMaxTransformers) transformers = kMaxTransformers;
}
