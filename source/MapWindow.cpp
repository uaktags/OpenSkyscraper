#include "Application.h"
#include "Game.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Hotel.h"
#include "Item/Item.h"
#include "MapWindow.h"

#include <algorithm>
#include <cmath>

using namespace OT;

static const int kCanvasW = 200;
static const int kCanvasH = 288;
static const int kMapSkyH = 264;
static const int kMapGroundH = 24;

MapWindow::MapWindow(Game * game)
	: GameObject(game)
{
	window = NULL;
	canvas = NULL;
	towerMinX = towerMinY = 0;
	towerMaxX = towerMaxY = 0;
	scale = 1.f;
	desiredVisible = true; // visible by default; close/reopen with 'M'
}

void MapWindow::close()
{
	if (window)
	{
		app->gui.remove(window);
		window.reset();
		canvas.reset();
	}
}

void MapWindow::reload()
{
	close();
	if (!desiredVisible) return; // user closed it; reopen with 'M'

	window = tgui::ChildWindow::create();
	window->setTitle("Map");
	auto renderer = window->getRenderer();
	renderer->setTitleBarHeight(14 * app->uiScale);
	renderer->setBackgroundColor(sf::Color(15, 20, 28, 235));
	renderer->setTitleBarColor(sf::Color(45, 50, 60));
	renderer->setBorderColor(sf::Color(90, 100, 120));
	renderer->setBorders(1);
	window->setClientSize({static_cast<float>(kCanvasW * app->uiScale),
	                        static_cast<float>(kCanvasH * app->uiScale)});
	// Anchor to the right edge of the screen.
	{
		unsigned int winW = app->window.getSize().x;
		float w = kCanvasW * app->uiScale;
		window->setPosition({static_cast<float>(winW) - w - 8.f, 40.f});
	}

	canvas = tgui::CanvasSFML::create({static_cast<float>(kCanvasW * app->uiScale),
	                                   static_cast<float>(kCanvasH * app->uiScale)});
	canvas->setPosition(0, 0);
	window->add(canvas);

	// Click-to-jump.
	canvas->onClick([this](tgui::Vector2f pos) {
		handleClick(pos.x, pos.y);
	});

	window->onClose([this] { setVisible(false); });

	app->gui.add(window);
}

void MapWindow::setVisible(bool visible)
{
	desiredVisible = visible;
	if (visible == isVisible()) {
		// Even when already in the right live state, make sure the widget
		// exists if the player wants it visible.
		if (visible && !window) reload();
		return;
	}
	if (visible)
	{
		// Force the rebuild path to take the "show" branch.
		if (!window) reload();
		else window->setVisible(true);
	}
	else
	{
		if (window) window->setVisible(false);
	}
}

void MapWindow::computeTowerBounds()
{
	towerMinX = towerMinY = std::numeric_limits<int>::max();
	towerMaxX = towerMaxY = std::numeric_limits<int>::min();
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		towerMinX = std::min(towerMinX, item->position.x);
		towerMinY = std::min(towerMinY, item->position.y);
		towerMaxX = std::max(towerMaxX, item->position.x + item->size.x);
		towerMaxY = std::max(towerMaxY, item->position.y + item->size.y);
	}
	if (towerMinX > towerMaxX) { towerMinX = towerMinY = 0; towerMaxX = towerMaxY = 0; }
	towerMinY = std::min(towerMinY, -1);
	towerMaxY = std::max(towerMaxY, 6);
}

sf::Color MapWindow::colorForItem(Item::Item * item) const
{
	const std::string & id = item->prototype->id;

	// Elevators and stairs: always dark - they read as the tower's spine.
	if (item->isElevator())                  return sf::Color(40, 44, 52);
	if (item->prototype->id == "stairs" ||
	    item->prototype->id == "escalator")  return sf::Color(70, 70, 80);

	// StatusMode tinting matches the main viewport so the two views agree.
	if (game->statusMode != Game::kNormal)
	{
		bool isTenant = (id == "office" || id == "condo" || id == "yoot_condo" ||
		                 id == "hotel_single" || id == "hotel_double" ||
		                 id == "hotel_suite" || id == "hotel" ||
		                 id == "fastfood" || id == "restaurant" ||
		                 id == "cinema"    || id == "partyhall");

		if (!isTenant || item->underConstruction)
			return sf::Color(60, 65, 75);
	}

	if (game->statusMode == Game::kEval)
	{
		double e = item->evaluation;
		if      (e >= 70) return sf::Color( 60, 130, 230);
		else if (e >= 40) return sf::Color(230, 180,  40);
		else              return sf::Color(210,  60,  60);
	}
	if (game->statusMode == Game::kHotel)
	{
		Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
		if (hotel)
		{
			if      (hotel->roomState == Item::Hotel::kDirty)    return sf::Color(210, 60, 60);
			else if (hotel->roomState == Item::Hotel::kOccupied) return sf::Color(230, 180, 40);
			else                                                return sf::Color( 80, 200, 90);
		}
		// Non-hotels shown faintly so dirty rooms stand out.
		return sf::Color(60, 65, 75);
	}
	if (game->statusMode == Game::kPric)
	{
		if (id == "condo" || id == "yoot_condo" || id == "office")
		{
			if (!item->isOccupied())
				return sf::Color(230, 180, 40);
		}
		// Non-priced/occupied items shown faintly.
		return sf::Color(60, 65, 75);
	}

	// Normal mode mirrors the original map: a restrained grey silhouette.
	if (id == "lobby" || id == "floor") return sf::Color(155, 155, 165);
	return sf::Color(175, 175, 185);
}

void MapWindow::renderMap()
{
	if (!window || !canvas) return;

	computeTowerBounds();
	int w = std::max(1, towerMaxX - towerMinX);
	int hAboveGround = std::max(1, towerMaxY);
	int hBelowGround = std::max(0, -towerMinY);
	float sx = (static_cast<float>(kCanvasW * app->uiScale) - 8.f) / w;
	float syAbove = (static_cast<float>(kMapSkyH * app->uiScale) - 4.f) / hAboveGround;
	float syBelow = (static_cast<float>(kMapGroundH * app->uiScale) - 2.f) / std::max(1, hBelowGround);
	float sy = hBelowGround > 0 ? std::min(syAbove, syBelow) : syAbove;
	scale = std::min(sx, sy);

	canvas->clear(sf::Color(135, 210, 235));

	sf::Texture &skyTexture = app->bitmaps["simtower/ui/map/sky"];
	sf::Texture &groundTexture = app->bitmaps["simtower/ui/map/ground"];
	int skyFrame = std::max(0, std::min(3, game->sky.from));
	int skyOffset = static_cast<int>(std::floor(std::fmod(game->time.absolute * 200.0, 200.0)));
	if (skyOffset < 0) skyOffset += 200;

	Sprite sky;
	sky.setTexture(skyTexture);
	sky.setScale({app->uiScale, app->uiScale});
	int firstSkyWidth = 200 - skyOffset;
	sky.setTextureRect(sf::IntRect({skyFrame * 200 + skyOffset, 0}, {firstSkyWidth, kMapSkyH}));
	sky.setPosition({0.f, 0.f});
	canvas->draw(sky);
	if (skyOffset > 0)
	{
		sky.setTextureRect(sf::IntRect({skyFrame * 200, 0}, {skyOffset, kMapSkyH}));
		sky.setPosition({firstSkyWidth * app->uiScale, 0.f});
		canvas->draw(sky);
	}

	Sprite ground;
	ground.setTexture(groundTexture);
	ground.setTextureRect(sf::IntRect({0, 0}, {kCanvasW, kMapGroundH}));
	ground.setScale({app->uiScale, app->uiScale});
	ground.setPosition({0.f, static_cast<float>(kMapSkyH * app->uiScale)});
	canvas->draw(ground);

	// 4-pixel margin so items don't touch the canvas edge.
	const float margin = 4.f;
	const float groundY = kMapSkyH * app->uiScale;
	auto drawItem = [&](Item::Item *item) {
		float px = margin + (item->position.x - towerMinX) * scale;
		// Positive tower Y rises above the map's ground line.
		float py = groundY - (item->position.y + item->size.y) * scale;
		float pw = std::max(1.f, item->size.x * scale);
		float ph = std::max(1.f, item->size.y * scale);

		sf::RectangleShape r({pw, ph});
		r.setPosition({px, py});
		r.setFillColor(colorForItem(item));
		canvas->draw(r);
	};

	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		if (item->prototype->id == "floor")
			drawItem(item);
	}
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		if (item->prototype->id != "floor")
			drawItem(item);
	}

	canvas->display();
}

void MapWindow::handleClick(float canvasX, float canvasY)
{
	if (scale <= 0.f) return;
	const float margin = 4.f;
	// Invert the projection done in renderMap.
	float tileX = (canvasX - margin) / scale + towerMinX;
	float tileY = (kMapSkyH * app->uiScale - canvasY) / scale - 1; // approximate

	// Centre the main viewport on this tower coordinate. Game's poi is in
	// pixel space (8 px per tile horizontally, 36 px per floor vertically).
	game->centerViewportOnTile(tileX, tileY);
}
