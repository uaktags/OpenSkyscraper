/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include <string>
#include "../Sound.h"
#include "../YootVenues.h"
#include "Metro.h"
#include "YootFacilityItems.h"

#include <SFML/Audio/SoundBuffer.hpp>

namespace OT {
	namespace Item {
		/** Common base of the YootTower venues (plan step 14 part B): a
		 * plugin-art tenant (YootFacility: RT_BITMAP 1000 sliced by the
		 * OBJMAP 1000 View patterns) with the T2Tenant status (+0x08),
		 * today's visitor count (TotalCustomer +0xa8) and the people inside
		 * (YootFacility::leaveAt). VenueSystem drives them once per game
		 * minute; the classes hold state and draw. YootTower-only. */
		class YootVenue : public YootFacility
		{
		public:
			YootVenue(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			int status = 10;          ///< T2Tenant +0x08 (10 closed, 30.. open)
			int visitorsToday = 0;    ///< TotalCustomer +0xa8
			double arrivalCarry = 0;  ///< fractional arrivals (VenueSystem)
			int lastOpenDay = -1;     ///< absolute day the venue last opened
			int settledDay = -1;      ///< absolute day of the last settlement

			int openMinute() const;
			int closeMinute() const;
			bool isOpen() const { return status >= 30; }
		protected:
			virtual int defaultOpen() const { return 600; }
			virtual int defaultClose() const { return 1320; }
		};

		/// Theater.t2p (ATTR 1720, grade 3): 31x3 units, 500 seats, open
		/// 10:00-22:00, roadshow films; ClassicTheater.t2p (1721): 24x2,
		/// 40 seats, 12:00-22:00, famous films. Patterns: 0 closed, 1 doors
		/// open, 2 half full, 3 playing.
		class Theater : public YootVenue
		{
		public:
			Theater(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			virtual ~Theater();
			OT_ITEM_PROTOTYPE(Theater) {
				p->id = "theater";
				p->name = "Movie Theater";
				p->price = 500000;
				p->size = int2(31, 3);
				p->icon = 14;
			}
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			virtual bool classic() const { return false; }
			std::string movie;        ///< MV_*.t2p plugin showing ("" = none)
			bool expired = false;     ///< the roadshow left phase 1
			int audienceTarget = -1;  ///< +0x30 target per show
			int showBest = 0;         ///< best show of the current day
			int yesterdayBest = 0;
			int showCount = 0;        ///< days run with this movie

			/// Plays the movie's .MOV sound track while a show runs.
			void updateSoundtrack(bool playing);
		protected:
			const char * defaultPlugin() const { return "Theater.t2p"; }
			int defaultOpen() const { return 600; }
			int defaultClose() const { return 1320; }
		private:
			Sound soundtrack;
			sf::SoundBuffer soundBuffer;
			std::string soundMovie;
			bool soundPlaying = false;
		};

		class ClassicTheater : public Theater
		{
		public:
			ClassicTheater(Game * game, AbstractPrototype * prototype) : Theater(game, prototype) {}
			OT_ITEM_PROTOTYPE(ClassicTheater) {
				p->id = "classictheater";
				p->name = "Classic Movie Theatre";
				p->price = 300000;
				p->size = int2(24, 2);
				p->icon = 14;
			}
			bool classic() const { return true; }
		protected:
			const char * defaultPlugin() const { return "ClassicTheater.t2p"; }
			int defaultOpen() const { return 720; }
		};

		/// EventCenter.t2p (ATTR 1722, grade 3): 31x2, capacity 80, hosts
		/// the Pl* hall events.
		class EventCenter : public YootVenue
		{
		public:
			EventCenter(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(EventCenter) {
				p->id = "eventcenter";
				p->name = "Event Center";
				p->price = 500000;
				p->size = int2(31, 2);
				p->icon = 15;
			}
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			/// One booking (OpenSky keeps a single booking instead of the
			/// 12-day ring of FUN_1000e6e0): `event` runs `span` days from
			/// `startDay` (absolute day).
			std::string event;        ///< Pl*.t2p booked ("" = none)
			int startDay = 0;
			int span = 0;
			std::string today;        ///< event attached today (SetHallEventPlugin)
			int target = 0;           ///< visitors called for today (catalog value)
			bool runsOn(int day) const { return !event.empty() && day >= startDay && day < startDay + span; }

		protected:
			const char * defaultPlugin() const { return "EventCenter.t2p"; }
			int defaultOpen() const { return 540; }
			int defaultClose() const { return 1080; }
		};

		/// Stadium.t2p (ATTR 2001, Tokyo final item): 56x5, 400 seats, open
		/// 12:00-23:00 in April-October. Hosts the baseball event (R4).
		class Stadium : public YootVenue
		{
		public:
			Stadium(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(Stadium) {
				p->id = "stadium";
				p->name = "Stadium";
				p->price = 5000000;
				p->size = int2(56, 5);
				p->icon = 18;
			}
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			std::string teamName;     ///< BBEV naming dialog (stadiumDef OptionProc(1))
		protected:
			const char * defaultPlugin() const { return "Stadium.t2p"; }
			int defaultOpen() const { return 720; }
			int defaultClose() const { return 1380; }
		};

		/// Cathedral.t2p (ATTR 2000, Hawaii final item): 28x8, capacity
		/// 75, open on holidays 11:00-17:00. Hosts the wedding (R4).
		class YootCathedral : public YootVenue
		{
		public:
			YootCathedral(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(YootCathedral) {
				p->id = "yootcathedral";
				p->name = "Cathedral";
				p->price = 3000000;
				p->size = int2(28, 8);
				p->icon = 18;
			}
		protected:
			const char * defaultPlugin() const { return "Cathedral.t2p"; }
			int defaultOpen() const { return 660; }
			int defaultClose() const { return 1020; }
		};

		/// Attraction.t2p (ATTR 2002, Kegon final item): 45x6, capacity
		/// 70, open 10:00-17:00; renovated at 200 visitors a day (R4).
		class Attraction : public YootVenue
		{
		public:
			Attraction(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(Attraction) {
				p->id = "attraction";
				p->name = "Attraction";
				p->price = 1000000;
				p->size = int2(45, 6);
				p->icon = 18;
			}
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			/// FUN_1000c290 swaps the art to RT_BITMAP 9998. The original
			/// keeps this flag in the def and does not save it; OpenSky saves it.
			bool renovated = false;
			void renovate();
		protected:
			const char * defaultPlugin() const { return "Attraction.t2p"; }
			int defaultOpen() const { return 600; }
			int defaultClose() const { return 1020; }
		};

		/// Simple data-driven venues: Observatory.t2p (Kegon, 17x2, 08:00-
		/// 18:00), Ticket.t2p (Kegon ticket booth, 12x1, 09:00-16:00) and
		/// GatheringArea.t2p (35x2, 10:00-22:00). Visitors come and go during
		/// the open hours and pay CalcInMoney at close.
		class Observatory : public YootVenue
		{
		public:
			Observatory(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(Observatory) {
				p->id = "observatory";
				p->name = "Observatory";
				p->price = 1000000;
				p->size = int2(17, 2);
				p->icon = 18;
			}
		protected:
			const char * defaultPlugin() const { return "Observatory.t2p"; }
			int defaultOpen() const { return 480; }
			int defaultClose() const { return 1080; }
		};

		class TicketBooth : public YootVenue
		{
		public:
			TicketBooth(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(TicketBooth) {
				p->id = "ticketbooth";
				p->name = "Ticket Booth";
				p->price = 10000;
				p->size = int2(12, 1);
				p->icon = 18;
			}
		protected:
			const char * defaultPlugin() const { return "Ticket.t2p"; }
			int defaultOpen() const { return 540; }
			int defaultClose() const { return 960; }
		};

		class GatheringArea : public YootVenue
		{
		public:
			GatheringArea(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(GatheringArea) {
				p->id = "gatheringarea";
				p->name = "Gathering Area";
				p->price = 500000;
				p->size = int2(35, 2);
				p->icon = 18;
			}
		protected:
			const char * defaultPlugin() const { return "GatheringArea.t2p"; }
			int defaultOpen() const { return 600; }
			int defaultClose() const { return 1320; }
		};

		/** Billboard.t2p (ATTR 2100, an outer object, OODF 20x2): shows a
		 * sponsor ad (the 7 lPdA SP plugins) for its 12 campaign days; the
		 * sponsor pays its money up front (FUN_1000bba0). */
		class Billboard : public YootFacility
		{
		public:
			Billboard(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(Billboard) {
				p->id = "billboard";
				p->name = "Billboard";
				p->price = 100000;
				p->size = int2(20, 2);
				p->icon = 18;
			}
			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			std::string ad;           ///< advertise plugin showing ("" = empty board)
			int daysLeft = 0;
			int lastDay = -1;
			void showAd(const std::string & plugin, int days);
		protected:
			const char * defaultPlugin() const { return "Billboard.t2p"; }
		private:
			Sprite adSprite;
			sf::Texture adTexture;
		};

		/** Boatdeck.t2p (ATTR 1723, Hawaii, grade 3): 20x2 at the left edge
		 * of B2, open 06:00-20:00, capacity 60. Its BuildFinish creates the
		 * world cruise X-event (CREV): the ship docks four times a day and
		 * brings visitors (VenueSystem). */
		class BoatDeck : public YootVenue
		{
		public:
			BoatDeck(Game * game, AbstractPrototype * prototype) : YootVenue(game, prototype) {}
			OT_ITEM_PROTOTYPE(BoatDeck) {
				p->id = "boatdeck";
				p->name = "Boat Deck";
				p->price = 1000000;
				p->size = int2(20, 2);
				p->icon = 18;
			}
		protected:
			const char * defaultPlugin() const { return "Boatdeck.t2p"; }
			int defaultOpen() const { return 360; }
			int defaultClose() const { return 1200; }
		};

		/** Subway.t2p (ATTR 1701, Tokyo, grade 2): 40x3 below the lowest

		 * floor, open 06:30-23:59, capacity 100. Trains follow the world's
		 * MREV timetable (VenueSystem); riders become Metro visitors. The
		 * station still registers as Game::metroStation. */
		class Subway : public Metro
		{
		public:
			Subway(Game * game, AbstractPrototype * prototype) : Metro(game, prototype) {}
			OT_ITEM_PROTOTYPE(Subway) {
				p->id = "subway";
				p->name = "Subway Station";
				p->price = 1000000;
				p->size = int2(40, 3);
				p->icon = 19;
				p->entrance_offset = 2;
				p->exit_offset = 2;
			}
			virtual void init();
			virtual void advance(double dt);
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			virtual int dailyMaintenanceCost() const override { return 0; }

			int level = 1;            ///< T2Transport UpGrade level 1..4 (trains enabled)
			int pattern = 0;
			int trainType = 0;        ///< MREV track of the stopped train (MET1..MET4)
			/// A train from the timetable stops: riders get off into the tower.
			void trainArrives(int riders);
			void trainLeaves();
			void setPattern(int p);
		private:
			Sprite viewSprite;
			const sf::Texture * view = nullptr;
			Sprite trainSprite;       ///< Skin::Yoot: the stopped train
			void showTrain(bool shown);
		};
	}
}
