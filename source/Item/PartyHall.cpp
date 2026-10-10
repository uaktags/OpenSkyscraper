#include "../Game.h"
#include "../Math/Rand.h"
#include "PartyHall.h"
#include <cmath>
#include <algorithm>

using namespace OT;
namespace OT { namespace Item {

PartyHall::~PartyHall()
{
	clearVisitors();
}

void PartyHall::init()
{
	Item::init();
	open = false;
	quota = 50;
	patronsToday = 0;
	eventSettled = false;
	sprite.setTexture(App->bitmaps.editionTexture("partyhall", "simtower/partyhall"));
	sprite.setOrigin({0.f, 60.f});
	sprite.setPosition({0.f, 0.f});
	addSprite(&sprite);
	spriteNeedsUpdate = false;
	updateSprite();
}

void PartyHall::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("open", open);
	xml.PushAttribute("quota", quota);
	xml.PushAttribute("patronsToday", patronsToday);
	xml.PushAttribute("eventSettled", eventSettled);
}

void PartyHall::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open", false);
	quota = std::max(1, xml.IntAttribute("quota", 50));
	patronsToday = std::max(0, xml.IntAttribute("patronsToday", 0));
	eventSettled = xml.BoolAttribute("eventSettled", false);
	updateSprite();
}

void PartyHall::updateSprite()
{
	spriteNeedsUpdate = false;
	int index = 0;
	if (open)
	{
		// Lit hall; the third frame adds the buffet and stanchions once
		// visitors have actually arrived.
		index = people.empty() ? 1 : 2;
	}
	sprite.setTextureRect(sf::IntRect({index * 192, 0}, {192, 60}));
	sprite.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
}

void PartyHall::advance(double dt)
{
	(void)dt;
	// 1188:05af, all.c:21102-21123: party=0+50 in the 10:00
	// entertainment table rebuild. Opening the doors at 13:00 adds nobody.
	if (!prototype->yoot && game->time.checkHour(10)) {
		quota = 50;
		game->resetCategoryOnce(TenantRules::kCountParty);
		game->addPopulation(TenantRules::kCountParty, quota);
	}
	if (game->time.checkHour(13)) {
		open = true;
		quota = 50;
		if (prototype->yoot) {
			game->resetCategoryOnce(TenantRules::kCountParty);
			game->addPopulation(TenantRules::kCountParty, quota);
		}
		patronsToday = 0;
		eventSettled = false;
		clearVisitors();
		if (!lobbyRoute.empty()) {
			for (int i = 0; i < quota; ++i) {
				Visitor *v = new Visitor(this);
				visitors.insert(v);
				v->journey.set(lobbyRoute);
			}
		}
		spriteNeedsUpdate = true;
	}
	if (game->time.checkHour(17) && open) {
		open = false;
		spriteNeedsUpdate = true;
		if (!eventSettled) {
			if (patronsToday > 0 && !game->isDisasterOfferDay())
				game->transferFunds(20000, "entertainment_income", "Income from Party Hall");
			eventSettled = true;
		}
		const Route &r = game->findRoute(this, game->mainLobby);
		for (Visitors::iterator it = visitors.begin(); it != visitors.end(); ++it) {
			Visitor *v = *it;
			if (v->at == this) removePerson(v);
			v->state = Person::kReturning;
			v->from = prototype->name;
			v->goingTo = "Exit";
			if (!r.empty()) v->journey.set(r);
		}
	}
	if (spriteNeedsUpdate) updateSprite();
}

Path PartyHall::getRandomBackgroundSoundPath()
{
	if (!open)
		return "";
	return "simtower/partyhall";
}

void PartyHall::addPerson(Person *p)
{
	if (patronsToday >= quota) {
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

void PartyHall::removePerson(Person *p)
{
	Item::removePerson(p);
	spriteNeedsUpdate = true;
}

PartyHall::Visitor::Visitor(PartyHall *hall)
	: Person(hall->game)
{
	Type types[] = {kMan, kWoman1, kWoman2, kChild, kWomanWithChild1};
	type = types[rand() % 5];
	from = "City";
	goingTo = hall->prototype->name;
}

void PartyHall::clearVisitors()
{
	for (Visitors::iterator it = visitors.begin(); it != visitors.end(); ++it)
		delete *it;
	visitors.clear();
}

} } // namespace OT::Item

void OT::Item::PartyHall::evacuateForDisaster()
{
 clearVisitors(); open = false; patronsToday = 0; eventSettled = true;
 spriteNeedsUpdate = true;
 updateSprite();
}
