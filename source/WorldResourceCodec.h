/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Byte-exact decode + encode for the YootTower World plugin resources
/// (Tokyo.t2p, Hawai.t2p, Kegon.t2p). C++ port of tools/yoottower/world_sp.py;
/// field tables and engine addresses are in
/// doc/yoottower/world-and-sp-formats.md §2.
///
/// Two families:
///
///  * Binary CResFile resources (WORLDDEF, WATR, PODF, CODF, SPDF, T2EV, VIPL
///    and the `*EV` event resources) are described by a schema table, exactly
///    like world_sp.py's SCHEMAS, and decode to an ordered Json record whose
///    keys follow the engine read order. Counts that the file stores in front
///    of a list (numGrades, numCash, the T2EV count, ...) are derived from the
///    list length on encode, so editors can add and remove rows freely.
///    YootWorld.h keeps its own typed view of the same bytes for the game.
///
///  * Text and Mac-binary resources get typed structs: BINF (background tile
///    map), FINF (per-unit build mask), PTDF (palette animation def) and
///    PALETTE (Mac clut / pltt / raw ACT).
///
/// Every decoder is strict: it only succeeds when the decoded value encodes
/// back to the identical bytes, so a caller can fall back to raw storage.
#pragma once
#include "PluginResources.h"
#include "ResourceJson.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	// ---------------------------------------------------------------------
	// CResFile schema engine (binary world resources)
	// ---------------------------------------------------------------------

	/// The world resource types that decode through a schema.
	enum class WorldSchema
	{
		WorldDef,   // WORLDDEF id 1 (T2WorldDef::T2WorldDef 0x1006e4e0)
		Watr,       // WATR id 128
		PoolDef,    // PODF id 1 (MakePoolDef 0x1006eb10)
		ColdTable,  // CODF id 1 (MakeColdTableDef 0x1006ec50)
		SeasonParam,// SPDF (MakeSeasonParamDef 0x1006eba0)
		TowerEvents,// T2EV id 1 (T2TowerEvent::T2TowerEvent 0x10068ca0)
		VipPeople,  // VIPL 220..224
		Event,      // any `*EV` resource: T2EventItem header + body
	};

	/// Schema for a resource type name ("WORLDDEF", "FBEV", ...). Returns
	/// false when the type is not a schema world resource.
	bool worldSchemaForType(const std::string & type, WorldSchema & out);

	/// Decodes a schema resource into an ordered Json record. `type` matters
	/// for events (it selects the body layout: "FBEV", "VIEV", ...).
	/// Strings are Latin-1 mapped (see ResourceJson.h); 4ccs are written in
	/// Mac order ("BOEv"), or as a number when not printable. Event bodies
	/// that do not match the known layout are kept as "bodyInts" (or
	/// "bodyHex" when not a multiple of 4 bytes).
	bool decodeWorldSchemaResource(WorldSchema schema, const std::string & type,
	                               const char * data, size_t length, Json & out);
	/// Encodes a record produced by decodeWorldSchemaResource (possibly
	/// edited). Fails when a required field is missing or out of range;
	/// `error` names the field.
	bool encodeWorldSchemaResource(WorldSchema schema, const std::string & type,
	                               const Json & record, std::string & out, std::string * error = nullptr);

	/// The event body layouts known by type ("BOEV", "VIEV", ...); events
	/// without one (MREV, CREV, DOEV, TOEV) keep their tail as raw ints.
	bool worldEventHasTypedBody(const std::string & type);

	// ---------------------------------------------------------------------
	// BINF: background tile map (T2WorldPlugin::InitBkgndInfoProc 0x10073150)
	// ---------------------------------------------------------------------

	/// Text, one decimal per CRLF line: rows, cols, unused, then rows*cols
	/// tile values stored COLUMN-major. `cells` is kept ROW-major
	/// (cells[row * cols + col]); value & 0xffff indexes a 'Sky' tile of
	/// RT_BITMAP 1000.
	struct BkgndInfoDef
	{
		int32_t rows = 0;             // world height / 2
		int32_t cols = 0;             // world width / 8
		int32_t unused = 0;           // read, never used
		std::vector<int32_t> cells;   // row-major

		int32_t at(int row, int col) const { return cells[size_t(row) * size_t(cols) + size_t(col)]; }
	};

	bool decodeBkgndInfoDef(const char * data, size_t length, BkgndInfoDef & out);
	bool encodeBkgndInfoDef(const BkgndInfoDef & def, std::string & out);

	// ---------------------------------------------------------------------
	// FINF: per-unit build mask (T2FloorInfo::InitMask 0x10033d40)
	// ---------------------------------------------------------------------

	/// Text, one decimal per CRLF line: height, width, unk, RECT l,t,r,b (all
	/// read and discarded), then height*width values ROW-major. Non-zero
	/// marks a unit unbuildable (T2UnitInfo flag 0x8000).
	struct FloorInfoMaskDef
	{
		int32_t height = 0;
		int32_t width = 0;
		int32_t unk = 1;
		std::array<int32_t, 4> rectLTRB{{2, 2, 2, 2}};
		std::vector<int32_t> cells;   // row-major, cells[row * width + col]

		int32_t at(int row, int col) const { return cells[size_t(row) * size_t(width) + size_t(col)]; }
	};

	bool decodeFloorInfoMaskDef(const char * data, size_t length, FloorInfoMaskDef & out);
	bool encodeFloorInfoMaskDef(const FloorInfoMaskDef & def, std::string & out);

	// ---------------------------------------------------------------------
	// PTDF: palette animation def (T2PaletteAnimeDef 0x10047e60)
	// ---------------------------------------------------------------------

	/// Text `(name)<sep>value\r\n` lines. The separator whitespace is kept
	/// per line so the text re-encodes byte for byte.
	struct PaletteAnimeDef
	{
		struct Field
		{
			std::string name;         // "mNumOfTowerColor", ...
			std::string sep = "\t";   // whitespace between ')' and the value
			int32_t value = 0;
		};
		std::vector<Field> fields;    // engine order (8 fields in the shipped worlds)
	};

	bool decodePaletteAnimeDef(const char * data, size_t length, PaletteAnimeDef & out);
	bool encodePaletteAnimeDef(const PaletteAnimeDef & def, std::string & out);

	// ---------------------------------------------------------------------
	// PALETTE (LoadACTPaletteEntries 0x100120e0)
	// ---------------------------------------------------------------------

	/// Three on-disk forms: a raw 768-byte ACT, a Mac `clut` (first u16 0;
	/// u32 seed, u16 flags, u16 ctSize, then {index, r, g, b} u16 BE) and a
	/// Mac `pltt` (u16 count, 14 header bytes, then 16-byte entries
	/// {r, g, b, usage, tolerance, flags u16; private u32} BE).
	struct PaletteDef
	{
		enum Format { Act, Clut, Pltt };
		struct Entry
		{
			// clut: index, r, g, b. pltt: r, g, b, usage, tolerance, flags.
			// act: r, g, b (8-bit).
			std::array<uint16_t, 6> v{{0, 0, 0, 0, 0, 0}};
			uint32_t priv = 0;        // pltt private field
		};
		Format format = Clut;
		uint32_t seed = 0;            // clut
		uint16_t flags = 0;           // clut
		uint16_t ctSize = 0;          // clut: the engine loads only ctSize entries
		std::string plttHeader;       // pltt: the 14 reserved header bytes
		std::vector<Entry> entries;

		/// The 256 colours the engine builds (8-bit r,g,b).
		std::vector<std::array<uint8_t, 3>> rgb() const;
	};

	bool decodePaletteDef(const char * data, size_t length, PaletteDef & out);
	bool encodePaletteDef(const PaletteDef & def, std::string & out);

	// --- Json views of the typed world structs (OSP v3 and the editor) ---
	Json bkgndInfoToJson(const BkgndInfoDef & def);
	bool bkgndInfoFromJson(const Json & j, BkgndInfoDef & out, std::string * error = nullptr);
	Json floorInfoMaskToJson(const FloorInfoMaskDef & def);
	bool floorInfoMaskFromJson(const Json & j, FloorInfoMaskDef & out, std::string * error = nullptr);
	Json paletteAnimeToJson(const PaletteAnimeDef & def);
	bool paletteAnimeFromJson(const Json & j, PaletteAnimeDef & out, std::string * error = nullptr);
	Json paletteToJson(const PaletteDef & def);
	bool paletteFromJson(const Json & j, PaletteDef & out, std::string * error = nullptr);
}
