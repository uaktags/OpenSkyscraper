#include "ParkingRamp.h"
#include "Parking.h"
#include "../Game.h"
using namespace OT;
using namespace OT::Item;
void ParkingRamp::init()
{
 Item::init();
 // Folder plugin has three 128x24 frames (384x24 total).
 sprite.setTexture(App->bitmaps.editionTexture("parking_ramp", "simtower/parking/ramp"));
 sprite.setOrigin({0.f,24.f});
 sprite.setPosition({float(position.x*8),float(-position.y*36)});
 sprite.setTextureRect(sf::IntRect({0,0},{128,24}));
 addSprite(&sprite);
}
void ParkingRamp::advance(double)
{
 int frame=game->time.hour>=17 || game->time.hour<7 ? 2 : 0;
 sprite.setTextureRect(sf::IntRect({frame*128,0},{128,24}));
}
