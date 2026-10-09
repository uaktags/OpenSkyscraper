#include <cassert>
#include <algorithm>
#include <functional>
#include <vector>
#include <cstdlib>
#include <string>
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
	// YootTower-only floor holes; SimTower/OpenSky saves never carry them
	// (byte-identical to the pre-YootTower format).
	if (game && game->activeRules.profile().yootWorld && !holes.empty()) {
		std::string h;
		for (size_t i = 0; i < holes.size(); i++)
			h += (i ? "," : "") + std::to_string(holes[i].first) + ":" + std::to_string(holes[i].second);
		xml.PushAttribute("holes", h.c_str());
	}
}

bool Floor::coversCell(int x) const
{
	if (x < position.x || x >= position.x + size.x) return false;
	for (size_t i = 0; i < holes.size(); i++)
		if (x >= holes[i].first && x < holes[i].second) return false;
	return true;
}

int Floor::yootCover(int minX, int maxX)
{
	int fresh = 0;
	for (int x = minX; x < maxX; x++)
		if (!coversCell(x)) fresh++;
	std::vector<std::pair<int, int> > kept;
	for (size_t i = 0; i < holes.size(); i++) {
		const std::pair<int, int> & h = holes[i];
		if (h.second <= minX || h.first >= maxX) { kept.push_back(h); continue; }
		if (h.first < minX) kept.push_back(std::make_pair(h.first, minX));
		if (h.second > maxX) kept.push_back(std::make_pair(maxX, h.second));
	}
	const int left = position.x, right = position.x + size.x;
	if (minX > right) kept.push_back(std::make_pair(right, minX));
	if (maxX < left) kept.push_back(std::make_pair(maxX, left));
	std::sort(kept.begin(), kept.end());
	holes.swap(kept);
	return fresh;
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
	holes.clear();
	if (const char * h = xml.Attribute("holes")) {
		std::string s(h);
		size_t pos = 0;
		while (pos < s.size()) {
			size_t end = s.find(',', pos);
			if (end == std::string::npos) end = s.size();
			const std::string tok = s.substr(pos, end - pos);
			const size_t colon = tok.find(':');
			if (colon != std::string::npos) {
				const int l = atoi(tok.substr(0, colon).c_str()), r = atoi(tok.substr(colon + 1).c_str());
				if (l < r) holes.push_back(std::make_pair(l, r));
			}
			pos = end + 1;
		}
		std::sort(holes.begin(), holes.end());
	}
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

	// YootTower holes split the slab into separately drawn runs.
	std::vector<Span> runs;
	{
		int from = getRect().minX();
		for (size_t i = 0; i < holes.size(); i++) {
			if (holes[i].first > from) runs.push_back({from, holes[i].first});
			from = std::max(from, holes[i].second);
		}
		if (from < getRect().maxX()) runs.push_back({from, getRect().maxX()});
	}
	const std::vector<Span> allMerged = merged;
	for (size_t run = 0; run < runs.size(); run++) {
	const int floorLeft = runs[run].left;
	const int floorRight = runs[run].right;
	merged.clear();
	for (size_t k = 0; k < allMerged.size(); k++) {
		const int l = std::max(floorLeft, allMerged[k].left), r = std::min(floorRight, allMerged[k].right);
		if (l < r) merged.push_back({l, r});
	}
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
		// Tenants that paint their whole unit rect (Stadium.t2p DrawProc)
		// own those cells: no roof or strips there, the backdrop shows
		// through their transparent parts.
		// Transparent interiors (Cathedral) keep only their bottom strip.
		std::vector<Span> covered, keepStrip;
		if (floorItems != game->itemsByFloor.end())
			for (Game::ItemSet::const_iterator it = floorItems->second.begin(); it != floorItems->second.end(); ++it)
			{
				const int cover = (*it)->yootFloorCover();
				if (cover == 0) continue;
				covered.push_back({(*it)->getRect().minX(), (*it)->getRect().maxX()});
				if (cover == 2 && (*it)->getPosition().y == position.y)
					keepStrip.push_back(covered.back());
			}
		auto bare = [&](int left, int right, const std::function<void(float, float)> & draw) {
			for (const Span & c : covered) {
				if (c.right <= left || c.left >= right) continue;
				if (c.left > left) draw(left * 8.0f, c.left * 8.0f);
				left = std::max(left, c.right);
			}
			if (left < right) draw(left * 8.0f, right * 8.0f);
		};
		std::sort(covered.begin(), covered.end(), [](const Span & a, const Span & b) { return a.left < b.left; });
		bare(floorLeft, floorRight, [&](float l, float r) { yoot->drawRoof(target, l, r, top, yootTint); });
		auto emptySpan = [&](int left, int right) {
			if (left >= right) return;
			yoot->drawFloorView(target, left * 8.0f, right * 8.0f, top + 8.0f, yootTint, left == floorLeft);
			yoot->drawFloorStrip(target, left * 8.0f, right * 8.0f, top + 32.0f, yootTint);
		};
		int from = floorLeft;
		for (std::vector<Span>::const_iterator it = merged.begin(); it != merged.end(); ++it)
		{
			emptySpan(from, it->left);
			bare(std::max(from, it->left), it->right, [&](float l, float r) {
				yoot->drawFloorStrip(target, l, r, top + 8.0f, yootTint);
				yoot->drawFloorStrip(target, l, r, top + 32.0f, yootTint);
			});
			from = std::max(from, it->right);
		}
		for (const Span & k : keepStrip)
			yoot->drawFloorStrip(target, k.left * 8.0f, k.right * 8.0f, top + 32.0f, yootTint);
		emptySpan(from, floorRight);
		game->drawnSprites += 2 + static_cast<int>(merged.size());
		continue;
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
}
