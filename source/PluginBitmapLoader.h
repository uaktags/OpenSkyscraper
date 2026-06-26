/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <string>

namespace OT
{
	class Plugin;

	/// Utilities for rasterizing YootTower plugin resources into SFML
	/// textures. Split out of BitmapManager so the test target can link
	/// just this file (no Application/DataManager dependency).
	///
	/// Two entry points:
	///   - loadFromPlugin(Plugin&, ...) : the body that turns a cataloged
	///     Plugin's RT_BITMAP resource into a texture.
	///   - loadFromPEFile(path, ...)    : convenience wrapper that builds
	///     a throwaway Plugin from an arbitrary PE/SP file path; useful
	///     for tests and for one-off loads that bypass the catalog.
	struct PluginBitmapLoader
	{
		/// Loads RT_BITMAP `resourceId` from `plugin` into `dst`. The
		/// plugin must already be loaded (caller is expected to call
		/// ensureLoaded() or accept the implicit load this performs via
		/// getResource). Returns false if the plugin failed to load or
		/// doesn't have the resource.
		static bool loadFromPlugin(Plugin & plugin, int resourceId,
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
	};
}
