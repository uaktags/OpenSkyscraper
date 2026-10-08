/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include "../YootMisc.h"
#include "YootVenueItems.h"

namespace OT {
	namespace Item {
		/// School.t2p ("Elementary School", ATTR LOCS, grade 4, Tokyo only):
		/// 38x2 units, $1,000,000, 60 students from the pool, open
		/// 08:00-18:00, no income. VenueSystem::schoolTick runs
		/// Yoot::SchoolModel; the students are counted, not walked.
		class School : public YootVenue
		{
		public:
			School(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(School) {
				p->id = "school";
				p->name = "Elementary School";
				p->price = 1000000;
				p->size = int2(38, 2);
				p->icon = 18;
			}
			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			Yoot::SchoolModel model;

		protected:
			const char * defaultPlugin() const { return "School.t2p"; }
			int defaultOpen() const { return 480; }
			int defaultClose() const { return 1080; }
		};

		/// SkyBrdge.t2p ("Sky Bridge", attr 0x45980): a decorative one-row
		/// span between two occupied runs of a row (Yoot::skyBridgeAreaCheck
		/// snaps it to the gap, so each bridge has its own width). It is not
		/// a floor: nobody walks on it and it needs no slab. Drawn by tiling
		/// the plugin's 64x24 RT_BITMAP (pattern 3 = bitmap 1003, set by
		/// BuildFinish FUN_1000c3f0) under the row's 8 px roof.
		class SkyBridge : public Item
		{
		public:
			SkyBridge(Game * game, AbstractPrototype * prototype) : Item(game, prototype) {}
			OT_ITEM_PROTOTYPE(SkyBridge) {
				p->id = "skybridge";
				p->name = "Sky Bridge";
				p->price = 5000;
				p->size = int2(8, 1);
				p->icon = 18;
			}
			virtual void init();
			virtual void render(sf::RenderTarget & target) const;
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			virtual double constructionDuration() const { return 0.0; }
		private:
			const sf::Texture * texture = nullptr;
		};
	}
}
