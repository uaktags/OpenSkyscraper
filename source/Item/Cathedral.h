#pragma once
#include "../Sprite.h"
#include "Item.h"

namespace OT {
    namespace Item {
        class Cathedral : public Item
        {
        public:
            Cathedral(Game * game, AbstractPrototype * prototype);
            OT_ITEM_PROTOTYPE(Cathedral) {
                p->id = "cathedral";
                p->name = "Cathedral";
                p->price = 500000; // Dummy price
                p->size = int2(32, 4); // Dummy size
                   p->icon = 45; // Dummy icon
                   p->unlockRating = 6; // tower rating (6)
            }
            virtual ~Cathedral();

            virtual void init();
            virtual void encodeXML(tinyxml2::XMLPrinter & xml) override;
            virtual void decodeXML(tinyxml2::XMLElement & xml) override;
            virtual void advance(double dt) override;

        protected:
            void updateSprite();

            int rent;
            int rentDeposit;
            bool occupied;
            int variant;
            bool lit;

            Sprite sprite;
            bool spriteNeedsUpdate;
        };
    }
}
