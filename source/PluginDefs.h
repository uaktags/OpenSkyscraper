/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Typed decode + byte-exact encode for the YootTower template-definition
/// resources: TNDF / MVDF / OODF (tool -> equip -> tenant / mover / outer
/// object chain), TMDF (member tables), DPDF (day params), PLDF (place
/// params), PADF (people animation), SLDF (silhouettes), PPDF (people
/// types), WODF (speech lines) and STRTABLE/STRTBL (string lists).
///
/// Every struct mirrors the read order of the T2DLL.dll constructor that
/// consumes the resource; the field tables, addresses and confidence levels
/// are in doc/yoottower/plugin-schema.md §2 and the reference implementation
/// is tools/yoottower/t2p_schema.py. Field names follow the engine (`m`
/// prefix dropped). Fields the engine reads and throws away (`skipN`) or
/// never reads (`unreadN`) are kept so the bytes survive a round trip; the
/// editor should hide them and keep the observed defaults (plugin-schema §4).
///
/// Counts that the file stores (mValiationCnt, mNumOfInMoney, mNumOfElem,
/// list prefixes) are derived from the vector sizes on encode. The one
/// exception is `EquipDefPart::skip8`, which is usually but not always equal
/// to the variation count and is therefore stored explicitly.
///
/// Decoders are strict: they reject truncated data and trailing bytes, so a
/// successful decode always re-encodes to the identical bytes.
#pragma once
#include "PluginResources.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	// ---------------------------------------------------------------------
	// TNDF / MVDF / OODF id 1
	// ---------------------------------------------------------------------

	/// One ToolDef variation (T2ToolDef 0x10065540, up to 4 per plugin).
	struct ToolVariation
	{
		std::string priceString;      // +0x78 help "Cons. Cost" text ("$200000")
		std::string outMoneyString;   // +0x88 help "Maint. Cost" text ("$800/M")
		std::string help;             // +0x98 help body
		std::string helpTitle;        // +0xb8 help window title
		std::string name;             // +0xc8 GetName / GetToolName
		int32_t price = 0;            // +0xd8 CalcPayment, construction cost in $100 units
		int32_t outMoney = 0;         // +0xa8 GetOutMoney, maintenance per period in $100 units
		int32_t skip5 = 129;          // discarded (legacy Mac res ids)
		int32_t skip6 = 130;
		int32_t skip7 = 300;
		int32_t consumptionPower = 0; // +0xf0 GetConsumptionPower
	};

	/// ToolDef part shared by TNDF, MVDF and OODF.
	struct ToolDefPart
	{
		int32_t toolType = 0;         // +0x50 GetToolType (== ATTR mItemType)
		int32_t width = 1;            // +0x54 footprint width in units
		int32_t height = 1;           // +0x58 footprint height in floors
		int32_t level = 1;            // +0x5c no reader found (== ATTR mGameLevel)
		uint32_t attribute = 0;       // +0x60 GetAttribute bits (see ToolAttribute)
		std::string categoryHelp;     // +0x6c category description
		std::string categoryName;     // +0x68 GetCategoryName ("Shops", ...)
		int32_t category = 0;         // +0x64 GetCategory (toolbox group 10..200)
		int32_t toolNo = 0;           // +0x70 GetToolNo (order inside the group)
		int32_t skip1 = 0;            // discarded (movers carry 15..20)
		int32_t skip2 = 0;            // discarded (movers carry 1)
		FourCC subPluginType;         // +0x108 GetSubPluginType: 0, 'MoPI', 'HePI', 'BoPI'
		int32_t skip3 = 128;          // discarded (legacy res id 127/128)
		int32_t skip4 = 300;          // discarded (legacy res id 300)
		std::vector<ToolVariation> variations;  // mValiationCnt entries (<= 4)
	};

	/// ToolDef mAttribute bits proven in T2DLL (plugin-schema §2.3).
	namespace ToolAttribute
	{
		constexpr uint32_t Mover          = 0x2;       // IsMover
		constexpr uint32_t DragMaking     = 0x80;      // IsAbleDragMaking
		constexpr uint32_t PaysMaintenance = 0x100;    // CalcMentenanceCostProc
		constexpr uint32_t FloorType      = 0x2000;    // IsFloor / IsStair
		constexpr uint32_t VariableLength = 0x4000;    // IsValiableLength
		constexpr uint32_t Home           = 0x8000;    // IsHome
		constexpr uint32_t Concat         = 0x20000;
		constexpr uint32_t PeopleAnime    = 0x800000;  // IsPeopleAnime
	}

	/// Per-variation EquipDef record.
	struct EquipVariation
	{
		int32_t outSpan = 0;          // +0x154 (only the last one is kept)
		int32_t skip9 = 0;            // discarded
		int32_t skip10 = 0;           // discarded
	};

	/// EquipDef part (T2EquipDef 0x1002f240): TNDF and MVDF.
	struct EquipDefPart
	{
		int32_t capacity = 0;         // +0x11c GetCapacity
		int32_t stress = 0;           // +0x120 no reader found
		int32_t skip8 = 1;            // discarded; normally == variation count (movers: 3)
		std::vector<EquipVariation> variations;  // one per ToolDef variation
		int32_t inSpan = 0;           // +0x134 no reader found
		int32_t buildDays = 0;        // +0x138 no reader found
		std::vector<int32_t> inMoney; // +0x144[i] GetInMoney(i), income per price index ($100)
	};

	/// TenantDef part (T2TenantDef 0x100623c0): TNDF only.
	struct TenantDefPart
	{
		uint32_t registID = 0;        // +0x19c GetRegistID (1 hotel room, 2 trash, 3 parking, ...)
		uint32_t downFlag = 0;        // +0x1a0 GetDownFlag
		int32_t numOfKind = 1;        // +0x158 GetNumOfKind (sprite kinds)
		int32_t numOfPattern = 1;     // +0x15c GetNumOfPattern (patterns per kind)
		int32_t belongCapacity = 0;   // +0x160 residents / staff
		int32_t senseOfNoise = 0;     // +0x1a4 noise sensitivity
		int32_t entranceV = 0;        // +0x164 entrance cell
		int32_t entranceH = 0;        // +0x168
		int32_t exitV = 0;            // +0x16c exit cell
		int32_t exitH = 0;            // +0x170
		int32_t openTime = 0;         // +0x174 minutes from 0:00
		int32_t closeTime = 0;        // +0x178
		int32_t bindTime = 0;         // +0x17c stay length (minutes)
		int32_t maxBuildCount = 0;    // +0x104 0 = no limit
		uint32_t skip11 = 128;        // discarded
		uint32_t skip12 = 129;
		uint32_t skip13 = 128;
		std::string movieName;        // +0x184 QuickTime movie tag ("" / "[QT]:OF:")
	};

	/// MoverDef part (T2MoverDef 0x1003ffb0): MVDF only.
	struct MoverDefPart
	{
		PluginRect addSize;           // +0x158 GetAddSize, shaft margin
		int32_t maxLength = 0;        // +0x168 max floors (0 = unlimited / stairs)
		int32_t minLength = 0;        // +0x16c
		int32_t maxStop = 0;          // +0x170 GetMaxStop (express)
		int32_t score = 0;            // +0x1a8 route score
		int32_t defaultWait = 0;      // +0x174
		std::array<int32_t, 3> speed{{0, 0, 0}};  // +0x178 GetSpeed(i)
		int32_t maxModule = 0;        // +0x184 max cars
		int32_t moduleUnitH = 0;      // +0x188 no known consumer
		int32_t moduleUnitV = 0;      // +0x18c no known consumer
		int32_t moduleParam1 = 0;     // +0x190 no known consumer
		int32_t moduleParam2 = 0;     // +0x194 no known consumer; tracks the per-car price tier
		PluginRect moduleInset;       // +0x198 GetModuleInset
		int32_t skip14 = 6001;        // discarded (legacy sound ids)
		int32_t skip15 = 6002;
		int32_t skip16 = 0;
		int32_t skip17 = 0;
	};

	/// The plugin's main template definition: TNDF (tenant), MVDF (mover) or
	/// OODF (outer object), resource id 1. `kind` selects which parts are
	/// meaningful: Tenant = tool+equip+tenant, Mover = tool+equip+mover,
	/// OuterObj = tool + two unread int32s.
	struct TemplateDef
	{
		enum Kind { Tenant, Mover, OuterObj };

		Kind kind = Tenant;
		ToolDefPart tool;
		EquipDefPart equip;           // Tenant / Mover
		TenantDefPart tenant;         // Tenant
		MoverDefPart mover;           // Mover
		int32_t unreadA = 0;          // OuterObj: never read by T2OuterObjDef
		int32_t unreadB = 0;

		/// Resource type name for `kind`: "TNDF", "MVDF" or "OODF".
		const char * resourceType() const;
	};

	bool decodeTemplateDef(TemplateDef::Kind kind, const char * data, size_t length, TemplateDef & out);
	bool encodeTemplateDef(const TemplateDef & def, std::string & out);

	// ---------------------------------------------------------------------
	// TMDF id 1 (employees) / id 2 (customers)
	// ---------------------------------------------------------------------

	/// T2TenantMemberDef (0x10063000), 0x30 bytes in memory.
	struct MemberDef
	{
		std::string name;             // +0x04 char[16] (engine keeps <= 15 chars)
		int32_t demandType = -1;      // GetDemandType (pool demand class, -1 any)
		int32_t startSilhouetteType = 0;  // people-type range start
		int32_t endSilhouetteType = 0;    // people-type range end
		int32_t optionType = -1;      // GetOptionType
		uint32_t percent = 0;         // GetPercent
		uint32_t numOfPeople = 0;     // GetNumOfPeople
	};

	/// T2TenantMemberTableDef (0x100630a0).
	struct MemberTableDef
	{
		uint32_t econoType = 0xFFFFFFFFu;  // +0x08 GetEconoType (-1 none; bit4 = check only first)
		uint32_t specialFlag = 0;     // +0x0c GetSpecialFlag
		int32_t dayParamID = 0;       // DPDF resource id (0 = none)
		int32_t placeParamID = 0;     // PLDF resource id (0 = none)
		std::vector<MemberDef> members;  // mNumOfElem entries
	};

	bool decodeMemberTableDef(const char * data, size_t length, MemberTableDef & out);
	bool encodeMemberTableDef(const MemberTableDef & def, std::string & out);

	// ---------------------------------------------------------------------
	// DPDF / PLDF / PADF
	// ---------------------------------------------------------------------

	/// T2DayParamDef (0x1001d240): mPercent[EDay][EWhether], 0..100.
	/// Read order d0w0, d0w1, d1w0, d1w1. EDay 0/1 = weekday/holiday, EWhether
	/// 0/1 = fine/rain (names medium confidence).
	struct DayParamDef
	{
		uint32_t percent[2][2] = {{0, 0}, {0, 0}};
	};

	bool decodeDayParamDef(const char * data, size_t length, DayParamDef & out);
	bool encodeDayParamDef(const DayParamDef & def, std::string & out);

	/// T2PlaceParamDef (0x1004ed80): places[EPlace 0..2] = {percent, score}.
	/// EPlace 0 floor, 1 tenant, 2 pool. The engine reads the score as a
	/// short; `scoreSlot` keeps the whole 4-byte slot (negative scores are
	/// sign-extended in the shipped plugins) and `score()` is the engine view.
	struct PlaceParamDef
	{
		struct Place
		{
			uint32_t percent = 0;
			int32_t scoreSlot = 0;
			int16_t score() const { return int16_t(uint16_t(uint32_t(scoreSlot) & 0xFFFF)); }
		};
		Place places[3];
	};

	bool decodePlaceParamDef(const char * data, size_t length, PlaceParamDef & out);
	bool encodePlaceParamDef(const PlaceParamDef & def, std::string & out);

	/// T2PeopleAnimeDef (0x1004ad60): animated people drawn on a tenant.
	struct PeopleAnimeDef
	{
		struct Elem
		{
			int32_t position = 0;     // -1..6
			int32_t pattern = 0;      // 1..4
			int32_t silhouette = 0;   // silhouette index 0..28
			int32_t animeID = 0;      // 0..37
		};
		int32_t param1 = 1;           // +0x04 (always 1)
		int32_t param2 = 0;           // +0x08 (always == count in the shipped plugins)
		std::vector<Elem> elems;      // mNumOfElem entries
	};

	bool decodePeopleAnimeDef(const char * data, size_t length, PeopleAnimeDef & out);
	bool encodePeopleAnimeDef(const PeopleAnimeDef & def, std::string & out);

	// ---------------------------------------------------------------------
	// SLDF / PPDF / WODF / STRTABLE
	// ---------------------------------------------------------------------

	/// T2SilhouetteDef (0x10059f00), loaded by T2PeopleLoader::LoadSilhouette.
	struct SilhouetteDef
	{
		int32_t silhouetteType = 0;   // +0x58 GetSilhouetteType (300..1012)
		int32_t width = 1;            // +0x5c units (1/2)
		int32_t routeType = 0;        // +0x60 -1/0/1
		int32_t bitmapID = 0;         // +0x64 RT_BITMAP id (16x36 frames, 6 rows)
		std::string name;             // +0x68 GetName
	};
	using SilhouetteList = std::vector<SilhouetteDef>;

	bool decodeSilhouetteList(const char * data, size_t length, SilhouetteList & out);
	bool encodeSilhouetteList(const SilhouetteList & list, std::string & out);

	/// T2PeopleDef (0x1004bc00), loaded by T2PeopleLoader::LoadPeople (PE world
	/// plugins). The SP hall-event PPDF is a different 6-int record; see
	/// SubPluginDefs.h.
	struct PeopleDef
	{
		int32_t peopleType = 0;       // +0x50 GetPeopleType (1..61)
		int32_t attribute = 0;        // +0x54
		int32_t silhouetteType = 0;   // +0x58 AssignSilhouette
		int32_t stressMax = 300;      // +0x60
		int32_t stressBlueLimit = 100;    // +0x64
		int32_t stressYellowLimit = 230;  // +0x68
	};
	using PeopleList = std::vector<PeopleDef>;

	bool decodePeopleList(const char * data, size_t length, PeopleList & out);
	bool encodePeopleList(const PeopleList & list, std::string & out);

	/// T2WordDef (T2WordDef::Read 0x1006e210): one speech-bubble line.
	struct WordDef
	{
		uint32_t sexMask = 3;         // bit per sex (1 << sex)
		uint32_t ageMask = 0xFF;      // bit per age class (silhouette >> 3)
		uint32_t flags = 0;           // exact match: b0 own tenant, b1/b2 mood, b3 home, b4 VIP
		uint32_t special = 0;         // exact match: 0 normal, 1/2 status, 3..5 VIP, 6 Maru, 7 Reggae
		std::string text;
	};
	using WordList = std::vector<WordDef>;

	bool decodeWordList(const char * data, size_t length, WordList & out);
	bool encodeWordList(const WordList & list, std::string & out);

	/// STRTABLE (LoadStringTable 0x10012af0) / STRTBL: a counted string list.
	using StringTableDef = std::vector<std::string>;

	bool decodeStringTableDef(const char * data, size_t length, StringTableDef & out);
	bool encodeStringTableDef(const StringTableDef & list, std::string & out);
}
