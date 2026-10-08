/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../PluginManager.h"
#include "../PluginObjmap.h"
#include "../YootRules.h"
#include "YootTenant.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>

using namespace OT;
using namespace OT::Item;

namespace
{
	struct TenantView
	{
		bool loaded = false;
		sf::Texture texture;
		std::vector<sf::IntRect> bands;
	};

	/// RT_BITMAP 1000 + OBJMAP 1000 "View" rects (pattern order) of a plugin.
	TenantView & tenantView(Game * game, const std::string & plugin)
	{
		static std::map<std::string, std::unique_ptr<TenantView> > cache;
		std::unique_ptr<TenantView> & slot = cache[plugin];
		if (slot) return *slot;
		slot.reset(new TenantView);
		TenantView & v = *slot;
		v.loaded = game->app.bitmaps.loadFromPlugin(plugin, 1000, v.texture);
		AbstractPlugin * p = game->app.pluginManager.get(plugin);
		const WindowsPEExecutable::Resource * om = (p && p->ensureLoaded()) ? p->getResource("OBJMAP", 1000) : nullptr;
		std::vector<ObjmapBlock> blocks;
		if (om && parsePluginObjmap(om->data, om->length, blocks)) {
			std::vector<std::pair<int, sf::IntRect> > views;
			for (const ObjmapBlock & b : blocks)
				if (b.name == "View")
					views.push_back(std::make_pair(b.pattern, sf::IntRect({b.rect.left, b.rect.top}, {b.rect.width(), b.rect.height()})));
			std::stable_sort(views.begin(), views.end(),
				[](const std::pair<int, sf::IntRect> & a, const std::pair<int, sf::IntRect> & b) { return a.first < b.first; });
			for (const auto & view : views) v.bands.push_back(view.second);
		}
		return v;
	}
}

OT::Item::Item * YootTenantPrototype::make(Game * game)
{
	switch (def.family) {
		case YootTenantFamily::Restaurant: return new YootRestaurant(game, this);
		case YootTenantFamily::Vending: return new YootVending(game, this);
		case YootTenantFamily::Service: return new YootService(game, this);
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
	TenantView & v = tenantView(game, def().plugin);
	if (v.loaded) {
		view = &v.texture;
		bands = &v.bands;
		sprite.SetImage(v.texture);
		sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
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
		Yoot::tenantMinute(model, d, m, ev);
		if ((int)(((m % Yoot::kTenantIdleMinutes) + Yoot::kTenantIdleMinutes) % Yoot::kTenantIdleMinutes) != idlePhase)
			continue;
		Yoot::TenantInputs in;
		in.minute = (int)(((m % 1440) + 1440) % 1440);
		in.absMinute = m;
		in.holiday = game->time.isHoliday();
		in.rain = game->sky.rainyDay;
		in.reachable = reachable && game->mainLobby != NULL;
		in.peopleAround = game->population > 0;
		Yoot::tenantIdle(model, d, in, [] { return std::rand(); }, ev);
	}
	lastMinute = now;

	if (ev.paid) {
		const char * message = incomeMessage();
		game->yootBook(ev.income, incomeCategory(), message ? message : "");
	}
	if (ev.estimateChanged) {
		yootEstimate = model.estimate;
		evaluation = model.estimate / 3.0;
	}
	population = model.insideCount() + model.staffIn;
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
	model.pattern = std::max(0, xml.IntAttribute("pattern", open ? 1 : 0));
	model.customers = std::max(0, xml.IntAttribute("customers", 0));
	model.estimate = yootEstimate;
	// Customers inside are transient: they finish their stay from now.
	model.inside.clear();
	model.arriving.clear();
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	const int inside = std::max(0, std::min(d.capacity, xml.IntAttribute("inside", 0)));
	for (int i = 0; i < inside; ++i) model.inside.push_back(now + std::max(1, d.bindMinutes));
	lastMinute = -1;
	updateSprite();
}

const char * YootShop::incomeMessage() const
{
	// Spa.t2p pays through DoPay without the shop message.
	if (def().plugin == "Spa.t2p") return nullptr;
	return "Income from shop just came in.";
}
