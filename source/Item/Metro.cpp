#include "../Game.h"
#include "../Math/Rand.h"
#include "../NameManager.h"
#include "../Time.h"
#include "Metro.h"

#include <vector>

using namespace OT;
using namespace OT::Item;
using OT::Math::randd;

// Train cadence in absolute-time units. A train dwells at the platform for
// roughly 10 minutes, then is absent for ~30 minutes before the next arrives.
// Tuned from initial values (0.02 / 0.08) which were ~3x too long — at 1x
// speed a full cycle took most of the afternoon, so visitors never got a
// second train to leave on. 0.007/0.020 maps to ~10 min / ~30 min.
static const double kTrainDwellAbs = Time::hourToAbsolute(10.0 / 60.0);
static const double kTrainGapAbs   = Time::hourToAbsolute(0.5);

// Dwell window the visitor spends at a commercial venue before heading back.
static const double kVisitorMinDwell = Time::hourToAbsolute(0.25);
static const double kVisitorMaxDwell = Time::hourToAbsolute(0.75);

// Revenue per boarding passenger on departure.
static const int kFarePerBoarding = 50;

Metro::~Metro()
{
	clearAllVisitors();
}

void Metro::init()
{
	Item::init();

	open = true;
	trainPresent = false;
	nextTrainTime = game->time.absolute + kTrainGapAbs;

	station.setTexture(App->bitmaps["simtower/metro/station"]);
	station.setOrigin({0.f, 66.f});
	station.setPosition({0.f, 0.f});
	platform.setTexture(App->bitmaps["simtower/metro/station"]);
	platform.setOrigin({0.f, 30.f});
	platform.setPosition({0.f, 0.f});
	addSprite(&station);
	addSprite(&platform);
	spriteNeedsUpdate = true;

	// The construction flow already rejects a second Metro (Game.cpp:424-427
	// with "Only one Metro Station allowed"). This branch is defensive for
	// save/reload and any future code paths that bypass the construction
	// gate; in that case log and refuse to clobber the existing station.
	if (game->metroStation != NULL) {
		LOG(WARNING, "Metro::init() called while metroStation already set; "
		             "keeping the existing station and ignoring this one");
	} else {
		game->metroStation = this;
	}

	updateSprite();
}

void Metro::scheduleNextTrain()
{
	nextTrainTime = game->time.absolute + (trainPresent ? kTrainDwellAbs : kTrainGapAbs);
}

void Metro::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("open", open);
	xml.PushAttribute("trainPresent", trainPresent);
	xml.PushAttribute("nextTrainTime", nextTrainTime);

	for (std::set<Visitor *>::iterator it = visitors.begin(); it != visitors.end(); ++it)
	{
		Visitor *v = *it;
		xml.OpenElement("visitor");
		xml.PushAttribute("arrivalTime", v->arrivalTime);
		xml.PushAttribute("departTime", v->departTime);
		xml.PushAttribute("returnedToMetro", v->returnedToMetro);
		xml.PushAttribute("type", v->type);
		xml.PushAttribute("state", v->state);
		xml.PushAttribute("stress", v->stress);
		xml.PushAttribute("eval", v->eval);
		xml.PushAttribute("name", v->name.c_str());
		xml.PushAttribute("from", v->from.c_str());
		xml.PushAttribute("goingTo", v->goingTo.c_str());
		xml.CloseElement();
	}
}

void Metro::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	open = xml.BoolAttribute("open");
	trainPresent = xml.BoolAttribute("trainPresent");
	nextTrainTime = xml.DoubleAttribute("nextTrainTime", game->time.absolute + kTrainGapAbs);
	clearAllVisitors();

	for (tinyxml2::XMLElement *e = xml.FirstChildElement("visitor"); e; e = e->NextSiblingElement("visitor"))
	{
		Visitor *v = new Visitor(this);
		v->arrivalTime = e->DoubleAttribute("arrivalTime");
		v->departTime = e->DoubleAttribute("departTime");
		v->returnedToMetro = e->BoolAttribute("returnedToMetro", false);
		v->type = (Person::Type)e->IntAttribute("type", Person::kMan);
		v->state = (Person::State)e->IntAttribute("state", Person::kWandering);
		v->stress = e->DoubleAttribute("stress", 0.0);
		v->eval = e->DoubleAttribute("eval", 0.0);
		v->name = e->Attribute("name", "");
		v->from = e->Attribute("from", "");
		v->goingTo = e->Attribute("goingTo", "");
		visitors.insert(v);
	}

	updateSprite();
}

void Metro::updateSprite()
{
	spriteNeedsUpdate = false;
	int stationIndex = 2, platformIndex = 2;
	if (open)
	{
		stationIndex = 1;
		platformIndex = (trainPresent ? 0 : 1);
	}

	station.setTextureRect(sf::IntRect({stationIndex * 240, 0}, {240, 66}));
	platform.setTextureRect(sf::IntRect({platformIndex * 240, 66}, {240, 30}));
	station.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
	platform.setPosition({static_cast<float>(getPositionPixels().x), static_cast<float>(-getPositionPixels().y)});
}

OT::Item::Item * Metro::pickDestinationFor(Visitor *v)
{
	// Target underground commercial venues specifically.
	static const char *typeIds[] = {"fastfood", "restaurant", "cinema", "partyhall"};
	std::vector<OT::Item::Item *> reachable;

	for (size_t i = 0; i < sizeof(typeIds) / sizeof(typeIds[0]); ++i)
	{
		Game::ItemSetByString::const_iterator bucket = game->itemsByType.find(typeIds[i]);
		if (bucket == game->itemsByType.end()) continue;
		for (Game::ItemSet::const_iterator it = bucket->second.begin(); it != bucket->second.end(); ++it)
		{
			Item *item = *it;
			if (item->position.y < 0 && !game->findRoute(this, item).empty())
				reachable.push_back(item);
		}
	}

	if (reachable.empty()) return NULL;
	return reachable[rand() % reachable.size()];
}

void Metro::spawnVisitors(int count)
{
	if (lobbyRoute.empty()) return;

	for (int i = 0; i < count; ++i)
	{
		Item *dest = pickDestinationFor(NULL);
		if (!dest) continue;

		Visitor *v = new Visitor(this);
		v->arrivalTime = game->time.absolute;
		v->departTime = game->time.absolute + randd(kVisitorMinDwell, kVisitorMaxDwell);
		v->state = Person::kCommuting;
		v->goingTo = dest->prototype->name;
		v->from = "Metro";

		const Route &r = game->findRoute(this, dest);
		if (r.empty())
		{
			delete v;
			continue;
		}
		v->journey.set(r);
		visitors.insert(v);
	}
}

void Metro::boardReturnedVisitors()
{
	// Train departs: any visitor currently at the platform boards and is
	// removed from the simulation. Each boarding yields fare revenue.
	int boardings = 0;
	for (std::set<Visitor *>::iterator it = visitors.begin(); it != visitors.end();)
	{
		Visitor *v = *it;
		if (v->returnedToMetro || v->at == this)
		{
			++boardings;
			delete v;
			it = visitors.erase(it);
		}
		else
		{
			++it;
		}
	}

	if (boardings > 0)
	{
		game->transferFunds(boardings * kFarePerBoarding, "metro_fare", "Metro passenger fares");
		LOG(DEBUG, "Metro trainedeparted with %d boarding passenger(s)", boardings);
	}
}

void Metro::clearAllVisitors()
{
	for (std::set<Visitor *>::iterator it = visitors.begin(); it != visitors.end(); ++it)
		delete *it;
	visitors.clear();
}

void Metro::advance(double dt)
{
	// Open
	if (game->time.checkHour(7))
	{
		open = true;
		spriteNeedsUpdate = true;
	}

	// Close
	if (game->time.checkHour(23) && open)
	{
		open = false;
		// Send any in-station train away and clear stranded visitors so the
		// closed platform doesn't accumulate people overnight.
		if (trainPresent)
		{
			trainPresent = false;
			nextTrainTime = game->time.absolute + kTrainGapAbs;
			spriteNeedsUpdate = true;
		}
	}

	if (open)
	{
		// Train arrival / departure cycle.
		if (game->time.absolute >= nextTrainTime)
		{
			if (!trainPresent)
			{
				trainPresent = true;
				spriteNeedsUpdate = true;
				spawnVisitors(static_cast<int>(randd(2, 6)));
			}
			else
			{
				trainPresent = false;
				spriteNeedsUpdate = true;
				boardReturnedVisitors();
			}
			scheduleNextTrain();
		}

		// Drive visitor state transitions.
		for (std::set<Visitor *>::iterator it = visitors.begin(); it != visitors.end(); ++it)
		{
			Visitor *v = *it;

			// Visitor has reached their commercial destination - mark them
			// as shopping and arm the depart timer when they arrive.
			if (v->state == Person::kCommuting && v->at && v->at != this)
			{
				v->state = Person::kShopping;
				v->addStress(-5);
			}

			// Time to head back to the metro.
			if (v->state == Person::kShopping && game->time.absolute >= v->departTime)
			{
				Item *from = v->at ? v->at : game->mainLobby;
				const Route &r = game->findRoute(from, this);
				if (!r.empty())
				{
					v->state = Person::kReturning;
					v->goingTo = "Metro";
					v->journey.set(r);
				}
				else
				{
					// No way back; force-flag so the next departure cleans them up.
					v->returnedToMetro = true;
				}
			}
		}
	}

	if (spriteNeedsUpdate)
		updateSprite();
}

void Metro::addPerson(Person *p)
{
	Item::addPerson(p);

	Visitor *v = dynamic_cast<Visitor *>(p);
	if (v && v->state == Person::kReturning)
	{
		v->returnedToMetro = true;
		v->state = Person::kIdle;
	}
}

void Metro::removePerson(Person *p)
{
	Item::removePerson(p);
}

Metro::Visitor::Visitor(Metro *m)
	: Person(m->game)
{
	arrivalTime = 0;
	departTime = 0;
	returnedToMetro = false;
	Type types[] = {kMan, kWoman1, kWoman2, kChild};
	type = types[rand() % 4];
	from = "Metro";
	goingTo = "Tower";
}
