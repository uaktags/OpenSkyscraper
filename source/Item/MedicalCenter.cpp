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
    if (game->time.checkHour(7)) patients=0;
    for (auto it=treatments.begin();it!=treatments.end();) {
        if (!game->people.count(it->person)) { it=treatments.erase(it); continue; }
        Person *p=it->person;
        if (p->at==this && game->time.absolute>=it->until) {
            Route route=game->findRoute(this,it->origin);
            if (!route.empty()) { p->state=Person::kReturning; p->journey.set(route); }
            it=treatments.erase(it);
        } else ++it;
    }
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
    // Native art is two 312x24 frames; crop each to the 208px footprint.
    sprite.setTextureRect(sf::IntRect({variant * 312, 0}, {208, 24}));
}

bool MedicalCenter::admitPatient(Person *person)
{
 if (!person || patients>=40 || underConstruction) return false;
 Item *origin=person->home ? person->home : person->at;
 if (!origin) return false;
 ++patients;
 Route route=game->findRoute(origin,this);
 if (route.empty()) { person->recordNoRoute(origin->position.y,position.y); return false; }
 treatments.push_back({person,origin,game->time.absolute+Time::hourToAbsolute(0.5)});
 person->state=Person::kShopping; person->goingTo="Medical Center";
 person->journey.set(route); return true;
}
void MedicalCenter::addPerson(Person *person)
{
 Item::addPerson(person); person->state=Person::kResting;
 for (auto &t : treatments) if (t.person==person) t.until=game->time.absolute+Time::hourToAbsolute(0.5);
}
void MedicalCenter::evacuateForDisaster() { treatments.clear(); patients=0; }
