/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <string>

namespace OT
{
	class AbstractPlugin;
	class Plugin;

	/// Utilities for rasterizing plugin resources into SFML textures. Split
	/// out of BitmapManager so the test target can link just this file (no
	/// Application/DataManager dependency).
	///
	/// Works against AbstractPlugin so both PE (YootTower `.t2p`) and
	/// folder-based (OpenTower `DecompiledPlugin`) sources are handled by
	/// one code path. OpenTower plugins carry a `downscaleTarget` in their
	/// manifest; when present the decoded image is resampled to that exact
	/// size so it lands on the engine's cell grid (Office 72×24, Condo
	/// 128×24, Hotel 32–56×24) without any per-item frame-math changes.
	struct PluginBitmapLoader
	{
		/// Loads RT_BITMAP `resourceId` from `plugin` into `dst`. The
		/// plugin must already be loaded (caller is expected to call
		/// ensureLoaded() or accept the implicit load this performs via
		/// getResource). Returns false if the plugin failed to load or
		/// doesn't have the resource.
		static bool loadFromPlugin(AbstractPlugin & plugin, int resourceId,
		                           sf::Texture & dst,
		                           const sf::Color & maskColor = sf::Color::Transparent);

		/// Loads RT_BITMAP `resourceId` from any PE/SP file on disk.
		/// Used by tests and by callers that haven't (or can't) put the
		/// file on the PluginManager catalog. Production code should
		/// prefer BitmapManager::loadFromPlugin(name, ...) so the
		/// catalog stays the single source of truth.
		static bool loadFromPEFile(const std::string & path, int resourceId,
		                           sf::Texture & dst,
		                           const sf::Color & maskColor = sf::Color::Transparent);

		/// Turns raw RT_BITMAP bytes (a Windows DIB without the 14-byte
		/// BMP file header) into a texture. Exposed for unit tests that
		/// want to feed known-good DIB bytes without going through a PE.
		static bool rasterizeBitmapResource(const char * data, int length,
		                                    sf::Texture & dst,
		                                    const sf::Color & maskColor = sf::Color::Transparent);

		/// Same as rasterizeBitmapResource but yields the decoded pixels as
		/// an sf::Image (no GPU upload). Used by the plugin editor's t2p
		/// converter, which needs the raw pixels to write a PNG. The mask is
		/// NOT applied here — callers that want color-key transparency can
		/// call image.createMaskFromColor afterwards.
		static bool decodeBitmapResource(const char * data, int length,
		                                 sf::Image & dst);

		/// Box-filter downscale. If `src` is already <= `targetW`×`targetH`
		/// it is returned unchanged. Otherwise each output pixel is the
		/// unweighted average of the source block it maps to. Used to bring
		/// OpenTower's ~3x-resolution art onto the engine's cell grid. The
		/// averaging preserves transparent regions (alpha-weighted) so
		/// color-keyed sprites don't develop dark fringes.
		static void downscaleImage(const sf::Image & src, sf::Image & dst,
		                           unsigned int targetW, unsigned int targetH);
	};
}
