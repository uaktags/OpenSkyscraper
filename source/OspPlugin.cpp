/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "OspPlugin.h"
#include "PluginBitmapLoader.h"
#include "PluginManager.h"
#include "WindowsPEExecutable.h"

#include <SFML/Graphics/Color.hpp>

#include <cstdio>
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

		out = std::move(p);
		return {true, ""};
	}

	// -------------------------------------------------------------------------
	// saveOspPlugin
	// -------------------------------------------------------------------------
	OspResult saveOspPlugin(const std::string & dir, const OspPlugin & p, bool overwrite)
	{
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
		s += "  }\n";

		s += "}\n";
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

		/// Builds one sprite sheet from a t2p RT_BITMAP + matching OBJMAP.
		void convertSheet(Plugin & plugin, int bitmapId, int objmapId,
		                  const std::string & pluginName, const std::string & relPath,
		                  OspSpriteSheet & sheet)
		{
			const WindowsPEExecutable::Resource * bmp = plugin.getResource("2", bitmapId);
			if (!bmp || !PluginBitmapLoader::decodeBitmapResource(bmp->data, bmp->length, sheet.image))
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
			if (objmapId >= 0)
			{
				if (const WindowsPEExecutable::Resource * om = plugin.getResource("OBJMAP", objmapId))
					parsePluginObjmap(om->data, om->length, sheet.blocks);
			}
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

		out = std::move(p);
		return {true, ""};
	}
}
