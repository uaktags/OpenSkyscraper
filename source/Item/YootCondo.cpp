#include "YootCondo.h"
#include "../BitmapManager.h"
#include "../Game.h"
#include "../Math/Rand.h"

using namespace OT;
using namespace OT::Item;

sf::Texture YootCondo::emptyCondoTexture;
sf::Texture YootCondo::residentTexture;
bool YootCondo::texturesLoaded = false;

YootCondo::~YootCondo()
{
	removeOccupants();
}

bool YootCondo::loadTextures(Game *game)
{
	if (texturesLoaded)
		return true;

	// BitmapManager::loadFromPlugin is the canonical PE-bitmap loader
	// (used to live here as YootCondo::loadPEBitmap). The plugin is
	// resolved through PluginManager's catalog, so we no longer need to
	// walk data search paths manually.
	const std::string pluginName = "Condo.t2p";
	LOG(INFO, "Loading YootTower Condo textures from plugin %s", pluginName.c_str());

	if (!game->app.bitmaps.loadFromPlugin(pluginName, 1000, emptyCondoTexture, sf::Color(105, 33, 104))) {
		LOG(ERROR, "Failed to load empty condo texture (1000) from %s", pluginName.c_str());
		return false;
	}
	if (!game->app.bitmaps.loadFromPlugin(pluginName, 1100, residentTexture, sf::Color(255, 255, 255))) {
		LOG(ERROR, "Failed to load resident texture (1100) from %s", pluginName.c_str());
		return false;
	}

	texturesLoaded = true;
	return true;
}

void YootCondo::init()
{
	Item::init();

	loadTextures(game);

	variant = rand() % 3;
	occupied = false;
	updateLighting(game->time.getHour());
	rent = 5000;
	rentDeposit = rent;

	baseSprite.setTexture(emptyCondoTexture);
	baseSprite.setOrigin({0.f, 24.f});
	baseSprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});

	residentSprite.setTexture(residentTexture);
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
	rentDeposit = xml.IntAttribute("rentDeposit");
	variant = xml.IntAttribute("variant");
	lighting = (LightingConditions)xml.IntAttribute("lighting");
	occupied = xml.BoolAttribute("occupied");
	if (occupied)
	{
		createOccupants();
		population = hasPopulation ? xml.IntAttribute("population") : population;
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
	baseSprite.setTextureRect(sf::IntRect({0, frameIndex * 24}, {128, 24}));
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
	if (!occupied && !game->time.isHoliday() && game->time.hour >= 7 && game->time.hour < 17 && isAttractive())
	{
		if (game->time.checkTick(0.002) && (rand() % 10) == 0)
		{
			occupied = true;
			variant = rand() % 3;
			spriteNeedsUpdate = true;
			rentDeposit = prototype->price * 2;
			game->transferFunds(rentDeposit, "condo_sale", "Income from Yoot Condo sale");
			createOccupants();
		}
	}

	if (occupied && game->time.checkHour(5) && game->time.day == 0)
	{
		if (!isAttractive())
		{
			occupied = false;
			removeOccupants();
			spriteNeedsUpdate = true;
			population = 0;
			game->populationNeedsUpdate = true;
			game->transferFunds(-prototype->price, "condo_buyback", "Repurchased vacated Yoot Condo");
		}
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

void YootCondo::createOccupants()
{
	unsigned numAdults = Math::randi(1, 4);
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

	unsigned numKids = Math::randi(0, std::min(numAdults * 2, 6u));
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
		removePerson(occupant);
		delete occupant;
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
