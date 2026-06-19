#include "Game.h"
#include "Item/Item.h"
#include "Item/Hotel.h"
#include "Item/Parking.h"
#include "JudgeSystem.h"
#include "Person.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

using namespace OT;

namespace
{
	// Convenience iterators over a Game's typed item index. Returning a const
	// reference avoids copying the set when the type bucket is empty (operator[]
	// default-constructs an empty set on first access).
	const Game::ItemSet & bucket(const Game * game, const char * id)
	{
		static const Game::ItemSet empty;
		const Game::ItemSetByString & byType = game->itemsByType;
		Game::ItemSetByString::const_iterator it = byType.find(id);
		return (it == byType.end()) ? empty : it->second;
	}

	double averageStress(Item::Item * item)
	{
		if (item->people.empty()) return 0;
		double sum = 0;
		for (Item::Item::People::iterator p = item->people.begin(); p != item->people.end(); ++p)
			sum += (*p)->stress;
		return sum / item->people.size();
	}

	// Noise model approximating the original Kinsoku.h/c placement rules.
	struct NoiseProfile { int level; int sensitivity; };
	NoiseProfile noiseProfileFor(const std::string & id)
	{
		// Loud tenants - offices and commercial activity.
		if (id == "office"   || id == "fastfood"   || id == "restaurant" ||
		    id == "cinema"   || id == "partyhall"  || id == "metro")     return {90,  0};
		// Quiet but noise-sensitive residents.
		if (id == "condo" || id == "yoot_condo")                          return {10, 90};
		if (id == "hotel_single" || id == "hotel_double" ||
		    id == "hotel_suite"  || id == "hotel")                        return {10, 70};
		return {0, 0};
	}

	// Isolation radius in tiles, per the SimTower Notes
	// (120px / 8px = 15 tiles for hotels, 240px / 8px = 30 tiles for condos).
	int noiseRadiusTilesFor(const std::string & id)
	{
		if (id == "condo" || id == "yoot_condo")              return 30;
		if (id.find("hotel") != std::string::npos)            return 15;
		return 0;
	}

	// Sum the noise load on a sensitive item from loud neighbours on the same
	// floor and the two adjacent floors. Returns a 0..25 evaluation penalty.
	double computeNoisePenalty(Game * game, Item::Item * item)
	{
		NoiseProfile np = noiseProfileFor(item->prototype->id);
		if (np.sensitivity == 0) return 0;
		int radius = noiseRadiusTilesFor(item->prototype->id);
		if (radius <= 0) return 0;

		double noiseLoad = 0;
		for (int floorDelta = -1; floorDelta <= 1; ++floorDelta)
		{
			int floor = item->position.y + floorDelta;
			Game::ItemSetByInt::const_iterator it = game->itemsByFloor.find(floor);
			if (it == game->itemsByFloor.end()) continue;

			for (Game::ItemSet::const_iterator nit = it->second.begin();
			     nit != it->second.end(); ++nit)
			{
				Item::Item *neighbor = *nit;
				if (neighbor == item) continue;
				NoiseProfile np2 = noiseProfileFor(neighbor->prototype->id);
				if (np2.level == 0) continue;

				// Horizontal gap between the two item rectangles (tiles).
				int myMin = item->position.x;
				int myMax = item->position.x + item->size.x;
				int nMin  = neighbor->position.x;
				int nMax  = neighbor->position.x + neighbor->size.x;
				int dist;
				if (nMin >= myMax)      dist = nMin - myMax;
				else if (myMin >= nMax) dist = myMin - nMax;
				else                    dist = 0; // overlapping
				// Cross-floor noise has to pass through a ceiling, treat that
				// as a few extra tiles of distance.
				if (floorDelta != 0) dist += 5;

				if (dist < radius)
				{
					double weight = 1.0 - static_cast<double>(dist) / radius;
					noiseLoad += np2.level * weight;
				}
			}
		}

		if (noiseLoad <= 0) return 0;
		// Scale so a single neighbouring office at distance 0 (~90 load)
		// subtracts ~18 points from a condo's evaluation.
		if (noiseLoad > 125.0) noiseLoad = 125.0;
		return noiseLoad * 0.2;
	}
}

JudgeSystem::JudgeSystem()
{
	lastCounts = Counts();
	parkingCoverage = 1.0;
}

double JudgeSystem::clampScore(double v)
{
	if (v < 0)   return 0;
	if (v > 100) return 100;
	return v;
}

namespace {
	// Rule-of-thumb capacities used by the granular counters. They don't
	// drive any gameplay directly; they just give the statistics UI and
	// future VIP system a sensible "this tower can hold N people" number.
	int populationCapacityFor(const std::string & id)
	{
		if (id == "office")                       return 3;
		if (id == "condo" || id == "yoot_condo")  return 2;
		return 0; // hotels counted via capacity() below
	}
	int visitorCapacityFor(const std::string & id)
	{
		// Coarse visitor estimates for commercial venues.
		if (id == "fastfood")    return 6;
		if (id == "restaurant")  return 8;
		if (id == "cinema")      return 20;
		if (id == "partyhall")   return 15;
		return 0;
	}
}

/** Base score shared by all tenant types. Rewards being reachable from the
 *  lobby and penalises occupant stress. */
static double baseTenantScore(Item::Item * item)
{
	double score = 50;
	if (!item->lobbyRoute.empty())
	{
		score += 20;
		// Lower route cost (fewer hops / shorter) is better. lobbyRoute.score()
		// is unbounded above, so cap the bonus.
		int s = item->lobbyRoute.score();
		if (s > 0)
			score += std::max(0.0, 10.0 - s * 0.5);
	}
	else
	{
		// Unreachable tenants are deeply unhappy.
		score -= 30;
	}
	score -= averageStress(item) * 0.3;
	return score;
}

double JudgeSystem::scoreOffice(Game * game, Item::Item * item)
{
	double score = baseTenantScore(item);

	// Lunch amenity: office workers need a reachable FastFood for their 12:00
	// break. Without one they accumulate stress and eventually vacate.
	const Game::ItemSet & fastfood = bucket(game, "fastfood");
	bool lunchReachable = false;
	for (Game::ItemSet::const_iterator f = fastfood.begin(); f != fastfood.end(); ++f)
	{
		if (!game->findRoute(item, *f).empty())
		{
			lunchReachable = true;
			break;
		}
	}
	score += lunchReachable ? 10 : -15;

	// Security coverage within a 10-floor window keeps crime-related stress
	// down. Phase 3.3 (Thief) will make this more concrete.
	if (!bucket(game, "security").empty())
		score += 3;

	// Parking: original requires 1 space per 4 offices. Undercoverage taxes
	// the office's evaluation.
	if (parkingCoverage < 0.5)
		score -= 10;
	else if (parkingCoverage < 1.0)
		score -= 5;

	return clampScore(score);
}

double JudgeSystem::scoreCondo(Game * game, Item::Item * item)
{
	double score = baseTenantScore(item);

	// Residents value quiet: a tower with no security presence tends to mean
	// the player has not invested in residential infrastructure yet.
	if (!bucket(game, "security").empty())
		score += 3;

	// Reachable food is a minor plus for residents (dinner / convenience).
	const Game::ItemSet & restaurants = bucket(game, "restaurant");
	bool foodReachable = !restaurants.empty() &&
		std::any_of(restaurants.begin(), restaurants.end(),
			[&](Item::Item * r) { return !game->findRoute(item, r).empty(); });
	score += foodReachable ? 3 : 0;

	// Noise penalty - condos are the most noise-sensitive tenant.
	score -= computeNoisePenalty(game, item);

	return clampScore(score);
}

double JudgeSystem::scoreHotel(Game * game, Item::Item * item)
{
	double score = baseTenantScore(item);

	// Hotel-specific: dirty rooms tank the score. Use the room state directly
	// since Hotel exposes it as a public field.
	Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
	if (hotel)
	{
		if (hotel->roomState == Item::Hotel::kDirty)
			score -= 30;
		else if (hotel->roomState == Item::Hotel::kOccupied)
			score += 5;

		// Guests need a reachable restaurant for dinner, otherwise they leave
		// the tower which costs the player the dinner revenue.
		const Game::ItemSet & restaurants = bucket(game, "restaurant");
		bool dinnerReachable = !restaurants.empty() &&
			std::any_of(restaurants.begin(), restaurants.end(),
				[&](Item::Item * r) { return !game->findRoute(item, r).empty(); });
		score += dinnerReachable ? 8 : -8;

		// Parking: original requires 1 space per hotel suite (treat any hotel
		// room as needing 1 space for MVP).
		if (parkingCoverage < 0.5)
			score -= 12;
		else if (parkingCoverage < 1.0)
			score -= 6;

		// Noise penalty - hotels are noise-sensitive but less so than condos.
		score -= computeNoisePenalty(game, item) * 0.7;
	}

	return clampScore(score);
}

double JudgeSystem::scoreCommercial(Game * game, Item::Item * item)
{
	double score = baseTenantScore(item);

	// Commercial venues are happier when reachable from the lobby (footfall).
	// baseTenantScore already rewards this; add a small steady bonus so a
	// well-placed venue scores notably above residential.
	if (!item->lobbyRoute.empty())
		score += 5;

	return clampScore(score);
}

void JudgeSystem::evaluateAll(Game * game)
{
	// Reset tallies.
	lastCounts = Counts();

	// Pass 1: cheap counting pass so derived metrics (parking coverage) are
	// available before the per-item scorers run.
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		const std::string & id = (*it)->prototype->id;
		if (id == "office")                              ++lastCounts.offices;
		else if (id == "condo" || id == "yoot_condo")    ++lastCounts.condos;
		else if (id == "hotel_single" || id == "hotel_double" ||
		         id == "hotel_suite" || id == "hotel")
		{
			++lastCounts.hotels;
			Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(*it);
			if (hotel)
			{
				if (hotel->roomState == Item::Hotel::kDirty)    ++lastCounts.hotelsDirty;
				else if (hotel->roomState == Item::Hotel::kOccupied) ++lastCounts.hotelsOccupied;
				lastCounts.visitorCapacity += hotel->capacity();
			}
		}
		else if (id == "fastfood" || id == "restaurant") ++lastCounts.foodOutlets;
		else if (id == "security")                       ++lastCounts.securityOffices;
		else if (id == "medicalcenter")                  ++lastCounts.medicalCenters;
		else if (id == "metro")                          ++lastCounts.metros;

		// Granular capacity / occupancy tallies. These are reported
		// alongside the basic Counts above so future UI/VIP systems can
		// read a single struct instead of rescanning the world.
		lastCounts.populationCapacity += populationCapacityFor(id);
		lastCounts.visitorCapacity    += visitorCapacityFor(id);
		lastCounts.currentOccupants   += static_cast<int>((*it)->people.size());
	}
	lastCounts.population = game->population;

	// Tower-wide parking coverage, cached for the per-item scorers below.
	computeParkingCoverage(game);

	// Pass 2: walk every item, dispatching to the appropriate scorer.
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		const std::string & id = item->prototype->id;
		double score = 50; // neutral default for unhandled types

		if (id == "office")
		{
			score = scoreOffice(game, item);
		}
		else if (id == "condo" || id == "yoot_condo")
		{
			score = scoreCondo(game, item);
		}
		else if (id == "hotel_single" || id == "hotel_double" || id == "hotel_suite" || id == "hotel")
		{
			score = scoreHotel(game, item);
		}
		else if (id == "fastfood" || id == "restaurant" ||
		         id == "cinema"    || id == "partyhall")
		{
			score = scoreCommercial(game, item);
		}

		item->evaluation = score;
		if (score < 25.0) ++lastCounts.criticalTenants;

		// Mirror the item score onto each person currently at the item, so
		// the inspector and per-person overlays have something to show.
		for (Item::Item::People::iterator p = item->people.begin(); p != item->people.end(); ++p)
			(*p)->eval = score;
	}

	LOG(DEBUG, "JudgeSystem pass: o=%i c=%i h=%i (dirty=%i occ=%i) food=%i sec=%i med=%i pop=%i cap=%i/%i cov=%.2f",
	    lastCounts.offices, lastCounts.condos, lastCounts.hotels, lastCounts.hotelsDirty,
	    lastCounts.hotelsOccupied, lastCounts.foodOutlets, lastCounts.securityOffices,
	    lastCounts.medicalCenters, lastCounts.population, lastCounts.populationCapacity,
	    lastCounts.visitorCapacity, parkingCoverage);

	// Aggregate hotel review (JudgeAllHotel equivalent) and per-tenant
	// bad-day tracking (ExpandoBadHotel equivalent). Each may surface a
	// message via timeWindow.showMessage.
	reviewHotels(game);
	reviewUnderperformers(game);

	// Surface a daily complaint if any tenant is critically unhappy. The
	// original SimTower does this via InfoDlog/TenantInfo messages.
	if (lastCounts.criticalTenants > 0)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "%d tenant%s unhappy - check the Evaluation view (O)",
		         lastCounts.criticalTenants,
		         lastCounts.criticalTenants == 1 ? " is" : "s are");
		game->timeWindow.showMessage(buf);
	}
}

bool JudgeSystem::reviewHotels(Game * game)
{
	// Aggregate review across all hotel items. Updates hotelAvgEval and
	// emits a daily complaint when hotels are persistently underperforming
	// (low average evaluation OR persistent dirty-room ratio). This is
	// the JudgeAllHotel equivalent from the Yoot source.
	lastCounts.hotelAvgEval = 0.0;
	if (lastCounts.hotels <= 0) return false;

	double sumEval = 0;
	int counted = 0;
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(*it);
		if (!hotel) continue;
		sumEval += hotel->evaluation;
		++counted;
	}
	if (counted == 0) return false;
	lastCounts.hotelAvgEval = sumEval / counted;

	const double dirtyRatio = static_cast<double>(lastCounts.hotelsDirty) / lastCounts.hotels;
	const double occRatio   = static_cast<double>(lastCounts.hotelsOccupied) / lastCounts.hotels;

	// Complaint thresholds: very low average evaluation, or a tower where
	// most hotel rooms are dirty (housekeeping can't keep up). Phrased so
	// we only nag when something is concretely wrong.
	if (lastCounts.hotelAvgEval < 35.0)
	{
		char buf[160];
		snprintf(buf, sizeof(buf),
		         "Hotels struggling (avg eval %.0f) - check routes, restaurants & parking",
		         lastCounts.hotelAvgEval);
		game->timeWindow.showMessage(buf);
		return true;
	}
	if (dirtyRatio > 0.5 && lastCounts.hotels >= 3)
	{
		char buf[128];
		snprintf(buf, sizeof(buf),
		         "%d of %d hotel rooms need housekeeping",
		         lastCounts.hotelsDirty, lastCounts.hotels);
		game->timeWindow.showMessage(buf);
		return true;
	}
	// Positive note when running near capacity - helps the player understand
	// their hotel is doing well (mirrors original VIP/evaluation feedback).
	if (occRatio > 0.8 && lastCounts.hotelsOccupied >= 4)
	{
		game->timeWindow.showMessage("Hotel occupancy high - consider building more rooms");
	}
	return false;
}

void JudgeSystem::reviewUnderperformers(Game * game)
{
	// ExpandoBadHotel equivalent. The original dynamically grew/shrank
	// hotels; our items have fixed footprints after construction, so we
	// instead track a per-tenant "bad-day streak" and surface targeted
	// complaints only when a tenant has been unhappy for several days.
	// This avoids spamming the player on a single transient dip.
	const double kBadThreshold = 25.0;
	const int    kWarnAfterDays = 3;

	// Build the current set of underperformers so we can prune stale
	// entries below.
	std::set<Item::Item *> stillBad;
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		const std::string & id = item->prototype->id;
		// Only track actual tenant types (skip elevators, floors, lobbies).
		bool isTenant = (id == "office"   || id == "condo"    || id == "yoot_condo" ||
		                 id == "hotel_single" || id == "hotel_double" ||
		                 id == "hotel_suite"  || id == "hotel" ||
		                 id == "fastfood" || id == "restaurant" ||
		                 id == "cinema"   || id == "partyhall");
		if (!isTenant) continue;

		if (item->evaluation < kBadThreshold)
		{
			stillBad.insert(item);
			int & streak = badDayStreak[item];
			++streak;
			// Warn once when the tenant crosses the persistence threshold,
			// and again every 5 days after so the player is reminded.
			if (streak == kWarnAfterDays || (streak > kWarnAfterDays && (streak - kWarnAfterDays) % 5 == 0))
			{
				char buf[160];
				snprintf(buf, sizeof(buf), "%s on floor %i has been unhappy for %i day%s",
				         item->prototype->name.c_str(),
				         item->position.y,
				         streak,
				         streak == 1 ? "" : "s");
				game->timeWindow.showMessage(buf);
			}
		}
	}

	// Decay/clear streaks for items that recovered, and prune entries for
	// items that have been removed from the world.
	for (std::map<Item::Item *, int>::iterator it = badDayStreak.begin();
	     it != badDayStreak.end(); )
	{
		if (stillBad.find(it->first) == stillBad.end())
			it = badDayStreak.erase(it);
		else
			++it;
	}
}

void JudgeSystem::computeParkingCoverage(Game * game)
{
	// Sum capacity across all parking items.
	int totalSpaces = 0;
	const Game::ItemSet & parking = bucket(game, "parking");
	for (Game::ItemSet::const_iterator p = parking.begin(); p != parking.end(); ++p)
	{
		Item::Parking * park = dynamic_cast<Item::Parking *>(*p);
		if (park) totalSpaces += park->totalSpaces();
	}

	// Original SimTower rule of thumb: 1 space per 4 offices and 1 per hotel
	// room. Condos are owner-occupied and assumed to have their own garage.
	int required = (lastCounts.offices + 3) / 4 + lastCounts.hotels;

	parkingCoverage = (required == 0) ? 1.0 : static_cast<double>(totalSpaces) / required;
}
