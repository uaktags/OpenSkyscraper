#include "../Game.h"
#include "../Math/Rand.h"
#include "Recycling.h"
#include <cmath>
#include <vector>

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
    lower.setTexture(App->bitmaps.editionTexture("recycling", "simtower/recycling"));
    lower.setOrigin({0.f,24.f});
    lower.setPosition({float(position.x*8),float(-position.y*36)});
    addSprite(&lower);
    spriteNeedsUpdate = false;

    updateSprite();
}

int Recycling::required(int ratio)
{
 return ratio<500?1:ratio<1000?2:ratio<1500?3:ratio<2000?4:ratio<2500?5:6;
}
void Recycling::advanceAll(Game *game)
{
 if (game->rating<2) return;
 std::vector<Recycling*> centers;
 for (auto *item : game->itemsByType["recycling"]) if (auto *center=dynamic_cast<Recycling*>(item))
  if (!center->underConstruction) centers.push_back(center);
 if (centers.empty()) { game->recyclingAdequate=false; return; }
 const double now=game->time.absolute*Time::kSimFramesPerDay;
 const double previous=now-game->time.dta*Time::kSimFramesPerDay;
 const int stages[]={1600,1800,2000,2200,2400,2566};
 for (long frame=long(std::floor(previous+1e-7))+1;frame<=long(std::floor(now+1e-7));++frame) {
  const int ft=(frame-300+Time::kSimFramesPerDay)%Time::kSimFramesPerDay;
  if (ft==0 && game->recyclingAdequate && game->recyclingDisplayStage!=0) {
   for (auto *center : centers) { center->upperStage=0; center->lowerStage=6; center->spriteNeedsUpdate=true; }
   game->playOnce("simtower/recycling");
  }
  if (ft==32) {
   for (auto *center : centers) if (center->lowerStage==6) { center->lowerStage=0; center->spriteNeedsUpdate=true; }
   if (game->recyclingAdequate) game->recyclingDisplayStage=0;
  }
  for (int stage=0;stage<6;++stage) if (ft==stages[stage]) {
   const int need=required(game->population/int(centers.size()));
   game->recyclingAdequate=stage>=need;
   game->recyclingDisplayStage=game->recyclingAdequate?need:stage;
   for (auto *center : centers) {
    if (!game->recyclingAdequate && center->lowerStage==5) continue;
    center->upperStage=center->lowerStage=game->recyclingDisplayStage; center->spriteNeedsUpdate=true;
   }
   if (!game->recyclingAdequate && stage==5) game->timeWindow.showMessage("Recycling centers are full of garbage.");
  }
 }
}
void Recycling::advance(double) { if (spriteNeedsUpdate) updateSprite(); }
void Recycling::updateSprite()
{
 spriteNeedsUpdate=false; variant=lowerStage;
 sprite.setTextureRect(sf::IntRect({upperStage*200,0},{200,36}));
 lower.setTextureRect(sf::IntRect({lowerStage*200,36},{200,24}));
}
void Recycling::encodeXML(tinyxml2::XMLPrinter &xml)
{
 Item::encodeXML(xml); xml.PushAttribute("upperStage",upperStage); xml.PushAttribute("lowerStage",lowerStage);
}
void Recycling::decodeXML(tinyxml2::XMLElement &xml)
{
 Item::decodeXML(xml); upperStage=xml.IntAttribute("upperStage",0); lowerStage=xml.IntAttribute("lowerStage",0); updateSprite();
}
