#pragma once
#include "Item.h"
#include <array>
namespace OT { namespace Item {
class Hotel;
class Housekeeping : public Item {
public:
 OT_ITEM_CONSTRUCTOR(Housekeeping);
 OT_ITEM_PROTOTYPE(Housekeeping) {
  p->id="housekeeping"; p->name="Housekeeping"; p->size=int2(15,1); p->price=50000; p->icon=21;
 }
 class Maid : public Person {
 public:
  Maid(Housekeeping *center,int k);
  Hotel *target=nullptr;
  int currentFloor, work=0, mode=0;
 };
 std::array<Maid*,6> maids{};
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
