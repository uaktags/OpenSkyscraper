/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "OspPlugin.h"
#include "PluginBitmapLoader.h"
#include "PluginResourceJson.h"
#include "ResourceJson.h"
#include "PluginManager.h"
#include "WindowsPEExecutable.h"

#include <SFML/Graphics/Color.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace
{
	// -------------------------------------------------------------------------
	// Filesystem helpers (POSIX; mirrors the project's existing style). The
	// build also targets Windows but POSIX file IO is already used widely in
	// source/ and keeps the seam free of <filesystem> ABI quirks.
	// -------------------------------------------------------------------------
	bool readTextFile(const std::string & path, std::string & out)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f) return false;
		std::ostringstream ss;
		ss << f.rdbuf();
		out = ss.str();
		return true;
	}

	bool writeTextFile(const std::string & path, const std::string & content)
	{
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (!f) return false;
		f.write(content.data(), static_cast<std::streamsize>(content.size()));
		return f.good();
	}

	bool dirExists(const std::string & path)
	{
		struct stat st{};
		return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
	}

	bool fileExists(const std::string & path)
	{
		struct stat st{};
		return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
	}

	bool makeDirs(const std::string & path)
	{
		// Creates each path component (mkdir -p). Leading '/' is fine.
		std::string acc;
		for (size_t i = 0; i < path.size(); ++i)
		{
			char c = path[i];
			acc += c;
			if (c == '/' && acc.size() > 1)
			{
				::mkdir(acc.c_str(), 0755);  // ignore EEXIST
			}
		}
		::mkdir(path.c_str(), 0755);  // final component
		return dirExists(path);
	}

	std::string join(const std::string & a, const std::string & b)
	{
		if (a.empty()) return b;
		if (a.back() == '/') return a + b;
		return a + "/" + b;
	}

	std::string classToString(OT::PluginDesc::Kind k)
	{
		switch (k)
		{
			case OT::PluginDesc::TenantPlugin:   return "Tenant";
			case OT::PluginDesc::MoverPlugin:    return "Mover";
			case OT::PluginDesc::ToolPalette:    return "ToolPalette";
			case OT::PluginDesc::WorldPlugin:    return "World";
			case OT::PluginDesc::OuterObjPlugin: return "OuterObj";
			case OT::PluginDesc::MoviePlugin:    return "Movie";
			case OT::PluginDesc::HallEventPlugin: return "HallEvent";
			case OT::PluginDesc::AdvertisePlugin: return "Advertise";
			default:                             return "Unknown";
		}
	}

	bool classFromString(const std::string & s, OT::PluginDesc::Kind & out)
	{
		if (s == "Tenant")    { out = OT::PluginDesc::TenantPlugin;   return true; }
		if (s == "Mover")     { out = OT::PluginDesc::MoverPlugin;    return true; }
		if (s == "ToolPalette"){ out = OT::PluginDesc::ToolPalette;   return true; }
		if (s == "World")     { out = OT::PluginDesc::WorldPlugin;    return true; }
		if (s == "OuterObj")  { out = OT::PluginDesc::OuterObjPlugin; return true; }
		if (s == "Movie")     { out = OT::PluginDesc::MoviePlugin;    return true; }
		if (s == "HallEvent") { out = OT::PluginDesc::HallEventPlugin; return true; }
		if (s == "Advertise") { out = OT::PluginDesc::AdvertisePlugin; return true; }
		if (s == "Unknown")   { out = OT::PluginDesc::Unknown;        return true; }
		return false;
	}

	std::string maskHowToString(OT::MaskDiscovery m)
	{
		switch (m)
		{
			case OT::MaskDiscovery::Manual: return "manual";
			case OT::MaskDiscovery::Corner: return "corner";
			default:                        return "none";
		}
	}

	// -------------------------------------------------------------------------
	// Minimal JSON: read and write ONLY the flat OSP schema. This is not a
	// general JSON parser. The schema is small and fixed, so a purpose-built
	// ~150-line reader avoids vendoring a JSON library.
	//
	// Reader strategy: scan for `"key"` then the next `:`, then read a value
	// which may be a number, a quoted string, an array of 3 numbers [r,g,b],
	// `null`, `true`, or `false`. Braces/brackets are tracked only enough to
	// descend into the known top-level objects (attr/placement/price/bitmaps).
	// -------------------------------------------------------------------------

	/// Locates the value (as a substring) following `"key" :` in `s`, starting
	/// the search at `from`. Returns the index just past the value's opening
	/// (for scalars, the start of the token; for arrays, the index of '[').
	/// Returns std::string::npos if not found.
	size_t findKeyValue(const std::string & s, const std::string & key, size_t from)
	{
		std::string needle = "\"" + key + "\"";
		size_t k = s.find(needle, from);
		if (k == std::string::npos) return std::string::npos;
		k += needle.size();
		// Skip whitespace and exactly one ':'.
		while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) ++k;
		if (k >= s.size() || s[k] != ':') return std::string::npos;
		++k;
		while (k < s.size() && (s[k] == ' ' || s[k] == '\t' || s[k] == '\n' || s[k] == '\r')) ++k;
		return k;
	}

	bool readNumber(const std::string & s, size_t & i, long long & out)
	{
		size_t start = i;
		if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
		bool any = false;
		while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-'))
		{
			++i; any = true;
		}
		if (!any) { i = start; return false; }
		out = std::strtoll(s.c_str() + start, nullptr, 10);
		return true;
	}

	/// Reads a value into a long long. For arrays like [r,g,b] we read the
	/// 1st element only; callers needing the color use readColor.
	bool readScalarInt(const std::string & s, const std::string & key, size_t scope, long long & out)
	{
		size_t i = findKeyValue(s, key, scope);
		if (i == std::string::npos) return false;
		if (i < s.size() && s[i] == '[')
		{
			++i;
			while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
			return readNumber(s, i, out);
		}
		return readNumber(s, i, out);
	}

	bool readColor(const std::string & s, const std::string & key, size_t scope, sf::Color & out)
	{
		size_t i = findKeyValue(s, key, scope);
		if (i == std::string::npos || i >= s.size() || s[i] != '[') return false;
		++i;
		long long rgb[3] = {0, 0, 0};
		for (int c = 0; c < 3; ++c)
		{
			while (i < s.size() && (s[i] == ' ' || s[i] == ',' || s[i] == '\t')) ++i;
			if (!readNumber(s, i, rgb[c])) return false;
		}
		out = sf::Color(uint8_t(rgb[0]), uint8_t(rgb[1]), uint8_t(rgb[2]));
		return true;
	}

	bool isNullValue(const std::string & s, const std::string & key, size_t scope)
	{
		size_t i = findKeyValue(s, key, scope);
		return i != std::string::npos && i + 3 < s.size() + 1 && s.compare(i, 4, "null") == 0;
	}

	/// Reads a quoted-string value following `"key":`. Strips surrounding
	/// quotes and unescapes \" and \\. Returns false if absent / not a string.
	bool readString(const std::string & s, const std::string & key, size_t scope, std::string & out)
	{
		size_t i = findKeyValue(s, key, scope);
		if (i == std::string::npos || i >= s.size() || s[i] != '"') return false;
		++i;
		std::string val;
		while (i < s.size() && s[i] != '"')
		{
			if (s[i] == '\\' && i + 1 < s.size()) { val.push_back(s[i + 1]); i += 2; }
			else { val.push_back(s[i]); ++i; }
		}
		out = val;
		return true;
	}

	/// Reads every number of the (possibly nested) array following `"key":`,
	/// flattened in document order: [[70, 50], [100, 80]] -> 70 50 100 80.
	bool readIntArray(const std::string & s, const std::string & key, size_t scope, std::vector<long long> & out)
	{
		size_t i = findKeyValue(s, key, scope);
		if (i == std::string::npos || i >= s.size() || s[i] != '[') return false;
		out.clear();
		int depth = 0;
		while (i < s.size())
		{
			const char c = s[i];
			if (c == '[') { ++depth; ++i; }
			else if (c == ']') { ++i; if (--depth == 0) return true; }
			else if (c == '-' || (c >= '0' && c <= '9'))
			{
				long long v = 0;
				if (!readNumber(s, i, v)) return false;
				out.push_back(v);
			}
			else ++i;
		}
		return false;
	}

	/// Finds the opening brace of the object named `key` (e.g. "attr"), returns
	/// the index of '{' (the scope to search within), or npos.
	size_t findObject(const std::string & s, const std::string & key, size_t from)
	{
		size_t i = findKeyValue(s, key, from);
		if (i == std::string::npos || i >= s.size() || s[i] != '{') return std::string::npos;
		return i;
	}

	// ---- writer helpers (emits compact, deterministic JSON) ----
	void emitInt(std::string & s, const char * key, long long v, const char * indent)
	{
		s += indent; s += "\""; s += key; s += "\": "; s += std::to_string(v);
	}
	void emitStr(std::string & s, const char * key, const std::string & v, const char * indent)
	{
		s += indent; s += "\""; s += key; s += "\": \"";
		for (char c : v)
		{
			if (c == '"' || c == '\\') s.push_back('\\');
			s.push_back(c);
		}
		s += "\"";
	}
	void emitColor(std::string & s, const char * key, const sf::Color & c, const char * indent)
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%s\"%s\": [%d, %d, %d]", indent, key, c.r, c.g, c.b);
		s += buf;
	}
	void emitNullColor(std::string & s, const char * key, const char * indent)
	{
		s += indent; s += "\""; s += key; s += "\": null";
	}
}

namespace OT
{
	// OSP v3 resource store (defined at the end of this file).
	static OspResult loadOspResources(const std::string & dir, const std::string & manifest, OspPlugin & p);
	static OspResult writeOspResource(const std::string & dir, const OspPlugin & p, const OspResource & r, Json & entry);

	bool isOspPluginDir(const std::string & dir)
	{
		return dirExists(dir) && fileExists(join(dir, "plugin.json"));
	}

	bool readOspAttr(const std::string & json, PluginAttr & out)
	{
		const size_t scope = findObject(json, "attr", 0);
		if (scope == std::string::npos) return false;

		PluginAttr a;
		a.dependID = FourCC::fromFileOrder("****");
		long long v = 0;
		std::string s;

		// Plugin id (ATTR mID). v2: Mac order. v1 called it "itemType" and
		// stored the file-order bytes.
		if (readString(json, "pluginId", scope, s))      a.id = FourCC::fromMacOrder(s);
		else if (readString(json, "itemType", scope, s)) a.id = FourCC::fromFileOrder(s);

		// Numeric item type (ATTR mItemType). v1 called it "id".
		if (readScalarInt(json, "itemType", scope, v))   a.itemType = int32_t(v);
		else if (readScalarInt(json, "id", scope, v))    a.itemType = int32_t(v);

		// Dependency (ATTR mDependID). v1: "marker" (file order) with a
		// numeric "dependID" that was never populated from binary plugins.
		if (readString(json, "dependPluginId", scope, s)) a.dependID = FourCC::fromMacOrder(s);
		else if (readString(json, "marker", scope, s) && s.size() >= 4) a.dependID = FourCC::fromFileOrder(s);
		else if (readScalarInt(json, "dependID", scope, v) && v != 0) a.dependID = FourCC::fromUint32LE(uint32_t(v));

		if (readString(json, "name", scope, s))             a.name = s;
		if (readScalarInt(json, "gameLevel", scope, v))     a.gameLevel = int32_t(v);
		if (readScalarInt(json, "attrib", scope, v))        a.attrib = uint32_t(v);
		if (readScalarInt(json, "secretItem", scope, v))    a.secretItem = int32_t(v);
		if (readScalarInt(json, "mustItem", scope, v))      a.mustItem = int32_t(v);
		a.valid = true;
		out = std::move(a);
		return true;
	}

	// -------------------------------------------------------------------------
	// loadOspPlugin
	// -------------------------------------------------------------------------
	OspResult loadOspPlugin(const std::string & dir, OspPlugin & out)
	{
		if (!dirExists(dir))
			return {false, "Directory does not exist: " + dir};

		const std::string manifestPath = join(dir, "plugin.json");
		std::string json;
		if (!readTextFile(manifestPath, json))
			return {false, "Missing plugin.json in " + dir};

		// Format marker + version (must be exactly our schema).
		std::string fmt;
		if (!readString(json, "format", 0, fmt) || fmt != "openskyscraper-plugin")
			return {false, "Not an OpenSkyscraper plugin (missing/wrong 'format')"};
		long long ver = 0;
		if (!readScalarInt(json, "formatVersion", 0, ver))
			return {false, "Missing formatVersion"};
		if (ver < kOspMinFormatVersion || ver > kOspFormatVersion)
			return {false, "Unsupported OSP version " + std::to_string(ver) + " (expected "
			               + std::to_string(kOspMinFormatVersion) + ".." + std::to_string(kOspFormatVersion) + ")"};

		OspPlugin p;  // build into a temp; only commit on full success.
		p.sourceFormat = "openskyscraper-plugin";
		readString(json, "sourceFile", 0, p.sourceFile);

		std::string cls;
		if (readString(json, "class", 0, cls))
		{
			if (!classFromString(cls, p.pluginClass))
				return {false, "Unknown plugin class: " + cls};
		}

		// ATTR (required).
		if (!readOspAttr(json, p.attr))
			return {false, "Missing 'attr' object"};

		// DPDF day/weather table (optional). v2: "dayParam": {"percent":
		// [[d0w0, d0w1], [d1w0, d1w1]]}. v1 mislabeled the same four slots as
		// "placement": {w, h, baseX, baseY}.
		const size_t dayScope = findObject(json, "dayParam", 0);
		if (dayScope != std::string::npos)
		{
			std::vector<long long> v;
			if (readIntArray(json, "percent", dayScope, v) && v.size() == 4)
			{
				DayParamDef d;
				for (int i = 0; i < 4; ++i) d.percent[i / 2][i % 2] = uint32_t(v[size_t(i)]);
				p.dayParam = d;
			}
		}
		else
		{
			const size_t placScope = findObject(json, "placement", 0);
			if (placScope != std::string::npos)
			{
				DayParamDef d;
				long long v;
				if (readScalarInt(json, "w", placScope, v))     d.percent[0][0] = uint32_t(v);
				if (readScalarInt(json, "h", placScope, v))     d.percent[0][1] = uint32_t(v);
				if (readScalarInt(json, "baseX", placScope, v)) d.percent[1][0] = uint32_t(v);
				if (readScalarInt(json, "baseY", placScope, v)) d.percent[1][1] = uint32_t(v);
				p.dayParam = d;
			}
		}

		// PLDF place table (optional). v2: "placeParam": {"places":
		// [[percent, score] x 3]}. v1 mislabeled the six slots as "price":
		// {f1, basePrice, f2, f3, f4, f5}.
		const size_t placeScope = findObject(json, "placeParam", 0);
		if (placeScope != std::string::npos)
		{
			std::vector<long long> v;
			if (readIntArray(json, "places", placeScope, v) && v.size() == 6)
			{
				PlaceParamDef d;
				for (int i = 0; i < 3; ++i)
				{
					d.places[i].percent = uint32_t(v[size_t(2 * i)]);
					d.places[i].scoreSlot = int32_t(v[size_t(2 * i + 1)]);
				}
				p.placeParam = d;
			}
		}
		else
		{
			const size_t priceScope = findObject(json, "price", 0);
			if (priceScope != std::string::npos)
			{
				static const char * const kV1Keys[6] = {"f1", "basePrice", "f2", "f3", "f4", "f5"};
				PlaceParamDef d;
				for (int i = 0; i < 6; ++i)
				{
					long long v = 0;
					if (!readScalarInt(json, kV1Keys[i], priceScope, v)) continue;
					if (i % 2 == 0) d.places[i / 2].percent = uint32_t(v);
					else            d.places[i / 2].scoreSlot = int32_t(v);
				}
				p.placeParam = d;
			}
		}

		// Sprite sheets. Each entry under "bitmaps" may carry a
		// transparentColor (null or [r,g,b]) and a "relPath" pointing at its
		// pixels. The pixel path resolves as: the manifest-declared relPath,
		// else the canonical name (sprites/<sheet>.png), else the numeric
		// RT_BITMAP id name (sprites/<id>.png) used by the shipped OpenTower
		// plugins and the game side (DecompiledPlugin). `sheet.relPath`
		// records whichever path actually loaded so saveOspPlugin writes
		// the pixels back where they came from.
		auto loadSheet = [&](const char * key, OspSpriteSheet & sheet,
		                     const char * canonicalRel, const char * numericRel,
		                     bool tolerateMissing) -> bool {
			size_t scope = findObject(json, key, 0);
			if (scope == std::string::npos) return true;  // absent => not present
			// The writer records explicit "present": false entries for absent
			// sheets; honor them (an object being there is not enough).
			{
				size_t pi = findKeyValue(json, "present", scope);
				if (pi != std::string::npos && json.compare(pi, 5, "false") == 0)
					return true;
			}
			sheet.present = true;
			sf::Color c;
			if (readColor(json, "transparentColor", scope, c)) { sheet.mask = c; sheet.maskHow = MaskDiscovery::Manual; }
			else sheet.maskHow = MaskDiscovery::None;
			std::string rel;
			if (!readString(json, "relPath", scope, rel) || rel.empty())
				rel = fileExists(join(dir, canonicalRel)) ? canonicalRel : numericRel;
			sheet.relPath = rel;
			// Load pixels. A declared-but-missing sheet is fatal for the two
			// sheets every tenant needs and tolerable for the optional ones.
			if (!sheet.image.loadFromFile(join(dir, rel)))
			{
				if (!tolerateMissing) return false;
				sheet.present = false;
				sheet.relPath.clear();
				sheet.mask.reset();
				return true;
			}
			if (sheet.mask) sheet.image.createMaskFromColor(*sheet.mask);
			return true;
		};
		if (!loadSheet("toolbox", p.toolbox, "sprites/toolbox.png", "sprites/100.png", false))
			return {false, "Failed to load sprites/toolbox.png"};
		if (!loadSheet("ingame", p.ingame, "sprites/ingame.png", "sprites/1000.png", false))
			return {false, "Failed to load sprites/ingame.png"};
		loadSheet("occupants", p.occupants, "sprites/occupants.png", "sprites/1100.png", true);
		// Walking frames (Mover plugins). Absent for tenants — tolerate.
		loadSheet("walking", p.walking, "sprites/walking.png", "sprites/200.png", true);

		// MVDF transport definition (optional; Mover plugins).
		size_t mvdfScope = findObject(json, "transport", 0);
		if (mvdfScope != std::string::npos)
		{
			std::string ps, qs, nm;
			if (readString(json, "priceString", mvdfScope, ps)) p.transport.priceString = ps;
			if (readString(json, "outMoneyString", mvdfScope, qs)
			    || readString(json, "quarterlyString", mvdfScope, qs))  // v1 key
				p.transport.outMoneyString = qs;
			if (readString(json, "name", mvdfScope, nm)) p.transport.name = nm;
			p.transport.valid = true;
		}

		// OBJMAP layouts (optional; parsed from meta/).
		auto loadObjmap = [&](const std::string & relPath, std::vector<ObjmapBlock> & blocks) {
			const std::string p2 = join(dir, relPath);
			std::string txt;
			if (readTextFile(p2, txt) && !txt.empty())
				parsePluginObjmap(txt.data(), int(txt.size()), blocks);
		};
		loadObjmap("meta/objmap_100.txt", p.toolbox.blocks);
		loadObjmap("meta/objmap_1000.txt", p.ingame.blocks);
		loadObjmap("meta/objmap_200.txt", p.walking.blocks);  // Mover walking frames

		// v3: the lossless resource store. The v2 view is then re-derived
		// from the resources so both always agree.
		if (ver >= 3)
		{
			const OspResult rr = loadOspResources(dir, json, p);
			if (!rr.ok) return rr;
			if (!p.resources.empty()) syncOspLegacyFromResources(p);
		}

		out = std::move(p);
		return {true, ""};
	}

	// -------------------------------------------------------------------------
	// saveOspPlugin
	// -------------------------------------------------------------------------
	OspResult saveOspPlugin(const std::string & dir, const OspPlugin & plugin, bool overwrite)
	{
		// A v3 plugin's v2 blocks are derived from its resources.
		OspPlugin synced;
		const OspPlugin * pp = &plugin;
		if (!plugin.resources.empty())
		{
			synced = plugin;
			syncOspLegacyFromResources(synced);
			pp = &synced;
		}
		const OspPlugin & p = *pp;
		if (dirExists(dir) && !isOspPluginDir(dir) && !overwrite)
			return {false, "Directory exists and is not an OSP plugin: " + dir};
		if (!makeDirs(dir))                 return {false, "Cannot create directory: " + dir};
		if (!makeDirs(join(dir, "sprites"))) return {false, "Cannot create sprites/"};
		if (!makeDirs(join(dir, "meta")))    return {false, "Cannot create meta/"};

		// --- manifest ---
		std::string s;
		s += "{\n";
		emitStr(s, "format", "openskyscraper-plugin", "  "); s += ",\n";
		emitInt(s, "formatVersion", kOspFormatVersion, "  "); s += ",\n";
		emitStr(s, "sourceFormat", p.sourceFormat, "  "); s += ",\n";
		if (!p.sourceFile.empty()) { emitStr(s, "sourceFile", p.sourceFile, "  "); s += ",\n"; }
		emitStr(s, "class", classToString(p.pluginClass), "  "); s += ",\n";

		// attr
		// attr (ATTR id 128). 4ccs are written in Mac order ("BURG").
		s += "  \"attr\": {\n";
		emitStr(s, "pluginId", p.attr.id.macStr(), "    "); s += ",\n";
		emitInt(s, "itemType", p.attr.itemType, "    "); s += ",\n";
		emitStr(s, "name", p.attr.name, "    "); s += ",\n";
		emitInt(s, "gameLevel", p.attr.gameLevel, "    "); s += ",\n";
		emitInt(s, "attrib", p.attr.attrib, "    "); s += ",\n";
		emitStr(s, "dependPluginId", p.attr.dependID.macStr(), "    "); s += ",\n";
		emitInt(s, "secretItem", p.attr.secretItem, "    "); s += ",\n";
		emitInt(s, "mustItem", p.attr.mustItem, "    "); s += "\n";
		s += "  },\n";

		// dayParam (optional; DPDF 128): percent[EDay][EWhether]
		if (p.dayParam)
		{
			const DayParamDef & d = *p.dayParam;
			s += "  \"dayParam\": {\n";
			s += "    \"percent\": [[" + std::to_string(d.percent[0][0]) + ", " + std::to_string(d.percent[0][1])
			   + "], [" + std::to_string(d.percent[1][0]) + ", " + std::to_string(d.percent[1][1]) + "]]\n";
			s += "  },\n";
		}

		// placeParam (optional; PLDF 128): [percent, score] per EPlace
		if (p.placeParam)
		{
			const PlaceParamDef & d = *p.placeParam;
			s += "  \"placeParam\": {\n";
			s += "    \"places\": [";
			for (int i = 0; i < 3; ++i)
			{
				if (i) s += ", ";
				s += "[" + std::to_string(d.places[i].percent) + ", " + std::to_string(d.places[i].scoreSlot) + "]";
			}
			s += "]\n";
			s += "  },\n";
		}

		// transport (optional; MVDF — movers). Only the human-readable strings
		// of the first variation are part of the manifest so far.
		if (p.transport.valid)
		{
			s += "  \"transport\": {\n";
			emitStr(s, "priceString",    p.transport.priceString,    "    "); s += ",\n";
			emitStr(s, "outMoneyString", p.transport.outMoneyString, "    "); s += ",\n";
			emitStr(s, "name",           p.transport.name,           "    "); s += "\n";
			s += "  },\n";
		}

		// bitmaps
		s += "  \"bitmaps\": {\n";
		auto emitSheet = [&](const char * key, const OspSpriteSheet & sh, bool last) {
			s += "    \""; s += key; s += "\": { ";
			s += "\"present\": "; s += (sh.present ? "true" : "false"); s += ", ";
			// Record where the pixels live so a reload (editor or game-side
			// manifest reader) finds the sheet even under numeric naming.
			if (sh.present && !sh.relPath.empty())
			{
				emitStr(s, "relPath", sh.relPath, "");
				s += ", ";
			}
			s += "\"width\": "; s += std::to_string(sh.image.getSize().x); s += ", ";
			s += "\"height\": "; s += std::to_string(sh.image.getSize().y); s += ", ";
			if (sh.mask) { emitColor(s, "transparentColor", *sh.mask, ""); }
			else         { emitNullColor(s, "transparentColor", ""); }
			s += ", ";
			emitStr(s, "maskDiscovery", maskHowToString(sh.maskHow), "");
			s += " }";
			if (!last) s += ",";
			s += "\n";
		};
		emitSheet("toolbox", p.toolbox, false);
		emitSheet("ingame", p.ingame, false);
		emitSheet("occupants", p.occupants, false);
		emitSheet("walking", p.walking, true);
		s += "  },\n";

		// spriteLayout
		s += "  \"spriteLayout\": {\n";
		emitStr(s, "toolbox", "meta/objmap_100.txt", "    "); s += ",\n";
		emitStr(s, "ingame", "meta/objmap_1000.txt", "    "); s += ",\n";
		emitStr(s, "walking", "meta/objmap_200.txt", "    "); s += "\n";
		s += "  }";

		// v3 resource store index (one line per resource, container order).
		std::string resourceError;
		if (!p.resources.empty())
		{
			s += ",\n";
			emitStr(s, "container", p.container == ResourceContainer::SP ? "SP" : "PE", "  "); s += ",\n";
			if (!p.peImage.empty()) { emitStr(s, "peImage", "pe/original.t2p", "  "); s += ",\n"; }
			s += "  \"resources\": [\n";
			for (size_t i = 0; i < p.resources.size(); ++i)
			{
				Json entry;
				const OspResult wr = writeOspResource(dir, p, p.resources[i], entry);
				if (!wr.ok) return wr;
				s += "    " + entry.dump(-1);
				s += i + 1 < p.resources.size() ? ",\n" : "\n";
			}
			s += "  ]";
			if (!p.peImage.empty())
			{
				if (!makeDirs(join(dir, "pe")) || !writeTextFile(join(dir, "pe/original.t2p"), p.peImage))
					return {false, "Failed to write pe/original.t2p"};
			}
		}
		s += "\n}\n";
		if (!writeTextFile(join(dir, "plugin.json"), s))
			return {false, "Failed to write plugin.json"};

		// --- sprites ---
		auto writeSheet = [&](const OspSpriteSheet & sh) -> bool {
			if (!sh.present) return true;
			if (sh.image.getSize().x == 0) return true;  // nothing to write
			return sh.image.saveToFile(join(dir, sh.relPath));
		};
		if (!writeSheet(p.toolbox))   return {false, "Failed to write sprites/toolbox.png"};
		if (!writeSheet(p.ingame))    return {false, "Failed to write sprites/ingame.png"};
		if (!writeSheet(p.occupants)) return {false, "Failed to write sprites/occupants.png"};
		if (!writeSheet(p.walking))   return {false, "Failed to write sprites/walking.png"};

		// --- objmap layouts ---
		if (!writeTextFile(join(dir, "meta/objmap_100.txt"), writePluginObjmap(p.toolbox.blocks)))
			return {false, "Failed to write meta/objmap_100.txt"};
		if (!writeTextFile(join(dir, "meta/objmap_1000.txt"), writePluginObjmap(p.ingame.blocks)))
			return {false, "Failed to write meta/objmap_1000.txt"};
		if (!writeTextFile(join(dir, "meta/objmap_200.txt"), writePluginObjmap(p.walking.blocks)))
			return {false, "Failed to write meta/objmap_200.txt"};

		return {true, ""};
	}

	// -------------------------------------------------------------------------
	// convertT2pToOsp
	// -------------------------------------------------------------------------
	namespace
	{
		/// Known per-plugin / per-resource transparent colors. Only the colors
		/// the game itself uses are hard-coded today (resource-formats.md §open
		/// questions). Unknown => corner discovery.
		struct KnownMask { const char * plugin; int resourceId; uint8_t r, g, b; };
		const KnownMask kKnownMasks[] = {
			{"Condo.t2p", 1000, 105, 33, 104},   // magenta, YootCondo empty room
			{"Condo.t2p", 1100, 255, 255, 255},  // white, resident overlay
		};
		std::optional<sf::Color> knownMask(const std::string & pluginName, int rid)
		{
			for (const auto & m : kKnownMasks)
				if (pluginName == m.plugin && rid == m.resourceId)
					return sf::Color(m.r, m.g, m.b);
			return std::nullopt;
		}

		/// Builds one sprite sheet from an RT_BITMAP DIB + matching OBJMAP text.
		void convertSheetBytes(const std::string * dib, const std::string * objmap,
		                       const std::string & pluginName, int bitmapId,
		                       const std::string & relPath, OspSpriteSheet & sheet)
		{
			sheet = OspSpriteSheet{};
			if (!dib || !PluginBitmapLoader::decodeBitmapResource(dib->data(), int(dib->size()), sheet.image))
			{
				sheet.present = false;
				return;
			}
			sheet.present = true;
			sheet.relPath = relPath;

			// Resolve the mask: known table first, else corner discovery.
			std::optional<sf::Color> km = knownMask(pluginName, bitmapId);
			if (km)
			{
				sheet.mask = km;
				sheet.maskHow = MaskDiscovery::Manual;
				sheet.image.createMaskFromColor(*km);
			}
			else
			{
				// Corner discovery: sample top-left pixel as the color key.
				const sf::Vector2u sz = sheet.image.getSize();
				if (sz.x > 0 && sz.y > 0)
				{
					sf::Color corner = sheet.image.getPixel({0, 0});
					// Only treat as a mask if the sheet looks keyed (corner is a
					// saturated primary/magenta/green). Heuristic, recorded as
					// "corner" so a reload can re-evaluate.
					if (corner != sf::Color::Black)
					{
						sheet.mask = corner;
						sheet.maskHow = MaskDiscovery::Corner;
						sheet.image.createMaskFromColor(corner);
					}
				}
			}

			// Pull the matching OBJMAP layout if present.
			if (objmap) parsePluginObjmap(objmap->data(), int(objmap->size()), sheet.blocks);
		}

		void convertSheet(Plugin & plugin, int bitmapId, int objmapId,
		                  const std::string & pluginName, const std::string & relPath,
		                  OspSpriteSheet & sheet)
		{
			std::string dib, om;
			const WindowsPEExecutable::Resource * bmp = plugin.getResource("2", bitmapId);
			if (bmp) dib.assign(bmp->data, size_t(bmp->length));
			const WindowsPEExecutable::Resource * omr = objmapId >= 0 ? plugin.getResource("OBJMAP", objmapId) : nullptr;
			if (omr) om.assign(omr->data, size_t(omr->length));
			convertSheetBytes(bmp ? &dib : nullptr, omr ? &om : nullptr, pluginName, bitmapId, relPath, sheet);
		}
	}

	OspResult convertT2pToOsp(Plugin & plugin, OspPlugin & out)
	{
		if (!plugin.isLoaded() && !plugin.ensureLoaded())
			return {false, "Failed to load plugin PE: " + plugin.name()};

		OspPlugin p;
		p.sourceFormat = "yoottower-t2p";
		p.sourceFile = plugin.name();

		PluginDesc desc;
		if (plugin.getDesc(desc)) p.pluginClass = desc.kind;
		plugin.getAttr(p.attr);
		DayParamDef day;
		if (plugin.getDayParamDef(day, 128)) p.dayParam = day;
		PlaceParamDef place;
		if (plugin.getPlaceParamDef(place, 128)) p.placeParam = place;
		// Mover plugins: surface the first MVDF variation's strings.
		TemplateDef def;
		if (plugin.getTemplateDef(def) && def.kind == TemplateDef::Mover && !def.tool.variations.empty())
		{
			const ToolVariation & v = def.tool.variations.front();
			p.transport.valid = true;
			p.transport.priceString = v.priceString;
			p.transport.outMoneyString = v.outMoneyString;
			p.transport.name = v.name;
		}

		// Fresh plugins follow the shipped numeric RT_BITMAP convention
		// (same default the editor's Import uses; the game reads these).
		convertSheet(plugin, 100,  100,  plugin.name(), "sprites/100.png",   p.toolbox);
		convertSheet(plugin, 1000, 1000, plugin.name(), "sprites/1000.png",  p.ingame);
		convertSheet(plugin, 1100, -1,   plugin.name(), "sprites/1100.png",  p.occupants);
		convertSheet(plugin, 200,  200,  plugin.name(), "sprites/200.png",   p.walking);

		// v3: every resource, losslessly, plus the DLL image for export.
		std::string fileBytes;
		if (!readTextFile(plugin.path().str(), fileBytes))
			return {false, "Failed to read " + plugin.path().str()};
		p.container = plugin.container();
		if (p.container == ResourceContainer::SP)
		{
			SpContainer c;
			if (!decodeSpContainer(fileBytes.data(), fileBytes.size(), c))
				return {false, "Bad SP container: " + plugin.name()};
			for (const SpContainer::Entry & e : c.entries)
			{
				OspResource r;
				r.spType = e.type;
				r.res = decodeResource(e.typeName(), int(e.id), e.data.data(), e.data.size(), ResourceContainer::SP);
				p.resources.push_back(std::move(r));
			}
		}
		else
		{
			p.peImage = std::move(fileBytes);
			for (const AbstractPlugin::ResourceKey & k : plugin.enumerateResources())
			{
				const WindowsPEExecutable::Resource * res = plugin.getResource(k.type, k.id);
				if (!res) continue;
				OspResource r;
				r.res = decodeResource(k.type, k.id, res->data, size_t(res->length), ResourceContainer::PE);
				p.resources.push_back(std::move(r));
			}
		}

		out = std::move(p);
		return {true, ""};
	}
}

// ---------------------------------------------------------------------------
// OSP v3: the lossless resource store
// ---------------------------------------------------------------------------
namespace OT
{
	namespace
	{
		uint32_t le32(const std::string & s, size_t p)
		{
			return uint32_t(uint8_t(s[p])) | (uint32_t(uint8_t(s[p + 1])) << 8)
			     | (uint32_t(uint8_t(s[p + 2])) << 16) | (uint32_t(uint8_t(s[p + 3])) << 24);
		}
		uint16_t le16(const std::string & s, size_t p) { return uint16_t(uint8_t(s[p]) | (uint8_t(s[p + 1]) << 8)); }
		void put32(std::string & s, size_t p, uint32_t v)
		{
			for (int i = 0; i < 4; ++i) s[p + size_t(i)] = char((v >> (8 * i)) & 0xFF);
		}

		/// The DIB shapes we turn into PNG: BITMAPINFOHEADER, 1 plane,
		/// uncompressed, 4 or 8 bpp, palette right after the header.
		struct DibLayout
		{
			int32_t width = 0, height = 0;   // height < 0 = top-down
			uint16_t bpp = 0;
			uint32_t colors = 0;
			size_t pixels = 0;               // offset of the pixel rows
			size_t stride = 0;
			uint32_t rows() const { return uint32_t(height < 0 ? -height : height); }
		};

		bool parseDib(const std::string & d, DibLayout & L)
		{
			if (d.size() < 40 || le32(d, 0) != 40) return false;
			L.width = int32_t(le32(d, 4));
			L.height = int32_t(le32(d, 8));
			if (le16(d, 12) != 1 || le32(d, 16) != 0) return false;
			L.bpp = le16(d, 14);
			if (L.bpp != 4 && L.bpp != 8) return false;
			const uint32_t used = le32(d, 32);
			L.colors = used ? used : (1u << L.bpp);
			if (L.colors > (1u << L.bpp) || L.width <= 0 || L.height == 0 || L.width > 65535 || L.rows() > 65535)
				return false;
			L.pixels = 40 + 4 * size_t(L.colors);
			L.stride = ((size_t(L.width) * L.bpp + 31) / 32) * 4;
			return d.size() >= L.pixels + L.stride * L.rows();
		}

		uint8_t dibIndex(const std::string & d, const DibLayout & L, size_t rowStart, uint32_t x)
		{
			if (L.bpp == 8) return uint8_t(d[rowStart + x]);
			const uint8_t b = uint8_t(d[rowStart + x / 2]);
			return (x & 1) ? (b & 15) : (b >> 4);
		}

		size_t rowUsedBytes(const DibLayout & L) { return L.bpp == 8 ? size_t(L.width) : (size_t(L.width) + 1) / 2; }

		std::string sanitize(const std::string & s)
		{
			std::string o;
			for (char c : s)
				o.push_back((std::isalnum(uint8_t(c)) || c == '_' || c == '-') ? c : '_');
			return o.empty() ? std::string("_") : o;
		}

		bool isBitmap(const OspPlugin & p, const OspResource & r)
		{
			if (r.res.codec != ResourceCodec::Raw) return false;
			return p.container == ResourceContainer::SP ? r.spType == 0 : r.res.type == "2";
		}

		bool isWave(const OspResource & r)
		{
			return r.res.codec == ResourceCodec::Raw && r.res.type == "WAVE" && r.res.bytes.compare(0, 4, "RIFF") == 0;
		}

		bool codecFromName(const std::string & name, ResourceCodec & out)
		{
			for (int i = 0; i <= int(ResourceCodec::Palette); ++i)
				if (name == resourceCodecName(ResourceCodec(i))) { out = ResourceCodec(i); return true; }
			return false;
		}

		const std::string * resourceBytes(const OspPlugin & p, const std::string & type, int id)
		{
			const OspResource * r = findOspResource(p, type, id);
			if (!r || (r->res.codec != ResourceCodec::Raw && r->res.codec != ResourceCodec::Text)) return nullptr;
			return &r->res.bytes;
		}
	}

	OspResource * findOspResource(OspPlugin & p, const std::string & type, int id)
	{
		for (OspResource & r : p.resources)
			if (r.res.id == id && r.res.type == type) return &r;
		return nullptr;
	}

	const OspResource * findOspResource(const OspPlugin & p, const std::string & type, int id)
	{
		for (const OspResource & r : p.resources)
			if (r.res.id == id && r.res.type == type) return &r;
		return nullptr;
	}

	// ---- DIB <-> PNG ----
	bool dibToPng(const std::string & d, sf::Image & image, Json & side)
	{
		DibLayout L;
		if (!parseDib(d, L)) return false;
		std::vector<sf::Color> pal(L.colors);
		Json palJson = Json::array();
		for (uint32_t k = 0; k < L.colors; ++k)
		{
			const size_t o = 40 + 4 * size_t(k);
			pal[k] = sf::Color(uint8_t(d[o + 2]), uint8_t(d[o + 1]), uint8_t(d[o]));
			Json e = Json::array();
			e.push(Json(int(uint8_t(d[o + 2])))); e.push(Json(int(uint8_t(d[o + 1]))));
			e.push(Json(int(uint8_t(d[o])))); e.push(Json(int(uint8_t(d[o + 3]))));
			palJson.push(std::move(e));
		}
		image.resize({uint32_t(L.width), L.rows()}, sf::Color::Black);
		Json padding = Json::object();
		const size_t used = rowUsedBytes(L);
		for (uint32_t r = 0; r < L.rows(); ++r)
		{
			const size_t rowStart = L.pixels + L.stride * r;
			const uint32_t y = L.height > 0 ? L.rows() - 1 - r : r;
			for (uint32_t x = 0; x < uint32_t(L.width); ++x)
			{
				const uint8_t idx = dibIndex(d, L, rowStart, x);
				image.setPixel({x, y}, idx < L.colors ? pal[idx] : sf::Color::Black);
			}
			// 4 bpp odd widths leave a low nibble unused in the last byte;
			// keep it with the padding bytes.
			std::string pad = d.substr(rowStart + used, L.stride - used);
			const bool oddNibble = L.bpp == 4 && (L.width & 1) && (uint8_t(d[rowStart + used - 1]) & 15);
			if (oddNibble || pad.find_first_not_of('\0') != std::string::npos)
			{
				Json pe = Json(toHex(pad));
				if (oddNibble) pe = Json(toHex(std::string(1, char(uint8_t(d[rowStart + used - 1]) & 15)) + pad));
				padding.set(std::to_string(r), pe);
			}
		}
		// The palette must map colours back to the same indices: every
		// pixel's index is the first palette slot with its colour. Refuse
		// (keep raw) otherwise.
		{
			std::map<uint32_t, uint8_t> first;
			for (uint32_t k = 0; k < L.colors; ++k)
				first.emplace((uint32_t(pal[k].r) << 16) | (uint32_t(pal[k].g) << 8) | pal[k].b, uint8_t(k));
			for (uint32_t r = 0; r < L.rows(); ++r)
				for (uint32_t x = 0; x < uint32_t(L.width); ++x)
				{
					const uint8_t idx = dibIndex(d, L, L.pixels + L.stride * r, x);
					if (idx >= L.colors) return false;
					const uint32_t key = (uint32_t(pal[idx].r) << 16) | (uint32_t(pal[idx].g) << 8) | pal[idx].b;
					if (first[key] != idx) return false;
				}
		}
		side = Json::object();
		side.set("header", Json(toHex(d.substr(0, 40))));
		side.set("width", Json(L.width)).set("height", Json(L.height)).set("bitsPerPixel", Json(int(L.bpp)));
		side.set("palette", std::move(palJson));
		if (padding.size()) side.set("rowPadding", std::move(padding));
		const size_t end = L.pixels + L.stride * L.rows();
		if (end < d.size()) side.set("tailHex", Json(toHex(d.substr(end))));
		return true;
	}

	bool pngToDib(const sf::Image & image, const Json & side, std::string & out, std::string * error)
	{
		auto fail = [&](const std::string & m) { if (error) *error = m; return false; };
		std::string header;
		if (!side["header"].isString() || !fromHex(side["header"].asString(), header) || header.size() != 40)
			return fail("sidecar: bad header");
		DibLayout L;
		{
			std::string probe = header;
			const Json & pal = side["palette"];
			if (!pal.isArray() || pal.size() == 0 || pal.size() > 256) return fail("sidecar: bad palette");
			for (const Json & e : pal.elements())
			{
				if (!e.isArray() || e.size() != 4) return fail("sidecar: bad palette entry");
				probe.push_back(char(e.at(2).asInt())); probe.push_back(char(e.at(1).asInt()));
				probe.push_back(char(e.at(0).asInt())); probe.push_back(char(e.at(3).asInt()));
			}
			header = probe;  // header + palette
		}
		const sf::Vector2u sz = image.getSize();
		if (sz.x == 0 || sz.y == 0) return fail("empty image");
		int32_t w = int32_t(le32(header, 4)), h = int32_t(le32(header, 8));
		const bool resized = uint32_t(w) != sz.x || uint32_t(h < 0 ? -h : h) != sz.y;
		if (resized)
		{
			w = int32_t(sz.x);
			h = h < 0 ? -int32_t(sz.y) : int32_t(sz.y);
			put32(header, 4, uint32_t(w));
			put32(header, 8, uint32_t(h));
		}
		// Parse with a dummy pixel area to get the geometry.
		{
			const uint16_t bpp = le16(header, 14);
			const size_t stride = ((size_t(w) * bpp + 31) / 32) * 4;
			std::string probe = header + std::string(stride * sz.y, '\0');
			if (!parseDib(probe, L)) return fail("sidecar: unsupported DIB header");
			if (L.pixels != header.size()) return fail("sidecar: palette size does not match the header");
		}
		if (resized && le32(header, 20) != 0) put32(header, 20, uint32_t(L.stride * L.rows()));

		std::vector<sf::Color> pal(L.colors);
		std::map<uint32_t, uint8_t> exact;
		for (uint32_t k = 0; k < L.colors; ++k)
		{
			const size_t o = 40 + 4 * size_t(k);
			pal[k] = sf::Color(uint8_t(header[o + 2]), uint8_t(header[o + 1]), uint8_t(header[o]));
			exact.emplace((uint32_t(pal[k].r) << 16) | (uint32_t(pal[k].g) << 8) | pal[k].b, uint8_t(k));
		}
		auto indexOf = [&](const sf::Color & c) -> uint8_t {
			const uint32_t key = (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b;
			auto it = exact.find(key);
			if (it != exact.end()) return it->second;
			int best = 0;
			long bestD = -1;
			for (uint32_t k = 0; k < L.colors; ++k)
			{
				const long dr = long(c.r) - pal[k].r, dg = long(c.g) - pal[k].g, db = long(c.b) - pal[k].b;
				const long dd = dr * dr + dg * dg + db * db;
				if (bestD < 0 || dd < bestD) { bestD = dd; best = int(k); }
			}
			exact.emplace(key, uint8_t(best));
			return uint8_t(best);
		};

		std::string d = header;
		d.resize(L.pixels + L.stride * L.rows(), '\0');
		const size_t used = rowUsedBytes(L);
		const Json & padding = side["rowPadding"];
		for (uint32_t r = 0; r < L.rows(); ++r)
		{
			const size_t rowStart = L.pixels + L.stride * r;
			const uint32_t y = L.height > 0 ? L.rows() - 1 - r : r;
			for (uint32_t x = 0; x < uint32_t(L.width); ++x)
			{
				const uint8_t idx = indexOf(image.getPixel({x, y}));
				if (L.bpp == 8) d[rowStart + x] = char(idx);
				else if (x & 1) d[rowStart + x / 2] = char(uint8_t(d[rowStart + x / 2]) | (idx & 15));
				else d[rowStart + x / 2] = char((idx & 15) << 4);
			}
			if (!resized && padding.isObject())
			{
				const Json & pj = padding[std::to_string(r)];
				std::string pad;
				if (pj.isString() && fromHex(pj.asString(), pad))
				{
					const bool oddNibble = L.bpp == 4 && (L.width & 1) && pad.size() == L.stride - used + 1;
					if (oddNibble)
					{
						d[rowStart + used - 1] = char(uint8_t(d[rowStart + used - 1]) | (uint8_t(pad[0]) & 15));
						pad.erase(0, 1);
					}
					if (pad.size() == L.stride - used) d.replace(rowStart + used, pad.size(), pad);
				}
			}
		}
		if (!resized && side["tailHex"].isString())
		{
			std::string tail;
			if (!fromHex(side["tailHex"].asString(), tail)) return fail("sidecar: bad tailHex");
			d += tail;
		}
		out = std::move(d);
		return true;
	}

	bool replaceDibPixels(const std::string & dib, const sf::Image & image, std::string & out, std::string * error)
	{
		sf::Image old;
		Json side;
		if (!dibToPng(dib, old, side))
		{
			if (error) *error = "the bitmap is not a 4/8-bit palette DIB";
			return false;
		}
		return pngToDib(image, side, out, error);
	}

	// ---- v2 view ----
	void syncOspLegacyFromResources(OspPlugin & p)
	{
		if (p.resources.empty()) return;
		for (const OspResource & r : p.resources)
		{
			if (r.res.codec == ResourceCodec::Desc && r.res.id == 128)
				p.pluginClass = std::get<PluginDesc>(r.res.value).kind;
			else if (r.res.codec == ResourceCodec::Attr && r.res.id == 128)
				p.attr = std::get<PluginAttr>(r.res.value);
		}
		p.dayParam.reset();
		p.placeParam.reset();
		if (const OspResource * r = findOspResource(p, "DPDF", 128))
			if (r->res.codec == ResourceCodec::DayParam) p.dayParam = std::get<DayParamDef>(r->res.value);
		if (const OspResource * r = findOspResource(p, "PLDF", 128))
			if (r->res.codec == ResourceCodec::PlaceParam) p.placeParam = std::get<PlaceParamDef>(r->res.value);
		p.transport = OspTransport{};
		if (const OspResource * r = findOspResource(p, "MVDF", 1))
		{
			if (r->res.codec == ResourceCodec::TemplateDef)
			{
				const TemplateDef & def = std::get<TemplateDef>(r->res.value);
				if (!def.tool.variations.empty())
				{
					p.transport.valid = true;
					p.transport.priceString = def.tool.variations.front().priceString;
					p.transport.outMoneyString = def.tool.variations.front().outMoneyString;
					p.transport.name = def.tool.variations.front().name;
				}
			}
		}
		if (p.container == ResourceContainer::PE)
		{
			const std::string name = p.sourceFile;
			convertSheetBytes(resourceBytes(p, "2", 100), resourceBytes(p, "OBJMAP", 100), name, 100, "sprites/100.png", p.toolbox);
			convertSheetBytes(resourceBytes(p, "2", 1000), resourceBytes(p, "OBJMAP", 1000), name, 1000, "sprites/1000.png", p.ingame);
			convertSheetBytes(resourceBytes(p, "2", 1100), nullptr, name, 1100, "sprites/1100.png", p.occupants);
			convertSheetBytes(resourceBytes(p, "2", 200), resourceBytes(p, "OBJMAP", 200), name, 200, "sprites/200.png", p.walking);
		}
	}

	void synthesizeOspResources(OspPlugin & p)
	{
		if (!p.resources.empty()) return;
		auto add = [&](const char * type, int id, ResourceCodec codec, auto value) {
			OspResource r;
			r.res.type = type;
			r.res.id = id;
			r.res.codec = codec;
			r.res.value = std::move(value);
			p.resources.push_back(std::move(r));
		};
		PluginAttr a = p.attr;
		a.valid = true;
		if (a.isTextForm && a.text.empty()) a.isTextForm = false;
		add("ATTR", 128, ResourceCodec::Attr, a);
		PluginDesc d;
		d.valid = true;
		d.kind = p.pluginClass;
		d.classTag = pluginTagFromKind(p.pluginClass);
		d.value = 3;
		add("DESC", 128, ResourceCodec::Desc, d);
		if (p.sourceFormat == "scratch" && p.pluginClass == PluginDesc::TenantPlugin)
		{
			TemplateDef t;
			t.kind = TemplateDef::Tenant;
			t.tool.toolType = p.attr.itemType;
			t.tool.level = p.attr.gameLevel;
			t.tool.category = 80;
			t.tool.categoryName = "Shops";
			ToolVariation v;
			v.name = p.attr.name;
			v.helpTitle = p.attr.name;
			v.price = 100;
			v.priceString = "$10000";
			t.tool.variations.push_back(v);
			t.equip.variations.push_back(EquipVariation{});
			t.equip.skip8 = 1;
			add("TNDF", 1, ResourceCodec::TemplateDef, t);
		}
		if (p.dayParam) add("DPDF", 128, ResourceCodec::DayParam, *p.dayParam);
		if (p.placeParam) add("PLDF", 128, ResourceCodec::PlaceParam, *p.placeParam);
	}

	// ---- store ----
	static OspResult writeOspResource(const std::string & dir, const OspPlugin & p, const OspResource & r, Json & entry)
	{
		const DecodedResource & res = r.res;
		entry = Json::object();
		entry.set("type", Json(latin1ToUtf8(res.type))).set("id", Json(res.id));
		if (p.container == ResourceContainer::SP) entry.set("spType", Json(r.spType));
		entry.set("codec", Json(resourceCodecName(res.codec)));
		const std::string idStr = std::to_string(res.id);
		auto writeFile = [&](const std::string & sub, const std::string & name, const std::string & bytes) -> OspResult {
			if (!makeDirs(join(dir, sub))) return {false, "Cannot create " + sub};
			const std::string rel = sub + "/" + name;
			if (!writeTextFile(join(dir, rel), bytes)) return {false, "Failed to write " + rel};
			entry.set("file", Json(rel));
			return {true, ""};
		};
		if (isBitmap(p, r))
		{
			sf::Image img;
			Json side;
			if (dibToPng(res.bytes, img, side))
			{
				if (!makeDirs(join(dir, "bitmaps"))) return {false, "Cannot create bitmaps/"};
				const std::string png = "bitmaps/" + idStr + ".png";
				const std::string sc = "bitmaps/" + idStr + ".dib.json";
				if (!img.saveToFile(join(dir, png))) return {false, "Failed to write " + png};
				if (!writeTextFile(join(dir, sc), side.dump())) return {false, "Failed to write " + sc};
				entry.set("file", Json(png)).set("dib", Json(sc));
				return {true, ""};
			}
		}
		const std::string typeDir = sanitize(resourceTypeDisplayName(res.type));
		if (isWave(r)) return writeFile("sounds", idStr + ".wav", res.bytes);
		if (res.codec == ResourceCodec::Raw) return writeFile("resources/" + typeDir, idStr + ".bin", res.bytes);
		if (res.codec == ResourceCodec::Text) return writeFile("resources/" + typeDir, idStr + ".txt", res.bytes);
		return writeFile("resources/" + typeDir, idStr + ".json", resourceToJson(res).dump());
	}

	static OspResult loadOspResources(const std::string & dir, const std::string & manifest, OspPlugin & p)
	{
		Json m;
		std::string err;
		if (!Json::parse(manifest, m, &err)) return {false, "plugin.json: " + err};
		const Json & list = m["resources"];
		if (list.isNull()) return {true, ""};
		if (!list.isArray()) return {false, "plugin.json: \"resources\" is not a list"};
		p.container = m["container"].asString() == "SP" ? ResourceContainer::SP : ResourceContainer::PE;
		p.resources.clear();
		for (size_t i = 0; i < list.size(); ++i)
		{
			const Json & e = list.at(i);
			const std::string where = "resources[" + std::to_string(i) + "]";
			OspResource r;
			if (!utf8ToLatin1(e["type"].asString(), r.res.type) || r.res.type.empty() || !e["id"].isInt())
				return {false, where + ": missing type/id"};
			r.res.id = int(e["id"].asInt());
			r.spType = uint32_t(e["spType"].asInt());
			if (!codecFromName(e["codec"].asString(), r.res.codec)) return {false, where + ": unknown codec"};
			const std::string file = e["file"].asString();
			if (file.empty()) return {false, where + ": missing file"};
			const std::string label = resourceTypeDisplayName(r.res.type) + " " + std::to_string(r.res.id);
			if (e.has("dib"))
			{
				sf::Image img;
				std::string sideText;
				Json side;
				if (!img.loadFromFile(join(dir, file))) return {false, "Failed to load " + file};
				if (!readTextFile(join(dir, e["dib"].asString()), sideText) || !Json::parse(sideText, side, &err))
					return {false, "Failed to read " + e["dib"].asString()};
				if (!pngToDib(img, side, r.res.bytes, &err)) return {false, label + ": " + err};
			}
			else
			{
				std::string bytes;
				if (!readTextFile(join(dir, file), bytes)) return {false, "Failed to read " + file};
				if (r.res.codec == ResourceCodec::Raw || r.res.codec == ResourceCodec::Text) r.res.bytes = std::move(bytes);
				else
				{
					Json j;
					if (!Json::parse(bytes, j, &err)) return {false, file + ": " + err};
					if (!resourceFromJson(j, r.res, &err)) return {false, file + ": " + err};
				}
			}
			p.resources.push_back(std::move(r));
		}
		const std::string pe = m["peImage"].asString();
		if (!pe.empty() && !readTextFile(join(dir, pe), p.peImage)) return {false, "Failed to read " + pe};
		return {true, ""};
	}
}
