/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginResources.h"

#include <cstdio>
#include <cstdlib>
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
		// A real 4cc is exactly 4 chars; "GRU" and "GRUBX" must not match.
		for (int i = 0; i < 4; ++i)
		{
			if (s[i] == '\0') return false;
			if (c[i] != s[i]) return false;
		}
		return s[4] == '\0';
	}

	std::string FourCC::str() const
	{
		std::string s;
		s.reserve(4);
		for (int i = 0; i < 4; ++i)
			if (c[i] != '\0') s.push_back(c[i]);
		return s;
	}

	std::string FourCC::macStr() const
	{
		std::string s;
		s.reserve(4);
		for (int i = 3; i >= 0; --i)
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

	FourCC FourCC::fromUint32LE(uint32_t v)
	{
		FourCC f;
		for (int i = 0; i < 4; ++i) f.c[i] = char((v >> (8 * i)) & 0xFF);
		return f;
	}

	FourCC FourCC::fromFileOrder(std::string_view s)
	{
		FourCC f;
		for (size_t i = 0; i < 4 && i < s.size(); ++i) f.c[i] = s[i];
		return f;
	}

	FourCC FourCC::fromMacOrder(std::string_view s)
	{
		FourCC f;
		for (size_t i = 0; i < 4 && i < s.size(); ++i) f.c[3 - i] = s[i];
		return f;
	}

	// ---------------------------------------------------------------------------
	// CResReader / CResWriter (binary form)
	// ---------------------------------------------------------------------------

	CResReader::CResReader(const char * data, size_t length)
	{
		if (!data || length < 1 || uint8_t(data[0]) != 0x01) return;
		p_ = reinterpret_cast<const unsigned char *>(data) + 1;
		end_ = reinterpret_cast<const unsigned char *>(data) + length;
		ok_ = true;
	}

	uint32_t CResReader::u32()
	{
		if (!ok_ || end_ - p_ < 4) { ok_ = false; return 0; }
		const uint32_t v = uint32_t(p_[0]) | (uint32_t(p_[1]) << 8)
		                 | (uint32_t(p_[2]) << 16) | (uint32_t(p_[3]) << 24);
		p_ += 4;
		return v;
	}

	int32_t CResReader::i32()
	{
		return int32_t(u32());
	}

	FourCC CResReader::fourcc()
	{
		return FourCC::fromUint32LE(u32());
	}

	std::string CResReader::str()
	{
		// length byte n, n bytes, terminator byte (CResFile>>char* 0x10011830).
		if (!ok_ || end_ - p_ < 1) { ok_ = false; return {}; }
		const size_t n = p_[0];
		if (size_t(end_ - p_) < n + 2) { ok_ = false; return {}; }
		if (p_[1 + n] != 0) { ok_ = false; return {}; }  // non-zero terminator: not representable
		std::string s(reinterpret_cast<const char *>(p_ + 1), n);
		p_ += n + 2;
		return s;
	}

	PluginRect CResReader::rect()
	{
		PluginRect r;
		r.top = i32();
		r.left = i32();
		r.bottom = i32();
		r.right = i32();
		return r;
	}

	int32_t CResReader::count(int32_t maxCount)
	{
		const int32_t n = i32();
		// Every record is at least one byte, so a count larger than the rest
		// of the stream is corruption (and must not trigger a huge resize).
		if (!ok_ || n < 0 || n > maxCount || size_t(n) > remaining()) { ok_ = false; return 0; }
		return n;
	}

	void CResWriter::u32(uint32_t v)
	{
		buf_.push_back(char(v & 0xFF));
		buf_.push_back(char((v >> 8) & 0xFF));
		buf_.push_back(char((v >> 16) & 0xFF));
		buf_.push_back(char((v >> 24) & 0xFF));
	}

	void CResWriter::str(const std::string & s)
	{
		if (s.size() > 255) { ok_ = false; return; }
		buf_.push_back(char(uint8_t(s.size())));
		buf_ += s;
		buf_.push_back('\0');
	}

	bool CResWriter::finish(std::string & out)
	{
		if (!ok_) return false;
		out = std::move(buf_);
		buf_.assign(1, char(1));
		return true;
	}

	// ---------------------------------------------------------------------------
	// CResTextReader (text form)
	// ---------------------------------------------------------------------------

	static bool isTextSeparator(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
	}

	void CResTextReader::skip()
	{
		while (p_ < s_.size() && (isTextSeparator(s_[p_]) || s_[p_] == '('))
		{
			if (s_[p_] == '(')
			{
				while (p_ < s_.size() && s_[p_] != ')') ++p_;
			}
			++p_;
		}
	}

	std::string CResTextReader::token()
	{
		skip();
		const size_t start = p_;
		while (p_ < s_.size() && !isTextSeparator(s_[p_]) && s_[p_] != '(') ++p_;
		return s_.substr(start, p_ - start);
	}

	long long CResTextReader::integer()
	{
		const std::string t = token();
		if (t.empty()) return 0;
		return std::strtoll(t.c_str(), nullptr, 10);
	}

	std::string CResTextReader::line()
	{
		skip();
		const size_t start = p_;
		while (p_ < s_.size() && s_[p_] != '\r' && s_[p_] != '\n') ++p_;
		return s_.substr(start, p_ - start);
	}

	// ---------------------------------------------------------------------------
	// ATTR
	// ---------------------------------------------------------------------------

	static bool parseAttrBinary(const char * data, int length, PluginAttr & out)
	{
		CResReader r(data, size_t(length));
		out.version = 1;
		out.id = r.fourcc();
		out.attrib = r.u32();
		out.dependID = r.fourcc();
		out.gameLevel = r.i32();
		out.secretItem = r.i32();
		out.itemType = r.i32();
		out.mustItem = r.i32();
		out.name = r.str();
		if (!r.ok() || !r.atEnd()) return false;
		out.isTextForm = false;
		out.valid = true;
		return true;
	}

	static bool parseAttrText(const char * data, int length, PluginAttr & out)
	{
		CResTextReader t(data, size_t(length));
		out.id = FourCC::fromUint32LE(uint32_t(t.integer()));
		out.attrib = uint32_t(t.integer());
		out.dependID = FourCC::fromUint32LE(uint32_t(t.integer()));
		out.gameLevel = int32_t(t.integer());
		out.secretItem = int32_t(t.integer());
		out.itemType = int32_t(t.integer());
		out.mustItem = int32_t(t.integer());
		out.name = t.line();
		if (out.id.isZero() && out.name.empty()) return false;
		out.text.assign(data, size_t(length));
		out.version = 1;
		out.isTextForm = true;
		out.valid = true;
		return true;
	}

	bool parsePluginAttr(const char * data, int length, PluginAttr & out)
	{
		if (!data || length < 1) return false;
		PluginAttr tmp;
		const bool ok = uint8_t(data[0]) == 0x01 ? parseAttrBinary(data, length, tmp)
		                                         : parseAttrText(data, length, tmp);
		if (!ok) return false;
		out = std::move(tmp);
		return true;
	}

	static bool sameAttrValues(const PluginAttr & a, const PluginAttr & b)
	{
		return a.id == b.id && a.attrib == b.attrib && a.dependID == b.dependID
		    && a.gameLevel == b.gameLevel && a.secretItem == b.secretItem
		    && a.itemType == b.itemType && a.mustItem == b.mustItem && a.name == b.name;
	}

	bool encodePluginAttr(const PluginAttr & a, std::string & out)
	{
		if (a.isTextForm)
		{
			// Reuse the original text when it still says the same thing, so the
			// shipped DefaultToolPalette ATTR round-trips byte for byte.
			PluginAttr reparsed;
			if (!a.text.empty() && parsePluginAttr(a.text.data(), int(a.text.size()), reparsed)
			    && reparsed.isTextForm && sameAttrValues(reparsed, a))
			{
				out = a.text;
				return true;
			}
			char buf[256];
			std::snprintf(buf, sizeof(buf),
			              "(mID)\t\t%u\r\n(mAttrib)\t%u\r\n(mDependID)\t%u\r\n(mGameLevel)\t%d\r\n"
			              "(mSecretItem)\t%d\r\n(mItemType)\t%d\r\n(mMustItem)\t%d\r\n(mPluginName)\t",
			              a.id.toUint32LE(), a.attrib, a.dependID.toUint32LE(), a.gameLevel,
			              a.secretItem, a.itemType, a.mustItem);
			out = buf;
			out += a.name;
			out += "\r\n";
			return true;
		}
		CResWriter w;
		w.fourcc(a.id);
		w.u32(a.attrib);
		w.fourcc(a.dependID);
		w.i32(a.gameLevel);
		w.i32(a.secretItem);
		w.i32(a.itemType);
		w.i32(a.mustItem);
		w.str(a.name);
		return w.finish(out);
	}

	// ---------------------------------------------------------------------------
	// DESC
	// ---------------------------------------------------------------------------

	namespace
	{
		struct KindTag { PluginDesc::Kind kind; const char * fileOrder; };
		const KindTag kKindTags[] = {
			{PluginDesc::TenantPlugin,    "lPnT"},
			{PluginDesc::MoverPlugin,     "lPvM"},
			{PluginDesc::ToolPalette,     "LPOT"},
			{PluginDesc::WorldPlugin,     "lPdW"},
			{PluginDesc::OuterObjPlugin,  "lPoO"},
			{PluginDesc::MoviePlugin,     "lPoM"},
			{PluginDesc::HallEventPlugin, "lPeH"},
			{PluginDesc::AdvertisePlugin, "lPdA"},
		};
	}

	PluginDesc::Kind pluginKindFromTag(const FourCC & tag)
	{
		for (const KindTag & k : kKindTags)
			if (tag == k.fileOrder) return k.kind;
		return PluginDesc::Unknown;
	}

	FourCC pluginTagFromKind(PluginDesc::Kind kind)
	{
		for (const KindTag & k : kKindTags)
			if (k.kind == kind) return FourCC::fromFileOrder(k.fileOrder);
		return FourCC();
	}

	bool parsePluginDesc(const char * data, int length, PluginDesc & out)
	{
		if (!data || length < 9) return false;
		CResReader r(data, size_t(length));
		PluginDesc d;
		d.version = 1;
		d.classTag = r.fourcc();
		d.value = r.u32();
		if (!r.ok() || !r.atEnd()) return false;
		d.kind = pluginKindFromTag(d.classTag);
		d.valid = true;
		out = d;
		return true;
	}

	bool encodePluginDesc(const PluginDesc & d, std::string & out)
	{
		CResWriter w;
		w.fourcc(d.classTag);
		w.u32(d.value);
		return w.finish(out);
	}
}
