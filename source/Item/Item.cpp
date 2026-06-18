/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <cassert>
#include <string>
#include "../Game.h"
#include "../Time.h"
#include "Item.h"

using namespace OT::Item;
using OT::Person;
using OT::Sprite;
using OT::int2;
using OT::rectd;
using OT::recti;
using std::string;

double Item::constructionDuration() const
{
	// ~2 game-hours plus a small per-tile surcharge so larger footprints
	// take a little longer. Tunable per-subclass.
	return OT::Time::hourToAbsolute(2.0 + 0.2 * size.x);
}

Item::~Item()
{
	sprites.clear();
	while (!people.empty())
		removePerson(*people.begin());
}

void Item::setPosition(int2 p)
{
	if (position != p)
	{
		position.x = p.x;
		position.y = p.y;
	}
}

int2 Item::getPosition() const
{
	return position;
}

int2 Item::getPositionPixels() const
{
	return int2(position.x * 8, position.y * 36);
}

recti Item::getRect() const
{
	return recti(getPosition(), int2(getSize().x, getSize().y));
}

recti Item::getPixelRect() const
{
	return recti(getPositionPixels(), int2(getSizePixels().x, getSizePixels().y));
}

void Item::addSprite(Sprite *sprite)
{
	assert(sprite);
	sprites.insert(sprite);
}

void Item::removeSprite(Sprite *sprite)
{
	assert(sprite);
	sprites.erase(sprite);
}

void Item::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
	render(target);
}

void Item::render(sf::RenderTarget &target) const
{
	// Construction placeholder: draw a hatched overlay over the item's
	// footprint and skip the real sprites. The base still increments
	// drawnSprites for parity with the regular path.
	if (underConstruction)
	{
		const int2 p = getPositionPixels();
		const sf::Vector2u s = getSizePixels();
		sf::RectangleShape overlay({static_cast<float>(s.x), static_cast<float>(s.y)});
		overlay.setPosition({static_cast<float>(p.x),
		                     static_cast<float>(-(p.y + static_cast<int>(s.y)))});
		overlay.setFillColor(sf::Color(170, 145, 75, 230));
		overlay.setOutlineColor(sf::Color(90, 70, 30));
		overlay.setOutlineThickness(1.f);
		target.draw(overlay);
		game->drawnSprites++;
		return;
	}

	for (SpriteSet::iterator s = sprites.begin(); s != sprites.end(); s++)
	{
		game->drawnSprites++;
		target.draw(**s);
	}

	if (!canHaulPeople() && position.y != 0 && prototype->icon != ICON_FLOOR && lobbyRoute.empty())
	{
		Sprite noroute;
		noroute.setTexture(app->bitmaps["noroute.png"]);
		sf::Vector2u size = noroute.getTexture().getSize();
		noroute.setOrigin({size.x / 2.f, size.y / 2.f});
		size = getSize();
		noroute.setPosition({size.x / 2.f, -size.y / 2.f});
		target.draw(noroute);
	}
}

void Item::encodeXML(tinyxml2::XMLPrinter &xml)
{
	xml.PushAttribute("type", prototype->id.c_str());
	xml.PushAttribute("x", position.x);
	xml.PushAttribute("y", position.y);
	xml.PushAttribute("evaluation", evaluation);
	if (underConstruction)
	{
		xml.PushAttribute("underConstruction", underConstruction);
		xml.PushAttribute("constructionEndTime", constructionEndTime);
	}
}

void Item::decodeXML(tinyxml2::XMLElement &xml)
{
	evaluation = xml.DoubleAttribute("evaluation", 50.0);
	// Saved state wins over the Factory's "fresh placement" default so a
	// save mid-construction restores correctly (and finished items stay
	// finished after a reload).
	underConstruction = xml.BoolAttribute("underConstruction", false);
	constructionEndTime = xml.DoubleAttribute("constructionEndTime", 0.0);
}

void Item::addPerson(Person *p)
{
	assert(p);
	assert(!p->at);
	p->at = this;
	people.insert(p);
}

void Item::removePerson(Person *p)
{
	assert(p);
	assert(p->at == this);
	p->at = NULL;
	people.erase(p);
}

rectd Item::getMouseRegion()
{
	int2 p = getPositionPixels();
	sf::Vector2u s = getSizePixels();
	return rectd(p.x, p.y, s.x, s.y);
}

void Item::updateRoutes()
{
	if (!canHaulPeople() && position.y != 0)
	{
		lobbyRoute = game->findRoute(game->mainLobby, this);
	}
	else
	{
		lobbyRoute.clear();
	}
}
