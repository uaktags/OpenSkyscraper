/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Typed parsers for YootTower plugin (.t2p) Win32 resources.
///
/// `PluginManager` exposes raw resource bytes via `Plugin::getResource`.
/// These parsers turn the HIGH-confidence resource types documented in
/// `doc/yoottower/resource-formats.md` into small POD structs. They are
/// pure functions over a byte buffer (no Plugin / PE dependency) so they
/// can be unit-tested with hand-crafted inputs and reused by the audit.
///
/// Layouts reverse-engineered from the calibration set (Coin, Condo,
/// Apartment, Burger, Stair, Escalator, StdElev, DefaultToolPalette) and
/// cross-checked against the plugin DLLs disassembled in Ghidra.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	/// A 4-character code as stored in the file: four consecutive bytes.
	/// YootTower uses these everywhere as item-type and plugin-class tags.
	/// `str()` reads them in file (display) order; `toUint32LE()` yields the
	/// little-endian u32 the plugin DLLs pass to their constructors
	/// (confirmed in Ghidra: ConstructProgramPlugin passes exactly this value).
	struct FourCC
	{
		char c[4] = {0, 0, 0, 0};

		bool operator==(const FourCC & o) const;
		bool operator!=(const FourCC & o) const { return !(*this == o); }
		/// Compares against a C string of exactly 4 chars (e.g. `f == "GRUB"`).
		bool operator==(const char * s) const;
		bool operator!=(const char * s) const { return !(*this == s); }

		/// The four bytes in display order, e.g. "GRUB". Never NUL-terminated
		/// internally; `str()` returns a std::string of length 4 (or fewer if
		/// a NUL appears, which shouldn't happen for real tags).
		std::string str() const;
		/// Little-endian u32 (byte0 | byte1<<8 | byte2<<16 | byte3<<24).
		/// For "lPnT" this is 0x546E506C — the literal `ConstructProgramPlugin`
		/// passes to the `T2TenantPlugin` constructor.
		uint32_t toUint32LE() const;
	};

	/// `ATTR` (resource id 128) — item identity, unlock level, display name.
	///
	/// Two encodings exist:
	///   - binary form: every gameplay item (Coin, Condo, Burger, ...).
	///   - text form:   DefaultToolPalette only, but it is the rosetta stone
	///                  that names every field (mID, mAttrib, mDependID,
	///                  mGameLevel, mSecretItem, mItemType, mMustItem,
	///                  mPluginName). Both forms populate the same struct.
	struct PluginAttr
	{
		bool valid = false;
		bool isTextForm = false;     // true == parsed the (mField) text variant

		int version = 0;
		FourCC itemType;             // mItemType: GRUB/RATS/EdtS/...
		FourCC marker;               // 4-byte field at offset 9; usually "****"
		                             // but "NORF" on hotel-room plugins. Likely a
		                             // category/group code (binary form only).
		uint32_t attrib = 0;         // mAttrib (only meaningful in text form;
		                             // binary form leaves this at 0)
		uint32_t dependID = 0;       // mDependID
		uint32_t gameLevel = 0;      // mGameLevel (1 = Star1 .. 5 = Star5)
		uint32_t secretItem = 0;     // mSecretItem
		uint32_t mustItem = 0;       // mMustItem
		uint32_t id = 0;             // mID (cross-ref key into MVDF/TNDF)
		std::string name;            // mPluginName
	};

	/// `DESC` (resource id 128) — plugin-class discriminator.
	///
	/// The classTag (little-endian u32) is exactly the constant the plugin's
	/// `ConstructProgramPlugin` passes to its item-class constructor, so this
	/// one 4cc tells us the plugin kind without running any code.
	struct PluginDesc
	{
		enum Kind
		{
			Unknown = 0,
			TenantPlugin,   // classTag "lPnT" (0x546E506C) — T2TenantPlugin
			MoverPlugin,    // classTag "lPvM" (0x4D76506C) — T2MoverPlugin
			ToolPalette,    // classTag "LPOT" (0x544F504C)
			WorldPlugin,    // classTag "lPdW" (0x5764506C) — scenario plugins (Tokyo, Hawai, Kegon)
			OuterObjPlugin, // classTag "lPoO" (0x4F6F506C) — out-view objects (Billboard)
		};

		bool valid = false;
		int version = 0;
		FourCC classTag;
		uint32_t value = 0;          // always 3 in the calibration set
		Kind kind = Unknown;
	};

	/// `DPDF` (resource id 128) — default placement parameters (T2PlaceParamDef).
	/// Present on placement-driven items (Coin, Burger); absent on transport.
	struct PluginDpdf
	{
		bool valid = false;
		int version = 0;
		uint32_t w = 0;              // 70 (cell width / footprint-x)
		uint32_t h = 0;              // 50 (cell height / footprint-y)
		uint32_t baseX = 0;          // 100
		uint32_t baseY = 0;          // 80
	};

	/// Parses an ATTR resource. Accepts both the binary and the text form.
	/// Returns false (leaving `out` untouched) on truncated/garbage input.
	bool parsePluginAttr(const char * data, int length, PluginAttr & out);

	/// Parses a DESC resource (9-byte binary header). Maps the known classTags
	/// to PluginDesc::Kind; unknown tags still parse (kind stays Unknown).
	bool parsePluginDesc(const char * data, int length, PluginDesc & out);

	/// Parses a DPDF resource (17-byte binary). Returns false on truncation.
	bool parsePluginDpdf(const char * data, int length, PluginDpdf & out);

	/// `PLDF` (resource id 128) — price list for commercial items.
	///
	/// Short fixed-layout definition: a version byte then five little-endian
	/// u32s. Per resource-formats.md §3.10 the third field (here `basePrice`)
	/// is the construction cost; the others are income/expense factors whose
	/// exact semantics are inferred. Present on shops (Burger); absent on
	/// residences and transport.
	struct PluginPldf
	{
		bool valid = false;
		int version = 0;
		uint32_t f1 = 0;            // 40 (0x28) on Burger
		uint32_t basePrice = 0;     // 1000 (0x3E8) — construction cost
		uint32_t f2 = 0;            // 10
		uint32_t f3 = 0;            // 300 (0x12C)
		uint32_t f4 = 0;            // 50
		uint32_t f5 = 0;            // 100
	};

	/// Parses a PLDF resource (21-byte binary: u8 version + 5 u32 LE).
	/// Returns false on truncation.
	bool parsePluginPldf(const char * data, int length, PluginPldf & out);

	/// `MVDF` (resource id 1) — mover (transport) definition.
	///
	/// Used by stairs/escalators/elevators. The layout is an 18-byte binary
	/// header followed by an irregular tagged-pascal-string array carrying
	/// the description, category, human-readable price strings (e.g.
	/// "$150000:$50000/car", "$12000/Q"), and the item name. The header's
	/// per-field semantics (capacity / speed / max-floors) are documented at
	/// MED confidence in resource-formats.md §3.9 and confirmed by disassembly
	/// in T2DLL.dll's T2MoverPlugin constructor — several fields are therefore
	/// exposed generically here and tagged `// inferred` until that pass lands.
	///
	/// The string array is extracted as a robust printable-ASCII-run scan so
	/// the editor can always show the price/name/category regardless of the
	/// exact framing; the two most useful price strings are also surfaced
	/// directly as `priceString` (e.g. "$150000:$50000/car") and
	/// `quarterlyString` (e.g. "$12000/Q").
	struct PluginMvdf
	{
		bool valid = false;
		int version = 0;
		uint32_t itemID = 0;        // byte 1: == mID (Stair=1800, StdElev=1810, ...)
		uint32_t field5 = 0;        // byte 5: 8 for stairs/escalator; varies (inferred)
		uint32_t field9 = 0;        // byte 9: 2 stairs/esc, 1 elevators (inferred)
		uint32_t field13 = 0;       // byte 13: 1/2/3 (inferred capacity/variant)

		/// All printable-ASCII runs (length >= 3) found in the body, in order.
		/// Typically: [description, category, price, (quarterly), item-desc, name, name].
		std::vector<std::string> strings;

		/// The primary price string (e.g. "$150000:$50000/car" or "$5000").
		std::string priceString;
		/// The recurring/quarterly price string if present (e.g. "$12000/Q").
		std::string quarterlyString;
		/// The display name (last non-empty string; movers carry it twice).
		std::string name;
	};

	/// Parses an MVDF resource. Returns false if the buffer is too short for
	/// the 18-byte header. String extraction never fails (absent strings leave
	/// the fields empty), so a `true` result with empty strings means the body
	/// held no printable runs.
	bool parsePluginMvdf(const char * data, int length, PluginMvdf & out);
}
