/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include "Path.h"
#include "PluginResources.h"
#include "WindowsPEExecutable.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace OT
{
	/// A single YootTower plugin (a .t2p PE32 DLL).
	///
	/// The PE is parsed lazily on first resource access; once loaded the
	/// WindowsPEExecutable is cached for the lifetime of the Plugin. This
	/// keeps catalog startup cheap (156 plugins can be discovered with no
	/// PE parsing) and concentrates the cost on the items that actually
	/// get consulted.
	class Plugin
	{
	public:
		Plugin(std::string name, Path path) : name_(std::move(name)), path_(std::move(path)) {}

		const std::string & name() const { return name_; }
		const Path & path() const { return path_; }

		/// Parses the PE if it hasn't been parsed yet. Idempotent. Returns
		/// false if the PE failed to parse (the next call will retry).
		bool ensureLoaded() const;
		bool isLoaded() const { return loaded_; }

		/// Looks up a single resource by (type, id). Returns nullptr if
		/// the plugin isn't loaded or the resource is absent. The
		/// `type` string matches the keys of WindowsPEExecutable::resources
		/// (e.g. "OBJMAP", "ATTR", or "2" for RT_BITMAP).
		const WindowsPEExecutable::Resource * getResource(const std::string & type, int id) const;
		bool hasResource(const std::string & type, int id) const { return getResource(type, id) != nullptr; }

		struct ResourceKey
		{
			std::string type;
			int id;
		};
		/// Flat list of every (type, id) declared in the resource directory.
		/// Empty if the plugin failed to load.
		std::vector<ResourceKey> enumerateResources() const;

		/// Resources grouped by type. Each key is a resource type name
		/// (e.g. "OBJMAP", "ATTR", "2" for RT_BITMAP) and each value is
		/// the sorted list of resource ids under that type. The natural
		/// shape for the upcoming coverage audit, which spends most of
		/// its time asking "which plugins have OBJMAP id 1000?" without
		/// caring about other types.
		std::map<std::string, std::vector<int>> resourcesByType() const;

		/// Typed accessors. Each fetches the corresponding resource at
		/// id 128 (the conventional id for an item's metadata) and runs
		/// it through the parsers in PluginResources. Returns false if
		/// the plugin failed to load or the resource is absent/malformed.
		/// Field layouts: see doc/yoottower/resource-formats.md.
		bool getAttr(PluginAttr & out) const;  /// ATTR id=128
		bool getDesc(PluginDesc & out) const;  /// DESC id=128
		bool getDpdf(PluginDpdf & out) const;  /// DPDF id=128

	private:
		std::string name_;
		Path path_;
		mutable std::unique_ptr<WindowsPEExecutable> exe_;
		mutable bool loaded_ = false;
	};

	/// Catalogs every .t2p under one or more plugin directories. Cheap to
	/// construct; the heavy work (PE parsing) happens per-plugin on demand.
	///
	/// Independent of Application/DataManager so it can be unit-tested in
	/// isolation. The Application wires the search paths in by calling
	/// addPluginDirectory() for each base data dir's Plugins/ subfolder.
	class PluginManager
	{
	public:
		/// Scans `dir` for *.t2p files (case-insensitive extension) and
		/// adds them to the catalog. Idempotent: a second call with the
		/// same dir is a no-op and returns 0. Returns the number of newly
		/// discovered plugins.
		int addPluginDirectory(const Path & dir);

		bool has(const std::string & name) const;
		Plugin * get(const std::string & name) const;

		/// Plugin filenames discovered so far, sorted alphabetically.
		std::vector<std::string> enumerate() const;

		/// Counts how many of `essential` (a list of plugin filenames like
		/// "Condo.t2p") are present in the catalog. Used by Application's
		/// asset-fallback ladder to decide if the YootTower tier is usable.
		int countEssential(const std::vector<std::string> & essential) const;

	private:
		std::map<std::string, std::unique_ptr<Plugin>> plugins_;
		std::vector<Path> scannedDirs_;
	};
}
