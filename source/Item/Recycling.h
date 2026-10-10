#pragma once
#include "../Sprite.h"
#include "Item.h"
#include <set>

namespace OT {
    namespace Item {
        class Recycling : public Item
        {
        public:
            OT_ITEM_CONSTRUCTOR(Recycling);
            OT_ITEM_PROTOTYPE(Recycling) {
                p->id = "recycling";
                p->name = "Recycling Center";
                p->price = 500000;
                p->size = int2(25,2);
                p->icon = 18;
            }
            virtual ~Recycling();

            virtual void init();

            virtual void advance(double dt);

            int variant;
            int upperStage=0, lowerStage=0;
            static void advanceAll(Game*);
            static int required(int ratio);
            void encodeXML(tinyxml2::XMLPrinter&) override;
            void decodeXML(tinyxml2::XMLElement&) override;

            Sprite sprite;
            Sprite lower;
            bool spriteNeedsUpdate;
            void updateSprite();
        };
    }
}
