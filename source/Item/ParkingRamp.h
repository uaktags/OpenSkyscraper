#pragma once
#include "Item.h"
namespace OT { namespace Item {
class ParkingRamp : public Item {
public:
 OT_ITEM_CONSTRUCTOR(ParkingRamp);
 OT_ITEM_PROTOTYPE(ParkingRamp) {
  p->id="parkingramp"; p->name="Parking Ramp"; p->size=int2(16,1); p->price=50000; p->icon=17;
 }
 void init() override;
 void advance(double) override;
 Sprite sprite;
};
} }
