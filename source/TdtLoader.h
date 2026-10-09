/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OT {
namespace Tdt {

	/**
	 * Importer for the original 1994 SimTower .TDT save format.
	 *
	 * Layout facts: doc/simtower/TDT_format.txt (jcranmer's reverse engineering),
	 * cross-checked against maniator/verticopolis's harness-verified constants
	 * (their tools/simtower round-trip harness measured the elevator payload
	 * arithmetic and the 18-byte tenant record against the retail game).
	 *
	 * This file is pure logic: no SFML, no Game. It parses the binary into
	 * TdtData and emits our native <tower> XML (the schema Game::encodeXML
	 * writes), so the regular Application::loadGameFromFile path can decode
	 * it unchanged. Anything uncertain degrades to a warning, never a crash.
	 */

	struct Tenant
	{
		int left;        ///< left extent, 8px tiles from lot left (half-open)
		int right;       ///< right extent (exclusive)
		int type;        ///< tenant type id; negative = under construction
		int status;      ///< occupancy/flags byte
		int variant;     ///< byte 6: retail variety (fast food/restaurant; shops unused by us)
		int rentClass;   ///< byte 16: 0 very low .. 3 high, 4 no rate (not imported)
	};

	struct FloorRec
	{
		int index;       ///< TDT floor index 0..119 (0-9 = B10..B1, 10 = ground)
		int leftEdge;    ///< built extent in tiles (half-open)
		int rightEdge;
		std::vector<Tenant> tenants;
	};

	struct ElevatorRec
	{
		int type;        ///< 0 = express, 1 = standard, 2 = service
		int cars;        ///< 1..8
		int x;           ///< left tile
		int topFloor;    ///< TDT floor indexes (top >= bottom)
		int bottomFloor;
		std::vector<uint8_t> serviced; ///< 120 entries, nonzero = cars stop there
		std::vector<int> carHomes;     ///< home floor per car slot (TDT indexes)
	};

	struct StairRec
	{
		int type;        ///< 0 escalator, 1 stairs, 2/3 two-story, 4/5 three-story
		int x;           ///< left tile
		int floor;       ///< base floor (TDT index)
	};

	struct TdtData
	{
		int level;       ///< 1..5 stars, 6 = TOWER
		int32_t balance;     ///< stored funds (display dollars are x100)
		int frameTime;   ///< 0..2599 frames; 0 = 7:00 AM
		int currentDay;  ///< EXE calendar days since WD 1 / Q 1 / Year 1
		int population = 0; ///< serialized CountT total (not the live people count)
		int countT[10] = {0};
		bool populationValid = false;
		int viewX;       ///< saved view scroll, world px (0 = none stored)
		int viewY;
		std::vector<FloorRec> floors;
		std::vector<ElevatorRec> elevators;
		bool elevatorsValid;          ///< false = elevator table unreadable (warning issued)
		std::vector<StairRec> stairs;
		bool stairsValid;
		std::vector<std::string> warnings;
	};

	/// Quick sniff: does this path point at something with the .TDT magic (0x2400)?
	bool looksLikeTdt(const std::string & path);

	/// Parse a .TDT file. Returns false with a human-readable error; tolerable
	/// damage lands in out.warnings instead.
	bool loadFile(const std::string & path, TdtData & out, std::string & error);

	/// Convert parsed data into our native <tower> XML (Game::decodeXML schema).
	/// Fidelity notes land in report (aggregated, player-readable).
	std::string toTowerXml(const TdtData & in, std::vector<std::string> & report);

}
}
