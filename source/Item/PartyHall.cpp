#include "../Game.h"
#include "../Math/Rand.h"
#include "PartyHall.h"

using namespace OT;
using namespace OT::Item;
using OT::Math::randd;

// Attendance model: per-visitor fee net of the per-event cost.
static const int kVisitorFee   = 200;
static const int kEventCost    = 1500;
static const int kVisitorsPerEvent = 25;

PartyHall::~PartyHall()
{
	clearVisitors();
}

void PartyHall::init()
{
	Item::init();

	open = false;

	sprite.setTexture(App->bitmaps["simtower/partyhall"]);
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
}

void PartyHall::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open");
	updateSprite();
}

void PartyHall::updateSprite()
{
	spriteNeedsUpdate = false;
	int index = (open ? 1 : 0);
	sprite.setTextureRect(sf::IntRect({index * 192, 0}, {192, 60}));
	sprite.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
}

void PartyHall::advance(double dt)
{
	// Two parties per day: afternoon 13:00-17:00 and evening 19:00-23:00.
	if (game->time.checkHour(13) || game->time.checkHour(19))
	{
		open = true;
		spriteNeedsUpdate = true;

		// Spawn visitors for this session.
		clearVisitors();
		if (!lobbyRoute.empty())
		{
			for (int i = 0; i < kVisitorsPerEvent; ++i)
			{
				Visitor *v = new Visitor(this);
				visitors.insert(v);
				v->journey.set(lobbyRoute);
			}
		}
	}

	// Close
	if ((game->time.checkHour(17) || game->time.checkHour(23)) && open)
	{
		open = false;
		spriteNeedsUpdate = true;

		// Attendance-based income: visitors who actually made it into the
		// hall (counted via `people`) times the per-visitor fee, less the
		// fixed event cost.
		int attendees = static_cast<int>(people.size());
		int net = attendees * kVisitorFee - kEventCost;
		game->transferFunds(net, "entertainment_income", "Income from Party Hall");

		// Send visitors home.
		const Route &r = game->findRoute(this, game->mainLobby);
		for (Visitors::iterator it = visitors.begin(); it != visitors.end(); ++it)
		{
			Visitor *v = *it;
			if (!r.empty())
			{
				v->state = Person::kReturning;
				v->from = prototype->name;
				v->goingTo = "Exit";
				removePerson(v);
				v->journey.set(r);
			}
		}
		// Visitors will be cleared on the next opening or on destruction.
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

Path PartyHall::getRandomBackgroundSoundPath()
{
	if (!open)
		return "";
	return "simtower/partyhall";
}

void PartyHall::addPerson(Person *p)
{
	Item::addPerson(p);
	p->state = Person::kShopping;
	p->eval = 60;
	p->addStress(-15);
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
