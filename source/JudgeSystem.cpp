#include "Game.h"
#include "Item/Item.h"
#include "Item/Hotel.h"
#include "Item/Parking.h"
#include "JudgeSystem.h"
#include "Person.h"
#include "YootRules.h"

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
			sum += std::max(0.0, std::min(Person::kMaxStress, (*p)->stress));
		return sum / item->people.size();
	}

	// The EXE's hotel verdict uses a same-floor noisy-neighbour check. A
	// qualifying venue within 20 cells contributes one +60 verdict penalty;
	// vertical/radius falloff is deliberately not part of this rule.
	double computeNoisePenalty(Item::Item * item)
	{
		if (!item || item->prototype->id.find("hotel") == std::string::npos)
			return 0.0;
		const int minX = item->position.x;
		const int maxX = item->position.x + item->size.x;
		const int floor = item->position.y;
		double penalty = 0.0;
		for (Game::ItemSetByInt::const_iterator fit = item->game->itemsByFloor.begin();
		     fit != item->game->itemsByFloor.end(); ++fit)
		{
			if (fit->first != floor) continue;
			for (Game::ItemSet::const_iterator nit = fit->second.begin();
			     nit != fit->second.end(); ++nit)
			{
				Item::Item *neighbor = *nit;
				if (neighbor == item) continue;
				const std::string &id = neighbor->prototype->id;
				const bool loud = id == "restaurant" || id == "shop" ||
					id == "fastfood" || id == "office" || id == "cinema" ||
					id == "partyhall";
				if (!loud) continue;
				const int nMin = neighbor->position.x;
				const int nMax = neighbor->position.x + neighbor->size.x;
				const int gap = nMin >= maxX ? nMin - maxX :
					(minX >= nMax ? minX - nMax : 0);
				if (gap <= 20) penalty += 60.0;
			}
		}
		return std::min(100.0, penalty);
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
	// Capacities mirror the live occupant constructors and venue admission
	// pools; these values are counters, not satisfaction heuristics.
	int populationCapacityFor(const std::string & id)
	{
		if (id == "office")                       return 6;
		if (id == "condo" || id == "yoot_condo") return 10;
		return 0; // hotels counted via Hotel::capacity()
	}
	int visitorCapacityFor(const std::string & id)
	{
		// Commercial admission pools are bounded per venue type.
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
	score -= averageStress(item) * 0.1;
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

	// Noise is an EXE hotel verdict rule, not a condo radius/falloff model.

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
		// Same-floor loud neighbours within 20 cells subtract one +60 verdict.
		score -= computeNoisePenalty(item);
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

int JudgeSystem::yootTrafficRange(Game * game, Item::Item * item)
{
	// OpenSky has no per-cell traffic counter (T2TrafficInfo, decayed hourly
	// people passes bucketed into ranges 0..3). Stand-in: people queued at
	// or riding a mover that serves this floor within 8 cells of the
	// tenant, one range step per 5 people.
	const int floor = item->position.y;
	int count = 0;
	for (Game::ItemSet::const_iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item *mover = *it;
		if (!mover->canHaulPeople() || mover->people.empty()) continue;
		if (floor < mover->position.y || floor >= mover->position.y + mover->size.y) continue;
		const int gap = mover->position.x >= item->position.x + item->size.x
			? mover->position.x - (item->position.x + item->size.x)
			: (item->position.x >= mover->position.x + mover->size.x
				? item->position.x - (mover->position.x + mover->size.x) : 0);
		if (gap > 8) continue;
		for (Item::Item::People::const_iterator p = mover->people.begin(); p != mover->people.end(); ++p)
			if ((*p)->journey.fromFloor == floor || (*p)->journey.toFloor == floor) ++count;
	}
	return std::min(3, count / 5);
}

int JudgeSystem::yootEstimate(Game * game, Item::Item * item)
{
	const YootItemStats *stats = item->prototype->yoot;
	if (!stats) return 0;
	const std::string & id = item->prototype->id;
	const bool residential = id == "office" || id == "condo" || id == "yoot_condo" ||
		id == "hotel_single" || id == "hotel_double" || id == "hotel_suite";
	if (!residential)
		return item->yootEstimate; // commercial: set at close (CalcProfitEstimate)

	// T2Tenant::UpdateResidencialEstimate 0x10060e20.
	Yoot::ResidentialInputs in;
	for (Item::Item::People::const_iterator p = item->people.begin(); p != item->people.end(); ++p)
		in.peopleEstimates.push_back((*p)->yootCurrentEstimate());
	in.previousPeopleEstimate = Yoot::kPeopleEstimateStart;
	in.senseOfNoise = stats->senseOfNoise;
	in.trafficRange = in.senseOfNoise ? yootTrafficRange(game, item) : 0;
	in.priceIndex = Yoot::priceIndex(game->yootPriceSelection(item), (int)stats->inMoney.size());
	in.yearsSinceBuilt = 0; // OpenSky items carry no build date
	return Yoot::residentialEstimate(in);
}

void JudgeSystem::evaluateAll(Game * game)
{
	const int day = static_cast<int>(std::floor(game->time.absolute));
	const int oldDay = lastCounts.visitorDay;
	const int oldDailyArrivals = lastCounts.dailyArrivals;
	const int oldDailyHotel = lastCounts.dailyHotelArrivals;
	const int oldDailyFood = lastCounts.dailyFoodArrivals;
	const int oldDailyShop = lastCounts.dailyShopArrivals;
	const int oldDailyLeisure = lastCounts.dailyLeisureArrivals;
	lastCounts = Counts();
	lastCounts.visitorDay = day;
	if (oldDay == day)
	{
		lastCounts.dailyArrivals = oldDailyArrivals;
		lastCounts.dailyHotelArrivals = oldDailyHotel;
		lastCounts.dailyFoodArrivals = oldDailyFood;
		lastCounts.dailyShopArrivals = oldDailyShop;
		lastCounts.dailyLeisureArrivals = oldDailyLeisure;
	}

	// Pass 1: count facilities, live capacities, and current visitor pools.
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item *item = *it;
		const std::string &id = item->prototype->id;
		if (id == "office")                              ++lastCounts.offices;
		else if (id == "condo" || id == "yoot_condo")    ++lastCounts.condos;
		else if (id == "hotel_single" || id == "hotel_double" ||
		         id == "hotel_suite" || id == "hotel")
		{
			++lastCounts.hotels;
			Item::Hotel *hotel = dynamic_cast<Item::Hotel *>(item);
			if (hotel)
			{
				if (hotel->roomState == Item::Hotel::kDirty) ++lastCounts.hotelsDirty;
				else if (hotel->roomState == Item::Hotel::kOccupied) ++lastCounts.hotelsOccupied;
				lastCounts.visitorCapacity += hotel->capacity();
			}
		}
		else if (id == "fastfood" || id == "restaurant") ++lastCounts.foodOutlets;
		else if (id == "security")                       ++lastCounts.securityOffices;
		else if (id == "medicalcenter")                  ++lastCounts.medicalCenters;
		else if (id == "metro")                          ++lastCounts.metros;

		lastCounts.populationCapacity += populationCapacityFor(id);
		lastCounts.visitorCapacity    += visitorCapacityFor(id);
		const int occupants = static_cast<int>(item->people.size());
		lastCounts.currentOccupants += occupants;
		const bool hotel = id == "hotel_single" || id == "hotel_double" ||
			id == "hotel_suite" || id == "hotel";
		const bool food = id == "fastfood" || id == "restaurant";
		const bool shop = id == "shop";
		const bool leisure = id == "cinema" || id == "partyhall";
		if (hotel || food || shop || leisure)
		{
			lastCounts.visitorPool += occupants;
			if (hotel)   lastCounts.hotelVisitorPool += occupants;
			if (food)    lastCounts.foodVisitorPool += occupants;
			if (shop)    lastCounts.shopVisitorPool += occupants;
			if (leisure) lastCounts.leisureVisitorPool += occupants;
		}
	}
	lastCounts.population = game->population;

	// Daily visitor passes are idempotent across multiple judge invocations.
	for (Game::PersonSet::const_iterator p = game->people.begin(); p != game->people.end(); ++p)
	{
		Person *person = *p;
		Item::Item *at = person->at;
		if (!at) continue;
		const std::string &id = at->prototype->id;
		const bool visitor = id == "hotel_single" || id == "hotel_double" ||
			id == "hotel_suite" || id == "hotel" || id == "fastfood" ||
			id == "restaurant" || id == "shop" || id == "cinema" ||
			id == "partyhall";
		std::map<Person *, int>::iterator seen = visitorDaySeen.find(person);
		if (seen != visitorDaySeen.end() && seen->second == day) continue;
		visitorDaySeen[person] = day;
		++lastCounts.dailyArrivals;
		if (id == "hotel_single" || id == "hotel_double" ||
		    id == "hotel_suite" || id == "hotel")
			++lastCounts.dailyHotelArrivals;
		else if (id == "fastfood" || id == "restaurant")
			++lastCounts.dailyFoodArrivals;
		else if (id == "shop")
			++lastCounts.dailyShopArrivals;
		else if (id == "cinema" || id == "partyhall")
			++lastCounts.dailyLeisureArrivals;
	}
	for (std::map<Person *, int>::iterator p = visitorDaySeen.begin();
	     p != visitorDaySeen.end(); )
	{
		if (game->people.find(p->first) == game->people.end())
			p = visitorDaySeen.erase(p);
		else
			++p;
	}

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

		// YootTower (J1): the 1..300 estimate drives the decisions; the
		// 0..100 evaluation mirrors it for overlays and inspectors.
		const int yoot = game->activeRules.profile().yootWorld ? yootEstimate(game, item) : 0;
		if (yoot > 0)
		{
			item->yootEstimate = yoot;
			score = yoot / 3.0;
		}
		item->evaluation = score;
		if (yoot > 0 ? yoot < Yoot::kEstimateMoveOut : score < 25.0) ++lastCounts.criticalTenants;

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
