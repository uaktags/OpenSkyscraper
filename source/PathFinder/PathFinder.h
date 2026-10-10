#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "IndexedAStar.h"
#include "MapSearchNode.h"
#include "../Route.h"

namespace OT {
	namespace Item { class Item; }
	class MapNode;

	class PathFinder {
	public:
		PathFinder();
		~PathFinder();
		/// graphVersion identifies the node graph the nodes belong to (GameMap::version).
		/// The search never reads the end item, only the end map node (a floor node for
		/// every non-transport item), so one search result serves every item on that floor.
		/// Results are cached per (start item, start node, end node, service) and reused
		/// until the graph changes or an elevator-queue reading the search consumed
		/// changes, so a cache hit is always exactly the route a fresh search returns.
		/// cacheable=false forces a fresh search (YootTower slab tests are live state).
		Route findRoute(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute, unsigned graphVersion, bool cacheable = true);
		/// True when a route exists, i.e. !findRoute(...).empty(). Costs (and so the live
		/// elevator-queue lengths) only order the search, never decide reachability, so one
		/// exhaustive exploration from a start answers every destination: a floor-node goal
		/// is reachable iff the exploration touches that floor (the search cuts every node
		/// of the goal floor over to the floor node), any other goal iff it touches the
		/// node. Used only when the exploration fits in half the node pool, where no
		/// expansion order can exhaust it; otherwise this is findRoute(...).empty().
		bool routeExists(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute, unsigned graphVersion, bool cacheable = true);
		/// Drops every cached search (items holding raw pointers were removed).
		void clearCache() { cache.clear(); reachCache.clear(); }
		struct Stats { unsigned long hits = 0, misses = 0, existenceHits = 0; };
		Stats stats;

	private:
		/// Search outcome before the end item is attached: the route nodes up to
		/// (not including) the end item, where the end item lands, and the score.
		struct Raw {
			bool found = false;
			std::vector<Route::Node> prefix;
			int endFloor = 0;
			int score = 0;
			bool oom = false;       ///< node pool exhausted: "no route" is an artefact of the search
			int peakNodes = 0;      ///< most search nodes held at once
		};
		struct CacheKey {
			const Item::Item *start;
			const MapNode *startNode, *endNode;
			bool service;
			bool operator==(const CacheKey &o) const { return start == o.start && startNode == o.startNode && endNode == o.endNode && service == o.service; }
		};
		struct CacheKeyHash {
			size_t operator()(const CacheKey &k) const {
				return std::hash<const void *>()(k.start) * 31u ^ std::hash<const void *>()(k.startNode) * 17u ^ std::hash<const void *>()(k.endNode) * 7u ^ (k.service ? 1u : 0u);
			}
		};
		struct CacheEntry {
			Raw raw;
			std::vector<MapSearchNode::QueueDep> deps;
		};
		std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> cache;
		/// A floor-node start explores identically for every start item whose first
		/// expansion seeds the same two nodes, so those share one entry (start == NULL).
		struct ReachKey {
			const Item::Item *start;
			const MapNode *startNode, *left, *right;
			bool service;
			bool operator==(const ReachKey &o) const { return start == o.start && startNode == o.startNode && left == o.left && right == o.right && service == o.service; }
		};
		struct ReachKeyHash {
			size_t operator()(const ReachKey &k) const {
				return std::hash<const void *>()(k.start) * 31u ^ std::hash<const void *>()(k.startNode) * 17u ^ std::hash<const void *>()(k.left) * 7u ^ std::hash<const void *>()(k.right) * 3u ^ (k.service ? 1u : 0u);
			}
		};
		struct Reach {
			bool trusted = false;
			std::unordered_set<const MapNode *> nodes;
			std::unordered_set<int> floors;
		};
		std::unordered_map<ReachKey, Reach, ReachKeyHash> reachCache;
		const Reach &reach(const MapNode *start_mapnode, Item::Item *start_item, bool serviceRoute);
		unsigned cacheVersion = 0;

		/// Recreated after an out-of-memory search: stlastar leaks the
		/// node it had just popped, which would shrink the fixed node
		/// pool until every later search fails to start.
		std::unique_ptr< IndexedAStarSearch< MapSearchNode > > astarsearch;
		unsigned int SearchState;

		Raw search(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute);
		static Route materialise(const Raw &raw, Item::Item *start_item, Item::Item *end_item);
		void clear();
		void buildRaw(Raw &raw, Item::Item *start_item, Item::Item *end_item);
	};
}
