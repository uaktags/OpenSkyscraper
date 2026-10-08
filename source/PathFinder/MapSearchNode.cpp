#include <cmath>

#include "../Item/Elevator/Elevator.h"
#include "../Item/Elevator/Queue.h"
#include "../Item/Factory.h"
#include "../Item/Item.h"
#include "FloorNode.h"
#include "MapSearchNode.h"
#include "../YootCatalog.h"
#include "../YootRules.h"
using namespace OT;


namespace {
static const int kMaxRoutingSlots = 16;
static const int kExpressSlotWidth = 6;
static const int kStandardSlotWidth = 4;
static bool elevatorMatchesRoute(const Item::Item *item, bool serviceRoute) {
	return item && item->isElevator() && ((item->prototype->icon == 5) == serviceRoute);
}
static Item::Item *elevatorAt(const MapNode *node, bool serviceRoute) {
	if (!node) return NULL;
	for (int d = MapNode::UP; d <= MapNode::DOWN; ++d) {
		Item::Item *item = node->transportItems[d];
		if (elevatorMatchesRoute(item, serviceRoute)) return item;
	}
	return NULL;
}
static int elevatorQueueLength(const Item::Item *item, int floor, Item::Elevator::Elevator::Direction direction) {
	const Item::Elevator::Elevator *e = static_cast<const Item::Elevator::Elevator *>(item);
	for (Item::Elevator::Elevator::Queues::const_iterator q = e->queues.begin(); q != e->queues.end(); ++q)
		if ((*q)->floor == floor && (*q)->direction == direction) return static_cast<int>((*q)->people.size());
	return 0;
}
/// YootTower floor holes: the start item's own hop to the first transport
/// must not cross an uncovered span either (NULL test = always walkable).
static bool spanReachable(const MapNode *floor, int x1, int x2) {
	if (!floor || !floor->walkableBetween || !*floor->walkableBetween) return true;
	return (*floor->walkableBetween)(floor->position.y, x1, x2);
}
static bool destinationElevatorMatchesRoute(const MapNode *node, bool serviceRoute) {
	return elevatorAt(node, serviceRoute) != NULL;
}
static bool hasTransferSlot(const MapSearchNode *start, const MapNode *dest) {
	if (!start || !start->mapNode || !dest || !start->mapNode->floorNode) return false;
	Item::Item *from = start->parent_item && start->parent_item->isElevator()
		? start->parent_item : elevatorAt(start->mapNode, start->serviceRoute);
	Item::Item *to = elevatorAt(dest, start->serviceRoute);
	if (!from || !to) return false;
	const int fromWidth = from->prototype->icon == 6 ? kExpressSlotWidth : kStandardSlotWidth;
	const int toWidth = to->prototype->icon == 6 ? kExpressSlotWidth : kStandardSlotWidth;
	int slots = 0;
	const std::list<MapNode *> *nodes = start->mapNode->floorNode->nodesOnFloor;
	for (std::list<MapNode *>::const_iterator n = nodes->begin(); n != nodes->end(); ++n) {
		Item::Item *lobby = NULL;
		for (int d = MapNode::UP; d <= MapNode::DOWN; ++d)
			if ((*n)->transportItems[d] && (*n)->transportItems[d]->isLobby()) { lobby = (*n)->transportItems[d]; break; }
		if (!lobby) continue;
		const bool fromOverlaps = from->position.x < lobby->position.x + lobby->size.x
			&& from->position.x + fromWidth > lobby->position.x;
		const bool toOverlaps = to->position.x < lobby->position.x + lobby->size.x
			&& to->position.x + toWidth > lobby->position.x;
		if (!fromOverlaps && !toOverlaps) continue;
		if (++slots > kMaxRoutingSlots) return false;
		if (fromOverlaps && toOverlaps) return true;
	}
	return false;
}
}

MapSearchNode::MapSearchNode() : mapNode(NULL), parent_item(NULL),
numStairs(0), numEscalators(0), numElevators(0),
stairsSinceTransport(0), escalatorsSinceTransport(0),
walkFloorsSinceTransport(0), walkingHasStairs(false),
g(0), h(0),
                                 start_point(INT_MIN, INT_MIN), end_point(INT_MIN, INT_MIN),
                                 serviceRoute(false) {}

MapSearchNode::MapSearchNode(const OT::MapNode *mp) : mapNode(mp), parent_item(NULL),
                                                     numStairs(0), numEscalators(0), numElevators(0),
                                                     stairsSinceTransport(0), escalatorsSinceTransport(0),
                                                     walkFloorsSinceTransport(0), walkingHasStairs(false),
                                                     g(0), h(0),
                                                     start_point(INT_MIN, INT_MIN), end_point(INT_MIN, INT_MIN),
                                                     serviceRoute(false) {}

float MapSearchNode::GetCost(MapSearchNode &successor) {
	Item::Item *i = getItemOnRoute(&successor);
	successor.parent_item = i;
	successor.numStairs = numStairs;
	successor.numEscalators = numEscalators;
	successor.numElevators = numElevators;
	successor.stairsSinceTransport = stairsSinceTransport;
	successor.escalatorsSinceTransport = escalatorsSinceTransport;
	successor.walkFloorsSinceTransport = walkFloorsSinceTransport;
	successor.walkingHasStairs = walkingHasStairs;
	successor.start_point = start_point;
	successor.end_point = end_point;
	successor.serviceRoute = serviceRoute;
	successor.g = g;

	int traverse_cost = 0;
	if (!i) {
		// Horizontal walking is represented by the transport score; walking
		// inside a lobby/zone is free in the EXE route table.
		traverse_cost = 0;
	} else if (i->isStairlike()) {
		const int distance = std::abs((mapNode->floorNode ? start_point.x : mapNode->position.x) - start_point.x);
		const bool stairs = i->prototype->icon == ICON_STAIRS;
		// Escalator score is 8*d; therefore scores below 640 are the
		// TransferT early-accept branch and beat elevator candidates.
		traverse_cost = ROUTE_COST_PER_CELL * distance;
		// Only a flagged one-way escalator carries the stair base; the native
		// catalog currently has no such variant, so keep this branch explicit.
		if (stairs || i->prototype->id == "escalator-one-way") traverse_cost += ROUTE_BASE_COST;
		if (stairs) {
			successor.numStairs++;
			successor.stairsSinceTransport++;
			successor.walkingHasStairs = true;
		} else successor.numEscalators++;
		successor.walkFloorsSinceTransport += std::abs(successor.mapNode->position.y - mapNode->position.y);
	// The next transport score measures from this landing, not the original
	// room, after a multi-leg route.
	successor.start_point.x = mapNode->position.x;
	} else if (i->isElevator()) {
		const bool boarding = i != parent_item;
		if (boarding) {
			const bool transfer = numElevators > 0 || (parent_item && parent_item->isElevator());
			const int floor = mapNode->position.y;
			const Item::Elevator::Elevator::Direction direction = successor.mapNode->position.y >= floor
				? Item::Elevator::Elevator::kUp : Item::Elevator::Elevator::kDown;
			const int queue_length = elevatorQueueLength(i, floor, direction);
			const bool full = queue_length >= ELEVATOR_QUEUE_CAPACITY;
			const int origin_x = mapNode->floorNode ? start_point.x : mapNode->position.x;
			const int elevator_x = i->position.x + i->size.x / 2;
			const int distance = std::abs(origin_x - elevator_x);
			if (i->prototype->icon == 6)
				traverse_cost = queue_length + (transfer ? ROUTE_TRANSFER_COST : ROUTE_BASE_COST);
			else if (transfer)
				traverse_cost = ROUTE_COST_PER_CELL * distance + (full ? ROUTE_TRANSFER_FULL_COST : ROUTE_TRANSFER_COST);
			else
				traverse_cost = ROUTE_COST_PER_CELL * distance + ROUTE_BASE_COST + (full ? ROUTE_FULL_COST : 0);
			successor.numElevators++;
			successor.start_point.x = elevator_x;
		}
		successor.stairsSinceTransport = 0;
		successor.escalatorsSinceTransport = 0;
		successor.walkFloorsSinceTransport = 0;
		successor.walkingHasStairs = false;
	} else if (i->prototype->icon == ICON_LOBBY) {
		successor.stairsSinceTransport = 0;
		successor.escalatorsSinceTransport = 0;
		successor.walkFloorsSinceTransport = 0;
		successor.walkingHasStairs = false;
	} else traverse_cost = INHIBITORY_COST;

	if (successor.mapNode->hasElevator) {
		successor.stairsSinceTransport = 0;
		successor.escalatorsSinceTransport = 0;
		successor.walkFloorsSinceTransport = 0;
		successor.walkingHasStairs = false;
	}
	// YootTower (P3, T2Mover::CalcScore 0x1003ef20): a mover leg costs its
	// MVDF score x floors (Stairs 50, Escalator 30, Simple 15, Service 8,
	// Large 5); walking stays free. Only catalogued movers carry a score,
	// so SimTower/OpenSky routing is untouched.
	if (i && i->prototype->yoot && i->prototype->yoot->routeScore > 0 &&
	    (i->isStairlike() || i->isElevator()))
		traverse_cost = OT::Yoot::moverRouteScore(i->prototype->yoot->routeScore,
			successor.mapNode->position.y - mapNode->position.y);
	successor.g += (float)traverse_cost;
	return (float)traverse_cost;
}

float MapSearchNode::GoalDistanceEstimate(MapSearchNode &nodeGoal) {
	// Escalators can score zero at the same x, so zero is the admissible lower bound.
	h = 0;
	(void)nodeGoal;
	return h;
}

bool MapSearchNode::GetSuccessors(AStarSearch< MapSearchNode > *astarsearch, MapSearchNode *parent_node) {
	MapSearchNode n;
	MapNode *left = NULL;
	MapNode *right = NULL;
	bool ret;
	if (!parent_node && !mapNode->floorNode) {
		std::list<MapNode *> *nodesOnFloor = mapNode->nodesOnFloor;
		assert(nodesOnFloor != NULL);
		for (std::list<MapNode *>::const_iterator i = nodesOnFloor->begin(); i != nodesOnFloor->end(); ++i) {
			MapNode *node = *i;
			if (!spanReachable(mapNode, start_point.x, node->position.x)) continue;
			if (node->position.x <= start_point.x && canTransfer(this, node, MapNode::LEFT)) left = node;
			else if (canTransfer(this, node, MapNode::RIGHT)) { right = node; break; }
		}
		if (left) { createNode(n, left); ret = astarsearch->AddSuccessor(n); if (!ret) return false; }
		if (right) { createNode(n, right); ret = astarsearch->AddSuccessor(n); if (!ret) return false; }
		return true;
	}
	if (mapNode->position.y == end_point.y && !astarsearch->GetSolutionEnd()->mapNode->floorNode) {
		if (!mapNode->floorNode) return false;
		createNode(n, mapNode->floorNode);
		return astarsearch->AddSuccessor(n);
	}
	MapNode *node = mapNode->neighbours[MapNode::LEFT];
	while (node && !left) { if (!canTransfer(this, node, MapNode::LEFT)) node = node->neighbours[MapNode::LEFT]; else left = node; }
	node = mapNode->neighbours[MapNode::RIGHT];
	while (node && !right) { if (!canTransfer(this, node, MapNode::RIGHT)) node = node->neighbours[MapNode::RIGHT]; else right = node; }
	if (left) { createNode(n, left); ret = astarsearch->AddSuccessor(n); if (!ret) return false; }
	if (right) { createNode(n, right); ret = astarsearch->AddSuccessor(n); if (!ret) return false; }
	if (mapNode->neighbours[MapNode::UP]) {
		node = mapNode->neighbours[MapNode::UP];
		if (mapNode->hasElevator) {
			while (node) { if (canTransfer(this, node, MapNode::UP)) { createNode(n, node); ret = astarsearch->AddSuccessor(n); if (!ret) return false; } node = node->neighbours[MapNode::UP]; }
		} else if (canTransfer(this, node, MapNode::UP)) { createNode(n, node); ret = astarsearch->AddSuccessor(n); if (!ret) return false; }
	}
	if (mapNode->neighbours[MapNode::DOWN]) {
		node = mapNode->neighbours[MapNode::DOWN];
		if (mapNode->hasElevator) {
			while (node) { if (canTransfer(this, node, MapNode::DOWN)) { createNode(n, node); ret = astarsearch->AddSuccessor(n); if (!ret) return false; } node = node->neighbours[MapNode::DOWN]; }
		} else if (canTransfer(this, node, MapNode::DOWN)) { createNode(n, node); ret = astarsearch->AddSuccessor(n); if (!ret) return false; }
	}
	return true;
}

bool MapSearchNode::IsGoal(MapSearchNode &nodeGoal) {
	if(mapNode == nodeGoal.mapNode) {
		nodeGoal.g = g;
		nodeGoal.h = h;
		return true;
	} else return false;
}

bool MapSearchNode::IsSameState(MapSearchNode &rhs) {
	return mapNode == rhs.mapNode
		&& parent_item == rhs.parent_item
		&& stairsSinceTransport == rhs.stairsSinceTransport
		&& escalatorsSinceTransport == rhs.escalatorsSinceTransport
		&& walkFloorsSinceTransport == rhs.walkFloorsSinceTransport
		&& walkingHasStairs == rhs.walkingHasStairs
		&& numElevators == rhs.numElevators;
}

OT::Item::Item * MapSearchNode::getItemOnRoute(const MapSearchNode *successor) {
	// NOTE: Calling this on the start and end nodes will not give correct results.
	// Always use known start and destination items instead.

	const OT::MapNode *s_mapnode;

	if(!successor || !successor->mapNode) return NULL;
	else s_mapnode = successor->mapNode;

	if(mapNode->neighbours[MapNode::UP] == s_mapnode)
		 return mapNode->transportItems[MapNode::UP];
	else if(mapNode->neighbours[MapNode::DOWN] == s_mapnode)
		 return mapNode->transportItems[MapNode::DOWN];
	else if(mapNode->transportItems[MapNode::UP] && mapNode->transportItems[MapNode::UP] == s_mapnode->transportItems[MapNode::DOWN])
		return mapNode->transportItems[MapNode::UP];
	else if(mapNode->transportItems[MapNode::DOWN] && mapNode->transportItems[MapNode::DOWN] == s_mapnode->transportItems[MapNode::UP])
		return mapNode->transportItems[MapNode::DOWN];
	else
		return NULL;
}

bool MapSearchNode::canTransfer(const MapSearchNode *start, const OT::MapNode *dest, const OT::MapNode::Direction dir) const {
	if (!start || !start->mapNode || !dest) return false;
	if (dir == MapNode::UP || dir == MapNode::DOWN) {
		Item::Item *item = start->mapNode->transportItems[dir];
		if (!item) return false;
		if (item->isElevator()) return elevatorMatchesRoute(item, start->serviceRoute);
		if (!item->isStairlike() || (start->serviceRoute && item->prototype->icon != ICON_STAIRS)) return false;
		const int floors = std::abs(dest->position.y - start->mapNode->position.y);
		const bool stairs = start->walkingHasStairs || item->prototype->icon == ICON_STAIRS;
		return start->walkFloorsSinceTransport + floors <= (stairs ? MAX_STAIR_WALK_FLOORS : MAX_ESCALATOR_WALK_FLOORS);
	}
	if (!dest->hasElevator) return true;
	if (!destinationElevatorMatchesRoute(dest, start->serviceRoute)) return false;
	const int rides = start->numElevators + ((start->numElevators == 0 && start->parent_item && start->parent_item->isElevator()) ? 1 : 0);
	if (start->serviceRoute) return rides == 0;
	if (rides >= 2) return false;
	return rides == 0 || hasTransferSlot(start, dest);
}

void MapSearchNode::createNode(MapSearchNode &n, const OT::MapNode *node) {
	n = MapSearchNode(node);
	n.start_point = start_point;
	n.end_point = end_point;
	n.serviceRoute = serviceRoute;
}
