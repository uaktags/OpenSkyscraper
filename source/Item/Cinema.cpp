#include "../Game.h"
#include "../XmlUtil.h"
#include "../Math/Rand.h"
#include "Cinema.h"
#include <string>
#include <algorithm>
#include <cmath>

using namespace OT;
using namespace OT::Item;

namespace
{
	/** The EXE never requests soundtracks for films 3 and 6 (sound ids 9004 /
	 *  9007); those screenings run silent even though the audio ships in the
	 *  resources. */
	bool movieHasSoundtrack(int movieType)
	{
		return movieType != 3 && movieType != 6;
	}
}

Cinema::~Cinema()
{
	// Deregister from game->playingSounds before the sf::Sound member dies.
	soundtrack.Stop();
	clearCustomers();
}

void Cinema::init()
{
	Item::init();
	open = false;
	playing = false;
	intermission = false;
	movieType = rand() % 14; // B12: film 14 is unreachable in the EXE.
	movieAgeDays = 0;
	quotaMatinee = quotaEvening = patronsToday = 0;
	eveningShow = false;
	revenueSettled = false;
	soundtrackPlaying = false;
	animation = 0;
	animationFrame = 0;
	hallSprite.setTexture(App->bitmaps["simtower/cinema/hall"]);
	hallSprite.setOrigin({0.f, 60.f});
	hallSprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});
	screenSprite.setTexture(App->bitmaps["simtower/cinema/screens"]);
	screenSprite.setOrigin({0.f, 60.f});
	screenSprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});
	addSprite(&hallSprite);
	addSprite(&screenSprite);
	spriteNeedsUpdate = true;
	updateSprite();
}

void Cinema::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("open", open);
	xml.PushAttribute("playing", playing);
	xml.PushAttribute("intermission", intermission);
	xml.PushAttribute("movie", movieType);
	xml.PushAttribute("movieAgeDays", movieAgeDays);
	xml.PushAttribute("quotaMatinee", quotaMatinee);
	xml.PushAttribute("quotaEvening", quotaEvening);
	xml.PushAttribute("patronsToday", patronsToday);
	xml.PushAttribute("eveningShow", eveningShow);
	xml.PushAttribute("revenueSettled", revenueSettled);
	for (Customers::iterator c = customers.begin(); c != customers.end(); ++c) {
		Customer *customer = *c;
		if (customer->at != this) continue;
		xml.OpenElement("customer");
		xml.PushAttribute("evening", customer->evening);
		xml.PushAttribute("type", customer->type);
		xml.PushAttribute("state", customer->state);
		xml.PushAttribute("stress", customer->stress);
		xml.PushAttribute("eval", customer->eval);
		xml.PushAttribute("name", customer->name.c_str());
		xml.PushAttribute("from", customer->from.c_str());
		xml.PushAttribute("goingTo", customer->goingTo.c_str());
		xml.PushAttribute("bankCount", (int)customer->bankCount);
		xml.PushAttribute("bankTotal", (int)customer->bankTotal);
		xml.CloseElement();
	}
}

void Cinema::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open", false);
	playing = xml.BoolAttribute("playing", false);
	intermission = xml.BoolAttribute("intermission", false);
	movieType = xml.IntAttribute("movie", 0);
	if (movieType < 0 || movieType >= 14) movieType = 0;
	movieAgeDays = std::max(0, xml.IntAttribute("movieAgeDays", 0));
	quotaMatinee = std::max(0, xml.IntAttribute("quotaMatinee", 0));
	quotaEvening = std::max(0, xml.IntAttribute("quotaEvening", 0));
	patronsToday = std::max(0, xml.IntAttribute("patronsToday", 0));
	eveningShow = xml.BoolAttribute("eveningShow", false);
	revenueSettled = xml.BoolAttribute("revenueSettled", false);
	const int savedPatronsToday = patronsToday;
	clearCustomers();
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("customer"); e; e = e->NextSiblingElement("customer")) {
		Customer *c = new Customer(this);
		c->evening = e->BoolAttribute("evening", false);
		c->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		c->state = (Person::State)e->IntAttribute("state", Person::kShopping);
		c->stress = e->DoubleAttribute("stress", 0.0);
		c->eval = e->DoubleAttribute("eval", 0.0);
		c->name = xmlAttrOr(*e, "name", "");
		c->from = xmlAttrOr(*e, "from", "");
		c->goingTo = xmlAttrOr(*e, "goingTo", "");
		c->bankCount = (int8_t)e->IntAttribute("bankCount", 0);
		c->bankTotal = (int16_t)e->IntAttribute("bankTotal", 0);
		customers.insert(c);
		if (open) addPerson(c);
	}
	patronsToday = savedPatronsToday;
	updateSprite();
	// A mid-screening save resumes its soundtrack on load.
	updateSoundtrack();
}

void Cinema::updateSprite()
{
	spriteNeedsUpdate = false;
	int hallIndex = 0;
	int screenIndex = 0;
	if (open)
	{
		if (playing)
		{
			if (intermission)
			{
				hallIndex = 2;
				screenIndex = 2;
			}
			else
			{
				hallIndex = 3 + animationFrame;
				screenIndex = 3 + movieType;
			}
		}
		else
		{
			// Doors-open seating state: empty, then occupied.
			hallIndex = (people.size() > 0 ? 2 : 1);
			screenIndex = hallIndex;
		}
	}
	hallSprite.setTextureRect(sf::IntRect({hallIndex * 192, 0}, {192, 60}));
	// hallSprite.resize(192, 60);
	screenSprite.setTextureRect(sf::IntRect({screenIndex * 56, 0}, {56, 60}));
	// screenSprite.resize(56, 60);
}

void Cinema::advance(double dt)
{
	if (game->time.checkHour(10)) {
		if (movieAgeDays < 127) ++movieAgeDays;
		patronsToday = 0;
		quotaMatinee = (movieType >= 7 ? (movieAgeDays / 3 >= 3 ? 20 : movieAgeDays / 3 == 2 ? 40 : 60)
			: (movieAgeDays / 3 >= 3 ? 20 : 40));
		quotaEvening = quotaMatinee;
		// CountT cinema category (T14): daily reset, then quota sum per hall.
		game->resetCategoryOnce(TenantRules::kCountCinema);
		game->addPopulation(TenantRules::kCountCinema, quotaMatinee + quotaEvening);
		open = false;
		playing = false;
		intermission = false;
		eveningShow = false;
		revenueSettled = false;
		clearCustomers();
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(12)) {
		open = true;
		playing = false;
		eveningShow = false;
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(13) && open) {
		playing = true;
		intermission = false;
		eveningShow = false;
		for (int i = 0; i < quotaMatinee && customers.size() < static_cast<size_t>(quotaMatinee + quotaEvening); ++i) {
			Customer *c = new Customer(this);
			c->evening = false;
			customers.insert(c);
			if (!lobbyRoute.empty()) c->journey.set(lobbyRoute);
		}
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(15) && open) {
		for (int i = 0; i < quotaEvening; ++i) {
			Customer *c = new Customer(this);
			c->evening = true;
			customers.insert(c);
			if (!lobbyRoute.empty()) c->journey.set(lobbyRoute);
		}
		playing = false;
		eveningShow = true;
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(16) && open) {
		playing = false;
		for (Customers::iterator c = customers.begin(); c != customers.end(); ++c) {
			Customer *p = *c;
			if (p->evening || p->at != this) continue;
			removePerson(p);
			p->state = Person::kReturning;
			p->goingTo = "Exit";
			const Route &r = game->findRoute(this, game->mainLobby);
			if (!r.empty()) p->journey.set(r);
		}
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(17) && open) {
		playing = true;
		eveningShow = true;
		intermission = false;
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(20) && open) {
		playing = false;
		open = false;
		intermission = false;
		spriteNeedsUpdate = true;
		if (!revenueSettled) {
			int amount = patronsToday < 40 ? 0 : patronsToday < 80 ? 2000 : patronsToday < 100 ? 10000 : 15000;
			if (amount > 0 && !game->isDisasterOfferDay()) game->transferFunds(amount, "entertainment_income", "Income from Movie Theatre");
			revenueSettled = true;
		}
		const Route &r = game->findRoute(this, game->mainLobby);
		for (Customers::iterator c = customers.begin(); c != customers.end(); ++c) {
			Customer *p = *c;
			if (p->at != this) continue;
			removePerson(p);
			p->state = Person::kReturning;
			p->goingTo = "Exit";
			if (!r.empty()) p->journey.set(r);
		}
	}
	if (game->time.checkHour(16) || game->time.checkHour(22)) {
		if (open && playing && !intermission) {
			intermission = true;
			game->playOnce("simtower/doorbell");
		}
	}
	animation = fmod(animation + dt, 1);
	int af = floor(animation * 2);
	if (af != animationFrame) { animationFrame = af; spriteNeedsUpdate = true; }
	if (spriteNeedsUpdate) updateSprite();
	updateSoundtrack();
}

Path Cinema::getRandomBackgroundSoundPath()
{
	if (!open || !playing || intermission || !movieHasSoundtrack(movieType))
		return "";
	char name[128];
	snprintf(name, 128, "simtower/cinema/movie%i", movieType);
	return name;
}

/** Keeps the looping movie soundtrack in sync with the show state: the EXE
 *  requests sound 9001+movie_id when a screening starts and silences it at
 *  the intermission and at closing. */
void Cinema::updateSoundtrack()
{
	const bool wantSound = open && playing && !intermission && movieHasSoundtrack(movieType);
	if (wantSound == soundtrackPlaying) return;
	if (wantSound) {
		char name[128];
		snprintf(name, 128, "simtower/cinema/movie%i", movieType);
		soundtrack.setBuffer(app->sounds[name]);
		soundtrack.setLooping(true);
		soundtrack.Play(game);
	} else {
		soundtrack.Stop();
	}
	soundtrackPlaying = wantSound;
}

void Cinema::addPerson(Person *p)
{
	if (patronsToday >= quotaMatinee + quotaEvening) {
		Item::addPerson(p);
		Item::removePerson(p);
		p->state = Person::kReturning;
		p->goingTo = "Exit";
		if (game->mainLobby) {
			const Route &r = game->findRoute(this, game->mainLobby);
			if (!r.empty()) p->journey.set(r);
		}
		return;
	}
	Item::addPerson(p);
	p->state = Person::kShopping;
	p->eval = 60;
	++patronsToday;
	spriteNeedsUpdate = true;
}

void Cinema::removePerson(Person *p)
{
	Item::removePerson(p);
	spriteNeedsUpdate = true;
}

Cinema::Customer::Customer(Cinema *item)
	: Person(item->game)
{
	evening = false;
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	from = "City";
	goingTo = item->prototype->name;
}

/** Removes all customers from the item. */
void Cinema::clearCustomers()
{
	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
		delete *c;
	customers.clear();
}

void Cinema::evacuateForDisaster()
{
 clearCustomers();
 open = playing = intermission = false;
 quotaMatinee = quotaEvening = patronsToday = 0;
 revenueSettled = true;
 soundtrack.Stop(); soundtrackPlaying = false;
 spriteNeedsUpdate = true;
 updateSprite();
}
