#include "../Game.h"
#include "../XmlUtil.h"
#include "../Math/Rand.h"
#include "../NameManager.h"
#include "../Time.h"
#include "Metro.h"
#include "../XmlUtil.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <string>

using namespace OT;
using namespace OT::Item;
using OT::Math::randd;

// SIMTOWER.EXE runs 2600 simulation frames per day. From 10:00 (frame 240)
// until the 17:00 close every frame rolls 1 in 100 to toggle the train in or out.
static const double kFramesPerDay      = 2600.0;
static const int    kTrainToggleOdds   = 100;
static const double kTrainFirstHour    = 10.0;

// Dwell is owned by the venue (8 or 9 pumps of 8 frames = 64/72 frames).


Metro::~Metro()
{
	clearAllVisitors();
}

void Metro::init()
{
	Item::init();
	// MakeMetroStation seeds the state with the day phase: built from 17:00
	// until 07:00 the station starts on the closed frame.
	open = game->time.hour >= 7.0 && game->time.hour < 17.0;
	trainPresent = false;
	trainSound.setBuffer(App->sounds["simtower/metro"]);
	visitorsToday = 0;
	for (bool &used : slotUsed) used = false;
	lastVisitorDay = static_cast<int>(std::floor(game->time.absolute));
	station.setTexture(App->bitmaps["simtower/metro/station"]);
	// The 3-floor station art is one 240x96 column (3*36 - 12, like the
	// 2-floor cinema/party hall's 60px): the 66px concourse stacks directly
	// on top of the 30px platform, which sits on the bottom (track) floor.
	station.setOrigin({0.f, 66.f + 30.f});
	station.setPosition({0.f, 0.f});
	platform.setTexture(App->bitmaps["simtower/metro/station"]);
	platform.setOrigin({0.f, 30.f});
	platform.setPosition({0.f, 0.f});
	addSprite(&station);
	addSprite(&platform);
	spriteNeedsUpdate = true;
	if (game->metroStation != NULL) {
		LOG(WARNING, "Metro::init() called while metroStation already set; keeping the existing station");
	} else game->metroStation = this;
	updateSprite();
}

void Metro::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("open", open);
	xml.PushAttribute("trainPresent", trainPresent);
	xml.PushAttribute("visitorsToday", visitorsToday);
	xml.PushAttribute("lastVisitorDay", lastVisitorDay);
	{ std::string slots; for (bool used : slotUsed) slots += used ? '1' : '0'; xml.PushAttribute("slots", slots.c_str()); }
	for (std::set<Visitor *, SimLess>::iterator it = visitors.begin(); it != visitors.end(); ++it) {
		Visitor *v = *it;
		xml.OpenElement("visitor");
		xml.PushAttribute("arrivalTime", v->arrivalTime);
		xml.PushAttribute("departTime", v->departTime);
		xml.PushAttribute("returnedToMetro", v->returnedToMetro);
		xml.PushAttribute("type", v->type);
		xml.PushAttribute("state", v->state);
		xml.PushAttribute("stress", v->stress);
		xml.PushAttribute("bankCount", int(v->bankCount));
		xml.PushAttribute("bankTotal", int(v->bankTotal));
		xml.PushAttribute("slot", v->slot);
		xml.PushAttribute("eval", v->eval);
		xml.PushAttribute("name", v->name.c_str());
		xml.PushAttribute("from", v->from.c_str());
		xml.PushAttribute("goingTo", v->goingTo.c_str());
		xml.CloseElement();
	}
}

void Metro::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open", true);
	trainPresent = xml.BoolAttribute("trainPresent", false);
	visitorsToday = std::max(0, xml.IntAttribute("visitorsToday", 0));
	lastVisitorDay = xml.IntAttribute("lastVisitorDay", static_cast<int>(std::floor(game->time.absolute)));
	clearAllVisitors();
	{
		std::string slots = xmlAttrOr(xml, "slots", "");
		for (int k = 0; k < kPoolSize; ++k) slotUsed[k] = k < int(slots.size()) && slots[k] == '1';
	}
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("visitor"); e; e = e->NextSiblingElement("visitor")) {
		Visitor *v = new Visitor(this);
		v->arrivalTime = e->DoubleAttribute("arrivalTime");
		v->departTime = e->DoubleAttribute("departTime");
		v->returnedToMetro = e->BoolAttribute("returnedToMetro", false);
		v->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		v->state = (Person::State)e->IntAttribute("state", Person::kWandering);
		v->stress = e->DoubleAttribute("stress", 0.0);
		v->bankCount = e->IntAttribute("bankCount", 0);
		v->bankTotal = e->IntAttribute("bankTotal", 0);
		v->slot = e->IntAttribute("slot", -1);
		v->member = v->slot;
		v->eval = e->DoubleAttribute("eval", 0.0);
		v->name = xmlAttributeOr(*e, "name", "");
		v->from = xmlAttributeOr(*e, "from", "");
		v->goingTo = xmlAttributeOr(*e, "goingTo", "");
		visitors.insert(v);
	}
	updateSprite();
}

void Metro::updateSprite()
{
	spriteNeedsUpdate = false;
	// SIMTOWER.EXE draws all three station floors from one frame index, the
	// cell state: 0 open / no train, 1 closed, 2 train in (crowd on the
	// concourse, train on the platform). Sheet columns: 0 train, 1 open, 2 closed.
	const int frame = !open ? 2 : (trainPresent ? 0 : 1);

	station.setTextureRect(sf::IntRect({frame * 240, 0}, {240, 66}));
	platform.setTextureRect(sf::IntRect({frame * 240, 66}, {240, 30}));
	station.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
	platform.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
}

OT::Item::Item * Metro::pickDestination()
{
	// Zone 0 list of a random kind (shop 0, restaurant 1, fast food 2).
	const std::vector<OT::Item::Item *> & list = game->venueLists[0][rand() % 3];
	if (list.empty()) return NULL;
	OT::Item::Item * dest = list[rand() % list.size()];
	if (!game->items.count(dest) || game->findRoute(this, dest).empty()) return NULL;
	return dest;
}

void Metro::rollPool()
{
	if (lobbyRoute.empty()) return;
	for (int k = 0; k < kPoolSize; ++k) {
		if (slotUsed[k] || rand() % 36 != 0) continue;
		slotUsed[k] = true; // consumed until the day reset unless the rider returns
		++visitorsToday;
		Item *dest = pickDestination();
		if (!dest) continue;
		const Route &r = game->findRoute(this, dest);
		if (r.empty()) continue;
		Visitor *v = new Visitor(this);
		v->slot = k;
		v->member = k;
		v->arrivalTime = game->time.absolute;
		v->state = Person::kCommuting;
		v->goingTo = dest->prototype->name;
		v->from = "Metro";
		v->journey.set(r);
		visitors.insert(v);
	}
}

void Metro::boardReturnedVisitors()
{
	// Metro passenger travel is a visitor pool, not a fare source in the EXE.
	// Boarding removes only visitors that actually reached the platform.
	for (std::set<Visitor *, SimLess>::iterator it = visitors.begin(); it != visitors.end();) {
		Visitor *v = *it;
		if (v->returnedToMetro || v->at == this) {
			delete v;
			it = visitors.erase(it);
		} else ++it;
	}
}

void Metro::clearAllVisitors()
{
	for (std::set<Visitor *, SimLess>::iterator it = visitors.begin(); it != visitors.end(); ++it)
		delete *it;
	visitors.clear();
}

void Metro::advance(double dt)
{
	(void)dt;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	if (day != lastVisitorDay) {
		lastVisitorDay = day;
		visitorsToday = 0;
		for (bool &used : slotUsed) used = false;
	}
	// SimTower opens the station at frame 0 (07:00) and closes it at frame 1600
	// (17:00); trains only run from 10:00 until closing.
	if (game->time.checkHour(7)) { open = true; spriteNeedsUpdate = true; }
	if (game->time.checkHour(17) && open) {
		open = false;
		spriteNeedsUpdate = true;
		if (trainPresent) {
			trainPresent = false;
			spriteNeedsUpdate = true;
		}
	}
	if (open) {
		// The toggle's first gate is the fire/bomb flag: no trains during an emergency.
		if (game->time.hour >= kTrainFirstHour && !game->emergencyActive()) {
			const long frames = game->time.ticksCrossed(1.0 / kFramesPerDay);
			for (long i = 0; i < frames; ++i) {
				if (rand() % kTrainToggleOdds != 0) continue;
				if (!trainPresent) { trainPresent = true; spriteNeedsUpdate = true; trainSound.Play(game); }
				else { trainPresent = false; spriteNeedsUpdate = true; boardReturnedVisitors(); }
			}
		}
		// 10:00-17:00: every free slot rolls 1/36 per 16-frame visit. No fare
		// is charged and no commuter population is fabricated.
		if (game->time.hour >= 10.0 && game->time.hour < 17.0 && !game->emergencyActive()) {
			const long visits = visitsThisTick(game, 16, 0);
			for (long v = 0; v < visits; ++v) rollPool();
		}
	}
	if (spriteNeedsUpdate) updateSprite();
}

void Metro::addPerson(Person *p)
{
	Item::addPerson(p);
	Visitor *v = dynamic_cast<Visitor *>(p);
	if (v && v->state == Person::kReturning) {
		v->returnedToMetro = true;
		v->state = Person::kIdle;
		if (v->slot >= 0 && v->slot < kPoolSize) slotUsed[v->slot] = false; // recycle the same day
		if (visitorsToday > 0) --visitorsToday;
	}
}

void Metro::removePerson(Person *p)
{
	Item::removePerson(p);
}

Metro::Visitor::Visitor(Metro *m)
	: Person(m->game)
{
	station = m;
	arrivalTime = 0;
	departTime = 0;
	returnedToMetro = false;
	Type types[] = {kMan, kWoman1, kWoman2, kChild};
	type = types[rand() % 4];
	from = "Metro";
	goingTo = "Tower";
}

void Metro::evacuateForDisaster()
{
 clearAllVisitors(); open = true; trainPresent = false; visitorsToday = 0; for (bool &used : slotUsed) used = false;
 trainSound.Stop();
 spriteNeedsUpdate = true;
 updateSprite();
}
