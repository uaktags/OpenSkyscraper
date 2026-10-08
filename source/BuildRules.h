/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include <climits>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "YootEvents.h"
#include "YootFacilities.h"

namespace OT {

struct YootWorld;

/**
 * Per-ruleset game constants that differ between SimTower, OpenSky and the
 * YootTower worlds: start funds, build bounds, floor limits, lobby height,
 * grade table, calendar hooks and unlocks. SimTower/OpenSky profiles carry
 * today's constants so those rulesets stay bit-identical; the YootTower
 * profile is filled from a World plugin (YootWorld) or, without one, from
 * the built-in Tokyo constants.
 *
 * Coordinates: OpenSky x cells map to YootTower world units with
 * unit = x + unitOriginX (origin = world width / 2, so the world centre is
 * at x = 0 where the camera starts). OpenSky floor f (0 = 1F) maps to world
 * row groundLine - 1 - f (T2FloorInfo::UnitToFloor).
 */
struct RulesProfile
{
	enum class Calendar { SimTower, YootTower };
	enum class Rain { Random, YootTokyo, Never };

	/// YootTower grade row: promotion from grade (index+1) needs both.
	struct Grade
	{
		uint32_t vipFlag = 0;
		int population = 0;
	};

	/// VIP visit (VIEV) definition, reduced to what the rules need.
	struct Vip
	{
		int grade = 0;
		uint32_t vipFlag = 0;
		uint32_t destPluginId = 0;
		std::vector<std::string> destPrototypes; ///< OpenSky counterparts; empty = none
		int beginMinute = 600;   ///< VIEV header beginMin (reservation)
		int arrivalMinute = 900; ///< VIEV +0x38 (lobby arrival)
		std::string goodMsg, badMsg;
	};

	/// Per-prototype unlock (YootTower ATTR mGameLevel + world mask).
	struct Unlock
	{
		bool available = true;
		int minRating = 0; ///< zero-based OpenSky rating (= mGameLevel - 1)
	};

	bool yootWorld = false;       ///< true for a YootTower world profile
	std::string worldId;          ///< "tokyo" / "hawaii" / "kegon" ("" otherwise)
	std::string worldName;
	uint32_t worldMask = 0;

	int startFunds = 2000000;     ///< dollars
	int startRating = 0;          ///< zero-based (displayed stars - 1)
	int startDay = 0;             ///< absolute day index of a new tower's clock
	int lobbyHeight = 1;          ///< default lobby height (no modifier keys)
	int lowestFloor = -9;         ///< inclusive (B9 in SimTower/OpenSky)
	int highestFloor = INT_MAX;   ///< inclusive

	bool hasBuildArea = false;    ///< YootTower WORLDDEF build-area RECT
	int buildMinX = INT_MIN;      ///< OpenSky x, inclusive
	int buildMaxX = INT_MAX;      ///< OpenSky x, exclusive
	int unitOriginX = 0;
	int groundRow = 0;
	int buildTopRow = INT_MIN;    ///< build-area top row (inclusive)
	int buildBottomRow = INT_MAX; ///< build-area bottom row (exclusive)

	/// FINF mask: 1 = unbuildable world unit (row-major, maskWidth wide).
	std::vector<uint8_t> unbuildable;
	int maskWidth = 0;
	int maskHeight = 0;

	Calendar calendar = Calendar::SimTower;
	Rain rain = Rain::Random;
	bool linearClock = false;     ///< YootTower minute-linear day (T1)

	std::vector<Grade> grades;    ///< empty = SimTower LevelUp table
	std::vector<Vip> vips;
	std::map<std::string, Unlock> unlocks; ///< absent = SimTower lock table

	/// T2EV scenario events (VIEV excluded); empty = SimTower calendar.
	std::vector<YootEventSpec> events;
	int worldWidth = 0;           ///< WORLDDEF width in units (Santa's begin minute)

	/// Trash / restroom / cold rates (WORLDDEF +0x170.., CODF; plan step 12).
	YootFacilityRules facilities;

	int unitForX(int x) const { return x + unitOriginX; }
	int rowForFloor(int floor) const { return groundRow - 1 - floor; }
	/// One cell (x, floor) against bounds, floor limits and the FINF mask.
	bool allowsCell(int x, int floor) const;
	/// Build area and FINF mask only, ignoring the floor limits.
	bool allowsBuildAreaCell(int x, int floor) const;

	static RulesProfile simTower();
	static RulesProfile openSky();
	/// Profile for a YootTower world (WORLDDEF/WATR/FINF/VIEV).
	static RulesProfile yootTower(const YootWorld & world);
	/// Stub used when no world was supplied: built-in Tokyo constants.
	static RulesProfile yootTowerStub();
};

/**
 * Pure placement policy for a named tower ruleset.
 *
 * Horizontal footprints are [x, x + width). SimTower has its fixed 375-cell
 * lot; OpenSky is unbounded; YootTower uses its world's build area, floor
 * limits and FINF mask (via RulesProfile).
 */
class BuildRules
{
public:
	enum class Ruleset
	{
		SimTower,
		OpenSky,
		YootTower
	};

	/// Value returned by minRatingToBuild for items absent from a world.
	static const int kUnavailable = 1000;

	explicit BuildRules(Ruleset ruleset);
	BuildRules(Ruleset ruleset, const RulesProfile & profile);

	Ruleset ruleset() const;
	const RulesProfile & profile() const { return m_profile; }

	/// Stable, parseable identifier suitable for persistence and configuration.
	const char * identifier() const;
	const char * displayName() const;
	bool hasFixedHorizontalBoundary() const;

	/// Returns false for nonpositive or non-representable [x, x + width) spans.
	bool allowsHorizontalFootprint(int x, int width) const;

	/// Horizontal check plus floor limits and (YootTower) FINF mask for the
	/// rectangle [x, x + width) x [floor, floor + height).
	bool allowsFootprint(int x, int floor, int width, int height = 1) const;

	/// YootTower items placed outside the floor area but inside the
	/// WORLDDEF build area: a final item stands on the top floor and rises
	/// above it (Stadium/Cathedral/Attraction AreaCheck), the subway sits
	/// one row below the lowest floor (Subway.t2p AreaCheck). Checks the
	/// build-area rows and the FINF mask, not the floor limits.
	bool allowsSpecialFootprint(int x, int floor, int width, int height) const;

	/// Prototypes that exist only in the YootTower ruleset (plugin-backed
	/// native items) and the SimTower facilities they replace there.
	static bool isYootOnlyPrototype(const std::string & prototypeId);
	static bool isReplacedInYoot(const std::string & prototypeId);

	/// Minimum zero-based rating to build `prototypeId`; kUnavailable when
	/// the active world does not offer it at all.
	int minRatingToBuild(const std::string & prototypeId) const;

	/// Parses only the stable identifiers returned by identifier().
	static bool parseIdentifier(const std::string & identifier, Ruleset & ruleset);

private:
	Ruleset m_ruleset;
	RulesProfile m_profile;
};

} // namespace OT
