/* Copyright © 2013 Fabian Schuiki */
#include "../Game.h"
#include "../XmlUtil.h"
#include "../YootRules.h"
#include "../Math/Rand.h"
#include "Shop.h"
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>
#include "../Time.h"
extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);

using namespace OT;
using namespace OT::Item;

// Authentic tenant name pool (SIMTOWER.EXE string table res 0x2cc; matches
// the 11 storefront sheets 0x8668-0x8672 one-to-one).
const char * const Shop::kNameVariants[11] = {
	"Men's Clothing", "Pet Store", "Flower Shop", "Book Store",
	"Drug Store", "Boutique", "Electronics", "Bank", "Hair Salon",
	"Post Office", "Sports Gear"
};

Shop::~Shop()
{
	clearCustomers();
}

Shop::Customer::Customer(Shop *item)
	: Person(item->game)
{
	// member doubles as the generic-label selector for visitors (member & 7).
	static const Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	static const int members[] = {0, 1, 3, 5};
	const int i = rand() % 4;
	type = types[i];
	member = (int8_t)members[i];
	from = "City";
	goingTo = item->prototype->name;
}

void Shop::clearCustomers()
{
	// Persons inside are owned by `customers`; owner persons (condo
	// homebodies) are only detached.
	std::vector<Person *> inside(people.begin(), people.end());
	for (Person *p : inside) removePerson(p);
	leaveAt.clear();
	for (Customers::iterator c = customers.begin(); c != customers.end(); ++c)
		delete *c;
	customers.clear();
}

void Shop::init()
{
	Item::init();
	variant = rand() % 11;
	displayName = kNameVariants[variant];
	let = false;
	open = false;
	patrons = 0;
	tenant.rentClass = 1;
	serviceScore[0] = serviceScore[1] = serviceScore[2] = 10;
	dailyQuota = quotaRemaining = walkinsToday = walkinsYesterday = customersToday = 0;
	lastDemandDay = -1;
	sprite.setTexture(App->bitmaps.editionTexture("shop", "simtower/shops"));
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;
	updateSprite();
}

void Shop::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("let", let);
	xml.PushAttribute("open", open);
	xml.PushAttribute("patrons", patrons);
	xml.PushAttribute("dailyQuota", dailyQuota);
	xml.PushAttribute("quotaRemaining", quotaRemaining);
	xml.PushAttribute("walkinsToday", walkinsToday);
	xml.PushAttribute("walkinsYesterday", walkinsYesterday);
	xml.PushAttribute("customersToday", customersToday);
	xml.PushAttribute("lastDemandDay", lastDemandDay);
	for (int i = 0; i < 3; ++i) {
		char key[32]; snprintf(key, sizeof(key), "serviceScore%d", i);
		xml.PushAttribute(key, serviceScore[i]);
	}
	// Customers inside the shop survive a save with their bank counters.
	for (Customers::iterator c = customers.begin(); c != customers.end(); ++c)
	{
		Customer *customer = *c;
		if (customer->at != this) continue;
		xml.OpenElement("customer");
		xml.PushAttribute("type", customer->type);
		xml.PushAttribute("member", (int)customer->member);
		xml.PushAttribute("state", customer->state);
		xml.PushAttribute("stress", customer->stress);
		xml.PushAttribute("eval", customer->eval);
		xml.PushAttribute("name", customer->name.c_str());
		xml.PushAttribute("bankCount", (int)customer->bankCount);
		xml.PushAttribute("bankTotal", (int)customer->bankTotal);
		std::map<Person *, double>::iterator l = leaveAt.find(customer);
		xml.PushAttribute("leaveAt", l == leaveAt.end() ? 0.0 : l->second);
		xml.CloseElement();
	}
}

void Shop::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant", 0);
	if (variant < 0 || variant >= 11) variant = 0;
	displayName = kNameVariants[variant];
	let = xml.BoolAttribute("let", false);
	open = xml.BoolAttribute("open", false);
	patrons = std::max(0, std::min(40, xml.IntAttribute("patrons", 0)));
	dailyQuota = std::max(0, xml.IntAttribute("dailyQuota", 0));
	quotaRemaining = std::max(0, xml.IntAttribute("quotaRemaining", dailyQuota));
	walkinsToday = std::max(0, xml.IntAttribute("walkinsToday", 0));
	walkinsYesterday = std::max(0, xml.IntAttribute("walkinsYesterday", 0));
	customersToday = std::max(0, xml.IntAttribute("customersToday", 0));
	lastDemandDay = xml.IntAttribute("lastDemandDay", -1);
	for (int i = 0; i < 3; ++i) {
		char key[32]; snprintf(key, sizeof(key), "serviceScore%d", i);
		serviceScore[i] = std::max(0, xml.IntAttribute(key, 10));
	}
	// Old saves had no tenant record: an already let shop keeps its tenant.
	if (!xml.FindAttribute("armed") && let) tenant.armed = true;
	clearCustomers();
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("customer"); e; e = e->NextSiblingElement("customer"))
	{
		Customer *c = new Customer(this);
		c->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		c->member = (int8_t)e->IntAttribute("member", 0);
		c->stress = e->DoubleAttribute("stress", 0.0);
		c->eval = e->DoubleAttribute("eval", 0.0);
		c->name = xmlAttrOr(*e, "name", "");
		c->bankCount = (int8_t)e->IntAttribute("bankCount", 0);
		c->bankTotal = (int16_t)e->IntAttribute("bankTotal", 0);
		c->visited = true;
		customers.insert(c);
		Item::addPerson(c);
		c->state = Person::kShopping;
		leaveAt[c] = e->DoubleAttribute("leaveAt", 0.0);
	}
	patrons = std::min(40, (int)people.size());
	updateSprite();
}

int Shop::stateByte() const
{
	if (!let) return 0xFF;
	if (!open) return 3;
	return patrons >= 10 ? 2 : patrons >= 1 ? 1 : 0;
}

void Shop::setLet(bool l)
{
	if (let == l) return;
	let = l;
	spriteNeedsUpdate = true;
}

void Shop::setOpen(bool o)
{
	if (open == o) return;
	open = o;
	spriteNeedsUpdate = true;
}

void Shop::setPatrons(int p)
{
	p = std::max(0, std::min(40, p));
	if (patrons == p) return;
	patrons = p;
	spriteNeedsUpdate = true;
}

void Shop::updateSprite()
{
	spriteNeedsUpdate = false;
	if (!let)
	{
		// Shared "For RENT" storefront (0x8673), merged-sheet row 0, x 0..96.
		sprite.setTextureRect(sf::IntRect({0, 0}, {96, 24}));
	}
	else if (!open)
	{
		// Shared shuttered storefront (0x8674), row 0, x 96..192.
		sprite.setTextureRect(sf::IntRect({96, 0}, {96, 24}));
	}
	else
	{
		// Fill tiers of the variant strip (rows 1..11 below the shared
		// row): frame 0 empty, 1 for 1-9 patrons, 2 for 10+ packed.
		int tier = stateByte();
		sprite.setTextureRect(sf::IntRect({tier * 96, 24 + variant * 24}, {96, 24}));
	}
}

void Shop::advance(double dt)
{
	(void)dt;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	const int period = game->sky.retailPeriod();
	const int cap = period == 0 ? 25 : period == 1 ? 30 : 18;
	if (game->time.checkHour(10)) {
		clearCustomers();
		open = true;
		patrons = 0;
		walkinsYesterday = walkinsToday;
		population = walkinsYesterday;
		// CountT retail category (T14): reset once per day, then each shop
		// adds its rotated yesterday walk-ins.
		game->resetCategoryOnce(TenantRules::kCountShop);
		game->addPopulation(TenantRules::kCountShop, walkinsYesterday);
		customersToday = walkinsToday = 0;
		int score = serviceScore[period];
		if (period == 1) score = std::max(score, serviceScore[0]);
		dailyQuota = std::max(10, std::min(cap, score));
		quotaRemaining = dailyQuota;
		lastDemandDay = day;
		serviceScore[period] = 0;
		spriteNeedsUpdate = true;
	}

	// Pool rolls (T26): every 16-frame visit each undispatched slot of the
	// 48-person pool (1230:07cb, all.c:33438-33442) rolls 1/width.
	if (open && quotaRemaining > 0 && (let || tenant.armed)) {
		const int visits = visitsThisTick(game);
		const double hour = game->time.hour;
		const int width = (hour >= 10.0 && hour < 17.0) ? 36 :
			(hour >= 17.0 && hour < 21.0) ? 6 : 0;
		for (int v = 0; width > 0 && v < visits && quotaRemaining > 0; ++v) {
			int free = TenantRules::kRetailPoolSize - (int)customers.size();
			for (int slot = 0; slot < free && quotaRemaining > 0; ++slot) {
				if (rand() % width != 0) continue;
				if (!let) {
					if (!tenant.armed) break;
					relet(); // first walk-in re-lets an armed vacant shop
				}
				if (lobbyRoute.empty()) { slot = free; break; }
				Customer *c = new Customer(this);
				customers.insert(c);
				--quotaRemaining;
				++walkinsToday;
				++customersToday;
				c->state = Person::kWandering;
				c->journey.set(lobbyRoute);
			}
		}
	}

	// Dwell over (or shutters down): everybody inside heads home / to the lobby.
	std::vector<Person *> leaving;
	for (People::iterator ip = people.begin(); ip != people.end(); ++ip) {
		std::map<Person *, double>::iterator l = leaveAt.find(*ip);
		if (!open || (l != leaveAt.end() && game->time.absolute >= l->second))
			leaving.push_back(*ip);
	}
	for (Person *p : leaving) {
		const Route r = (p->home && p->home != this) ? game->findRoute(this, p->home)
		                                              : game->findRoute(this, game->mainLobby);
		if (r.empty()) continue;
		removePerson(p);
		p->state = Person::kReturning;
		p->goingTo = p->home ? "Home" : "Exit";
		p->journey.set(r);
	}

	// Customers that finished their visit and left the tower are retired,
	// which recycles their pool slot.
	for (Customers::iterator c = customers.begin(); c != customers.end();) {
		Customer *customer = *c;
		if (customer->visited && !customer->at && !customer->journey.item) {
			c = customers.erase(c);
			delete customer;
		} else ++c;
	}

	if (game->time.checkHour(21) && open) {
		open = false;
		population = walkinsToday;
		spriteNeedsUpdate = true;
		if (prototype->yoot && let) {
			// YootTower E6: shops earn CalcInMoney at close instead of rent.
			const YootItemStats &y = *prototype->yoot;
			game->yootBook(Yoot::commercialIncome(customersToday, y), "retail_income", "Income from Shop");
			yootEstimate = Yoot::profitEstimate(customersToday, y.inMoneyFor(0) / kYootMoneyUnit, y.outMoney / kYootMoneyUnit);
		}
	}
	setPatrons((int)people.size());
	if (spriteNeedsUpdate) updateSprite();
}

void Shop::relet()
{
	setLet(true);
	tenant.tenure = 0;
	tenant.category = TenantRules::kUnjudged;
	tenant.redraw = true;
	resetTenantBanks();
	refreshEvaluation();
	walkinsYesterday = 10;
	game->addPopulation(TenantRules::kCountShop, 10);
	game->transferFunds(openSkyQuarterlyRent("shop", tenant.rentClass), "rent_income", "Retail shop lease");
	spriteNeedsUpdate = true;
}

void Shop::vacate()
{
	// The unlet storefront (state 255). CountT keeps its +10 until the next
	// daily retail reset (EXE leak, bounded to one day).
	setLet(false);
	spriteNeedsUpdate = true;
}

void Shop::addPerson(Person *p)
{
	const int period = game->sky.retailPeriod();
	const int cap = period == 0 ? 25 : period == 1 ? 30 : 18;
	// The trip was banked on arrival; grade the person's historical mean
	// (late arrivals at a closed door still grade).
	const int mean = p->bankedMean();
	const int bar2 = TenantRules::bar2(game->rating);
	const int delta = mean < TenantRules::bar1() ? 2 : mean < bar2 ? 1 : 0;
	serviceScore[period] = std::min(cap, serviceScore[period] + delta);

	if (!let || !open || people.size() >= 40) {
		// Closed door or full house: bounce back out.
		Item::addPerson(p);
		Item::removePerson(p);
		if (Customer *c = dynamic_cast<Customer *>(p)) c->visited = true;
		p->state = Person::kReturning;
		p->goingTo = "Exit";
		if (game->mainLobby) {
			const Route r = (p->home && p->home != this) ? game->findRoute(this, p->home)
			                                              : game->findRoute(this, game->mainLobby);
			if (!r.empty()) p->journey.set(r);
		}
		return;
	}
	Item::addPerson(p);
	p->state = Person::kShopping;
	p->eval = 50;
	if (Customer *c = dynamic_cast<Customer *>(p)) c->visited = true;
	// Dwell (8 + slot*5 % 12) * 8 frames, never below the 60 frame minimum stay.
	const int dwell = std::max(60, (8 + (game->tenantSlotIndex(this) * 5) % 12) * 8);
	leaveAt[p] = game->time.absolute + (double)dwell / Time::kSimFramesPerDay;
	setPatrons((int)people.size());
}

void Shop::removePerson(Person *p)
{
	Item::removePerson(p);
	leaveAt.erase(p);
	setPatrons((int)people.size());
}

Path Shop::getRandomBackgroundSoundPath()
{
	if (!let || !open)
		return "";
	// The EXE alternates the shop ambience between the two waves fast food
	// also uses (concilia audio_events.h: AMB_RESTAURANT_B 0x8569 and
	// AMB_SHOP_FF_B 0x8668), loaded as simtower/fastfood/0 and /2.
	char name[128];
	snprintf(name, 128, "simtower/fastfood/%i", (rand() & 1) ? 0 : 2);
	return name;
}

void Shop::evacuateForDisaster()
{
 clearCustomers();
 open = false; patrons = 0;
 quotaRemaining = customersToday = walkinsToday = 0;
 spriteNeedsUpdate = true;
 updateSprite();
}
