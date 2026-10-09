/* Copyright © 2013 Fabian Schuiki */
#include "../Game.h"
#include "../Math/Rand.h"
#include "Restaurant.h"
#include "Hotel.h"
#include <string>
#include <algorithm>
#include <cmath>

using namespace OT;
using namespace OT::Item;

// Authentic tenant name pool (OpenSkyWeb ISSUE-035, SIMTOWER.EXE strings
// 0xbaa02-0xbaa3d). The original has five variants and the loader now
// carries all five rows, including the Steak House row.
const char * const Restaurant::kNameVariants[5] = {
	"English Pub", "French Restaurant", "Chinese Restaurant", "Sushi Bar",
	"Steak House"
};

Restaurant::~Restaurant()
{
	clearCustomers();
}

void Restaurant::init()
{
	Item::init();
	variant = rand() % 5;
	displayName = kNameVariants[variant];
	open = false;
	pricePerMeal = 400;
	serviceScore[0] = serviceScore[1] = serviceScore[2] = 10;
	dailyQuota = quotaRemaining = walkinsToday = customersToday = 0;
	walkinsYesterday = 0;
	lastDemandDay = -1;
	revenueSettled = false;
	sprite.setTexture(App->bitmaps.editionTexture("restaurant", "simtower/restaurant"));
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;
	updateSprite();
}

void Restaurant::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("open", open);
	xml.PushAttribute("pricePerMeal", pricePerMeal);
	xml.PushAttribute("population", population);
	xml.PushAttribute("dailyQuota", dailyQuota);
	xml.PushAttribute("quotaRemaining", quotaRemaining);
	xml.PushAttribute("walkinsToday", walkinsToday);
	xml.PushAttribute("walkinsYesterday", walkinsYesterday);
	xml.PushAttribute("customersToday", customersToday);
	xml.PushAttribute("lastDemandDay", lastDemandDay);
	xml.PushAttribute("revenueSettled", revenueSettled);
	for (int i = 0; i < 3; ++i) {
		char key[32]; snprintf(key, sizeof(key), "serviceScore%d", i);
		xml.PushAttribute(key, serviceScore[i]);
	}
	for (Customers::iterator c = customers.begin(); c != customers.end(); c++) {
		Customer *customer = *c;
		CustomerMetadataMap::iterator m = customerMetadata.find(customer);
		bool eating = (m != customerMetadata.end());
		bool arriving = (!eating && customer->at == NULL && customer->state == Person::kWandering);
		if (!eating && !arriving) continue;
		xml.OpenElement("customer");
		xml.PushAttribute("lobbyArrival", customer->arrivalTime);
		xml.PushAttribute("type", customer->type);
		xml.PushAttribute("state", customer->state);
		xml.PushAttribute("stress", customer->stress);
		xml.PushAttribute("eval", customer->eval);
		xml.PushAttribute("name", customer->name.c_str());
		xml.PushAttribute("from", customer->from.c_str());
		xml.PushAttribute("goingTo", customer->goingTo.c_str());
		xml.PushAttribute("phase", eating ? "eating" : "arriving");
		if (eating) xml.PushAttribute("itemArrival", m->second.arrivalTime);
		xml.CloseElement();
	}
}

void Restaurant::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant", 0);
	if (variant < 0 || variant >= 5) variant = 0;
	displayName = kNameVariants[variant];
	open = xml.BoolAttribute("open", false);
	pricePerMeal = xml.IntAttribute("pricePerMeal", 400);
	population = std::max(0, xml.IntAttribute("population", 0));
	dailyQuota = std::max(0, xml.IntAttribute("dailyQuota", 0));
	quotaRemaining = std::max(0, xml.IntAttribute("quotaRemaining", dailyQuota));
	walkinsToday = std::max(0, xml.IntAttribute("walkinsToday", 0));
	walkinsYesterday = std::max(0, xml.IntAttribute("walkinsYesterday", 0));
	customersToday = std::max(0, xml.IntAttribute("customersToday", 0));
	lastDemandDay = xml.IntAttribute("lastDemandDay", -1);
	revenueSettled = xml.BoolAttribute("revenueSettled", false);
	for (int i = 0; i < 3; ++i) {
		char key[32]; snprintf(key, sizeof(key), "serviceScore%d", i);
		serviceScore[i] = std::max(0, xml.IntAttribute(key, 10));
	}
	const int savedCustomersToday = customersToday;
	const int savedServiceScore[3] = {serviceScore[0], serviceScore[1], serviceScore[2]};
	clearCustomers();
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("customer"); e; e = e->NextSiblingElement("customer")) {
		Customer *c = new Customer(this);
		c->arrivalTime = e->DoubleAttribute("lobbyArrival");
		c->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		c->state = (Person::State)e->IntAttribute("state", Person::kWandering);
		c->stress = e->DoubleAttribute("stress", 0.0);
		c->eval = e->DoubleAttribute("eval", 0.0);
		c->name = e->Attribute("name", "");
		c->from = e->Attribute("from", "");
		c->goingTo = e->Attribute("goingTo", "");
		customers.insert(c);
		if (std::string(e->Attribute("phase", "arriving")) == "eating") {
			addPerson(c);
			customerMetadata[c].arrivalTime = e->DoubleAttribute("itemArrival");
		} else arrivingCustomers.push(c);
	}
	customersToday = savedCustomersToday;
	serviceScore[0] = savedServiceScore[0];
	serviceScore[1] = savedServiceScore[1];
	serviceScore[2] = savedServiceScore[2];
	updateSprite();
}

void Restaurant::updateSprite()
{
	spriteNeedsUpdate = false;
	// EXE venue state byte (UpdateVenueBusyTier 11a8:0bd5): 0 empty,
	// 1 busy (1-9 inside), 2 packed (10+), 3 closed shutters.
	int index = 3;
	if (open)
		index = (people.size() >= 10) ? 2 : (people.size() >= 1) ? 1 : 0;
	sprite.setTextureRect(sf::IntRect({index * 192, variant * 24}, {192, 24}));
}

void Restaurant::advance(double dt)
{
	(void)dt;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	const int period = game->sky.rainyDay ? 2 : (game->time.isHoliday() ? 1 : 0);
	const int cap = period == 0 ? 35 : period == 1 ? 50 : 25;
	if (game->time.checkHour(17)) {
		open = true;
		revenueSettled = false;
		clearCustomers();
		walkinsYesterday = walkinsToday;
		population = walkinsYesterday;
		customersToday = walkinsToday = 0;
		int score = serviceScore[period];
		if (period == 1) score = std::max(score, serviceScore[0]);
		dailyQuota = std::max(10, std::min(cap, score));
		quotaRemaining = dailyQuota;
		lastDemandDay = day;
		serviceScore[period] = 0;
		spriteNeedsUpdate = true;
	}
	if (open && quotaRemaining > 0 && customers.size() < 40 &&
		game->time.checkTick(16.0 / 2600.0)) {
		const double hour = game->time.hour;
		int width = (hour >= 17.0 && hour < 21.0) ? 12 :
			(hour >= 21.0 && hour < 23.0) ? 1 : 0;
		if (width == 1 || (width > 0 && rand() % width == 0)) {
			Customer *c = new Customer(this);
			c->arrivalTime = game->time.absolute;
			customers.insert(c);
			arrivingCustomers.push(c);
			--quotaRemaining;
			++walkinsToday;
		}
	}
	if (game->time.checkHour(23) && open) {
		open = false;
		population = walkinsToday;
		game->populationNeedsUpdate = true;
		spriteNeedsUpdate = true;
		if (!revenueSettled) {
			int amount = customersToday >= 50 ? 10000 : customersToday >= 35 ? 6000
				: customersToday >= 25 ? 4000 : -6000;
			if (!game->isDisasterOfferDay())
				game->transferFunds(amount, "retail_income", "Income from Restaurant");
			revenueSettled = true;
		}
	}
	while (!arrivingCustomers.empty()) {
		Customer *c = arrivingCustomers.top();
		if (game->time.absolute < c->arrivalTime) break;
		arrivingCustomers.pop();
		if (open && game->time.hour < 23.0 && !lobbyRoute.empty()) c->journey.set(lobbyRoute);
		else { customers.erase(c); delete c; }
	}
	for (std::list<Person *>::iterator ip = eatingCustomers.begin(); ip != eatingCustomers.end();) {
		Person *p = *ip;
		CustomerMetadata &m = customerMetadata[p];
		if (game->time.absolute >= m.arrivalTime + 20 * Time::kBaseSpeed || !open) {
			Route r;
			Hotel::Guest *guest = dynamic_cast<Hotel::Guest *>(p);
			if (guest && guest->hotel) r = game->findRoute(this, guest->hotel);
			else r = game->findRoute(this, game->mainLobby);
			if (r.empty()) { ++ip; continue; }
			ip = eatingCustomers.erase(ip);
			removePerson(p);
			p->state = Person::kReturning;
			p->from = prototype->name;
			p->goingTo = guest && guest->hotel ? guest->hotel->prototype->name : "Exit";
			p->journey.set(r);
		} else ++ip;
	}
	if (spriteNeedsUpdate) updateSprite();
}

void Restaurant::addPerson(Person *p)
{
	if (people.size() >= 40) {
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
	p->eval = 50;
	CustomerMetadata &m = customerMetadata[p];
	m.arrivalTime = game->time.absolute;
	eatingCustomers.push_back(p);
	++customersToday;
	const int period = game->sky.rainyDay ? 2 : (game->time.isHoliday() ? 1 : 0);
	serviceScore[period] = std::min(50, serviceScore[period] + 2);
	spriteNeedsUpdate = true;
}

void Restaurant::removePerson(Person *p)
{
	Item::removePerson(p);
	eatingCustomers.remove(p);
	customerMetadata.erase(p);
	spriteNeedsUpdate = true;
}

void Restaurant::clearCustomers()
{
	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
		delete *c;
	while (!arrivingCustomers.empty())
		arrivingCustomers.pop();
	eatingCustomers.clear();
	customers.clear();
	customerMetadata.clear();
}

Restaurant::Customer::Customer(Restaurant *item)
	: Person(item->game)
{
	arrivalTime = 0;
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	from = "City";
	goingTo = item->prototype->name;
}

Path Restaurant::getRandomBackgroundSoundPath()
{
	if (!open)
		return "";
	return "simtower/restaurant";
}

void Restaurant::evacuateForDisaster()
{
 clearCustomers();
 open = false;
 quotaRemaining = customersToday = walkinsToday = 0;
 revenueSettled = true;
 spriteNeedsUpdate = true;
 updateSprite();
}
