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
		// Compose the placeholder color with the global Lighting tint so
		// construction sites also respect day/night/rain (Phase 3.4).
		const sf::Color base(170, 145, 75, 230);
		overlay.setFillColor(game->lighting.compose(base));
		overlay.setOutlineColor(sf::Color(90, 70, 30));
		overlay.setOutlineThickness(1.f);
		target.draw(overlay);
		game->drawnSprites++;
		return;
	}

	// Apply the global Lighting tint to each sprite. Sprites keep their
	// own color elsewhere (often white, sometimes a subclass-specific
	// tint), so we save/restore around the draw to avoid compounding the
	// tint across frames. Phase 3.4.
	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255));
	for (SpriteSet::iterator s = sprites.begin(); s != sprites.end(); s++)
	{
		game->drawnSprites++;
		if (tinted)
		{
			const sf::Color orig = (*s)->getColor();
			(*s)->setColor(game->lighting.compose(orig));
			target.draw(**s);
			(*s)->setColor(orig);
		}
		else
		{
			target.draw(**s);
		}
	}

	if (!canHaulPeople() && position.y != 0 && prototype->icon != ICON_FLOOR && lobbyRoute.empty())
	{
		Sprite noroute;
		noroute.setTexture(app->bitmaps["noroute.png"]);
		sf::Vector2u size = noroute.getTexture().getSize();
		noroute.setOrigin({size.x / 2.f, size.y / 2.f});
		size = getSize();
		noroute.setPosition({size.x / 2.f, -size.y / 2.f});
		if (tinted) noroute.setColor(game->lighting.compose(sf::Color::White));
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
