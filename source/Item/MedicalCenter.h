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
                p->price = 100000;
                p->size = int2(32,1);
                p->icon = 22;
            }
            virtual ~MedicalCenter();

            virtual void init();

            int variant;

            Sprite sprite;
            bool spriteNeedsUpdate;
            void updateSprite();
        };
    }
}
