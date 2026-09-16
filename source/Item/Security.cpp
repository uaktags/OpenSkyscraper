#include "../Game.h"
#include "../Math/Rand.h"
#include "Security.h"

using namespace OT;
using namespace OT::Item;


Security::~Security()
{
}

void Security::init()
{
    Item::init();
    variant = 0;

    sprite.SetImage(App->bitmaps.editionTexture("security", "simtower/security"));
    sprite.setOrigin({0.f, 24.f});
    sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
    addSprite(&sprite);
    spriteNeedsUpdate = false;

    updateSprite();
}

void Security::advance(double dt)
{
    // The EXE animates sheet 0x8768 all day by cycling color-table
    // entries (197<->198, 199<->200, 201->202->203; AnimeT). We advance
    // the three pre-extracted frames one step per tick, the same
    // cadence as the recycling cycle.
    int frame = static_cast<int>(game->time.absolute * 1000.0) % 3;
    if (frame != variant)
    {
        variant = frame;
        updateSprite();
    }
}

void Security::updateSprite()
{
    spriteNeedsUpdate = false;
    // Sheet: three 128x24 palette-cycle frames side by side (frames
    // were pinned to 0 before; the EXE cycles them continuously).
    sprite.setTextureRect(sf::IntRect({variant * 128, 0}, {128, 24}));
}

AbstractPrototype * Security::makeSecomPrototype()
{
	AbstractPrototype * p = new Prototype<Security>;
	p->id             = "secom";
	p->name           = "SECOM Center";
	p->price          = 100000;
	p->size           = int2(16, 1);
	// Authentic SIMTOWER toolbox cell 25 (the strip cell is literally
	// labeled "SECOM"); cell 21 is the Security guard icon the web port
	// reused, which made SECOM read as a duplicate Security entry.
	p->icon           = 25;
	p->variant        = 0;
	p->entrance_offset = 0;
	p->exit_offset    = 0;
	return p;
}
