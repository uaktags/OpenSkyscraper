#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>
namespace OT { namespace Item {
class Parking;
class Hotel : public Item {
public:
 OT_ITEM_CONSTRUCTOR(Hotel);
 static AbstractPrototype *makeSinglePrototype();
 static AbstractPrototype *makeDoublePrototype();
 static AbstractPrototype *makeSuitePrototype();
 ~Hotel() override;
 enum RoomType { kSingle=0, kDouble=1, kSuite=2 };
 enum RoomState { kClean=0, kOccupied=1, kDirty=2, kInfested=3 };
 int variant, subVariant, roomState, hosted = 0;
 double dirtySince;
 bool dinnerSent = false, evacuated = false;
 int capacity() const;
 void setRentClass(int c);
 class Guest : public Person {
 public:
  Guest(Hotel *item, int memberIndex=0);
  Hotel *hotel;
  bool atHotel = false, leaving = false;
  Parking *parkingUsed = nullptr;
  Item *entry = nullptr;
 };
 std::set<Guest*> guests;
 void init() override;
 void encodeXML(tinyxml2::XMLPrinter&) override;
 void decodeXML(tinyxml2::XMLElement&) override;
 void advance(double) override;
 void addPerson(Person*) override;
 void removePerson(Person*) override;
 void evacuateForDisaster() override;
 int populationCategory() const override { return variant + 1; }
 int populationContribution() const override { return hosted; }
 void stampInfested();
 void checkout(bool paid);
 bool checkIn();
 static void spreadRoaches(Game*);
 static void judgeAll(Game*);
 Sprite sprite;
 bool spriteNeedsUpdate;
 int roomFrameIndex;
 void updateSprite();
 Path getRandomBackgroundSoundPath() override;
protected:
 void clearAll(bool adjustPopulation = true);
 void applyVariant();
 void resetBanks();
};
} }
