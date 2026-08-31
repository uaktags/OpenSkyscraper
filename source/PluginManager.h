/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include "AbstractPlugin.h"
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
	///
	/// Inherits the typed accessors (getAttr/getDesc/...) from
	/// AbstractPlugin, which it shares with the folder-based
	/// DecompiledPlugin used by the OpenTower edition.
	class Plugin : public AbstractPlugin
	{
	public:
		Plugin(std::string name, Path path)
		    : AbstractPlugin(std::move(name)), path_(std::move(path)) {}

		const Path & path() const { return path_; }

		/// Parses the PE if it hasn't been parsed yet. Idempotent. Returns
		/// false if the PE failed to parse (the next call will retry).
		bool ensureLoaded() const override;
		bool isLoaded() const override { return loaded_; }

		/// Looks up a single resource by (type, id). Returns nullptr if
		/// the plugin isn't loaded or the resource is absent. The
		/// `type` string matches the keys of WindowsPEExecutable::resources
		/// (e.g. "OBJMAP", "ATTR", or "2" for RT_BITMAP).
		const WindowsPEExecutable::Resource * getResource(const std::string & type, int id) const override;

		/// Flat list of every (type, id) declared in the resource directory.
		/// Empty if the plugin failed to load.
		std::vector<ResourceKey> enumerateResources() const override;

		/// Resources grouped by type. Each key is a resource type name
		/// (e.g. "OBJMAP", "ATTR", "2" for RT_BITMAP) and each value is
		/// the sorted list of resource ids under that type. The natural
		/// shape for the upcoming coverage audit, which spends most of
		/// its time asking "which plugins have OBJMAP id 1000?" without
		/// caring about other types.
		std::map<std::string, std::vector<int>> resourcesByType() const override;

	private:
		Path path_;
		mutable std::unique_ptr<WindowsPEExecutable> exe_;
		mutable bool loaded_ = false;
	};

	/// Catalogs every plugin under one or more directories. Cheap to
	/// construct; the heavy work (PE parsing) happens per-plugin on demand.
	///
	/// Two backing formats are discovered:
	///  - `*.t2p` files  → PE `Plugin` instances (YootTower edition).
	///  - subdirectories containing `plugin.json` → `DecompiledPlugin`
	///    instances (OpenTower folder plugins). The catalog key is the
	///    directory path relative to the scanned root, forward-slashed
	///    (e.g. "OpenTower/Office"), so it can never collide with a
	///    "Foo.t2p" filename.
	///
	/// Independent of Application/DataManager so it can be unit-tested in
	/// isolation. The Application wires the search paths in by calling
	/// addPluginDirectory() for each base data dir's Plugins/ subfolder.
	class PluginManager
	{
	public:
		/// Scans `dir` for plugins and adds them to the catalog. `.t2p`
		/// files become PE plugins; immediate subdirectories that contain
		/// a `plugin.json` become DecompiledPlugins (nested one level, so
		/// `Plugins/OpenTower/Office` is cataloged as "OpenTower/Office").
		/// Idempotent: a second call with the same dir is a no-op and
		/// returns 0. Returns the number of newly discovered plugins.
		int addPluginDirectory(const Path & dir);

		bool has(const std::string & name) const;
		AbstractPlugin * get(const std::string & name) const;

		/// Plugin names discovered so far, sorted alphabetically.
		std::vector<std::string> enumerate() const;

		/// Counts how many names from a caller-provided set are present. Kept
		/// for diagnostics and coverage tools; startup never requires a quota.
		int countEssential(const std::vector<std::string> & essential) const;

	private:
		std::map<std::string, std::unique_ptr<AbstractPlugin>> plugins_;
		std::vector<Path> scannedDirs_;
	};
}
