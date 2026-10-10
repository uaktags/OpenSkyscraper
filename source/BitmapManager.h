#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "ResourceManager.h"
#include "Skin.h"

namespace OT
{
	class Application;

	class BitmapManager : public ResourceManager<sf::Texture>
	{
	public:
		BitmapManager(Application * app) : ResourceManager<sf::Texture>(app), usedFallback(false), internal_path("simtower") { internal_path.push_back(Path::SEPARATOR); }

		/// Sets the active skin. Called before texture loads at startup;
		/// live switching goes through Game::setSkin, which purges caches
		/// and refreshes sprites on top of this.
		void setSkin(Skin skin) { m_skin = skin; }
		Skin skin() const { return m_skin; }

		/// True when an OpenTower folder plugin cataloged under a name
		/// mapped for `logicalName` can provide art (used by the
		/// SimTowerLoader install gate to leave room for plugin sheets).
		bool editionProvides(const char * logicalName) const;

		/// editionProvides for a full texture key such as
		/// "simtower/office": strips the simtower prefix and checks the
		/// OpenTower plugin catalog.
		bool editionProvidesKey(const Path & name) const;

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
		/// `logicalName` is the short item id ("office", "condo", ...).
		/// Resolution depends on the active skin:
		///   Classic  — EXE sheet installed by SimTowerLoader, then
		///              data/bitmaps file, then placeholder.
		///   OpenSky  — OpenTower folder plugin RT_BITMAP 1000, then the
		///              EXE sheet (only for keys the install gate left
		///              room for), then data/bitmaps, then placeholder.
		///   Yoot     — YootTower t2p art where available (condo today),
		///              then data/bitmaps/yoottower drop-ins, then EXE
		///              sheets the gate allowed, then placeholders.
		/// The result is cached under the simtower key, letting item
		/// classes keep their existing texture calls while the art source
		/// switches transparently.
		sf::Texture & editionTexture(const char * logicalName,
		                             const char * simtowerKey);

		/// True when first-party replacement art for `name` (a texture key
		/// such as "simtower/condo") ships as data/bitmaps/<key>.png.
		/// SimTowerLoader consults this before installing the extracted
		/// EXE sheet so the file edition is what loads.
		bool hasFirstPartyArt(const Path & name) const;

		/// High-resolution twin of a cached texture: data/bitmaps@4x/<key>
		/// with the same layout at `scale`x pixels. Returns nullptr unless
		/// the twin is exactly scale x the cached 1x texture and the cache
		/// still maps its key to `base` (so skin switches never mix art).
		/// Loaded lazily on first request and cached (misses too).
		const sf::Texture * hiresFor(const sf::Texture * base, unsigned & scale);

		bool usedFallback;
	private:
		struct Hires {
			Path key;
			sf::Vector2u baseSize;
			std::unique_ptr<sf::Texture> texture; // null = no twin
		};
		std::unordered_map<const sf::Texture *, Hires> hires;
		std::string internal_path;
		Skin m_skin = Skin::OpenSky;
	};
}
