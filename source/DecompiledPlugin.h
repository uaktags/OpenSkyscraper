/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Folder-based ("decompiled") plugin loader for the OpenTower edition.
///
/// A DecompiledPlugin is a directory on disk with this layout:
///
///   <Plugin>/
///     plugin.json          manifest (OSP v1 schema + downscale block)
///     sprites/
///       1000.png           RT_BITMAP id 1000 (primary sprite sheet)
///       100.png            RT_BITMAP id 100  (toolbox icon, optional)
///       1100.png           RT_BITMAP id 1100 (occupants, optional)
///     meta/
///       objmap_1000.txt    OBJMAP id 1000 (frame rects)
///       objmap_100.txt     OBJMAP id 100  (optional)
///
/// It implements AbstractPlugin: getResource(type,id) returns the file
/// contents (PNG bytes for RT_BITMAP, OBJMAP text for OBJMAP, synthesised
/// ATTR/DESC/DPDF/PLDF blobs for the typed metadata accessors). This means
/// a folder plugin is interchangeable with a PE `.t2p` plugin everywhere
/// — BitmapManager, PluginBitmapLoader, tests — without those callers
/// knowing which backing format they hold.
///
/// The `downscale` block in the manifest declares the engine cell size;
/// PluginBitmapLoader reads it (via downscaleTarget()) and resamples the
/// ~3x art down so per-item frame math is untouched.
#pragma once
#include "AbstractPlugin.h"
#include "Path.h"
#include "WindowsPEExecutable.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace OT
{
	class DecompiledPlugin : public AbstractPlugin
	{
	public:
		DecompiledPlugin(std::string name, Path dir)
		    : AbstractPlugin(std::move(name)), dir_(std::move(dir)) {}

		bool ensureLoaded() const override;
		bool isLoaded() const override { return loaded_; }

		const WindowsPEExecutable::Resource * getResource(const std::string & type, int id) const override;
		std::vector<ResourceKey> enumerateResources() const override;
		std::map<std::string, std::vector<int>> resourcesByType() const override;

		/// From the manifest's `bitmaps.<sheet>.downscale` block. The first
		/// ingame sheet (id 1000) that declares a target wins; toolbox/
		/// occupant sheets are never downscaled today (they're loaded at
		/// native size by callers that don't index into a cell grid).
		bool downscaleTarget(unsigned int & w, unsigned int & h) const override;

	private:
		Path dir_;

		/// Loaded lazily by ensureLoaded(). Built once and cached; the
		/// mutable lets ensureLoaded() stay const (matching Plugin).
		struct Stored
		{
			/// (type,id) -> resource. We synthesise PE-style Resource
			/// entries (owning their data buffers) so getResource can return
			/// the same pointer type the PE path returns.
			std::map<std::string, WindowsPEExecutable::Resources> table;
			unsigned int downscaleW = 0;
			unsigned int downscaleH = 0;
		};
		mutable std::unique_ptr<Stored> stored_;
		mutable bool loaded_ = false;

		bool loadManifest() const;
	};
}
