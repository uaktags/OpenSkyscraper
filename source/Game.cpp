/* Copyright (c) 2012-2015 Fabian Schuiki */
/* Copyright (c) 2026 OpenSkyscraper contributors */
#include <cassert>
#include <chrono>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Button.hpp>
#include "Application.h"
#include "Game.h"
#include "SaveHash.h"
#include "Item/FastFood.h"
#include "Item/Lobby.h"
#include "Item/Office.h"
#include "Item/Hotel.h"
#include "OpenGL.h"

#ifdef _WIN32
#include "Math/Round.h"
#endif

using namespace OT;

static double autosaveWallClock()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool allowsFloorSpan(const BuildRules & rules, int minX, int maxX)
{
	const long long width = (long long)maxX - minX;
	return width > 0 && width <= INT_MAX &&
		rules.allowsHorizontalFootprint(minX, (int)width);
}

static bool canExtendFloor(const Game & game, int floor, int minX, int maxX)
{
	if (maxX <= minX) return false;
	Game::FloorItems::const_iterator existing = game.floorItems.find(floor);
	if (existing != game.floorItems.end())
	{
		minX = std::min(minX, existing->second->position.x);
		maxX = std::max(maxX, existing->second->getRect().maxX());
	}
	return allowsFloorSpan(game.activeRules, minX, maxX);
}

void Game::askEventChoice(const std::string & title, const std::string & text,
                          const std::string & yesLabel, const std::string & noLabel,
                          const std::function<void(bool)> & onChoice)
{
	closeEventChoice();
	eventChoiceSavedSpeed = speedMode;
	setSpeedMode(0); // hold the sim while the player decides

	tgui::ChildWindow::Ptr window = tgui::ChildWindow::create(title);
	window->setClientSize({360 * app.uiScale, 130 * app.uiScale});
	window->setPosition({(app.window.getSize().x - 360 * app.uiScale) / 2.f,
	                     (app.window.getSize().y - 130 * app.uiScale) / 2.f});
	window->setResizable(false);
	window->setTitleButtons(tgui::ChildWindow::TitleButton::None);

	tgui::Label::Ptr label = tgui::Label::create(text);
	label->setPosition({14 * app.uiScale, 12 * app.uiScale});
	label->setTextSize(13 * app.uiScale);
	window->add(label);

	tgui::Button::Ptr yes = tgui::Button::create(yesLabel);
	yes->setSize({160 * app.uiScale, 30 * app.uiScale});
	yes->setPosition({20 * app.uiScale, 85 * app.uiScale});
	yes->onPress([this]() { answerEventChoice(true); });
	window->add(yes);

	tgui::Button::Ptr no = tgui::Button::create(noLabel);
	no->setSize({160 * app.uiScale, 30 * app.uiScale});
	no->setPosition({180 * app.uiScale, 85 * app.uiScale});
	no->onPress([this]() { answerEventChoice(false); });
	window->add(no);

	app.gui.add(window);
	eventChoiceWindow = window;
	eventChoiceCallback = onChoice;
}

void Game::answerEventChoice(bool yes)
{
	if (!eventChoiceCallback) return;
	std::function<void(bool)> cb = eventChoiceCallback;
	closeEventChoice();
	cb(yes);
}

void Game::closeEventChoice()
{
	if (eventChoiceWindow) {
		app.gui.remove(eventChoiceWindow);
		eventChoiceWindow.reset();
	}
	eventChoiceCallback = nullptr;
	if (eventChoiceSavedSpeed >= 0) {
		setSpeedMode(eventChoiceSavedSpeed);
		eventChoiceSavedSpeed = -1;
	}
}

Game::Game(Application & app)
:	State("game"),
	activeRules(app.selectedRuleset()),
	app(app),
	itemFactory(this),
	toolboxWindow(this),
	timeWindow(this),
	inspectorDialog(this),
	elevatorDialog(this),
	mapWindow(this),
	financeWindow(this),
	sky(this),
	lighting(this),
	decorations(this),
	vipSystem(this),
	eventSystem(this),
	levelUpDialog(this)
{
	//mapWindow     = NULL;

	funds  = 4000000;
	money.setBalance(funds);
	rating = 0;
	population = 0;
	populationNeedsUpdate = false;
	lastAutosaveDay = (int)floor(time.absolute);
	lastAutosaveWallClock = autosaveWallClock();

	time.set(7/78.0);
	lastAccountingDay = (int)floor(time.absolute);
	lastAccountingQuarter = time.quarter;
	itemBelowCursor = NULL;
	toolPrototype = NULL;
	statusMode = kNormal;

	zoom = 0.5;
	poi.y = 200;
	poi.x = 0;

	draggingElevator = NULL;
	draggingMotor = 0;
	viewportDrag = ViewportDrag::None;

	mainLobby = NULL;
	metroStation = NULL;

	itemFactory.loadPrototypes();

	/*Item::Item * i = itemFactory.prototypes.front()->make(this);
	addItem(i);
	i = itemFactory.prototypes.front()->make(this);
	i->setPosition(int2(20, 0));
	addItem(i);*/
	/*Sprite * s = new Sprite;
	s->SetImage(app.bitmaps["simtower/security"]);
	s->Resize(384, 24);
	s->SetCenter(0, 24);
	s->SetPosition(0, 0);
	sprites.insert(s);*/

	reloadGUI();

	cockSound.setBuffer(app.sounds["simtower/cock"]);
	morningSound.setBuffer(app.sounds["simtower/birds/morning"]);
	bellsSound.setBuffer(app.sounds["simtower/bells"]);
	eveningSound.setBuffer(app.sounds["simtower/birds/evening"]);

}

Game::~Game()
{
	clearWorld();
}

void Game::clearWorld()
{
	inspectorDialog.close();
	elevatorDialog.close();
	closeEventChoice();
	mapWindow.close();
	financeWindow.close();
	routeBuildQueue.clear();
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		delete *i;
	}
	items.clear();
	itemsByFloor.clear();
	itemsByType.clear();
	floorItems.clear();
	mainLobby = NULL;
	metroStation = NULL;
	itemBelowCursor = NULL;
	population = 0;
	populationNeedsUpdate = false;
	visualizeRoute.clear();
	gameMap.clear();
	decorations.reset();

	draggingElevator = NULL;
	draggingMotor = 0;
	draggingElevatorStart = 0;
	draggingElevatorLower = false;
	viewportDrag = ViewportDrag::None;
	toolPrototype = NULL;

	cockSound.Stop();
	morningSound.Stop();
	bellsSound.Stop();
	eveningSound.Stop();
	while (!autoreleaseSounds.empty()) {
		Sound * sound = *autoreleaseSounds.begin();
		sound->Stop();
		autoreleaseSounds.erase(autoreleaseSounds.begin());
		delete sound;
	}
	playingSounds.clear();
	soundPlayTimes.clear();
	vipSystem.reset();
	eventSystem.reset();
	levelUpDialog.close();
}

void Game::activate()
{
	State::activate();
}

void Game::deactivate()
{
	State::deactivate();
}

bool Game::handleEvent(sf::Event & event)
{
	if (handleViewportScrollbarEvent(event))
		return true;

	if (const auto *wheelEvent = event.getIf<sf::Event::MouseWheelScrolled>()) {
		sf::Vector2i mousePosition = wheelEvent->position;
		if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
			return false;
		if (timeWindow.window && timeWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
			return false;
		if (app.menu && app.menu->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
			return false;

		bool isShiftPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
		bool isCtrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
		float delta = wheelEvent->delta;
		if (isCtrlPressed && wheelEvent->wheel == sf::Mouse::Wheel::Vertical) {
			// Ctrl + wheel: zoom toward the cursor (~1.25x per notch).
			// Plain wheel keeps scrolling the viewport.
			zoomBy(std::pow(1.25, -delta), &mousePosition);
			return true;
		}
		if (wheelEvent->wheel == sf::Mouse::Wheel::Horizontal || isShiftPressed) {
			poi.x -= delta * 40.f * zoom;
		} else {
			poi.y += delta * 40.f * zoom;
		}
		return true;
	}

	enum class EventType {
		None,
		KeyPressed,
		TextEntered,
		MouseButtonPressed,
		MouseMoved,
		MouseButtonReleased
	};

	EventType eventType = EventType::None;
	sf::Keyboard::Key keyCode = sf::Keyboard::Key::Unknown;
	char32_t textUnicode = 0;
	sf::Vector2i mousePosition{};

	if (const auto * keyPressed = event.getIf<sf::Event::KeyPressed>()) {
		eventType = EventType::KeyPressed;
		keyCode = keyPressed->code;
	} else if (const auto * textEntered = event.getIf<sf::Event::TextEntered>()) {
		eventType = EventType::TextEntered;
		textUnicode = textEntered->unicode;
	} else if (const auto * mouseButtonPressed = event.getIf<sf::Event::MouseButtonPressed>()) {
		eventType = EventType::MouseButtonPressed;
		mousePosition = mouseButtonPressed->position;
	} else if (const auto * mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
		eventType = EventType::MouseMoved;
		mousePosition = mouseMoved->position;
	} else if (const auto * mouseButtonReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
		eventType = EventType::MouseButtonReleased;
		mousePosition = mouseButtonReleased->position;
	}

	switch (eventType) {
		case EventType::KeyPressed: {
			switch (keyCode) {
				case sf::Keyboard::Key::Left:  poi.x -= 20; return true;
				case sf::Keyboard::Key::Right: poi.x += 20; return true;
				case sf::Keyboard::Key::Up:    poi.y += 20; return true;
				case sf::Keyboard::Key::Down:  poi.y -= 20; return true;
				case sf::Keyboard::Key::F1:    reloadGUI(); return true;
				case sf::Keyboard::Key::F3:    setRating(1); return true;
				case sf::Keyboard::Key::F2:
					writeTowerFile("default.tower");
					return true;
		case sf::Keyboard::Key::PageUp:   zoomBy(0.5); return true;
		case sf::Keyboard::Key::PageDown: zoomBy(2.0); return true;
		case sf::Keyboard::Key::O:        cycleStatusMode(); return true;
		case sf::Keyboard::Key::M:
			mapWindow.setVisible(!mapWindow.isVisible());
			return true;
		case sf::Keyboard::Key::F:
			financeWindow.setVisible(!financeWindow.isVisible());
			return true;
		case sf::Keyboard::Key::Escape:
			if (inspectorDialog.isVisible()) { inspectorDialog.close(); return true; }
			if (elevatorDialog.isVisible())  { elevatorDialog.close();  return true; }
			if (financeWindow.isVisible())   { financeWindow.setVisible(false); return true; }
			if (mapWindow.isVisible())       { mapWindow.setVisible(false); return true; }
			break;
			default: break;
		}
		} break;

		case EventType::TextEntered: {
			switch (textUnicode) {
				case '0': setSpeedMode(0); return true;
				case '1': setSpeedMode(1); return true;
				case '2': setSpeedMode(2); return true;
				case '3': setSpeedMode(3); return true;
				default: break;
			} break;
		} break;

		case EventType::MouseButtonPressed: {
			if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
				return false;

			// Emergency events take priority over all other click actions.
			// If a fire or bomb is active, clicking the event location
			// dispatches the helicopter / defuses the bomb.
			if (eventSystem.isActive()) {
				if (eventSystem.handleClick(toolPosition))
					return true;
			}

			float2 mousePoint(mousePosition.x, mousePosition.y);
			//rectf toolboxWindowRect(float2(toolboxWindow.window->getAbsolutePosition().x, toolboxWindow.window->getAbsolutePosition().y), float2(toolboxWindow.window->GetClientWidth(), toolboxWindow.window->GetClientHeight()));
			// rectf timeWindowRect(float2(timeWindow.window->GetAbsoluteLeft(), timeWindow.window->GetAbsoluteTop()), float2(timeWindow.window->GetClientWidth(), timeWindow.window->GetClientHeight()));
			// rectf mapWindowRect(float2(mapWindow->GetAbsoluteLeft(), mapWindow->GetAbsoluteTop()), float2(mapWindow->GetClientWidth(), mapWindow->GetClientHeight()));

			// Prevent construction or triggering of tool if mouse cursor within toolboxWindow
			//if (toolboxWindowRect.containsPoint(mousePoint)) break;

			// Prevent construction or triggering of tool if mouse cursor within timeWindow
			// if (timeWindowRect.containsPoint(mousePoint)) break;

			// Prevent construction or triggering of tool if mouse cursor within mapWindow
			// if (mapWindowRect.containsPoint(mousePoint)) {
			// 	break;	// Break for now, may add code to handle viewport shift in future
			// }

			if (selectedTool.find("item-") == 0 && toolPrototype) {
				// Construction logic for item tools only
				double2 halfsize((app.window.getSize().x)*0.5*zoom, (app.window.getSize().y)*0.5*zoom);
				sf::FloatRect viewRect(
					{static_cast<float>(round(poi.x - halfsize.x)), static_cast<float>(round(-poi.y - halfsize.y))},
					{static_cast<float>(halfsize.x * 2), static_cast<float>(halfsize.y * 2)}
				);
				sf::View cameraView(viewRect);
				sf::Vector2f mp = app.window.mapPixelToCoords(mousePosition, cameraView);
				mp.y = -mp.y;

				int height = toolPrototype->size.y;
				int yHeightOffset = height;
				if (toolPrototype->icon == ICON_LOBBY) {
					bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
					bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
					if (ctrl) {
						height = shift ? 3 : 2;
					} else {
						height = 1;
					}
					yHeightOffset = 1;
				}
				toolPosition = int2(round(mp.x/8-toolPrototype->size.x/2.0), round(mp.y/36-yHeightOffset/2.0));

				// ISSUE-039 (OpenSkyWeb): batchable room tools start a preview-only
				// drag (ghosts mark each cell valid/red) and commit the whole
				// batch on mouse release. Non-batchable tools (elevators, lobbies,
				// floors, stairs, escalators, metro, parking ramp) keep instant
				// placement.
				if (isBatchDraggable(toolPrototype)) {
					startBatchDrag();
					return true;
				}
				constructToolAt(toolPosition);
				return true;
			}
			else if (itemBelowCursor) {
				if (selectedTool == "bulldozer") {
					if (itemBelowCursor->prototype->icon == ICON_LOBBY || itemBelowCursor->prototype->icon == ICON_FLOOR || itemBelowCursor->prototype->icon == ICON_METRO) {
						playOnce("simtower/construction/impossible");
						timeWindow.showMessage("Cannot bulldoze " + itemBelowCursor->prototype->name);
						break;
					}
					LOG(DEBUG, "destroy %s", itemBelowCursor->desc().c_str());
					bool canHaulPeople = false;
					if (itemBelowCursor->canHaulPeople()) canHaulPeople = true;
					removeItem(itemBelowCursor);
					if (canHaulPeople) updateRoutes();
					playOnce("simtower/bulldozer");
				}
				else if (selectedTool == "finger") {
					if (itemBelowCursor->prototype->id.find("elevator") == 0) {
						Item::Elevator::Elevator * e = (Item::Elevator::Elevator *)itemBelowCursor;

						draggingMotor = 0;
						if (toolPosition.y < itemBelowCursor->position.y) draggingMotor = -1;
						if (toolPosition.y >= itemBelowCursor->position.y + itemBelowCursor->size.y) draggingMotor = 1;

						if (draggingMotor != 0) {
							LOG(DEBUG, "drag elevator %s motor %i", itemBelowCursor->desc().c_str(), draggingMotor);
							draggingElevator = e;
							draggingElevatorStart = toolPosition.y;
							if (draggingElevatorStart < draggingElevator->position.y) {
								draggingElevatorLower = true;
								draggingElevatorStart++;
							} else {
								draggingElevatorLower = false;
								draggingElevatorStart--;
							}
						} else {
							LOG(DEBUG, "clicked elevator %s on floor %i", itemBelowCursor->desc().c_str(), toolPosition.y);
							if (!e->unservicedFloors.erase(toolPosition.y)) {
								e->unservicedFloors.insert(toolPosition.y);
								gameMap.removeNode(MapNode::Point(e->position.x + e->size.x/2, toolPosition.y), itemBelowCursor);
							} else {
								gameMap.addNode(MapNode::Point(e->position.x + e->size.x/2, toolPosition.y), itemBelowCursor);
							}
							e->cleanQueues();
							updateRoutes();
						}
					}
				}
			else if (selectedTool == "inspector") {
				LOG(DEBUG, "inspect %s", itemBelowCursor->desc().c_str());
				visualizeRoute = itemBelowCursor->lobbyRoute;
				inspectorDialog.showForItem(itemBelowCursor);
			}
			}
		} break;

case EventType::MouseMoved: {
if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
return false;

if (batchDragActive()) updateBatchDrag();
if (draggingElevator &&
    !activeRules.allowsHorizontalFootprint(draggingElevator->position.x, draggingElevator->size.x)) {
	timeWindow.showMessage("Cannot extend elevator outside the " +
		std::string(activeRules.displayName()) + " build boundary.");
	draggingElevator = NULL;
}


if (draggingElevator && draggingElevator->repositionMotor(draggingMotor, toolPosition.y)) {
				// Construct floors
				if (draggingElevatorLower) {
					if (draggingElevatorStart > draggingElevator->position.y) {
						for(int i = draggingElevator->position.y; i < draggingElevatorStart ; i++) {
							extendFloor(i, draggingElevator->position.x, draggingElevator->getRect().maxX());
						}
					}
				} else {
					recti rect = draggingElevator->getRect();
					if (draggingElevatorStart < rect.maxY() - 1) {
						for(int i = rect.maxY() - 1; i > draggingElevatorStart; i--) {
							extendFloor(i, draggingElevator->position.x, draggingElevator->getRect().maxX());
						}
					}
				}

				// Update PathFinder map
				gameMap.handleElevatorResize(draggingElevator, draggingElevatorLower, draggingElevatorStart);

				updateRoutes();
			}
		} break;

case EventType::MouseButtonReleased: {
if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
return false;

// ISSUE-039: commit the whole batch now (nothing was built/charged while dragging).
if (batchDragActive()) { finishBatchDrag(); return true; }

draggingElevator = NULL;
} break;
		case EventType::None:
			break;
	}
	return false;
}

bool Game::handleViewportScrollbarEvent(sf::Event &event)
{
	sf::Vector2u ws = app.window.getSize();
	const int scrollbarSize = 16;
	const int topOffset = 23;
	const int rightX = static_cast<int>(ws.x) - scrollbarSize;
	const int bottomY = static_cast<int>(ws.y) - scrollbarSize;

	auto setVerticalFromMouse = [&](int y) {
		double usable = std::max(1, static_cast<int>(ws.y) - topOffset - scrollbarSize);
		double t = std::max(0.0, std::min(1.0, (y - topOffset) / usable));
		poi.y = 360 * 12 - t * (360 * 13);
	};
	auto setHorizontalFromMouse = [&](int x) {
		double usable = std::max(1, static_cast<int>(ws.x) - scrollbarSize);
		double t = std::max(0.0, std::min(1.0, x / usable));
		poi.x = -512 + t * 1024;
	};

	if (const auto *pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
		const sf::Vector2i p = pressed->position;
		if (p.x >= rightX && p.y >= topOffset && p.y < bottomY) {
			viewportDrag = ViewportDrag::Vertical;
			setVerticalFromMouse(p.y);
			return true;
		}
		if (p.y >= bottomY && p.x < rightX) {
			viewportDrag = ViewportDrag::Horizontal;
			setHorizontalFromMouse(p.x);
			return true;
		}
	}
	if (const auto *moved = event.getIf<sf::Event::MouseMoved>()) {
		if (viewportDrag == ViewportDrag::Vertical) {
			setVerticalFromMouse(moved->position.y);
			return true;
		}
		if (viewportDrag == ViewportDrag::Horizontal) {
			setHorizontalFromMouse(moved->position.x);
			return true;
		}
	}
	if (event.is<sf::Event::MouseButtonReleased>()) {
		if (viewportDrag != ViewportDrag::None) {
			viewportDrag = ViewportDrag::None;
			return true;
		}
	}
	return false;
}

void Game::drawViewportScrollbars(sf::RenderTarget &target) const
{
	sf::Vector2u ws = app.window.getSize();
	const float scrollbarSize = 16.f;
	const float topOffset = 23.f;
	const float rightX = static_cast<float>(ws.x) - scrollbarSize;
	const float bottomY = static_cast<float>(ws.y) - scrollbarSize;

	sf::View previous = target.getView();
	target.setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(ws.x), static_cast<float>(ws.y)})));

	sf::RectangleShape track;
	track.setFillColor(sf::Color(198, 198, 198));
	track.setOutlineColor(sf::Color(32, 32, 32));
	track.setOutlineThickness(1.f);
	track.setSize({scrollbarSize, bottomY - topOffset});
	track.setPosition({rightX, topOffset});
	target.draw(track);

	double verticalRange = 360 * 13;
	double vt = std::max(0.0, std::min(1.0, (360 * 12 - poi.y) / verticalRange));
	sf::RectangleShape thumb;
	thumb.setFillColor(sf::Color(145, 145, 145));
	thumb.setOutlineColor(sf::Color(32, 32, 32));
	thumb.setOutlineThickness(1.f);
	thumb.setSize({scrollbarSize - 2.f, 96.f});
	thumb.setPosition({rightX + 1.f, topOffset + static_cast<float>(vt * std::max(1.f, bottomY - topOffset - 96.f))});
	target.draw(thumb);

	track.setSize({rightX, scrollbarSize});
	track.setPosition({0.f, bottomY});
	target.draw(track);

	double ht = std::max(0.0, std::min(1.0, (poi.x + 512) / 1024));
	thumb.setSize({128.f, scrollbarSize - 2.f});
	thumb.setPosition({static_cast<float>(ht * std::max(1.f, rightX - 128.f)), bottomY + 1.f});
	target.draw(thumb);

	target.setView(previous);
}

void Game::qaSetViewportScrollFractions(double horizontal, double vertical)
{
	horizontal = std::max(0.0, std::min(1.0, horizontal));
	vertical = std::max(0.0, std::min(1.0, vertical));
	poi.x = -512 + horizontal * 1024;
	poi.y = 360 * 12 - vertical * (360 * 13);
}

void Game::advance(double dt)
{
	sf::RenderWindow & win = app.window;
	drawnSprites = 0;

	// Rebuild a slice of the invalidated lobby routes (see updateRoutes).
	pumpRouteBuilds();

	//Advance time.
	time.advance(dt);
	int currentAccountingDay = (int)floor(time.absolute);
	if (currentAccountingDay != lastAccountingDay) {
		settleDailyAccounting();
		lastAccountingDay = currentAccountingDay;
		autosaveIfDue();
	}
	// Quarter rollover: snapshot the balance so the finance window can
	// show "Last Quarter Balance" and reset the quarterly accumulators.
	if (time.quarter != lastAccountingQuarter) {
		money.finalizeQuarter();
		lastAccountingQuarter = time.quarter;
	}
	timeWindow.updateTime();

	timeWindow.advance(dt);
	sky.advance(dt);
	lighting.advance(dt);
	vipSystem.advance(dt);
	eventSystem.advance(dt);
	toolboxWindow.update();

	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		Item::Item * item = *i;
		// Construction timer: clear the flag once elapsed and let the
		// item start ticking. Items still under construction are skipped
		// entirely - no people, no schedules, no rent.
		if (item->underConstruction) {
			if (time.absolute >= item->constructionEndTime) {
				item->underConstruction = false;
				playOnce("simtower/construction/normal");
			} else {
				continue;
			}
		}
		item->advance(dt);
	}

	for (PersonSet::iterator p = people.begin(); p != people.end(); p++) {
		(*p)->advance(dt);
	}

	if (populationNeedsUpdate) {
		populationNeedsUpdate = false;
		int p = 0;
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			p += (*i)->population;
		}
		setPopulation(p);
	}

	// Keep the inspector popup live if it's on screen.
	inspectorDialog.refresh();
	elevatorDialog.refresh();

	// Refresh the minimap every ~1 second of game time (cheaper than every
	// frame, plenty for an overview that mostly changes on construction).
	if (mapWindow.isVisible() &&
	    floor(time.absolute / (0.25 * Time::kBaseSpeed)) !=
	    floor((time.absolute - time.dta) / (0.25 * Time::kBaseSpeed)))
	{
		mapWindow.renderMap();
	}

	// Refresh the finance window on the same cadence; its figures only
	// change when money moves, so a ~1s throttle is plenty.
	if (financeWindow.isVisible() &&
	    floor(time.absolute / (0.25 * Time::kBaseSpeed)) !=
	    floor((time.absolute - time.dta) / (0.25 * Time::kBaseSpeed)))
	{
		financeWindow.refresh();
	}

	//Play sounds.
	if (time.checkHour(5))  cockSound.Play(this);
	if (time.checkHour(6))  morningSound.Play(this);
	if (time.checkHour(9))  bellsSound.Play(this);
	if (time.checkHour(18)) eveningSound.Play(this);
	morningSound.setLooping(time.hour < 8);

	//Constrain the POI.
	double2 halfsize((win.getView().getSize().x)*0.5*zoom, (win.getView().getSize().y)*0.5*zoom);
	poi.y = std::max<double>(std::min<double>(poi.y, 360*12 - halfsize.y), -360 + halfsize.y);

	//Adust the camera.
	sf::FloatRect view(
		{static_cast<float>(round(poi.x - halfsize.x)), static_cast<float>(round(-poi.y - halfsize.y))},
		{static_cast<float>(halfsize.x * 2), static_cast<float>(halfsize.y * 2)}
	);
	sf::View cameraView(view);
	win.setView(cameraView);

	//Prepare the current tool.
	sf::Vector2f mp = win.mapPixelToCoords(sf::Mouse::getPosition(win));
	mp.y = -mp.y;
	/* [TRICKY]
	 * The view as defined by SFML's sf::View uses a coordinate system that 
	 * has the origin in the top right corner and grows down and to the right.
	 * The rest of this application (that is, the positioning of the sprites)
	 * uses a coordinate system that has the origin in the bottom right corner
	 * and grows to the right.
	 * This was incredibly tricky for me to wrap my head around, but the only
	 * translation from one to the other that is needed is to negate the "top"
	 * of the view when doing the conditional drawing of on-screen objects.
	 */
	view.position.y = -view.position.y;
	Item::AbstractPrototype * previousPrototype = toolPrototype;
	if (selectedTool.find("item-") == 0) {
		toolPrototype = itemFactory.prototypesById[selectedTool.substr(5)];
		int height = toolPrototype->size.y;
		int yHeightOffset = height;
		if (toolPrototype->icon == ICON_LOBBY) {
			bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
			bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
			if (ctrl) {
				height = shift ? 3 : 2;
			} else {
				height = 1;
			}
			yHeightOffset = 1;
		}
		toolPosition = int2(round(mp.x/8-toolPrototype->size.x/2.0), round(mp.y/36-yHeightOffset/2.0));
	} else {
		toolPrototype = NULL;
		toolPosition = int2(floor(mp.x/8), floor(mp.y/36));
	}
	if (previousPrototype != toolPrototype) timeWindow.updateTooltip();

	//Draw the sky and decorations.
	win.draw(sky);
	win.draw(decorations);

	//Draw the items that are in view.
	Item::Item * previousItemBelowCursor = itemBelowCursor;
	itemBelowCursor = NULL;

	//Draw floor items first
	for (ItemSet::iterator i = itemsByType["floor"].begin(); i != itemsByType["floor"].end(); i++) {
		const int2 & vp = (*i)->getPositionPixels();
		const sf::Vector2u & vs = (*i)->getSizePixels();
		if ((vp.x + vs.x >= view.position.x) && (vp.x <= (view.position.x + view.size.x)) &&
			((vp.y + vs.y) >= (view.position.y - view.size.y)) && (vp.y <= view.position.y)) {
			win.draw(**i);
			if ((*i)->containsPoint(double2(mp.x, mp.y))) itemBelowCursor = *i;
		}
	}

	//Draw the remaining items in the building
	for (int layer = 0; layer < 2; layer++) {
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			if ((*i)->layer != layer) continue;
			const int2 & vp = (*i)->getPositionPixels();
			const sf::Vector2u & vs = (*i)->getSizePixels();
			if ((vp.x + vs.x >= view.position.x) && (vp.x <= (view.position.x + view.size.x)) &&
				((vp.y + vs.y) >= (view.position.y - view.size.y)) && (vp.y <= view.position.y)) {
				win.draw(**i);
				if ((*i)->containsPoint(double2(mp.x, mp.y))) itemBelowCursor = *i;
			}
		}
	}

	//Status overlay pass: tint tenant items according to statusMode. The
	//overlay is a translucent quad sized to the item. Only tenant types are
	//tinted - floors, elevators and lobbies are left alone.
	if (statusMode != kNormal) {
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			Item::Item * item = *i;
			if (item->underConstruction) continue;
			const std::string & id = item->prototype->id;
			bool isTenant = (id == "office" || id == "condo" || id == "yoot_condo" ||
			                 id == "hotel_single" || id == "hotel_double" ||
			                 id == "hotel_suite" || id == "hotel" ||
			                 id == "fastfood" || id == "restaurant" ||
			                 id == "cinema"    || id == "partyhall");
			if (!isTenant) continue;

			sf::Color tint = sf::Color::Transparent;
			if (statusMode == kEval) {
				double e = item->evaluation;
				if      (e >= 70) tint = sf::Color(  0,  96, 255, 110); // blue = high
				else if (e >= 40) tint = sf::Color(255, 200,   0, 110); // yellow = medium
				else              tint = sf::Color(255,   0,   0, 110); // red = low
			} else if (statusMode == kHotel) {
				if (id == "hotel_single" || id == "hotel_double" ||
				    id == "hotel_suite"  || id == "hotel") {
					Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
					if (hotel) {
						if      (hotel->roomState == Item::Hotel::kDirty)    tint = sf::Color(255, 0, 0, 140);
						else if (hotel->roomState == Item::Hotel::kOccupied) tint = sf::Color(255, 200, 0, 90);
						else                                                tint = sf::Color(0, 200, 0, 90);
					}
				} else continue;
			} else if (statusMode == kPric) {
				if (id == "condo" || id == "yoot_condo" || id == "office") {
					if (!item->isOccupied()) {
						tint = sf::Color(255, 200, 0, 110); // yellow = For Sale/Rent
					}
				}
			}
			if (tint.a == 0) continue;

			const int2 vp = item->getPositionPixels();
			const sf::Vector2u vs = item->getSizePixels();
			// Cull offscreen overlays.
			if (!(vp.x + vs.x >= view.position.x) || !(vp.x <= (view.position.x + view.size.x)) ||
			    !((vp.y + vs.y) >= (view.position.y - view.size.y)) || !(vp.y <= view.position.y))
				continue;

			sf::RectangleShape overlay({static_cast<float>(vs.x), static_cast<float>(vs.y)});
			overlay.setPosition({static_cast<float>(vp.x),
			                     static_cast<float>(-(vp.y + static_cast<int>(vs.y)))});
			overlay.setFillColor(tint);
			win.draw(overlay);
			drawnSprites++;
		}
	}

	// Draw active emergency events (fire, bomb alerts) on top of everything
	// so the player can always see and click them.
	win.draw(eventSystem);

	//Highlight the item below the cursor.
	if (!toolPrototype && itemBelowCursor) {
		const int2 itemPixels = itemBelowCursor->getPositionPixels();
		const sf::Vector2u itemSize = itemBelowCursor->getSizePixels();
		sf::RectangleShape s({
			static_cast<float>(itemSize.x),
			static_cast<float>(itemSize.y)
		});
		s.setPosition({
			static_cast<float>(itemPixels.x),
			static_cast<float>(-(itemPixels.y + static_cast<int>(itemSize.y)))
		});
		s.setFillColor(sf::Color(255, 255, 255, 128));
		win.draw(s);
		drawnSprites++;
		if (previousItemBelowCursor != itemBelowCursor) {
			timeWindow.showMessage(itemBelowCursor->prototype->name);
		}
	}

	if (toolPrototype) {
		int height = toolPrototype->size.y;
		if (toolPrototype->icon == ICON_LOBBY) {
			bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
			bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
			if (ctrl) {
				height = shift ? 3 : 2;
			} else {
				height = 1;
			}
		}
		sf::RectangleShape placement({
			static_cast<float>(toolPrototype->size.x * 8),
			static_cast<float>(height * 36)
		});
		placement.setPosition({
			static_cast<float>(toolPosition.x * 8),
			static_cast<float>(-(toolPosition.y + height) * 36)
		});
placement.setFillColor(sf::Color(255, 255, 255, 48));
placement.setOutlineColor(sf::Color::White);
placement.setOutlineThickness(1.f);
win.draw(placement);
drawnSprites++;
}

// ISSUE-039: batch drag ghosts — white = buildable & affordable, red = blocked.
if (batchDragActive() && toolPrototype) {
for (const BatchCell & c : getBatchCells()) {
const bool ok = c.valid && batchAllAffordable();
sf::RectangleShape ghost({(float)(toolPrototype->size.x * 8), (float)(toolPrototype->size.y * 36)});
ghost.setPosition({(float)(c.x * 8), (float)(-(c.y + toolPrototype->size.y) * 36)});
ghost.setFillColor(ok ? sf::Color(255, 255, 255, 48) : sf::Color(255, 0, 0, 72));
ghost.setOutlineColor(ok ? sf::Color::White : sf::Color::Red);
ghost.setOutlineThickness(1.f);
win.draw(ghost);
drawnSprites++;
}
}

	//Visualize route.
	glColor3f(0, 1, 0);
	glBegin(GL_LINE_STRIP);
	int prevFloor = 0;
	if (!visualizeRoute.nodes.empty()) prevFloor = visualizeRoute.nodes[0].item->position.y;
	for (std::vector<Route::Node>::iterator nit = visualizeRoute.nodes.begin(); nit != visualizeRoute.nodes.end(); nit++) {
		Route::Node & n = *nit;
		if (n.item != mainLobby) {
			// Use the item's visual X (left edge of its first tile + 4px
			// to centre on the narrow stair/elevator sprite), not the
			// logical-tile centre which is offset by size.x/2 tiles for
			// transport items with large footprints.
			float px = n.item->position.x * 8 + 4;
			glVertex2f(px, -prevFloor * 36 - 5);
			glVertex2f(px, -n.toFloor * 36 - 5);
		}
		prevFloor = n.toFloor;
	}
	glEnd();

	//Play background sounds at a regular interval.
	const double backgroundSoundPeriod = 0.5 * Time::kBaseSpeed;
	if (floor(time.absolute / backgroundSoundPeriod) != floor((time.absolute - time.dta) / backgroundSoundPeriod)) {
		// playRandomBackgroundSound();
	}

	//Adjust pitch of playing sounds.
	for (SoundSet::iterator s = playingSounds.begin(); s != playingSounds.end();) {
		if ((*s)->getStatus() == sf::SoundSource::Status::Stopped) {
			playingSounds.erase(s++);
		} else {
			(*s)->setPitch(1 + (time.speed_animated-1) * 0.2);
			s++;
		}
	}

	//Autorelease sounds.
	for (SoundSet::iterator s = autoreleaseSounds.begin(); s != autoreleaseSounds.end();) {
		if ((*s)->getStatus() == sf::SoundSource::Status::Stopped) {
			delete *s;
			autoreleaseSounds.erase(s++);
		} else {
			s++;
		}
	}

	//Draw the debug string.
	snprintf(debugString, 512, "%i sprites", drawnSprites);
}

void Game::reloadGUI()
{
	// if (mapWindow) {
	// 	mapWindow->RemoveReference();
	// 	mapWindow->Close();
	// }

	// //mapWindow     = gui.loadDocument("map.rml");

	// if (mapWindow)     mapWindow    ->Show();

	toolboxWindow.reload();
	timeWindow.reload();
	mapWindow.reload();
	financeWindow.reload();
}

void Game::addItem(Item::Item * item)
{
	assert(item);
	items.insert(item);
	itemsByType[item->prototype->id].insert(item);

	if (item->canHaulPeople()) {
		itemsByType["canHaulPeople"].insert(item);
		if (item->isElevator()) itemsByType["elevator"].insert(item);
		else itemsByType["stairlike"].insert(item);
	}

	if (item->prototype->icon == ICON_FLOOR) {
		// Add floor item
		Item::Floor * f = (Item::Floor *) item;
		if (floorItems.count(item->position.y) != 0) {
			// Replace existing floor item with newly created floor item
			Item::Floor * existing_f = floorItems[item->position.y];
			if (existing_f->size.x >= f->size.x) {
				f->position = existing_f->position;
				f->size = existing_f->size;
			} else {
				std::multiset<int> &existing_f_i = existing_f->interval;
				existing_f_i.erase(existing_f->position.x);
				existing_f_i.erase(existing_f->getRect().maxX());
				existing_f_i.insert(f->position.x);
				existing_f_i.insert(f->getRect().maxX());
			}

			f->interval = existing_f->interval;
			removeItem(existing_f);
		}

		floorItems[item->position.y] = f;
		decorations.updateFloor(item->position.y);
	} else {
		for (int i = 0; i < item->size.y; i++) {
			itemsByFloor[item->position.y + i].insert(item);

			if (floorItems.count(item->position.y + i) == 0) {
				// This is necessary in case of loading a save game where the floor item is not loaded before the building item on that floor.
				int minX = item->position.x;
				int maxX = item->getRect().maxX();
				Item::Floor * f = (Item::Floor *) itemFactory.make(itemFactory.prototypesById["floor"], int2(minX,item->position.y + i));
				f->interval.erase(f->getRect().maxX());
				f->size.x = maxX - minX;
				f->interval.insert(f->getRect().maxX());
				f->updateSprite();
				addItem(f);
			}
			if (!item->canHaulPeople()) {
				FloorItems::iterator fi = floorItems.find(item->position.y + i);
				if (fi != floorItems.end()) {
					std::multiset<int> &interval = fi->second->interval;
					interval.insert(item->position.x);
					interval.insert(item->getRect().maxX());
				} else {
					LOG(WARNING, "unable to find floorItems[%i]", item->position.y + i);
				}
			}
		}
	}

	gameMap.addNode(MapNode::Point(item->position.x + item->size.x/2, item->position.y + item->prototype->entrance_offset), item);
	decorations.updateCrane();
	if (item == metroStation) decorations.updateTracks();
}

void Game::removeItem(Item::Item * item)
{
	assert(item);
	// Drop any pending route rebuild for this item before it is deleted.
	std::vector<Item::Item *>::iterator rq = std::find(routeBuildQueue.begin(), routeBuildQueue.end(), item);
	if (rq != routeBuildQueue.end()) routeBuildQueue.erase(rq);
	items.erase(item);
	itemsByType[item->prototype->id].erase(item);

	if (item->canHaulPeople()) {
		itemsByType["canHaulPeople"].erase(item);
		if (item->isElevator()) itemsByType["elevator"].erase(item);
		else itemsByType["stairlike"].erase(item);
	}

	if (item->prototype->icon == ICON_FLOOR) {
		// Remove floor item
		floorItems.erase(item->position.y);
		decorations.updateFloor(item->position.y);
	} else {
		for (int i = 0; i < item->size.y; i++) {
			itemsByFloor[item->position.y + i].erase(item);
			if (!item->canHaulPeople()) {
				std::multiset<int> &interval = floorItems[item->position.y + i]->interval;
				interval.erase(interval.find(item->position.x));
				interval.erase(interval.find(item->getRect().maxX()));
			}
		}
	}

	if (item == itemBelowCursor) itemBelowCursor = NULL;
	if (item == mainLobby) mainLobby = NULL;
	if (item == metroStation) metroStation = NULL;

	gameMap.removeNode(MapNode::Point(item->position.x + item->size.x/2, item->position.y + item->prototype->entrance_offset), item);
	decorations.updateCrane();
	if (item->prototype->icon == ICON_METRO) decorations.updateTracks(); // Technically, this should not happen as Metro Stations are not removable.
	delete item;
}

bool Game::extendFloor(int floor, int minX, int maxX, bool free) {
	if (!canExtendFloor(*this, floor, minX, maxX)) {
		LOG(WARNING, "refused floor extension on floor %i outside the %s horizontal build policy",
		    floor, activeRules.displayName());
		return false;
	}

	if (floorItems.count(floor) != 0) {
		// Look for existing floor to extend
		Item::Floor * f = floorItems[floor];
		std::multiset<int> &interval = f->interval;
		interval.erase(interval.find(f->position.x));
		interval.erase(interval.find(f->getRect().maxX()));
		gameMap.removeNode(MapNode::Point(f->position.x + f->size.x/2, f->position.y + f->prototype->entrance_offset), f);
		float diff_left = 0;
		if (minX < f->position.x) {
			diff_left = f->position.x - minX;
			f->size.x += diff_left;
			f->setPosition(int2(minX, floor));
		}

		float diff_right = 0;
		diff_right = maxX - f->getRect().maxX();
		if (diff_right < 0) diff_right = 0;
		f->size.x += diff_right;

		f->updateSprite();
		interval.insert(f->position.x);
		interval.insert(f->getRect().maxX());
		gameMap.addNode(MapNode::Point(f->position.x + f->size.x/2, f->position.y + f->prototype->entrance_offset), f);
		if (diff_left + diff_right > 0) {
			decorations.updateFloor(f->position.y);
			if (!free) {
				transferFunds(-f->prototype->price * (diff_left + diff_right), "construction", "Floor extension");
				playOnce("simtower/construction/flexible");
			}
		}
	} else {
		// Otherwise construct a new floor
		Item::Floor * f = (Item::Floor *) itemFactory.make(itemFactory.prototypesById["floor"], int2(minX,floor));
		f->interval.erase(f->getRect().maxX());
		f->size.x = maxX - minX;
		f->interval.insert(f->getRect().maxX());
		f->updateSprite();
		addItem(f);
		if (!free) {
			transferFunds(-f->prototype->price, "construction", "Floor");
			playOnce("simtower/construction/normal");
		}
	}
	return true;
}

bool Game::writeTowerFile(const std::string & path)
{
	FILE * f = fopen(path.c_str(), "w");
	if (!f) {
		LOG(ERROR, "Could not open %s for writing", path.c_str());
		return false;
	}
	tinyxml2::XMLPrinter xml;
	encodeXML(xml);
	// Integrity hash, ported from OpenSkyWeb (src/core/savehash.js): the
	// digest covers the exact serialized text minus the hash attribute, so
	// saves interop with OpenSkyWeb's loader (same salt + algorithm).
	const std::string out = SaveHash::injectSaveHash(xml.CStr());
	fwrite(out.data(), 1, out.size(), f);
	fclose(f);
	return true;
}

void Game::autosaveIfDue()
{
	// Policy ported from OpenSkyWeb (src/core/autosave.js): snapshot on an
	// in-game day change, throttled to at most one write per 60 real seconds.
	const int day = (int)floor(time.absolute);
	if (day == lastAutosaveDay) return;
	const double now = autosaveWallClock();
	if (now - lastAutosaveWallClock < 60.0) return;
	lastAutosaveDay = day;
	lastAutosaveWallClock = now;

	std::string name = saveFilename.empty() ? "default.tower" : saveFilename;
	const std::string suffix = ".tower";
	if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
		name = name.substr(0, name.size() - suffix.size());
	const std::string path = name + ".autosave.tower";
	if (writeTowerFile(path)) {
		LOG(INFO, "autosaved to %s", path.c_str());
		timeWindow.showMessage("Autosaved.");
	}
}

bool Game::repairFloorCoverage()
{
	// Ported from OpenSkyWeb Game::repairFloorCoverage (ISSUE-041): mints
	// free stub floor slabs under hauler columns (stairs/elevators) where a
	// level lost its floor entirely, and restores slab coverage under room
	// footprints. Heals towers saved by older builds on first load.
	std::map<int, std::pair<int,int> > needs; // level -> [minX, maxX)
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		Item::Item * item = *i;
		if (item->prototype->icon == ICON_FLOOR) continue;
		// Loaded geometry is retained verbatim, but repair must never create
		// a slab outside the active ruleset to support it.
		if (!activeRules.allowsHorizontalFootprint(item->position.x, item->size.x)) {
			LOG(WARNING, "skipped floor repair for %s outside the %s horizontal build policy",
			    item->desc().c_str(), activeRules.displayName());
			continue;
		}
		const int h = std::max(1, item->size.y);
		for (int yy = item->position.y; yy < item->position.y + h; yy++) {
			// Haulers only need a slab where the level has none at all;
			// rooms always need coverage spanning their footprint.
			if (item->canHaulPeople() && floorItems.count(yy) != 0) continue;
			const int minX = item->position.x;
			const int maxX = item->getRect().maxX();
			std::map<int, std::pair<int,int> >::iterator n = needs.find(yy);
			if (n == needs.end()) needs[yy] = std::make_pair(minX, maxX);
			else {
				n->second.first = std::min(n->second.first, minX);
				n->second.second = std::max(n->second.second, maxX);
			}
		}
	}
	bool repaired = false;
	for (std::map<int, std::pair<int,int> >::iterator i = needs.begin(); i != needs.end(); i++) {
		const int y = i->first;
		Item::Floor * f = floorItems.count(y) ? floorItems[y] : NULL;
		if (!f || f->position.x > i->second.first || f->getRect().maxX() < i->second.second) {
			if (extendFloor(y, i->second.first, i->second.second, true))
				repaired = true;
		}
	}
	if (repaired) updateRoutes();
	return repaired;
}

void Game::seedOfficeLunchQa()
{
	clearWorld();
	time.set(Time::hourToAbsolute(11.95));
	setSpeedMode(3);
	Item::Lobby *lobby = (Item::Lobby *)itemFactory.make("lobby", int2(-16, 0));
	lobby->size.x = 32;
	addItem(lobby);
	addItem(itemFactory.make("stairs", int2(-4, 0)));
	addItem(itemFactory.make("floor", int2(-16, 1)));
	Item::Office *office = (Item::Office *)itemFactory.make("office", int2(-12, 1));
	addItem(office);
	Item::FastFood *fastFood = (Item::FastFood *)itemFactory.make("fastfood", int2(0, 1));
	fastFood->open = true;
	fastFood->updateSprite();
	addItem(fastFood);
	updateRoutes();
	office->prepareLunchQa(12.0);
	LOG(IMPORTANT, "seeded office lunch QA tower");
}

void Game::seedNewTower()
{
	clearWorld();
	funds = 4000000;
	money.clear(funds);
	rating = 0;
	population = 0;
	populationNeedsUpdate = false;
	time.set(7/78.0);
	lastAccountingDay = (int)floor(time.absolute);
	lastAccountingQuarter = time.quarter;
	setSpeedMode(1);
	selectTool("inspector");
	poi.x = 0;
	poi.y = 200;

	funds = 4000000;
	updateRoutes();
	LOG(IMPORTANT, "seeded new tower");
}

void Game::encodeXML(tinyxml2::XMLPrinter & xml)
{
	xml.OpenElement("tower");
	xml.PushAttribute("funds", funds);
	xml.PushAttribute("rating", rating);
	xml.PushAttribute("time", time.absolute);
	xml.PushAttribute("speed", speedMode);
	xml.PushAttribute("rainy", sky.rainyDay);
	xml.PushAttribute("tool", selectedTool.c_str());

	xml.PushAttribute("ruleset", activeRules.identifier());

	xml.PushAttribute("x", (int)poi.x);
	xml.PushAttribute("y", (int)poi.y);

	// Persist VIP scheduling state so visits don't reset on save/reload.
	vipSystem.encodeXML(xml);

	// Persist event scheduling state (next event time).
	eventSystem.encodeXML(xml);

	xml.OpenElement("money");
	xml.PushAttribute("todayIncome", money.todayIncome);
	xml.PushAttribute("todayExpenses", money.todayExpenses);
	xml.PushAttribute("yesterdayIncome", money.yesterdayIncome);
	xml.PushAttribute("yesterdayExpenses", money.yesterdayExpenses);
	xml.PushAttribute("lastQuarterBalance", money.lastQuarterBalance);
	xml.PushAttribute("quarterIncome", money.quarterIncome);
	xml.PushAttribute("quarterExpenses", money.quarterExpenses);
	for (std::map<std::string, int>::const_iterator i = money.todayTotalsByCategory.begin(); i != money.todayTotalsByCategory.end(); i++) {
		xml.OpenElement("today");
		xml.PushAttribute("category", i->first.c_str());
		xml.PushAttribute("total", i->second);
		xml.CloseElement();
	}
	for (std::map<std::string, int>::const_iterator i = money.yesterdayTotalsByCategory.begin(); i != money.yesterdayTotalsByCategory.end(); i++) {
		xml.OpenElement("yesterday");
		xml.PushAttribute("category", i->first.c_str());
		xml.PushAttribute("total", i->second);
		xml.CloseElement();
	}
	for (std::vector<Money::DaySummary>::const_iterator i = money.recentDays.begin(); i != money.recentDays.end(); i++) {
		xml.OpenElement("recentDay");
		xml.PushAttribute("income", i->income);
		xml.PushAttribute("expenses", i->expenses);
		for (std::map<std::string, int>::const_iterator c = i->totalsByCategory.begin(); c != i->totalsByCategory.end(); c++) {
			xml.OpenElement("category");
			xml.PushAttribute("name", c->first.c_str());
			xml.PushAttribute("total", c->second);
			xml.CloseElement();
		}
		xml.CloseElement();
	}
	for (std::map<std::string, int>::const_iterator i = money.quarterTotalsByCategory.begin(); i != money.quarterTotalsByCategory.end(); i++) {
		xml.OpenElement("quarter");
		xml.PushAttribute("category", i->first.c_str());
		xml.PushAttribute("total", i->second);
		xml.CloseElement();
	}
	xml.CloseElement();

	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		xml.OpenElement("item");
		(*i)->encodeXML(xml);
		xml.CloseElement();
	}

	xml.CloseElement();
}

void Game::decodeXML(tinyxml2::XMLDocument & xml)
{
	tinyxml2::XMLElement * root = xml.RootElement();
	assert(root);
	if (!app.hasRulesetOverride()) {
		const char * savedIdentifier = root->Attribute("ruleset");
		if (savedIdentifier) {
			BuildRules::Ruleset savedRuleset = BuildRules::Ruleset::OpenSky;
			if (!BuildRules::parseIdentifier(savedIdentifier, savedRuleset))
				LOG(WARNING, "invalid saved ruleset '%s'; defaulting to opensky", savedIdentifier);
			activeRules = BuildRules(savedRuleset);
		} else if (activeRules.ruleset() == app.selectedRuleset()) {
			// Normal native XML loads begin at the application's default,
			// so old saves safely select OpenSky. A TDT conversion marks
			// its preselected SimTower policy directly before decode.
			activeRules = BuildRules(BuildRules::Ruleset::OpenSky);
		}
	}


	clearWorld();

	setFunds(root->IntAttribute("funds"));
	money.clear(funds);
	if (tinyxml2::XMLElement * moneyElement = root->FirstChildElement("money")) {
		money.todayIncome = moneyElement->IntAttribute("todayIncome");
		money.todayExpenses = moneyElement->IntAttribute("todayExpenses");
		money.yesterdayIncome = moneyElement->IntAttribute("yesterdayIncome");
		money.yesterdayExpenses = moneyElement->IntAttribute("yesterdayExpenses");
		money.lastQuarterBalance = moneyElement->IntAttribute("lastQuarterBalance", money.balance);
		money.quarterStartBalance = money.lastQuarterBalance;
		money.quarterIncome = moneyElement->IntAttribute("quarterIncome", 0);
		money.quarterExpenses = moneyElement->IntAttribute("quarterExpenses", 0);
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("today"); e; e = e->NextSiblingElement("today")) {
			const char * category = e->Attribute("category");
			if (category) money.todayTotalsByCategory[category] = e->IntAttribute("total");
		}
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("yesterday"); e; e = e->NextSiblingElement("yesterday")) {
			const char * category = e->Attribute("category");
			if (category) money.yesterdayTotalsByCategory[category] = e->IntAttribute("total");
		}
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("quarter"); e; e = e->NextSiblingElement("quarter")) {
			const char * category = e->Attribute("category");
			if (category) money.quarterTotalsByCategory[category] = e->IntAttribute("total");
		}
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("recentDay"); e; e = e->NextSiblingElement("recentDay")) {
			Money::DaySummary day;
			day.income = e->IntAttribute("income");
			day.expenses = e->IntAttribute("expenses");
			for (tinyxml2::XMLElement * c = e->FirstChildElement("category"); c; c = c->NextSiblingElement("category")) {
				const char * category = c->Attribute("name");
				if (category) day.totalsByCategory[category] = c->IntAttribute("total");
			}
			money.recentDays.push_back(day);
		}
	}
	setRating(root->IntAttribute("rating"));
	time.set(root->DoubleAttribute("time"));
	lastAccountingDay = (int)floor(time.absolute);
	lastAccountingQuarter = time.quarter;
	setSpeedMode(root->IntAttribute("speed"));
	sky.rainyDay = root->BoolAttribute("rainy");
	selectTool(root->Attribute("tool"));

	poi.x = root->IntAttribute("x");
	poi.y = root->IntAttribute("y");

	// Restore VIP scheduling state (no-op on older saves - all fields
	// default to fresh-tower values inside decodeXML).
	vipSystem.decodeXML(*root);

	// Restore event scheduling state (no-op on older saves).
	eventSystem.decodeXML(*root);

	tinyxml2::XMLElement * e = root->FirstChildElement("item");
	while (e) {
		Item::Item * item = itemFactory.make(*e);
		addItem(item);
		e = e->NextSiblingElement("item");
	}
	populationNeedsUpdate = true;
	// Self-heal missing floor slabs on load (OpenSkyWeb ISSUE-041): repairs
	// towers saved by builds that minted floors with the off-by-one drag bug.
	if (repairFloorCoverage())
		LOG(IMPORTANT, "repairFloorCoverage: minted missing floor slabs on load");
	updateRoutes();
}

void Game::settleDailyAccounting()

{
	money.finalizeDay();
	int maintenanceCost = calculateDailyMaintenanceCost();
	if (maintenanceCost > 0) {
		transferFunds(-maintenanceCost, "maintenance", "Daily maintenance");
	}
	// Re-evaluate every tenant so Office/Condo::isAttractive() and the
	// inspector have fresh satisfaction scores for the new day. Runs after
	// maintenance so stress/counts reflect yesterday's state.
	judgeSystem.evaluateAll(this);
	timeWindow.updateMoneyStats();
}

int Game::calculateDailyMaintenanceCost() const
{
	int total = 0;
	for (ItemSet::const_iterator i = items.begin(); i != items.end(); i++) {
		// Unfinished buildings cost nothing to maintain.
		if ((*i)->underConstruction) continue;
		total += (*i)->dailyMaintenanceCost();
	}
	return total;
}

void Game::transferFunds(int f, std::string category, std::string message)
{
	money.record(f, category);
	setFunds(money.balance);
	playOnce("simtower/cash");
	if (!message.empty()) {
		char c[32];
		snprintf(c, 32, ": $%i", f);
		timeWindow.showMessage(message + c);
	}
	timeWindow.updateMoneyStats();
}

void Game::setFunds(int f)
{
	if (funds != f) {
		funds = f;
		money.setBalance(funds);
		//timeWindow.updateFunds();
		timeWindow.updateFunds();
		timeWindow.updateMoneyStats();
	}
}

void Game::setRating(int r)
{
	if (rating != r) {
		bool improved = (r > rating);
		rating = r;
		if (improved) {
			//TODO: show window
			LOG(IMPORTANT, "rating increased to %i", rating);
			playOnce("simtower/rating/increased");
		}
		timeWindow.updateRating();
	}
}

void Game::setPopulation(int p)
{
	if (population != p) {
		population = p;
		ratingMayIncrease();
		timeWindow.updatePopulation();
	}
}

/** Called whenever an event occurs that might allow the rating to increase, e.g. a change in
 *  population, or an item constructed. */
void Game::ratingMayIncrease()
{
	// Run a fresh judge pass so facility counts reflect the current state
	// (e.g. the Security office the player just finished). This is cheap
	// relative to construction events.
	judgeSystem.evaluateAll(this);
	const JudgeSystem::Counts & counts = judgeSystem.counts();

	while (true)
	{
		const LevelUp::Requirements * req = LevelUp::advancementRequirements(rating);
		if (!req) break; // already at max rating

		if (!LevelUp::meetsRequirements(*req, population, counts, vipSystem.positiveReviews()))
		{
			// Surface what's missing as a one-shot hint, but only when the
			// player is close enough that the next push might trigger it.
			if (population >= req->population - 50 && population < req->population)
			{
				timeWindow.showMessage(std::string("Next: ") + req->summary);
			}
			break;
		}
		setRating(rating + 1);
		// Show a level-up modal in addition to the transient message.
		// The modal lists the freshly unlocked items so the player knows
		// what they can now build (Phase 3.2).
		levelUpDialog.showForRating(rating);
		timeWindow.showMessage(std::string("Promoted to ") + std::to_string(rating + 1) + " stars!");
		// Star award gift message (OpenSkyWeb ISSUE-034, EXE strings
		// 0x79636-0x79c36).
		const std::string award = LevelUp::starRewardMessage(rating + 1);
		if (!award.empty()) timeWindow.showMessage(award);
		toolboxWindow.reload(); // refresh build locks
	}
}

void Game::setSpeedMode(int sm)
{
	assert(sm >= 0 && sm <= 3);
	if (speedMode != sm) {
		speedMode = sm;
		double speed = 0;
			switch (speedMode) {
				case 0: speed = 0; break;
				case 1: speed = 1; break;
				case 2: speed = 4; break;
				case 3: speed = 12; break;
			}
		time.speed = speed;
		toolboxWindow.updateSpeed();
	}
}

void Game::cycleStatusMode()
{
	StatusMode next = static_cast<StatusMode>((static_cast<int>(statusMode) + 1) % 4);
	statusMode = next;
	const char *names[] = { "Normal view", "Evaluation view", "Pricing view", "Hotel view" };
	timeWindow.showMessage(names[static_cast<int>(next)]);
}

void Game::centerViewportOnTile(double tileX, double tileY)
{
	poi.x = tileX * 8.0;
	poi.y = tileY * 36.0;
}

void Game::zoomBy(double factor, const sf::Vector2i * anchorPx)
{
	const sf::Vector2u ws = app.window.getSize();
	const double newZoom = std::clamp(zoom * factor, 1.0 / 3.0, 2.0);
	if (newZoom == zoom)
		return;
	const double f = newZoom / zoom;
	const double halfx = ws.x * 0.5 * zoom,  halfy = ws.y * 0.5 * zoom;
	const double nhalfx = halfx * f,          nhalfy = halfy * f;
	const double ax = anchorPx ? anchorPx->x : ws.x * 0.5;
	const double ay = anchorPx ? anchorPx->y : ws.y * 0.5;
	// Camera view rect in SFML view space (y grows down):
	//   position = (poi.x - halfx, -(poi.y + halfy)), size = 2 * halfsize.
	// Keep the world point under the anchor fixed while swapping the rect
	// for the zoomed one, then read the new poi back off the new rect.
	const double vx = poi.x - halfx, vy = -(poi.y + halfy);
	const double wx = vx + ax * (2 * halfx) / ws.x;
	const double wy = vy + ay * (2 * halfy) / ws.y;
	poi.x = (wx - ax * (2 * nhalfx) / ws.x) + nhalfx;
	poi.y = -(wy - ay * (2 * nhalfy) / ws.y) - nhalfy;
	zoom = newZoom;
}

void Game::toggleElevatorService(Item::Elevator::Elevator * e, int floor)
{
	if (!e) return;
	if (!e->unservicedFloors.erase(floor))
	{
		e->unservicedFloors.insert(floor);
		gameMap.removeNode(MapNode::Point(e->position.x + e->size.x / 2, floor), e);
	}
	else
	{
		gameMap.addNode(MapNode::Point(e->position.x + e->size.x / 2, floor), e);
	}
	e->cleanQueues();
	updateRoutes();
}

bool Game::constructToolAt(int2 pos)
{
	toolPosition = pos;
	if (!activeRules.allowsHorizontalFootprint(pos.x, toolPrototype->size.x)) {
		const std::string reason = "Horizontal footprint is outside the " +
			std::string(activeRules.displayName()) + " build boundary";
		LOG(DEBUG, "cannot construct %s at %ix%i: %s",
		    toolPrototype->id.c_str(), pos.x, pos.y, reason.c_str());
		playOnce("simtower/construction/impossible");
		timeWindow.showMessage("Cannot place item there. " + reason + ".");
		return false;
	}

	const size_t itemsBefore = items.size();
	int height = toolPrototype->size.y;
	int yHeightOffset = height;
	if (toolPrototype->icon == ICON_LOBBY) {
		bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
		bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
		if (ctrl) {
			height = shift ? 3 : 2;
		} else {
			height = 1;
		}
		yHeightOffset = 1;
	}
	struct SizeGuard {
		int &val;
		int oldVal;
		~SizeGuard() { val = oldVal; }
	} guard{toolPrototype->size.y, toolPrototype->size.y};

	if (toolPrototype->icon == ICON_LOBBY) {
		toolPrototype->size.y = height;
	}

	bool handled = false;
	recti toolBoundary = recti(toolPosition, toolPrototype->size);
	if (toolPrototype->id.find("elevator") == 0) {
		const ItemSet &elevators = itemsByType[toolPrototype->id];
		for (ItemSet::const_iterator i = elevators.begin(); i != elevators.end(); i++) {
			Item::Elevator::Elevator * e = (Item::Elevator::Elevator *) *i;
			recti itemRect = e->getRect();
			if (toolBoundary.minX() == itemRect.minX() &&
				toolBoundary.maxX() == itemRect.maxX() &&
				toolBoundary.minY() >= itemRect.minY() &&
				toolBoundary.maxY() <= itemRect.maxY()) {
				LOG(DEBUG, "add car on floor %i to elevator %s", toolPosition.y, e->desc().c_str());
				e->addCar(toolPosition.y);
				transferFunds(-toolPrototype->carCost, "construction", "Elevator car");
				handled = true;
				break;
			}
		}
	}
if (!handled) {
	bool constructionBlocked = false;
	std::string blockReason;
	int minFloorX = INT_MAX;
	int maxFloorX = INT_MIN;
	for (int i = 0; i < toolPrototype->size.y; ++i) {
		if (!canExtendFloor(*this, toolPosition.y + i, toolPosition.x,
		                    toolPosition.x + toolPrototype->size.x)) {
			constructionBlocked = true;
			blockReason = "Horizontal footprint is outside the " +
				std::string(activeRules.displayName()) + " build boundary";
			break;
		}
	}


	// Star-rating gate: some prototypes require a minimum tower
	// rating to build. Reject early so the player gets feedback.
	int minRating = LevelUp::minRatingToBuild(toolPrototype->id);
	if (minRating > rating)
	{
		constructionBlocked = true;
		blockReason = toolPrototype->name + " unlocks at " +
		              std::to_string(minRating + 1) + " stars";
	}

	if (toolPosition.y < -9 && toolPrototype->icon != ICON_METRO) {
			constructionBlocked = true;
			blockReason = "Cannot build below floor B9";
		}

		if (metroStation != NULL && toolPosition.y < metroStation->position.y) {
			constructionBlocked = true;
			blockReason = "Cannot build below Metro Station";
		}

		if (toolPosition.y > 0 && toolPrototype->icon == ICON_METRO) {
			constructionBlocked = true;
			blockReason = toolPrototype->name + " unavailable above ground";
		}

		if (toolPosition.y > 0 && toolPrototype->id == "cinema") {
			constructionBlocked = true;
			blockReason = toolPrototype->name + " unavailable above ground";
		}

		if (toolPrototype->icon == ICON_LOBBY) {
			if (toolPosition.y % 15 != 0) {
				constructionBlocked = true;
				blockReason = "Lobbies can only be built on every 15th floor";
			}
			else if (toolPosition.y != 0) {
				// Check floor width below
				if (floorItems.count(toolPosition.y - 1) != 0) {
					Item::Item * i = floorItems[toolPosition.y - 1];
					minFloorX = i->position.x;
					maxFloorX = i->getRect().maxX();
				}

				// Check for non-lobby items blocking construction on lobby floor
				ItemSet itemsOnFloor = itemsByFloor[toolPosition.y];
				for (ItemSet::const_iterator ii = itemsOnFloor.begin(); !constructionBlocked && ii != itemsOnFloor.end(); ii++) {
					Item::Item * i = *ii;
					if (i->canHaulPeople() || i->prototype->icon == ICON_LOBBY) continue;
					minFloorX = std::max(minFloorX, i->getRect().maxX());
					maxFloorX = std::min(maxFloorX, i->position.x);
				}
			} else {
				minFloorX = INT_MIN;
				maxFloorX = INT_MAX;
			}
		} else if (toolPrototype->icon == ICON_STAIRS || toolPrototype->icon == ICON_ELEVATOR) {
			// Check obstruction from other transport items
			const ItemSet &stairlike = itemsByType["stairlike"];
			for (ItemSet::const_iterator ii = stairlike.begin(); !constructionBlocked && ii != stairlike.end(); ii++) {
				Item::Item * i = *ii;
				int xOffset = (toolPosition.x - i->position.x);
				if ((i->position.y == toolPosition.y && xOffset > -4 && xOffset < 4) ||
					(i->position.y == toolPosition.y - 1 && xOffset > 0 && xOffset < 8) ||
					(i->position.y == toolPosition.y + 1 && xOffset > -8 && xOffset < 0)) {
					constructionBlocked = true;
					blockReason = "Other " + i->prototype->name + " is in the way";
				}
			}

			const ItemSet &elevators = itemsByType["elevator"];
			for (ItemSet::const_iterator ii = elevators.begin(); !constructionBlocked && ii != elevators.end(); ii++) {
				Item::Item * i = *ii;
				recti itemRect = i->getRect();
				itemRect.origin.y -= 1;
				itemRect.size.y += 2;
				if (toolBoundary.intersectsRect(itemRect)) {
					constructionBlocked = true;
					blockReason = i->prototype->name + " is in the way";
				}
			}

			// Check floor width above
			if (floorItems.count(toolPosition.y + 1) != 0) {
				Item::Item * i = floorItems[toolPosition.y + 1];
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();
			}
			if (toolPosition.x < minFloorX || toolPosition.x + toolPrototype->size.x > maxFloorX) {
				if (!constructionBlocked)
					blockReason = "Upper floor is not wide enough";
				constructionBlocked = true;
			}

			// Check floor width
			minFloorX = INT_MAX;
			maxFloorX = INT_MIN;
			if (floorItems.count(toolPosition.y) != 0) {
				Item::Item * i = floorItems[toolPosition.y];
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();
			}
		} else if (toolPrototype->id.find("elevator") == 0) {
			// Check obstruction from other transport items
			toolBoundary.origin.y -= 1;
			toolBoundary.size.y += 2;

			const ItemSet &stairlike = itemsByType["stairlike"];
			for (ItemSet::const_iterator ii = stairlike.begin(); !constructionBlocked && ii != stairlike.end(); ii++) {
				Item::Item * i = *ii;
				if (toolBoundary.intersectsRect(i->getRect())) {
					constructionBlocked = true;
					blockReason = i->prototype->name + " is in the way";
				}
			}

			const ItemSet &elevators = itemsByType["elevator"];
			for (ItemSet::const_iterator ii = elevators.begin(); !constructionBlocked && ii != elevators.end(); ii++) {
				Item::Item * i = *ii;
				recti itemRect = i->getRect();
				itemRect.origin.y -= 1;
				itemRect.size.y += 2;
				if (toolBoundary.intersectsRect(itemRect)) {
					constructionBlocked = true;
					blockReason = "Other " + i->prototype->name + " is in the way";
				}
			}

			// Check floor width below/above if constructing above/below ground level
			Item::Item * i = NULL;
			if (toolPosition.y > 0 && floorItems.count(toolPosition.y - 1) != 0)
				i = floorItems[toolPosition.y - 1];
			else if (toolPosition.y < 0 && floorItems.count(toolPosition.y + 1) != 0)
				i = floorItems[toolPosition.y + 1];
			else if (floorItems.count(0) != 0)
				i = floorItems[0];
			if (i) {
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();
			}
		} else {
			if (toolPrototype->icon == ICON_METRO && metroStation != NULL) {
				constructionBlocked = true;
				blockReason = "Only one Metro Station allowed";
			}

			{
				// SECOM Center singleton (OpenSkyWeb ISSUE-031,
				// EXE string "Only one SECOM allowed").
				Game::ItemSetByString::const_iterator sit = itemsByType.find("secom");
				if (toolPrototype->id == "secom" && sit != itemsByType.end() && !sit->second.empty()) {
					constructionBlocked = true;
					blockReason = "Only one SECOM allowed";
				}
			}

			if (toolPosition.y == 0) {
				constructionBlocked = true;
				blockReason = "Only lobbies may be built on the ground floor";
			}

			// Check obstruction from other buildings.
			ItemSet itemsNearby;
			for (int y = 0; !constructionBlocked && y < toolPrototype->size.y; y++) {
				itemsNearby = itemsByFloor[toolPosition.y + y];
				for (ItemSet::const_iterator ii = itemsNearby.begin(); !constructionBlocked && ii != itemsNearby.end(); ii++) {
					Item::Item * i = *ii;
					if (i->canHaulPeople()) continue;
					if (toolBoundary.intersectsRect(i->getRect())) {
						constructionBlocked = true;
						blockReason = i->prototype->name + " is in the way";
					}
				}
			}

			// Check floor width below/above if constructing above/below ground level
			Item::Item * i = NULL;
			if (toolPosition.y > 0 && floorItems.count(toolPosition.y - 1) != 0)
				i = floorItems[toolPosition.y - 1];
			else if (floorItems.count(toolPosition.y + toolPrototype->size.y) != 0)
				i = floorItems[toolPosition.y + toolPrototype->size.y];
			if (i) {
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();

				// If the floor below is the top of a multi-story lobby
				// and the new item is wider than the lobby, auto-extend
				// the lobby to match. This mirrors SimTower's behaviour of
				// letting you place condos/offices directly above a sky
				// lobby without manually widening it first. See Phase 2.5.
				if (toolPosition.y > 0 && toolPrototype->icon != ICON_LOBBY) {
					const ItemSet &belowItems = itemsByFloor[toolPosition.y - 1];
					for (ItemSet::const_iterator bi = belowItems.begin(); bi != belowItems.end(); bi++) {
						Item::Item * below = *bi;
						if (below->prototype->icon == ICON_LOBBY && below->size.y > 1) {
							Item::Lobby * l = (Item::Lobby *) below;
							const int lobbyRight = l->position.x + l->size.x;
							const int toolRight  = toolPosition.x + toolPrototype->size.x;
							int diffLeft = 0, diffRight = 0;
							int oldMinX = l->position.x;
							int oldMaxX = l->getRect().maxX();
							const int newMinX = std::min(l->position.x, toolPosition.x);
							const int newMaxX = std::max(lobbyRight, toolRight);
							for (int f = l->position.y; f < l->position.y + l->size.y; ++f) {
								if (!canExtendFloor(*this, f, newMinX, newMaxX)) {
									constructionBlocked = true;
									blockReason = "Horizontal footprint is outside the " +
										std::string(activeRules.displayName()) + " build boundary";
									break;
								}
							}
							if (constructionBlocked) break;

							if (toolPosition.x < l->position.x) {
								diffLeft = l->position.x - toolPosition.x;
								l->size.x += diffLeft;
								l->setPosition(toolPosition);
							}
							if (toolRight > lobbyRight) {
								diffRight = toolRight - lobbyRight;
								l->size.x += diffRight;
							}
							if (diffLeft + diffRight > 0) {
								int newMinX = l->position.x;
								int newMaxX = l->getRect().maxX();
								// Extend floor intervals on every lobby floor.
								for (int f = l->position.y; f < l->position.y + l->size.y; f++) {
									extendFloor(f, newMinX, newMaxX);
									FloorItems::iterator fi = floorItems.find(f);
									if (fi != floorItems.end()) {
										std::multiset<int> &interval = fi->second->interval;
										std::multiset<int>::iterator it = interval.find(oldMinX);
										if (it != interval.end()) interval.erase(it);
										it = interval.find(oldMaxX);
										if (it != interval.end()) interval.erase(it);
										interval.insert(newMinX);
										interval.insert(newMaxX);
									}
								}
								maxFloorX = l->position.x + l->size.x;
								minFloorX = l->position.x;
								transferFunds(-l->prototype->price * 4 / (diffLeft + diffRight), "construction", "Lobby extension");
								playOnce("simtower/construction/flexible");
							}
							break;
						}
					}
				}
			}
		}
		if (toolPosition.x < minFloorX || toolPosition.x + toolPrototype->size.x > maxFloorX) {
			if (!constructionBlocked)
				blockReason = "Floor " + std::string(toolPosition.y > 0 ? "below" : "above") + " is not wide enough";
			constructionBlocked = true;
		}

		if (!constructionBlocked && toolPrototype->icon == ICON_LOBBY) {
			const ItemSet &itemsOnFloor = itemsByFloor[toolPosition.y];
			for (ItemSet::const_iterator ii = itemsOnFloor.begin(); ii != itemsOnFloor.end(); ++ii) {
				Item::Item * item = *ii;
				if (item->prototype->icon != ICON_LOBBY) continue;
				const int newMinX = std::min(item->position.x, toolPosition.x);
				const int newMaxX = std::max(item->getRect().maxX(),
					toolPosition.x + toolPrototype->size.x);
				for (int f = item->position.y; f < item->position.y + item->size.y; ++f) {
					if (!canExtendFloor(*this, f, newMinX, newMaxX)) {
						constructionBlocked = true;
						blockReason = "Horizontal footprint is outside the " +
							std::string(activeRules.displayName()) + " build boundary";
						break;
					}
				}
				break;
			}
		}

		if (!constructionBlocked) {
			LOG(DEBUG, "construct %s at %ix%i, size %ix%i", toolPrototype->id.c_str(), toolPosition.x, toolPosition.y, toolPrototype->size.x, toolPrototype->size.y);

			// Construct floors
			for (int i = 0; i < toolPrototype->size.y; i++)
				extendFloor(toolPosition.y + i, toolPosition.x, toolPosition.x + toolPrototype->size.x);

			if (toolPrototype->icon == ICON_LOBBY) {
				// Look for existing lobby to extend
				bool existingLobby = false;
				const ItemSet &itemsOnFloor = itemsByFloor[toolPosition.y];
				for (ItemSet::const_iterator ii = itemsOnFloor.begin(); !existingLobby && ii != itemsOnFloor.end(); ii++) {
					Item::Item * i = *ii;
					if (i->prototype->icon == ICON_LOBBY) {
						gameMap.removeNode(MapNode::Point(i->position.x + i->size.x/2, i->position.y + i->prototype->entrance_offset), i);
						Item::Lobby * l = (Item::Lobby *) i;
						int diff = 0;
						int oldMinX = l->position.x;
						int oldMaxX = l->getRect().maxX();
						if (toolPosition.x < l->position.x) {
							diff = l->position.x - toolPosition.x;
							l->size.x += diff;
							l->setPosition(toolPosition);
						} else {
							diff = toolPosition.x + toolPrototype->size.x - l->getRect().maxX();
							if (diff < 0) diff = 0;
							l->size.x += diff;
						}
						LOG(INFO, "lobby diff = %d", diff);
						l->updateSprite();
						gameMap.addNode(MapNode::Point(i->position.x + i->size.x/2, i->position.y + i->prototype->entrance_offset), i);
						if (diff > 0) {
							int newMinX = l->position.x;
							int newMaxX = l->getRect().maxX();
							for (int f = l->position.y; f < l->position.y + l->size.y; f++) {
								extendFloor(f, newMinX, newMaxX);
								FloorItems::iterator fi = floorItems.find(f);
								if (fi != floorItems.end()) {
									std::multiset<int> &interval = fi->second->interval;
									std::multiset<int>::iterator it = interval.find(oldMinX);
									if (it != interval.end()) interval.erase(it);
									it = interval.find(oldMaxX);
									if (it != interval.end()) interval.erase(it);
									interval.insert(newMinX);
									interval.insert(newMaxX);
								}
							}
							transferFunds(-toolPrototype->price * 4 / diff, "construction", "Lobby extension");
							playOnce("simtower/construction/flexible");
						}
						existingLobby = true;
					}
				}
				// Otherwise construct a new lobby
				if (!existingLobby) {
					Item::Item * item = itemFactory.make(toolPrototype, toolPosition);
					LOG(INFO, "created new lobby item %p", item);
					addItem(item);
					transferFunds(-toolPrototype->price, "construction", toolPrototype->name);
					playOnce("simtower/construction/normal");
				}
			} else if (toolPrototype->icon != ICON_FLOOR) {
				Item::Item * item = itemFactory.make(toolPrototype, toolPosition);
				addItem(item);
				transferFunds(-toolPrototype->price, "construction", toolPrototype->name);
				if (item->canHaulPeople()) {
					if (item->isElevator()) selectTool("finger");
					updateRoutes();
				} else
					item->updateRoutes();
				playOnce("simtower/construction/normal");
			}
		} else {
			LOG(DEBUG, "cannot construct %s at %ix%i, size %ix%i, reason: %s", toolPrototype->id.c_str(), toolPosition.x, toolPosition.y, toolPrototype->size.x, toolPrototype->size.y, blockReason.c_str());
			playOnce("simtower/construction/impossible");
			timeWindow.showMessage("Cannot place item there. " + blockReason + ".");
		}
	}
	return items.size() != itemsBefore;
}

bool Game::isBatchDraggable(Item::AbstractPrototype * proto) const
{
	if (!proto) return false;
	if (proto->id.find("elevator") == 0 || proto->id == "parkingramp") return false;
	if (proto->icon == ICON_LOBBY || proto->icon == ICON_FLOOR
	    || proto->icon == ICON_METRO || proto->icon == ICON_STAIRS
	    || proto->icon == ICON_ELEVATOR) return false;
	return true;
}

void Game::startBatchDrag()
{
	batchDragging = true;
	batchAnchorX = toolPosition.x;
	batchAnchorY = toolPosition.y;
	batchCells.clear();
	batchAffordable = true;
	batchFirstReason.clear();
	updateBatchDrag();
}

void Game::updateBatchDrag()
{
	if (!batchDragging) return;
	if (!isBatchDraggable(toolPrototype)) { cancelBatchDrag(); return; }
	const int dxRaw = toolPosition.x - batchAnchorX;
	const int dyRaw = toolPosition.y - batchAnchorY;
	// A plain drag snaps to the dominant axis (RTS-style line); Shift spans
	// both axes at once (rows x columns grid), preserving drag direction.
	const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
	                || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
	const bool horizontal = std::abs(dxRaw) >= std::abs(dyRaw);
	const bool useX = shift || horizontal;
	const bool useY = shift || !horizontal;
	const int cols = useX ? std::max(1, (int)std::lround(std::abs((double)dxRaw) / toolPrototype->size.x)) : 1;
	const int rows = useY ? std::max(1, (int)std::lround(std::abs((double)dyRaw) / toolPrototype->size.y)) : 1;
	const int stepX = toolPrototype->size.x * (dxRaw < 0 ? -1 : 1);
	const int stepY = toolPrototype->size.y * (dyRaw < 0 ? -1 : 1);

std::vector<BatchCell> cells;
std::string firstReason;
// Cells are generated anchor-outward, so a vertical drag is ordered
// bottom-to-top (top-to-bottom underground). Simulate the slabs the
// batch creates as we go: a cell with no floor yet is still valid when
// an earlier cell of this same drag will have laid the supporting slab
// by the time it is constructed.
std::set<int> simulatedSlabs;
for (int j = 0; j < rows; j++) {
for (int k = 0; k < cols; k++) {
BatchCell c;
c.x = batchAnchorX + k * stepX;
c.y = batchAnchorY + j * stepY;
c.valid = validateBatchCell(toolPrototype, c.x, c.y, c.reason, simulatedSlabs);
if (!c.valid && firstReason.empty()) firstReason = c.reason;
if (c.valid) {
for (int yy = 0; yy < toolPrototype->size.y; yy++) simulatedSlabs.insert(c.y + yy);
}
cells.push_back(c);
}
}
	batchCells = cells;
	batchAffordable = money.balance >= (long long)cells.size() * toolPrototype->price;
	batchFirstReason = firstReason;
}

void Game::finishBatchDrag()
{
	batchDragging = false;
	if (batchCells.empty()) return;
	const int price = toolPrototype->price;
	int built = 0;
	std::string firstReason = batchFirstReason;
	for (const BatchCell & c : batchCells) {
		if (!c.valid) continue;
		// Stop when the balance can no longer cover another copy.
		if (money.balance < price) break;
		if (constructToolAt(int2(c.x, c.y))) built++;
		else if (firstReason.empty()) firstReason = "placement failed";
	}
	batchCells.clear();
	if (built > 0) {
		updateRoutes();
	} else {
		playOnce("simtower/construction/impossible");
		timeWindow.showMessage("Cannot place item there. " + (firstReason.empty() ? std::string("no space available") : firstReason) + ".");
	}
}

void Game::cancelBatchDrag()
{
	batchDragging = false;
	batchCells.clear();
}

bool Game::validateBatchCell(Item::AbstractPrototype * proto, int x, int y, std::string & reason, const std::set<int> & simulatedSlabs) const
{
	if (!activeRules.allowsHorizontalFootprint(x, proto->size.x)) {
		reason = "Horizontal footprint is outside the " +
			std::string(activeRules.displayName()) + " build boundary";
		return false;
	}

	const int minRating = LevelUp::minRatingToBuild(proto->id);
	if (minRating > rating) {
		reason = proto->name + " unlocks at " + std::to_string(minRating + 1) + " stars";
		return false;
	}
	if (y < -9) { reason = "Cannot build below floor B9"; return false; }
	if (metroStation != NULL && y < metroStation->position.y) {
		reason = "Cannot build above the metro station";
		return false;
	}
	if (proto->id == "secom") {
		Game::ItemSetByString::const_iterator sit = itemsByType.find("secom");
		if (sit != itemsByType.end() && !sit->second.empty()) { reason = "Only one SECOM allowed"; return false; }
	}
	if (y == 0) { reason = "Only lobbies may be built on the ground floor"; return false; }

	const recti boundary(int2(x, y), proto->size);
	for (int yy = 0; yy < proto->size.y; yy++) {
		Game::ItemSetByInt::const_iterator flIt = itemsByFloor.find(y + yy);
		if (flIt == itemsByFloor.end()) continue;
		const ItemSet & itemsOnFloor = flIt->second;
		for (ItemSet::const_iterator ii = itemsOnFloor.begin(); ii != itemsOnFloor.end(); ii++) {
			Item::Item * i = *ii;
			if (i->canHaulPeople()) continue;
			if (boundary.intersectsRect(i->getRect())) {
				reason = i->prototype->name + " is in the way";
				return false;
			}
		}
	}
// Floor support: an existing slab, OR a slab the batch itself will have
// created by the time this cell is constructed (vertical drags build
// bottom-to-top, so each item lays the floor the next one stands on).
const int supportLevel = (y > 0) ? y - 1 : y + proto->size.y;
int minFloorX = INT_MAX;
int maxFloorX = INT_MIN;
bool hasFloor = false;
FloorItems::const_iterator f = (y > 0) ? floorItems.find(y - 1) : floorItems.end();
if (f == floorItems.end()) f = floorItems.find(y + proto->size.y);
if (f != floorItems.end()) {
hasFloor = true;
minFloorX = f->second->position.x;
maxFloorX = f->second->getRect().maxX();
} else if (simulatedSlabs.count(supportLevel)) {
// The batch builds the supporting slab in this same drag.
hasFloor = true;
minFloorX = x;
maxFloorX = x + proto->size.x;
}
if (!hasFloor) {
reason = "Floor " + std::string(y > 0 ? "below" : "above") + " is not wide enough";
return false;
}
if (x < minFloorX || x + proto->size.x > maxFloorX) {
reason = "Floor " + std::string(y > 0 ? "below" : "above") + " is not wide enough";
return false;
}
return true;
}


void Game::selectTool(const char * tool)
{
if (!tool) return;
if (selectedTool != tool) {
cancelBatchDrag(); // a tool change mid-drag invalidates the batch
selectedTool = tool;
		toolboxWindow.updateTool();
		timeWindow.updateTooltip();
	}
}

/** Starts playing the given sound resource once. Releasing the sf::Sound after playback is done
 *  internally. */
void Game::playOnce(Path sound)
{
	//Make sure we don't play sounds to frequently.
	if (soundPlayTimes.count(sound)) {
		if (soundPlayTimes[sound] > time.absolute - 0.25 * Time::kBaseSpeed) {
			return;
		}
	}
	soundPlayTimes[sound] = time.absolute;

	//Actually play the sound.
	Sound * snd = new Sound;
	snd->setBuffer(app.sounds[sound]);
	snd->Play(this);
	autoreleaseSounds.insert(snd);
}

/** Randomly picks one of the items in view and asks it for a background sound that it would like
 * to have played back. */
void Game::playRandomBackgroundSound()
{
	sf::RenderWindow &win = app.window;
	const sf::View currentView = win.getView();
	sf::FloatRect view(
		currentView.getCenter() - currentView.getSize() / 2.f,
		currentView.getSize()
	);
	view.position.y = -view.position.y;

	//Pick a random value between 0 and the screen area.
	double r = (double)rand() / RAND_MAX * view.size.x * view.size.y;

	//Iterate through the items that are in view, subtracting each item's area from the random
	//value until it drops below 0. That item will be given the chance to play the sound. This
	//effectively picks a random item in view, weighted by the item areas.
	Item::Item *pick = NULL;
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		if ((*i)->layer != 0) continue;
		const int2 & vp = (*i)->getPositionPixels();
		const sf::Vector2u & vs = (*i)->getSizePixels();
		if (vp.x + vs.x >= view.position.x && vp.x <= (view.position.x + view.size.x) &&
			(vp.y + vs.y) >= (view.position.y - view.size.y) && vp.y <= view.position.y) {
			int area = vs.x * vs.y;
			r -= area;
			if (r <= 0) {
				pick = *i;
				break;
			}
		}
	}

	//Ask the picked item to play the sound.
	if (pick) {
		LOG(DEBUG, "Playing random background sound of %s", pick->desc().c_str());
		Path path = pick->getRandomBackgroundSoundPath();
		if (!path.str().empty()) {
			LOG(DEBUG, "-> sound is %s", path.c_str());
			playOnce(path);
		}
	}
}

/** Called whenever the transportation layout of the tower changes. Items and people should update
 *  their calculated routes here, so they may find new ways of accessing things. */
void Game::updateRoutes()
{
	LOG(DEBUG, "updating routes");
	// Invalidate every cached lobby route and queue the recompute. The
	// searches themselves are drained by pumpRouteBuilds() with a per-frame
	// time budget: eagerly running one A* per room in this call froze large
	// towers for minutes on load (one search per room, ~2800 rooms in a
	// full-lot tower).
	visualizeRoute.clear();
	routeBuildQueue.clear();
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		(*i)->updateRoutes();
		routeBuildQueue.push_back(*i);
	}
}

/** Recomputes queued lobby routes, bounded to a few milliseconds per frame
 *  so a full rebuild of a maxed tower spreads over a couple of seconds
 *  instead of blocking the load or the frame loop. */
void Game::pumpRouteBuilds()
{
	if (routeBuildQueue.empty()) return;
	const auto budget = std::chrono::milliseconds(4);
	const auto start = std::chrono::steady_clock::now();
	while (!routeBuildQueue.empty()) {
		Item::Item * item = routeBuildQueue.back();
		routeBuildQueue.pop_back();
		item->computeLobbyRoute();
		if (std::chrono::steady_clock::now() - start > budget) break;
	}
}

/** Finds a route from start to destination through the tower. The returned route contains start as
 *  the first and destination as the last node, with transportation in between. If the returned
 *  route is empty(), no path was found through the tower. */
Route Game::findRoute(Item::Item * start, Item::Item * destination, bool serviceRoute)
{
	MapNode::Point start_point(start->position.x + start->size.x/2, start->position.y + start->prototype->exit_offset);
	MapNode::Point end_point(destination->position.x + destination->size.x/2, destination->position.y + destination->prototype->entrance_offset);

	// For the special case of Metro, passengers can enter/exit from the middle floor if their destination is on that floor
	if (start->prototype->icon == ICON_METRO && (end_point.y == start_point.y - 1))
		start_point.y = end_point.y;
	else if (destination->prototype->icon == ICON_METRO && (start_point.y == end_point.y - 1))
		end_point.y = start_point.y;

	MapNode *start_mapnode = gameMap.findNode(start_point, start);
	MapNode *destination_mapnode = gameMap.findNode(end_point, destination);
	return pathFinder.findRoute(start_mapnode, destination_mapnode, start, destination, serviceRoute);
}
