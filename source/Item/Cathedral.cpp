/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Cathedral / "Tower of the Year" endgame monument.
 * Ported from OpenSkyWeb (src/game/items/cathedral.js, ISSUE-010). */
#include "../Game.h"
#include "../Application.h"
#include "Cathedral.h"

using namespace OT;
using namespace OT::Item;

Cathedral::~Cathedral()
{
}

void Cathedral::init()
{
	Item::init();
	ceremonyTriggered = false;

	// The SIMTOWER.EXE resources carry no dedicated cathedral bitmap; the
	// web port composes "simtower/cathedral/main" from the party hall art
	// (resource 0x8b28). Draw one party hall frame anchored bottom-left.
	sprite.SetImage(App->bitmaps.editionTexture("partyhall", "simtower/partyhall"));
	sprite.setTextureRect(sf::IntRect({0, 0}, {192, 60}));
	sprite.setOrigin({0.f, 60.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	addSprite(&sprite);
}

void Cathedral::advance(double dt)
{
	Item::advance(dt);
	if (underConstruction || ceremonyTriggered) return;

	// Victory ceremony (web parity): bells ring, the tower is awarded the
	// TOWER rating, and the authentic award message is shown.
	ceremonyTriggered = true;
	game->playOnce("simtower/bells");
	game->setRating(5); // TOWER rating
	game->timeWindow.showMessage("Your tower has been given a \"Tower of the Year\" award!");
	LOG(IMPORTANT, "Cathedral ceremony complete: TOWER rating reached");
}

void Cathedral::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("ceremony", (int)ceremonyTriggered);
}

void Cathedral::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	ceremonyTriggered = xml.BoolAttribute("ceremony", false);
}
