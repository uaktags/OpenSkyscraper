/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootVenues.h"
#include "YootTenants.h"

#include <algorithm>
#include <cmath>

namespace OT
{
	namespace Yoot
	{
		// ------------------------------------------------------------ movies

		MovieCycle movieCycle(const MovieDef & def)
		{
			MovieCycle c;
			c.p1 = def.phase1Months;
			c.p2 = c.p1 + def.phase2Months;
			c.p3 = c.p2 + def.phase3Months;
			return c;
		}

		int movieLifePhase(const MovieDef & def, const MovieState & s, int totalDays, int baseDay)
		{
			if (s.flags & MovieState::kRetired) return 4;
			const MovieCycle c = movieCycle(def);
			if (c.cycle() <= 0) return 1;
			int t = (totalDays - baseDay) % c.cycle();
			if (t < 0) t += c.cycle();
			if (t < c.p1) return 0;
			if (t < c.p2) return 1;
			return 2;
		}

		bool isRoadshowFilm(const MovieDef & def) { return def.filmKind == 0; }

		bool isFamousFilm(const MovieDef & def, const MovieState & s)
		{
			return (s.flags & MovieState::kFamous) != 0 || def.filmKind == 2;
		}

		void movieReMake(const MovieDef & def, MovieState & s)
		{
			if (s.bestR == -1) return;
			if (s.bestR >= def.famousAudience && s.bestF == -1)
			{
				s.bestF = s.bestR;
				s.flags |= MovieState::kFamous;
			}
			if (s.bestR >= def.remakeAudience)
			{
				s.flags |= MovieState::kSequel;
				s.bestR = -1;
				s.remakeCount++;
			}
			else
				s.flags |= MovieState::kRetired;
		}

		bool movieChkReMake(const MovieDef & def, MovieState & s, int elapsedDays)
		{
			if (s.pendingR > 0) s.bestR = s.pendingR;
			if (s.pendingF > 0) s.bestF = s.pendingF;
			s.pendingR = s.pendingF = -1;
			const int cycle = movieCycle(def).cycle();
			if (s.bestR == -1 || cycle <= 0) return false;
			if (elapsedDays % cycle != 0 || elapsedDays / cycle == 0) return false;
			movieReMake(def, s);
			return true;
		}

		std::string movieTitle(const MovieDef & def, const MovieState & s, bool classic)
		{
			std::string t = def.header.title;
			if (classic || !(s.flags & MovieState::kSequel) || s.remakeCount < 1) return t;
			// GetTitle 0x10043530, type 1: unk114 selects the sequel style.
			// Shipped films use 1 ("PART II", "PART III", ...) except Train (0,
			// a number). The prefix/extra-string styles (2..4) are unused.
			static const char * roman[] = {"II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
			const int n = s.remakeCount; // 1 = first sequel
			if (def.unk114 == 1)
				return t + " PART " + roman[std::min(n - 1, 8)];
			if (def.unk114 == 2)
				return "After " + t;
			return t + " " + std::to_string(n + 1);
		}

		bool movieOffered(const MovieDef & def, const MovieState & s, bool classic, int phase)
		{
			if (classic) return isFamousFilm(def, s);
			return isRoadshowFilm(def) && phase == 1;
		}

		int theaterShowCount(int open, int close, int runningTime)
		{
			const int slot = runningTime + 30;
			if (slot <= 0 || close <= open) return 0;
			return (close - open) / slot;
		}

		TheaterShow theaterShowAt(int minute, int open, int close, int runningTime)
		{
			TheaterShow r;
			const int slot = runningTime + 30;
			const int n = theaterShowCount(open, close, runningTime);
			for (int k = 0; k < n; ++k)
			{
				const int start = open + 30 + k * slot;
				if (minute >= start - 30 && minute < start)
				{
					r.status = TheaterShow::Doors;
					r.show = k;
					return r;
				}
				if (minute >= start && minute < start + runningTime)
				{
					r.status = TheaterShow::Playing;
					r.show = k;
					return r;
				}
			}
			return r;
		}

		int theaterAudienceTarget(int target, int yesterdayBest, int persons,
		                          int initialAudience, int capacity, int retentionPct)
		{
			if (target < 0)
			{
				if (persons > 0) return persons;
				if (initialAudience > 0) return initialAudience;
				return capacity;
			}
			return (int)(yesterdayBest * retentionPct * 0.01);
		}

		bool theaterCollects(int minute, int open, int close, int bindTime)
		{
			return minute >= open && minute < close - bindTime - 60;  // WithinMinutes: [a, b)
		}

		// ------------------------------------------------------- hall events

		bool hallEventInMonth(const HallEventDef & def, int dayOfYear)
		{
			if (dayOfYear < 0 || dayOfYear > 11) return false;
			return (def.monthMask >> dayOfYear) & 1u;
		}

		int hallEventSettlement(int costUnits, int outMoneyUnits)
		{
			return (costUnits - outMoneyUnits) * 100;
		}

		int hallStatusOnEnter(int status, int inside, int capacity)
		{
			if (status == 30) return 31;
			if (status == 31 && inside >= capacity / 2) return 32;
			return status;
		}

		int hallStatusOnLeave(int status, int inside, int capacity)
		{
			if (inside == 0) return status != 10 ? 30 : status;
			if (status == 32 && inside <= capacity / 2) return 31;
			return status;
		}

		bool hallStatusReset(int minute, int open)
		{
			return withinMinutes(minute, open - 60, open - 52) || withinMinutes(minute, open, open + 8);
		}

		bool hallEstimateIdle(int minute, int open)
		{
			return minute >= open && (minute % 60 < 8 || withinMinutes(minute, open, open + 8));
		}

		bool hallEventFlop(const HallEventDef & def, int attendance)
		{
			return attendance < def.unkE0;
		}

		int hallEventNextValue(const HallEventDef & def, int attendance, int good, int medium)
		{
			return hallEventFlop(def, attendance) ? def.unkDC : 2 * good + medium;
		}

		int hallCollectRoom(int target, int capacity, int inside, int walking)
		{
			if (target - inside - walking <= 0) return 0;
			return std::max(0, capacity - inside - walking);
		}

		bool hallEventCollects(int minute, int tenantOpen, int tenantClose, int customerUnit)
		{
			return minute >= tenantOpen && minute < tenantClose - customerUnit - 60;
		}

		// --------------------------------------------------------------- ads

		bool advertiseFits(const AdvertiseDef & def, uint32_t hostSubPluginType)
		{
			const uint32_t v = uint32_t((unsigned char)def.subType.c[0]) |
			                   (uint32_t((unsigned char)def.subType.c[1]) << 8) |
			                   (uint32_t((unsigned char)def.subType.c[2]) << 16) |
			                   (uint32_t((unsigned char)def.subType.c[3]) << 24);
			return v == hostSubPluginType;
		}

		// ------------------------------------------------------- final items

		bool stadiumSeason(int dayOfYear)
		{
			const unsigned m = (unsigned)dayOfYear + 1;
			return m > 3 && m < 11;
		}

		int stadiumStatus(int s, int minute, int open, int close, bool season, int roll)
		{
			if (s == 10)
			{
				if (season && minute >= open && minute < open + 8) return 30;
				return 10;
			}
			if (s < 33)
			{
				if (minute >= 1080) return 33;
				if (minute >= 1020) return 32;
				if (minute >= 960) return 31;
				return s;
			}
			if (s < 35)
			{
				if (minute < 1260) return (roll % 10 != 0) ? 33 : 34;
				return 35;
			}
			// 35: the crowd leaves; the day closes at TNDF close.
			if (minute >= close) return 10;
			return 35;
		}

		bool baseballBeginDay(bool done, int dayOfYear, bool hasStadium)
		{
			return !done && stadiumSeason(dayOfYear) && hasStadium;
		}

		bool weddingBeginDay(bool done, bool holiday, bool hasCathedral)
		{
			return !done && holiday && hasCathedral;
		}

		bool weddingReady(bool cathedralOpen, int visitorsToday, bool couple)
		{
			return cathedralOpen && visitorsToday >= kWeddingVisitors && couple;
		}

		std::string weddingAnnouncement(const std::string & roomA, const std::string & nameA,
		                                const std::string & roomB, const std::string & nameB)
		{
			// Hawai.t2p Exec status 2, STRTBL 1 strings 0 and 1.
			return std::string("'From now at the top of the Cathedral,") + "," +
			       roomA + " of " + nameA + " Mr." + " and " + "," +
			       roomB + " of " + nameB + " Ms." + " of " + "the wedding will start.'";
		}

		bool cathedralOpen(bool holiday, int minute, int open, int close)
		{
			return holiday && minute >= open && minute < close;
		}

		int attractionPattern(bool open, int inside)
		{
			if (!open) return 0;
			if (inside <= 0) return 1;
			return inside < 40 ? 2 : 3;
		}

		bool attractionRenovates(int grade, int visitorsToday, bool renovated)
		{
			return grade < 5 && visitorsToday >= kAttractionRenovateVisitors && !renovated;
		}

		// ------------------------------------------------------------ subway

		std::vector<int> subwayArrivals(const std::vector<std::pair<int, int> > & trains, int level)
		{
			std::vector<int> out;
			for (const auto & t : trains)
				if (t.second < level) out.push_back(t.first);
			std::sort(out.begin(), out.end());
			return out;
		}

		int subwayStoppedTrain(const std::vector<int> & arrivals, int minute)
		{
			for (size_t i = 0; i < arrivals.size(); ++i)
				if (minute >= arrivals[i] && minute < arrivals[i] + kSubwayStopMinutes) return (int)i;
			return -1;
		}

		int subwayStoppedTrainType(const std::vector<std::pair<int, int> > & trains, int level, int minute)
		{
			int best = -1, bestMinute = -1;
			for (const auto & t : trains)
				if (t.second < level && minute >= t.first && minute < t.first + kSubwayStopMinutes && t.first > bestMinute)
				{
					best = t.second;
					bestMinute = t.first;
				}
			return best;
		}

		std::vector<SubwayTrainPos> subwayTrains(const std::vector<std::pair<int, int> > & trains, int level,
		                                         int worldWidth, int stationUnit, int minute, int tick)
		{
			std::vector<SubwayTrainPos> out;
			for (const auto & t : trains)
			{
				const int arrive = t.first;
				const bool stops = t.second < level;
				if (minute < arrive - worldWidth / 4) continue;
				int off;
				if (minute < arrive) off = tick + 4 * (minute - arrive);
				else if (stops && minute < arrive + kSubwayStopMinutes) off = 0;
				else off = tick + 4 * (minute - (stops ? arrive + kSubwayStopMinutes : arrive));
				if (stationUnit + off >= worldWidth) continue;
				out.push_back({t.second, stationUnit + off});
			}
			return out;
		}

		int subwayPattern(bool open, int inside, int capacity)
		{
			if (!open) return 0;
			if (inside <= 0) return 1;
			return (capacity > 0 && inside >= capacity / 2) ? 3 : 2;
		}

		bool subwayOpen(int minute, int open, int close)
		{
			return minute >= open && minute < close;
		}

		// -------------------------------------------------------- sky bridge

		SkyBridgeError skyBridgeRule(int floor, bool leftTower, bool rightTower, int gap, bool clearAboveBelow)
		{
			if (floor < kSkyBridgeMinFloor) return SkyBridgeError::TooLow;
			if (!leftTower || !rightTower || gap <= 0) return SkyBridgeError::NotBetweenTowers;
			if (gap > kSkyBridgeMaxGap) return SkyBridgeError::TooLong;
			if (!clearAboveBelow) return SkyBridgeError::NotClear;
			return SkyBridgeError::None;
		}

		// ------------------------------------------------------------ clouds

		int cloudFloor(int G, int roll)
		{
			if (G <= 0) return 0;
			const long long range = (long long)G * G;
			const long long r = ((long long)roll % range + range) % range;
			return G - (int)std::floor(std::sqrt((double)r));
		}
	}
}
