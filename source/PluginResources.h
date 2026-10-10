/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Low-level codec for YootTower plugin (.t2p) resources plus the two
/// identity resources every plugin carries (ATTR, DESC).
///
/// YootTower's runtime (T2DLL.dll) reads plugin data through `CResFile`
/// (T2DLL 0x10010ca0..0x10011990). The rules, proven against all 105 PE
/// plugins (doc/yoottower/plugin-schema.md §1, tools/yoottower/t2p_schema.py):
///
///   * A resource whose first byte is 0x01 is BINARY; the 0x01 is consumed.
///     Any other first byte selects TEXT mode (whitespace/comma separated
///     tokens, `( ... )` comments skipped, strings run to end of line).
///   * Binary: EVERY integer extraction (uchar, short, int, uint, long)
///     consumes one 4-byte little-endian slot. The engine only looks at the
///     low byte(s) of uchar/short fields, but we keep the whole slot so the
///     high bytes survive a round trip.
///   * Binary strings: a length byte n, n bytes of text, then a terminator
///     byte (always 0 in the shipped plugins): n+2 bytes in total.
///   * RECT is four slots in Mac order top,left,bottom,right; POINT is y,x.
///
/// `CResReader`/`CResWriter` implement exactly that. The typed template
/// definitions (TNDF/MVDF/OODF/TMDF/...) live in PluginDefs.h, the SP
/// sub-plugin payloads in SubPluginDefs.h and the generic per-type
/// dispatcher in PluginResourceCodec.h.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace OT
{
	/// A 4-character code. `c` holds the four bytes exactly as they sit in the
	/// file, e.g. "GRUB" for Burger's ATTR mID. The same bytes read as a
	/// little-endian u32 are the constant the Mac-era source compares against
	/// (0x42555247 == 'BURG'), so `macStr()` ("BURG") is the human/engine
	/// spelling used by plugin-schema.md and the save files, while `str()`
	/// ("GRUB") is the raw file order.
	struct FourCC
	{
		char c[4] = {0, 0, 0, 0};

		bool operator==(const FourCC & o) const;
		bool operator!=(const FourCC & o) const { return !(*this == o); }
		/// Compares against a C string of exactly 4 chars in FILE order
		/// (e.g. `f == "GRUB"`).
		bool operator==(const char * s) const;
		bool operator!=(const char * s) const { return !(*this == s); }

		/// The four bytes in file order, e.g. "GRUB". Embedded NULs are dropped.
		std::string str() const;
		/// The four bytes in Mac source order (reversed), e.g. "BURG". This
		/// is the spelling of plugin-schema.md / t2p_schema.py dumps.
		std::string macStr() const;
		/// Little-endian u32 of the file bytes == the Mac 4cc constant.
		uint32_t toUint32LE() const;
		/// True for the "****" placeholder (ATTR mDependID "no dependency").
		bool isNone() const { return *this == "****"; }
		bool isZero() const { return toUint32LE() == 0; }

		static FourCC fromUint32LE(uint32_t v);
		/// From file-order text ("GRUB"). Shorter input is NUL padded.
		static FourCC fromFileOrder(std::string_view s);
		/// From Mac-order text ("BURG" -> bytes "GRUB").
		static FourCC fromMacOrder(std::string_view s);
	};

	/// A Mac-order rectangle as stored by `CResFile>>RECT` (0x10011730):
	/// four int32 slots top,left,bottom,right.
	struct PluginRect
	{
		int32_t top = 0, left = 0, bottom = 0, right = 0;
		bool operator==(const PluginRect & o) const
		{
			return top == o.top && left == o.left && bottom == o.bottom && right == o.right;
		}
	};

	/// Sequential reader for the binary CResFile form. Every accessor
	/// returns a zero value once a read fails; check `ok()` at the end.
	/// Strings with a non-zero terminator are rejected (no shipped plugin has
	/// one) so that every accepted resource re-encodes byte-identically.
	class CResReader
	{
	public:
		/// Fails (ok()==false) unless the buffer starts with the 0x01 marker.
		CResReader(const char * data, size_t length);

		bool ok() const { return ok_; }
		bool atEnd() const { return p_ == end_; }
		size_t remaining() const { return size_t(end_ - p_); }

		int32_t i32();
		uint32_t u32();
		FourCC fourcc();
		std::string str();
		PluginRect rect();
		/// Reads an int32 element count and checks 0 <= n <= maxCount (and
		/// n <= remaining bytes).
		int32_t count(int32_t maxCount = 100000);
		/// Marks the stream bad (used by decoders for semantic checks).
		void fail() { ok_ = false; }

	private:
		const unsigned char * p_ = nullptr;
		const unsigned char * end_ = nullptr;
		bool ok_ = false;
	};

	/// Writer counterpart of CResReader. Starts with the 0x01 marker.
	/// `ok()` turns false if a string exceeds 255 bytes.
	class CResWriter
	{
	public:
		CResWriter() : buf_(1, char(1)) {}

		void i32(int32_t v) { u32(uint32_t(v)); }
		void u32(uint32_t v);
		void fourcc(const FourCC & f) { buf_.append(f.c, 4); }
		void str(const std::string & s);
		void rect(const PluginRect & r) { i32(r.top); i32(r.left); i32(r.bottom); i32(r.right); }
		/// Writes a count slot (size of a vector).
		void count(size_t n) { u32(uint32_t(n)); }
		void fail() { ok_ = false; }

		bool ok() const { return ok_; }
		/// Moves the bytes into `out` and returns ok().
		bool finish(std::string & out);

	private:
		std::string buf_;
		bool ok_ = true;
	};

	/// Tokenizer for the CResFile TEXT form (CResFile::Token 0x10011290):
	/// tokens are separated by space/tab/CR/LF/comma, `( ... )` is a comment,
	/// and a string runs to the end of the line.
	class CResTextReader
	{
	public:
		CResTextReader(const char * data, size_t length) : s_(data, length) {}
		/// Next token, or "" at end of input.
		std::string token();
		/// Next token parsed as a decimal integer (0 on failure).
		long long integer();
		/// Rest of the current line after skipping separators/comments.
		std::string line();

	private:
		void skip();
		std::string s_;
		size_t p_ = 0;
	};

	/// `ATTR` id 128: plugin identity. `CFilePlugin::CFilePlugin` 0x1000df20
	/// reads, in order: mID, mAttrib, mDependID, mGameLevel, mSecretItem,
	/// mItemType, mMustItem, mPluginName (plugin-schema.md §2.1).
	///
	/// Earlier OpenSky code called the 4cc at offset 1 `itemType` and the int
	/// at offset 21 `id`; that was backwards. mID is the plugin's unique 4cc
	/// (the save files reference it) and mItemType is the numeric item type
	/// (Floor 1000, Office 1200, Burger 1301, StdElev 1810) that equals the
	/// ToolDef mToolType.
	///
	/// Two encodings exist: the binary form (every item plugin) and the text
	/// form (DefaultToolPalette only), which uses the same token order with
	/// `(mName)` comments. Both populate the same struct; the text form keeps
	/// the original text so it re-encodes byte-identically.
	struct PluginAttr
	{
		bool valid = false;
		bool isTextForm = false;

		int version = 1;             // leading byte of the binary form (always 1)
		FourCC id;                   // mID: unique plugin id, file order ("GRUB" == 'BURG')
		uint32_t attrib = 0;         // mAttrib: world-availability mask (Tokyo 0x1/0x2/0x4, Kegon 0x10, Hawai 0x100)
		FourCC dependID;             // mDependID: required plugin ("****" none, 'FRON' = needs a hotel Front)
		int32_t gameLevel = 0;       // mGameLevel: star level that unlocks the item (1..6)
		int32_t secretItem = 0;      // mSecretItem: hidden unless secret mode
		int32_t itemType = 0;        // mItemType: numeric item type (== ToolDef mToolType)
		int32_t mustItem = 0;        // mMustItem: always-available item
		std::string name;            // mPluginName (cp1252 bytes)

		std::string text;            // text form only: the original resource text

		/// True when mDependID names a real plugin (not "****" / 0).
		bool hasDependency() const { return !dependID.isNone() && !dependID.isZero(); }
	};

	/// `DESC` id 128: plugin class (`T2PluginSpecifier::Initialize` 0x1004fd00).
	/// The classTag bytes (file order) read as a LE u32 are the literal the
	/// plugin's `ConstructProgramPlugin` passes to its class constructor.
	struct PluginDesc
	{
		enum Kind
		{
			Unknown = 0,
			TenantPlugin,     // "lPnT" ('TnPl') T2TenantPlugin
			MoverPlugin,      // "lPvM" ('MvPl') T2MoverPlugin
			ToolPalette,      // "LPOT" ('TOPL')
			WorldPlugin,      // "lPdW" ('WdPl') Tokyo, Hawai, Kegon
			OuterObjPlugin,   // "lPoO" ('OoPl') Billboard
			MoviePlugin,      // "lPoM" ('MoPl') SP-format MV_* movies
			HallEventPlugin,  // "lPeH" ('HePl') SP-format Pl* hall events
			AdvertisePlugin,  // "lPdA" ('AdPl') SP-format ads
		};

		bool valid = false;
		int version = 1;             // leading byte (always 1)
		FourCC classTag;             // mType, file order
		uint32_t value = 0;          // mVersion: T2PluginLoader::GetNext only accepts 3
		Kind kind = Unknown;
	};

	/// Parses an ATTR resource (binary or text form). Returns false (leaving
	/// `out` untouched) on truncated/garbage input or trailing bytes.
	bool parsePluginAttr(const char * data, int length, PluginAttr & out);
	/// Encodes an ATTR. Binary form unless `isTextForm`; the text form reuses
	/// `text` verbatim when it still parses to the same values, otherwise it
	/// is regenerated in DefaultToolPalette's `(mField)\tvalue\r\n` style.
	bool encodePluginAttr(const PluginAttr & attr, std::string & out);

	/// Parses a DESC resource (9 bytes). Unknown tags still parse (kind stays
	/// Unknown).
	bool parsePluginDesc(const char * data, int length, PluginDesc & out);
	bool encodePluginDesc(const PluginDesc & desc, std::string & out);

	/// Maps a DESC classTag to its Kind (Unknown when not recognised).
	PluginDesc::Kind pluginKindFromTag(const FourCC & tag);
	/// The classTag (file order) for a Kind; "\0\0\0\0" for Unknown.
	FourCC pluginTagFromKind(PluginDesc::Kind kind);
}
