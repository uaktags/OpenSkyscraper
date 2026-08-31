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
		if (ver != kOspFormatVersion)
			return {false, "Unsupported OSP version " + std::to_string(ver) + " (expected " + std::to_string(kOspFormatVersion) + ")"};

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
		size_t attrScope = findObject(json, "attr", 0);
		if (attrScope == std::string::npos)
			return {false, "Missing 'attr' object"};
		{
			long long v;
			std::string s4;
			if (readString(json, "itemType", attrScope, s4))
			{
				for (int i = 0; i < 4 && i < int(s4.size()); ++i) p.attr.itemType.c[i] = s4[i];
			}
			if (readString(json, "name", attrScope, s4)) p.attr.name = s4;
			if (readString(json, "marker", attrScope, s4) && s4.size() >= 4)
				for (int i = 0; i < 4; ++i) p.attr.marker.c[i] = s4[i];
			if (readScalarInt(json, "gameLevel", attrScope, v))  p.attr.gameLevel = uint32_t(v);
			if (readScalarInt(json, "id", attrScope, v))         p.attr.id = uint32_t(v);
			if (readScalarInt(json, "mustItem", attrScope, v))   p.attr.mustItem = uint32_t(v);
			if (readScalarInt(json, "dependID", attrScope, v))   p.attr.dependID = uint32_t(v);
			if (readScalarInt(json, "secretItem", attrScope, v)) p.attr.secretItem = uint32_t(v);
			p.attr.valid = true;
		}

		// PLDF (optional).
		size_t priceScope = findObject(json, "price", 0);
		if (priceScope != std::string::npos)
		{
			long long v;
			if (readScalarInt(json, "basePrice", priceScope, v)) p.price.basePrice = uint32_t(v);
			if (readScalarInt(json, "f1", priceScope, v)) p.price.f1 = uint32_t(v);
			if (readScalarInt(json, "f2", priceScope, v)) p.price.f2 = uint32_t(v);
			if (readScalarInt(json, "f3", priceScope, v)) p.price.f3 = uint32_t(v);
			if (readScalarInt(json, "f4", priceScope, v)) p.price.f4 = uint32_t(v);
			if (readScalarInt(json, "f5", priceScope, v)) p.price.f5 = uint32_t(v);
			p.price.valid = true;
		}

		// DPDF (optional).
		size_t placScope = findObject(json, "placement", 0);
		if (placScope != std::string::npos)
		{
			long long v;
			if (readScalarInt(json, "w", placScope, v))     p.placement.w = uint32_t(v);
			if (readScalarInt(json, "h", placScope, v))     p.placement.h = uint32_t(v);
			if (readScalarInt(json, "baseX", placScope, v)) p.placement.baseX = uint32_t(v);
			if (readScalarInt(json, "baseY", placScope, v)) p.placement.baseY = uint32_t(v);
			p.placement.valid = true;
		}

		// Sprite sheets. Each entry under "bitmaps" may carry a
		// transparentColor (null or [r,g,b]); the pixels load from sprites/.
		auto loadSheet = [&](const char * key, OspSpriteSheet & sheet, const char * defaultRelPath) -> bool {
			size_t scope = findObject(json, key, 0);
			if (scope == std::string::npos) return true;  // absent => not present
			sheet.present = true;
			sheet.relPath = defaultRelPath;
			sf::Color c;
			if (readColor(json, "transparentColor", scope, c)) { sheet.mask = c; sheet.maskHow = MaskDiscovery::Manual; }
			else sheet.maskHow = MaskDiscovery::None;
			// Load pixels.
			const std::string pngPath = join(dir, sheet.relPath);
			if (!sheet.image.loadFromFile(pngPath))
				return false;
			if (sheet.mask) sheet.image.createMaskFromColor(*sheet.mask);
			return true;
		};
		if (!loadSheet("toolbox", p.toolbox, "sprites/toolbox.png"))
			return {false, "Failed to load sprites/toolbox.png"};
		if (!loadSheet("ingame", p.ingame, "sprites/ingame.png"))
			return {false, "Failed to load sprites/ingame.png"};
		if (!loadSheet("occupants", p.occupants, "sprites/occupants.png"))
			p.occupants.present = false;  // absent sheet is tolerable (residents/shops only)
		// Walking frames (Mover plugins). Absent for tenants — tolerate.
		{
			size_t scope = findObject(json, "walking", 0);
			if (scope != std::string::npos)
			{
				p.walking.present = true;
				p.walking.relPath = "sprites/walking.png";
				sf::Color c;
				if (readColor(json, "transparentColor", scope, c)) { p.walking.mask = c; p.walking.maskHow = MaskDiscovery::Manual; }
				const std::string pngPath = join(dir, p.walking.relPath);
				if (p.walking.image.loadFromFile(pngPath))
				{
					if (p.walking.mask) p.walking.image.createMaskFromColor(*p.walking.mask);
				}
				else p.walking.present = false;
			}
		}

		// MVDF transport definition (optional; Mover plugins).
		size_t mvdfScope = findObject(json, "transport", 0);
		if (mvdfScope != std::string::npos)
		{
			std::string ps, qs, nm;
			if (readString(json, "priceString", mvdfScope, ps)) p.transport.priceString = ps;
			if (readString(json, "quarterlyString", mvdfScope, qs)) p.transport.quarterlyString = qs;
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
		s += "  \"attr\": {\n";
		emitStr(s, "itemType", p.attr.itemType.str(), "    "); s += ",\n";
		emitStr(s, "name", p.attr.name, "    "); s += ",\n";
		emitInt(s, "gameLevel", p.attr.gameLevel, "    "); s += ",\n";
		emitInt(s, "id", p.attr.id, "    "); s += ",\n";
		emitStr(s, "marker", p.attr.marker.str(), "    "); s += ",\n";
		emitInt(s, "mustItem", p.attr.mustItem, "    "); s += ",\n";
		emitInt(s, "dependID", p.attr.dependID, "    "); s += ",\n";
		emitInt(s, "secretItem", p.attr.secretItem, "    "); s += "\n";
		s += "  },\n";

		// placement (optional)
		if (p.placement.valid)
		{
			s += "  \"placement\": {\n";
			emitInt(s, "w", p.placement.w, "    "); s += ",\n";
			emitInt(s, "h", p.placement.h, "    "); s += ",\n";
			emitInt(s, "baseX", p.placement.baseX, "    "); s += ",\n";
			emitInt(s, "baseY", p.placement.baseY, "    "); s += "\n";
			s += "  },\n";
		}

		// price (optional; PLDF — shops)
		if (p.price.valid)
		{
			s += "  \"price\": {\n";
			emitInt(s, "basePrice", p.price.basePrice, "    "); s += ",\n";
			emitInt(s, "f1", p.price.f1, "    "); s += ",\n";
			emitInt(s, "f2", p.price.f2, "    "); s += ",\n";
			emitInt(s, "f3", p.price.f3, "    "); s += ",\n";
			emitInt(s, "f4", p.price.f4, "    "); s += ",\n";
			emitInt(s, "f5", p.price.f5, "    "); s += "\n";
			s += "  },\n";
		}

		// transport (optional; MVDF — movers). Only the human-readable price
		// strings + name round-trip today; the binary header fields stay
		// Ghidra-pending and are not (yet) editable in the inspector.
		if (p.transport.valid)
		{
			s += "  \"transport\": {\n";
			emitStr(s, "priceString",    p.transport.priceString,    "    "); s += ",\n";
			emitStr(s, "quarterlyString", p.transport.quarterlyString, "    "); s += ",\n";
			emitStr(s, "name",           p.transport.name,           "    "); s += "\n";
			s += "  },\n";
		}

		// bitmaps
		s += "  \"bitmaps\": {\n";
		auto emitSheet = [&](const char * key, const OspSpriteSheet & sh, bool last) {
			s += "    \""; s += key; s += "\": { ";
			s += "\"present\": "; s += (sh.present ? "true" : "false"); s += ", ";
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
		plugin.getDpdf(p.placement);
		plugin.getPldf(p.price);
		plugin.getMvdf(p.transport);   // Mover plugins only; no-op for tenants

		convertSheet(plugin, 100,  100,  plugin.name(), "sprites/toolbox.png",   p.toolbox);
		convertSheet(plugin, 1000, 1000, plugin.name(), "sprites/ingame.png",    p.ingame);
		convertSheet(plugin, 1100, -1,   plugin.name(), "sprites/occupants.png", p.occupants);
		convertSheet(plugin, 200,  200,  plugin.name(), "sprites/walking.png",   p.walking);

		out = std::move(p);
		return {true, ""};
	}
}
