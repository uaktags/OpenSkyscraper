/* Copyright © 2022 Vuong Ly */
/* Copyright © 2025 Tim G */
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

    sprite.setTexture(App->bitmaps["simtower/medicalcenter"]);
    sprite.setOrigin(0, 24);
    sprite.setPosition(getPosition().x * 8, -getPosition().y * 36);
    addSprite(&sprite);
    spriteNeedsUpdate = false;

    LOG(INFO, "Bitmap loaded for %s: simtower/medicalcenter", prototype->name.c_str());

    updateSprite();
}

void MedicalCenter::updateSprite()
{
    spriteNeedsUpdate = false;
    int index = 0;
    sprite.setTextureRect(sf::IntRect(index*256, variant*24, 256, 24));
}