/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootFacilities.h"

namespace OT
{
	namespace Yoot
	{
		bool isDustOn(const YootFacilityRules & r, int grade)
		{
			return r.dustOnGrade != 0 && r.dustOnGrade <= grade;
		}

		int toiletDenomi(const YootFacilityRules & r, int grade)
		{
			if (r.toiletGrade != 0 && r.toiletGrade <= grade) return r.toiletDenomi;
			return 0;
		}

		int coldDenomi(const YootFacilityRules & r, int grade, int monthPlusOne)
		{
			if (!r.hasColdTable) return 0;
			if (grade < r.coldMinGrade || monthPlusOne < 0 || monthPlusOne >= 12) return 0;
			return r.coldMonthly[monthPlusOne];
		}

		bool rollDenomi(int denomi, int roll)
		{
			return denomi != 0 && roll % denomi == 0;
		}

		bool trashWarningMinute(int minute)
		{
			return (minute > 0x199 && minute < 0x1a4) || (minute > 0x51d && minute < 0x528);
		}

		bool TrashFacilityModel::deliver()
		{
			if (!accepts()) return false;
			++garbage;
			// FUN_1000bcc0 compares the new count against capacity >> 2.
			const int quarter = capacity >> 2;
			if (garbage == 1)                { status = kOpen + 1; pattern = 1; }
			else if (garbage == quarter)     { status = kOpen + 2; pattern = 2; }
			else if (garbage == quarter * 2) { status = kOpen + 3; pattern = 3; }
			else if (garbage == quarter * 3) { status = kOpen + 4; pattern = 4; }
			else if (garbage == capacity)    { status = kFull;     pattern = 5; }
			return true;
		}

		static bool within(int minute, int from)
		{
			// T2DateTime::WithinMinutes(from, from + 8).
			return minute >= from && minute < from + 8;
		}

		bool TrashFacilityModel::idle(int minute, int openMinute, int closeMinute)
		{
			if (status > kOpen && status <= kFull) {
				if (within(minute, closeMinute)) {
					garbage = 0;
					status = kEmptying;
					pattern = kEmptyingPattern;
					return true;
				}
			} else if (status == kEmptying) {
				garbage = 0;
				if (within(minute, openMinute)) {
					status = kOpen;
					pattern = 0;
					return true;
				}
			}
			return false;
		}

		ToiletResult searchToilet(const std::vector<RestroomCandidate> & c, int & index)
		{
			int result = -1;
			index = -1;
			for (size_t i = 0; i < c.size() && result != 0; ++i) {
				if (!c[i].reachable || !c[i].open) continue;
				if (c[i].users < c[i].capacity) {
					result = 0;
					index = (int)i;
				} else {
					result = -2;
				}
			}
			if (result == 0) return ToiletResult::Found;
			return result == -2 ? ToiletResult::Crowded : ToiletResult::NoneNearby;
		}

		const char * toiletFailureMessage(ToiletResult r)
		{
			switch (r) {
			case ToiletResult::Crowded:    return "Restroom is crowded.";
			case ToiletResult::NoneNearby: return "No restroom nearby.";
			default:                       return "";
			}
		}

		int restroomPattern(int previous, int users, int capacity, bool entering)
		{
			const int half = capacity >> 1;
			if (entering) {
				if (users == 1) return 1;
				if (users == half) return 2;
			} else {
				if (users == 0) return 0;
				if (users == half - 1) return 1;
			}
			return previous;
		}

		int searchDoctor(const std::vector<DoctorCandidate> & c)
		{
			int result = 0; // the plugin's 0: nothing in range
			int found = -1;
			for (size_t i = 0; i < c.size() && result < 1; ++i) {
				if (!c[i].reachable) continue;
				if (!c[i].open) {
					if (result != -1) result = -2;
				} else if (c[i].patients < c[i].capacity) {
					result = 1;
					found = (int)i;
				} else {
					result = -1;
				}
			}
			if (result == 1) return found;
			return result == 0 ? -3 : result;
		}

		bool doctorCallMinute(int minute)
		{
			return minute >= 0x1e0 && minute < 0x438 && minute % 60 < 8;
		}

		int doctorPattern(int previous, int patients, int capacity, bool entering)
		{
			const int half = capacity / 2;
			if (entering) {
				if (patients == 1) return 2;
				if (patients == half) return 3;
			} else {
				if (patients == 0) return 1;
				if (patients == half - 1) return 2;
			}
			return previous;
		}

		const char * trashCommentText(TrashComment c)
		{
			switch (c) {
			case TrashComment::NoFacility:   return "There's no Trash Facility.";
			case TrashComment::NotConnected: return "Item not connected to Trash Facility.";
			case TrashComment::Full:         return "Full of garbage.";
			default:                         return "";
			}
		}
	}
}
