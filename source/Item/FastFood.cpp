/* Copyright © 2013 Fabian Schuiki */
#include "../Game.h"
#include "../XmlUtil.h"
#include "../YootRules.h"
#include "../Math/Rand.h"
#include "FastFood.h"
#include "Office.h"
#include "Metro.h"
#include <string>
#include <algorithm>
#include <cmath>

using namespace OT;
using namespace OT::Item;

// Authentic tenant name pool (OpenSkyWeb ISSUE-035, SIMTOWER.EXE strings
// 0xbac01-0xbac37).
const char * const FastFood::kNameVariants[5] = {
	"Japanese Stall", "Chinese Cafe", "Hamburger Stand", "Ice Cream", "Coffee Shop"
};

FastFood::~FastFood()
{
	clearCustomers();
}

void FastFood::init()
{
	Item::init();

	variant = rand() % 5;
	displayName = kNameVariants[variant];
	open = false;
	pricePerMeal = 200;
	serviceScore[0] = serviceScore[1] = serviceScore[2] = 10;
	dailyQuota = quotaRemaining = walkinsToday = customersToday = 0;
	walkinsYesterday = 0;
	lastDemandDay = -1;
	revenueSettled = false;

	sprite.setTexture(App->bitmaps.editionTexture("fastfood", "simtower/fastfood"));
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;

	updateSprite();
}

void FastFood::encodeXML(tinyxml2::XMLPrinter &xml)
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
    std::string slots; for (bool used : dispatched) slots += used ? '1' : '0';
    xml.PushAttribute("dispatched",slots.c_str());
	for (int i = 0; i < 3; ++i) {
		char key[32];
		snprintf(key, sizeof(key), "serviceScore%d", i);
		xml.PushAttribute(key, serviceScore[i]);
	}

	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
	{
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
        xml.PushAttribute("bankCount", int(customer->bankCount));
        xml.PushAttribute("bankTotal", int(customer->bankTotal));
        xml.PushAttribute("poolSlot", customer->poolSlot);
		xml.PushAttribute("eval", customer->eval);
		xml.PushAttribute("name", customer->name.c_str());
		xml.PushAttribute("from", customer->from.c_str());
		xml.PushAttribute("goingTo", customer->goingTo.c_str());
		xml.PushAttribute("phase", eating ? "eating" : "arriving");
		if (eating) xml.PushAttribute("itemArrival", m->second.arrivalTime);
		xml.CloseElement();
	}
}

void FastFood::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant");
	if (variant >= 0 && (size_t)variant < sizeof(kNameVariants)/sizeof(kNameVariants[0]))
		displayName = kNameVariants[variant];
	open = xml.BoolAttribute("open");
	pricePerMeal = xml.IntAttribute("pricePerMeal", 200);
	population = xml.IntAttribute("population", 0);
	dailyQuota = std::max(0, xml.IntAttribute("dailyQuota", 0));
	quotaRemaining = std::max(0, xml.IntAttribute("quotaRemaining", dailyQuota));
	walkinsToday = std::max(0, xml.IntAttribute("walkinsToday", 0));
	walkinsYesterday = std::max(0, xml.IntAttribute("walkinsYesterday", 0));
	customersToday = std::max(0, xml.IntAttribute("customersToday", 0));
	lastDemandDay = xml.IntAttribute("lastDemandDay", -1);
	revenueSettled = xml.BoolAttribute("revenueSettled", false);
    std::string slots=xmlAttrOr(xml, "dispatched", "");
    for (int k=0;k<TenantRules::kRetailPoolSize;++k) dispatched[k]=k<int(slots.size()) && slots[k]=='1';
	for (int i = 0; i < 3; ++i) {
		char key[32];
		snprintf(key, sizeof(key), "serviceScore%d", i);
		serviceScore[i] = std::max(0, xml.IntAttribute(key, 10));
	}
	const int savedCustomersToday = customersToday;
	const int savedServiceScore[3] = {serviceScore[0], serviceScore[1], serviceScore[2]};
	clearCustomers();
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("customer"); e; e = e->NextSiblingElement("customer"))
	{
		Customer *c = new Customer(this);
		c->arrivalTime = e->DoubleAttribute("lobbyArrival");
		c->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		c->state = (Person::State)e->IntAttribute("state", Person::kWandering);
		c->stress = e->DoubleAttribute("stress", 0.0);
        c->bankCount=e->IntAttribute("bankCount",0); c->bankTotal=e->IntAttribute("bankTotal",0);
        c->poolSlot=e->IntAttribute("poolSlot",-1);
		c->eval = e->DoubleAttribute("eval", 0.0);
		c->name = xmlAttributeOr(*e, "name", "");
		c->from = xmlAttributeOr(*e, "from", "");
		c->goingTo = xmlAttributeOr(*e, "goingTo", "");
		customers.insert(c);
		if (std::string(xmlAttributeOr(*e, "phase", "arriving")) == "eating") {
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

void FastFood::updateSprite()
{
	spriteNeedsUpdate = false;
	// EXE venue state byte (UpdateVenueBusyTier 11a8:0bd5): 0 empty,
	// 1 busy (1-9 inside), 2 packed (10+), 3 closed shutters.
	int index = 3;
	if (open)
		index = (people.size() >= 10) ? 2 : (people.size() >= 1) ? 1 : 0;
	sprite.setTextureRect(sf::IntRect({index * 128, variant * 24}, {128, 24}));
}

void FastFood::advance(double dt)
{
	(void)dt;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	const int period = game->sky.retailPeriod();
	const int cap = period == 0 ? 35 : period == 1 ? 50 : 25;

	if (game->time.checkHour(10) && lastDemandDay != day) openDay();

    if (open && quotaRemaining>0) {
        const double hour=game->time.hour;
        const int width=(hour >= 10 && hour < 17) ? 36 : (hour >= 17 && hour < 21) ? 6 : 0;
        const long visits=visitsThisTick(game, 16, 0);
        for (long v=0;v<visits;++v) for (int k=0;k<TenantRules::kRetailPoolSize && quotaRemaining>0;++k) {
            if (dispatched[k] || width==0 || (width>1 && rand()%width!=0)) continue;
            dispatched[k]=true;
            Customer *c=new Customer(this); c->poolSlot=k; c->member=k;
            c->arrivalTime=game->time.absolute;
            customers.insert(c); arrivingCustomers.push(c);
            --quotaRemaining; ++walkinsToday; ++customersToday;
        }
    }

	if (game->time.checkHour(21) && open)
	{
		open = false;
		population = walkinsToday;
		// Closing changes the inspector display only; CountT rotates on opening.
		spriteNeedsUpdate = true;
		if (!revenueSettled && prototype->yoot) {
			// YootTower E6 (T2Tenant::CalcInMoney 0x1005f620): paid at close,
			// customers x inMoney / 100 - the daily outMoney.
			const YootItemStats &y = *prototype->yoot;
			game->yootBook(Yoot::commercialIncome(customersToday, y), "retail_income", "Income from Fast Food");
			yootEstimate = Yoot::profitEstimate(customersToday, y.inMoneyFor(0) / kYootMoneyUnit, y.outMoney / kYootMoneyUnit);
			revenueSettled = true;
		}
		if (!revenueSettled) {
			int amount = customersToday >= 50 ? 5000 : customersToday >= 35 ? 3000
				: customersToday >= 25 ? 2000 : -3000;
			if (!game->isDisasterOfferDay())
				game->transferFunds(amount, "retail_income", "Income from Fast Food");
			revenueSettled = true;
		}
	}

	while (!arrivingCustomers.empty()) {
		Customer *c = arrivingCustomers.top();
		if (game->time.absolute < c->arrivalTime) break;
		arrivingCustomers.pop();
		if (open && game->time.hour < 21.0 && !lobbyRoute.empty()) c->journey.set(lobbyRoute);
		else { customers.erase(c); delete c; }
	}

	for (std::list<Person *>::iterator ip = eatingCustomers.begin(); ip != eatingCustomers.end();) {
		Person *p = *ip;
		CustomerMetadata &m = customerMetadata[p];
		if (game->time.eventFramesSince(m.arrivalTime) >= 60 && (!open || game->time.eventFramesSince(m.arrivalTime) >= (8 + (game->tenantSlotIndex(this)*5)%12)*8)) {
            Item *destination=p->home;
            if (auto *visitor=dynamic_cast<Metro::Visitor*>(p)) destination=visitor->station;
            if (!destination || !game->items.count(destination)) destination=game->mainLobby;
            Route r=game->findRoute(this,destination);
			if (r.empty()) { ++ip; continue; }
			++ip;
			removePerson(p);
			p->state = Person::kReturning;
			p->from = prototype->name;
			p->goingTo = "Exit";
			p->journey.set(r);
		} else ++ip;
	}
    for (auto it=customers.begin();it!=customers.end();) {
        Customer *c=*it;
        if (c->state==Person::kReturning && !c->at && !c->journey.item) { it=customers.erase(it); delete c; }
        else ++it;
    }
	if (spriteNeedsUpdate) updateSprite();
}

void FastFood::addPerson(Person *p)
{
    // Journey::next already banked the final transport leg, so grading reads
    // the updated mean even at a shut door.
    const int period=game->sky.retailPeriod();
    const int cap=period==0?35:period==1?50:25;
    const int mean=p->bankedMean(), bar2=game->rating<=2?150:200;
    serviceScore[period]=std::min(cap,serviceScore[period]+(mean<80?2:mean<bar2?1:0));
    if (!open || people.size()>=40) {
        Item::addPerson(p);
        Item *destination=p->home;
        if (auto *visitor=dynamic_cast<Metro::Visitor*>(p)) destination=visitor->station;
        if (!destination || !game->items.count(destination)) destination=game->mainLobby;
        p->state=Person::kReturning;
        Route route=game->findRoute(this,destination);
        if (!route.empty()) p->journey.set(route);
        else { Item::removePerson(p); p->recordNoRoute(position.y,destination?destination->position.y:0); }
        return;
    }
    Item::addPerson(p); p->state=Person::kLunch;
    customerMetadata[p].arrivalTime=game->time.absolute;
    eatingCustomers.push_back(p);
    auto *customer=dynamic_cast<Customer*>(p);
    if (!customer || customer->owner!=this) ++customersToday;
    spriteNeedsUpdate=true;
}

void FastFood::removePerson(Person *p)
{
	Item::removePerson(p);
	eatingCustomers.remove(p);
	customerMetadata.erase(p);
	spriteNeedsUpdate = true;
}

void FastFood::clearCustomers()
{
	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
		delete *c;
	while (!arrivingCustomers.empty())
		arrivingCustomers.pop();
	customers.clear();
    for (auto it=eatingCustomers.begin();it!=eatingCustomers.end();) {
        Person *p=*it; ++it;
        if (!game->people.count(p)) continue;
        Item *dest=p->home ? p->home : game->mainLobby;
        if (auto *visitor=dynamic_cast<Metro::Visitor*>(p)) dest=visitor->station;
        Route route=game->findRoute(this,dest);
        if (!route.empty()) { p->state=Person::kReturning; p->journey.set(route); }
    }
}

FastFood::Customer::Customer(FastFood *item)
	: Person(item->game)
{
	owner=item; home=nullptr;
	arrivalTime = 0;
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	from = "City";
	goingTo = item->prototype->name;
}

Path FastFood::getRandomBackgroundSoundPath()
{
	if (!open)
		return "";
	char name[128];
	snprintf(name, 128, "simtower/fastfood/%i", rand() % 3);
	// Maybe we should make the choice of the sound based on the number of customers, not
	// completely random.
	return name;
}

void FastFood::evacuateForDisaster()
{
 clearCustomers();
 open = false;
 quotaRemaining = customersToday = walkinsToday = 0;
 revenueSettled = true;
 spriteNeedsUpdate = true;
 updateSprite();
}

void FastFood::openDay()
{
    const int day=int(std::floor(game->time.absolute));
    const int period=game->sky.retailPeriod();
    const int cap=period==0?35:period==1?50:25;
    if (lastDemandDay==day) return;

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
	
    for (bool &used : dispatched) used=false;
    game->resetCategoryOnce(5);
    game->addPopulation(5,walkinsYesterday);
}
