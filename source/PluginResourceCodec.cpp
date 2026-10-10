/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginResourceCodec.h"

#include <algorithm>
#include <utility>

namespace OT
{
	namespace
	{
		uint16_t readLE16(const unsigned char * p)
		{
			return uint16_t(p[0] | (p[1] << 8));
		}

		void appendLE16(std::string & s, uint16_t v)
		{
			s.push_back(char(v & 0xFF));
			s.push_back(char((v >> 8) & 0xFF));
		}

		bool allZero(const std::string & s)
		{
			for (char c : s)
				if (c != '\0') return false;
			return true;
		}

		size_t align4(size_t p)
		{
			return (p + 3) & ~size_t(3);
		}

		/// UTF-16 code units -> UTF-8 (unpaired surrogates encoded as-is).
		std::string utf16ToUtf8(const std::u16string & s)
		{
			std::string out;
			for (size_t i = 0; i < s.size(); ++i)
			{
				uint32_t cp = s[i];
				if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000)
				{
					cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
					++i;
				}
				if (cp < 0x80) out.push_back(char(cp));
				else if (cp < 0x800)
				{
					out.push_back(char(0xC0 | (cp >> 6)));
					out.push_back(char(0x80 | (cp & 0x3F)));
				}
				else if (cp < 0x10000)
				{
					out.push_back(char(0xE0 | (cp >> 12)));
					out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
					out.push_back(char(0x80 | (cp & 0x3F)));
				}
				else
				{
					out.push_back(char(0xF0 | (cp >> 18)));
					out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
					out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
					out.push_back(char(0x80 | (cp & 0x3F)));
				}
			}
			return out;
		}

		std::u16string bytesToUtf16(const std::string & b)
		{
			std::u16string s;
			for (size_t i = 0; i + 1 < b.size(); i += 2)
				s.push_back(char16_t(uint8_t(b[i]) | (uint8_t(b[i + 1]) << 8)));
			return s;
		}

		void appendUtf16(std::string & out, const std::u16string & s)
		{
			for (char16_t c : s) appendLE16(out, uint16_t(c));
		}

		// ---- VS_VERSIONINFO node tree (mirrors t2p_schema._vs_node) ----
		// `p` and the alignment are absolute offsets into the resource.
		bool readVersionNode(const std::string & d, size_t p, VersionNode & node, size_t & endOut, int depth)
		{
			if (depth > 16 || p + 6 > d.size()) return false;
			const unsigned char * b = reinterpret_cast<const unsigned char *>(d.data());
			node.length = readLE16(b + p);
			node.valueLength = readLE16(b + p + 2);
			node.type = readLE16(b + p + 4);
			const size_t end = p + node.length;
			if (node.length < 6 || end > d.size()) return false;
			size_t q = p + 6;
			size_t k = q;
			while (true)
			{
				if (k + 2 > d.size()) return false;
				if (readLE16(b + k) == 0) break;
				node.key.push_back(char16_t(readLE16(b + k)));
				k += 2;
			}
			q = k + 2;
			size_t a = align4(q);
			if (a > d.size()) return false;
			{
				std::string pad = d.substr(q, a - q);
				if (!allZero(pad)) node.pad1 = pad;
			}
			q = a;
			const size_t vbytes = node.type == 1 ? size_t(node.valueLength) * 2 : node.valueLength;
			if (q + vbytes > d.size()) return false;
			node.value = d.substr(q, vbytes);
			q += vbytes;
			a = align4(q);
			if (a <= end)
			{
				std::string pad = d.substr(q, a - q);
				if (!allZero(pad)) node.pad2 = pad;
				q = a;
			}
			while (q + 6 <= end)
			{
				if (readLE16(b + q) == 0) break;
				VersionNode child;
				size_t childEnd = 0;
				if (!readVersionNode(d, q, child, childEnd, depth + 1)) return false;
				q = childEnd;
				a = std::min(align4(q), end);
				std::string pad = d.substr(q, a - q);
				if (!allZero(pad)) child.padAfter = pad;
				q = a;
				node.children.push_back(std::move(child));
			}
			if (q < end) node.extra = d.substr(q, end - q);
			endOut = end;
			return true;
		}

		/// `base` = absolute offset at which this node starts (for alignment).
		std::string writeVersionNode(const VersionNode & n, size_t base)
		{
			std::string out;
			appendLE16(out, n.length);
			appendLE16(out, n.valueLength);
			appendLE16(out, n.type);
			appendUtf16(out, n.key);
			appendLE16(out, 0);
			size_t pad = (4 - ((base + out.size()) & 3)) & 3;
			if (!n.pad1.empty()) out += n.pad1;
			else out.append(pad, '\0');
			out += n.value;
			if (!n.children.empty() || !n.extra.empty() || !n.pad2.empty())
			{
				pad = (4 - ((base + out.size()) & 3)) & 3;
				if (out.size() + pad <= n.length)
				{
					if (!n.pad2.empty()) out += n.pad2;
					else out.append(pad, '\0');
				}
			}
			for (const VersionNode & c : n.children)
			{
				out += writeVersionNode(c, base + out.size());
				if (!c.padAfter.empty()) out += c.padAfter;
				else
				{
					pad = (4 - ((base + out.size()) & 3)) & 3;
					if (out.size() + pad <= n.length) out.append(pad, '\0');
				}
			}
			out += n.extra;
			return out;
		}

		template <class T>
		bool tryTyped(DecodedResource & r, ResourceCodec codec, const char * data, size_t length,
		              bool (*decode)(const char *, size_t, T &))
		{
			T v{};
			if (!decode(data, length, v)) return false;
			r.codec = codec;
			r.value = std::move(v);
			return true;
		}

		bool isTextType(const std::string & t)
		{
			return t == "OBJMAP" || t == "T2DLG" || t == "TENANTDEF";
		}
	}

	// -------------------------------------------------------------------------
	// RT_STRING
	// -------------------------------------------------------------------------

	bool decodeStringBlock(const char * data, size_t length, StringBlock & out)
	{
		if (!data && length) return false;
		const unsigned char * b = reinterpret_cast<const unsigned char *>(data);
		StringBlock blk;
		size_t p = 0;
		while (p + 2 <= length && blk.strings.size() < 16)
		{
			const size_t n = readLE16(b + p);
			if (p + 2 + 2 * n > length) return false;
			std::u16string s;
			for (size_t i = 0; i < n; ++i) s.push_back(char16_t(readLE16(b + p + 2 + 2 * i)));
			blk.strings.push_back(std::move(s));
			p += 2 + 2 * n;
		}
		blk.trailing.assign(data + p, length - p);
		out = std::move(blk);
		return true;
	}

	bool encodeStringBlock(const StringBlock & block, std::string & out)
	{
		std::string s;
		for (const std::u16string & str : block.strings)
		{
			if (str.size() > 0xFFFF) return false;
			appendLE16(s, uint16_t(str.size()));
			appendUtf16(s, str);
		}
		s += block.trailing;
		out = std::move(s);
		return true;
	}

	// -------------------------------------------------------------------------
	// RT_VERSION
	// -------------------------------------------------------------------------

	std::string VersionNode::keyString() const
	{
		return utf16ToUtf8(key);
	}

	std::string VersionNode::textValue() const
	{
		std::u16string s = bytesToUtf16(value);
		while (!s.empty() && s.back() == 0) s.pop_back();
		return utf16ToUtf8(s);
	}

	const VersionNode * VersionNode::find(const std::string & k) const
	{
		for (const VersionNode & c : children)
		{
			if (c.keyString() == k) return &c;
			if (const VersionNode * f = c.find(k)) return f;
		}
		return nullptr;
	}

	bool decodeVersionInfo(const char * data, size_t length, VersionInfo & out)
	{
		if (!data) return false;
		const std::string d(data, length);
		VersionInfo info;
		size_t end = 0;
		if (!readVersionNode(d, 0, info.root, end, 0)) return false;
		info.trailing = d.substr(end);
		// Only accept trees that reproduce the input exactly.
		std::string again;
		if (!encodeVersionInfo(info, again) || again != d) return false;
		out = std::move(info);
		return true;
	}

	bool encodeVersionInfo(const VersionInfo & info, std::string & out)
	{
		out = writeVersionNode(info.root, 0) + info.trailing;
		return true;
	}

	// -------------------------------------------------------------------------
	// Dispatcher
	// -------------------------------------------------------------------------

	const char * resourceCodecName(ResourceCodec c)
	{
		switch (c)
		{
			case ResourceCodec::Raw:                  return "Raw";
			case ResourceCodec::Text:                 return "Text";
			case ResourceCodec::Attr:                 return "Attr";
			case ResourceCodec::Desc:                 return "Desc";
			case ResourceCodec::TemplateDef:          return "TemplateDef";
			case ResourceCodec::MemberTable:          return "MemberTable";
			case ResourceCodec::DayParam:             return "DayParam";
			case ResourceCodec::PlaceParam:           return "PlaceParam";
			case ResourceCodec::PeopleAnime:          return "PeopleAnime";
			case ResourceCodec::Silhouettes:          return "Silhouettes";
			case ResourceCodec::People:               return "People";
			case ResourceCodec::Words:                return "Words";
			case ResourceCodec::StringTable:          return "StringTable";
			case ResourceCodec::Movie:                return "Movie";
			case ResourceCodec::HallEvent:            return "HallEvent";
			case ResourceCodec::Advertise:            return "Advertise";
			case ResourceCodec::SubPluginPeopleParam: return "SubPluginPeopleParam";
			case ResourceCodec::StringBlock:          return "StringBlock";
			case ResourceCodec::VersionInfo:          return "VersionInfo";
			case ResourceCodec::WorldRecord:          return "WorldRecord";
			case ResourceCodec::BkgndInfo:            return "BkgndInfo";
			case ResourceCodec::FloorInfoMask:        return "FloorInfoMask";
			case ResourceCodec::PaletteAnime:         return "PaletteAnime";
			case ResourceCodec::Palette:              return "Palette";
		}
		return "?";
	}

	std::string resourceTypeDisplayName(const std::string & type)
	{
		if (type == "1")  return "RT_CURSOR";
		if (type == "2")  return "RT_BITMAP";
		if (type == "3")  return "RT_ICON";
		if (type == "4")  return "RT_MENU";
		if (type == "5")  return "RT_DIALOG";
		if (type == "6")  return "RT_STRING";
		if (type == "9")  return "RT_ACCELERATOR";
		if (type == "10") return "RT_RCDATA";
		if (type == "14") return "RT_GROUP_ICON";
		if (type == "16") return "RT_VERSION";
		return type;
	}

	ResourceCodec codecForResourceType(const std::string & type, ResourceContainer container)
	{
		const std::string t = resourceTypeDisplayName(type);
		if (t == "ATTR") return ResourceCodec::Attr;
		if (t == "DESC") return ResourceCodec::Desc;
		if (t == "TNDF" || t == "MVDF" || t == "OODF") return ResourceCodec::TemplateDef;
		if (t == "TMDF") return ResourceCodec::MemberTable;
		if (t == "DPDF") return ResourceCodec::DayParam;
		if (t == "PLDF") return ResourceCodec::PlaceParam;
		if (t == "PADF") return ResourceCodec::PeopleAnime;
		if (t == "SLDF") return ResourceCodec::Silhouettes;
		if (t == "PPDF") return container == ResourceContainer::SP ? ResourceCodec::SubPluginPeopleParam
		                                                           : ResourceCodec::People;
		if (t == "WODF") return ResourceCodec::Words;
		if (t == "STRTABLE" || t == "STRTBL") return ResourceCodec::StringTable;
		if (t == "MODF") return ResourceCodec::Movie;
		if (t == "HEDF") return ResourceCodec::HallEvent;
		if (t == "ADDF") return ResourceCodec::Advertise;
		if (t == "RT_STRING") return ResourceCodec::StringBlock;
		if (t == "RT_VERSION") return ResourceCodec::VersionInfo;
		if (container == ResourceContainer::PE)
		{
			WorldSchema ws;
			if (worldSchemaForType(t, ws)) return ResourceCodec::WorldRecord;
			if (t == "BINF") return ResourceCodec::BkgndInfo;
			if (t == "FINF") return ResourceCodec::FloorInfoMask;
			if (t == "PTDF") return ResourceCodec::PaletteAnime;
			if (t == "PALETTE") return ResourceCodec::Palette;
		}
		if (isTextType(t)) return ResourceCodec::Text;
		return ResourceCodec::Raw;
	}

	DecodedResource decodeResource(const std::string & type, int id, const char * data, size_t length,
	                               ResourceContainer container)
	{
		DecodedResource r;
		r.type = type;
		r.id = id;
		const ResourceCodec want = codecForResourceType(type, container);
		bool done = false;
		switch (want)
		{
			case ResourceCodec::Attr:
			{
				PluginAttr a;
				if (parsePluginAttr(data, int(length), a)) { r.codec = want; r.value = std::move(a); done = true; }
				break;
			}
			case ResourceCodec::Desc:
			{
				PluginDesc d;
				if (parsePluginDesc(data, int(length), d)) { r.codec = want; r.value = d; done = true; }
				break;
			}
			case ResourceCodec::TemplateDef:
			{
				const std::string t = type;
				const TemplateDef::Kind k = t == "MVDF" ? TemplateDef::Mover
				                          : t == "OODF" ? TemplateDef::OuterObj : TemplateDef::Tenant;
				TemplateDef def;
				if (decodeTemplateDef(k, data, length, def)) { r.codec = want; r.value = std::move(def); done = true; }
				break;
			}
			case ResourceCodec::MemberTable: done = tryTyped(r, want, data, length, &decodeMemberTableDef); break;
			case ResourceCodec::DayParam:    done = tryTyped(r, want, data, length, &decodeDayParamDef); break;
			case ResourceCodec::PlaceParam:  done = tryTyped(r, want, data, length, &decodePlaceParamDef); break;
			case ResourceCodec::PeopleAnime: done = tryTyped(r, want, data, length, &decodePeopleAnimeDef); break;
			case ResourceCodec::Silhouettes: done = tryTyped(r, want, data, length, &decodeSilhouetteList); break;
			case ResourceCodec::People:      done = tryTyped(r, want, data, length, &decodePeopleList); break;
			case ResourceCodec::Words:       done = tryTyped(r, want, data, length, &decodeWordList); break;
			case ResourceCodec::StringTable: done = tryTyped(r, want, data, length, &decodeStringTableDef); break;
			case ResourceCodec::Movie:       done = tryTyped(r, want, data, length, &decodeMovieDef); break;
			case ResourceCodec::HallEvent:   done = tryTyped(r, want, data, length, &decodeHallEventDef); break;
			case ResourceCodec::Advertise:   done = tryTyped(r, want, data, length, &decodeAdvertiseDef); break;
			case ResourceCodec::SubPluginPeopleParam:
				done = tryTyped(r, want, data, length, &decodeSubPluginPeopleParamDef);
				break;
			case ResourceCodec::StringBlock: done = tryTyped(r, want, data, length, &decodeStringBlock); break;
			case ResourceCodec::VersionInfo: done = tryTyped(r, want, data, length, &decodeVersionInfo); break;
			case ResourceCodec::WorldRecord:
			{
				WorldSchema ws = WorldSchema::Event;
				Json rec;
				if (worldSchemaForType(type, ws) && decodeWorldSchemaResource(ws, type, data, length, rec))
				{
					r.codec = want;
					r.value = std::move(rec);
					done = true;
				}
				break;
			}
			case ResourceCodec::BkgndInfo:     done = tryTyped(r, want, data, length, &decodeBkgndInfoDef); break;
			case ResourceCodec::FloorInfoMask: done = tryTyped(r, want, data, length, &decodeFloorInfoMaskDef); break;
			case ResourceCodec::PaletteAnime:  done = tryTyped(r, want, data, length, &decodePaletteAnimeDef); break;
			case ResourceCodec::Palette:       done = tryTyped(r, want, data, length, &decodePaletteDef); break;
			case ResourceCodec::Text:
				r.codec = ResourceCodec::Text;
				r.bytes.assign(data, length);
				done = true;
				break;
			case ResourceCodec::Raw:
				break;
		}
		if (!done)
		{
			r.codec = ResourceCodec::Raw;
			r.value = std::monostate{};
			r.bytes.assign(data ? data : "", data ? length : 0);
		}
		return r;
	}

	bool encodeResource(const DecodedResource & r, std::string & out)
	{
		switch (r.codec)
		{
			case ResourceCodec::Raw:
			case ResourceCodec::Text:
				out = r.bytes;
				return true;
			case ResourceCodec::Attr:        return encodePluginAttr(std::get<PluginAttr>(r.value), out);
			case ResourceCodec::Desc:        return encodePluginDesc(std::get<PluginDesc>(r.value), out);
			case ResourceCodec::TemplateDef: return encodeTemplateDef(std::get<TemplateDef>(r.value), out);
			case ResourceCodec::MemberTable: return encodeMemberTableDef(std::get<MemberTableDef>(r.value), out);
			case ResourceCodec::DayParam:    return encodeDayParamDef(std::get<DayParamDef>(r.value), out);
			case ResourceCodec::PlaceParam:  return encodePlaceParamDef(std::get<PlaceParamDef>(r.value), out);
			case ResourceCodec::PeopleAnime: return encodePeopleAnimeDef(std::get<PeopleAnimeDef>(r.value), out);
			case ResourceCodec::Silhouettes: return encodeSilhouetteList(std::get<SilhouetteList>(r.value), out);
			case ResourceCodec::People:      return encodePeopleList(std::get<PeopleList>(r.value), out);
			case ResourceCodec::Words:       return encodeWordList(std::get<WordList>(r.value), out);
			case ResourceCodec::StringTable: return encodeStringTableDef(std::get<StringTableDef>(r.value), out);
			case ResourceCodec::Movie:       return encodeMovieDef(std::get<MovieDef>(r.value), out);
			case ResourceCodec::HallEvent:   return encodeHallEventDef(std::get<HallEventDef>(r.value), out);
			case ResourceCodec::Advertise:   return encodeAdvertiseDef(std::get<AdvertiseDef>(r.value), out);
			case ResourceCodec::SubPluginPeopleParam:
				return encodeSubPluginPeopleParamDef(std::get<SubPluginPeopleParamDef>(r.value), out);
			case ResourceCodec::StringBlock: return encodeStringBlock(std::get<StringBlock>(r.value), out);
			case ResourceCodec::VersionInfo: return encodeVersionInfo(std::get<VersionInfo>(r.value), out);
			case ResourceCodec::WorldRecord:
			{
				WorldSchema ws = WorldSchema::Event;
				return worldSchemaForType(r.type, ws)
				    && encodeWorldSchemaResource(ws, r.type, std::get<Json>(r.value), out);
			}
			case ResourceCodec::BkgndInfo:     return encodeBkgndInfoDef(std::get<BkgndInfoDef>(r.value), out);
			case ResourceCodec::FloorInfoMask: return encodeFloorInfoMaskDef(std::get<FloorInfoMaskDef>(r.value), out);
			case ResourceCodec::PaletteAnime:  return encodePaletteAnimeDef(std::get<PaletteAnimeDef>(r.value), out);
			case ResourceCodec::Palette:       return encodePaletteDef(std::get<PaletteDef>(r.value), out);
		}
		return false;
	}
}
