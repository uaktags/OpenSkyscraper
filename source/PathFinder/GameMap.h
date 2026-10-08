#pragma once
#include <functional>
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

		/// YootTower floor holes: Game installs a coverage test here; an
		/// empty function keeps the SimTower/OpenSky behaviour (every
		/// same-floor pair links, as before).
		std::function<bool(int, int, int)> horizontalWalkable;
		/// Rebuild one floor's LEFT/RIGHT links against the test (floor
		/// coverage changed: a hole was punched or covered).
		void revalidateFloorLinks(int floor);
		/// revalidateFloorLinks for every floor that has nodes.
		void revalidateAllFloorLinks();

	private:
		/// Creates the node at p if missing, wiring it into its floor list.
		MapNode *ensureNode(const MapNode::Point &p);
		/// True when the closed x-span on floor y is walkable (no test
		/// installed = always true).
		bool spanWalkable(int y, int x1, int x2) const {
			return !horizontalWalkable || horizontalWalkable(y, x1, x2);
		}
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
