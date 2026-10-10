/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Plugin sprites for OpenSky items drawn under Skin::Yoot: a plugin's
/// RT_BITMAP 1000 sliced by its OBJMAP 1000 `View` rects (pattern order),
/// or any other OBJMAP id / part name. Pattern index = numOfPattern x kind +
/// pattern (T2Tenant::UpdatePatternIndex). Tenant sheets key their windows
/// with dark magenta (105, 33, 104) so the backdrop shows through. Parts
/// flagged `Transparent` (Stair, Escalator) key palette index 0.
///
/// Under Skin::Yoot the sheets follow the tower palette like in the
/// original: pixels in the world's animated range (PTDF mAnimStartIndex..)
/// are repainted with the current sky / refraction / SE colours, so the
/// windows (index 189) take the sky colour of the hour and signs blink.
#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "YootBackdrop.h"

namespace OT
{
	class Application;

	struct YootPluginView
	{
		sf::Texture texture;
		std::vector<sf::IntRect> bands;  ///< parts in pattern order
		std::vector<int> patterns;       ///< OBJMAP Pattern of each band
		std::vector<int> spans;          ///< OBJMAP Span of each band (-1 = none)
		/// Band for a pattern index, clamped; the whole sheet without bands.
		sf::IntRect band(int index) const;
		/// Band index of a multi-storey part (Stair, Escalator): `span`
		/// storeys and, when the part has several, `pattern`; -1 if none.
		int findSpan(int span, int pattern = -1) const;

		// Palette animation (empty when the sheet is not 8 bpp).
		std::vector<uint8_t> indices;    ///< palette index per pixel, row 0 = top
		std::vector<uint8_t> baseRgba;   ///< the sheet as first loaded (mask applied)
		std::vector<int> usedIndices;    ///< distinct indices of the sheet
		std::vector<YootRgb> applied;    ///< colours last painted at usedIndices
	};

	/// Window key of the tenant sheets (Office, Condo, hotel rooms).
	const sf::Color kYootWindowKey(105, 33, 104);

	/// Cached sheet `bitmapId` of `plugin` sliced by OBJMAP `objmapId` parts
	/// named `part`. Null when the plugin or bitmap is missing.
	const YootPluginView * yootPluginView(Application & app, const std::string & plugin,
		const sf::Color & mask = sf::Color::Transparent, int bitmapId = 1000,
		int objmapId = 1000, const std::string & part = "View");

	/// Repaints the animated indices of every cached sheet with `palette`
	/// (Game calls it when the world palette moves). An empty palette puts
	/// the sheets back as loaded (other skins).
	void yootRecolourPluginViews(const std::vector<YootRgb> & palette, int start, int count);

	/// Draws `band` of `view` with its top-left at (x, y), repeated to the
	/// right (OBJMAP `Loop` parts) and clipped to `width` pixels.
	void yootDrawLoop(sf::RenderTarget & target, const YootPluginView & view, const sf::IntRect & band,
		float x, float y, float width, sf::Color color = sf::Color::White);

	/// YootTower rows are 36 px: 8 px roof, 24 px interior, 4 px floor
	/// (UT2Coordinate::UnitToQD). Item sprites are bottom-aligned on the
	/// row's bottom edge, so their art sits this much higher.
	const int kYootInteriorLift = 4;
}
