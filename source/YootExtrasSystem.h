/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "Route.h"
#include "YootMisc.h"

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	class YootVisitor;
	struct YootEventSpec;
	class YootEventScheduler;
	namespace Item { class Item; }

	/** YootTower systems closed after plan step 14 (YootMisc.h has the
	 * rules): the School's day, the Kegon black kite (TOEV) and Hawaii
	 * dolphins (DOEV), Mr. Maru (MAEV 128), the Kegon hot spring (HSEV) and
	 * the traffic map (T2TrafficInfo) behind the residential noise term.
	 * Owned by EventSystem; every entry point is a no-op outside YootTower.
	 *
	 * Simplifications: Mr. Maru is not drawn and walks 3 minutes a floor
	 * (people are drawn elsewhere); students are counted; traffic is
	 * booked per route when a person sets out (T2People::FindRoute books
	 * each leg as it is taken). */
	class YootExtrasSystem : public GameObject
	{
	public:
		YootExtrasSystem(Game * game);

		void reset();
		bool enabled() const;
		/// Per sim tick: minute and hour work.
		void advance();
		/// A T2EV event this system owns begins (DOEV, TOEV, MAEV 128).
		/// Returns false when the type is not handled here.
		bool begin(const YootEventSpec & e, int index);
		/// Marks its running events on the scheduler.
		void syncRunning(YootEventScheduler & scheduler) const;

		/// A build finished at the tool (T2TowerEvent::SetLatestBuild).
		void onBuilt(int topFloor);
		/// T2People::FindRoute -> T2TrafficInfo::Pass for each walk leg.
		void bookRoute(const Route & route);
		/// GetRange at the tenant's entrance (bottom row, left + width / 2).
		int trafficRange(const Item::Item * item) const;

		void drawBehindTower(sf::RenderTarget & target) const;
		void drawInFront(sf::RenderTarget & target) const;

		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		Yoot::TrafficMap traffic;

	private:
		struct Flight
		{
			bool active = false;
			int index = -1;
			int endMin = 1020;
			Yoot::TonbiDef def;
			Yoot::TonbiFlight run;
		};
		struct Maru
		{
			bool active = false;
			int index = -1;
			int endMin = 1380;
			Item::Item * target = nullptr;
			long long arriveAt = 0;   ///< absolute minute he reached the target (-1 walking)
			bool soundPlayed = false;
			int visits = 0;
			bool flipped = false;     ///< +0x5c: FindNextTenant turned at an edge once
			YootVisitor * walker = nullptr;  ///< the pool VIP who walks (PeopleSystem)
		};

		void configureTraffic();
		void schoolMinute(long long now, int minute);
		void flightMinute(Flight & f, int minute);
		void maruMinute(long long now, int minute);
		Item::Item * pickMaruTarget(Item::Item * not_) const;
		Item::Item * nextMaruTenant();
		void drawFlight(sf::RenderTarget & target, const Flight & f) const;
		sf::Texture * worldTexture(int id) const;

		long long lastMinute = -1;
		long long lastHour = -1;
		std::string trafficKey;
		std::vector<Flight> flights;
		Maru maru;
		mutable std::map<int, sf::Texture> textures;
		std::unique_ptr<sf::SoundBuffer> maruBuffer;
		std::unique_ptr<sf::Sound> maruSound;
	};
}
