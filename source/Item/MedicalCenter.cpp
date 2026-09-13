#include "../Game.h"
#include "../Math/Rand.h"
#include "MedicalCenter.h"

using namespace OT;
using namespace OT::Item;


MedicalCenter::~MedicalCenter()
{
}

void MedicalCenter::init()
{
    Item::init();
    variant = 0;

    sprite.SetImage(App->bitmaps.editionTexture("medicalcenter", "simtower/medicalcenter"));
    sprite.setOrigin({0.f, 24.f});
    sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
    addSprite(&sprite);
    spriteNeedsUpdate = false;

    updateSprite();
}

void MedicalCenter::advance(double dt)
{
    // Sheet frame 0 is the day interior, frame 1 the night interior; follow
    // the tower lighting hours (same rule as Condo::updateLighting).
    int frame = ((game->time.getHour() < 7.0) || (game->time.getHour() > 22.0)) ? 1 : 0;
    if (frame != variant)
    {
        variant = frame;
        updateSprite();
    }
}

void MedicalCenter::updateSprite()
{
    spriteNeedsUpdate = false;
    // First-party sheet: two 312x24 frames (day, night).
    sprite.setTextureRect(sf::IntRect({variant * 312, 0}, {312, 24}));
}
