/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once

#include <string>

namespace OT
{
	class Game;

	/// Deterministic headless simulation harness (`--sim`). Advances a
	/// loaded or freshly seeded tower with a fixed timestep and no
	/// rendering, then writes the final save plus a compact JSON summary.
	/// See doc/headless-sim.md.
	namespace Sim
	{
		struct Options
		{
			bool enabled = false;
			/// Run length: `ticks` fixed steps, or (when `days` > 0) until
			/// the clock has advanced that many game days.
			long ticks = 0;
			double days = 0;
			unsigned seed = 1;
			/// Speed mode while simulating (0..4, as the toolbox; 3 = 12x).
			int speed = 3;
			/// Fixed timestep in seconds of real time per tick.
			double dt = 1.0 / 60.0;
			std::string outPath;
			std::string summaryPath; ///< default: outPath + ".json"
			long dumpEvery = 0;      ///< ticks between snapshots (0 = off)
			std::string dumpDir;
			int choice = -1;         ///< --sim-choice N: the multi-button event dialog button to press (fire: 0 helicopter, 1 security, 2 leave)
			std::string carTrace;    ///< --sim-car-trace FILE: one line per car state change (game minute, car, altitude, state, doors, riders, gear)
		};

		/// True while a deterministic run is active. Gates the few
		/// wall-clock dependencies of the sim (route-build time budget,
		/// autosave throttle) so normal play is unchanged.
		bool deterministic();
		void setDeterministic(bool on);

		/// Parses one `--sim*` argument at argv[i] (advancing i past its
		/// values). Returns false for arguments that are not sim flags.
		bool parseArg(int argc, char * argv[], int & i, Options & options);

		/// Runs the configured simulation on `game`; returns a process
		/// exit code (0 = success).
		int run(Game & game, const Options & options);

		/// Compact JSON summary of the tower state (funds, population,
		/// rating, date, item counts, recorded event log).
		std::string summaryJson(const Game & game, const Options & options, long ticksRun);
	}
}
