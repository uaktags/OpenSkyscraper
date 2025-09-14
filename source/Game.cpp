/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <cassert>
#include <algorithm>
#include "Application.h"
#include "Game.h"
#include "Item/Lobby.h"
#include "OpenGL.h"
#include <iostream>
#include "Logger.h"

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
	sky(this),
	decorations(this)
{
	//mapWindow = nullptr;  // Map window disabled during TGUI migration

	funds  = 4000000;
	rating = 0;
	population = 0;
	populationNeedsUpdate = false;

	time.set(7/78.0);
	speedMode = 1;
	selectedTool = "inspector";
	itemBelowCursor = NULL;
	toolPrototype = NULL;

	zoom = 1;
	poi.y = 0;
	poi.x = 0;
	currentFloor = 0;

	draggingElevator = NULL;
	draggingMotor = 0;

	panning = false;
	panStartPoi = double2(0,0);
	panAnchorWorld = double2(0,0);

	mainLobby = NULL;
	metroStation = NULL;

	itemFactory.loadPrototypes();

	reloadGUI();

	//cockSound.setBuffer(app.sounds["simtower/cock"]);
	//morningSound.setBuffer(app.sounds["simtower/birds/morning"]);
	//bellsSound.setBuffer(app.sounds["simtower/bells"]);
	//eveningSound.setBuffer(app.sounds["simtower/birds/evening"]);

	//DEBUG: load from disk.
	 /*tinyxml2::XMLDocument xml;
	 DataManager::Paths p = app.data.paths("default.tower");
	 for (DataManager::Paths::iterator ip = p.begin(); ip != p.end(); ip++) {
	 	LOG(DEBUG, "trying %s", (*ip).c_str());
	 	if (xml.LoadFile(*ip) == 0) break;
	 }
	 decodeXML(xml);*/
}

Game::~Game()
{
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) delete *i;
	items.clear();
	itemsByFloor.clear();
	itemsByType.clear();
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
	switch (event.type) {
		case sf::Event::Resized: {
			// When the window is resized, reload GUI documents so layout and
			// CSS are re-evaluated. This helps keep UI elements and mouse
			// coordinate mapping in sync when the window size changes.
			reloadGUI();
			LOG(INFO, "window resized to %ix%i", event.size.width, event.size.height);
		} break;
		case sf::Event::KeyPressed: {
			switch (event.key.code) {
				case sf::Keyboard::Left:  poi.x -= 20; logNextFrame = true; return true;
				case sf::Keyboard::Right: poi.x += 20; logNextFrame = true; return true;
				case sf::Keyboard::Up: {
					int maxFloor = floorItems.empty() ? 0 : floorItems.rbegin()->first;
					if (currentFloor < maxFloor) {
						currentFloor++;
					}
					logNextFrame = true;
					return true;
				}
				case sf::Keyboard::Down: {
					if (currentFloor > 0) {
						currentFloor--;
					}
					logNextFrame = true;
					return true;
				}
				case sf::Keyboard::F1:    reloadGUI(); return true;
				case sf::Keyboard::F3:    setRating(1); return true;
				case sf::Keyboard::F2: {
					FILE * f = fopen("default.tower", "w");
					tinyxml2::XMLPrinter xml(f);
					encodeXML(xml);
					fclose(f);
				} return true;
					// Ctrl+S to save
					case sf::Keyboard::S:
						if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl)) {
							FILE * f = fopen("default.tower", "w");
							tinyxml2::XMLPrinter xml(f);
							encodeXML(xml);
							fclose(f);
							timeWindow.showMessage("Tower saved.");
							return true;
						}
						break;
				case sf::Keyboard::PageUp:   zoom /= 2; return true;
				case sf::Keyboard::PageDown: zoom *= 2; return true;
			}
		} break;

		case sf::Event::TextEntered: {
			switch (event.text.unicode) {
				case '0': setSpeedMode(0); return true;
				case '1': setSpeedMode(1); return true;
				case '2': setSpeedMode(2); return true;
				case '3': setSpeedMode(3); return true;
			} break;
		} break;

		case sf::Event::MouseButtonPressed: {
			// Start panning when middle mouse button pressed anywhere on map
			if (event.mouseButton.button == sf::Mouse::Middle) {
				panning = true;
				panStartPoi = poi;
				sf::Vector2i mp(event.mouseButton.x, event.mouseButton.y);
				sf::Vector2f world = app.window.mapPixelToCoords(mp);
				panAnchorWorld.x = world.x;
				panAnchorWorld.y = -world.y;
				return true;
			}
			float2 mousePoint(event.mouseButton.x, event.mouseButton.y);
			rectf toolboxWindowRect(float2(toolboxWindow.window->getAbsolutePosition().x, toolboxWindow.window->getAbsolutePosition().y), float2(toolboxWindow.window->getSize().x, toolboxWindow.window->getSize().y));
			rectf timeWindowRect(float2(timeWindow.getWindowPosition().x, timeWindow.getWindowPosition().y), float2(timeWindow.getWindowSize().x, timeWindow.getWindowSize().y));
			// TODO: Map window collision detection disabled during TGUI migration
			// rectf mapWindowRect(float2(mapWindow->GetAbsoluteLeft(), mapWindow->GetAbsoluteTop()), float2(mapWindow->GetClientWidth(), mapWindow->GetClientHeight()));

			// Prevent construction or triggering of tool if mouse cursor within toolboxWindow
			if (toolboxWindowRect.containsPoint(mousePoint)) break;

			// Prevent construction or triggering of tool if mouse cursor within timeWindow
			if (timeWindowRect.containsPoint(mousePoint)) break;

			// TODO: Map window collision detection disabled during TGUI migration
			// Prevent construction or triggering of tool if mouse cursor within mapWindow
			// if (mapWindowRect.containsPoint(mousePoint)) {
			//     break;	// Break for now, may add code to handle viewport shift in future
			// }


			if (toolPrototype) {
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
							transferFunds(-80000);
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

					if (toolPosition.y < -9 && toolPrototype->icon != ICON_METRO) {
						constructionBlocked = true;
						blockReason = "Cannot build below floor B9";
					}

					if (toolPosition.y > 0 && toolPrototype->icon == ICON_METRO) {
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
									gameMap.removeNode(MapNode::Point(i->position.x + i->size.x/2, i->position.y), i);
									Item::Lobby * l = (Item::Lobby *) i;
									int diff = 0;
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
									gameMap.addNode(MapNode::Point(i->position.x + i->size.x/2, i->position.y), i);
									if (diff > 0) {
										transferFunds(-toolPrototype->price * 4 / diff);
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
								transferFunds(-toolPrototype->price);
								playOnce("simtower/construction/normal");
							}
						} else if (toolPrototype->icon != ICON_FLOOR) {
							Item::Item * item = itemFactory.make(toolPrototype, toolPosition);
							addItem(item);
							transferFunds(-toolPrototype->price);
							if (item->canHaulPeople()) {
								if (item->isElevator()) selectTool("finger");
								updateRoutes();
							} else
								item->updateRoutes();
							playOnce("simtower/construction/normal");
						}
					} else {
						LOG(DEBUG, "cannot construct %s at %ix%i, size %ix%i", toolPrototype->id.c_str(), toolPosition.x, toolPosition.y, toolPrototype->size.x, toolPrototype->size.y);
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
					char c[128];
					snprintf(c, 128, "route score = %i", visualizeRoute.score());
					timeWindow.showMessage(c);
				}
			}
		} break;

		case sf::Event::MouseMoved: {
			if (panning) {
				// Compute world coords for mouse position and update poi accordingly
				sf::Vector2i mousePixels = sf::Mouse::getPosition(app.window);
				sf::Vector2f world = app.window.mapPixelToCoords(mousePixels);
				// world.y is inverted in game coordinate system
				double wx = world.x;
				double wy = -world.y;
				// Anchor mapping: when panning started, panAnchorWorld corresponds to panStartPoi
				poi.x = panStartPoi.x + (panAnchorWorld.x - wx);
				poi.y = panStartPoi.y + (panAnchorWorld.y - wy);
				return true;
			} else if (draggingElevator && draggingElevator->repositionMotor(draggingMotor, toolPosition.y)) {
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

		case sf::Event::MouseButtonReleased: {
			if (event.mouseButton.button == sf::Mouse::Middle) {
				panning = false;
				return true;
			}
			draggingElevator = NULL;
		} break;

		case sf::Event::MouseWheelMoved: {
			// If Ctrl is pressed, change world zoom; otherwise pan vertically a bit.
			if (event.mouseWheel.delta != 0) {
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl)) {
					double factor = (event.mouseWheel.delta > 0) ? 1.25 : 0.8;
					setZoom(zoom * factor);
				} else {
					// Scroll vertically: move poi by some pixels scaled with zoom
					double step = 36 * (event.mouseWheel.delta);
					poi.y += step;
				}
				return true;
			}
		} break;
		case sf::Event::Closed: {
			app.window.close();
			return true;
		} break;
	}
	return false;
}

void Game::advance(double dt)
{
	sf::RenderWindow & win = app.window;
	drawnSprites = 0;

	//Advance time.
	time.advance(dt);
	timeWindow.updateTime();

	// Derive floor_height from floor items after load (or default)
	double floor_height = 36.0;
	if (!floorItems.empty()) {
		// Use the pixel height of the first floor item; assume uniform across floors
		floor_height = floorItems.begin()->second->getSizePixels().y;
	}
	poi.y = currentFloor * floor_height + floor_height / 2.0;
	if (logNextFrame) {
		std::cout << "Current floor: " << currentFloor << ", poi.y: " << poi.y << ", floor_height: " << floor_height << std::endl;
		logNextFrame = false;
	}
	toolboxWindow.advance(dt);

	timeWindow.advance(dt);
	sky.advance(dt);

	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		(*i)->advance(dt);
	}

	if (populationNeedsUpdate) {
		populationNeedsUpdate = false;
		int p = 0;
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			p += (*i)->population;
		}
		setPopulation(p);
	}

	if(app.soundEnabled) {
	//Play sounds.
	if (time.checkHour(5))  cockSound.Play(this);
	if (time.checkHour(6))  morningSound.Play(this);
	if (time.checkHour(9))  bellsSound.Play(this);
	if (time.checkHour(18)) eveningSound.Play(this);
	morningSound.setLoop(time.hour < 8, this);
	}
	//Constrain the POI.
	sf::Vector2u screenSize = win.getSize();
	float screenWidth = static_cast<float>(screenSize.x);
	float screenHeight = static_cast<float>(screenSize.y);
	double2 halfsize(screenWidth * 0.5 * zoom, screenHeight * 0.5 * zoom);
	poi.y = std::max<double>(std::min<double>(poi.y, 360*12 - halfsize.y), -360 + halfsize.y);

	//Adjust the camera using SFML best practices.
	sf::View cameraView;
	cameraView.setCenter(sf::Vector2f(static_cast<float>(poi.x), static_cast<float>(-poi.y)));
	cameraView.setSize(sf::Vector2f(screenWidth * static_cast<float>(zoom), screenHeight * static_cast<float>(zoom)));
	win.setView(cameraView);

	//Prepare the current tool.
	sf::Vector2f mp = win.mapPixelToCoords(sf::Mouse::getPosition(win));
	mp.y = -mp.y;
	Item::AbstractPrototype * previousPrototype = toolPrototype;
	if (selectedTool.find("item-") == 0) {
		toolPrototype = itemFactory.prototypesById[selectedTool.substr(5)];
		toolPosition = int2(round(mp.x/8-toolPrototype->size.x/2.0), round(mp.y/floor_height-toolPrototype->size.y/2.0));
	} else {
		toolPrototype = NULL;
		toolPosition = int2(floor(mp.x/8), floor(mp.y/floor_height));
	}
	if (previousPrototype != toolPrototype) timeWindow.updateTooltip();


	//Play background sounds at a regular interval.
	const double backgroundSoundPeriod = 0.5 * Time::kBaseSpeed;
	if (floor(time.absolute / backgroundSoundPeriod) != floor((time.absolute - time.dta) / backgroundSoundPeriod)) {
		// playRandomBackgroundSound();
	}

	//Adjust pitch of playing sounds.
	/*for (SoundSet::iterator s = playingSounds.begin(); s != playingSounds.end();) {
		if ((*s)->getStatus() == sf::Sound::Stopped) {
			playingSounds.erase(s++);
		} else {
			(*s)->setPitch(1 + (time.speed_animated-1) * 0.2);
			s++;
		}
	}*/

	//Autorelease sounds.
	/*for (SoundSet::iterator s = autoreleaseSounds.begin(); s != autoreleaseSounds.end();) {
		if ((*s)->getStatus() == sf::Sound::Stopped) {
			delete *s;
			autoreleaseSounds.erase(s++);
		} else {
			s++;
		}
	}*/

	//Draw the debug string.
	snprintf(debugString, 512, "%i sprites", drawnSprites);
}

void Game::draw(sf::RenderWindow& window)
{
	sf::RenderWindow & win = app.window;
	sf::View cameraView = win.getView();
	sf::Vector2f center = cameraView.getCenter();
	sf::Vector2f size = cameraView.getSize();
	float halfWidth = size.x / 2.0f;
	float halfHeight = size.y / 2.0f;
	float viewLeft = center.x - halfWidth;
	float viewRight = center.x + halfWidth;
	float viewLower = center.y - halfHeight;
	float viewUpper = center.y + halfHeight;
	float game_min = poi.y - halfHeight;
	float game_max = poi.y + halfHeight;
	sf::Vector2f mp = win.mapPixelToCoords(sf::Mouse::getPosition(win));
	mp.y = -mp.y;

	int layer0_shown=0, layer0_hidden=0, layer1_shown=0, layer1_hidden=0, fire_stairs_shown=0;

	//Draw the sky and decorations.
	sky.renderBackground(win);
	win.draw(decorations);

	//Draw the items that are in view.
	Item::Item * previousItemBelowCursor = itemBelowCursor;
	itemBelowCursor = NULL;

		if (logNextFrame) {
			std::cout << "Game frustum: min=" << game_min << ", max=" << game_max << std::endl;
		}
	
		//Draw floor items first
	//Draw floor items first
	for (ItemSet::iterator i = itemsByType["floor"].begin(); i != itemsByType["floor"].end(); i++) {
		if ((*i)->position.y == 0) continue;  // Don't draw ground floor, as lobby provides background
		const int2 & vp = (*i)->getPositionPixels();
		const sf::Vector2u & vs = (*i)->getSizePixels();
		bool visible = (vp.x+vs.x >= viewLeft) && (vp.x <= viewRight) &&
			(vp.y <= game_max && (vp.y + vs.y) >= game_min);
		if (visible) {
			win.draw(**i);
			if ((*i)->getMouseRegion().containsPoint(double2(mp.x, mp.y))) itemBelowCursor = *i;
		}
		if (logNextFrame) {
			std::cout << "Floor at y=" << (*i)->position.y << " shown/hidden" << (visible ? " shown" : " hidden") << std::endl;
		}
	}

	//Draw the remaining items in the building
	for (int layer = 0; layer < 2; layer++) {
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			if ((*i)->layer != layer) continue;
			const int2 & vp = (*i)->getPositionPixels();
			const sf::Vector2u & vs = (*i)->getSizePixels();
			bool visible = (vp.x+vs.x >= viewLeft) && (vp.x <= viewRight) &&
				(vp.y <= game_max && (vp.y + vs.y) >= game_min);
			if (visible) {
				if (layer == 0) layer0_shown++;
				else layer1_shown++;
				if (layer == 1 && (*i)->prototype->id == "stairs") fire_stairs_shown++;
				win.draw(**i);
				if ((*i)->getMouseRegion().containsPoint(double2(mp.x, mp.y))) itemBelowCursor = *i;
			} else {
				if (layer == 0) layer0_hidden++;
				else layer1_hidden++;
			}
		}
	}
	if (logNextFrame) {
		std::cout << "Layer 0: " << layer0_shown << " shown, " << layer0_hidden << " hidden" << std::endl;
		std::cout << "Layer 1: " << layer1_shown << " shown, " << layer1_hidden << " hidden" << std::endl;
		if(fire_stairs_shown > 0) std::cout << "Fire stairs shown: " << fire_stairs_shown << std::endl;
		logNextFrame = false;
	}

	//Highlight the item below the cursor.
	if (!toolPrototype && itemBelowCursor) {
		sf::Sprite s;
		s.scale(itemBelowCursor->getSize().x, itemBelowCursor->getSize().y-12);
		s.setOrigin(0, 1);
		s.setPosition(itemBelowCursor->getPosition().x, itemBelowCursor->getPosition().y);
		s.setColor(sf::Color(255, 255, 255, 255*0.5));
		win.draw(s);
		drawnSprites++;
		if (previousItemBelowCursor != itemBelowCursor) {
			timeWindow.showMessage(itemBelowCursor->prototype->name);
		}
	}

	//Draw construction template.
	glBindTexture(GL_TEXTURE_2D, 0);
	if (toolPrototype) {
		rectf r(toolPosition.x * 8, -(toolPosition.y+toolPrototype->size.y) * 36, toolPrototype->size.x*8, toolPrototype->size.y*36);
		r.inset(float2(0.5, 0.5));
		glColor3f(1, 1, 1);
		glBegin(GL_LINE_STRIP);
		glVertex2f(r.minX(), r.minY());
		glVertex2f(r.maxX(), r.minY());
		glVertex2f(r.maxX(), r.maxY());
		glVertex2f(r.minX(), r.maxY());
		glVertex2f(r.minX(), r.minY());
		glEnd();
	}

	//Visualize route.
	glColor3f(0, 1, 0);
	glBegin(GL_LINE_STRIP);
	int prevFloor = 0;
	if (!visualizeRoute.nodes.empty()) prevFloor = visualizeRoute.nodes[0].item->position.y;
	for (std::vector<Route::Node>::iterator nit = visualizeRoute.nodes.begin(); nit != visualizeRoute.nodes.end(); nit++) {
		Route::Node & n = *nit;
		rectf r = n.item->getRect();
		if (n.item != mainLobby) {
			glVertex2f(r.midX()*8, -prevFloor * 36 - 5);
			glVertex2f(r.midX()*8, -n.toFloor * 36 - 5);
		}
		prevFloor = n.toFloor;
	}
	glEnd();
}

void Game::reloadGUI()
{
	// TODO: Map window functionality has been disabled during TGUI migration
	// The original code tried to load "map.rml" which doesn't exist in TGUI
	// If a minimap is needed, it should be implemented using TGUI widgets
	/*
	if (mapWindow) {
		mapWindow->RemoveReference();
		mapWindow->Close();
	}

	mapWindow = gui.loadDocument("map.rml");

	if (mapWindow) mapWindow->Show();
	*/

	// Set mapWindow to nullptr to disable map window collision detection
	mapWindow = nullptr;

	toolboxWindow.reload();
	timeWindow.reload();
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

	gameMap.addNode(MapNode::Point(item->position.x + item->size.x/2, item->position.y), item);
	decorations.updateCrane();
	if (item == metroStation) decorations.updateTracks();

	// Log added item
	LOG(INFO, "Added %s on floor %i", item->prototype->name.c_str(), item->position.y);

	// Recompute rating on adding items that may affect stars
	ratingMayIncrease();
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

	gameMap.removeNode(MapNode::Point(item->position.x + item->size.x/2, item->position.y), item);
	decorations.updateCrane();
	if (item->prototype->icon == ICON_METRO) decorations.updateTracks(); // Technically, this should not happen as Metro Stations are not removable.

	// Recompute rating on removal of items that may affect stars
	ratingMayIncrease();
}

void Game::extendFloor(int floor, int minX, int maxX) {
	if (floorItems.count(floor) != 0) {
		// Look for existing floor to extend
		Item::Floor * f = floorItems[floor];
		std::multiset<int> &interval = f->interval;
		interval.erase(interval.find(f->position.x));
		interval.erase(interval.find(f->getRect().maxX()));
		gameMap.removeNode(MapNode::Point(f->position.x + f->size.x/2, f->position.y), f);
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
		gameMap.addNode(MapNode::Point(f->position.x + f->size.x/2, f->position.y), f);
		if (diff_left + diff_right > 0) {
			decorations.updateFloor(f->position.y);
			transferFunds(-f->prototype->price * (diff_left + diff_right));
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
		transferFunds(-f->prototype->price);
		playOnce("simtower/construction/normal");
	}
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
	// Persist UI scale and world zoom so user preferences and view are restored on load.
	xml.PushAttribute("ui_scale", (double)app.guiManager.getUIScale());
	xml.PushAttribute("zoom", zoom);

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

	setFunds(root->IntAttribute("funds"));
	setRating(root->IntAttribute("rating"));
	time.set(root->DoubleAttribute("time"));
	setSpeedMode(root->IntAttribute("speed"));
	sky.rainyDay = root->BoolAttribute("rainy");
	selectTool(root->Attribute("tool"));

	poi.x = root->IntAttribute("x");
	poi.y = root->IntAttribute("y");

	// Restore UI scale and world zoom if present in the save file.
	if (root->Attribute("ui_scale")) {
		double ui_s = root->DoubleAttribute("ui_scale");
		app.guiManager.setUIScale((float)ui_s);
	}
	if (root->Attribute("zoom")) {
		double z = root->DoubleAttribute("zoom");
		setZoom(z);
	}

	// Propagate the UI scale to this state's GUI and force a reload so
	// documents reflow with the restored scale. Game inherits State so
	// we can access our own `gui` member directly.
	if (root->Attribute("ui_scale")) {
		double ui_s = root->DoubleAttribute("ui_scale");
		// Force reload so documents reflow with the restored scale
		reloadGUI();
	}

	tinyxml2::XMLElement * e = root->FirstChildElement("item");
	while (e) {
		Item::Item * item = itemFactory.make(*e);
		addItem(item);
		e = e->NextSiblingElement("item");
	}
	// Refresh item sprites post-load in case bitmaps were unavailable during init
	for (ItemSet::iterator i = items.begin(); i != items.end(); ++i) {
		(*i)->updateSprite();
	}
	updateRoutes();

	// Re-evaluate rating after loading a saved game
	ratingMayIncrease();

	if (!floorItems.empty()) {
		currentFloor = 0;
		poi.y = (currentFloor + 0.5) * 36.0;
	}
}

void Game::transferFunds(int f, std::string message)
{
	setFunds(funds + f);
	playOnce("simtower/cash");
	if (!message.empty()) {
		char c[32];
		snprintf(c, 32, ": $%i", f);
		timeWindow.showMessage(message + c);
	}
}

void Game::setFunds(int f)
{
	if (funds != f) {
		funds = f;
		timeWindow.updateFunds();
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

			// Notify the player via the time window
			char msg[64];
			snprintf(msg, sizeof(msg), "Your tower rating increased to %i star%s.", rating, rating == 1 ? "" : "s");
			timeWindow.showMessage(msg);
		}
		timeWindow.updateRating();
		// Update toolbox availability based on new rating
		toolboxWindow.updateAvailability();
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
	// Advance rating by at most one star per invocation. Check the next-star
	// requirements based on the current rating and only call setRating once.
	int r = rating;

	// Next -> 2 stars
	if (r < 2) {
		if (population >= 300) { setRating(2); }
		return;
	}

	// Next -> 3 stars
	if (r == 2) {
		if (population >= 1000) {
			int security = countItemsById("security");
			if (security >= 1) setRating(3);
			else timeWindow.showMessage("Your tower needs security.");
		}
		return;
	}

	// Next -> 4 stars
	if (r == 3) {
		if (population >= 5000) {
			int suites = countItemsById("suite");
			if (suites >= 2 && checkVIPFavourable() && checkRecyclingAndMedical()) setRating(4);
		}
		return;
	}

	// Next -> 5 stars
	if (r == 4) {
		if (population >= 10000) {
			if (metroStation) setRating(5);
		}
		return;
	}

	// Next -> Tower (6?)
	if (r == 5) {
		if (population >= 15000) {
			int cathedralCount = countItemsById("cathedral");
			if (cathedralCount >= 1) setRating(6);
		}
		return;
	}
}

bool Game::hasItemCount(const std::string & prototypeId, int minCount) const {
    int count = countItemsById(prototypeId);
    return count >= minCount;
}

int Game::computeStarRating() const {
    // Canonical evaluator following SimTower rules.
    // Conservative: if subsystems are missing, conditions cannot be satisfied.
    if (population < 300)
        return 1;
    if (population < 1000)
        return 2;
    if (population >= 1000) {
        if (!hasItemCount("security", 1)) {
            LOG(DEBUG, "computeStarRating: missing security -> cap at 2 stars");
            return 2;
        }
        if (population < 5000)
            return 3;
        if (population >= 5000) {
            if (hasItemCount("suite", 2) && checkVIPFavourable() && checkRecyclingAndMedical()) {
                if (population < 10000)
                    return 4;
                if (population >= 10000) {
                    if (metroStation) {
                        if (population < 15000)
                            return 5;
                        if (population >= 15000) {
                            if (hasItemCount("cathedral", 1))
                                return 6; // Tower status
                        }
                        return 5;
                    } else {
                        LOG(DEBUG, "computeStarRating: missing metro -> cap at 4 stars");
                        return 4;
                    }
                }
            } else {
                LOG(DEBUG, "computeStarRating: missing suites/VIP/recycling/medical -> cap at 3 stars");
                return 3;
            }
        }
    }
    return 1; // Fallback
}

bool Game::checkDemandsSatisfied() const {
    // Not implemented yet — always false for now.
    return false;
}

int Game::countItemsById(const std::string & id) const
{
    if (itemsByType.count(id) == 0) return 0;
    return (int)itemsByType.at(id).size();
}

bool Game::checkVIPFavourable() const
{
    // VIP/favourable rating subsystem not implemented yet — return false to be conservative.
    return false;
}

bool Game::checkRecyclingAndMedical() const
{
    // Check for at least one recycling and one medical center in the tower.
    int recycling = countItemsById("recycling");
    int medical = countItemsById("medicalcenter");
    return (recycling >= 1 && medical >= 1);
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

void Game::setZoom(double z)
{
	if (z <= 0) return;
	// clamp zoom to reasonable range
	double newz = std::max<double>(0.125, std::min<double>(16.0, z));
	if (std::fabs(zoom - newz) > 1e-9) {
		zoom = newz;
		// When zoom changes, the view will be updated in advance() next frame
		// because advance() computes camera based on zoom and window size.
	}
}

double Game::getZoom() const { return zoom; }

void Game::selectTool(const char * tool)
{
	LOG(DEBUG, "Game::selectTool called with: '%s'", tool ? tool : "NULL");
	if (!tool) return;
	if (selectedTool != tool) {
		LOG(DEBUG, "Game::selectTool changing from '%s' to '%s'", selectedTool.c_str(), tool);
		selectedTool = tool;
		toolboxWindow.updateTool();
		timeWindow.updateTooltip();
		LOG(DEBUG, "Game::selectTool completed, selectedTool is now: '%s'", selectedTool.c_str());
	} else {
		LOG(DEBUG, "Game::selectTool - tool '%s' already selected", tool);
	}
}

/** Starts playing the given sound resource once. Releasing the sf::Sound after playback is done
 *  internally. */
void Game::playOnce(Path sound)
{
	if (!app.soundEnabled) return;
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
	sf::FloatRect view = win.getView().getViewport();

	//Pick a random value between 0 and the screen area.
	double r = (double)rand() / RAND_MAX * (view.width) * (view.height);

	//Iterate through the items that are in view, subtracting each item's area from the random
	//value until it drops below 0. That item will be given the chance to play the sound. This
	//effectively picks a random item in view, weighted by the item areas.
	Item::Item *pick = NULL;
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		if ((*i)->layer != 0) continue;
		const int2 & vp = (*i)->getPosition();
		const sf::Vector2u & vs = (*i)->getSize();
		if (vp.x+vs.x >= view.left && vp.x <= (view.left + view.width) && vp.y >= view.top && vp.y-vs.y <= (view.top + view.height)) {
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
