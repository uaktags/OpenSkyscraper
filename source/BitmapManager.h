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

		/// Loads an RT_BITMAP resource from a cataloged .t2p plugin.
		/// Returns false if the plugin isn't in the PluginManager catalog,
		/// fails to load, or doesn't have the requested resource. If
		/// `maskColor` is non-transparent, that exact color is made
		/// transparent in the resulting texture (the YootTower convention
		/// for sprite-background keying, e.g. the magenta in Condo.t2p).
		///
		/// The actual rasterization is in PluginBitmapLoader so tests can
		/// exercise it without an Application singleton.
		bool loadFromPlugin(const std::string & pluginName, int resourceId,
		                    sf::Texture & dst,
		                    const sf::Color & maskColor = sf::Color::Transparent);

		bool usedFallback;
	private:
		std::string internal_path;
	};
}
