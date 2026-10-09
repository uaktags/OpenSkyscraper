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
#include "PluginDefs.h"
#include "PluginObjmap.h"
#include "PluginResourceCodec.h"
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

	/// The current OSP on-disk schema version. A higher version on load is a
	/// hard error (not silent corruption). Bump when the schema gains a
	/// non-back-compatible field.
	///
	/// v2 renamed the ATTR keys to the proven T2DLL semantics (pluginId 4cc,
	/// numeric itemType, dependPluginId) and replaced the mislabeled
	/// "placement"/"price" objects with "dayParam" (DPDF) and "placeParam"
	/// (PLDF). v1 manifests are still read; see readOspAttr.
	///
	/// v3 adds the lossless resource store: every resource of the source
	/// plugin, listed in the manifest's "resources" array and kept as typed
	/// Json (resources/<TYPE>/<id>.json), verbatim text (.txt), PNG plus the
	/// DIB header/palette sidecar (bitmaps/), RIFF .wav files (sounds/) or raw
	/// bytes (.bin), plus the original DLL image (pe/original.t2p) for .t2p
	/// export. The v2 blocks are still written (derived from the resources)
	/// so v2 readers and the game's folder-plugin loader keep working.
	constexpr int kOspFormatVersion = 3;
	/// Oldest manifest version loadOspPlugin still accepts.
	constexpr int kOspMinFormatVersion = 1;

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

	/// The human-readable price/name strings of a mover's first MVDF
	/// variation (the rest of the MVDF is not yet part of the OSP manifest).
	struct OspTransport
	{
		bool valid = false;
		std::string priceString;      // mPriceString, e.g. "$150000:$50000/car"
		std::string outMoneyString;   // mOutMoneyString, e.g. "$12000/Q" (v1 key "quarterlyString")
		std::string name;             // mName
	};

	/// The in-memory editable plugin. Built by loadOspPlugin (from an OSP
	/// directory) or convertT2pToOsp (from a .t2p); mutated by the editor
	/// UI; serialized by saveOspPlugin.
	/// One resource of the source plugin, in container order (OSP v3).
	/// Bitmaps (RT_BITMAP / SP DIBs) and WAVE sounds stay Raw in `res`; the
	/// OSP writer stores them as PNG + palette sidecar and .wav files.
	struct OspResource
	{
		DecodedResource res;
		uint32_t spType = 0;          // SP containers only: CResFile tag constant (0 = DIB)
	};

	struct OspPlugin
	{
		/// "yoottower-t2p" (converted, not yet saved) | "openskyscraper-plugin"
		/// (loaded from OSP) | "scratch" (new, unsaved).
		std::string sourceFormat = "scratch";
		std::string sourceFile;                          // original filename, if any
		PluginDesc::Kind pluginClass = PluginDesc::TenantPlugin;

		PluginAttr attr;                                 // identity (always present)
		std::optional<DayParamDef> dayParam;             // DPDF 128 (customer day/weather %)
		std::optional<PlaceParamDef> placeParam;         // PLDF 128 (customer place %/score)
		OspTransport transport;                          // optional (movers, from MVDF)
		OspSpriteSheet toolbox;                          // RT_BITMAP 100  + OBJMAP 100
		OspSpriteSheet ingame;                           // RT_BITMAP 1000 + OBJMAP 1000
		OspSpriteSheet occupants;                        // RT_BITMAP 1100 (resident/shop SHIL)
		OspSpriteSheet walking;                          // RT_BITMAP 200  + OBJMAP 200 (mover SHIL)

		// --- OSP v3: the lossless resource store (empty for v1/v2/scratch) ---
		ResourceContainer container = ResourceContainer::PE;
		std::vector<OspResource> resources;              // container order
		std::string peImage;                             // original DLL bytes (PE plugins; .t2p export)
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

	/// Uncompressed 4/8-bpp DIB (BITMAPINFOHEADER + palette + rows) layout,
	/// as written into OSP bitmap resources. Exposed for validation tests.
	struct DibLayout
	{
		int32_t width = 0, height = 0;   // height < 0 = top-down
		uint16_t bpp = 0;
		uint32_t colors = 0;
		size_t pixels = 0;               // offset of the pixel rows
		size_t stride = 0;
		/// Row count; callers must reject INT32_MIN first (negating it is UB).
		uint32_t rows() const { return uint32_t(height < 0 ? -height : height); }
	};
	/// Parses and validates the DIB header in d. False on malformed input,
	/// including a height of INT32_MIN (rows() would be signed overflow).
	bool parseDib(const std::string & d, DibLayout & L);

	/// Reads the "attr" object of an OSP manifest (v1 or v2) into `out`.
	/// v2 keys: pluginId (Mac-order 4cc, "BURG"), itemType (number), name,
	/// gameLevel, attrib, dependPluginId (Mac-order 4cc, "****" = none),
	/// secretItem, mustItem. v1 keys are mapped for compatibility: a string
	/// "itemType" is the file-order plugin id ("GRUB"), "id" is the numeric
	/// item type and "marker" the file-order dependency ("NORF").
	/// Returns false if the manifest has no "attr" object.
	bool readOspAttr(const std::string & manifest, PluginAttr & out);

	/// True if `dir` looks like an OSP plugin root (contains plugin.json).
	bool isOspPluginDir(const std::string & dir);

	// ---- OSP v3 helpers ----

	/// Resource by (type key, id); type keys as in DecodedResource ("2" for
	/// RT_BITMAP, "TNDF", ...). nullptr when absent.
	OspResource * findOspResource(OspPlugin & p, const std::string & type, int id);
	const OspResource * findOspResource(const OspPlugin & p, const std::string & type, int id);

	/// Re-derives the v2 view (pluginClass, attr, dayParam, placeParam,
	/// transport and the four sprite sheets) from `resources`. No-op when
	/// the plugin has no resources.
	void syncOspLegacyFromResources(OspPlugin & p);

	/// Builds typed ATTR/DESC/DPDF/PLDF resources from the v2 fields of a
	/// plugin that has none (v1/v2 manifests, new scratch plugins), so the
	/// resource editor can work on it. Scratch Tenants also get a one-
	/// variation TNDF with the population defaults.
	void synthesizeOspResources(OspPlugin & p);

	/// DIB (BITMAPINFOHEADER, 4/8 bpp, uncompressed) <-> PNG pixels plus a
	/// Json sidecar holding the header, the palette, non-zero row padding
	/// and trailing bytes. pngToDib(dibToPng(x)) == x for every shipped
	/// bitmap. An edited image is mapped back onto the palette (exact colour
	/// first, nearest otherwise); a resized one gets a fresh header.
	bool dibToPng(const std::string & dib, sf::Image & image, Json & sidecar);
	bool pngToDib(const sf::Image & image, const Json & sidecar, std::string & dib, std::string * error = nullptr);
	/// Re-quantises `image` onto the palette of an existing DIB resource.
	bool replaceDibPixels(const std::string & dib, const sf::Image & image, std::string & out, std::string * error = nullptr);
}
