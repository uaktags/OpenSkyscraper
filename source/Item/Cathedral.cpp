#include "Cathedral.h"
#include "../Game.h"
#include "../Math/Rand.h"
#include "../Time.h"

using OT::Item::Cathedral;
using OT::Path;
using OT::Math::randd;
using OT::Time;

Cathedral::Cathedral(Game * game, AbstractPrototype * prototype) : Item(game, prototype)
{
}

Cathedral::~Cathedral()
{
}

void Cathedral::init()
{
	Item::init();

	variant = rand() % 3;
	occupied = false;
	lit = false;
	rent = 5000;
	rentDeposit = rent;

	// For dummy, just set a basic sprite if available
	// sprite.setTexture(App->bitmaps["simtower/cathedral"]); // Would need actual bitmap
	// sprite.setOrigin(0, 24);
	// addSprite(&sprite);
	spriteNeedsUpdate = false;

	updateSprite();
}

void Cathedral::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("rent", rent);
	xml.PushAttribute("rentDeposit", rentDeposit);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("occupied", occupied);
	xml.PushAttribute("lit", lit);
}

void Cathedral::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	rent        = xml.IntAttribute("rent");
	rentDeposit = xml.IntAttribute("rentDeposit");
	variant     = xml.IntAttribute("variant");
	occupied    = xml.BoolAttribute("occupied");
	lit         = xml.BoolAttribute("lit");
	updateSprite();
}

void Cathedral::updateSprite()
{
	spriteNeedsUpdate = false;
	// Dummy implementation - no actual sprite update needed
}

void Cathedral::advance(double dt)
{
	// Dummy implementation - minimal functionality
	// Turn on lights during day
	bool shouldBeLit = (game->time.day != 2 && game->time.hour >= 7 && game->time.hour < 17);
	if (lit != shouldBeLit) {
		lit = shouldBeLit;
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate) updateSprite();
}

