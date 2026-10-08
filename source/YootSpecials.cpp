/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootSpecials.h"

#include <algorithm>

namespace OT
{
	namespace Yoot
	{
		int guestroomType(int toolType)
		{
			switch (toolType) {
				case 0x579: case 0x5c9: return 0; // Singleroom 1401, Smallroom 1481
				case 0x57a: case 0x5ca: return 1; // Twinroom 1402, Largeroom 1482
				case 0x57b: return 2;             // Suiteroom 1403
			}
			return -1;
		}

		bool roomNeedsFrontDesk(uint32_t dependId)
		{
			return dependId == kFrontDeskId;
		}

		bool tenantWindow(int minute, int first)
		{
			return minute >= first && minute < first + 8;
		}

		void FrontDeskLedger::checkIn(int roomType, int inMoney)
		{
			// FUN_10015420: only types 0..2 are tracked.
			if (roomType < 0 || roomType > 2) return;
			accrued[roomType] += inMoney;
		}

		int FrontDeskLedger::noonIncome() const
		{
			return accrued[0] + accrued[1] + accrued[2];
		}

		void FrontDeskLedger::reset17()
		{
			accrued = {{0, 0, 0}};
		}

		bool frontDeskWarningMinute(int minute)
		{
			// 0x10013970: [0x1a4, 0x1ac) or [0x528, 0x530).
			return (minute >= 420 && minute < 428) || (minute >= 1320 && minute < 1328);
		}

		int linkRooms(int deskId, std::vector<RoomLink> & rooms)
		{
			int linked = 0;
			for (RoomLink & r : rooms) {
				if (r.linkedDesk != 0 || !r.reachable) continue;
				r.linkedDesk = deskId;
				++linked;
			}
			return linked;
		}

		bool housekeepingDispatchMinute(int minute, int open, int close)
		{
			// FUN_1000bd00: WithinMinutes(close, open) pushes the staff out;
			// otherwise FUN_1000bf70 runs while WithinMinutes(open, close - 120).
			return minute >= open && minute <= close - 120;
		}

		std::vector<int> maidTrip(const std::vector<bool> & dirtyAndReachable)
		{
			std::vector<int> trip;
			for (size_t i = 0; i < dirtyAndReachable.size() && (int)trip.size() < kRoomsPerMaid; ++i)
				if (dirtyAndReachable[i]) trip.push_back((int)i);
			return trip;
		}

		bool parkingIsGate(int width, int defWidth)
		{
			return defWidth < width;
		}

		int gatePattern(bool onFirstFloor, bool gateBelow)
		{
			if (onFirstFloor) return 0;
			return gateBelow ? 1 : 2;
		}

		bool ParkingLotModel::hasSpace() const
		{
			for (int id : driver) if (id == 0) return true;
			return false;
		}

		int ParkingLotModel::cars() const
		{
			int n = 0;
			for (bool p : parked) if (p) ++n;
			return n;
		}

		bool ParkingLotModel::park(int peopleId)
		{
			if (peopleId == 0) return false;
			int slot = -1;
			for (int k = 0; k < kSlots; ++k)
				if (driver[k] == peopleId) slot = k;
			for (int k = 0; slot < 0 && k < kSlots; ++k)
				if (driver[k] == 0) slot = k;
			if (slot < 0) return false;
			driver[slot] = peopleId;
			parked[slot] = true;
			updatePattern();
			return true;
		}

		bool ParkingLotModel::leave(int peopleId)
		{
			for (int k = 0; k < kSlots; ++k) {
				if (driver[k] != peopleId || peopleId == 0) continue;
				driver[k] = 0;
				parked[k] = false;
				updatePattern();
				return true;
			}
			return false;
		}

		void ParkingLotModel::updatePattern()
		{
			// FUN_1000cdb0: the highest parked slot shows its car.
			pattern = 0;
			for (int k = kSlots - 1; k >= 0; --k)
				if (parked[k]) { pattern = (int)((unsigned)driver[k] % 13u) + 1; break; }
		}

		bool parkingWarningMinute(int minute)
		{
			// 0x1000cb60: [0x172, 0x17c) or [0x4f6, 0x500).
			return (minute >= 370 && minute < 380) || (minute >= 1270 && minute < 1280);
		}

		std::vector<int> callFiremen(const std::vector<int> & present)
		{
			std::vector<int> sent(present.size(), 0);
			int total = 0;
			for (size_t i = 0; i < present.size() && total < kMaxFiremen; ++i) {
				const int n = std::min(std::max(0, present[i]), kMaxFiremen - total);
				sent[i] = n;
				total += n;
			}
			return sent;
		}

		RoomLook roomLook(bool occupied, bool dirty, int guestsIn, int asleep, int awakeAfterSleep)
		{
			if (dirty) return RoomLook::Dirty;
			if (!occupied) return RoomLook::Vacant;
			if (guestsIn > 0 && asleep >= guestsIn) return RoomLook::Asleep;
			if (awakeAfterSleep > 0) return RoomLook::Morning;
			return RoomLook::Lit;
		}

		int roomPatternIndex(RoomLook look, int kind, int numKinds)
		{
			numKinds = std::max(1, numKinds);
			kind = ((kind % numKinds) + numKinds) % numKinds;
			return 5 * kind + static_cast<int>(look);
		}
	}
}
