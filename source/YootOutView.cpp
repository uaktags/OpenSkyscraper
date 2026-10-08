/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootOutView.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "YootBackdrop.h"
#include "YootCatalog.h"
#include "YootItemArt.h"
#include "YootWorldArt.h"

#include <SFML/Graphics/Sprite.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace OT;

namespace
{
	int findPattern(const YootPluginView & v, int pattern)
	{
		for (size_t i = 0; i < v.patterns.size(); ++i)
			if (v.patterns[i] == pattern) return (int)i;
		return -1;
	}

	void drawPart(sf::RenderTarget & target, const YootPluginView & v, sf::IntRect src, float x, float y, sf::Color tint)
	{
		if (src.size.x <= 0 || src.size.y <= 0) return;
		sf::Sprite s(v.texture, src);
		s.setPosition({x, y});
		s.setColor(tint);
		target.draw(s);
	}
}

void OT::drawYootOutView(Game & game, sf::RenderTarget & target)
{
	const YootWorldArt * art = game.yootSkinArt();
	if (!art || art->worldId() == YootWorld::Id::Kegon) return;
	const int kind = art->outViewKind();
	const int sheet = 1001 + kind;
	const int origin = game.activeRules.profile().unitOriginX;
	const sf::Color tint = game.lighting.compose(sf::Color::White);

	// Floors and lobbies: Floor.t2p DrawProc 0x1000bc60 in view mode 1, the
	// loop only, pattern 100000 + (rows + 10 * lobby + 100 * (kind + 1)) * 100.
	const YootPluginView * floors = yootPluginView(game.app, YootWorldArt::floorPluginFor(art->worldId()),
		sf::Color::Transparent, sheet, sheet, "OutView");
	for (Item::Item * item : game.items)
	{
		const std::string & id = item->prototype->id;
		if (id != "floor" && id != "lobby") continue;
		if (!floors || item->getPosition().y < 0) continue;
		const bool lobby = id == "lobby";
		const int rows = lobby ? std::max(1, (int)item->getSize().y) : 1;
		const int band = findPattern(*floors, 100000 + yootOutViewPattern(rows, lobby ? 1 : 0, kind));
		if (band < 0) continue;
		const sf::IntRect loop = floors->bands[band];
		const int w = loop.size.x;
		if (w <= 0) continue;
		const float top = -(item->getPosition().y + rows) * 36.0f;
		// The floor only owns the cells no tenant or lobby took (movers do
		// not take them: their shafts get the facade).
		std::vector<std::pair<int, int> > taken;
		Game::ItemSetByInt::const_iterator onFloor = game.itemsByFloor.find(item->getPosition().y);
		if (onFloor != game.itemsByFloor.end())
			for (Item::Item * other : onFloor->second)
				if (other != item && !other->canHaulPeople() && other->prototype->id != "floor")
					taken.push_back(std::make_pair(other->getPosition().x * 8, (other->getPosition().x + (int)other->getSize().x) * 8));
		const int x0 = item->getPosition().x * 8, x1 = (item->getPosition().x + (int)item->getSize().x) * 8;
		for (int x = x0; x < x1;)
		{
			bool skipped = false;
			for (const std::pair<int, int> & t : taken)
				if (x >= t.first && x < t.second) { x = t.second; skipped = true; break; }
			if (skipped) continue;
			int end = x1;
			for (const std::pair<int, int> & t : taken)
				if (t.first > x) end = std::min(end, t.first);
			int phase = (x + origin * 8) % w;
			if (phase < 0) phase += w;
			const int run = std::min(w - phase, end - x);
			drawPart(target, *floors, sf::IntRect({loop.position.x + phase, loop.position.y}, {run, loop.size.y}),
				float(x), top, tint);
			x += run;
		}
	}

	// Tenants: T2TenantDef::DrawProc 0x10062780 in view mode 1, the whole
	// unit rect, pattern (base + 10 * valiation + 100 * (kind + 1)) * 100
	// where the View pattern is (base + 10 * valiation) * 100.
	for (Item::Item * item : game.items)
	{
		const std::string & id = item->prototype->id;
		if (id == "floor" || id == "lobby" || item->canHaulPeople() || item->getPosition().y < 0) continue;
		const YootItemStats * stats = item->prototype->yoot;
		if (!stats || stats->plugin.empty()) continue;
		const YootPluginView * view = yootPluginView(game.app, stats->plugin);
		const YootPluginView * out = yootPluginView(game.app, stats->plugin, sf::Color::Transparent, sheet, sheet, "OutView");
		if (!out || out->bands.empty()) continue;
		const int viewBase = (view && !view->patterns.empty()) ? view->patterns[0] / 100 : 1;
		int band = findPattern(*out, yootOutViewPattern(viewBase % 10, (viewBase / 10) % 10, kind));
		if (band < 0) band = 0;
		sf::IntRect src = out->bands[band];
		src.size.x = std::min(src.size.x, (int)item->getSize().x * 8);
		src.size.y = std::min(src.size.y, (int)item->getSize().y * 36);
		drawPart(target, *out, src, item->getPosition().x * 8.0f,
			-(item->getPosition().y + (int)item->getSize().y) * 36.0f, tint);
	}
}
