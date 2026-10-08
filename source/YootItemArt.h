/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Plugin sprites for OpenSky items drawn under Skin::Yoot: a plugin's
/// RT_BITMAP 1000 sliced by its OBJMAP 1000 `View` rects (pattern order),
/// or any other OBJMAP id / part name. Pattern index = numOfPattern x kind +
/// pattern (T2Tenant::UpdatePatternIndex). Tenant sheets key their windows
/// with dark magenta (105, 33, 104) so the backdrop shows through.
#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <string>
#include <vector>

namespace OT
{
	class Application;

	struct YootPluginView
	{
		sf::Texture texture;
		std::vector<sf::IntRect> bands;  ///< parts in pattern order
		std::vector<int> patterns;       ///< OBJMAP Pattern of each band
		/// Band for a pattern index, clamped; the whole sheet without bands.
		sf::IntRect band(int index) const;
	};

	/// Window key of the tenant sheets (Office, Condo, hotel rooms).
	const sf::Color kYootWindowKey(105, 33, 104);

	/// Cached sheet `bitmapId` of `plugin` sliced by OBJMAP `objmapId` parts
	/// named `part`. Null when the plugin or bitmap is missing.
	const YootPluginView * yootPluginView(Application & app, const std::string & plugin,
		const sf::Color & mask = sf::Color::Transparent, int bitmapId = 1000,
		int objmapId = 1000, const std::string & part = "View");
}
