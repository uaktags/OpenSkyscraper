#include "../Game.h"
#include "Parking.h"

using namespace OT;
using namespace OT::Item;

// Each horizontal tile of parking provides this many car spaces. Tunable;
// the original SimTower roughly follows "one car per cell" with the ramp
// cells consumed by access, but for MVP we keep this simple.
static const int kSpacesPerTile = 2;

// Visual size of each drawn car rectangle, in world pixels. The sprite
// sheet only ships ramp + space frames, so we approximate a parked car
// with a small dark rectangle until a dedicated car bitmap lands.
static const float kCarW = 5.f;
static const float kCarH = 11.f;

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

	setupGate();
	updateSprite();
}

void Parking::setupGate()
{
	// SetupAllGate() equivalent: in the original this links the parking
	// facility to the road/gate tile so cars know where to spawn from.
	// OpenSky has no road item yet, so we just leave a wiring point
	// here. When a road tile lands, this is where the connection should
	// be made (e.g. register this Parking with the road item's arrival
	// queue).
	LOG(DEBUG, "Parking %p setupGate: no road item yet, skipping", this);
}

int Parking::totalSpaces() const
{
	return size.x * kSpacesPerTile;
}

bool Parking::assignSpace()
{
	if (!hasSpace()) return false;
	++used;
	spriteNeedsUpdate = true;
	return true;
}

void Parking::freeSpace()
{
	if (used > 0) {
		--used;
		spriteNeedsUpdate = true;
	}
}

void Parking::advance(double dt)
{
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
	sprite.setTextureRect(sf::IntRect({0, 0}, {size.x * 8, 24}));
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(position.y * -36)});
}

void Parking::render(sf::RenderTarget & target) const
{
	// Draw the base parking-space sprite (Item::render applies the
	// global Lighting tint per sprite and may also draw the no-route
	// indicator on top of the footprint). For under-construction sites
	// Item::render draws the placeholder overlay and returns early; we
	// skip the car overlay in that case so it doesn't draw on top of
	// the hatched rectangle.
	Item::render(target);
	if (underConstruction || used <= 0) return;
	// TODO(Phase 2.2 art): replace this rectangle with a proper
	// "simtower/parking/car" frame once the bitmap is shipped.
	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255));
	const float baseX = static_cast<float>(position.x * 8);
	const float baseY = static_cast<float>(position.y * -36);

	for (int i = 0; i < used; ++i)
	{
		int tile = i / kSpacesPerTile;
		int slot = i % kSpacesPerTile;
		// Each tile is 8 px wide; two cars per tile get ~5 px width
		// with 1 px padding each side and 1 px gap between them.
		float carX = baseX + tile * 8.f + 1.f + slot * (kCarW + 1.f);
		// Parked cars sit in the lower-middle of the 24 px sprite so
		// the space markings stay partially visible above them.
		float carY = baseY - kCarH - 4.f;

		sf::RectangleShape car({kCarW, kCarH});
		car.setPosition({carX, carY});
		sf::Color body(50, 70, 130, 235);
		if (tinted) body = game->lighting.compose(body);
		car.setFillColor(body);
		car.setOutlineColor(sf::Color(20, 25, 45, 235));
		car.setOutlineThickness(1.f);
		target.draw(car);
		game->drawnSprites++;
	}
}
