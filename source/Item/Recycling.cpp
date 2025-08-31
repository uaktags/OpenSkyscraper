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

    sprite.SetImage(App->bitmaps["simtower/recycling"]);
    sprite.setOrigin(0, 60);
    sprite.setPosition(getPosition().x * 8, -getPosition().y * 36);
    addSprite(&sprite);
    spriteNeedsUpdate = false;

    updateSprite();
}

void Recycling::updateSprite()
{
    spriteNeedsUpdate = false;
    int index = 0;
    sprite.setTextureRect(sf::IntRect(index*200, variant*60, 200, 60));
}