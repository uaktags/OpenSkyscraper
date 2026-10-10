/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower SP-format sub-plugins (the 51 `MV_*`, `Pl*` and ad `.t2p` files):
/// the "SP" container and the typed payloads of its three plugin classes.
/// Spec: doc/yoottower/world-and-sp-formats.md §3, reference implementation
/// tools/yoottower/world_sp.py.
///
///   "SP"  u32 count  count x {u32 type, u32 id, u32 offset, u32 size}  payloads
///
/// `type` is the CResFile tag constant stored LE, so the bytes read reversed
/// ("FDOM" is type 'MODF'); type 0 is a bare DIB. Payloads are contiguous in
/// directory order. The payloads are binary CResFile resources: ATTR/DESC as
/// in PE plugins, TMDF/WODF/DPDF as in PluginDefs.h, plus the sub-plugin
/// definitions below. Strings in SP files are Japanese-Windows (cp932) bytes,
/// kept verbatim.
#pragma once
#include "PluginResources.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	/// The SP container directory + payloads.
	struct SpContainer
	{
		struct Entry
		{
			uint32_t type = 0;        // CResFile tag constant (0 = DIB)
			uint32_t id = 0;
			uint32_t offset = 0;      // as read; recomputed on encode
			std::string data;

			/// Engine resource-type name, e.g. "MODF", or "BITMAP" for type 0.
			std::string typeName() const;
		};
		std::vector<Entry> entries;

		const Entry * find(const std::string & typeName, uint32_t id) const;
	};

	/// Parses an SP container. Fails on a bad magic or out-of-range entry.
	bool decodeSpContainer(const char * data, size_t length, SpContainer & out);
	/// Rebuilds the container with payloads laid out contiguously in directory
	/// order (the shipped layout), so decode+encode is byte-identical.
	bool encodeSpContainer(const SpContainer & c, std::string & out);

	/// Shared header of every sub-plugin payload (T2SubPlugin::LoadRsrcFromStream
	/// 0x1005d110).
	struct SubPluginHeader
	{
		std::string title;            // +0x70 GetTitle (== ATTR name)
		uint32_t tieupDate = 0;       // BCD 0xYYYYMMDD real-world tie-up expiry; 0 = none
	};

	/// MODF id 128: a movie for the theatres (T2MoviePlugin::LoadRsrcFromStream
	/// 0x100430e0). DESC 'MoPl'.
	struct MovieDef
	{
		SubPluginHeader header;
		int32_t imageID = 128;        // ushort, poster DIB id
		int32_t filmKind = 0;         // 0 roadshow (IsRoadshowFilm), 2 famous (IsFamousFilm)
		int32_t phase1Months = 0;     // life-cycle phase lengths (made cumulative on load)
		int32_t phase2Months = 0;
		int32_t phase3Months = 0;
		int32_t phase4Months = 0;     // read, then zeroed by the loader
		int32_t cycleMonths = 0;      // read, then zeroed by the loader
		std::string production;       // credits
		std::string director;
		std::string starring;
		int32_t runningTime = 0;      // minutes
		int32_t unkFC = 0;            // +0xfc, 200..500 (price or popularity?)
		std::string description;      // genre blurb
		int32_t customerTableID = 128;    // ushort, TMDF id of the audience table
		int32_t unk104 = 0;
		int32_t unk108 = 0;
		int32_t famousAudience = 0;   // audience >= this marks the film famous
		int32_t remakeAudience = 0;   // audience >= this lets the film re-run
		int32_t unk114 = 0;
		std::vector<std::string> extra;   // numExtra extra strings (empty in all files)
		int32_t stillImageID = 129;   // second DIB (208x117 still)
		int32_t clipSeconds = 0;      // trailer clip length (low confidence)
		std::string movieFile;        // file inside Movie/ (e.g. "MV_1stContact.MOV")
	};

	bool decodeMovieDef(const char * data, size_t length, MovieDef & out);
	bool encodeMovieDef(const MovieDef & def, std::string & out);

	/// HEDF id 128: an event-hall booking (T2HallEventPlugin::LoadRsrcFromStream
	/// 0x100380e0). DESC 'HePl'.
	struct HallEventDef
	{
		SubPluginHeader header;
		int32_t imageID = 128;        // ushort, 4 stacked frames (parts 200..203)
		uint32_t unkCC = 0;
		uint32_t bookingDays = 0;     // copied into the EventCenter booking entry (inferred)
		uint32_t openTime = 0;        // GetOpenTime, minute of day
		uint32_t closeTime = 0;       // GetCloseTime
		uint32_t monthMask = 0;       // 12-bit month mask, bit0 = January (data pattern)
		int32_t cost = 0;             // settled at close: funds += tenantValue - cost
		int32_t customerTableID = 128;    // ushort, TMDF id
		int32_t unkDC = 0;            // copied into the booking entry; probably a price
		int32_t unkE0 = 0;
		int32_t unkE4 = 0;
		int32_t bindTime = 0;         // GetBindTime: minutes visitors stay
		int32_t stillImageID = 129;   // second DIB
		std::string description;
		std::string movieFile;        // file inside Movie/ (HE_*.MOV)
	};

	bool decodeHallEventDef(const char * data, size_t length, HallEventDef & out);
	bool encodeHallEventDef(const HallEventDef & def, std::string & out);

	/// ADDF id 128: an advertisement for the Billboard (T2AdvertisePlugin::
	/// LoadRsrcFromStream 0x10015bd0). DESC 'AdPl'.
	struct AdvertiseDef
	{
		SubPluginHeader header;
		FourCC subType;               // GetSubType: must equal the host ToolDef mSubPluginType ('BoPI')
		int32_t imageID = 128;        // ushort, ad picture DIB id
		uint32_t unkD0 = 0;
		uint32_t days = 0;            // GetDay: campaign length
		int32_t money = 0;            // GetMoney: cost
		std::string path;             // GetPath
	};

	bool decodeAdvertiseDef(const char * data, size_t length, AdvertiseDef & out);
	bool encodeAdvertiseDef(const AdvertiseDef & def, std::string & out);

	/// PPDF id 128 in hall-event SP files. Unlike the PE world PPDF (a counted
	/// T2PeopleDef list) this is six bare int32s whose consumer has not been
	/// identified; the values are named by position. Shipped: 90, 2000, 0, 0,
	/// 10, 100.
	struct SubPluginPeopleParamDef
	{
		std::array<int32_t, 6> values{{0, 0, 0, 0, 0, 0}};
	};

	bool decodeSubPluginPeopleParamDef(const char * data, size_t length, SubPluginPeopleParamDef & out);
	bool encodeSubPluginPeopleParamDef(const SubPluginPeopleParamDef & def, std::string & out);
}
