/* Copyright (c) 2012-2015 Fabian Schuiki */
/* Copyright (c) 2026 OpenSkyscraper contributors */
#include <cassert>
#include "Application.h"
#include "Game.h"
#include "Item/FastFood.h"
#include "Item/Lobby.h"
#include "Item/Office.h"
#include "Item/Hotel.h"
#include "OpenGL.h"

#ifdef _WIN32
#include "Math/Round.h"
#endif

using namespace OT;

Game::Game(Application & app)
:	State("game"),
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
	levelUpDialog(this)
{
	//mapWindow     = NULL;

	funds  = 4000000;
	money.setBalance(funds);
	rating = 0;
	population = 0;
	populationNeedsUpdate = false;

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
	mapWindow.close();
	financeWindow.close();
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
		float delta = wheelEvent->delta;
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
				case sf::Keyboard::Key::F2: {
					FILE * f = fopen("default.tower", "w");
					tinyxml2::XMLPrinter xml(f);
					encodeXML(xml);
					fclose(f);
				} return true;
		case sf::Keyboard::Key::PageUp:   zoom /= 2; return true;
		case sf::Keyboard::Key::PageDown: zoom *= 2; return true;
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
							transferFunds(-80000, "construction", "Elevator car");
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

	//Advance time.
	time.advance(dt);
	int currentAccountingDay = (int)floor(time.absolute);
	if (currentAccountingDay != lastAccountingDay) {
		settleDailyAccounting();
		lastAccountingDay = currentAccountingDay;
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

void Game::extendFloor(int floor, int minX, int maxX) {
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
			transferFunds(-f->prototype->price * (diff_left + diff_right), "construction", "Floor extension");
			playOnce("simtower/construction/flexible");
		}
	} else {
		// Otherwise construct a new floor
		Item::Floor * f = (Item::Floor *) itemFactory.make(itemFactory.prototypesById["floor"], int2(minX,floor));
		f->interval.erase(f->getRect().maxX());
		f->size.x = maxX - minX;
		f->interval.insert(f->getRect().maxX());
		f->updateSprite();
		addItem(f);
		transferFunds(-f->prototype->price, "construction", "Floor");
		playOnce("simtower/construction/normal");
	}
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

	xml.PushAttribute("x", (int)poi.x);
	xml.PushAttribute("y", (int)poi.y);

	// Persist VIP scheduling state so visits don't reset on save/reload.
	vipSystem.encodeXML(xml);

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

	tinyxml2::XMLElement * e = root->FirstChildElement("item");
	while (e) {
		Item::Item * item = itemFactory.make(*e);
		addItem(item);
		e = e->NextSiblingElement("item");
	}
	populationNeedsUpdate = true;
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
			case 2: speed = 2; break;
			case 3: speed = 4; break;
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

void Game::selectTool(const char * tool)
{
	if (!tool) return;
	if (selectedTool != tool) {
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
	visualizeRoute.clear();
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		(*i)->updateRoutes();
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
