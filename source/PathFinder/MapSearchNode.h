#pragma once
#include "IndexedAStar.h"
#include <vector>
#include "GameMap.h"
#include "MapNode.h"


namespace OT { 
	namespace Item { class Item; }
}

class MapSearchNode {
public:
	const OT::MapNode *mapNode;
	OT::Item::Item *parent_item;
	// The shaft last ridden, kept while walking along a floor so a transfer to
	// a shaft that is not the next node over is still judged against the
	// elevator the rider arrived on (hasTransferSlot).
	OT::Item::Item *lastElevator;
	int numStairs;
	int numEscalators;
	// Stairlike usage since the last elevator/lobby contact (OpenSkyWeb
	// PORT NOTE 1): the stair budget refills at transport contact instead
	// of being cumulative over the whole journey.
	int stairsSinceTransport;
	int escalatorsSinceTransport;
	// Floors walked since the last transport contact; the EXE caps an
	// all-escalator leg at six floors and any stair leg at three.
	int walkFloorsSinceTransport;
	bool walkingHasStairs;
	int numElevators;
	float g;
	float h;
	OT::MapNode::Point start_point;
	OT::MapNode::Point end_point;
	bool serviceRoute;

	MapSearchNode();
	explicit MapSearchNode(const OT::MapNode *mp);

	float GetCost(MapSearchNode &successor);
	float GoalDistanceEstimate(MapSearchNode &nodeGoal);
	bool GetSuccessors(IndexedAStarSearch<MapSearchNode> *astarsearch, MapSearchNode *parent_node);
	bool IsGoal(MapSearchNode &nodeGoal);
	bool IsSameState(MapSearchNode &rhs);
	/// Hash over exactly the fields IsSameState compares (stlastar index).
	size_t StateHash() const;

	/// A live elevator-queue reading the search consumed. A cached route stays
	/// valid only while every reading still holds, so it equals a fresh search.
	struct QueueDep {
		const OT::Item::Item *elevator;
		int floor;
		int direction;
		int value;   ///< exact queue length (express) or 0/1 "full" flag
		bool exact;
		bool sameKey(const QueueDep &o) const { return elevator == o.elevator && floor == o.floor && direction == o.direction; }
	};
	/// Routes subsequent searches' queue readings into *deps (NULL stops).
	static void recordQueueDeps(std::vector<QueueDep> *deps);
	/// True when every recorded reading still reads the same.
	static bool queueDepsHold(const std::vector<QueueDep> &deps);

	/// The nearest enterable nodes either side of start_point on a floor-node start's
	/// floor (the first expansion's two successors); NULL when none.
	void firstExpansionSeeds(const OT::MapNode *&left, const OT::MapNode *&right) const;

	OT::Item::Item * getItemOnRoute(const MapSearchNode *successor);
	bool canTransfer(const MapSearchNode *start, const OT::MapNode *dest, const OT::MapNode::Direction dir) const;
	void createNode(MapSearchNode &n, const OT::MapNode *node);

private:
	static const int ROUTE_COST_PER_CELL = 8;        // TransferT score table
	static const int ROUTE_BASE_COST = 640;           // 0x280
	static const int ROUTE_FULL_COST = 1000;          // 0x3e8
	static const int ROUTE_TRANSFER_COST = 3000;      // 0xbb8
	static const int ROUTE_TRANSFER_FULL_COST = 6000; // 0x1770
	static const int ELEVATOR_QUEUE_CAPACITY = 40;    // TransferT queue test
	static const int MAX_ESCALATOR_WALK_FLOORS = 6;   // TransferT 0x0dc0
	static const int MAX_STAIR_WALK_FLOORS = 3;       // TransferT 0x0e80
	static const int MAX_ROUTING_SLOTS = 16;          // TransferT DS:0xdb9c
	static const int EXPRESS_SLOT_WIDTH = 6;           // lobby overlap footprint
	static const int STANDARD_SLOT_WIDTH = 4;          // std/service footprint
	static const int INHIBITORY_COST = 10000;         // malformed graph guard
};
