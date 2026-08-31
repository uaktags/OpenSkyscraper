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

            /// Second prototype for the SECOM Center (OpenSkyWeb ISSUE-031):
            /// the original ships two security items — the manual Security
            /// Office and the automated SECOM Center ($100,000, one per
            /// tower). Same class, same sprite, different prototype id so
            /// the EventSystem can single it out.
            static AbstractPrototype * makeSecomPrototype();

            /// Effective security patrol radius: guards cover 15 floors
            /// above or below the office (OpenSkyWeb ISSUE-036 security
            /// coupling for burglars / bomb defusal / fire quenching).
            bool isCoveringFloor(int floor) const
            {
                const int d = position.y - floor;
                return (d >= -15 && d <= 15);
            }

            int variant;

            Sprite sprite;
            bool spriteNeedsUpdate;
            void updateSprite();
        };
    }
}
