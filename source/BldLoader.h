/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace OT {
namespace Bld {

	/**
	 * Importer for YootTower (The Tower II, Win32 1999) `.bld` tower saves.
	 *
	 * The format is one raw little-endian T2Archive stream with no chunk
	 * directory: doc/yoottower/bld-format.md. This is a C++ port of the
	 * reference codec tools/yoottower/bld.py. One set of structure
	 * functions drives both reading and writing (like bld.py's Codec), so
	 * writeBytes(parseBytes(x)) == x byte for byte. Every field is kept,
	 * including the unnamed ones (`fXX` = offset in the in-memory object)
	 * and the header of every unused slot.
	 *
	 * Parsing is strict: any marker, count or length that does not fit
	 * fails loudly with the offset and the structure path, exactly where
	 * bld.py would raise BldError.
	 *
	 * toTowerXml() converts the parsed save into our native <tower> XML in
	 * YootTower mode (Game::decodeXML schema), like TdtLoader does for
	 * SimTower. Pure logic: no SFML, no Game.
	 */

	/// QuickDraw Rect read big-endian top,left,bottom,right; stored l,t,r,b.
	/// Units are world units (x) and world rows (y), right/bottom exclusive.
	struct Rect
	{
		int left = 0, top = 0, right = 0, bottom = 0;
	};

	/// LArray::ReadAsWord (T2DLL 0x100134f0).
	struct LArrayWord
	{
		int32_t itemSize = 2;
		int32_t h2 = 0, h3 = 0;
		uint8_t h4 = 0, h5 = 0;
		int32_t dataBytes = 0;
		std::vector<uint16_t> items;
		std::vector<uint16_t> pad;
	};

	/// LArray::ReadAsChar (T2DLL 0x10013720).
	struct LArrayChar
	{
		int32_t itemSize = 1;
		int32_t h2 = 0, h3 = 0;
		int32_t total = 0;
		std::vector<uint8_t> items;
		std::vector<uint8_t> pad;
	};

	/// T2Object::LoadSelf 0x100453b0. Unused slots carry only this.
	struct ObjectHeader
	{
		uint8_t used = 0;
		int16_t status = 0;
		uint8_t favorite = 0;
	};

	/// T2CrossEquipArray: 'AEC_','AECT','AECR','AECF','AECM' + ids; 'AECx' = none.
	struct CEArray
	{
		std::string tag;
		LArrayWord ids;
	};

	/// T2Equip::LoadSelf 0x1002ef60 (tenants and movers).
	struct Equip
	{
		ObjectHeader hdr;
		uint16_t equipID = 0;
		Rect area;
		int16_t equipType = 0;     ///< plugin numeric itemType
		int16_t drawMode = 0;
		uint8_t patIndex = 0;
		int16_t f44 = 0;
		int16_t inMoneyIndex = 0;  ///< price selection (rent tier)
		int16_t f48 = 0;
		int32_t buildDay = 0;
		CEArray ce;
		uint32_t valiation = 0;
		std::vector<uint8_t> extra; ///< def LoadExtraData (s32 n + n bytes)
	};

	struct Tenant : Equip
	{
		uint8_t kindIndex = 0;
		int32_t resideDay = 0;
		int16_t f8c = 0;
		int16_t estimate = 0;      ///< 1..300 tenant evaluation (0 = none yet)
		uint8_t bind = 0;
		uint16_t relatedTenantID = 0;
		int16_t reserveCount = 0, belongCount = 0, customerCount = 0, workCount = 0, totalCustomer = 0;
		uint16_t tenantNumber = 0;
		uint32_t relatedTenantListID = 0;
		int16_t nMarks = 0;
		std::vector<uint8_t> marks;
		uint32_t firstPeople = 0;
		uint32_t people64 = 0;
		uint32_t hasBelongList = 0;
		std::vector<uint32_t> belongList;
	};

	/// T2MoverModule + T2ElevatorModule / T2StairModule.
	struct MoverModule
	{
		ObjectHeader hdr;
		int16_t position = 0, direction = 0, waitCounter = 0, patIndex = 0, numPeople = 0, f4c = 0;
		uint32_t firstPeople = 0, lastPeople = 0;
		// elevator car ('LMlE')
		int16_t index = 0, homePosition = 0, offsetPos = 0, nextStop = 0, f6c = 0, topTurn = 0, bottomTurn = 0;
		std::string stopTag;       ///< 'AfIS' = stop table follows
		LArrayChar stops;          ///< 2 bytes per shaft floor (meaning unknown)
		// stair module ('LMtS')
		Rect moduleRect;
	};

	/// T2MoverModuleList::ReadMMList 0x10041000.
	struct ModuleList
	{
		std::string kind;          ///< 'LMlE' elevator, 'LMtS' stair, 'LMM_' base, 'LMMx' none
		int16_t f20 = 0;
		Rect rect;
		std::vector<MoverModule> modules;
		Rect rect2;                ///< LMlE only
	};

	struct Mover : Equip
	{
		uint8_t showShaft = 0;
		int16_t length = 0;        ///< floors served by the shaft
		std::string reqIDTag;      ///< 'ADIR' = request id list follows
		LArrayWord reqIDs;         ///< 2 per floor from the bottom; 0 = no stop
		int16_t waitCount = 0, f6c = 0;
		ModuleList modules;
		int32_t refCon = 0;
	};

	/// T2Request::LoadSelf 0x10053660 (waiting area beside a stop).
	struct Request
	{
		ObjectHeader hdr;
		uint16_t requestID = 0, moverID = 0;
		uint8_t upDown = 0;
		int16_t f18 = 0, f1c = 0;
		int8_t moduleIndex = 0;
		int16_t drawWidth = 0;
		uint32_t people[4] = {0, 0, 0, 0};
	};

	/// T2OutObj::LoadSelf 0x100462e0 (no marker).
	struct OutObj
	{
		ObjectHeader hdr;
		uint16_t outObjID = 0;
		Rect area;
		uint32_t valiation = 0;
		int16_t outObjType = 0;
	};

	/// T2PeopleType::Read 0x1004e6c0.
	struct PeopleType
	{
		int8_t f0 = 0, silhouetteType = 0, econoType = 0, transportType = 0, life = 0;
	};

	/// T2People::LoadSelf 0x1004a810 (+ T2Matter::LoadSelf).
	struct People
	{
		ObjectHeader hdr;
		uint32_t matterID = 0;
		uint16_t workTenant = 0, homeTenant = 0, currEquipID = 0, destination = 0;
		int16_t curH = 0, curV = 0, dstH = 0, dstV = 0, startTime = 0;
		int8_t direction = 0, walkStyle = 0;
		std::string returnTag;     ///< 'KTSR' = return stack follows
		std::vector<std::pair<uint16_t, uint16_t> > returnStack; ///< (tenant, time)
		uint8_t specialFlag = 0, walk = 0;
		int16_t stress = 0;
		uint16_t f80 = 0;
		int16_t estimate = 0, color = 0, f68 = 0, prevTenantType = 0;
		int32_t style = 0;
		std::string typeTag;       ///< 'PYTP' = people type follows
		PeopleType type;
		int16_t silhouettes[3] = {0, 0, 0};
		uint32_t linkPrev = 0, linkNext = 0;
	};

	/// CPluginInfo::Read 0x10010720.
	struct PluginInfo
	{
		std::string type;          ///< DESC class, file order ('lPnT' tenant, 'lPvM' mover, ...)
		std::string id;            ///< plugin mID 4cc, file order ("ROLF" = 'FLOR')
		int16_t itemType = 0;      ///< numeric item type referenced by records
		int16_t mustItem = 0;
		std::string name;
		std::string fileName;      ///< absolute path on the saving machine
		uint8_t used = 0;
	};

	struct TypeArray
	{
		std::vector<PeopleType> types;
		uint32_t f24 = 0;
		int32_t f28 = 0, f2c = 0;
	};

	struct TimeZone
	{
		uint8_t fixed = 0;
		int8_t f28 = 0, currentItem = 0, f30 = 0;
		std::vector<TypeArray> typeArrays;
	};

	/// T2Pool (visitor pool + the population shown in the Information bar).
	struct Pool
	{
		int16_t gameLevel = 0;
		int32_t population = 0;
		int32_t f18 = 0;
		std::vector<TimeZone> timeZones;
	};

	struct DefCost
	{
		int16_t itemType = 0;
		int32_t amount = 0;
	};

	/// T2Settlement: money in $100 units.
	struct Settlement
	{
		int32_t funds = 0;
		int32_t previousFunds = 0;
		int32_t totals[5] = {0, 0, 0, 0, 0};
		std::vector<DefCost> tenantCosts, moverCosts, outObjCosts;
	};

	struct Blob4cc
	{
		std::string id;
		std::vector<uint8_t> data;
	};

	struct TowerEventRec
	{
		std::string type;
		int16_t id = 0;
		uint8_t standby = 0;
		std::vector<uint8_t> data;
	};

	struct Name
	{
		uint16_t type = 0;
		uint32_t id = 0;
		uint8_t favorite = 0;
		int32_t len = 0;
		std::string name;          ///< raw bytes incl. the trailing NUL
	};

	struct BldData
	{
		std::string world;         ///< world plugin mID, file order ("KJNS" Tokyo, "IAWH" Hawaii)

		// header
		int16_t level = 0;         ///< star level (1-based)
		int16_t f9c = 0;
		uint32_t fa0 = 0;          ///< doc+0xa0, CheckGameLevel VIP flags
		int16_t f80 = 0;
		uint8_t f88 = 0;
		int16_t speed = 0;         ///< ChangeSpeedMtoF(GetSpeed()); -2 in every save seen
		int16_t zero = 0;
		int16_t fa4 = 0;
		uint8_t reserved[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

		// T2DateTime
		uint16_t year = 1, month0 = 0, minutes = 0, seconds = 0;

		std::vector<PluginInfo> plugins;
		std::vector<People> people;      ///< every slot (256 per block)

		// T2FloorInfo
		int32_t vRange = 0, hRange = 0;
		std::vector<uint8_t> unitMap;    ///< vRange*hRange x (u16 mover/request, u16 tenant)
		int32_t nextTenantNumber = 0;
		std::vector<Tenant> tenants;     ///< every slot (128 per block), ids from 1000
		std::vector<Mover> movers;       ///< every slot (32 per block), ids from 1
		std::vector<Request> requests;   ///< every slot (64 per block), ids from 1000
		std::vector<OutObj> outObjs;     ///< every slot (64 per block), ids from 1000
		std::vector<uint16_t> outside;

		int16_t queuePresent = 0;
		std::vector<int16_t> queue;
		uint8_t f1e0 = 0;
		int16_t f1f0 = 0;
		Pool pool;
		std::vector<uint8_t> trafficMap; ///< cells x (u16 traffic, u16 range)

		Settlement settlement;
		std::vector<Blob4cc> movies;
		uint32_t latestBuild = 0;
		std::vector<TowerEventRec> events;

		Rect mainWindow;
		/// Main view scroll in pixels at the 1:1 zoom (36 px per row, 8 px
		/// per unit). Vertical first: bld.py's "scrollH"/"scrollV" names are
		/// swapped (checked against the oracle).
		int32_t scrollTop = 0, scrollLeft = 0;
		Rect toolPalette, controlPalette, messageWindow, unusedRect;
		uint8_t uiFlags[4] = {0, 0, 0, 0};
		uint16_t searchedTenant = 0;
		std::vector<Name> names;

		bool hasWorldStats = false;      ///< the 265-byte Kegon.dll block
		int16_t wsA = 0, wsB = 0, wsSwapped = 0;
		uint8_t wsEnabled = 0;
		int16_t wsF49 = 0;
		int32_t wsValues[64] = {};
		std::vector<uint8_t> worldStatsRaw; ///< anything else, kept verbatim

		/// Plugin table entry for a numeric item type (first match), or null.
		const PluginInfo * pluginForItemType(int itemType) const;
		/// Used slots only.
		int usedTenants() const;
		int usedMovers() const;
		int usedPeople() const;
		/// Funds in dollars ($100 units x 100).
		long long fundsDollars() const { return (long long)settlement.funds * 100; }
	};

	/// 4cc in file byte order -> Mac order ("ROLF" -> "FLOR").
	std::string macOrder(const std::string & fileOrder);

	/// Does this look like a .bld (the '002T' magic)?
	bool looksLikeBld(const std::string & path);

	bool parseBytes(const std::vector<uint8_t> & bytes, BldData & out, std::string & error);
	bool loadFile(const std::string & path, BldData & out, std::string & error);
	/// Serialises back to the exact .bld byte stream.
	bool writeBytes(const BldData & in, std::vector<uint8_t> & out, std::string & error);

	// ---- Import into OpenSky ----------------------------------------------

	/// Answers whether an OpenSky prototype exists and its fixed footprint
	/// (cells). Variable-size items (floor, lobby, elevators) still report
	/// their prototype size; the importer knows which dimensions vary.
	typedef std::function<bool(const std::string & id, int & width, int & height)> PrototypeLookup;

	struct ImportReport
	{
		std::string worldId;                       ///< "tokyo" / "hawaii" / "kegon"
		std::map<std::string, int> imported;       ///< prototype id -> items emitted
		std::map<std::string, int> dropped;        ///< "4cc name (reason)" -> records
		std::vector<std::string> notes;            ///< fidelity notes, player-readable
	};

	/// Plugin file stem ("Office", "StdElev") for a plugin table entry: the
	/// save's own path when present, else the shipped catalog by 4cc.
	std::string pluginStem(const PluginInfo & p);

	/// The OpenSky prototype for a plugin file stem ("" = no counterpart).
	/// Floor.t2p resolves to "floor"; toTowerXml turns ground-row floors
	/// into "lobby".
	std::string prototypeForPluginStem(const std::string & stem);

	/// Converts the save into native <tower> XML (ruleset yoottower). Fails
	/// only when the world is unknown.
	bool toTowerXml(const BldData & in, const PrototypeLookup & prototypes,
		std::string & xml, ImportReport & report, std::string & error);

}
}
