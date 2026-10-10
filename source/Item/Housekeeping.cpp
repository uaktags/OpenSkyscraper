#include "Housekeeping.h"
#include "Hotel.h"
#include "Elevator/Elevator.h"
#include "../Game.h"
#include <algorithm>
using namespace OT;
using namespace OT::Item;
Housekeeping::Maid::Maid(Housekeeping *center,int k) : Person(center->game,kHousekeeper), currentFloor(center->position.y)
{
 member=k; home=nullptr; state=kIdle; center->Item::addPerson(this);
}
Housekeeping::~Housekeeping()
{
 for (Maid *maid : maidStaff) if (maid) { maid->journey.cancel(); delete maid; }
}
void Housekeeping::init()
{
 Item::init();
 sprite.setTexture(App->bitmaps.editionTexture("housekeeping","simtower/housekeeping"));
 sprite.setOrigin({0.f,24.f}); sprite.setPosition({float(position.x*8),float(-position.y*36)});
 sprite.setTextureRect(sf::IntRect({0,0},{120,24})); addSprite(&sprite);
}
Hotel *Housekeeping::pickDirtyRoom(int anchor,int k) const
{
 auto pickFloor=[&](int floor)->Hotel* {
  if (((floor+10)%6+6)%6 != k) return nullptr;
  auto it=game->itemsByFloor.find(floor);
  if (it==game->itemsByFloor.end()) return nullptr;
  std::vector<Hotel*> hotels;
  for (auto *item : it->second) if (auto *h=dynamic_cast<Hotel*>(item))
   if (h->roomState==Hotel::kDirty && !h->underConstruction) hotels.push_back(h);
  std::sort(hotels.begin(),hotels.end(),[](Hotel *a,Hotel *b) {
   if (a->position.x!=b->position.x) return a->position.x<b->position.x;
   return a->prototype->id<b->prototype->id;
  });
  return hotels.empty() ? nullptr : hotels.front();
 };
 if (game->itemsByFloor.empty()) return nullptr;
 const int lo=game->itemsByFloor.begin()->first, hi=game->itemsByFloor.rbegin()->first;
 for (int f=anchor; f<=hi; ++f) if (auto *h=pickFloor(f)) return h;
 for (int f=anchor-1; f>=lo; --f) if (auto *h=pickFloor(f)) return h;
 return nullptr;
}
bool Housekeeping::select(Maid *maid)
{
 if (game->time.eventFrame()>=1500 || game->emergencyActive() || Elevator::Elevator::schedulePeriod(game->time.absolute)<0) return false;
 Hotel *room=pickDirtyRoom(maid->currentFloor,maid->member);
 if (!room) return false;
 Item *origin=maid->at ? maid->at : this;
 Route route=game->findRoute(origin,room,true);
 if (route.empty()) return false;
 maid->target=room; maid->mode=1; maid->goingTo=room->prototype->name;
 maid->journey.set(route); return true;
}
void Housekeeping::goHome(Maid *maid)
{
 maid->target=nullptr; maid->mode=0; maid->state=Person::kIdle;
 if (maid->at==this) return;
 Route route=game->findRoute(maid->at ? maid->at : this,this,true);
 if (!route.empty()) { maid->mode=3; maid->goingTo="Housekeeping"; maid->journey.set(route); }
}
// YootTower Maintenance.t2p facility view (SpecialSystem::housekeepingTick).
int Housekeeping::maids() const
{
 if (prototype->yoot && prototype->yoot->belongCapacity > 0) return prototype->yoot->belongCapacity;
 return 6; // Maintenance.t2p TNDF belongCapacity (RMaintenance: 4)
}

int Housekeeping::openMinute() const
{
 if (prototype->yoot && prototype->yoot->closeMinute > 0) return prototype->yoot->openMinute;
 return 420;
}

int Housekeeping::closeMinute() const
{
 if (prototype->yoot && prototype->yoot->closeMinute > 0) return prototype->yoot->closeMinute;
 return 960;
}

void Housekeeping::advance(double)
{
 for (int k=0;k<6;++k) if (!maidStaff[k]) maidStaff[k]=new Maid(this,k);
 // YootTower: the facility staff tick (SpecialSystem::housekeepingTick)
 // owns maid dispatch; the EXE maid AI below is the SimTower path.
 if (game->activeRules.profile().yootWorld) return;
 std::vector<Housekeeping*> centers;
 for (auto *item : game->itemsByType["housekeeping"]) if (auto *center=dynamic_cast<Housekeeping*>(item)) centers.push_back(center);
 std::sort(centers.begin(),centers.end(),[](auto *a,auto *b) {
  if (a->position.y!=b->position.y) return a->position.y<b->position.y;
  return a->position.x<b->position.x;
 });
 const int ordinal=std::find(centers.begin(),centers.end(),this)-centers.begin();
 // Enumerate actual ft visits crossed, including large clock advances.
 const double now=game->time.absolute*Time::kSimFramesPerDay;
 const double before=now-game->time.dta*Time::kSimFramesPerDay;
 for (Maid *maid : maidStaff) {
  const int phase=(ordinal*6+maid->member)%16;
  for (long frame=long(std::floor(before+1e-7))+1; frame<=long(std::floor(now+1e-7)); ++frame) {
   const int ft=(frame-300+Time::kSimFramesPerDay)%Time::kSimFramesPerDay;
   if (ft%16!=phase) continue;
   if (maid->at) maid->currentFloor=maid->at->position.y;
   if (maid->mode==3) { if (maid->at==this) maid->mode=0; else continue; }
   if (maid->mode==1) {
    if (!maid->target || !game->items.count(maid->target)) { goHome(maid); continue; }
    if (maid->at!=maid->target) continue;
    if (ft>=1500 || Elevator::Elevator::schedulePeriod(game->time.absolute)<0 || maid->target->roomState!=Hotel::kDirty) { goHome(maid); continue; }
    maid->target->roomState=Hotel::kClean; maid->target->tenant.redraw=true; maid->target->spriteNeedsUpdate=true;
    maid->mode=2; maid->work=3; maid->state=Person::kWorking; continue;
   }
   // 1228:602b case 2 returns after finishing work (all.c:32800-32812,
   // 32834-32839); selection starts on the next 16-frame visit.
   if (maid->mode==2) { if (maid->work>0) { --maid->work; continue; } maid->mode=0; continue; }
   if (maid->mode==0 && !select(maid)) goHome(maid);
  }
 }
}
void Housekeeping::evacuateForDisaster()
{
 for (Maid *maid : maidStaff) if (maid) { maid->journey.cancel(); maid->target=nullptr; maid->mode=0; maid->currentFloor=position.y; Item::addPerson(maid); }
}
