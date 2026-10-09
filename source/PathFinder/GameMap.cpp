#include <cassert>
#include "../Item/Item.h"
#include "../Item/Elevator/Elevator.h"
#include "FloorNode.h"
#include "GameMap.h"
using namespace OT;


GameMap::GameMap() {}

GameMap::~GameMap() {
	clear();
}

void GameMap::clear() {
	for(std::map<MapNode::Point, MapNode *>::iterator i = gameMap.begin(); i != gameMap.end(); i++)
		delete i->second;
	gameMap.clear();
	mapNodesByFloor.clear();
	for (std::map<int, FloorNode *>::iterator i = floorNodes.begin(); i != floorNodes.end(); i++)
		delete i->second;
	floorNodes.clear();
}

MapNode* GameMap::addNode(const MapNode::Point &p, Item::Item *item) {
	if(!item) return NULL;
	//LOG(DEBUG, "Adding map node at %dx%d", p.x, p.y);

	// Create corresponding FloorNode if not available
	FloorNode *f;
	if(floorNodes.count(p.y) == 0) {
		f = new FloorNode(&mapNodesByFloor[p.y]);
		floorNodes[p.y] = f;
		f->position.x = INT_MIN;
		f->position.y = p.y;
		f->walkableBetween = &horizontalWalkable;
	} else f = floorNodes[p.y];

	if(!item->canHaulPeople()) {
		// Every lobby is a routing slot. Marker nodes let the pathfinder apply
		// the EXE's 16-slot overlapping-shaft transfer table.
		if(item->isLobby() && p.y == item->position.y) {
			MapNode::Point slotPoint(p.x, p.y);
			while (gameMap.count(slotPoint) && gameMap[slotPoint]->hasElevator)
				slotPoint.x++;
			MapNode *slot = ensureNode(slotPoint);
			if (!slot->transportItems[MapNode::UP] || slot->transportItems[MapNode::UP]->isLobby())
				slot->transportItems[MapNode::UP] = item;
			if (item->size.y > 1) linkLobbyAtrium(p, item);
		}
		return f; // Building item, do not add into transport map
	}

	MapNode *n = ensureNode(p);

	if(item->isStairlike() && p.y == item->position.y) {
		// If stairlike, automatically create pair node above and link accordingly.
		// Multi-story spiral stairs (size.y > 2) span more than two floors, so
		// the pair node is at the top of the stair's footprint, not just one
		// floor above the entry. People entering at position.y exit at
		// position.y + size.y - 1 (intermediate floors are skipped, matching
		// the original game where a spiral stair is one continuous spiral).
		const int upper_y = p.y + item->size.y - 1;
		// A multi-story lobby's atrium links every column it covers, so a
		// stair placed inside the lobby starts on an already-linked node.
		// The stair supersedes the atrium hop at its own tile (OpenSkyWeb
		// gamemap.js resolves the overlap the same way: last writer wins).
		assert(n->transportItems[MapNode::UP] == NULL ||
		       (n->transportItems[MapNode::UP]->isLobby() && n->transportItems[MapNode::UP]->size.y > 1));
		MapNode *n_upper = addNode(MapNode::Point(p.x, upper_y), item);

		n->neighbours[MapNode::UP] = n_upper;
		n->transportItems[MapNode::UP] = item;

		assert(n_upper->neighbours[MapNode::DOWN] == NULL);
		assert(n_upper->transportItems[MapNode::DOWN] == NULL);
		n_upper->neighbours[MapNode::DOWN] = n;
		n_upper->transportItems[MapNode::DOWN] = item;
	} else if(item->isElevator()) {
		n->hasElevator = true;
		if(item->prototype->icon == 5) n->hasServiceElevator = true;
		// Link to upper/lower floor node
		Item::Elevator::Elevator *e =(Item::Elevator::Elevator *) item;
		for(int i = p.y + 1; i < item->position.y + item->size.y; i++) {
			if(e->connectsFloor(i)) {
				MapNode::Point ep(p.x, i);
				if(gameMap.count(ep) == 0) addNode(ep, item);
				else {
					MapNode *upper = gameMap[ep];
					n->neighbours[MapNode::UP] = upper;
					n->transportItems[MapNode::UP] = item;

					if(upper->neighbours[MapNode::DOWN]) {
						n->neighbours[MapNode::DOWN] = upper->neighbours[MapNode::DOWN];
						n->transportItems[MapNode::DOWN] = item;
					}

					upper->neighbours[MapNode::DOWN] = n;
					upper->transportItems[MapNode::DOWN] = item;
				}
				break;
			}
		}

		for(int i = p.y - 1; i >= item->position.y; i--) {
			if(e->connectsFloor(i)) {
				assert(gameMap.count(MapNode::Point(p.x, i)) != 0);
				MapNode *lower = gameMap[MapNode::Point(p.x, i)];
				n->neighbours[MapNode::DOWN] = lower;
				n->transportItems[MapNode::DOWN] = item;

				if(lower->neighbours[MapNode::UP]) {
					n->neighbours[MapNode::UP] = lower->neighbours[MapNode::UP];
					n->transportItems[MapNode::UP] = item;
				}

				lower->neighbours[MapNode::UP] = n;
				lower->transportItems[MapNode::UP] = item;
				break;
			}
		}
	}

	return n;
}

/// Creates the node at p if missing, wiring it into its floor's node list
/// with LEFT/RIGHT links to its new neighbours.
MapNode *GameMap::ensureNode(const MapNode::Point &p) {
	if(floorNodes.count(p.y) == 0) {
		FloorNode *f = new FloorNode(&mapNodesByFloor[p.y]);
		floorNodes[p.y] = f;
		f->position.x = INT_MIN;
		f->position.y = p.y;
		f->walkableBetween = &horizontalWalkable;
	}


	MapNode *n;
	if(gameMap.count(p) == 0) {
		n = new MapNode(floorNodes[p.y]);
		gameMap[p] = n;
		n->position = p;

		// Just insert node if list is empty
		if(mapNodesByFloor[p.y].empty()) mapNodesByFloor[p.y].push_back(n);
		else {
			// Look for nearby nodes on same floor to insert
			MapNode *left = NULL;
			MapNode *right = NULL;
			for(std::list<MapNode *>::iterator i = mapNodesByFloor[p.y].begin(); i != mapNodesByFloor[p.y].end(); i++) {
				MapNode *node = *i;
				if(node->position.x <= p.x) left = node;
				else {
					right = node;
					mapNodesByFloor[p.y].insert(i, n);
					break;
				}
			}



			if(left) {
				if(!right) mapNodesByFloor[p.y].push_back(n); // Insert node as last node in list
				// YootTower floor holes: only link across a span that is
				// actually covered by floor (SimTower/OpenSky: always).
				if (spanWalkable(p.y, left->position.x, n->position.x)) {
					n->neighbours[MapNode::LEFT] = left;
					left->neighbours[MapNode::RIGHT] = n;
				}
			}


			if(right) {
				if (spanWalkable(p.y, n->position.x, right->position.x)) {
					n->neighbours[MapNode::RIGHT] = right;
					right->neighbours[MapNode::LEFT] = n;
				}
			}
		}


	} else {
		n = gameMap[p];
	}

	return n;
}

/// Links a multi-story lobby's entrance node to every extension floor the
/// lobby covers, turning those floors into first-class pedestrian (and thus
/// elevator pickup) floors (OpenSkyWeb ISSUE-006).
void GameMap::linkLobbyAtrium(const MapNode::Point &p, Item::Item *lobby) {
	MapNode *lower = ensureNode(p);
	for(int y = p.y + 1; y < lobby->position.y + lobby->size.y; y++) {
		MapNode *upper = ensureNode(MapNode::Point(p.x, y));
		lower->neighbours[MapNode::UP] = upper;
		lower->transportItems[MapNode::UP] = lobby;
		upper->neighbours[MapNode::DOWN] = lower;
		upper->transportItems[MapNode::DOWN] = lobby;
		lower = upper;
	}

}

/// Removes the atrium links of a multi-story lobby and drops extension
/// nodes that no other transport item still uses.
void GameMap::unlinkLobbyAtrium(const MapNode::Point &p, Item::Item *lobby) {
	if(gameMap.count(p) == 0) return;
	MapNode *entrance = gameMap[p];
	MapNode *chain = entrance->neighbours[MapNode::UP];
	entrance->neighbours[MapNode::UP] = NULL;
	entrance->transportItems[MapNode::UP] = NULL;
	while(chain) {
		MapNode *next = chain->neighbours[MapNode::UP];
		if(chain->transportItems[MapNode::DOWN] == lobby) {
			chain->neighbours[MapNode::DOWN] = NULL;
			chain->transportItems[MapNode::DOWN] = NULL;
		}

		if(chain->transportItems[MapNode::UP] == lobby) {
			chain->neighbours[MapNode::UP] = NULL;
			chain->transportItems[MapNode::UP] = NULL;
		}


		// Drop the extension node unless another transport still uses it.
		if(!chain->hasElevator && !chain->hasServiceElevator
		   && !chain->transportItems[MapNode::UP] && !chain->transportItems[MapNode::DOWN]) {
			MapNode *cl = chain->neighbours[MapNode::LEFT];
			MapNode *cr = chain->neighbours[MapNode::RIGHT];
			if(cl) cl->neighbours[MapNode::RIGHT] = (cr && spanWalkable(chain->position.y, cl->position.x, cr->position.x)) ? cr : NULL;
			if(cr) cr->neighbours[MapNode::LEFT] = (cl && spanWalkable(chain->position.y, cl->position.x, cr->position.x)) ? cl : NULL;
			gameMap.erase(chain->position);
			for(std::list<MapNode *>::iterator i = mapNodesByFloor[chain->position.y].begin(); i != mapNodesByFloor[chain->position.y].end(); i++) {
				if(*i == chain) {
					mapNodesByFloor[chain->position.y].erase(i);
					break;
				}
			}


			delete chain;
		}

		chain = next;
	}

}

void GameMap::removeNode(const MapNode::Point &p, Item::Item *item) {
	if(!item) return;
	if(!item->canHaulPeople()) {
		if(item->isLobby() && p.y == item->position.y) {
			if (item->size.y > 1) unlinkLobbyAtrium(p, item);
			else {
				for (std::list<MapNode *>::iterator it = mapNodesByFloor[p.y].begin(); it != mapNodesByFloor[p.y].end(); ++it) {
					MapNode *slot = *it;
					if (slot->transportItems[MapNode::UP] != item) continue;
					slot->transportItems[MapNode::UP] = NULL;
					if (!slot->hasElevator && !slot->hasServiceElevator && !slot->transportItems[MapNode::DOWN] && !slot->neighbours[MapNode::UP] && !slot->neighbours[MapNode::DOWN]) {
						MapNode *sl = slot->neighbours[MapNode::LEFT];
						MapNode *sr = slot->neighbours[MapNode::RIGHT];
						if (sl) sl->neighbours[MapNode::RIGHT] = (sr && spanWalkable(p.y, sl->position.x, sr->position.x)) ? sr : NULL;
						if (sr) sr->neighbours[MapNode::LEFT] = (sl && spanWalkable(p.y, sl->position.x, sr->position.x)) ? sl : NULL;
						gameMap.erase(slot->position);
						mapNodesByFloor[p.y].erase(it);
						delete slot;
					}
					break;
				}
			}
		}
		return;
	}

	if(gameMap.count(p) == 0) return;
	MapNode *n = gameMap[p];
	//LOG(DEBUG, "Removing node %s at %dx%d", item->desc().c_str(), p.x, p.y);

	// Update all neighbour links
	if(item->isStairlike() && p.y == item->position.y) {
		n->neighbours[MapNode::UP]->neighbours[MapNode::DOWN] = NULL;
		n->neighbours[MapNode::UP]->transportItems[MapNode::DOWN] = NULL;

		n->neighbours[MapNode::UP] = NULL;
		n->transportItems[MapNode::UP] = NULL;
		
		removeNode(MapNode::Point(p.x, p.y + item->size.y - 1), item);
	} else if(item->isElevator()) {
		assert(n->hasElevator == true);
		n->hasElevator = false;
		if(item->prototype->icon == 5) n->hasServiceElevator = false;

		// Link upper & lower floor node to skip this node
		if(n->neighbours[MapNode::UP]) {
			n->neighbours[MapNode::UP]->neighbours[MapNode::DOWN] = n->neighbours[MapNode::DOWN];
			if(!n->neighbours[MapNode::DOWN]) {
				n->neighbours[MapNode::UP]->transportItems[MapNode::DOWN] = NULL;
			}
		}

		if(n->neighbours[MapNode::DOWN]) {
			n->neighbours[MapNode::DOWN]->neighbours[MapNode::UP] = n->neighbours[MapNode::UP];
			if(!n->neighbours[MapNode::UP]) {
				n->neighbours[MapNode::DOWN]->transportItems[MapNode::UP] = NULL;
			}
		}

		n->neighbours[MapNode::UP] = NULL;
		n->transportItems[MapNode::UP] = NULL;
		n->neighbours[MapNode::DOWN] = NULL;
		n->transportItems[MapNode::DOWN] = NULL;
	}

	// Delete and erase only if no other overlapping item
	if( !n->hasElevator && !n->hasServiceElevator &&
		!n->transportItems[MapNode::UP] && !n->transportItems[MapNode::DOWN]) {
		//LOG(DEBUG, "Deleting node at %dx%d", p.x, p.y);
		MapNode *nl = n->neighbours[MapNode::LEFT];
		MapNode *nr = n->neighbours[MapNode::RIGHT];
		if(nl) {
			nl->neighbours[MapNode::RIGHT] = (nr && spanWalkable(p.y, nl->position.x, nr->position.x)) ? nr : NULL;
		}

		if(nr) {
			nr->neighbours[MapNode::LEFT] = (nl && spanWalkable(p.y, nl->position.x, nr->position.x)) ? nl : NULL;
		}

		gameMap.erase(p);
		for(std::list<MapNode *>::iterator i = mapNodesByFloor[p.y].begin(); i != mapNodesByFloor[p.y].end(); i++) {
			if(*i == n) {
				mapNodesByFloor[p.y].erase(i);
				break;
			}
		}
		delete n;
	}
}

/// Rebuilds one floor's LEFT/RIGHT links: adjacent nodes (x-sorted list)
/// link only when horizontalWalkable covers the span between them. Without
/// a test installed this reproduces the previous link-everything graph.
void GameMap::revalidateFloorLinks(int floor) {
	std::map<int, std::list<MapNode*> >::iterator fi = mapNodesByFloor.find(floor);
	if(fi == mapNodesByFloor.end()) return;
	std::list<MapNode *> &nodes = fi->second;
	for(std::list<MapNode *>::iterator n = nodes.begin(); n != nodes.end(); ++n) {
		(*n)->neighbours[MapNode::LEFT] = NULL;
		(*n)->neighbours[MapNode::RIGHT] = NULL;
	}
	MapNode *prev = NULL;
	for(std::list<MapNode *>::iterator n = nodes.begin(); n != nodes.end(); ++n) {
		MapNode *node = *n;
		if(prev && spanWalkable(floor, prev->position.x, node->position.x)) {
			prev->neighbours[MapNode::RIGHT] = node;
			node->neighbours[MapNode::LEFT] = prev;
		}
		prev = node;
	}
}

void GameMap::revalidateAllFloorLinks() {
	for(std::map<int, std::list<MapNode*> >::iterator i = mapNodesByFloor.begin(); i != mapNodesByFloor.end(); ++i)
		revalidateFloorLinks(i->first);
}

MapNode* GameMap::findNode(const MapNode::Point &p, Item::Item *item) {
	if(!item) return NULL;
	if(!item->canHaulPeople()) {
		if(floorNodes.count(p.y) == 0) return NULL;
		else return floorNodes[p.y];
	}

	MapNode *n = NULL;
	if(gameMap.count(p) == 0) return NULL;
	else n = gameMap[p];

	if(item->isElevator() && (!n->hasElevator || (n->transportItems[MapNode::UP] != item && n->transportItems[MapNode::DOWN] != item)))
		return NULL;
	else if(item->isStairlike() && n->transportItems[MapNode::UP] != item)
		return NULL;
	else
		return n;
}

void GameMap::handleElevatorResize(Item::Elevator::Elevator *draggingElevator, bool draggingElevatorLower, int &draggingElevatorStart) {
	MapNode::Point p(draggingElevator->getPosition().x + draggingElevator->getSize().x/2, 0);
	if (draggingElevatorLower) {
		if (draggingElevatorStart <= draggingElevator->position.y) {
			// Elevator bottom shifted up
			int previousLowestAccessibleFloor = draggingElevatorStart;
			while (previousLowestAccessibleFloor < draggingElevator->position.y) {
				p.y = previousLowestAccessibleFloor;
				if (findNode(p, draggingElevator)) removeNode(p, draggingElevator);
				previousLowestAccessibleFloor++;
			}
		} else {
			// Elevator bottom shifted down
			p.y = draggingElevator->position.y;
			while (p.y < draggingElevatorStart) {
				if (draggingElevator->connectsFloor(p.y)) {
					addNode(p, draggingElevator);
					break;
				}
				p.y++;
			}
		}
		draggingElevatorStart = draggingElevator->position.y;
	} else {
		if (draggingElevatorStart >= draggingElevator->position.y + draggingElevator->size.y - 1) {
			// Elevator top shifted down
			int previousHighestAccessibleFloor = draggingElevatorStart;
			while (previousHighestAccessibleFloor >= draggingElevator->position.y + draggingElevator->size.y) {
				p.y = previousHighestAccessibleFloor;
				if (findNode(p, draggingElevator)) removeNode(p, draggingElevator);
				previousHighestAccessibleFloor--;
			}
		} else {
			// Elevator top shifted up
			p.y = draggingElevatorStart + 1;
			while (p.y < draggingElevator->position.y + draggingElevator->size.y) {
				if (draggingElevator->connectsFloor(p.y)) {
					addNode(p, draggingElevator);
					break;
				}
				p.y++;
			}
		}
		draggingElevatorStart = draggingElevator->position.y + draggingElevator->size.y - 1;
	}
}
