/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower economy, evaluation and stress formulas (game-parity.md §1.4
/// E3-E7, §1.5 J1-J3, §1.6 P1-P3; plan steps 7-9).
///
/// Verbatim ports of the T2DLL.dll / T2.exe / plugin functions named on each
/// declaration. Money is in dollars unless a name says "units" (the engine
/// stores $100 units; kYootMoneyUnit converts). Every function is pure so the
/// rules can be unit-tested without a Game; Game, the items, Person and
/// JudgeSystem call them only when the YootTower profile is active.
///
/// Clock: the engine steps 4 sub-ticks per game minute (T2DateTime::
/// IsIncDateTime 0x1001ca90). People idle once every 8 steps
/// (T2PeopleArrayList::DispatchIdle 0x1004b620 round-robin of 8) = every
/// 2 game minutes; tenants once every 32 steps (T2TenantArrayList::
/// DispatchIdle 0x10061c70, round-robin of 32) = every 8 game minutes.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	struct YootItemStats;

	namespace Yoot
	{
		// ---- clock ----
		const int kMinutesPerDay = 1440;
		const int kPeopleIdleMinutes = 2;   ///< one T2People::Idle per 2 game minutes
		const int kTenantIdleMinutes = 8;   ///< one tenant ChangeStatusOnIdle per 8 game minutes

		/// Absolute game minute (day * 1440 + minute) of an OpenSky absolute
		/// time on the YootTower minute-linear clock.
		long long absoluteMinute(double absolute);

		// ---- E3 quarterly maintenance ----
		/// T2.exe FUN_0041dc40: when the day-of-quarter changes to 1 (at
		/// midnight, inside the hour-change branch) the doc calls
		/// T2Settlement::Update and T2FloorInfo::CalcMentenanceCost.
		/// `day` is the absolute day index; day % 3 == 0 opens a quarter.
		bool opensQuarter(long long day);

		/// One item's quarterly maintenance in dollars
		/// (T2TenantDef 0x10062df0 / T2MoverDef 0x10040520 / T2ToolDef 0x10065bc0
		/// ::CalcMentenanceCostProc): 0 without attr 0x100; attr 0x4000 items
		/// pay outMoney x width units; movers pay outMoney x cars, halved
		/// (integer) when the mover is floor-type (attr 0x2000).
		int maintenanceCost(const YootItemStats & stats, int widthUnits, int cars);

		// ---- E4 office rent ----
		/// Office.t2p FUN_1000c0c0: rent is due while 0x123 < minute < 0x12c
		/// (04:52..04:59) on day-of-quarter 1 (month % 3 == 0). The tenant
		/// idles once per 8 minutes, so it pays exactly once per quarter.
		bool officeRentWindow(long long day, int minuteOfDay);
		const int kOfficeRentFirstMinute = 292;
		const int kOfficeRentLastMinute = 299;
		/// 3 x inMoney[selection] (dollars).
		int officeQuarterRent(const YootItemStats & stats, int selection);

		// ---- E5 price index ----
		/// T2Tenant::GetPriceIndex 0x1005f880: selection + 1 when the def
		/// has more than one inMoney level, otherwise 0.
		int priceIndex(int selection, int numInMoney);
		/// OpenSky rent class (0 High .. 3 Very low) -> YootTower price
		/// selection (0 = cheapest inMoney level).
		int selectionFromRentClass(int rentClass, int numInMoney);

		// ---- E6 commercial income ----
		/// T2Tenant::CalcInMoney 0x1005f620, engine $100 units:
		/// customers x inMoney / 100 - outMoney.
		int calcInMoneyUnits(int customers, int inMoneyUnits, int outMoneyUnits);
		/// Same in dollars (stats in dollars, result x100 of the unit value).
		int commercialIncome(int customers, const YootItemStats & stats);
		/// T2Tenant::CalcProfitEstimate 0x10061410:
		/// 70 x (CalcInMoney + outMoney) / outMoney, clamped to 1..300.
		int profitEstimate(int customers, int inMoneyUnits, int outMoneyUnits);

		// ---- E7 hotel ----
		/// T2GuestroomTable::CalcInMoney 0x10037a00 sums inMoney for rooms in
		/// the occupied states; one room's share in dollars.
		int hotelRoomIncome(const YootItemStats & stats, int selection);

		// ---- J1-J3 tenant estimate (1..300) ----
		const int kEstimateMin = 1;
		const int kEstimateMax = 300;
		const int kEstimateMoveOut = 70;   ///< below: residents leave
		const int kEstimateGrow = 200;     ///< above: residents duplicate
		int clampEstimate(int estimate);
		/// T2Tenant::CalcEstimateColor 0x10060a20: -1 none, 14 red (< 70),
		/// 9 yellow (70..200), 12 blue (> 200).
		int estimateColor(int estimate);

		struct ResidentialInputs
		{
			bool hasBelongs = true;           ///< tenant+0x84 != 0 (else estimate 1)
			std::vector<int> peopleEstimates; ///< residents' CalcCurrEstimate
			int previousPeopleEstimate = 0;   ///< tenant+0x8c kept when no residents
			int senseOfNoise = 0;             ///< TenantDef +0x1a4
			int trafficRange = 0;             ///< T2TrafficInfo::GetRange(entrance), 0..3
			int priceIndex = 0;               ///< GetPriceIndex (0 = no price levels)
			int yearsSinceBuilt = 0;          ///< T2DateTime::CalcLapseYears
		};
		/// T2Tenant::UpdateResidencialEstimate 0x10060e20: mean resident
		/// estimate - noise x traffic + (3 - priceIndex) x 30 - years, 1..300.
		/// `peopleEstimateOut` receives the stored mean (tenant+0x8c).
		int residentialEstimate(const ResidentialInputs & in, int * peopleEstimateOut = nullptr);

		enum class Decision { MoveOut, Grow, Stay };
		/// T2Tenant::DecideResidencialEstimation 0x10061380: < 70 move out,
		/// > 200 grow, otherwise grow on every 5th call (shared counter).
		Decision decideResidential(int estimate, int & counter);

		// ---- P2 people estimate ----
		const int kPeopleEstimateStart = 300; ///< T2People::Initialize 0x10048320
		const int kMaxTrips = 4;
		/// T2People::CalcCurrEstimate 0x100496f0. `resident` = status 0xb or 1
		/// (no +300 term, divides by n).
		int calcCurrEstimate(int estimate, int trips, int stress, bool resident = false);
		/// T2People::UpdateEstimation 0x10049790: estimate = current,
		/// stress = 0, trips + 1 (max 4; residents keep 0).
		void updateEstimation(int & estimate, int & trips, int & stress, bool resident = false);
		/// T2People::IncEstimate 0x10049750 (clamped to 1..300).
		int incEstimate(int estimate, int delta);

		// ---- P1 stress ----
		const int kMaxStress = 300;
		const int kWaitStress = 5;            ///< IdleWaitMover 0x10048880
		const int kNoRouteStress = 30;        ///< SolveNoRoute 0x10048c40
		const int kGiveUpEstimatePenalty = 50;    ///< waited out (IncEstimate -0x32)
		const int kNoRouteEstimatePenalty = 150;  ///< no route (IncEstimate -0x96)
		/// T2People::IncStress 0x100497e0 (clamped to 0..300).
		int incStress(int stress, int delta);
		/// One idle tick while waiting for a mover: +5; returns true when the
		/// person gives up (stress >= 300: jump to destination, -50 estimate).
		bool waitTick(int & stress);
		/// One failed route attempt: +30; true when stress >= 300 ("No route
		/// to destination.", jump to destination, -150 estimate).
		bool noRouteTry(int & stress);

		// ---- floor walk ----
		const int kWalkStep = 16;  ///< units per people idle
		/// T2People::IsReachDestination 0x10049310, one people idle of a walk
		/// from `curH` to `dstH`: moves up to 16 units and returns the units
		/// moved; off the ground floor the step costs IncStress(step / 4).
		int walkStep(int & curH, int dstH);
		/// T2People::StopOnHisWay 0x100494b0: a walker at `curH` passes a
		/// tenant spanning [left, right) within `score` units.
		bool passesTenant(int curH, int left, int right, int score);

		// ---- P3 route score ----
		/// T2Mover::CalcScore 0x1003ef20: MoverDef score x |floors|; 0x7fff
		/// when the mover has no def.
		int moverRouteScore(int score, int floors);
		const int kNoMoverScore = 0x7fff;

		// ---- ledger fixture (plan step 7 test) ----
		/// A minimal tower for simulating the timed YootTower cash flows.
		struct LedgerItem
		{
			enum Kind { Maintained, Office, Commercial, HotelRoom };
			Kind kind = Maintained;
			const YootItemStats * stats = nullptr;
			int widthUnits = 1;
			int cars = 0;
			int selection = 0;        ///< price selection (office / hotel)
			bool occupied = true;     ///< office let / hotel room occupied
			int customersPerDay = 0;  ///< commercial customers per day
		};
		struct LedgerEntry
		{
			long long minute = 0;     ///< absolute game minute
			int amount = 0;           ///< dollars (+ income, - cost)
			std::string category;     ///< "maintenance", "rent_income", ...
		};
		/// Steps minute by minute over (fromMinute, toMinute] and books every
		/// timed flow in the order the engine performs them: quarterly
		/// maintenance at midnight of day 1, office rent at 04:52 on day 1,
		/// commercial income at the tenant's close time (CalcInMoney, net of
		/// the daily outMoney), hotel income at the 11:00 checkout.
		std::vector<LedgerEntry> simulateLedger(const std::vector<LedgerItem> & items,
		                                        long long fromMinute, long long toMinute);
		const int kHotelCheckoutMinute = 11 * 60;
	}
}
