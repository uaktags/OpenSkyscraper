#pragma once
#include "Item.h"
#include <array>
namespace OT { namespace Item {
class Hotel;
class Housekeeping : public Item {
public:
 OT_ITEM_CONSTRUCTOR(Housekeeping);
 OT_ITEM_PROTOTYPE(Housekeeping) {
  p->id="housekeeping"; p->name="Housekeeping"; p->size=int2(15,1); p->price=50000; p->icon=23;
 }
 class Maid : public Person {
 public:
  Maid(Housekeeping *center,int k);
  Hotel *target=nullptr;
  int currentFloor, work=0, mode=0;
 };
 std::array<Maid*,6> maidStaff{};
 // YootTower Maintenance.t2p facility API (SpecialSystem::housekeepingTick).
 int maids() const;
 int openMinute() const;
 int closeMinute() const;
 void setPattern(int p) { facilityPattern = p; }
 int maidsOut = 0, roomsCleaned = 0, facilityPattern = 0;
 ~Housekeeping() override;
 void init() override;
 void advance(double) override;
 void evacuateForDisaster() override;
 Hotel *pickDirtyRoom(int currentFloor,int member) const;
 bool select(Maid*);
 void goHome(Maid*);
 Sprite sprite;
};
} }
