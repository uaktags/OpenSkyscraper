/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootRules.h"
#include "YootCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace OT
{
namespace Yoot
{
	long long absoluteMinute(double absolute)
	{
		// Round to the nearest 1/4 minute first so values that are exact
		// minutes in decimal (7:00 = 7/24 day) do not land one minute early.
		const double quarters = std::floor(absolute * kMinutesPerDay * 4.0 + 1e-6);
		return (long long)std::floor(quarters / 4.0);
	}

	bool opensQuarter(long long day)
	{
		return ((day % 3) + 3) % 3 == 0;
	}

	int maintenanceCost(const YootItemStats & stats, int widthUnits, int cars)
	{
		if (!stats.paysQuarterly())
			return 0;
		if (stats.mover)
		{
			// T2MoverDef::CalcMentenanceCostProc: GetOutMoney(0) x modules,
			// halving the module count for floor-type movers.
			int modules = std::max(0, cars);
			if (stats.attribute & 0x2000u)
				modules /= 2;
			return modules * stats.outMoney;
		}
		if (stats.maintenancePerUnit())
			return std::max(0, widthUnits) * stats.outMoney;
		return stats.outMoney;
	}

	bool officeRentWindow(long long day, int minuteOfDay)
	{
		return opensQuarter(day) && minuteOfDay >= kOfficeRentFirstMinute &&
		       minuteOfDay <= kOfficeRentLastMinute;
	}

	int officeQuarterRent(const YootItemStats & stats, int selection)
	{
		return 3 * stats.inMoneyFor(selection);
	}

	int priceIndex(int selection, int numInMoney)
	{
		if (numInMoney <= 1) return 0;
		return std::max(0, std::min(numInMoney - 1, selection)) + 1;
	}

	int selectionFromRentClass(int rentClass, int numInMoney)
	{
		if (numInMoney <= 0) return 0;
		rentClass = std::max(0, std::min(3, rentClass));
		return std::max(0, std::min(numInMoney - 1, 3 - rentClass));
	}

	int calcInMoneyUnits(int customers, int inMoneyUnits, int outMoneyUnits)
	{
		return (inMoneyUnits * customers) / 100 - outMoneyUnits;
	}

	int commercialIncome(int customers, const YootItemStats & stats)
	{
		const int in = stats.inMoneyFor(0) / kYootMoneyUnit;
		const int out = stats.outMoney / kYootMoneyUnit;
		return calcInMoneyUnits(customers, in, out) * kYootMoneyUnit;
	}

	int profitEstimate(int customers, int inMoneyUnits, int outMoneyUnits)
	{
		const int income = calcInMoneyUnits(customers, inMoneyUnits, outMoneyUnits);
		int e = (income + outMoneyUnits) * 70;
		if (e < 0) e = 0;
		if (outMoneyUnits != 0) e /= outMoneyUnits;
		return clampEstimate(e);
	}

	int hotelRoomIncome(const YootItemStats & stats, int selection)
	{
		return stats.inMoneyFor(selection);
	}

	int clampEstimate(int estimate)
	{
		return std::max(kEstimateMin, std::min(kEstimateMax, estimate));
	}

	int estimateColor(int estimate)
	{
		if (estimate == 0) return -1;
		if (estimate > kEstimateGrow) return 12;
		return estimate < kEstimateMoveOut ? 14 : 9;
	}

	int residentialEstimate(const ResidentialInputs & in, int * peopleEstimateOut)
	{
		int people = in.previousPeopleEstimate;
		int e = 1;
		if (in.hasBelongs)
		{
			if (!in.peopleEstimates.empty())
			{
				// Unsigned sum / count in the engine; estimates are >= 1.
				unsigned sum = 0;
				for (size_t i = 0; i < in.peopleEstimates.size(); ++i)
					sum += (unsigned)in.peopleEstimates[i];
				people = (int)(sum / (unsigned)in.peopleEstimates.size());
			}
			e = people;
			if (in.senseOfNoise != 0)
				e -= in.trafficRange * in.senseOfNoise;
			if (in.priceIndex != 0)
				e += (3 - in.priceIndex) * 30;
			e -= in.yearsSinceBuilt;
		}
		if (peopleEstimateOut) *peopleEstimateOut = people;
		return clampEstimate(e);
	}

	int roomNightBonus(int currentEstimate)
	{
		return currentEstimate >= kEstimateMoveOut ? 20 : 5;
	}

	int incPeopleEstimate(int peopleEstimate, int delta)
	{
		return std::max(0, std::min(kEstimateMax, peopleEstimate + delta));
	}

	bool roomEstimateDue(int status, int minuteOfDay)
	{
		return (status == 30 || status == 32 || status == 33) && ((minuteOfDay % 60) + 60) % 60 < kTenantIdleMinutes;
	}

	Decision decideResidential(int estimate, int & counter)
	{
		if (estimate < kEstimateMoveOut) return Decision::MoveOut;
		if (estimate > kEstimateGrow) return Decision::Grow;
		if (++counter > 4)
		{
			counter = 0;
			return Decision::Grow;
		}
		return Decision::Stay;
	}

	int calcCurrEstimate(int estimate, int trips, int stress, bool resident)
	{
		int e;
		if (trips < 1)
			e = estimate - stress;
		else if (resident)
			e = (estimate * trips - stress) / trips;
		else
			e = (estimate * trips - stress + 300) / (trips + 1);
		return e < 1 ? 1 : e;
	}

	void updateEstimation(int & estimate, int & trips, int & stress, bool resident)
	{
		estimate = calcCurrEstimate(estimate, trips, stress, resident);
		stress = 0;
		if (!resident && trips < kMaxTrips)
			++trips;
	}

	int incEstimate(int estimate, int delta)
	{
		return clampEstimate(estimate + delta);
	}

	int incStress(int stress, int delta)
	{
		return std::max(0, std::min(kMaxStress, stress + delta));
	}

	bool waitTick(int & stress)
	{
		stress = incStress(stress, kWaitStress);
		return stress >= kMaxStress;
	}

	bool noRouteTry(int & stress)
	{
		stress = incStress(stress, kNoRouteStress);
		return stress >= kMaxStress;
	}

	int walkStep(int & curH, int dstH)
	{
		const int d = dstH - curH;
		const int step = std::min(kWalkStep, std::abs(d));
		curH += d < 0 ? -step : step;
		return step;
	}

	bool passesTenant(int curH, int left, int right, int score)
	{
		return left - score < curH && curH < score + right;
	}

	int moverRouteScore(int score, int floors)
	{
		return score * std::abs(floors);
	}

	std::vector<LedgerEntry> simulateLedger(const std::vector<LedgerItem> & items,
	                                        long long fromMinute, long long toMinute)
	{
		std::vector<LedgerEntry> out;
		for (long long m = fromMinute + 1; m <= toMinute; ++m)
		{
			const long long day = m / kMinutesPerDay;
			const int minute = (int)(m % kMinutesPerDay);

			// Midnight of day 1: T2Settlement::Update + CalcMentenanceCost.
			if (minute == 0 && opensQuarter(day))
			{
				int total = 0;
				for (size_t i = 0; i < items.size(); ++i)
					if (items[i].stats)
						total += maintenanceCost(*items[i].stats, items[i].widthUnits, items[i].cars);
				if (total != 0)
					out.push_back({m, -total, "maintenance"});
			}
			for (size_t i = 0; i < items.size(); ++i)
			{
				const LedgerItem & it = items[i];
				if (!it.stats) continue;
				switch (it.kind)
				{
				case LedgerItem::Office:
					// First idle inside the 8-minute window.
					if (it.occupied && minute == kOfficeRentFirstMinute && officeRentWindow(day, minute))
						out.push_back({m, officeQuarterRent(*it.stats, it.selection), "rent_income"});
					break;
				case LedgerItem::Commercial:
					if (minute == it.stats->closeMinute)
						out.push_back({m, commercialIncome(it.customersPerDay, *it.stats), "retail_income"});
					break;
				case LedgerItem::HotelRoom:
					if (it.occupied && minute == kHotelCheckoutMinute)
						out.push_back({m, hotelRoomIncome(*it.stats, it.selection), "hotel_income"});
					break;
				case LedgerItem::Maintained:
					break;
				}
			}
		}
		return out;
	}
}
}
