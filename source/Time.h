#pragma once
#include <cmath>

namespace OT {
	class Game;
	
	/** Manages the game's time progression and scaling.
	 * The following table lists a basic absolute time to hour conversion:
	 * 
	 *  time  absolute
	 *  ----  --------
	 *  0:00  0        0.000
	 *  1:00  1/26    ~0.039
	 *  2:00  2/39    ~0.051
	 *  3:00  5/78    ~0.064
	 *  4:00  1/13    ~0.077
	 *  5:00  7/78    ~0.090
	 *  6:00  4/39    ~0.103
	 *  7:00  3/26    ~0.115
	 *  8:00  19/130  ~0.146
	 *  9:00  23/130  ~0.177
	 * 10:00  27/130  ~0.208
	 * 11:00  31/130  ~0.238
	 * 12:00  7/26    ~0.269
	 * 13:00  15/26   ~0.577
	 * 14:00  8/13    ~0.615
	 * 15:00  17/26   ~0.654
	 * 16:00  9/13    ~0.692
	 * 17:00  19/26   ~0.731
	 * 18:00  10/13   ~0.769
	 * 19:00  21/26   ~0.808
	 * 20:00  11/13   ~0.846
	 * 21:00  23/26   ~0.885
	 * 22:00  12/13   ~0.923
	 * 23:00  25/26   ~0.962
	 *  0:00  1        1
	 */
	class Time
	{
		friend class Game;
		
	public:
		/// Which day-kind rules the calendar follows. SimTower: every third
		/// day (day % 3 == 2) is the weekend. YootTower: the world plugins'
		/// IsHoliday (game-parity.md T4).
		enum class Calendar { SimTower, YootTower };

		/// SimTower EXE frames; ft 0 is 07:00 (our stage 300).
		static const int kSimFramesPerDay = 2600;
		int eventFrame() const;
		double eventFramesSince(double previousAbsolute) const;
		/// Jump forward to an EXE frame later today; no-op if it has already passed.
		void advanceToEventFrame(int frame);

		const static double kBaseSpeed;
		static double absoluteToHour(double a);
		static double hourToAbsolute(double a);

		/// YootTower minute-linear clock (T2DateTime, game-parity.md T1):
		/// when enabled, absolute day fractions map linearly to hours instead
		/// of SimTower's 2600-frame table. Process-wide because the
		/// conversions are static; Game sets it from the active ruleset.
		static void setLinearClock(bool linear);
		static bool linearClock();

		/// Tokyo.t2p 0x10014c10 / Hawai.t2p 0x10017770 / Kegon.t2p
		/// WorldDef::IsHoliday 0x10014e70 (identical): `month` is the
		/// internal day index 0..11 within the year, `year` is 1-based.
		static bool yootIsHoliday(unsigned year, unsigned month);
		/// Tokyo.t2p IsRainyDay 0x10014c50: days 4 and 8 of each year.
		/// Hawaii and Kegon keep the base T2WorldDef::IsRainyDay (never).
		static bool yootTokyoIsRainy(unsigned month);
		
		Time();
		
		double absolute;
		double dta; //absolute time delta
		double hour;
		int day;
		int quarter;
		int year;
		
		Calendar calendar;

		/// Day index within the year (0..11); one YootTower "month".
		int dayOfYear() const;
		/// Weekend/holiday for the active calendar. SimTower: day == 2.
		bool isHoliday() const;

		double getHour();
		bool check(double a);
		bool checkTick(double p);
		/// Number of p-sized tick boundaries crossed by the last advance.
		long ticksCrossed(double p);
		bool checkHour(double h);

	protected:
		void set(double a);
		void advance(double dt);

		double prev_absolute;
		double prev_hour;
		int prev_day;
		int prev_quarter;
		int prev_year;

		double speed;
		double speed_animated;
	};
}
