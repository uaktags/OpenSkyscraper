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

	sf::Texture &floor = App->bitmaps.editionTexture("floor", "simtower/floor");
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
	// Re-seed the boundary interval: init() already ran while this floor was
	// still at the prototype's 1-tile width, so the boundaries it inserted
	// describe the wrong extents. extendFloor() erases these by find(); a
	// stale entry aborted (erase of end()) the first time an elevator was
	// dragged across a floor loaded from a save with a wide slab.
	interval.clear();
	interval.insert(position.x);
	interval.insert(getRect().maxX());
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
	// YootTower skin: the world 1004 roof strip along the whole floor, and
	// on the spans no tenant covers the Floor.t2p empty-floor view plus the
	// floor strip (8 + 24 + 4 px, the original's row layout). Under the
	// tenants the floor strip goes at both ends of the interior: plugin art
	// covers 8..32 px, OpenSky art 12..36 px.
	const YootWorldArt * yoot = game->yootSkinArt();
	if (yoot && yoot->hasSlab() && yoot->hasFloorView())
	{
		const sf::Color yootTint = tinted ? game->lighting.compose(sf::Color::White) : sf::Color::White;
		const float top = -(getPosition().y + 1) * 36.0f;
		yoot->drawRoof(target, floorLeft * 8.0f, floorRight * 8.0f, top, yootTint);
		auto emptySpan = [&](int left, int right) {
			if (left >= right) return;
			yoot->drawFloorView(target, left * 8.0f, right * 8.0f, top + 8.0f, yootTint);
			yoot->drawFloorStrip(target, left * 8.0f, right * 8.0f, top + 32.0f, yootTint);
		};
		int from = floorLeft;
		for (std::vector<Span>::const_iterator it = merged.begin(); it != merged.end(); ++it)
		{
			emptySpan(from, it->left);
			yoot->drawFloorStrip(target, std::max(from, it->left) * 8.0f, it->right * 8.0f, top + 8.0f, yootTint);
			yoot->drawFloorStrip(target, std::max(from, it->left) * 8.0f, it->right * 8.0f, top + 32.0f, yootTint);
			from = std::max(from, it->right);
		}
		emptySpan(from, floorRight);
		game->drawnSprites += 2 + static_cast<int>(merged.size());
		return;
	}

	int cursor = floorLeft;
	for (std::vector<Span>::const_iterator it = merged.begin(); it != merged.end(); ++it)
	{
		drawSpan(b, cursor, it->left);
		drawSpan(c, it->left, it->right);
		cursor = it->right;
	}
	drawSpan(b, cursor, floorRight);
}
