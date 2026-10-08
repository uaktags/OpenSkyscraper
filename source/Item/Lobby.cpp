/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <cassert>
#include "../Game.h"
#include "Lobby.h"

using namespace OT;
using namespace Item;

Lobby::~Lobby()
{
}

void Lobby::init()
{
	Item::init();
	bool groundLobby = (position.y == 0);

	if (groundLobby)
	{
		assert(game->mainLobby == NULL);
		game->mainLobby = this;

		for (int i = 0; i < 2; i++)
		{
			entrances[i].setTexture(App->bitmaps["simtower/deco/entrances"], true);
			entrances[i].setOrigin({0.f, 36.f});
			entrances[i].setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});
			entrances[i].setTextureRect(sf::IntRect({i * 56, 0}, {56, 36}));
			addSprite(&entrances[i]);
		}
	}

	updateSprite();
}

const static double kAtriumWalkTime = 1.5;

void Lobby::addPerson(Person *p)
{
	transitionTimes[p] = 0;
	Item::addPerson(p);
}

void Lobby::removePerson(Person *p)
{
	transitionTimes.erase(p);
	Item::removePerson(p);
}

void Lobby::advance(double dt)
{
	Item::advance(dt);
	// The atrium is open: a pedestrian whose journey crosses the lobby takes
	// a short walk per floor it covers (mirrors Stairlike's transition) and
	// then continues at the next route node - typically boarding an elevator
	// on a lobby extension floor.
	double dta = game->time.dta / Time::kBaseSpeed;
	for (People::iterator ip = people.begin(); ip != people.end();)
	{
		Person *p = *(ip++);
		if (p->journey.item != this) continue;
		if ((transitionTimes[p] += dta) < kAtriumWalkTime) continue;
		transitionTimes.erase(p);
		p->journey.next();
	}
}

void Lobby::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("width", size.x);
	xml.PushAttribute("height", size.y);
}

void Lobby::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	size.x = xml.IntAttribute("width");
	size.y = xml.IntAttribute("height", 1);
	updateSprite();
}

void Lobby::updateSprite()
{
	bool groundLobby = (position.y == 0);
	Path p;
	if (size.y > 1) {
		p = "simtower/lobby/high";
	} else {
		p = (groundLobby ? "simtower/lobby/normal" : "simtower/lobby/sky");
	}

	const char *edition = size.y > 1 ? "lobby_high" : (groundLobby ? "lobby_normal" : "lobby_sky");
	background.setTexture(App->bitmaps.editionTexture(edition, p), true);
	overlay.setTexture(App->bitmaps.editionTexture(edition, p), true);

	entrances[0].setPosition({(float)getRect().minX() * 8.f - 16.f, (float)-position.y * 36.f});
	entrances[1].setPosition({(float)getRect().maxX() * 8.f - 40.f, (float)-position.y * 36.f});
}

void Lobby::render(sf::RenderTarget &target) const
{
	// YootTower skin: the Floor.t2p lobby view for this height (its left
	// part carries the entrance), the world slab on top and, on the ground
	// lobby, the 1003 awnings either side. Heights the plugin lacks
	// (KFloor.t2p only has 1-row lobbies) keep the OpenSky art.
	const YootWorldArt * yoot = game->yootSkinArt();
	if (yoot && !underConstruction && yoot->hasLobbyView(size.y) && yoot->hasSlab())
	{
		sf::Color tint = game->lighting.compose(sf::Color::White);
		if (game->statusMode == Game::kHotel)
			tint = sf::Color(tint.r * 110 / 255, tint.g * 110 / 255, tint.b * 110 / 255, 160);
		const recti r = getRect();
		const float top = -(position.y + size.y) * 36.0f;
		// A tenant built through the lobby (Kegon's Attraction reaches the
		// top floor) owns its cells: the lobby is drawn only around it.
		std::vector<std::pair<int, int> > runs(1, std::make_pair(r.minX(), r.maxX()));
		for (int row = position.y; row < position.y + size.y; ++row)
		{
			Game::ItemSetByInt::const_iterator on = game->itemsByFloor.find(row);
			if (on == game->itemsByFloor.end()) continue;
			for (Item * other : on->second)
			{
				if (other == this || other->yootFloorCover() == 0) continue;
				std::vector<std::pair<int, int> > rest;
				for (const std::pair<int, int> & run : runs)
				{
					if (other->getRect().minX() > run.first) rest.push_back(std::make_pair(run.first, std::min(run.second, other->getRect().minX())));
					if (other->getRect().maxX() < run.second) rest.push_back(std::make_pair(std::max(run.first, other->getRect().maxX()), run.second));
				}
				runs.swap(rest);
			}
		}
		// Roof (8 px), the lobby view (36*h - 12 px), the floor strip (4 px).
		for (const std::pair<int, int> & run : runs)
		{
			if (run.first >= run.second) continue;
			yoot->drawRoof(target, run.first * 8.0f, run.second * 8.0f, top, tint);
			yoot->drawLobbyView(target, size.y, run.first * 8.0f, run.second * 8.0f, top + 8.0f, tint, run.first == r.minX());
			yoot->drawFloorStrip(target, run.first * 8.0f, run.second * 8.0f, top + size.y * 36.0f - 4.0f, tint);
		}
		if (position.y == 0)
			yoot->drawTents(target, r.minX() * 8.0f, r.maxX() * 8.0f, 0.0f, tint);
		game->drawnSprites += 3;
		return;
	}

	Item::render(target);

	const sf::View &view = target.getView();
	sf::Vector2f center = view.getCenter();
	sf::Vector2f size_view = view.getSize();
	sf::Vector2f dmax = center + size_view / 2.f;
	sf::Vector2f dmin = center - size_view / 2.f;

	recti rect = getRect();

	int minx = std::max<int>(floor(dmin.x / 256), floor(rect.minX() / 32.0));
	int maxx = std::min<int>(ceil(dmax.x / 256), ceil(rect.maxX() / 32.0));

	sf::Color statusTint = sf::Color::White;
	if (game->statusMode == Game::kHotel)
	{
		statusTint = sf::Color(110, 110, 110, 160);
	}

	int ratingY = 0;
	if (game->rating == 2)
		ratingY = 1;
	if (game->rating >= 3)
		ratingY = 2;

	struct Segment {
		int textureY;
		int height;
		float screenYOffset;
	};
	std::vector<Segment> segments;

	if (size.y == 3) {
		segments.push_back({ratingY * 108, 108, 0.f});
	} else if (size.y == 2) {
		segments.push_back({ratingY * 108 + 48, 60, 0.f});
		segments.push_back({ratingY * 108, 12, -60.f});
	} else {
		segments.push_back({ratingY * 36, 36, 0.f});
	}

	for (const auto &seg : segments)
	{
		Sprite b = background;
		b.setOrigin({0.f, static_cast<float>(seg.height)});
		if (statusTint != sf::Color::White)
		{
			b.setColor(statusTint);
		}
		for (int x = minx; x < maxx; x++)
		{
			int cutoffLeft = 0;
			int cutoffRight = 0;

			cutoffLeft = std::max(0, rect.minX() * 8 - x * 256);
			cutoffRight = std::max(0, (x + 1) * 256 - rect.maxX() * 8);

			b.setTextureRect(sf::IntRect({56 + cutoffLeft, seg.textureY}, {256 - cutoffLeft - cutoffRight, seg.height}));
			b.setPosition({(float)x * 256.f + cutoffLeft, (float)-position.y * 36.f + seg.screenYOffset});
			target.draw(b);
			game->drawnSprites++;
		}

		Sprite o = overlay;
		o.setOrigin({0.f, static_cast<float>(seg.height)});
		if (statusTint != sf::Color::White)
		{
			o.setColor(statusTint);
		}
		int overlayWidth = std::min(56, rect.size.x * 8 - 16);
		o.setTextureRect(sf::IntRect({0, seg.textureY}, {overlayWidth, seg.height}));
		o.setPosition({static_cast<float>(position.x * 8 + 8), static_cast<float>(-position.y * 36 + seg.screenYOffset)});
		target.draw(o);
		game->drawnSprites++;
	}
}
