/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
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

		// ---- typed accessors (shared logic; subclasses just provide
		// getResource). Each fetches the corresponding resource at id 128
		// (the conventional id for an item's metadata) and runs it through
		// the parsers in PluginResources. ----
		bool getAttr(PluginAttr & out) const;  /// ATTR id=128
		bool getDesc(PluginDesc & out) const;  /// DESC id=128
		bool getDpdf(PluginDpdf & out) const;  /// DPDF id=128
		bool getPldf(PluginPldf & out) const;  /// PLDF id=128
		bool getMvdf(PluginMvdf & out) const;  /// MVDF id=1 (Mover plugins only)

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
