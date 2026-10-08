/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../PeopleSystem.h"
#include "../YootPeopleArt.h"
#include "../PluginManager.h"
#include "../PluginObjmap.h"
#include "../YootItemArt.h"
#include "../YootRules.h"
#include "YootTenant.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>

using namespace OT;
using namespace OT::Item;

OT::Item::Item * YootTenantPrototype::make(Game * game)
{
	switch (def.family) {
		case YootTenantFamily::Restaurant: return new YootRestaurant(game, this);
		case YootTenantFamily::Vending: return new YootVending(game, this);
		default: return new YootShop(game, this);
	}
}

void OT::Item::registerYootTenantPrototypes(std::vector<AbstractPrototype *> & prototypes, const PluginManager & plugins)
{
	for (const YootTenantDef & d : loadYootTenantDefs(plugins)) {
		YootTenantPrototype * p = new YootTenantPrototype;
		p->def = d;
		p->id = d.prototypeId;
		p->name = d.name;
		p->price = d.price;
		p->size = int2(d.width, d.height);
		p->icon = 13; // OpenSky shop cell; the YootTower toolbox uses OBJMAP 100
		p->variant = 0;
		p->entrance_offset = 0;
		p->exit_offset = 0;
		prototypes.push_back(p);
	}
}

YootTenant::YootTenant(Game * game, AbstractPrototype * prototype) : Item(game, prototype)
{
}

const YootTenantDef & YootTenant::def() const
{
	return static_cast<const YootTenantPrototype *>(prototype)->def;
}

void YootTenant::init()
{
	Item::init();
	model = Yoot::TenantModel();
	lastMinute = -1;
	// Spread the idles over the 8-minute round robin like the 32 dispatch
	// slots of T2TenantArray do.
	idlePhase = (int)(((unsigned)(position.x * 31 + position.y * 17)) % Yoot::kTenantIdleMinutes);
	if (const YootPluginView * v = yootPluginView(game->app, def().plugin)) {
		view = &v->texture;
		bands = &v->bands;
		sprite.SetImage(v->texture);
		// Skin::Yoot: the 24 px interior sits above the 4 px floor strip.
		const int lift = game->skin == Skin::Yoot ? kYootInteriorLift : 0;
		sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36 - lift)});
		addSprite(&sprite);
	}
	updateSprite();
}

void YootTenant::updateSprite()
{
	if (!view) return;
	sf::IntRect band({0, 0}, {static_cast<int>(view->getSize().x), static_cast<int>(view->getSize().y)});
	if (bands && !bands->empty()) {
		// T2Tenant::UpdatePatternIndex: numOfPattern x kind + pattern; all
		// covered plugins have one kind.
		const int index = std::max(0, std::min((int)bands->size() - 1, model.pattern));
		band = (*bands)[index];
	}
	sprite.setTextureRect(band);
	sprite.setOrigin({0.f, static_cast<float>(band.size.y)});
}

void YootTenant::advance(double dt)
{
	(void)dt;
	const YootTenantDef & d = def();
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	// Catch up minute by minute, but never replay more than a day (loads,
	// big jumps).
	if (lastMinute < 0 || now < lastMinute || now - lastMinute > 1440) lastMinute = now - 1;
	if (now == lastMinute) return;

	if (lobbyRouteComputed)
		reachable = position.y == 0 || !lobbyRoute.empty();
	Yoot::TenantEvents ev;
	for (long long m = lastMinute + 1; m <= now; ++m) {
		releaseVisitors(m);
		Yoot::tenantMinute(model, d, m, ev);
		if ((int)(((m % Yoot::kTenantIdleMinutes) + Yoot::kTenantIdleMinutes) % Yoot::kTenantIdleMinutes) != idlePhase)
			continue;
		Yoot::TenantInputs in;
		in.minute = (int)(((m % 1440) + 1440) % 1440);
		in.absMinute = m;
		in.holiday = game->time.isHoliday();
		in.rain = game->sky.rainyDay;
		in.reachable = reachable && game->mainLobby != NULL;
		in.sources = game->eventSystem.people.enabled() && !game->eventSystem.people.pool.empty() ? this : nullptr;
		releaseVisitors(m);
		Yoot::tenantIdle(model, d, in, [] { return std::rand(); }, ev);
		releaseVisitors(m);
		rollStaffAnime();
	}
	lastMinute = now;

	if (ev.paid) {
		const char * message = incomeMessage();
		game->yootBook(ev.income, incomeCategory(), message ? message : "");
		game->recordSimEvent(def().prototypeId + " customers: " + std::to_string(ev.paidCustomers));
	}
	if (ev.estimateChanged) {
		yootEstimate = model.estimate;
		evaluation = model.estimate / 3.0;
	}
	// Staff are pool people kept by the tenant; visitors are counted by
	// the pool (T2Pool::GetPopulation).
	population = model.staff;
	if (ev.patternChanged) updateSprite();
}

void YootTenant::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("tenantStatus", model.status);
	xml.PushAttribute("pattern", model.pattern);
	xml.PushAttribute("staff", model.staff);
	xml.PushAttribute("staffIn", model.staffIn);
	xml.PushAttribute("customers", model.customers);
	xml.PushAttribute("inside", (int)model.inside.size());
}

void YootTenant::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	const YootTenantDef & d = def();
	model.status = xml.IntAttribute("tenantStatus", Yoot::kStatusBuilt);
	// Statuses below 9 (for rent / unbuilt records of a .bld) restart as built.
	if (model.status < Yoot::kStatusBuilt) model.status = Yoot::kStatusBuilt;
	const bool open = isOpen();
	// Imported towers carry no staff counts: a tenant past hiring (11+)
	// has its staff, at work while open.
	const int staffDefault = model.status >= Yoot::kStatusNight ? d.belongCapacity : 0;
	model.staff = std::max(0, std::min(d.belongCapacity, xml.IntAttribute("staff", staffDefault)));
	model.staffIn = std::max(0, std::min(model.staff, xml.IntAttribute("staffIn", open ? model.staff : 0)));
	// Vending machines open round the clock keep pattern 0 (Drink.t2p).
	const bool allDay = d.family == YootTenantFamily::Vending && d.openMinute == d.closeMinute;
	model.pattern = std::max(0, xml.IntAttribute("pattern", open && !allDay ? 1 : 0));
	model.customers = std::max(0, xml.IntAttribute("customers", 0));
	model.estimate = yootEstimate;
	// Customers inside are transient: they finish their stay from now.
	model.inside.clear();
	model.walking = 0;
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	const int inside = std::max(0, std::min(d.capacity, xml.IntAttribute("inside", 0)));
	for (int i = 0; i < inside; ++i) model.inside.push_back(now + std::max(1, d.bindMinutes));
	lastMinute = -1;
	updateSprite();
}

void YootTenant::addPerson(Person * p)
{
	Item::addPerson(p);
	YootVisitor * v = dynamic_cast<YootVisitor *>(p);
	if (!v || v->dest != this) return;
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	Yoot::TenantEvents ev;
	const bool admitted = Yoot::customerArrives(model, def(), now, ev);
	v->stayEnd = admitted ? model.inside.back() : -1;
	game->eventSystem.people.arrived(v, this, admitted, v->stayEnd);
	if (ev.patternChanged) updateSprite();
}

void YootTenant::rollStaffAnime()
{
	// Barber.t2p / Burger.t2p call SetPeopleAnime on every idle while open.
	staffAnime.clear();
	if (!isOpen() || model.staffIn <= 0) return;
	const YootTenantPeopleArt & art = yootTenantPeopleArt(game, def().plugin);
	if (!art.hasAnime) return; // the sim rolls even without the art (determinism)
	for (int i = 0; i < model.staffIn; ++i)
	{
		int pattern = -1, x = 0;
		if (!rollPeopleAnime(art.anime, i, position.x, [] { return std::rand(); }, pattern, x)) pattern = -1;
		staffAnime.push_back(std::make_pair(pattern, x));
	}
}

void YootTenant::render(sf::RenderTarget & target) const
{
	Item::render(target);
	if (staffAnime.empty() || underConstruction) return;
	const YootTenantPeopleArt & art = yootTenantPeopleArt(game, def().plugin);
	if (!art.loaded) return;
	sf::Sprite s(art.texture);
	sf::Color color = game->lighting.compose(sf::Color::White);
	if (game->statusMode == Game::kHotel) color = sf::Color(color.r * 110 / 255, color.g * 110 / 255, color.b * 110 / 255, 160);
	s.setColor(color);
	// The 24 px tenant band sits on the floor line (YootTenant::updateSprite).
	const float top = -position.y * 36.0f - 24.0f;
	for (const std::pair<int, int> & a : staffAnime)
	{
		if (a.first < 0 || a.first >= (int)art.shil.size() || art.shil[a.first].size.x <= 0) continue;
		s.setTextureRect(art.shil[a.first]);
		s.setPosition({a.second * 8.0f, top});
		target.draw(s);
	}
}

void YootTenant::releaseVisitors(long long absMinute)
{
	if (people.empty()) return;
	std::vector<YootVisitor *> inside;
	for (Person * p : people)
	{
		YootVisitor * v = dynamic_cast<YootVisitor *>(p);
		if (v && v->phase == YootVisitor::kInTenant && v->inside == this) inside.push_back(v);
	}
	// T2People::Idle case 11: the start minute (end of stay, or the call of
	// another tenant) came.
	for (size_t k = 0; k < inside.size();)
	{
		YootVisitor * v = inside[k];
		if (v->startMinute > absMinute) { ++k; continue; }
		std::vector<long long>::iterator e = std::find(model.inside.begin(), model.inside.end(), v->stayEnd);
		if (e != model.inside.end()) model.inside.erase(e);
		inside.erase(inside.begin() + k);
		game->eventSystem.people.pushOut(v);
	}
	// PushOutAllCustomer emptied the model: everyone inside leaves.
	while ((int)inside.size() > model.insideCount())
	{
		YootVisitor * v = inside.back();
		inside.pop_back();
		game->eventSystem.people.pushOut(v);
	}
}

int YootTenant::fromPool(const YootTenantDef & d, int free, const Yoot::TenantInputs & in)
{
	return game->eventSystem.people.callFromPool(this, d, free, in.absMinute);
}

int YootTenant::fromTenant(const YootTenantDef & d, int free, const Yoot::TenantInputs & in)
{
	return game->eventSystem.people.callFromTenant(this, d, free, in.absMinute);
}

int YootTenant::fromFloor(const YootTenantDef & d, int free, const Yoot::TenantInputs & in)
{
	return game->eventSystem.people.callFromFloor(this, d, free, in.absMinute);
}

const char * YootShop::incomeMessage() const
{
	// Spa.t2p pays through DoPay without the shop message.
	if (def().plugin == "Spa.t2p") return nullptr;
	return "Income from shop just came in.";
}
