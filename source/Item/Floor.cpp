#include <cassert>
#include <algorithm>
#include <vector>
#include "../Game.h"
#include "Floor.h"

using namespace OT;
using namespace Item;

Floor::~Floor() {}

void Floor::init()
{
	Item::init();

	layer = -1;

	sf::Texture &floor = App->bitmaps["simtower/floor"];
	background.setTexture(floor);
	background.setTextureRect(sf::IntRect({0, 0}, {8, 36}));
	background.setOrigin({0.f, static_cast<float>(floor.getSize().y)});
	background.setScale({8.0f / floor.getSize().x, 36.0f / floor.getSize().y});
	background.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});

	ceiling.setTexture(floor);
	ceiling.setTextureRect(sf::IntRect({0, 0}, {8, 12}));
	ceiling.setOrigin({0.f, static_cast<float>(floor.getSize().y)});
	ceiling.setScale({8.0f / floor.getSize().x, 36.0f / floor.getSize().y});
	ceiling.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});

	interval.insert(position.x);
	interval.insert(getRect().maxX());

	updateSprite();
}

void Floor::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("width", size.x);
}

void Floor::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	size.x = xml.IntAttribute("width");
	updateSprite();
}

void Floor::updateSprite() {}

void Floor::render(sf::RenderTarget &target) const
{
	Sprite b = background;
	Sprite c = ceiling;
	// Compose floor sprites with the global Lighting tint (Phase 3.4).
	// Floor is the single biggest visual surface in the tower, so it
	// needs the day/night/rain treatment even though it overrides
	// Item::render entirely.
	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255));
	if (tinted)
	{
		b.setColor(game->lighting.compose(b.getColor()));
		c.setColor(game->lighting.compose(c.getColor()));
	}

	struct Span
	{
		int left;
		int right;
	};
	std::vector<Span> occupied;
	Game::ItemSetByInt::const_iterator floorItems = game->itemsByFloor.find(position.y);
	if (floorItems != game->itemsByFloor.end())
	{
		const int floorLeft = getRect().minX();
		const int floorRight = getRect().maxX();
		for (Game::ItemSet::const_iterator it = floorItems->second.begin(); it != floorItems->second.end(); ++it)
		{
			Item *item = *it;
			if (item->canHaulPeople())
				continue;
			const int left = std::max(floorLeft, item->getRect().minX());
			const int right = std::min(floorRight, item->getRect().maxX());
			if (left < right)
				occupied.push_back({left, right});
		}
	}

	std::sort(occupied.begin(), occupied.end(), [](const Span &a, const Span &b) {
		return (a.left == b.left) ? a.right < b.right : a.left < b.left;
	});

	std::vector<Span> merged;
	for (std::vector<Span>::const_iterator it = occupied.begin(); it != occupied.end(); ++it)
	{
		if (merged.empty() || it->left > merged.back().right)
			merged.push_back(*it);
		else if (it->right > merged.back().right)
			merged.back().right = it->right;
	}

	auto drawSpan = [&](Sprite &sprite, int left, int right) {
		if (left >= right)
			return;
		sprite.setScale({(right - left) * 8.0f / sprite.getTextureRect().size.x, 1.f});
		sprite.setPosition({left * 8.0f, static_cast<float>(-getPosition().y * 36)});
		target.draw(sprite);
		game->drawnSprites++;
	};

	const int floorLeft = getRect().minX();
	const int floorRight = getRect().maxX();
	int cursor = floorLeft;
	for (std::vector<Span>::const_iterator it = merged.begin(); it != merged.end(); ++it)
	{
		drawSpan(b, cursor, it->left);
		drawSpan(c, it->left, it->right);
		cursor = it->right;
	}
	drawSpan(b, cursor, floorRight);
}
