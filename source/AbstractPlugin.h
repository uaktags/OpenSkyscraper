/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include "PluginResourceCodec.h"
#include "PluginResources.h"
#include "WindowsPEExecutable.h"

#include <map>
#include <string>
#include <vector>

namespace OT
{
	/// Common interface for every plugin backing store.
	///
	/// Originally the only implementation was `Plugin` (a YootTower `.t2p`
	/// Win32 PE DLL). The OpenTower edition ships its art as folder-based
	/// "decompiled" plugins (`DecompiledPlugin`): a `plugin.json` manifest
	/// plus loose PNGs and OBJMAP text files. Both expose the same surface
	/// so callers — BitmapManager, PluginBitmapLoader, the editor, tests —
	/// treat them uniformly and need no knowledge of the backing format.
	///
	/// `ResourceKey` and `getResource` mirror the PE resource model: a
	/// resource is addressed by (type, id), where `type` is a string
	/// ("ATTR", "DESC", "OBJMAP", "2" for RT_BITMAP, ...) and `id` an int.
	/// The folder plugin synthesises the same (type,id) keys from its
	/// manifest so the OBJMAP/ATTR/RT_BITMAP lookups are identical.
	class AbstractPlugin
	{
	public:
		virtual ~AbstractPlugin() = default;

		const std::string & name() const { return name_; }

		/// Parses the backing store on first access. Idempotent. Returns
		/// false if parsing failed (the next call retries). PE plugins lazy-
		/// load their DLL here; folder plugins parse plugin.json here.
		virtual bool ensureLoaded() const = 0;
		virtual bool isLoaded() const = 0;

		struct ResourceKey
		{
			std::string type;
			int id;
		};

		/// Flat list of every (type, id) the plugin exposes. Empty if it
		/// failed to load. Used by the coverage audit and the editor.
		virtual std::vector<ResourceKey> enumerateResources() const = 0;

		/// Looks up a single resource by (type, id). Returns nullptr if the
		/// plugin isn't loaded or the resource is absent. The returned
		/// pointer is owned by the plugin and is valid until it is destroyed
		/// or reloaded.
		virtual const WindowsPEExecutable::Resource * getResource(const std::string & type, int id) const = 0;

		bool hasResource(const std::string & type, int id) const { return getResource(type, id) != nullptr; }

		/// Resources grouped by type. Each key is a resource type name and
		/// each value is the sorted list of ids under that type.
		virtual std::map<std::string, std::vector<int>> resourcesByType() const = 0;

		/// Which container the resources came from (SP sub-plugins reuse the
		/// PPDF tag with a different layout). Folder plugins report PE.
		virtual ResourceContainer container() const { return ResourceContainer::PE; }

		// ---- typed accessors (shared logic; subclasses just provide
		// getResource). Each fetches one resource and runs it through the
		// codecs in PluginResources / PluginDefs / SubPluginDefs. They return
		// false if the resource is absent or does not decode. ----
		bool getAttr(PluginAttr & out) const;  /// ATTR id=128
		bool getDesc(PluginDesc & out) const;  /// DESC id=128

		/// The main template definition: TNDF (tenant), MVDF (mover) or OODF
		/// (outer object), id 1 - whichever the plugin carries.
		bool getTemplateDef(TemplateDef & out) const;
		/// TMDF member table by id (1 = employees, 2 = customers; SP plugins
		/// use 128 for the audience table).
		bool getMemberTable(MemberTableDef & out, int id) const;
		/// Every TMDF the plugin carries, keyed by resource id.
		std::map<int, MemberTableDef> getMemberTables() const;
		/// DPDF / PLDF by id. The ids come from TMDF dayParamID/placeParamID
		/// (128 in the shipped plugins).
		bool getDayParamDef(DayParamDef & out, int id = 128) const;
		bool getPlaceParamDef(PlaceParamDef & out, int id = 128) const;
		bool getPeopleAnimeDef(PeopleAnimeDef & out, int id = 1) const;  /// PADF
		bool getSilhouettes(SilhouetteList & out, int id = 1) const;     /// SLDF
		bool getPeopleDefs(PeopleList & out, int id = 1) const;          /// PPDF (PE world plugins)
		bool getWords(WordList & out, int id = 1) const;                 /// WODF
		/// STRTABLE (or STRTBL) by id.
		bool getStringTable(StringTableDef & out, int id) const;
		/// RT_STRING block by id (block n holds string ids 16(n-1)..16n-1).
		bool getStringBlock(StringBlock & out, int id) const;
		/// RT_VERSION (the first one).
		bool getVersionInfo(VersionInfo & out) const;

		// SP sub-plugins (id 128).
		bool getMovieDef(MovieDef & out) const;          /// MODF
		bool getHallEventDef(HallEventDef & out) const;  /// HEDF
		bool getAdvertiseDef(AdvertiseDef & out) const;  /// ADDF
		bool getSubPluginPeopleParamDef(SubPluginPeopleParamDef & out, int id = 128) const;  /// SP PPDF

		/// Generic access: decodes any resource with the per-type dispatcher.
		bool decodeResource(const std::string & type, int id, DecodedResource & out) const;

		/// Downscale factor applied when this plugin's RT_BITMAP art is
		/// rasterized. 1 = use the source pixels as-is (PE plugins, art
		/// already at engine scale). OpenTower folder plugins store art at
		/// ~3x engine resolution and return a fractional factor (e.g. ~0.45)
		/// so PluginBitmapLoader can resample to the engine's cell grid
		/// without touching per-item frame math. See `downscaleTarget`.
		virtual int downscaleFactorX() const { return 1; }
		virtual int downscaleFactorY() const { return 1; }
		/// When non-empty, PluginBitmapLoader resamples the decoded image to
		/// exactly this size (overrides the factor). Folder plugins use this
		/// to declare the target sheet dimensions in the manifest.
		virtual bool downscaleTarget(unsigned int & w, unsigned int & h) const { (void)w; (void)h; return false; }

	protected:
		explicit AbstractPlugin(std::string name) : name_(std::move(name)) {}
		std::string name_;
	};
}
