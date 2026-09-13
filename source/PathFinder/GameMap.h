#pragma once
#include <list>
#include <map>
#include "MapNode.h"


namespace OT {
	class FloorNode;
	namespace Item { 
		class Item;
		namespace Elevator { class Elevator; }
	}

	class GameMap {
	public:
		GameMap();
		~GameMap();
		void clear();

		MapNode* addNode(const MapNode::Point &p, Item::Item *item);
		void removeNode(const MapNode::Point &p, Item::Item *item);
		MapNode* findNode(const MapNode::Point &p, Item::Item *item);
		void handleElevatorResize(Item::Elevator::Elevator *draggingElevator, bool draggingElevatorLower, int &draggingElevatorStart);

	private:
		/// Creates the node at p if missing, wiring it into its floor list.
		MapNode *ensureNode(const MapNode::Point &p);
		/// Links a multi-story lobby's entrance floor to every extension
		/// floor it covers (OpenSkyWeb ISSUE-006): the atrium is open, so
		/// pedestrians can walk up to mezzanine elevator doors.
		void linkLobbyAtrium(const MapNode::Point &p, Item::Item *lobby);
		/// Removes the atrium links (and orphaned extension nodes).
		void unlinkLobbyAtrium(const MapNode::Point &p, Item::Item *lobby);
		std::map<MapNode::Point, MapNode *> gameMap;
		std::map<int, std::list<MapNode*> > mapNodesByFloor;
		std::map<int, FloorNode *> floorNodes;
	};
}
