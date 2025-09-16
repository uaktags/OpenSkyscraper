/* Copyright © 2022 Vuong Ly */
/* Copyright © 2025 Tim G */
#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "Factory.h"
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
                p->icon = ICON_MEDICAL_CENTER;
                p->unlockRating = 4; // unlocked at 4 stars
                p->toolboxCategory = CAT_UTILITIES;
                p->toolboxOrder = 2;
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