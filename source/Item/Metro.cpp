#include "../Game.h"
#include "../Math/Rand.h"
#include "Metro.h"

using namespace OT;
using namespace OT::Item;

Metro::~Metro()
{
}

void Metro::init()
{
	Item::init();

	open = true;
	trainPresent = true;

	station.setTexture(App->bitmaps["simtower/metro/station"]);
	station.setOrigin({0.f, 66.f});
	station.setPosition({0.f, 0.f});
	platform.setTexture(App->bitmaps["simtower/metro/station"]);
	platform.setOrigin({0.f, 30.f});
	platform.setPosition({0.f, 0.f});
	addSprite(&station);
	addSprite(&platform);
	spriteNeedsUpdate = true;

	assert(game->metroStation == NULL);
	game->metroStation = this;

	updateSprite();
}

void Metro::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("open", open);
	xml.PushAttribute("trainPresent", trainPresent);
}

void Metro::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open");
	trainPresent = xml.BoolAttribute("trainPresent");
	updateSprite();
}

void Metro::updateSprite()
{
	spriteNeedsUpdate = false;
	int stationIndex = 2, platformIndex = 2;
	if (open)
	{
		stationIndex = 1;
		platformIndex = (trainPresent ? 0 : 1);
	}

	station.setTextureRect(sf::IntRect({stationIndex * 240, 0}, {240, 66}));
	platform.setTextureRect(sf::IntRect({platformIndex * 240, 66}, {240, 30}));
	station.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
	platform.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
}

void Metro::advance(double dt)
{
	// Open
	if (game->time.checkHour(7))
	{
		open = true;
		spriteNeedsUpdate = true;
	}

	// Close
	if (game->time.checkHour(23) && open)
	{
		open = false;
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}
