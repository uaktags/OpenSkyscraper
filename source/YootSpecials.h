/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower special items, part A: hotel back office, parking and the fire
/// department (game-parity.md W4, E7, I4-I6, L1; plan step 14).
///
/// Verbatim ports of the plugin / T2DLL.dll functions named on each
/// declaration, kept pure so they can be unit-tested without a Game.
/// SpecialSystem applies them to the tower; the items live in
/// Item/YootSpecialItems.*.
///
/// Tenant clock: a tenant idles once per 8 game minutes (YootRules.h), so a
/// "WithinMinutes(a, a + 8)" window fires exactly once a day.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace OT
{
	namespace Yoot
	{
		// ---- W4 / E7 hotel ----
		/// T2GuestroomItem::GetRoomType 0x10037cd0 by TNDF tool type:
		/// 1401 Single / 1481 Small Room -> 0, 1402 Twin / 1482 Large Room
		/// -> 1, 1403 Suite -> 2, anything else -1.
		int guestroomType(int toolType);
		/// ATTR mID of Front.t2p ("NORF" in Mac order); Single/Twin/Suite
		/// rooms and Housekeeping carry it as mDependID (ATTR +9). The Kegon
		/// rooms (Small/Large) and RFront/RMaintenance have none.
		const uint32_t kFrontDeskId = 0x46524f4e;
		/// A room whose plugin depends on the Front Desk gets no guests
		/// until a desk has linked it (T2.exe string 1049).
		bool roomNeedsFrontDesk(uint32_t dependId);

		/// Front.t2p ChangeStatusOnIdle thunk_FUN_10013570 windows (minutes):
		/// staff are called at open - 60 .. open - 52, the desk closes at
		/// close .. close + 8, the day's room income is paid at 12:00 .. 12:08
		/// (FUN_10015540) and the ledger restarts at 17:00 .. 17:08
		/// (FUN_10015450). TNDF open 06:00, close 01:00.
		const int kHotelPayMinute = 720;
		const int kHotelLedgerResetMinute = 1020;
		bool tenantWindow(int minuteOfDay, int first);   ///< [first, first + 8)

		/// The desk's refcon table (Front.t2p FUN_100148d0, 0x30 bytes):
		/// per room type a price selection (+0x16, default 3 = index 2) and
		/// the inMoney accrued since 17:00 (+0x24).
		struct FrontDeskLedger
		{
			std::array<int, 3> selection{{3, 3, 3}}; ///< 1-based (GetInMoneyIndex)
			std::array<int, 3> accrued{{0, 0, 0}};   ///< dollars

			/// FUN_10013ce0 -> FUN_10015420: a guest checked in at the desk
			/// and was sent to a room of `roomType`; its inMoney is booked.
			void checkIn(int roomType, int inMoney);
			/// FUN_10015540 at 12:00: the accrued income per room type, paid
			/// as one "Income from Hotel Facilities just came in." each.
			/// Returns the total (dollars); the ledger keeps its values.
			int noonIncome() const;
			/// FUN_10015450 at 17:00: clear the accrued income.
			void reset17();
		};
		const char * const kHotelIncomeMessage = "Income from Hotel Facilities just came in.";
		/// Front.t2p DefIdleProc 0x10013970: while 07:00..07:07 or
		/// 22:00..22:07, guest rooms exist (registry 1) but no Front Desk.
		bool frontDeskWarningMinute(int minuteOfDay);
		const char * const kFrontDeskWarning = "A Front is required to make the Hotel Facilities work.";

		/// One registry-1 room as Front.t2p FUN_10013b70 sees it.
		struct RoomLink
		{
			int linkedDesk = 0;     ///< T2Tenant +0x94 (0 = none)
			bool reachable = false; ///< CheckRoute(desk exit -> room entrance, search score)
		};
		/// FUN_10013b70, run when a desk opens with staff inside: every
		/// unlinked room in route range of the desk is linked to it.
		/// Returns how many rooms were linked.
		int linkRooms(int deskId, std::vector<RoomLink> & rooms);

		// ---- Housekeeping (Maintenance.t2p / RMaintenance.t2p) ----
		/// RMaintenance FUN_1000bd00 state 12: maids go out while the time
		/// is within [open, close - 120] (07:00..14:00); between close and
		/// open they are pushed out (go home).
		bool housekeepingDispatchMinute(int minuteOfDay, int openMinute, int closeMinute);
		/// FUN_1000bf70 / T2Matter return stack (FUN_100549a0 holds 3): one
		/// maid takes up to three dirty rooms (status 0x23 -> 0x24) per trip.
		const int kRoomsPerMaid = 3;
		/// Picks the next trip: the indices of up to kRoomsPerMaid dirty,
		/// reachable rooms in list order (FUN_1000c010 walks registry 1).
		std::vector<int> maidTrip(const std::vector<bool> & dirtyAndReachable);
		const char * const kBedsNotMade = "Beds are not made.";              ///< string 1047
		const char * const kRoomTooDirty = "Room is too dirty.";             ///< string 1048
		const char * const kNoFrontDesk = "Room is not connected to the front desk."; ///< string 1049

		// ---- I6 parking (Parking.t2p) ----
		/// FUN_1000cee0: a parking tenant wider than the TNDF width (4) is a
		/// gate (variation 1); a 4-unit tenant is a lot.
		bool parkingIsGate(int widthUnits, int defWidthUnits = 4);
		/// Parking.t2p idle FUN_1000bd20 for a gate: pattern 0 on the first
		/// floor (bottom row on B1), 1 with another gate directly below,
		/// otherwise 2 (the lowest gate of the stack).
		int gatePattern(bool onFirstFloor, bool gateBelow);
		/// One lot: TNDF belongCapacity 5 drivers (FindEmptyParking /
		/// T2Tenant::IsEmptyParking), a parked bit per slot in the status
		/// (FUN_1000cd40: status | 0x20 << (slot - 1)) and the pattern of the
		/// highest parked slot's car (FUN_1000cdb0: people id % 13 + 1).
		struct ParkingLotModel
		{
			static const int kSlots = 5;
			std::array<int, kSlots> driver{{0, 0, 0, 0, 0}}; ///< people ids, 0 = free
			std::array<bool, kSlots> parked{{false, false, false, false, false}};
			int pattern = 0;

			bool hasSpace() const;
			int cars() const;
			/// EnterTenant FUN_1000ca30 on the way in: register the driver in
			/// a free slot and park. Returns false when the lot is full.
			bool park(int peopleId);
			/// EnterTenant on the way out: unpark and drop the driver.
			bool leave(int peopleId);
			void updatePattern();
		};
		/// Parking.t2p DefIdleProc 0x1000cb60: while 06:10..06:19 or
		/// 21:10..21:19 and no parking has been built.
		bool parkingWarningMinute(int minuteOfDay);
		const char * const kParkingWarning = "The residents are requesting parking facilities.";
		/// T2People::MoveToTowerEntrance 0x10048910: a driver that finds no
		/// parking walks in from the lobby and loses 100 estimate.
		const int kNoParkingEstimate = 100;

		// ---- I4 / I5 fire department ----
		/// T2FireBurning::CallFireman 0x10031ed0: up to 6 people present in
		/// registry-5 tenants (Security Room, Fire Department), taken in
		/// list order. Returns how many each source sends.
		const int kMaxFiremen = 6;
		std::vector<int> callFiremen(const std::vector<int> & presentPerSource);
	}
}
