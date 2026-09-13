/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// The OpenSkyscraper plugin (OSP) format — the native, editable, save target.
///
/// An OSP plugin is a loose directory:
///
///   Name.osp-plugin/
///     plugin.json          manifest (schema below)
///     sprites/
///       toolbox.png        RT_BITMAP 100 (toolbox icon sheet, ~26x130)
///       ingame.png         RT_BITMAP 1000 (primary sprite sheet)
///       occupants.png      RT_BITMAP 1100 (people animation, optional)
///     meta/
///       objmap_100.txt     toolbox-icon rects (OBJMAP grammar)
///       objmap_1000.txt    primary sprite rects (OBJMAP grammar)
///
/// This file defines the in-memory editable model (OspPlugin), the reader
/// (loadOspPlugin) and writer (saveOspPlugin), and the t2p converter
/// (convertT2pToOsp). JSON handling is a tiny hand-rolled reader/writer for
/// the flat schema — no external JSON dependency. See
/// doc/openskyscraper/plugin-format.md for the full spec.
#pragma once
#include "PluginObjmap.h"
#include "PluginResources.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OT
{
	class Plugin;  // forward decl — defined in PluginManager.h

	/// The current OSP on-disk schema version. The editor only reads/writes
	/// this version; a higher version on load is a hard error (not silent
	/// corruption). Bump when the schema gains a non-back-compatible field.
	constexpr int kOspFormatVersion = 1;

	/// How the transparent (color-key) mask for a sprite sheet was chosen.
	/// Recorded in the manifest so a future reload can re-derive or flag it.
	enum class MaskDiscovery
	{
		None,     // no mask (opaque sheet)
		Manual,   // hand-picked / known from the source plugin
		Corner,   // sampled from the top-left pixel of the sheet
	};

	/// One sprite sheet and its OBJMAP layout. `image` holds the decoded
	/// pixels (mask already applied); `blocks` slice it into named rects.
	/// `relPath` is the path relative to the plugin dir (e.g. "sprites/1000.png").
	struct OspSpriteSheet
	{
		bool present = false;             // false => no resource on the source
		std::string relPath;              // manifest-relative PNG path
		sf::Image image;                  // pixels (mask applied if any)
		std::optional<sf::Color> mask;    // the keyed-out color, if any
		MaskDiscovery maskHow = MaskDiscovery::None;
		std::vector<ObjmapBlock> blocks;  // OBJMAP rects (may be empty)
	};

	/// The in-memory editable plugin. Built by loadOspPlugin (from an OSP
	/// directory) or convertT2pToOsp (from a .t2p); mutated by the editor
	/// UI; serialized by saveOspPlugin.
	struct OspPlugin
	{
		/// "yoottower-t2p" (converted, not yet saved) | "openskyscraper-plugin"
		/// (loaded from OSP) | "scratch" (new, unsaved).
		std::string sourceFormat = "scratch";
		std::string sourceFile;                          // original filename, if any
		PluginDesc::Kind pluginClass = PluginDesc::TenantPlugin;

		PluginAttr attr;                                 // identity (always present)
		PluginDpdf placement;                            // optional (placement-driven items)
		PluginPldf price;                                // optional (shops, PLDF)
		PluginMvdf transport;                            // optional (movers, MVDF)
		OspSpriteSheet toolbox;                          // RT_BITMAP 100  + OBJMAP 100
		OspSpriteSheet ingame;                           // RT_BITMAP 1000 + OBJMAP 1000
		OspSpriteSheet occupants;                        // RT_BITMAP 1100 (resident/shop SHIL)
		OspSpriteSheet walking;                          // RT_BITMAP 200  + OBJMAP 200 (mover SHIL)
	};

	/// Result of a load/save/convert operation. On error, `ok` is false and
	/// `message` carries a human-readable cause for the editor to display.
	struct OspResult
	{
		bool ok = true;
		std::string message;
	};

	/// Loads an OSP plugin directory into `out`. Fails if the manifest is
	/// missing, the format marker is wrong, or the version is unsupported.
	/// Missing sprite sheets are tolerated (left `present=false`).
	OspResult loadOspPlugin(const std::string & dir, OspPlugin & out);

	/// Writes `plugin` to `dir` as a complete OSP plugin (creating the
	/// directory tree). Refuses to overwrite an existing directory that is
	/// not recognizable as an OSP plugin unless `overwrite` is true.
	OspResult saveOspPlugin(const std::string & dir, const OspPlugin & plugin,
	                        bool overwrite = false);

	/// Converts a loaded YootTower `.t2p` plugin into an editable OspPlugin.
	/// Pulls ATTR/DESC/DPDF/PLDF via the typed accessors, rasterizes the
	/// RT_BITMAP sheets, and parses the OBJMAP layouts. Sets
	/// `sourceFormat="yoottower-t2p"` and `sourceFile` to the plugin name.
	/// `pluginClass` is taken from the plugin's DESC. (Only Tenant is fully
	/// supported by the editor UI; other classes still convert but the
	/// manifest records their real class.)
	OspResult convertT2pToOsp(Plugin & plugin, OspPlugin & out);

	/// True if `dir` looks like an OSP plugin root (contains plugin.json).
	bool isOspPluginDir(const std::string & dir);
}
