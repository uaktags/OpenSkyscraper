#pragma once
#include <memory>
#include <stlastar.h>
#include "MapSearchNode.h"
#include "../Route.h"

namespace OT {
	namespace Item { class Item; }
	class MapNode;

	class PathFinder {
	public:
		PathFinder();
		~PathFinder();
		Route findRoute(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute);

	private:
		/// Recreated after an out-of-memory search: stlastar leaks the
		/// node it had just popped, which would shrink the fixed node
		/// pool until every later search fails to start.
		std::unique_ptr< AStarSearch< MapSearchNode > > astarsearch;
		unsigned int SearchState;

		void clear();
		void buildRoute(Route &r, Item::Item *start_item, Item::Item *end_item);
	};
}

