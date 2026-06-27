/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginResources.h"

#include <cstring>

namespace OT
{

	// ---------------------------------------------------------------------------
	// FourCC
	// ---------------------------------------------------------------------------

	bool FourCC::operator==(const FourCC & o) const
	{
		return std::memcmp(c, o.c, 4) == 0;
	}

	bool FourCC::operator==(const char * s) const
	{
		if (!s) return false;
		// Compare only the bytes that `s` actually has; a real 4cc is exactly
		// 4 chars with no NUL, but be tolerant of callers passing "GRU"/etc.
		for (int i = 0; i < 4; ++i)
		{
			char sc = s[i];
			if (sc == '\0') return false;          // `s` is shorter than 4
			if (c[i] != sc) return false;
		}
		// Require `s` to be exactly 4 chars (5th must be NUL) so "GRUBX" does
		// not silently match "GRUB".
		return s[4] == '\0';
	}

	std::string FourCC::str() const
	{
		// Drop embedded NULs (none expected on real tags).
		std::string s;
		s.reserve(4);
		for (int i = 0; i < 4; ++i)
			if (c[i] != '\0') s.push_back(c[i]);
		return s;
	}

	uint32_t FourCC::toUint32LE() const
	{
		return uint32_t(uint8_t(c[0]))
		     | (uint32_t(uint8_t(c[1])) << 8)
		     | (uint32_t(uint8_t(c[2])) << 16)
		     | (uint32_t(uint8_t(c[3])) << 24);
	}

	// ---------------------------------------------------------------------------
	// Little-endian readers (bounds-checked; never read past `end`)
	// ---------------------------------------------------------------------------

	static uint32_t readU32LE(const char * p, const char * end, bool & ok)
	{
		if (!ok || end - p < 4) { ok = false; return 0; }
		return uint32_t(uint8_t(p[0]))
		     | (uint32_t(uint8_t(p[1])) << 8)
		     | (uint32_t(uint8_t(p[2])) << 16)
		     | (uint32_t(uint8_t(p[3])) << 24);
	}

	/// Reads a Pascal string (1-byte length prefix + N bytes, no NUL) into
	/// `out`. Advances `p` past it. Sets `ok=false` if truncated.
	static void readPStr(const char *& p, const char * end, bool & ok, std::string & out)
	{
		if (!ok || end - p < 1) { ok = false; return; }
		const uint8_t n = uint8_t(p[0]);
		p += 1;
		if (!ok || end - p < int(n)) { ok = false; return; }
		out.assign(p, n);
		p += n;
	}

	// ---------------------------------------------------------------------------
	// ATTR — binary form
	//   0  u8    version
	//   1  char4 mItemType
	//   5  u32   field5     (binary form only; not exposed as a named field)
	//   9  char4 "****"     (marker)
	//  13  u32   mGameLevel
	//  17  u32   mMustItem
	//  21  u32   mID
	//  25  u32   (pad)
	//  29  pstr  mPluginName
	// ---------------------------------------------------------------------------
	static bool parseAttrBinary(const char * data, int length, PluginAttr & out)
	{
		const char * p = data;
		const char * end = data + length;
		bool ok = true;

		out.version = uint8_t(p[0]); p += 1;
		std::memcpy(out.itemType.c, p, 4); p += 4;
		/*uint32_t field5 =*/ (void)readU32LE(p, end, ok); p += 4;  // variant; meaning TBD
		std::memcpy(out.marker.c, p, 4); p += 4;                    // "****"/"NORF"/...
		out.gameLevel = readU32LE(p, end, ok); p += 4;
		out.mustItem  = readU32LE(p, end, ok); p += 4;
		out.id        = readU32LE(p, end, ok); p += 4;
		/*uint32_t pad =*/ (void)readU32LE(p, end, ok); p += 4;
		readPStr(p, end, ok, out.name);

		if (!ok) return false;
		out.valid = true;
		return true;
	}

	// ---------------------------------------------------------------------------
	// ATTR — text form (DefaultToolPalette only)
	//   (mID)\t\t1000\r\n(mAttrib)\t16777215\r\n...
	// Tab-separated key/value, CRLF lines. Names the binary-form fields.
	// ---------------------------------------------------------------------------
	static bool parseAttrText(const char * data, int length, PluginAttr & out)
	{
		// Operate on a NUL-terminated copy for easy tokenizing. The resource
		// is small (<1 KB) so this is cheap.
		std::string s(data, length);
		const char * src = s.c_str();

		auto matchKey = [&](const char * key) -> bool
		{
			size_t kl = std::strlen(key);
			return std::strncmp(src, key, kl) == 0;
		};
		auto readUint = [&](const char * key) -> uint32_t
		{
			size_t kl = std::strlen(key);
			src += kl;
			// skip tabs/spaces
			while (*src == '\t' || *src == ' ') ++src;
			unsigned long long v = 0;
			while (*src >= '0' && *src <= '9') { v = v * 10 + (*src - '0'); ++src; }
			return uint32_t(v);
		};

		// Iterate CRLF-delimited lines.
		while (*src)
		{
			const char * line = src;
			const char * nl = std::strchr(line, '\n');
			const char * lineEnd = nl ? nl : line + std::strlen(line);

			bool consumed = false;
			if (matchKey("(mID)"))        { out.id = readUint("(mID)"); consumed = true; }
			else if (matchKey("(mAttrib)"))    { out.attrib = readUint("(mAttrib)"); consumed = true; }
			else if (matchKey("(mDependID)"))  { out.dependID = readUint("(mDependID)"); consumed = true; }
			else if (matchKey("(mGameLevel)")) { out.gameLevel = readUint("(mGameLevel)"); consumed = true; }
			else if (matchKey("(mSecretItem)")){ out.secretItem = readUint("(mSecretItem)"); consumed = true; }
			else if (matchKey("(mMustItem)"))  { out.mustItem = readUint("(mMustItem)"); consumed = true; }
			else if (matchKey("(mItemType)"))
			{
				src += std::strlen("(mItemType)");
				while (*src == '\t' || *src == ' ') ++src;
				uint32_t v = 0;
				while (*src >= '0' && *src <= '9') { v = v * 10 + (*src - '0'); ++src; }
				out.itemType.c[0] = char(v & 0xFF);
				out.itemType.c[1] = char((v >> 8) & 0xFF);
				out.itemType.c[2] = char((v >> 16) & 0xFF);
				out.itemType.c[3] = char((v >> 24) & 0xFF);
				consumed = true;
			}
			else if (matchKey("(mPluginName)"))
			{
				src += std::strlen("(mPluginName)");
				while (*src == '\t' || *src == ' ') ++src;
				const char * nameStart = src;
				out.name.assign(nameStart, size_t(lineEnd - nameStart));
				// strip trailing \r
				if (!out.name.empty() && out.name.back() == '\r') out.name.pop_back();
				consumed = true;
			}

			(void)consumed;
			src = nl ? nl + 1 : lineEnd;
		}

		// Must have at least read an mID to consider this a real ATTR.
		if (out.id == 0 && out.name.empty()) return false;
		out.version = 1;
		out.isTextForm = true;
		out.valid = true;
		return true;
	}

	bool parsePluginAttr(const char * data, int length, PluginAttr & out)
	{
		if (!data || length < 1) return false;
		// The text form begins with "(mID)"; the binary form begins with 0x01.
		if (length >= 5 && data[0] == '(' && data[1] == 'm')
		{
			PluginAttr tmp;
			if (!parseAttrText(data, length, tmp)) return false;
			out = std::move(tmp);
			return true;
		}
		PluginAttr tmp;
		if (!parseAttrBinary(data, length, tmp)) return false;
		out = std::move(tmp);
		return true;
	}

	// ---------------------------------------------------------------------------
	// DESC — 9-byte header
	//   0  u8    version
	//   1  char4 classTag  (lPnT / lPvM / LPOT)
	//   5  u32    value    (3)
	// ---------------------------------------------------------------------------
	bool parsePluginDesc(const char * data, int length, PluginDesc & out)
	{
		if (!data || length < 9) return false;
		out.version = uint8_t(data[0]);
		std::memcpy(out.classTag.c, data + 1, 4);
		bool ok = true;
		out.value = readU32LE(data + 5, data + length, ok);
		if (!ok) return false;

		// Map known classTags to a plugin kind. The u32 is the literal the
		// plugin's ConstructProgramPlugin passes to its constructor.
		const uint32_t k = out.classTag.toUint32LE();
		switch (k)
		{
			case 0x546E506Cu: out.kind = PluginDesc::TenantPlugin; break; // "lPnT"
			case 0x4D76506Cu: out.kind = PluginDesc::MoverPlugin;  break; // "lPvM"
			case 0x544F504Cu: out.kind = PluginDesc::ToolPalette;  break; // "LPOT"
			case 0x5764506Cu: out.kind = PluginDesc::WorldPlugin;  break; // "lPdW"
			case 0x4F6F506Cu: out.kind = PluginDesc::OuterObjPlugin; break; // "lPoO"
			default:          out.kind = PluginDesc::Unknown;      break;
		}
		out.valid = true;
		return true;
	}

	// ---------------------------------------------------------------------------
	// DPDF — 17-byte placement header
	//   0  u8  version
	//   1  u32 w
	//   5  u32 h
	//   9  u32 baseX
	//  13  u32 baseY
	// ---------------------------------------------------------------------------
	bool parsePluginDpdf(const char * data, int length, PluginDpdf & out)
	{
		if (!data || length < 17) return false;
		out.version = uint8_t(data[0]);
		bool ok = true;
		out.w     = readU32LE(data + 1,  data + length, ok);
		out.h     = readU32LE(data + 5,  data + length, ok);
		out.baseX = readU32LE(data + 9,  data + length, ok);
		out.baseY = readU32LE(data + 13, data + length, ok);
		if (!ok) return false;
		out.valid = true;
		return true;
	}
}
