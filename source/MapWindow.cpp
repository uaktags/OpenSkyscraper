#include "Application.h"
#include "Game.h"
#include "Item/Shop.h"
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

// Minimap palette, keyed to the original MapPaint documentation
// (doc/simtower/SimTower Notes.md:11-23):
//
// Unitmap/normal (Notes.md:12): "the tower as a gray body, each elevator
// is a black line, and the backdrop is the sky background."
static const sf::Color kBodyGrey    (160,160,160);
static const sf::Color kCirculation(120,120,124); ///< stairs/escalators
static const sf::Color kElevatorBlack( 20, 20, 24); ///< Unitmap "black line"
//
// Eval overlay (Notes.md:17): "Blue means very high rating, yellow is
// medium, and red is low."
static const sf::Color kEvalHigh   ( 60,130,230);
static const sf::Color kEvalMedium (230,180, 40);
static const sf::Color kEvalLow    (210, 60, 60);
//
// Pric overlay (Notes.md:19-20): "Light Blue is Very low, Yellow is
// medium, Green is high, and red is very high."
static const sf::Color kPricVeryLow(120,190,255);
static const sf::Color kPricMedium (230,180, 40);
static const sf::Color kPricHigh   ( 80,200, 90);
static const sf::Color kPricVeryHigh(210, 60, 60);
//
// Hotel overlay (Notes.md:22-23): "Red means they are not clean."
static const sf::Color kHotelUnclean(210, 60, 60);
//
// Mode-independent states.
static const sf::Color kConstruction(196,178, 90); ///< khaki, like the viewport construction overlay
static const sf::Color kAbandoned   (100, 92, 84); ///< vacated offices/condos, un-let shops
//
// Elevator "service colours": one hue per elevator type so shafts read
// as true-colour lines instead of dark bars in the overlay modes. The
// express red follows the classic car art's dominant accent (#DF3B34,
// data/bitmaps.hidden/elevator/express.png); standard/service hues are
// chosen for mutual contrast.
static const sf::Color kElevStandard( 58,120,220);
static const sf::Color kElevExpress (210, 60, 60);
static const sf::Color kElevService (200,150, 40);

// Shared rent-table reader implemented by Game.cpp (resource 0x3e9);
// same extern pattern as Condo.cpp/Hotel.cpp/InspectorDialog.cpp.
extern int openSkyDefaultRentClass(const std::string &prototypeId);

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

// A tenantable unit standing empty: vacated office/condo or an un-let
// shop. Hotels cycle vacancy as part of their lifecycle, so they never
// read as abandoned.
static bool isAbandonedUnit(Item::Item * item)
{
	if (item->underConstruction) return false;
	const std::string & id = item->prototype->id;
	if (id == "office" || id == "condo" || id == "yoot_condo")
		return !item->isOccupied();
	if (id == "shop")
	{
		Item::Shop * shop = dynamic_cast<Item::Shop *>(item);
		return shop && !shop->let;
	}
	return false;
}

// Rent class 0..3, most expensive first (openSkyQuarterlyRent tables in
// Game.cpp). Shops store their class; every other priced unit uses the
// InspectorDialog default.
static int rentClassFor(Item::Item * item)
{
	const std::string & id = item->prototype->id;
	if (id == "shop")
	{
		Item::Shop * shop = dynamic_cast<Item::Shop *>(item);
		if (shop) return shop->rentClass;
	}
	return openSkyDefaultRentClass(id);
}

static bool isPricedUnitId(const std::string & id)
{
	return id == "office" || id == "condo" || id == "yoot_condo" ||
	       id == "shop" ||
	       id == "hotel_single" || id == "hotel_double" ||
	       id == "hotel_suite" || id == "hotel";
}

sf::Color MapWindow::colorForItem(Item::Item * item) const
{
	const std::string & id = item->prototype->id;

	// Construction reads in every mode, in the khaki of the viewport's
	// construction overlay.
	if (item->underConstruction)
		return kConstruction;

	// Stairs and escalators are circulation, not tenants.
	if (id == "stairs" || id == "escalator")
		return kCirculation;

	switch (game->statusMode)
	{
		case Game::kEval:
		{
			// "On each piece of building that you have built and has a
			// population" (Notes.md:14-16). Tenanted offices/condos count
			// even while everyone is out; vacant ones fall through.
			bool populated = (item->population > 0 || item->isOccupied());
			if (populated)
			{
				double e = item->evaluation;
				if      (e >= 70) return kEvalHigh;   // blue   = very high
				else if (e >= 40) return kEvalMedium; // yellow = medium
				else              return kEvalLow;    // red    = low
			}
			if (isAbandonedUnit(item))
				return kAbandoned;
			return kBodyGrey;
		}
		case Game::kPric:
		{
			// The pricing ladder (Notes.md:19-20), one colour per rent
			// class. Class 0 is the most expensive, so the ladder walks
			// red (very high) down to light blue (very low). Vacant units
			// still show their asking price - that is the point of Pric.
			if (isPricedUnitId(id))
			{
				switch (rentClassFor(item))
				{
					case 0:  return kPricVeryHigh;
					case 1:  return kPricHigh;
					case 2:  return kPricMedium;
					default: return kPricVeryLow;
				}
			}
			return kBodyGrey;
		}
		case Game::kHotel:
		{
			// "Red means they are not clean" (Notes.md:22-23); infested
			// rooms count as unclean. Everything else stays in the grey
			// body so dirty rooms stand out.
			Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
			if (hotel && (hotel->roomState == Item::Hotel::kDirty ||
			              hotel->roomState == Item::Hotel::kInfested))
				return kHotelUnclean;
			return kBodyGrey;
		}
		case Game::kNormal:
		default:
			break;
	}

	// Unitmap: the tower as a gray body (Notes.md:12), with vacated
	// units reading in their own colour.
	if (isAbandonedUnit(item))
		return kAbandoned;
	return kBodyGrey;
}

sf::Color MapWindow::elevatorColorFor(Item::Item * item) const
{
	// The Unitmap draws "each elevator ... a black line" (Notes.md:12);
	// the overlay modes keep shafts in their per-type service colours.
	if (game->statusMode == Game::kNormal)
		return kElevatorBlack;
	const std::string & id = item->prototype->id;
	if (id == "elevator-express") return kElevExpress;
	if (id == "elevator-service") return kElevService;
	return kElevStandard;
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

		if (item->isElevator())
		{
			// Shafts draw as a thin vertical line down the shaft's
			// centre - the Unitmap's "black line" (Notes.md:12),
			// recoloured per type in the overlay modes.
			float lw = std::max(2.f, std::min(pw * 0.34f, 6.f * app->uiScale));
			sf::RectangleShape r({lw, ph});
			r.setPosition({px + (pw - lw) * 0.5f, py});
			r.setFillColor(elevatorColorFor(item));
			canvas->draw(r);
			return;
		}

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
		if (item->prototype->id != "floor" && !item->isElevator())
			drawItem(item);
	}
	// Shafts last so their lines sit on top of the body blocks.
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		if (item->isElevator())
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
