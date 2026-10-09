/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower odds and ends closed after plan step 14: the School, the Sky
/// Bridge placement and split, the cosmetic Tonbi / Dolphin flights, Mr.
/// Maru's visit, the Kegon hot spring, the traffic map behind the noise
/// term and the per-cell floor charge (game-parity.md S2, M8, V4, V6, J2,
/// E2).
///
/// Verbatim ports of the plugin / T2DLL.dll functions named on each
/// declaration, kept pure so they can be unit-tested without a Game.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace OT
{
	struct YootEventSpec;

	namespace Yoot
	{
		// ================================================= School (S2)

		/// School.t2p (ATTR LOCS, grade 4, Tokyo only): TNDF 38x2, $1,000,000,
		/// $50,000/Q, power 100, 60 students from the pool (TMDF 1, econo 1),
		/// open 08:00, close 18:00, no income, no sounds. Status machine of
		/// ChangeStatusOnIdle 0x1000bf30 plus EnterTenant 0x1000c1a0 /
		/// LeaveTenant 0x1000c240. Patterns: 0 closed or empty, 1 some
		/// students, 2 class in session, 3 half full (break / dismissal).
		struct SchoolModel
		{
			enum Status { Built = 9, Hiring = 10, Closed = 11, Some = 12, InClass = 13, Half = 14 };
			int status = Built;
			int pattern = 0;
			int inside = 0;
			int capacity = 60;
			/// Absolute minutes at which called students arrive
			/// (CallEmployee 0x100612a0: now + (rand % 6) * 10).
			std::vector<long long> arrivals;

			/// One tenant idle (every 8 game minutes). `reachable` stands for
			/// the pool route check of CollectEmployee / CallEmployee.
			void idle(long long now, int minuteOfDay, int open, int close, bool holiday,
			          bool reachable, const std::function<int()> & rnd);
			/// Students due by `now` walk in (EnterTenant).
			void arrive(long long now);
			void enter();
			void leave();
			/// PushOutPeople: up to n students go home.
			void pushOut(int n);
		};

		// ================================================= Sky Bridge (M8)

		/// SkyBrdge.t2p TNDF: 1 row high, default 8 units, $5,000 flat
		/// (attr 0x45980 has no 0x2000, so CalcPayment does not charge per
		/// cell), $1,000 per unit per quarter (attr 0x4000), never routed
		/// (no IsFloor).
		struct BuildRect { int left = 0, top = 0, right = 0, bottom = 0; };
		/// SkyBrdge.t2p AreaCheck FUN_1000bcc0 over a [left,right) x
		/// [top,bottom) unit/row rect: refuses a bottom row below
		/// groundLine - 4 (#62), snaps left/right to the nearest tenant cells
		/// on the bottom row inside the build area (#63 when either side has
		/// none), needs 3 empty rows below and above the gap inside the
		/// build area (#59) and a gap of at most 72 units (#60). Returns 0 or
		/// the STRTABLE 9999 error; `rect` is snapped even on errors after
		/// the snap. `occupied(row, unit)` is T2FloorInfo::GetTenant != 0.
		int skyBridgeAreaCheck(BuildRect & rect, const BuildRect & buildArea, int groundLine,
		                       const std::function<bool(int, int)> & occupied);
		/// FUN_1000c0b0 (called from the Floor BuildFinish hook): after
		/// cells of a bridge are taken over, the bridge keeps the runs of
		/// [left,right) that are still free; no run removes it.
		std::vector<std::pair<int, int> > skyBridgeSplit(int left, int right,
		                                                 const std::function<bool(int)> & taken);

		// ============================================ Tonbi / Dolphin (V6)

		/// TOEV (Kegon black kite) / DOEV (Hawaii dolphins) body, read by the
		/// T2Tonbi ctor (Kegon.t2p 0x10013030; T2Dolphin Hawai.t2p 0x10013e10
		/// calls it): layer, n points as (row, unit), n x {pict, count, dy,
		/// dx}. pict 0 loops items dy..dx (1-based) `count` times.
		struct TonbiAnim { int pict = 0, count = 0, dy = 0, dx = 0; };
		struct TonbiDef
		{
			int layer = 1;
			std::vector<std::pair<int, int> > points; ///< (row, unit)
			std::vector<TonbiAnim> anims;
			int minPict = 0;  ///< bitmap id (smallest non-zero pict)
			int maxPict = 0;
		};
		bool decodeYootTonbi(const YootEventSpec & e, TonbiDef & out);
		/// The shipped scripts, for running without the world plugin.
		TonbiDef builtinYootTonbi(bool dolphin);

		/// T2Tonbi::Start / Exec / TonbiAnim::Fly. Runs at 4 ticks per game
		/// minute: a flight is tried every 512 ticks and steps every 4.
		/// Positions are QuickDraw pixels of the world (x = unit * 8,
		/// y = row * 36 + 8 at the start point).
		class TonbiFlight
		{
		public:
			void start(const TonbiDef & def);
			/// One Exec tick. `ready` = T2Tonbi::IsReady (sky not rain/snow);
			/// `pick` chooses the start point (rand() % n). Returns true when
			/// the event stops (a flight ended after endMin).
			bool tick(int minuteOfDay, int endMin, bool ready, const std::function<int()> & pick);
			bool visible() const { return visible_; }
			int x() const { return x_; }
			int y() const { return y_; }
			int pattern() const { return pattern_; }
			int steps() const { return steps_; }

		private:
			bool fly(int index);
			const TonbiDef * def_ = nullptr;
			std::vector<std::pair<int, int> > state_; ///< per anim {n, cur}
			unsigned counter_ = 0;
			unsigned mask_ = 0x1ff;
			int idx_ = 0;
			bool visible_ = false;
			int x_ = 0, y_ = 0, pattern_ = 0, steps_ = 0;
		};

		// ================================================= Mr. Maru (V4)

		/// T2Maru_Reggae::MaruReception 0x1003aec0: per tenant he stands 30
		/// minutes, dances 8 minutes playing "MR_Maru" (WAVE 9003, only when
		/// on screen) and stands 30 more, then walks to the next random
		/// tenant until the event's end minute (23:00). No stress, income or
		/// message.
		const int kMaruStandMinutes = 30;
		const int kMaruDanceMinutes = 8;
		const int kMaruSoundId = 9003;
		/// Phase at `minutesAtTenant` after arriving: 0 standing, 1 dancing,
		/// 2 standing, 3 done (move on).
		int maruPhase(int minutesAtTenant);

		// ================================================= hot spring (V6)

		/// Kegon.t2p T2HotSpring::Exec 0x10012ad0: while not done, at a
		/// grade in the HSEV levelBit (4 = grade 3) and population >= the
		/// body's first int (100), the first new build whose top row is at
		/// or below the ground line (all of it underground) shows dialog
		/// 8700 and calls FUN_00422080(1), which sets doc+0x9c |= 1 and
		/// loads the mSecretItem plugins (the Kegon Spa).
		bool hotSpringTriggers(bool done, int levelBit, int grade, int population, int threshold,
		                       int topFloorOfBuild);
		const char * const kHotSpringMessage = "Spa was found underground.\nSpa can be placed.";

		// ================================================= traffic (J2)

		/// T2TrafficInfo (T2DLL 0x1006b4f0..): one cell per row and 4 units,
		/// T2.exe builds it with thresholds 100 / 500 / 1000 shifted >> 2.
		class TrafficMap
		{
		public:
			struct Cell { int count = 0; int accum = 0; int range = 0; };
			void configure(int left, int top, int right, int bottom);
			bool configured() const { return cols_ > 0; }
			/// Pass 0x1006b5d0: +1 in every cell from min(x) to max(x) on `row`
			/// (T2People::FindRoute, one call per route leg).
			void pass(int row, int unitA, int unitB);
			/// HourChanged 0x1006b640: accum = accum * 31/32 + count (cap
			/// 0xffff), count = 0, range recomputed.
			void hourChanged();
			/// GetRange 0x1006b7d0; 0 off the grid.
			int range(int row, int unit) const;
			static int calcRange(int accum);  ///< CalcRange 0x1006b740: 25/125/250
			const Cell * cell(int row, int unit) const;
			/// "count:accum" pairs, comma separated, row-major (save form).
			std::string encode() const;
			void decode(const std::string & s);
			int cols() const { return cols_; }
			int rows() const { return rows_; }

		private:
			int index(int row, int unit) const;  ///< CalcUnitIndex; <= 0 = none (GetUnit quirk)
			int left_ = 0, top_ = 0, cols_ = 0, rows_ = 0;
			std::vector<Cell> cells_;
		};

		// ================================================= age (J2)

		/// T2Equip +0x4c buildDay = CalcTotalDays at InitEquip; the estimate
		/// subtracts CalcLapseYears = (totalDays - buildDay) / 12.
		int yearsSinceBuilt(int totalDays, int buildDay);

		// ================================================= E2 floor charge

		/// T2TenantDef::CalcPayment 0x10062b10 for attr 0x2000 items (Floor,
		/// Lobby): price per covered cell that holds no tenant (existing
		/// floor is a tenant, so it is free).
		int floorCharge(int pricePerCell, int emptyCells);
	}
}
