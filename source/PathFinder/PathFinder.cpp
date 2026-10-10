#include <climits>
#include <cmath>
#include "../Item/Item.h"
#include "../Route.h"
#include "MapNode.h"
#include "PathFinder.h"
#include "../Logger.h"
using namespace OT;


// stlastar's default allocator holds 1000 search nodes. Large imported towers
// (eight express shafts, two-ride transfers, one state per ridden shaft) need
// more than that, and an exhausted allocator reports "no route"; the EXE has no
// such limit.
static const int kMaxSearchNodes = 20000;

PathFinder::PathFinder() : astarsearch(new IndexedAStarSearch< MapSearchNode >(kMaxSearchNodes)), SearchState(IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_NOT_INITIALISED) {}

PathFinder::~PathFinder() { clear(); }

static const size_t kMaxCachedRoutes = 60000;

Route PathFinder::findRoute(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute, unsigned graphVersion, bool cacheable) {
	if(!cacheable) return materialise(search(start_mapnode, end_mapnode, start_item, end_item, serviceRoute), start_item, end_item);
	if(graphVersion != cacheVersion) {
		cache.clear();
		reachCache.clear();
		cacheVersion = graphVersion;
	}
	const CacheKey key = { start_item, start_mapnode, end_mapnode, serviceRoute };
	std::unordered_map<CacheKey, CacheEntry, CacheKeyHash>::iterator hit = cache.find(key);
	if(hit != cache.end() && MapSearchNode::queueDepsHold(hit->second.deps)) {
		stats.hits++;
		return materialise(hit->second.raw, start_item, end_item);
	}
	stats.misses++;
	CacheEntry entry;
	MapSearchNode::recordQueueDeps(&entry.deps);
	entry.raw = search(start_mapnode, end_mapnode, start_item, end_item, serviceRoute);
	MapSearchNode::recordQueueDeps(NULL);
	if(cache.size() >= kMaxCachedRoutes) cache.clear();
	Route result = materialise(entry.raw, start_item, end_item);
	cache[key] = std::move(entry);
	return result;
}

const PathFinder::Reach &PathFinder::reach(const MapNode *start_mapnode, Item::Item *start_item, bool serviceRoute) {
	MapSearchNode nodeStart(start_mapnode);
	nodeStart.parent_item = start_item;
	nodeStart.start_point = MapNode::Point(start_item->position.x + start_item->size.x/2, start_mapnode->position.y);
	nodeStart.end_point = MapNode::Point(0, INT_MIN);
	nodeStart.serviceRoute = serviceRoute;
	ReachKey key = { start_item, start_mapnode, NULL, NULL, serviceRoute };
	if(!start_mapnode->floorNode) {
		// Floor-node start: only the two first-expansion seeds depend on the start item.
		key.start = NULL;
		nodeStart.firstExpansionSeeds(key.left, key.right);
	}
	std::unordered_map<ReachKey, Reach, ReachKeyHash>::iterator it = reachCache.find(key);
	if(it != reachCache.end()) return it->second;
	if(reachCache.size() >= kMaxCachedRoutes) reachCache.clear();
	Reach &out = reachCache[key];

	// Exhaustive exploration: no goal can match (NULL node) and the goal-floor cut-over
	// never fires (end_point.y = INT_MIN), so the search closes every reachable state.
	MapSearchNode nodeEnd((const MapNode *)NULL);
	nodeEnd.end_point = nodeStart.end_point;
	std::function<void(const MapSearchNode &)> observer = [&out](const MapSearchNode &s) {
		if(out.nodes.insert(s.mapNode).second) out.floors.insert(s.mapNode->position.y);
	};
	astarsearch->SetExpandObserver(&observer);
	astarsearch->SetStartAndGoalStates(nodeStart, nodeEnd);
	do {
		SearchState = astarsearch->SearchStep();
	} while(SearchState == IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_SEARCHING);
	astarsearch->SetExpandObserver(NULL);
	// Exhausted (not out of memory) and small enough that the real search, which only
	// ever holds a subset of these states (twice over at worst), cannot run dry either.
	out.trusted = SearchState == IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_FAILED
		&& astarsearch->GetPeakNodeCount() * 2 + 16 <= kMaxSearchNodes;
	clear();
	return out;
}

bool PathFinder::routeExists(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute, unsigned graphVersion, bool cacheable) {
	if(cacheable && start_item && end_item) {
		if(graphVersion != cacheVersion) {
			cache.clear();
			reachCache.clear();
			cacheVersion = graphVersion;
		}
		const Reach &r = reach(start_mapnode, start_item, serviceRoute);
		if(r.trusted) {
			stats.existenceHits++;
			return end_mapnode->floorNode ? r.nodes.count(end_mapnode) != 0 : r.floors.count(end_mapnode->position.y) != 0;
		}
	}
	return !findRoute(start_mapnode, end_mapnode, start_item, end_item, serviceRoute, graphVersion, cacheable).empty();
}

Route PathFinder::materialise(const Raw &raw, Item::Item *start_item, Item::Item *end_item) {
	Route r;
	if(!raw.found || !start_item || !end_item) return r;
	for(std::vector<Route::Node>::const_iterator n = raw.prefix.begin(); n != raw.prefix.end(); ++n) r.add(n->item, n->toFloor);
	r.add(end_item, raw.endFloor);
	r.updateScore(raw.score);
	return r;
}

PathFinder::Raw PathFinder::search(const MapNode *start_mapnode, const MapNode *end_mapnode, Item::Item *start_item, Item::Item *end_item, bool serviceRoute) {
	MapNode::Point start_point(start_item->position.x + start_item->size.x/2, start_mapnode->position.y);
	MapNode::Point end_point(end_item->position.x + end_item->size.x/2, end_mapnode->position.y);

	MapSearchNode nodeStart(start_mapnode);
	nodeStart.parent_item = start_item;
	nodeStart.start_point = start_point;
	nodeStart.end_point = end_point;
	nodeStart.serviceRoute = serviceRoute;
	MapSearchNode nodeEnd(end_mapnode);
	astarsearch->SetStartAndGoalStates(nodeStart, nodeEnd);

	unsigned int SearchSteps = 0;
	do {
		SearchState = astarsearch->SearchStep();
		SearchSteps++;
	} while(SearchState == IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_SEARCHING);

	Raw raw;
	const int peak = astarsearch->GetPeakNodeCount();
	const bool oom = SearchState == IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_OUT_OF_MEMORY;
	buildRaw(raw, start_item, end_item);
	raw.peakNodes = peak;
	raw.oom = oom;
	clear();

	return raw;
}


void PathFinder::clear() {
	if(SearchState == IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_SUCCEEDED) astarsearch->FreeSolutionNodes();
	if(SearchState == IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_OUT_OF_MEMORY)
		astarsearch.reset(new IndexedAStarSearch< MapSearchNode >(kMaxSearchNodes));
	astarsearch->EnsureMemoryFreed();
	SearchState = IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_NOT_INITIALISED;
}

void PathFinder::buildRaw(Raw &raw, Item::Item *start_item, Item::Item *end_item) {
	raw = Raw();
	if(SearchState != IndexedAStarSearch< MapSearchNode >::SEARCH_STATE_SUCCEEDED) return;
	if(!start_item || !end_item) return;

	MapSearchNode *end_node = astarsearch->GetSolutionEnd();
	MapSearchNode *start_node = astarsearch->GetSolutionStart();
	std::vector<Route::Node> &nodes = raw.prefix;
	struct Push {
		static void add(std::vector<Route::Node> &v, Item::Item *item, int floor) {
			Route::Node n;
			n.item = item;
			n.toFloor = floor;
			v.push_back(n);
		}
	};

	// IsGoal() matches on the map node alone, so a search whose start node
	// already sits on the goal's map node succeeds with a one-node solution
	// even though IsSameState() differs (the start carries parent_item).
	// That solution has no child links; walking it would dereference null.
	if(start_node->IsSameState(*end_node) || start_node->mapNode == end_node->mapNode) {
		Push::add(nodes, start_item, start_node->mapNode->position.y);
	}

	else {
		MapSearchNode *n = start_node;
		MapSearchNode *n_child;
		Push::add(nodes, start_item, n->mapNode->position.y);
		n = astarsearch->GetSolutionNext();
		while(n && n != end_node) {
			n_child = astarsearch->GetSolutionNext();
			if(!n_child) break;
			Item::Item *i = n_child->parent_item;
			if(i) {
				int toFloor;
				// A multi-story lobby is a walkable atrium: its route node must
				// keep the hop's destination floor so journeys can cross the
				// lobby's extension floors (the lobby itself advances the
				// pedestrian). Other non-hauling items keep the legacy floor.
				if(i->canHaulPeople() || i->isLobby()) toFloor = n_child->mapNode->position.y;
				else				                   toFloor = n->mapNode->position.y;

				Route::Node &rn_prev = nodes.back();
				if(i->isElevator() && rn_prev.item->isElevator() && i == rn_prev.item) {
					// Moving along same elevator
					rn_prev.toFloor = toFloor;
				} else Push::add(nodes, i, toFloor);
			}

			n = n_child;
		};
	}
	raw.endFloor = end_node->mapNode->position.y;
	raw.score = (int)std::abs(end_node->g + end_node->h);
	raw.found = true;
}
