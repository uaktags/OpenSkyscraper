#pragma once
#include <string>
#include <SFML/Graphics/Texture.hpp>
#include "ResourceManager.h"

namespace OT
{
	class Application;

	class BitmapManager : public ResourceManager<sf::Texture>
	{
	public:
		BitmapManager(Application * app) : ResourceManager<sf::Texture>(app), usedFallback(false), internal_path("simtower") { internal_path.push_back(Path::SEPARATOR); }
		bool load(Path name, sf::Texture & dst);

		/// Loads an RT_BITMAP resource from a cataloged plugin (PE `.t2p`
		/// or folder-based OpenTower plugin). Returns false if the plugin
		/// isn't in the PluginManager catalog, fails to load, or doesn't
		/// have the requested resource. If `maskColor` is non-transparent,
		/// that exact color is made transparent in the resulting texture
		/// (the YootTower convention for sprite-background keying, e.g. the
		/// magenta in Condo.t2p).
		///
		/// The actual rasterization is in PluginBitmapLoader so tests can
		/// exercise it without an Application singleton.
		bool loadFromPlugin(const std::string & pluginName, int resourceId,
		                    sf::Texture & dst,
		                    const sf::Color & maskColor = sf::Color::Transparent);

		/// Edition-aware texture resolution for the legacy item classes.
		/// `logicalName` is the short item id ("office", "condo",
		/// "hotel_single", ...). If an OpenTower folder plugin for that
		/// item is cataloged (e.g. "OpenTower/Office"), its RT_BITMAP 1000
		/// is loaded when SimTowerLoader has not already populated the requested
		/// texture. An explicitly selected or auto-discovered SIMTOWER executable
		/// therefore takes precedence; OpenTower remains the fallback edition.
		/// The result is cached under the simtower key, letting item classes keep
		/// their existing
		/// `App->bitmaps["simtower/office"]` calls unchanged while the art
		/// source switches transparently when the OpenTower edition is
		/// installed.
		sf::Texture & editionTexture(const char * logicalName,
		                             const char * simtowerKey);

		bool usedFallback;
	private:
		std::string internal_path;
	};
}
