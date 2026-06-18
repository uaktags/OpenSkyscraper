#include "Game.h"
#include "Item/Item.h"
#include "Item/Hotel.h"
#include "Item/Parking.h"
#include "JudgeSystem.h"
#include "Person.h"

#include <algorithm>
#include <cmath>

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
			if (hotel && hotel->roomState == Item::Hotel::kDirty)
				++lastCounts.hotelsDirty;
		}
		else if (id == "fastfood" || id == "restaurant") ++lastCounts.foodOutlets;
		else if (id == "security")                       ++lastCounts.securityOffices;
		else if (id == "medicalcenter")                  ++lastCounts.medicalCenters;
		else if (id == "metro")                          ++lastCounts.metros;
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

		// Mirror the item score onto each person currently at the item, so
		// the inspector and per-person overlays have something to show.
		for (Item::Item::People::iterator p = item->people.begin(); p != item->people.end(); ++p)
			(*p)->eval = score;
	}

	LOG(DEBUG, "JudgeSystem pass: o=%i c=%i h=%i (dirty=%i) food=%i sec=%i med=%i pop=%i cov=%.2f",
	    lastCounts.offices, lastCounts.condos, lastCounts.hotels, lastCounts.hotelsDirty,
	    lastCounts.foodOutlets, lastCounts.securityOffices, lastCounts.medicalCenters,
	    lastCounts.population, parkingCoverage);
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
