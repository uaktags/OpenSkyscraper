/* Copyright © 2013 Fabian Schuiki */
#include "../Game.h"
#include "../Math/Rand.h"
#include "FastFood.h"
#include <string>

using namespace OT;
using namespace OT::Item;

FastFood::~FastFood()
{
	clearCustomers();
}

void FastFood::init()
{
	Item::init();

	variant = rand() % 5;
	open = false;

	sprite.setTexture(App->bitmaps["simtower/fastfood"]);
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

	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
	{
		Customer *customer = *c;

		// Persist customers that are still scheduled to arrive or are currently
		// eating. Customers already on their way home are skipped; they will be
		// cleaned up the next time the item opens.
		CustomerMetadataMap::iterator m = customerMetadata.find(customer);
		bool eating = (m != customerMetadata.end());
		bool arriving = (!eating && customer->at == NULL && customer->state == Person::kWandering);
		if (!eating && !arriving)
			continue;

		xml.OpenElement("customer");
		xml.PushAttribute("lobbyArrival", customer->arrivalTime);
		xml.PushAttribute("type", customer->type);
		xml.PushAttribute("state", customer->state);
		xml.PushAttribute("stress", customer->stress);
		xml.PushAttribute("eval", customer->eval);
		xml.PushAttribute("name", customer->name.c_str());
		xml.PushAttribute("from", customer->from.c_str());
		xml.PushAttribute("goingTo", customer->goingTo.c_str());
		if (eating)
		{
			xml.PushAttribute("phase", "eating");
			xml.PushAttribute("itemArrival", m->second.arrivalTime);
		}
		else
		{
			xml.PushAttribute("phase", "arriving");
		}
		xml.CloseElement();
	}
}

void FastFood::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant");
	open = xml.BoolAttribute("open");
	clearCustomers();

	for (tinyxml2::XMLElement *e = xml.FirstChildElement("customer"); e; e = e->NextSiblingElement("customer"))
	{
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
		std::string phase = e->Attribute("phase", "arriving");
		if (phase == "eating")
		{
			addPerson(c);
			customerMetadata[c].arrivalTime = e->DoubleAttribute("itemArrival");
		}
		else
		{
			arrivingCustomers.push(c);
		}
	}

	updateSprite();
}

void FastFood::updateSprite()
{
	spriteNeedsUpdate = false;
	int index = 3;
	if (open)
		index = std::min<int>((int)ceil(people.size() / 5.0), 2);
	sprite.setTextureRect(sf::IntRect({index * 128, variant * 24}, {128, 24}));
}

void FastFood::advance(double dt)
{
	// Open
	if (game->time.checkHour(10))
	{
		open = true;
		spriteNeedsUpdate = true;

		// Create new customers for today.
		int today = 10;
		clearCustomers();
		for (int i = 0; i < today; i++)
		{
			Customer *c = new Customer(this);
			c->arrivalTime = (game->time.year - 1) * 12 + (game->time.quarter - 1) * 3 + game->time.day + Math::randd(Time::hourToAbsolute(10), Time::hourToAbsolute(20));
			customers.insert(c);
			arrivingCustomers.push(c);
		}
	}

	// Close
	if (game->time.checkHour(21) && open)
	{
		open = false;
		population = customerMetadata.size();
		game->populationNeedsUpdate = true;
		spriteNeedsUpdate = true;

		game->transferFunds(population * 200 - 2000, "retail_income", "Income from Fast Food");
	}

	// Make customers arrive.
	while (!arrivingCustomers.empty())
	{
		Customer *c = arrivingCustomers.top();
		if (game->time.absolute > c->arrivalTime && !lobbyRoute.empty())
		{
			arrivingCustomers.pop();
			c->journey.set(lobbyRoute);
		}
		else
			break;
	}

	// Make customers leave once they're done.
	for (std::list<Person *>::iterator ip = eatingCustomers.begin(); ip != eatingCustomers.end();)
	{
		Person *p = *ip;
		CustomerMetadata &m = customerMetadata[p];
		if (game->time.absolute >= m.arrivalTime + 20 * Time::kBaseSpeed || !open)
		{
			const Route &r = game->findRoute(this, game->mainLobby); // Customers may leave for different destinations besides main lobby, so this is not precomputed
			if (r.empty())
			{
				LOG(DEBUG, "%p has no route to leave", p);
				ip++;
			}
			else
			{
				LOG(DEBUG, "%p leaving", p);
				ip = eatingCustomers.erase(ip);
				removePerson(p);
				p->state = Person::kReturning;
				p->from = prototype->name;
				p->goingTo = "Exit";
				p->journey.set(r);
			}
		}
		else
			break;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

void FastFood::addPerson(Person *p)
{
	Item::addPerson(p);
	p->state = Person::kLunch;
	p->eval = 50;
	p->addStress(-10);
	CustomerMetadata &m = customerMetadata[p];
	m.arrivalTime = game->time.absolute;
	eatingCustomers.push_back(p);
	spriteNeedsUpdate = true;
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
	eatingCustomers.clear();
	customers.clear();
	customerMetadata.clear();
}

FastFood::Customer::Customer(FastFood *item)
	: Person(item->game)
{
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
