/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../YootItemArt.h"
#include "PowerRoom.h"

using namespace OT;
using namespace OT::Item;

const int PowerRoom::kMaxTransformers;

void PowerRoom::init()
{
	Item::init();
	transformers = 1;

	// PowerRoom.t2p RT_BITMAP 1000 is the 184x60 room view (OBJMAP 1000
	// "View", pattern 200), bottom-aligned like the other two-floor rooms.
	if (const YootPluginView * v = yootPluginView(game->app, "PowerRoom.t2p")) {
		sprite.SetImage(v->texture);
		sprite.setTextureRect(v->band(0));
		sprite.setOrigin({0.f, static_cast<float>(v->band(0).size.y)});
		// Skin::Yoot: the art sits on the 4 px floor strip.
		const int lift = game->skin == Skin::Yoot ? kYootInteriorLift : 0;
		sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36 - lift)});
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
