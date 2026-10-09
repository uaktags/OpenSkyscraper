#include "YootCondo.h"
#include "../BitmapManager.h"
#include "../Game.h"
#include "../Math/Rand.h"
#include "../YootCatalog.h"
#include "../YootItemArt.h"

#include <cmath>
#include <map>
#include <memory>

using namespace OT;
using namespace OT::Item;
extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);

namespace
{
	struct CondoArt
	{
		bool loaded = false;
		sf::Texture empty;
		sf::Texture resident;
	};
}

YootCondo::~YootCondo()
{
	removeOccupants();
}

const char * YootCondo::condoPlugin() const
{
	// Condo.t2p in Tokyo, HawaianCondo.t2p in Hawaii (catalog link).
	if (prototype->yoot && !prototype->yoot->plugin.empty())
		return prototype->yoot->plugin.c_str();
	return "Condo.t2p";
}

int YootCondo::residentsToCreate() const
{
	// Condo.t2p / HawaianCondo.t2p: every sold unit hires its TNDF belong capacity
	// (3) of residents from the pool; the original's population shows 55 of 60 for 20 units.
	if (prototype->yoot && prototype->yoot->belongCapacity > 0) return prototype->yoot->belongCapacity;
	return Math::randi(1, 4);
}

bool YootCondo::loadTextures()
{
	// BitmapManager::loadFromPlugin is the canonical PE-bitmap loader
	// (used to live here as YootCondo::loadPEBitmap). The plugin is
	// resolved through PluginManager's catalog; art is cached per plugin.
	static std::map<std::string, std::unique_ptr<CondoArt> > cache;
	const std::string pluginName = condoPlugin();
	std::unique_ptr<CondoArt> & slot = cache[pluginName];
	if (!slot) {
		slot.reset(new CondoArt);
		LOG(INFO, "Loading YootTower residential textures from plugin %s", pluginName.c_str());
		slot->loaded = game->app.bitmaps.loadFromPlugin(pluginName, 1000, slot->empty, sf::Color(105, 33, 104));
		if (!slot->loaded)
			LOG(ERROR, "Failed to load empty condo texture (1000) from %s", pluginName.c_str());
		if (!game->app.bitmaps.loadFromPlugin(pluginName, 1100, slot->resident, sf::Color(255, 255, 255)))
			LOG(ERROR, "Failed to load resident texture (1100) from %s", pluginName.c_str());
	}
	emptyCondoTexture = &slot->empty;
	residentTexture = &slot->resident;
	// Skin::Yoot: the shared plugin sheet, whose windows follow the sky.
	if (game->skin == Skin::Yoot)
		if (const YootPluginView * v = yootPluginView(game->app, pluginName, kYootWindowKey))
			emptyCondoTexture = &v->texture;
	return slot->loaded;
}

void YootCondo::init()
{
	Item::init();

	loadTextures();

	variant = rand() % 3;
	occupied = false;
	updateLighting(game->time.getHour());
	rent = 5000;
	rentDeposit = rent;

	baseSprite.setTexture(*emptyCondoTexture);
	baseSprite.setOrigin({0.f, 24.f});
	// Skin::Yoot: the 24 px interior sits above the 4 px floor strip.
	const int lift = game->skin == Skin::Yoot ? kYootInteriorLift : 0;
	baseSprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36 - lift)});

	residentSprite.setTexture(*residentTexture);
	residentSprite.setOrigin({8.f, 24.f});

	updateSprite();
}

void YootCondo::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("rent", rent);
	xml.PushAttribute("rentDeposit", rentDeposit);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("lighting", lighting);
	xml.PushAttribute("occupied", occupied);
	xml.PushAttribute("population", population);
}

void YootCondo::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	const bool hasPopulation = (xml.FindAttribute("population") != NULL);
	rent = xml.IntAttribute("rent");
	rentDeposit = xml.IntAttribute("rentDeposit", xml.BoolAttribute("occupied") ? 150000 : rent);
	variant = xml.IntAttribute("variant");
	lighting = (LightingConditions)xml.IntAttribute("lighting");
	occupied = xml.BoolAttribute("occupied");
	if (occupied)
	{
		createOccupants();
		// An imported let home has an empty belong list (population 0);
		// it is occupied, so keep the residents just created.
		if (hasPopulation && xml.IntAttribute("population") > 0) population = xml.IntAttribute("population");
	}
	else
	{
		population = 0;
	}
	updateSprite();
}

void YootCondo::updateSprite()
{
	spriteNeedsUpdate = false;
	int stateIndex = 0;

	if (occupied) {
		switch (lighting) {
		case DAYTIME:
			stateIndex = 2;
			break;
		case LIT:
			stateIndex = 3;
			break;
		case NIGHT:
			stateIndex = 4;
			break;
		}
	} else {
		switch (lighting) {
		case DAYTIME:
			stateIndex = 0;
			break;
		case LIT:
		case NIGHT:
			stateIndex = 1;
			break;
		}
	}

	int frameIndex = variant * 5 + stateIndex;
	baseSprite.setTextureRect(sf::IntRect({0, frameIndex * 24}, {size.x * 8, 24}));
}

void YootCondo::render(sf::RenderTarget &target) const
{
	sf::Color origBaseColor = baseSprite.getColor();
	if (game->statusMode == Game::kHotel)
	{
		sf::Color composed = game->lighting.compose(origBaseColor);
		composed.r = (composed.r * 110) / 255;
		composed.g = (composed.g * 110) / 255;
		composed.b = (composed.b * 110) / 255;
		composed.a = (composed.a * 160) / 255;
		baseSprite.setColor(composed);
	}
	else
	{
		baseSprite.setColor(game->lighting.compose(origBaseColor));
	}
	target.draw(baseSprite);
	baseSprite.setColor(origBaseColor);
	game->drawnSprites++;

	// Occupants are drawn solely by Item::renderPeople's AnimPeple condo
	// band (former double-draw: this loop and renderPeople both rendered
	// every resident while they were home; adoption review row 11).
	Item::render(target);
}

void YootCondo::advance(double dt)
{
	if (game->eventSystem.tenantIdleFrozen()) return;
	// YootTower: Condo.t2p / HawaianCondo.t2p ChangeStatusOnIdle (Condo.dll decompile,
	// rules-agent/out/plg2/Condo.dll.decomp.c, the CollectEmployee branch): at the top of
	// the hour, 7:00 to 20:59, with the estimate above 200 (isAttractive), the residents
	// come out of the pool (TNDF 1, CollectEmployee 0x1005f5a0 -> CollectEmployeeFromPool
	// 0x10060a50) and the sale is collected. A unit whose hire fails stays empty, and a
	// partial hire houses fewer residents.
	const bool yootHireWindow = prototype->yoot && game->time.hour >= 7 && game->time.hour < 21 &&
		std::fmod(game->time.hour, 1.0) * 60.0 < 8.0;
	if (!occupied && (prototype->yoot ? yootHireWindow : (!game->time.isHoliday() && game->time.hour >= 7 && game->time.hour < 17)) &&
	    isAttractive())
	{
		if ((prototype->yoot || (game->time.checkTick(0.002) && (rand() % 10) == 0)) &&
		    (!prototype->yoot || game->eventSystem.people.hireBelong(this)))
		{
			occupied = true;
			variant = rand() % 3;
			spriteNeedsUpdate = true;
			moveIn();			createOccupants();
		}
	}

	if (occupied && game->time.checkHour(5) && game->time.day == 0)
	{
		if (!isAttractive())
		{
			occupied = false;
			removeOccupants();
			if (prototype->yoot) game->eventSystem.people.releaseBelong(this);
			spriteNeedsUpdate = true;
			population = 0;
			game->populationNeedsUpdate = true;
			moveOut();		}
	}

	if (occupied && game->time.checkHour(3))
	{
		generateJitters();
	}

	if (occupied)
	{
		moveOccupants();
	}

	if (updateLighting(game->time.getHour()))
	{
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

void YootCondo::generateJitters()
{
	while (!returnQueue.empty())
		returnQueue.pop();
	while (!departureQueue.empty())
		departureQueue.pop();

	for (YootCondoOccupant *person : occupants)
	{
		person->departureJitter = Math::randd(-0.1, 0.3);
		person->returnJitter = Math::randd(-0.1, 0.3);
		returnQueue.push(person);
		departureQueue.push(person);
	}
}

void YootCondo::moveOccupants()
{
	while (!departureQueue.empty())
	{
		YootCondoOccupant *c = departureQueue.top();
		if (game->time.hour > c->actualDepartureTime())
		{
			departureQueue.pop();
			c->journey.set(lobbyRoute);
		}
		else
			break;
	}

	while (!returnQueue.empty())
	{
		YootCondoOccupant *c = returnQueue.top();
		if (game->time.hour > c->actualReturnTime() && !lobbyRoute.empty())
		{
			returnQueue.pop();
			const Route &r = game->findRoute(this, game->mainLobby);
			if (r.empty())
			{
				LOG(DEBUG, "%p has no route to leave his or her Condo", c);
			}
			else
			{
				LOG(DEBUG, "%p leaving condo", c);
				c->journey.set(r);
			}
		}
		else
			break;
	}
}

bool YootCondo::updateLighting(double time)
{
	LightingConditions newLighting = lighting;
	if ((time < 7.0) || (time > 22.0))
	{
		newLighting = NIGHT;
	}
	else if (time < 19.0)
	{
		newLighting = DAYTIME;
	}
	else
	{
		newLighting = LIT;
	}

	bool retval = (newLighting != lighting);
	lighting = newLighting;
	return retval;
}

void YootCondo::addPerson(Person *p)
{
	Item::addPerson(p);
	spriteNeedsUpdate = true;
}

void YootCondo::moveIn()
{
	// YootTower: the condo is sold for its inMoney[selection] (Condo.t2p
	// $100,000..$200,000), like an office's occupancy payment (E4).
	rentDeposit = prototype->yoot ? prototype->yoot->inMoneyFor(game->yootPriceSelection(this)) : 0;
	if (rentDeposit <= 0) rentDeposit = prototype->price * 2;
	game->transferFunds(rentDeposit, "condo_sale", "Income from Yoot Condo sale");
}

void YootCondo::moveOut()
{
	game->transferFunds(-prototype->price, "condo_buyback", "Repurchased vacated Yoot Condo");
}

void YootCondo::createOccupants()
{
	unsigned numAdults = (unsigned)std::max(1, residentsToCreate());
	if (prototype->yoot && game->eventSystem.people.enabled())
	{
		// One resident per type hired from the pool (a reloaded unit hires again).
		if (game->eventSystem.people.belongOf(this).empty()) game->eventSystem.people.hireBelong(this);
		numAdults = (unsigned)game->eventSystem.people.belongOf(this).size();
	}
	Person::Type adults[] = {
		Person::kMan,
		Person::kWoman1,
		Person::kWoman2};

	for (unsigned i = 0; i < numAdults; ++i)
	{
		unsigned gender = std::max(Math::randi(0, 3) - 1, 0);
		double leavingTime = Math::randd(7.5, 9.5);
		double returnTime = Math::randd(17.5, 19.5);

		occupants.insert(new YootCondoOccupant(this, adults[gender], leavingTime, returnTime));
	}

	unsigned numKids = (hasFamilies() && !prototype->yoot) ? Math::randi(0, std::min(numAdults * 2, 6u)) : 0;
	Person::Type kids[] = {
		Person::kChild,
		Person::kWomanWithChild1,
		Person::kWomanWithChild2,
	};

	for (unsigned i = 0; i < numKids; ++i)
	{
		unsigned type = Math::randi(0, 2);
		double leavingTime = 7.5;
		double returnTime = (Math::randi(0, 1) == 0) ? 15.5 : 17.5;

		occupants.insert(new YootCondoOccupant(this, kids[type], leavingTime, returnTime));
	}

	population = occupants.size();
	game->populationNeedsUpdate = true;
}

void YootCondo::removeOccupants()
{
	for (Person *occupant : occupants)
	{
		// Residents out at work are not in this item (same rule as
		// Hotel::clearAll); removePerson asserts p->at == this.
		if (occupant->at == this)
			removePerson(occupant);		delete occupant;
	}
	occupants.clear();
}

void YootCondo::removePerson(Person *p)
{
	Item::removePerson(p);
	spriteNeedsUpdate = true;
}

bool YootCondo::isAttractive()
{
	if (prototype->yoot)
		return !lobbyRoute.empty() && yootKeepsTenants(occupied);
	return !lobbyRoute.empty();
}

double YootCondo::YootCondoOccupant::actualReturnTime() const
{
	return departureTime + departureJitter;
}

double YootCondo::YootCondoOccupant::actualDepartureTime() const
{
	return returnTime + returnJitter;
}

void YootCondo::evacuateForDisaster()
{
 departureQueue = decltype(departureQueue)();
 returnQueue = decltype(returnQueue)();
 for (auto *p : occupants) { p->state = Person::kIdle; }
 lighting = DAYTIME;
 spriteNeedsUpdate = true;
 updateSprite();
}
