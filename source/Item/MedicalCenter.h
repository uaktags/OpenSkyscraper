#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>

namespace OT {
    namespace Item {
        class MedicalCenter : public Item
        {
        public:
            OT_ITEM_CONSTRUCTOR(MedicalCenter);
            OT_ITEM_PROTOTYPE(MedicalCenter) {
                p->id = "medicalcenter";
                p->name = "Medical Center";
                p->price = 500000;
                p->size = int2(26,1);
                p->icon = 22;
            }
            virtual ~MedicalCenter();

            virtual void init();

            virtual void advance(double dt);

            int variant;
            int patients=0;
            struct Treatment { Person *person; Item *origin; double until; };
            std::vector<Treatment> treatments;
            bool admitPatient(Person*);
            void addPerson(Person*) override;
            void evacuateForDisaster() override;

            Sprite sprite;
            bool spriteNeedsUpdate;
            void updateSprite();
        };
    }
}
