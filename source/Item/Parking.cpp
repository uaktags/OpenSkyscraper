#include "../Game.h"
#include "Parking.h"

using namespace OT;
using namespace OT::Item;

// Each horizontal tile of parking provides this many car spaces. Tunable;
// the original SimTower roughly follows "one car per cell" with the ramp
// cells consumed by access, but for MVP we keep this simple.
static const int kSpacesPerTile = 2;

Parking::~Parking() {}

void Parking::init()
{
	Item::init();

	used = 0;

	sprite.setTexture(App->bitmaps["simtower/parking/space"]);
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(position.y * -36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;

	updateSprite();
}

int Parking::totalSpaces() const
{
	return size.x * kSpacesPerTile;
}

bool Parking::assignSpace()
{
	if (!hasSpace()) return false;
	++used;
	return true;
}

void Parking::freeSpace()
{
	if (used > 0) --used;
}

void Parking::advance(double dt)
{
	// Parking is largely passive - the JudgeSystem reads totalSpaces() once
	// per day. Idle animation / car sprites go here later.
	if (spriteNeedsUpdate)
		updateSprite();
}

void Parking::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("used", used);
}

void Parking::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	used = xml.IntAttribute("used");
	updateSprite();
}

void Parking::updateSprite()
{
	spriteNeedsUpdate = false;
	// TODO(Phase 2.2): per-cell occupancy sprites. For now render the plain
	// space bitmap sized to the item.
	sprite.setTextureRect(sf::IntRect({0, 0}, {size.x * 8, 24}));
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(position.y * -36)});
}
