#include "Application.h"
#include "Game.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Hotel.h"
#include "Item/Item.h"
#include "MapWindow.h"

#include <algorithm>

using namespace OT;

static const int kCanvasW = 180;
static const int kCanvasH = 220;

MapWindow::MapWindow(Game * game)
	: GameObject(game)
{
	window = NULL;
	canvas = NULL;
	towerMinX = towerMinY = 0;
	towerMaxX = towerMaxY = 0;
	scale = 1.f;
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
	bool wasVisible = isVisible();
	close();
	if (!wasVisible) return; // hidden by default - toggle with 'M'

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
	if (visible == isVisible()) return;
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
}

sf::Color MapWindow::colorForItem(Item::Item * item) const
{
	const std::string & id = item->prototype->id;

	// Elevators and stairs: always dark - they read as the tower's spine.
	if (item->isElevator())                  return sf::Color(40, 44, 52);
	if (item->prototype->id == "stairs" ||
	    item->prototype->id == "escalator")  return sf::Color(70, 70, 80);

	// StatusMode tinting matches the main viewport so the two views agree.
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

	// Normal category palette.
	if (id == "lobby" || id == "floor")       return sf::Color(150, 150, 160);
	if (id == "office")                       return sf::Color( 90, 130, 220);
	if (id == "condo" || id == "yoot_condo")  return sf::Color(110, 210, 130);
	if (id.find("hotel") != std::string::npos) return sf::Color(220, 170, 110);
	if (id == "fastfood" || id == "restaurant") return sf::Color(230, 210, 90);
	if (id == "cinema" || id == "partyhall")  return sf::Color(190, 110, 210);
	if (id == "metro")                        return sf::Color(120, 200, 230);
	if (id == "security" || id == "medicalcenter") return sf::Color(220, 90, 110);
	if (id == "parking")                      return sf::Color(120, 120, 130);
	return sf::Color(100, 100, 110);
}

void MapWindow::renderMap()
{
	if (!window || !canvas) return;

	computeTowerBounds();
	int w = std::max(1, towerMaxX - towerMinX);
	int h = std::max(1, towerMaxY - towerMinY);
	float sx = (static_cast<float>(kCanvasW * app->uiScale) - 8.f) / w;
	float sy = (static_cast<float>(kCanvasH * app->uiScale) - 8.f) / h;
	scale = std::min(sx, sy);

	canvas->clear(sf::Color(8, 12, 20));

	// 4-pixel margin so items don't touch the canvas edge.
	const float margin = 4.f;
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		// Skip floor items - they'd cover everything else. The lobbies
		// already convey the floor extent in grey.
		if (item->prototype->id == "floor") continue;

		float px = margin + (item->position.x - towerMinX) * scale;
		// Y is inverted in screen space (positive Y = up in tower).
		float py = margin + (towerMaxY - (item->position.y + item->size.y)) * scale;
		float pw = std::max(1.f, item->size.x * scale);
		float ph = std::max(1.f, item->size.y * scale);

		sf::RectangleShape r({pw, ph});
		r.setPosition({px, py});
		r.setFillColor(colorForItem(item));
		canvas->draw(r);
	}

	canvas->display();
}

void MapWindow::handleClick(float canvasX, float canvasY)
{
	if (scale <= 0.f) return;
	const float margin = 4.f;
	// Invert the projection done in renderMap.
	float tileX = (canvasX - margin) / scale + towerMinX;
	float tileY = (towerMaxY) - (canvasY - margin) / scale - 1; // approximate

	// Centre the main viewport on this tower coordinate. Game's poi is in
	// pixel space (8 px per tile horizontally, 36 px per floor vertically).
	game->centerViewportOnTile(tileX, tileY);
}
