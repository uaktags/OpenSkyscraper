#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>

namespace OT {
    namespace Item {
        class Security : public Item
        {
        public:
            OT_ITEM_CONSTRUCTOR(Security);
            OT_ITEM_PROTOTYPE(Security) {
                p->id = "security";
                p->name = "Security";
                p->price = 100000;
                p->size = int2(16,1);
                p->icon = 21;
            }
            virtual ~Security();

            virtual void init();

            int variant;

            Sprite sprite;
            bool spriteNeedsUpdate;
            void updateSprite();
        };
    }
}
