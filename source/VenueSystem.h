/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "Sprite.h"
#include "PluginDefs.h"
#include "SubPluginDefs.h"
#include "YootVenues.h"

#include <map>
#include <string>
#include <vector>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	class YootVisitor;
	namespace Item { class Item; class YootVenue; class Theater; class EventCenter; class Billboard; class Subway; }

	/** YootTower venues, final items, transport and cosmetic world events
	 * (game-parity.md S1, S2, R4, V6, I7, M8; plan step 14 part B).
	 *
	 * The rules are the pure functions in YootVenues.h; this class applies
	 * them to the tower once per game minute. Owned by EventSystem next to
	 * PowerSystem and FacilitySystem; every entry point is a no-op outside
	 * YootTower.
	 *
	 * The audiences are pool people called by T2Tenant::CollectCustomer
	 * from each venue plugin's 8-minute idle (PeopleSystem::collectVenue);
	 * they walk in, stay by the venue's EnterCustomer rule and walk out.
	 *
	 * Simplifications (documented in game-parity.md):
	 * - the Event Center keeps one booking instead of the 12-day ring;
	 * - the baseball naming dialog has no fireworks after it. */
	class VenueSystem : public GameObject
	{
	public:
		VenueSystem(Game * game);

		void reset();
		bool enabled() const;

		/// Per-frame: runs minuteTick once per new game minute.
		void advance();
		void draw(sf::RenderTarget & target) const;
		/// Skin::Yoot: the MREV trains on the track row (layer 2, under the
		/// movers and people).
		void drawTrains(sf::RenderTarget & target) const;

		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		/// Inspector lines for venues ("" if none).
		std::string describe(Item::Item * item);

		/// A called visitor reaches the venue (the plugin's EnterCustomer):
		/// admitted while open, with the venue's stay.
		void enter(Item::YootVenue * v, YootVisitor * p);

		// --- player choices (inspector) ---
		/// Labels and plugin names offered for a theater / event center /
		/// billboard ("" plugin = none).
		std::vector<std::pair<std::string, std::string> > choices(Item::Item * item);
		/// Applies a choice; returns false with a reason when refused.
		bool choose(Item::Item * item, const std::string & plugin, std::string * reason);
		/// Subway timetable upgrade (T2Transport::UpGrade, $500,000 a step).
		bool upgradeSubway(Item::Subway * subway, std::string * reason);

		// --- world events (EventSystemYoot) ---
		void beginBaseball();
		void beginWedding();
		bool baseballActive() const { return baseball.active; }
		bool weddingActive() const { return wedding.active; }
		/// The cruise is at the Boat Deck (CREV: 30 minutes from each
		/// sailing, grade 3+): T2Transport::GetAvailable for ship visitors.
		bool cruiseDocked() const;
		/// T2Transport::GetAvailable 0x1006bd80 / IsAvailable 0x1006be30: a
		/// train (ship) arrives within the next 60 minutes, so a rider can
		/// board it (MoveToSubway / MoveToShip).
		bool transportAvailable(bool ship) const;
		/// The station of the subway / the Boat Deck while its train or ship
		/// is stopped (ChkStoppage 0x1006bcf0), else null.
		Item::Item * transportStopped(bool ship) const;

		// --- movies ---
		struct Movie
		{
			std::string plugin;
			MovieDef def;
			Yoot::MovieState state;
		};
		const std::vector<Movie> & movies();
		Movie * movie(const std::string & plugin);
		int movieBaseDay = -1;    ///< doc+0x234: day the first theater opened
		bool baseballDone = false;
		bool weddingDone = false;

	private:
		int lastMinute;
		int lastDay;
		bool moviesLoaded;
		std::vector<Movie> movieList;
		std::map<std::string, int> hallCooldown;  ///< event plugin -> days left
		std::map<std::string, int> hallValue;     ///< event plugin -> target visitors

		struct WorldEvent { bool active = false; int state = 0; };
		WorldEvent baseball, wedding;

		struct Cloud { bool live = false; float x = 0, y = 0; int speed = 2; int image = 0; };
		std::vector<Cloud> clouds;
		std::vector<int> cloudImages;
		bool cloudsInit = false;
		long long lastCloudMinute = -1;
		mutable std::map<int, sf::Texture> worldTextures;

		struct Sailing { int arrive = 0; };
		int cruiseDockX = 46, cruiseDockMinutes = 30;
		std::vector<int> cruiseTimes;
		bool cruiseInit = false;

		int grade() const;
		int dayOfYear() const;
		bool reachable(Item::Item * item) const;
		void minuteTick(int minute);
		void newDay(int day);
		void collect(Item::YootVenue * v, int minute, int stay);
		/// One CollectCustomer for the venue with its own table, the people
		/// walking to `dest` (null: the venue).
		void collectNow(Item::YootVenue * v, Item::Item * dest = nullptr);
		void ticketBoothIdle(Item::YootVenue * booth, Item::YootVenue * obs);
		void pay(Item::Item * item, int dollars, const char * message);
		void theaterTick(Item::Theater * t, int minute, int day);
		void eventCenterTick(Item::EventCenter * e, int minute, int day);
		/// T2Tenant::CalcProfitEstimate of the hall (visitors since it opened).
		int hallEstimate(const Item::EventCenter * e) const;
		/// EventCenter.t2p EnterTenant hook 0x1000bba0: 30 -> 31 -> 32.
		void hallEntered(Item::EventCenter * e);
		void genericTick(Item::YootVenue * v, int minute);
		void subwayTick(int minute);
		void cruiseTick(int minute);
		void baseballTick();
		void weddingTick();
		void cloudTick(long long absMinute);
		sf::Texture * worldTexture(int id) const;
	};
}
