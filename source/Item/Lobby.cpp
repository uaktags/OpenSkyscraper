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

	background.setTexture(App->bitmaps[p], true);
	overlay.setTexture(App->bitmaps[p], true);

	entrances[0].setPosition({(float)getRect().minX() * 8.f - 16.f, (float)-position.y * 36.f});
	entrances[1].setPosition({(float)getRect().maxX() * 8.f - 40.f, (float)-position.y * 36.f});
}

void Lobby::render(sf::RenderTarget &target) const
{
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
