/* Copyright (c) 2026 OpenSkyscraper Project */
///
/// YootTower World plugins (Tokyo.t2p, Hawai.t2p, Kegon.t2p).
///
/// Parses the game-rule resources of a world plugin into a plain struct:
/// WORLDDEF (id 1), WATR (id 128), T2EV (id 1) plus every event resource
/// it names, FINF (id 1, Kegon only) and the world's own ATTR mask. Field
/// order follows `T2WorldDef::T2WorldDef` (T2DLL 0x1006e4e0) and the event
/// constructors; see doc/yoottower/world-and-sp-formats.md §1-2 and
/// tools/yoottower/rules/world_rules.py (the Python reference port).
/// Unknown/discarded fields are kept so nothing is lost.
///
/// Binary CResFile resources start with 0x01, then every scalar (even a
/// char/short) is a 4-byte LE int, strings are <u8 len><bytes><NUL> and a
/// RECT is stored top, left, bottom, right (CResFile::operator>>).
///
/// Pure module: no Game dependency, linked into OpenSkyTests.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace OT
{
	class AbstractPlugin;
	class PluginManager;

	struct YootRect
	{
		int left = 0, top = 0, right = 0, bottom = 0; ///< right/bottom exclusive (Win32 RECT)
	};

	/// One WORLDDEF grade row (6 ints). Row L-1 is used when upgrading from grade L.
	struct YootGradeRow
	{
		uint32_t vipFlag = 0;    ///< CheckGameLevel: (doc+0xa0 & vipFlag) == vipFlag
		int population = 0;      ///< promotion needs population >= value
		int animId = 0;          ///< grade-up animation bitmap (1207..1210)
		int animFrames = 0;
		int unk = 0;
		int animParam = 0;
	};

	/// Common 5-int event header (T2EventItem::T2EventItem 0x10030050).
	struct YootEventHeader
	{
		int levelBit = 0;  ///< grade mask, bit g-1
		int exclusive = 0;
		int cycleDay = 0;  ///< IsBeginDay: d % cycleDay == 0
		int beginMin = 0;  ///< minute of day
		int endMin = 0;
	};

	/// One VIEV entry (T2VisitVIP::Init 0x1006d360).
	struct YootVipDef
	{
		int grade = 0;
		uint32_t destPluginId = 0; ///< destination plugin mID (ATTR field 0, LE u32)
		int arrivalMin = 0;        ///< +0x38: lobby arrival minute ("arriving at 3:00 p.m.")
		int f3c = 0, f40 = 0, f44 = 0;
		int viplRes = 0;
		std::string goodMsg, badMsg;
		uint32_t vipFlag = 0;      ///< derived like Init: first free bit of 1<<(4(g-1))
	};

	/// One T2EV entry plus its decoded `*EV` resource.
	struct YootEvent
	{
		uint32_t typeTag = 0;      ///< raw LE int from T2EV
		std::string resType;       ///< resource type name, e.g. "FBEV"
		int resId = 0;
		bool found = false;        ///< resource present and header parsed
		YootEventHeader header;
		std::vector<int> body;     ///< remaining ints (non-VIEV events)
		std::string text;          ///< MAEV trailing pstr (T2Maru_Reggae ctor 0x1003a770)
		std::string bodyTail;      ///< bytes after the last whole int (MAEV message); kept for encodeYootEventResource
		std::vector<YootVipDef> vips; ///< VIEV only
	};

	struct YootWorld
	{
		enum class Id { Tokyo, Hawaii, Kegon };

		Id id = Id::Tokyo;
		bool fromPlugin = false;   ///< false = built-in fallback constants
		std::string pluginName;    ///< e.g. "Tokyo.t2p"

		// --- WORLDDEF ---
		int sortKey = 0;
		int height = 0;            ///< world rows
		int width = 0;             ///< world units
		int groundLine = 0;        ///< row ground-1 is 1F, row ground is B1
		YootRect buildArea;
		int entranceX = 0, entranceY = 0;
		int topFloorLine = 0;      ///< highest floor row (inclusive)
		int bottomFloorLine = 0;   ///< floor area bottom (exclusive)
		int startGrade = 1;        ///< doc+0x8c at new game
		int startMonth = 0;        ///< +0x74: new-game T2DateTime month (T2.exe doc init)
		int lobbyHeight = 1;
		int outViewKind = 0;
		int unk17c = 0;
		int tiledBkgnd = 0;
		std::vector<YootGradeRow> grades;
		int bkgndTileHeight = 0, bkgndTileWidth = 0, unk188 = 0, unk18c = 0;
		std::vector<int> discarded10;
		std::vector<int> gradeIconAnimIds;
		std::string towerNamePrefix;
		int dustOnAt = 0, toiletAt = 0, toiletDenomi = 0, discarded9999 = 0;
		std::vector<int> cashAmounts;

		// --- WATR ---
		std::vector<int> watrInts; ///< 6 ints discarded by the engine ([3]=floors above, [4]=below)
		int initialFunds = 0;      ///< internal units (x exchangeMul / exchangeDiv = dollars)
		int exchangeMul = 100, exchangeDiv = 1;
		std::string moneyUnit;

		// --- ATTR (world plugin) ---
		uint32_t worldMask = 0;    ///< Tokyo 0x7, Hawaii 0xF01, Kegon 0xF0

		// --- T2EV ---
		std::vector<YootEvent> events;

		// --- FINF (Kegon) ---
		std::vector<int> finfHeader;   ///< 7 ints: height, width, unk, RECT
		int finfWidth = 0, finfHeight = 0;
		std::vector<uint8_t> unbuildable; ///< row-major, 1 = unit flag 0x8000 (not buildable)

		/// Start funds in dollars (T2WorldDef::ExchangeMoney).
		long long initialFundsDollars() const;
		/// Floor-area derived limits in OpenSky floor numbers (0 = 1F).
		int highestFloor() const { return groundLine - 1 - topFloorLine; }
		int lowestFloor() const { return groundLine - bottomFloorLine; }
		/// T2UnitInfo::IsBuildable for one unit; outside the mask = buildable.
		bool isUnitBuildable(int unit, int row) const;
		/// First event with the given resource type (e.g. "VIEV"), or null.
		const YootEvent * findEvent(const std::string & resType) const;
	};

	const char * yootWorldIdentifier(YootWorld::Id id);     ///< "tokyo" / "hawaii" / "kegon"
	const char * yootWorldDisplayName(YootWorld::Id id);    ///< "Tokyo" / "Hawaii" / "Kegon"
	const char * yootWorldPluginFile(YootWorld::Id id);     ///< "Tokyo.t2p" / "Hawai.t2p" / "Kegon.t2p"
	bool parseYootWorldIdentifier(const std::string & s, YootWorld::Id & out);

	// --- Resource parsers (pure, over raw bytes) ---
	bool parseYootWorldDef(const char * data, int length, YootWorld & out);
	bool parseYootWatr(const char * data, int length, YootWorld & out);
	bool parseYootFinf(const char * data, int length, YootWorld & out);
	bool parseYootEventResource(const std::string & resType, const char * data, int length, YootEvent & out);
	/// Derives each VIP's flag exactly like T2VisitVIP::Init (shared mask accumulator).
	void assignYootVipFlags(std::vector<YootVipDef> & vips);

	// --- Resource encoders (inverse of the parsers above) ---
	// Each writes the bytes its parser reads, so parse + encode is
	// byte-identical for the shipped worlds (tests/test_main.cpp
	// testYootWorldEncoders). The lossless editable form of every world
	// resource is WorldResourceCodec.h; these keep the game's typed view
	// writable. Return false when a string exceeds 255 bytes or a grid does
	// not match its header.
	bool encodeYootWorldDef(const YootWorld & w, std::string & out);          ///< WORLDDEF
	bool encodeYootWatr(const YootWorld & w, std::string & out);              ///< WATR
	/// FINF text: the 7-int header, then the mask as 0/1, one per CRLF line.
	bool encodeYootFinf(const YootWorld & w, std::string & out);
	/// One `*EV` resource: header, then the VIP list (VIEV) or the raw body ints.
	bool encodeYootEventResource(const YootEvent & e, std::string & out);
	/// T2EV: count, then {typeTag, resId} per event.
	bool encodeYootTowerEvents(const std::vector<YootEvent> & events, std::string & out);

	/// Loads every rule resource from a world plugin. Returns false when
	/// WORLDDEF or WATR is missing/garbled (`error` says which).
	bool loadYootWorld(const AbstractPlugin & plugin, YootWorld::Id id, YootWorld & out, std::string * error = nullptr);
	/// Looks the world plugin up in the catalog; falls back to builtinYootWorld().
	YootWorld loadYootWorld(const PluginManager & plugins, YootWorld::Id id, std::string * error = nullptr);
	/// Constants decoded from the shipped plugins (doc/yoottower/world-and-sp-formats.md
	/// §2.2-2.3) for running without the .t2p. No FINF mask and no events.
	YootWorld builtinYootWorld(YootWorld::Id id);

	/// Plugin identity from the ATTR resource
	/// (CFilePlugin ctor 0x1000df20 / T2PluginSpecifier::Initialize 0x1004fd00):
	/// mID, mAttrib, mDependID, mGameLevel, mSecretItem, mItemType, mMustItem, name.
	/// Decoded with parsePluginAttr; 4ccs are the LE u32 (== Mac constant).
	struct YootPluginIdentity
	{
		uint32_t id = 0;
		uint32_t attrib = 0;   ///< world-availability mask
		uint32_t dependId = 0;
		int gameLevel = 0;
		int secretItem = 0;
		int itemType = 0;
		int mustItem = 0;
		std::string name;
	};
	bool readYootPluginIdentity(const char * data, int length, YootPluginIdentity & out);
	bool readYootPluginIdentity(const AbstractPlugin & plugin, YootPluginIdentity & out);

	/// T2.exe FUN_0041ca80 toolbox filter for one plugin.
	bool yootPluginAvailable(const YootPluginIdentity & p, uint32_t worldMask, int grade, bool secretMode = false);

	/// OpenSky prototype -> YootTower plugin counterparts.
	struct YootPrototypeLink
	{
		const char * prototypeId;
		const char * plugins[5];   ///< plugin files; null-terminated
	};
	const std::vector<YootPrototypeLink> & yootPrototypeLinks();

	/// Per-prototype unlock resolved against a world.
	struct YootUnlock
	{
		bool available = false;  ///< some counterpart exists in this world
		int minGrade = 0;        ///< lowest mGameLevel among available counterparts (1-based)
	};
	/// Resolves yootPrototypeLinks() through the catalog's ATTR data.
	/// Prototypes whose counterparts are all missing from the catalog are
	/// left out of the map (caller falls back to the SimTower lock table).
	std::map<std::string, YootUnlock> resolveYootUnlocks(const PluginManager & plugins, uint32_t worldMask);
	/// Maps a VIEV destination plugin mID to OpenSky prototype ids ("" when none).
	std::vector<std::string> yootPrototypesForPluginId(const PluginManager & plugins, uint32_t pluginId);
}
