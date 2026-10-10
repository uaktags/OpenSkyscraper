/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "DecompiledPlugin.h"
#include "OspPlugin.h"
#include "PluginResources.h"

#include <SFML/Graphics/Color.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace
{
	// ---- filesystem helpers (POSIX; mirrors OspPlugin.cpp's style) ----
	bool readBinaryFile(const std::string & path, std::string & out)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f) return false;
		std::ostringstream ss;
		ss << f.rdbuf();
		out = ss.str();
		return true;
	}

	bool fileExists(const std::string & path)
	{
		struct stat st{};
		return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
	}

	std::string join(const std::string & a, const std::string & b)
	{
		if (a.empty()) return b;
		if (a.back() == '/' || a.back() == '\\') return a + b;
		return a + "/" + b;
	}

	// ---- minimal flat JSON reader (mirrors OspPlugin.cpp's) ----
	// Only what the DecompiledPlugin manifest needs: strings, ints, nested
	// objects, and the downscale {w,h} pair.
	size_t findKeyValue(const std::string & s, const std::string & key, size_t from)
	{
		std::string needle = "\"" + key + "\"";
		size_t k = s.find(needle, from);
		if (k == std::string::npos) return std::string::npos;
		k += needle.size();
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
		while (i < s.size() && s[i] >= '0' && s[i] <= '9') { ++i; any = true; }
		if (!any) { i = start; return false; }
		out = std::strtoll(s.c_str() + start, nullptr, 10);
		return true;
	}

	bool readScalarInt(const std::string & s, const std::string & key, size_t scope, long long & out)
	{
		size_t i = findKeyValue(s, key, scope);
		if (i == std::string::npos) return false;
		return readNumber(s, i, out);
	}

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

	size_t findObject(const std::string & s, const std::string & key, size_t from)
	{
		size_t i = findKeyValue(s, key, from);
		if (i == std::string::npos || i >= s.size() || s[i] != '{') return std::string::npos;
		return i;
	}
}

namespace OT
{
namespace
{
	/// Builds a synthetic ATTR binary blob from the manifest's "attr" object
	/// (v1 or v2 keys, see readOspAttr) so getAttr() decodes it exactly as
	/// if it came from a PE plugin.
	std::string buildAttrBlob(const std::string & manifest)
	{
		PluginAttr attr;
		if (!readOspAttr(manifest, attr)) return {};
		if (attr.name.size() > 255) attr.name.resize(255);
		std::string blob;
		if (!encodePluginAttr(attr, blob)) return {};
		return blob;
	}

	/// Builds a synthetic DESC binary blob (class discriminator). 9 bytes:
	/// version u8 + classTag 4cc + value u32.
	std::string buildDescBlob(const std::string & manifest)
	{
		std::string cls;
		if (!readString(manifest, "class", 0, cls)) return {};
		std::string tag = "lPnT"; // default Tenant
		uint32_t value = 3;
		if      (cls == "Tenant")    { tag = "lPnT"; }
		else if (cls == "Mover")     { tag = "lPvM"; }
		else if (cls == "ToolPalette"){ tag = "LPOT"; }
		else if (cls == "World")     { tag = "lPdW"; }
		else if (cls == "OuterObj")  { tag = "lPoO"; }
		std::string blob;
		blob.push_back(char(1));
		blob += tag;
		blob.push_back(char(value & 0xFF));
		blob.push_back(char((value >> 8) & 0xFF));
		blob.push_back(char((value >> 16) & 0xFF));
		blob.push_back(char((value >> 24) & 0xFF));
		return blob;
	}

	/// Inserts a resource: copies `bytes` into an owning Resource entry.
	void putResource(std::map<std::string, WindowsPEExecutable::Resources> & table,
	                 const std::string & type, int id, const std::string & bytes)
	{
		WindowsPEExecutable::Resource & r = table[type][id];
		r.type = type;
		r.id = id;
		r.length = static_cast<int>(bytes.size());
		delete[] r.data;
		r.data = new char[bytes.size()];
		std::memcpy(r.data, bytes.data(), bytes.size());
	}

	/// Loads <dir>/<rel> into a resource if the file exists. Returns true
	/// if a file was found and loaded.
	bool loadFileResource(std::map<std::string, WindowsPEExecutable::Resources> & table,
	                      const std::string & dir, const std::string & rel,
	                      const std::string & type, int id)
	{
		std::string path = join(dir, rel);
		std::string bytes;
		if (!readBinaryFile(path, bytes) || bytes.empty()) return false;
		putResource(table, type, id, bytes);
		return true;
	}
}

	bool DecompiledPlugin::ensureLoaded() const
	{
		if (loaded_) return stored_ != nullptr;
		loaded_ = true;
		stored_ = std::make_unique<Stored>();
		if (!loadManifest())
		{
			stored_.reset();
			return false;
		}
		return stored_ != nullptr;
	}

	bool DecompiledPlugin::loadManifest() const
	{
		const std::string dirStr = dir_.str();
		const std::string manifestPath = join(dirStr, "plugin.json");
		std::string manifest;
		if (!readBinaryFile(manifestPath, manifest) || manifest.empty())
			return false;

		// Validate the format marker so a random directory with a stray
		// plugin.json doesn't get picked up as a plugin.
		std::string fmt;
		if (!readString(manifest, "format", 0, fmt) || fmt != "openskyscraper-plugin")
			return false;
		long long ver = 0;
		if (!readScalarInt(manifest, "formatVersion", 0, ver))
			return false;

		// ---- RT_BITMAP sheets (sprites/*.png) ----
		// id 1000 = ingame, 100 = toolbox, 1100 = occupants, 200 = walking.
		// Numeric id names are the shipped convention; hand-made plugins may
		// use the canonical names (sprites/ingame.png etc.), so fall back to
		// those only when the numeric file was absent.
		if (!loadFileResource(stored_->table, dirStr, "sprites/1000.png", "2", 1000))
			loadFileResource(stored_->table, dirStr, "sprites/ingame.png",    "2", 1000);
		if (!loadFileResource(stored_->table, dirStr, "sprites/100.png",  "2", 100))
			loadFileResource(stored_->table, dirStr, "sprites/toolbox.png",   "2", 100);
		if (!loadFileResource(stored_->table, dirStr, "sprites/1100.png", "2", 1100))
			loadFileResource(stored_->table, dirStr, "sprites/occupants.png", "2", 1100);
		if (!loadFileResource(stored_->table, dirStr, "sprites/200.png",  "2", 200))
			loadFileResource(stored_->table, dirStr, "sprites/walking.png",   "2", 200);

		// ---- OBJMAP frame layouts (meta/objmap_*.txt) ----
		loadFileResource(stored_->table, dirStr, "meta/objmap_1000.txt", "OBJMAP", 1000);
		loadFileResource(stored_->table, dirStr, "meta/objmap_100.txt",  "OBJMAP", 100);
		loadFileResource(stored_->table, dirStr, "meta/objmap_200.txt",  "OBJMAP", 200);

		// ---- typed metadata, synthesised from the manifest ----
		// ATTR/DESC so getAttr()/getDesc() work without a binary blob.
		std::string attr = buildAttrBlob(manifest);
		if (!attr.empty()) putResource(stored_->table, "ATTR", 128, attr);
		std::string desc = buildDescBlob(manifest);
		if (!desc.empty()) putResource(stored_->table, "DESC", 128, desc);

		// ---- downscale target (bitmaps.ingame.downscale) ----
		// Read from the first present ingame sheet. The schema allows a
		// `toW`/`toH` pair giving the engine cell-grid size of the whole
		// sheet (e.g. Office: toW=144, toH=168 = 2 cols × 7 rows of 72×24).
		size_t bitmapsScope = findObject(manifest, "bitmaps", 0);
		if (bitmapsScope != std::string::npos)
		{
			size_t ingameScope = findObject(manifest, "ingame", bitmapsScope);
			if (ingameScope != std::string::npos)
			{
				size_t dsScope = findObject(manifest, "downscale", ingameScope);
				if (dsScope != std::string::npos)
				{
					long long w = 0, h = 0;
					if (readScalarInt(manifest, "toW", dsScope, w) &&
					    readScalarInt(manifest, "toH", dsScope, h))
					{
						stored_->downscaleW = static_cast<unsigned int>(w);
						stored_->downscaleH = static_cast<unsigned int>(h);
					}
				}
			}
		}
		return true;
	}

	const WindowsPEExecutable::Resource * DecompiledPlugin::getResource(const std::string & type, int id) const
	{
		if (!ensureLoaded() || !stored_) return nullptr;
		auto tit = stored_->table.find(type);
		if (tit == stored_->table.end()) return nullptr;
		auto iit = tit->second.find(id);
		if (iit == tit->second.end()) return nullptr;
		return &iit->second;
	}

	std::vector<AbstractPlugin::ResourceKey> DecompiledPlugin::enumerateResources() const
	{
		std::vector<ResourceKey> out;
		if (!ensureLoaded() || !stored_) return out;
		for (const auto & tp : stored_->table)
			for (const auto & ip : tp.second)
				out.push_back({tp.first, ip.first});
		return out;
	}

	std::map<std::string, std::vector<int>> DecompiledPlugin::resourcesByType() const
	{
		std::map<std::string, std::vector<int>> out;
		if (!ensureLoaded() || !stored_) return out;
		for (const auto & tp : stored_->table)
		{
			std::vector<int> & ids = out[tp.first];
			for (const auto & ip : tp.second)
				ids.push_back(ip.first);
		}
		return out;
	}

	bool DecompiledPlugin::downscaleTarget(unsigned int & w, unsigned int & h) const
	{
		if (!ensureLoaded() || !stored_) return false;
		if (stored_->downscaleW == 0 || stored_->downscaleH == 0) return false;
		w = stored_->downscaleW;
		h = stored_->downscaleH;
		return true;
	}
}
