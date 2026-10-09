#include "Game.h"
#include "Item/Item.h"
#include "Item/Hotel.h"
#include "Item/Office.h"
#include "Item/Condo.h"
#include "Item/Shop.h"
#include "Item/Restaurant.h"
#include "Item/FastFood.h"
#include "JudgeSystem.h"
#include "Person.h"
#include "TenantRules.h"

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

	bool isHotelId(const std::string & id)
	{
		return id == "hotel_single" || id == "hotel_double" ||
		       id == "hotel_suite" || id == "hotel";
	}

	int slotTypeFor(const std::string & id)
	{
		using namespace TenantRules;
		if (id == "office") return kSlotOffice;
		if (isHotelId(id)) return kSlotHotel;
		if (id == "condo") return kSlotCondo;
		if (id == "restaurant") return kSlotRestaurant;
		if (id == "shop") return kSlotShop;
		if (id == "fastfood") return kSlotFastFood;
		if (id == "cinema") return kSlotCinema;
		if (id == "partyhall") return kSlotParty;
		return kSlotOther;
	}
}

JudgeSystem::JudgeSystem()
{
	lastCounts = Counts();
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

	// Stressed tenants (category 0) feed LevelUp and the daily complaint.
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		const std::string & id = item->prototype->id;
		if (!item->hasTenantRecord() || id == "restaurant" || id == "fastfood") continue;
		if (item->tenant.category == TenantRules::kStressed) ++lastCounts.criticalTenants;
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

int JudgeSystem::tenantMetric(Item::Item * item)
{
	using namespace TenantRules;
	if (!item || !item->hasTenantRecord()) return -1;
	const std::string & id = item->prototype->id;
	Kind kind;
	if (id == "office")
	{
		if (!item->isOccupied() && item->tenant.armed) return -1;
		kind = kOffice;
	}
	else if (id == "condo")
	{
		if (!item->isOccupied() && item->tenant.armed) return -1;
		kind = kCondo;
	}
	else if (isHotelId(id))
	{
		Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
		if (hotel && hotel->roomState >= Item::Hotel::kInfested) return -1;
		kind = id == "hotel_double" ? kTwin : id == "hotel_suite" ? kSuite : kSingle;
	}
	else return -1;
	const int raw = rawMetric(item->tenant.bank, memberCount(kind), divisorFor(kind));
	return adjustMetric(raw, item->tenant.rentClass, noiseSource(item) != nullptr);
}

Item::Item * JudgeSystem::noiseSource(Item::Item * item)
{
	if (!item || !item->game) return nullptr;
	std::vector<Item::Item *> order = item->game->slotOrder(item->position.y);
	std::vector<TenantRules::Slot> slots;
	int victim = -1;
	for (size_t i = 0; i < order.size(); ++i)
	{
		Item::Item * o = order[i];
		TenantRules::Slot s;
		s.left = o->position.x;
		s.right = o->position.x + o->size.x;
		s.type = slotTypeFor(o->prototype->id);
		slots.push_back(s);
		if (o == item) victim = (int)i;
	}
	const int idx = TenantRules::noiseSourceIndex(slots.data(), (int)slots.size(), victim);
	return idx < 0 ? nullptr : order[idx];
}

int JudgeSystem::judgeOne(Item::Item * item)
{
	using namespace TenantRules;
	if (!item || !item->hasTenantRecord()) return kUnjudged;
	const std::string & id = item->prototype->id;
	int category;
	if (id == "restaurant")
	{
		Item::Restaurant * r = dynamic_cast<Item::Restaurant *>(item);
		category = foodCategory(r ? r->customersToday : 0);
		item->tenant.category = (uint8_t)category;
		return category;
	}
	if (id == "fastfood")
	{
		Item::FastFood * f = dynamic_cast<Item::FastFood *>(item);
		category = foodCategory(f ? f->customersToday : 0);
		item->tenant.category = (uint8_t)category;
		return category;
	}
	if (id == "shop")
	{
		Item::Shop * shop = dynamic_cast<Item::Shop *>(item);
		category = shop ? shopCategory(shop->let, shop->quotaRemaining + shop->customersToday,
		                               item->tenant.rentClass) : (int)kUnjudged;
	}
	else
	{
		const int metric = tenantMetric(item);
		category = categoryFor(metric, item->game ? item->game->rating : 0);
	}
	item->tenant.category = (uint8_t)category;
	item->refreshEvaluation();
	return category;
}

void JudgeSystem::rescueOne(Item::Item * item)
{
	if (!item || !item->hasTenantRecord()) return;
	judgeOne(item);
	bool vacant = false;
	if (Item::Shop * shop = dynamic_cast<Item::Shop *>(item)) vacant = !shop->let;
	else vacant = !item->isOccupied();
	if (vacant && !item->tenant.armed && item->tenant.category != TenantRules::kStressed)
		item->tenant.armed = true;
}

bool JudgeSystem::moveOut(Game * game, Item::Item * item,
                          const std::vector<Item::Item *> & floorTenants)
{
	(void)game;
	const std::string & id = item->prototype->id;
	Item::Item::TenantRecord & t = item->tenant;
	bool leave = false;
	if (id == "office" || id == "condo")
		leave = item->isOccupied() && t.category == TenantRules::kStressed;
	else if (id == "shop")
	{
		Item::Shop * shop = dynamic_cast<Item::Shop *>(item);
		leave = shop && shop->let && t.category == TenantRules::kStressed && t.tenure != 0;
	}
	if (!leave) return false;

	t.armed = false; t.tenure = 0; t.redraw = true;
	if (Item::Office * office = dynamic_cast<Item::Office *>(item)) office->vacate();
	else if (Item::Condo * condo = dynamic_cast<Item::Condo *>(item)) condo->vacate();
	else if (Item::Shop * shop = dynamic_cast<Item::Shop *>(item)) shop->vacate();

	// Rescue pairing: first same-prototype neighbour judged happy.
	for (size_t i = 0; i < floorTenants.size(); ++i)
	{
		Item::Item * other = floorTenants[i];
		if (other == item || other->prototype->id != id) continue;
		if (other->tenant.category != TenantRules::kHappy) continue;
		other->tenant.category = TenantRules::kOk;
		other->refreshEvaluation();
		t.category = TenantRules::kOk;
		t.armed = true;
		item->resetMemberBanks();
		break;
	}
	item->refreshEvaluation();
	return true;
}

void JudgeSystem::judgeDaily(Game * game, bool isQuarterDay)
{
	const bool quarter = isQuarterDay;
	evaluateAll(game);

	std::vector<int> floors;
	for (Game::ItemSetByInt::const_iterator f = game->itemsByFloor.begin();
	     f != game->itemsByFloor.end(); ++f)
		floors.push_back(f->first);

	for (size_t fi = 0; fi < floors.size(); ++fi)
	{
		std::vector<Item::Item *> order = game->slotOrder(floors[fi]);
		std::vector<Item::Item *> tenants;
		for (size_t i = 0; i < order.size(); ++i)
		{
			Item::Item * item = order[i];
			if (item->underConstruction || !item->hasTenantRecord()) continue;
			if (isHotelId(item->prototype->id)) continue; // 17:00 hotel pass
			tenants.push_back(item);
		}
		for (size_t i = 0; i < tenants.size(); ++i) judgeOne(tenants[i]);

		if (quarter)
		{
			std::vector<bool> left(tenants.size(), false);
			for (size_t i = 0; i < tenants.size(); ++i)
				left[i] = moveOut(game, tenants[i], tenants);
			for (size_t i = 0; i < tenants.size(); ++i)
			{
				Item::Item * item = tenants[i];
				const std::string & id = item->prototype->id;
				if (left[i] || id == "restaurant" || id == "fastfood") continue;
				bool tenanted = item->isOccupied();
				if (Item::Shop * shop = dynamic_cast<Item::Shop *>(item)) tenanted = shop->let;
				if (!tenanted) continue;
				item->tenant.tenure = (uint8_t)std::min(120, (int)item->tenant.tenure + 1);
				item->resetMemberBanks();
			}
		}
		else
		{
			for (size_t i = 0; i < tenants.size(); ++i)
			{
				Item::Item * item = tenants[i];
				const std::string & id = item->prototype->id;
				if (id == "restaurant" || id == "fastfood") continue;
				if (!item->tenant.armed && item->tenant.category != TenantRules::kStressed)
					item->tenant.armed = true; // plain byte test: 255 arms too
			}
		}
	}

	if (lastCounts.criticalTenants > 0)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "%d tenant%s unhappy - check the Evaluation view (O)",
		         lastCounts.criticalTenants,
		         lastCounts.criticalTenants == 1 ? " is" : "s are");
		game->timeWindow.showMessage(buf);
	}
	reviewHotels(game);
}
