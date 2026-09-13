#include "../Game.h"
#include "../Math/Rand.h"
#include "Recycling.h"

using namespace OT;
using namespace OT::Item;


Recycling::~Recycling()
{
}

void Recycling::init()
{
    Item::init();
    variant = 0;

    sprite.SetImage(App->bitmaps.editionTexture("recycling", "simtower/recycling"));
    sprite.setOrigin({0.f, 60.f});
    sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
    addSprite(&sprite);
    spriteNeedsUpdate = false;

    updateSprite();
}

void Recycling::advance(double dt)
{
    // Working animation: cycle the sheet's seven frames (a full pass takes
    // about ten in-game minutes).
    int frame = static_cast<int>(game->time.absolute * 1000.0) % 7;
    if (frame != variant)
    {
        variant = frame;
        updateSprite();
    }
}

void Recycling::updateSprite()
{
    spriteNeedsUpdate = false;
    // First-party sheet: seven 200x60 frames of the working animation.
    sprite.setTextureRect(sf::IntRect({variant * 200, 0}, {200, 60}));
}
