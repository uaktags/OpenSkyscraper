#include "../Game.h"
#include "../Math/Rand.h"
#include "Cinema.h"
#include <string>

using namespace OT;
using namespace OT::Item;

Cinema::~Cinema()
{
}

void Cinema::init()
{
	Item::init();

	open = false;
	playing = false;
	intermission = false;
	movieType = rand() % 15;
	animation = 0;
	animationFrame = 0;

	hallSprite.setTexture(App->bitmaps["simtower/cinema/hall"]);
	hallSprite.setOrigin({0.f, 60.f});
	hallSprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});
	screenSprite.setTexture(App->bitmaps["simtower/cinema/screens"]);
	screenSprite.setOrigin({0.f, 60.f});
	screenSprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});
}

void Cinema::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("open", open);
	xml.PushAttribute("playing", playing);
	xml.PushAttribute("intermission", intermission);
	xml.PushAttribute("movie", movieType);

	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
	{
		Customer *customer = *c;
		if (customer->at != this)
			continue;

		xml.OpenElement("customer");
		xml.PushAttribute("type", customer->type);
		xml.PushAttribute("state", customer->state);
		xml.PushAttribute("stress", customer->stress);
		xml.PushAttribute("eval", customer->eval);
		xml.PushAttribute("name", customer->name.c_str());
		xml.PushAttribute("from", customer->from.c_str());
		xml.PushAttribute("goingTo", customer->goingTo.c_str());
		xml.CloseElement();
	}
}

void Cinema::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open");
	playing = xml.BoolAttribute("playing");
	intermission = xml.BoolAttribute("intermission", false);
	movieType = xml.IntAttribute("movie");
	clearCustomers();

	for (tinyxml2::XMLElement *e = xml.FirstChildElement("customer"); e; e = e->NextSiblingElement("customer"))
	{
		Customer *c = new Customer(this);
		c->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		c->state = (Person::State)e->IntAttribute("state", Person::kShopping);
		c->stress = e->DoubleAttribute("stress", 0.0);
		c->eval = e->DoubleAttribute("eval", 0.0);
		c->name = e->Attribute("name", "");
		c->from = e->Attribute("from", "");
		c->goingTo = e->Attribute("goingTo", "");

		customers.insert(c);
		if (open)
			addPerson(c);
	}

	updateSprite();
}

void Cinema::updateSprite()
{
	spriteNeedsUpdate = false;
	int hallIndex = 0;
	int screenIndex = 0;
	if (open)
	{
		if (playing)
		{
			if (intermission)
			{
				hallIndex = 2;
				screenIndex = 2;
			}
			else
			{
				hallIndex = 3 + animationFrame;
				screenIndex = 3 + movieType;
			}
		}
		else
		{
			hallIndex = (people.size() > 0 ? 2 : 1); // TODO: make this based on the number of guests
			screenIndex = hallIndex;
		}
	}
	hallSprite.setTextureRect(sf::IntRect({hallIndex * 192, 0}, {192, 60}));
	// hallSprite.resize(192, 60);
	screenSprite.setTextureRect(sf::IntRect({screenIndex * 56, 0}, {56, 60}));
	// screenSprite.resize(56, 60);
}

void Cinema::advance(double dt)
{
	// Open
	if (game->time.checkHour(13) || game->time.checkHour(19))
	{
		open = true;
		playing = false;
		intermission = false;
		spriteNeedsUpdate = true;

		// Fill in the customers for this screening.
		clearCustomers();
		const int numCustomers = 50;
		for (int i = 0; i < numCustomers; i++)
		{
			Customer *p = new Customer(this);
			customers.insert(p);

			// Make the customer journey to the cinema immediately.
			p->journey.set(lobbyRoute);
		}
	}

	// Start Screening
	if ((game->time.checkHour(15) || game->time.checkHour(21)) && open)
	{
		playing = true;
		intermission = false;
		spriteNeedsUpdate = true;
		// game->timeWindow.showMessage("Movie starts at the Movie Theatre");
	}

	// Intermission
	if ((game->time.checkHour(16) || game->time.checkHour(22)) && open && playing)
	{
		if (!intermission)
		{
			intermission = true;
			spriteNeedsUpdate = true;
			game->playOnce("simtower/doorbell");

			for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
			{
				Customer *p = *c;
				if (p->at == this)
				{
					p->eval = std::min(p->eval + 15.0, 100.0);
					p->addStress(-15.0);
				}
			}
		}
	}

	// Close
	if ((game->time.checkHour(17) || game->time.checkHour(23)) && open)
	{
		open = false;
		playing = false;
		intermission = false;
		spriteNeedsUpdate = true;

		// Attendance-based income: count customers who actually reached the
		// theatre (those still in `people`), times the ticket price, less the
		// fixed royalty per screening. Replaces the previous flat calculation
		// that credited every spawned customer regardless of whether they
		// found a route in.
		const int kTicketPrice = 80;
		const int kScreeningFee = 2000;
		int attendees = static_cast<int>(people.size());
		int net = attendees * kTicketPrice - kScreeningFee;
		game->transferFunds(net, "entertainment_income", "Income from Movie Theatre");

		// Make the customers leave.
		const Route &r = game->findRoute(this, game->mainLobby);
		for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
		{
			Customer *p = *c;
			if (r.empty())
			{
				LOG(DEBUG, "%p has no route to leave", p);
			}
			else
			{
				LOG(DEBUG, "%p leaving", p);
				p->state = Person::kReturning;
				p->from = prototype->name;
				p->goingTo = "Exit";
				if (p->at == this)
				{
					removePerson(p);
				}
				p->journey.set(r);
			}
		}
	}

	// Animate the sprite.
	animation = fmod(animation + dt, 1);
	int af = floor(animation * 2);
	if (af != animationFrame)
	{
		animationFrame = af;
		spriteNeedsUpdate = true;
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

Path Cinema::getRandomBackgroundSoundPath()
{
	if (!open || !playing || intermission)
		return "";
	char name[128];
	snprintf(name, 128, "simtower/cinema/movie%i", movieType);
	return name;
}

void Cinema::addPerson(Person *p)
{
	Item::addPerson(p);
	p->state = Person::kShopping;
	p->eval = 60;
	p->addStress(-20);
	spriteNeedsUpdate = true;
}

void Cinema::removePerson(Person *p)
{
	Item::removePerson(p);
	spriteNeedsUpdate = true;
}

Cinema::Customer::Customer(Cinema *item)
	: Person(item->game)
{
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	from = "City";
	goingTo = item->prototype->name;
}

/** Removes all customers from the item. */
void Cinema::clearCustomers()
{
	for (Customers::iterator c = customers.begin(); c != customers.end(); c++)
		delete *c;
	customers.clear();
}
