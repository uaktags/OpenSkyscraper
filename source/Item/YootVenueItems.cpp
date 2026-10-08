/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../Logger.h"
#include "../PeopleSystem.h"
#include "../YootCatalog.h"
#include "../YootItemArt.h"
#include "../YootMovie.h"
#include "../YootRules.h"
#include "YootVenueItems.h"

#include <algorithm>
#include <fstream>

using namespace OT;
using namespace OT::Item;

namespace
{
	bool fileExists(const std::string & p)
	{
		std::ifstream f(p, std::ios::binary);
		return (bool)f;
	}

	/// Movie/<file> next to a data dir's Plugins/ folder, as in the
	/// YootTower install ("" when absent).
	std::string findMovieFile(Game * game, const std::string & file)
	{
		if (file.empty()) return "";
		for (const Path & base : game->app.data.searchDirs())
		{
			const std::string p = (base + "Movie" + file).str();
			if (fileExists(p)) return p;
		}
		return "";
	}
}

// ------------------------------------------------------------------ venue

void YootVenue::init()
{
	YootFacility::init();
	status = 10;
	visitorsToday = 0;
	arrivalCarry = 0;
	idlePhase = (int)(((unsigned)(position.x * 31 + position.y * 17)) % Yoot::kTenantIdleMinutes);
}

void YootVenue::addPerson(Person * p)
{
	YootFacility::addPerson(p);
	if (YootVisitor * v = dynamic_cast<YootVisitor *>(p))
		if (v->dest == this) game->eventSystem.venues.enter(this, v);
}

int YootVenue::openMinute() const
{
	if (prototype->yoot && prototype->yoot->closeMinute > 0) return prototype->yoot->openMinute;
	return defaultOpen();
}

int YootVenue::closeMinute() const
{
	if (prototype->yoot && prototype->yoot->closeMinute > 0) return prototype->yoot->closeMinute;
	return defaultClose();
}

void YootVenue::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootFacility::encodeXML(xml);
	xml.PushAttribute("status", status);
	xml.PushAttribute("visitorsToday", visitorsToday);
	xml.PushAttribute("lastOpenDay", lastOpenDay);
	xml.PushAttribute("settledDay", settledDay);
}

void YootVenue::decodeXML(tinyxml2::XMLElement & xml)
{
	YootFacility::decodeXML(xml);
	status = xml.IntAttribute("status", 10);
	visitorsToday = std::max(0, xml.IntAttribute("visitorsToday", 0));
	lastOpenDay = xml.IntAttribute("lastOpenDay", -1);
	settledDay = xml.IntAttribute("settledDay", -1);

	// Visitors inside are transient (YootFacility::decodeXML drops them):
	// a reloaded venue reopens empty.
	if (status > 10) status = 30;
}

// ---------------------------------------------------------------- theater

Theater::~Theater()
{
	soundtrack.Stop();
}

void Theater::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootVenue::encodeXML(xml);
	xml.PushAttribute("movie", movie.c_str());
	xml.PushAttribute("expired", expired);
	xml.PushAttribute("audienceTarget", audienceTarget);
	xml.PushAttribute("showBest", showBest);
	xml.PushAttribute("yesterdayBest", yesterdayBest);
	xml.PushAttribute("showCount", showCount);
}

void Theater::decodeXML(tinyxml2::XMLElement & xml)
{
	YootVenue::decodeXML(xml);
	movie = xml.Attribute("movie") ? xml.Attribute("movie") : "";
	expired = xml.BoolAttribute("expired", false);
	audienceTarget = xml.IntAttribute("audienceTarget", -1);
	showBest = std::max(0, xml.IntAttribute("showBest", 0));
	yesterdayBest = std::max(0, xml.IntAttribute("yesterdayBest", 0));
	showCount = std::max(0, xml.IntAttribute("showCount", 0));
}

void Theater::updateSoundtrack(bool playing)
{
	// The original plays the clip in the info dialog (T2DlgItemMovie). OpenSky
	// plays the clip's PCM sound track on loop while a show runs (no Cinepak).
	if (playing && movie != soundMovie)
	{
		soundtrack.Stop();
		soundPlaying = false;
		soundMovie = movie;
		soundBuffer = sf::SoundBuffer();
		MovieDef def;
		AbstractPlugin * p = game->app.pluginManager.get(movie);
		YootMovieSound s;
		const std::string path = (p && p->ensureLoaded() && p->getMovieDef(def)) ? findMovieFile(game, def.movieFile) : "";
		if (!path.empty() && loadQuickTimeSound(path, s))
		{
			std::vector<sf::SoundChannel> map = s.channels == 2
				? std::vector<sf::SoundChannel>{sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight}
				: std::vector<sf::SoundChannel>{sf::SoundChannel::Mono};
			if (!soundBuffer.loadFromSamples(s.samples.data(), s.samples.size(), s.channels, s.sampleRate, map))
				soundBuffer = sf::SoundBuffer();
		}
	}
	const bool want = playing && soundBuffer.getSampleCount() > 0;
	if (want == soundPlaying) return;
	if (want)
	{
		soundtrack.setBuffer(soundBuffer);
		soundtrack.setLooping(true);
		soundtrack.Play(game);
	}
	else
		soundtrack.Stop();
	soundPlaying = want;
}

// ------------------------------------------------------------ event center

void EventCenter::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootVenue::encodeXML(xml);
	xml.PushAttribute("event", event.c_str());
	xml.PushAttribute("startDay", startDay);
	xml.PushAttribute("span", span);
	xml.PushAttribute("today", today.c_str());
	xml.PushAttribute("target", target);
}

void EventCenter::decodeXML(tinyxml2::XMLElement & xml)
{
	YootVenue::decodeXML(xml);
	event = xml.Attribute("event") ? xml.Attribute("event") : "";
	startDay = xml.IntAttribute("startDay", 0);
	span = std::max(0, xml.IntAttribute("span", 0));
	today = xml.Attribute("today") ? xml.Attribute("today") : "";
	target = std::max(0, xml.IntAttribute("target", 0));
}

// ---------------------------------------------------------------- billboard

void Billboard::init()
{
	YootFacility::init();
	adSprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
}

void Billboard::showAd(const std::string & plugin, int days)
{
	ad = plugin;
	daysLeft = plugin.empty() ? 0 : days;
	removeSprite(&adSprite);
	if (ad.empty() || !game->app.bitmaps.loadFromPlugin(ad, 128, adTexture)) { setPattern(0); return; }
	// Billboard.t2p FUN_1000c1a0: the ad DIB centred on the 160x60 board.
	adSprite.SetImage(adTexture);
	const sf::Vector2u s = adTexture.getSize();
	const float boardW = view ? (float)view->getSize().x : 160.f;
	const float boardH = view ? (float)view->getSize().y : 60.f;
	adSprite.setOrigin({0.f, 0.f});
	adSprite.setPosition({getPosition().x * 8 + (boardW - s.x) / 2.f,
	                      -getPosition().y * 36 - interiorLift() - boardH + (boardH - s.y) / 2.f});
	addSprite(&adSprite);
	setPattern(1);
}

void Billboard::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootFacility::encodeXML(xml);
	xml.PushAttribute("ad", ad.c_str());
	xml.PushAttribute("daysLeft", daysLeft);
	xml.PushAttribute("lastDay", lastDay);
}

void Billboard::decodeXML(tinyxml2::XMLElement & xml)
{
	YootFacility::decodeXML(xml);
	lastDay = xml.IntAttribute("lastDay", -1);
	const std::string a = xml.Attribute("ad") ? xml.Attribute("ad") : "";
	showAd(a, std::max(0, xml.IntAttribute("daysLeft", 0)));
}

// ----------------------------------------------------------------- stadium

void Stadium::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootVenue::encodeXML(xml);
	if (!teamName.empty()) xml.PushAttribute("team", teamName.c_str());
}

void Stadium::decodeXML(tinyxml2::XMLElement & xml)
{
	YootVenue::decodeXML(xml);
	teamName = xml.Attribute("team") ? xml.Attribute("team") : "";
}

// -------------------------------------------------------------- attraction

void Attraction::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootVenue::encodeXML(xml);
	xml.PushAttribute("renovated", renovated);
}

void Attraction::decodeXML(tinyxml2::XMLElement & xml)
{
	YootVenue::decodeXML(xml);
	if (xml.BoolAttribute("renovated", false)) renovate();
}

void Attraction::renovate()
{
	renovated = true;
	// FUN_1000c290: the renovated art is RT_BITMAP 9998 with the same
	// 360x204 View bands.
	const YootPluginView * renovated = yootPluginView(game->app, defaultPlugin(), sf::Color::Transparent, 9998);
	if (renovated && view)
	{
		view = &renovated->texture;
		sprite.SetImage(renovated->texture);
		updateSprite();
	}
}

// ------------------------------------------------------------------ subway

void Subway::init()
{
	Metro::init();
	// Replace the SimTower station art with Subway.t2p RT_BITMAP 1000
	// (four 320x96 View bands, bottom-aligned on the station's floor).
	removeSprite(&station);
	removeSprite(&platform);
	if (const YootPluginView * v = yootPluginView(game->app, "Subway.t2p"))
	{
		view = &v->texture;
		viewSprite.SetImage(v->texture);
		const int lift = game->skin == Skin::Yoot ? kYootInteriorLift : 0;
		viewSprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36 - lift)});
		addSprite(&viewSprite);
	}
	level = 1;
	pattern = -1;
	setPattern(0);
}

void Subway::setPattern(int p)
{
	if (p == pattern) return;
	pattern = p;
	if (!view) return;
	const int bandH = static_cast<int>(view->getSize().y) / 4;
	const int b = std::max(0, std::min(3, p));
	viewSprite.setTextureRect(sf::IntRect({0, b * bandH}, {static_cast<int>(view->getSize().x), bandH}));
	viewSprite.setOrigin({0.f, static_cast<float>(bandH)});
}

void Subway::advance(double dt)
{
	// VenueSystem drives the timetable; visitors keep Metro's dwell and
	// return logic (no per-passenger income, as in Subway.t2p).
	(void)dt;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	if (day != lastVisitorDay) { lastVisitorDay = day; visitorsToday = 0; }
	for (std::set<Visitor *>::iterator it = visitors.begin(); it != visitors.end(); ++it)
	{
		Visitor * v = *it;
		if (v->state == Person::kCommuting && v->at && v->at != this) v->state = Person::kShopping;
		if (v->state == Person::kShopping && game->time.absolute >= v->departTime)
		{
			Item * from = v->at ? v->at : game->mainLobby;
			const Route & r = game->findRoute(from, this);
			if (!r.empty()) { v->state = Person::kReturning; v->goingTo = "Subway"; v->journey.set(r); }
			else v->returnedToMetro = true;
		}
	}
	int inside = 0;
	for (std::set<Visitor *>::iterator it = visitors.begin(); it != visitors.end(); ++it)
		if ((*it)->at == this) ++inside;
	const int cap = prototype->yoot && prototype->yoot->capacity > 0 ? prototype->yoot->capacity : 100;
	setPattern(Yoot::subwayPattern(open, inside, cap));
}

void Subway::trainArrives(int riders)
{
	trainPresent = true;
	// With the visitor pool, riders are pool people of transport 3 called by
	// the tenants while the train is in (CheckRouteFromPool bit 8).
	if (game->eventSystem.people.enabled() && !game->eventSystem.people.pool.empty()) return;
	// CheckRouteFromPool 0x10060bf0: subway riders are pool visitors bound
	// for any tenant reachable from the station (not only underground ones,
	// as for the SimTower metro).
	static const char * kinds[] = {"fastfood", "restaurant", "shop", "theater", "classictheater",
	                               "eventcenter", "gatheringarea", "stadium", "office"};
	std::vector<Item *> targets;
	for (const char * kind : kinds)
	{
		Game::ItemSetByString::const_iterator b = game->itemsByType.find(kind);
		if (b == game->itemsByType.end()) continue;
		for (Game::ItemSet::const_iterator it = b->second.begin(); it != b->second.end(); ++it)
			if (!(*it)->underConstruction && !game->findRoute(this, *it).empty()) targets.push_back(*it);
	}
	if (targets.empty()) return;
	for (int i = 0; i < riders && visitorsToday < 240; ++i)
	{
		Item * dest = targets[rand() % targets.size()];
		Visitor * v = new Visitor(this);
		v->arrivalTime = game->time.absolute;
		v->departTime = game->time.absolute + Time::hourToAbsolute(0.25 + (rand() % 31) / 60.0);
		v->state = Person::kCommuting;
		v->goingTo = dest->prototype->name;
		v->from = "Subway";
		v->journey.set(game->findRoute(this, dest));
		visitors.insert(v);
		++visitorsToday;
	}
	LOG(DEBUG, "subway train: %d riders, %zu in the tower", riders, visitors.size());
}

void Subway::trainLeaves()
{
	trainPresent = false;
	boardReturnedVisitors();
}

void Subway::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Metro::encodeXML(xml);
	xml.PushAttribute("level", level);
}

void Subway::decodeXML(tinyxml2::XMLElement & xml)
{
	Metro::decodeXML(xml);
	level = std::max(1, std::min(Yoot::kSubwayMaxLevel, xml.IntAttribute("level", 1)));
	pattern = -1;
	setPattern(0);
}
