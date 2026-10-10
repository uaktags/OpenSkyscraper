/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower venues, final items and transport (game-parity.md S1, S2, R4,
/// V6, I7; plan step 14 part B): movie theaters and their MV_* films, the
/// Event Center and its Pl* hall events, the Billboard and its ads, the
/// Stadium/baseball, Cathedral/wedding and Attraction renovation that grant
/// the top grade, and the subway timetable.
///
/// Verbatim ports of the T2DLL.dll / plugin functions named on each
/// declaration, kept pure so they can be unit-tested without a Game.
/// VenueSystem applies them to the tower; the items live in
/// Item/YootVenueItems.*.
///
/// Time: a YootTower "month" is one game day (12 per year); totalDays is
/// T2DateTime::CalcTotalDays (month + 12*year - 12), minutes are 0..1439.
#pragma once

#include "SubPluginDefs.h"

#include <string>
#include <utility>
#include <vector>

namespace OT
{
	namespace Yoot
	{
		// ================================================= movies (S1)

		/// T2MoviePlugin runtime state (+0x128.. ; the 12-byte per-movie
		/// record of a .bld). R/F are the best roadshow / classic audience,
		/// -1 = never judged; the setters only store a pending value that
		/// ChkReMake commits at the next midnight.
		struct MovieState
		{
			static const int kFamous = 2;         ///< flags: ReMake made it famous
			static const int kSequel = 1;         ///< flags: re-runs as a sequel
			static const int kRetired = 0x8000;   ///< flags: life phase 4 for good
			int flags = 0;
			int bestR = -1;
			int bestF = -1;
			int pendingR = -1;
			int pendingF = -1;
			int remakeCount = 0;                  ///< +0x134
		};

		/// UnfoldPluginSelf 0x100433b0: cumulative phase ends.
		struct MovieCycle { int p1 = 0, p2 = 0, p3 = 0; int cycle() const { return p3; } };
		MovieCycle movieCycle(const MovieDef & def);

		/// GetMovieLifePhase 0x10043320 (no tie-up date in the shipped
		/// films): 4 when retired, else t = (totalDays - baseDay) % cycle
		/// gives 0 (not released, t < P1), 1 (now showing, t < P2) or 2.
		int movieLifePhase(const MovieDef & def, const MovieState & s, int totalDays, int baseDay);
		/// IsRoadshowFilm 0x10043710 / IsFamousFilm 0x100436f0.
		bool isRoadshowFilm(const MovieDef & def);
		bool isFamousFilm(const MovieDef & def, const MovieState & s);
		/// ReMake 0x100434c0 (only when R != -1): R >= famousAudience and no
		/// F yet -> F = R, famous; R >= remakeAudience -> sequel (R = -1,
		/// remakeCount++), else retired.
		void movieReMake(const MovieDef & def, MovieState & s);
		/// ChkReMake 0x10043720: commit pending R/F, then ReMake on the first
		/// day of every new cycle (elapsed % cycle == 0, elapsed / cycle != 0).
		/// Returns true when ReMake ran.
		bool movieChkReMake(const MovieDef & def, MovieState & s, int elapsedDays);
		/// GetTitle 0x10043530 with type 1 (Theater) or 3 (Classic).
		std::string movieTitle(const MovieDef & def, const MovieState & s, bool classic);
		/// Dialog filter: Theater.t2p 0x1000b890 (roadshow film in phase 1),
		/// ClassicTheater.t2p 0x1000bf50 (famous film, any time).
		bool movieOffered(const MovieDef & def, const MovieState & s, bool classic, int phase);

		/// Theater.t2p / ClassicTheater.t2p show schedule (0x1000e5e0): shows
		/// of runningTime with 30 minutes of open doors before each, back to
		/// back from `open`; show n (0-based) starts at open + 30 + n*slot.
		struct TheaterShow
		{
			enum Status { Closed = 10, Doors = 30, Playing = 32 };
			Status status = Closed;
			int show = -1;   ///< 0-based show of the day, -1 when closed
		};
		int theaterShowCount(int open, int close, int runningTime);
		TheaterShow theaterShowAt(int minute, int open, int close, int runningTime);
		/// Theater.t2p 0x1000e640 at the day's first door opening: a negative
		/// target (first day / new movie) takes the record (`persons`), then
		/// the movie's initial audience (+0x11c), then the capacity; later
		/// days keep `retentionPct`% (unk104 = 95) of yesterday's best show.
		int theaterAudienceTarget(int target, int yesterdayBest, int persons,
		                          int initialAudience, int capacity, int retentionPct);
		/// Theater.t2p collects customers only until close - bindTime - 60
		/// (Theater 21:00; Classic 19:00, so its 20:00 show draws nobody:
		/// the oracle's Classic Movie Theatre earned 3 x 40 tickets a day).
		bool theaterCollects(int minute, int open, int close, int bindTime);

		// ================================================= hall events (S1)

		/// HEDF monthMask: bit d = day-of-year d (0..11) the event may be
		/// booked / held.
		bool hallEventInMonth(const HallEventDef & def, int dayOfYear);
		/// EventCenter.t2p FUN_1000bec0 at close: DoPayTool(outMoney - cost),
		/// i.e. funds += cost - outMoney (engine $100 units; cost 0 when
		/// nothing is booked). Returns dollars.
		int hallEventSettlement(int costUnits, int outMoneyUnits);
		/// EventCenter.t2p EnterTenant hook 0x1000bba0 (inside already counts the
		/// entrant): 30 -> 31 on the first visitor, 31 -> 32 once capacity / 2 are
		/// inside (signed halving). Other statuses are unchanged.
		int hallStatusOnEnter(int status, int inside, int capacity);
		/// LeaveTenant hook 0x1000bcb0 (inside no longer counts the leaver): an
		/// empty hall that is not closed (10) goes back to 30; 32 -> 31 when
		/// inside <= capacity / 2.
		int hallStatusOnLeave(int status, int inside, int capacity);
		/// FUN_1000bec0 resets the booked hall to status 30 in [open - 60,
		/// open - 52) and again in [open, open + 8) (one plugin idle each), even
		/// with visitors inside (oracle: status 30 with 3 and 7 inside at 11:00).
		bool hallStatusReset(int minute, int open);
		/// FUN_1000bec0 refreshes the tenant estimate (UpdateCommercialEstimate)
		/// on a plugin idle at or after the open minute in the first 8 minutes of
		/// every hour, in [open, open + 8) and at close.
		bool hallEstimateIdle(int minute, int open);
		/// EventCenter.t2p FUN_1000dcb0 after an event's last day: attendance
		/// below unkE0 is a flop (36-day cooldown, its bookings dropped, the
		/// target falls back to unkDC); otherwise the next target is
		/// 2 x good + medium leavers (LeaveTenant hook FUN_1000bcb0).
		const int kHallEventCooldownDays = 36;
		bool hallEventFlop(const HallEventDef & def, int attendance);
		int hallEventNextValue(const HallEventDef & def, int attendance, int good, int medium);
		/// T2Tenant::CollectCustomer 0x1005f440: visitors are only collected
		/// in [TNDF open, close - customerUnit - 60) (WithinMinutes is half
		/// open; Event Center 09:00-17:00), so the evening events (Rock Live,
		/// Fashion Show) draw nobody.
		bool hallEventCollects(int minute, int tenantOpen, int tenantClose, int customerUnit);

		// ================================================= ads (Billboard)

		/// T2ToolDef::Add(T2AdvertisePlugin*) 0x10065cc0: an ad only goes on
		/// a medium whose ToolDef mSubPluginType equals its subType ('BoPI').
		bool advertiseFits(const AdvertiseDef & def, uint32_t hostSubPluginType);

		// ================================================= final items (R4)

		/// Stadium.t2p FUN_1000bf10: games run when the day index + 1 is in
		/// (3, 11), i.e. days 3..9 ("April-October").
		bool stadiumSeason(int dayOfYear);
		/// Stadium status machine (pattern = status - 29, closed = 0):
		/// 10 closed, 30 open at TNDF open (12:00), 31 from 16:00, 32 from
		/// 17:00, 33/34 game from 18:00 (34 = 1-in-10 cheer), 35 from 21:00
		/// (crowd leaving), back to 10 at close (23:00) with the income.
		int stadiumStatus(int previous, int minute, int open, int close, bool season, int roll);

		/// T2Baseball (Tokyo.t2p 0x100118d0..): grade 4 only, IsBeginDay
		/// 0x10011ac0 = not done, stadium season, a stadium exists; it begins
		/// at exactly 20:00 and goes on once the stadium has >= 200 visitors
		/// today (Exec 0x10011c40 state 1).
		const int kBaseballMinute = 1200;
		const int kBaseballVisitors = 200;
		bool baseballBeginDay(bool done, int dayOfYear, bool hasStadium);

		/// T2Wedding (Hawai.t2p 0x10014ed0..): IsBeginDay 0x10014f90 = not
		/// done, holiday, a Cathedral exists; Exec 0x10015120 needs the
		/// Cathedral open with >= 30 visitors today and a couple.
		const int kWeddingVisitors = 30;
		bool weddingBeginDay(bool done, bool holiday, bool hasCathedral);
		bool weddingReady(bool cathedralOpen, int visitorsToday, bool couple);
		/// The STRTBL 1 announcement, assembled as the original does (the
		/// "Mr." lands on the first resident).
		std::string weddingAnnouncement(const std::string & roomA, const std::string & nameA,
		                                const std::string & roomB, const std::string & nameB);

		/// Cathedral.t2p: opens on holidays only, 11:00 (TNDF 660) to 17:00.
		bool cathedralOpen(bool holiday, int minute, int open, int close);

		/// Attraction.t2p ChangeStatusOnIdle (0x1000c290): closed is pattern
		/// 0; open it is status 30/31/32 for 0, < 40, >= 40 visitors inside,
		/// drawn as pattern status - 29 (1 / 2 / 3). It renovates once
		/// grade < 5 and today's visitors reach 200.
		const int kAttractionRenovateVisitors = 200;
		int attractionPattern(bool open, int inside);
		bool attractionRenovates(int grade, int visitorsToday, bool renovated);

		// ================================================= subway (I7)

		/// MREV (FUN_1003d690) trains with T2Transport::UpGrade 0x1006bb80:
		/// level L enables train types < L (levels 1..4 run 4/8/10/12
		/// trains); a train stops at the station for 8 minutes from its
		/// arrival minute. Returns the enabled arrival minutes, sorted.
		const int kSubwayMaxLevel = 4;
		const int kSubwayStopMinutes = 8;
		std::vector<int> subwayArrivals(const std::vector<std::pair<int, int> > & trains, int level);
		/// Index of the train stopped at `minute` in `arrivals`, or -1.
		int subwayStoppedTrain(const std::vector<int> & arrivals, int minute);
		/// Type (MREV track 0..3) of the enabled train stopped at `minute`,
		/// or -1; the world's 8000 `MET1`..`MET4` sprite is drawn for it.
		int subwayStoppedTrainType(const std::vector<std::pair<int, int> > & trains, int level, int minute);
		/// A train on the track row: MREV track (MET1..MET4) and left unit.
		struct SubwayTrainPos { int track; int unit; };
		/// T2MetroRailway FUN_1003dc80: every MREV train runs every day,
		/// starting worldWidth / 4 minutes before its arrival at offset
		/// -worldWidth and moving right one unit a tick (4 a minute); one
		/// whose track < level stops at the station for 8 minutes; it is
		/// gone once its left edge reaches worldWidth. `stationUnit` is the
		/// Subway's left unit (worldWidth / 2 with none), `tick` 0..3.
		std::vector<SubwayTrainPos> subwayTrains(const std::vector<std::pair<int, int> > & trains, int level,
		                                         int worldWidth, int stationUnit, int minute, int tick);
		/// Subway.t2p FUN_1000d1e0/d280: pattern 0 closed, 1 empty,
		/// 2 occupied, 3 at half capacity or more.
		int subwayPattern(bool open, int inside, int capacity);
		/// Subway.t2p FUN_1000d330: open from TNDF open (06:30) until the
		/// close minute (23:59), when everyone is pushed out.
		bool subwayOpen(int minute, int open, int close);

		// ================================================= sky bridge (M8)

		/// SkyBrdge.t2p AreaCheck FUN_1000bcc0, reduced to its inputs: the
		/// bridge row is at or above 5F (floor >= 4 in OpenSky numbering),
		/// there is a building on both sides of the gap within the build
		/// area, the gap is at most 72 units, and the three floors above and
		/// below are empty across it. Returns a BuildErr-style code (0 = ok).
		const int kSkyBridgeMinFloor = 4;
		const int kSkyBridgeMaxGap = 72;
		const int kSkyBridgeClearFloors = 3;
		enum class SkyBridgeError { None = 0, TooLow, NotBetweenTowers, TooLong, NotClear };
		SkyBridgeError skyBridgeRule(int floor, bool leftTower, bool rightTower, int gap, bool clearAboveBelow);

		// ================================================= clouds (V6)

		/// T2Cloud (Tokyo.t2p 0x10012bd0, Exec 0x100130b0): the height band
		/// of a new cloud, y floors above ground = G - floor(sqrt(roll % G^2))
		/// with G = groundLine - 25 (the world's sky rows above the city).
		int cloudFloor(int G, int roll);
		const int kCloudCount = 10;
		const int kCloudStepMinutes = 10;
	}
}
