/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower world backdrop maths: the BINF tile map, the PTDF/PALETTE
/// palette-animation resources, the time-of-day sky (T2DayLightFilter) and
/// Tokyo's own sky-state machine. See doc/yoottower/world-and-sp-formats.md
/// §2.4-2.7 and §5.
///
/// Pure module (no SFML, no Game): linked into OpenSkyTests. The renderer
/// that turns these into textures lives in YootWorldArt.
#pragma once

#include <cstdint>
#include <vector>

namespace OT
{
	struct YootRgb
	{
		uint8_t r = 0, g = 0, b = 0;
		YootRgb() {}
		YootRgb(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
		bool operator==(const YootRgb & o) const { return r == o.r && g == o.g && b == o.b; }
		bool operator!=(const YootRgb & o) const { return !(*this == o); }
	};

	/// An 8-bit DIB decoded to palette indices (top-down rows) plus its
	/// colour table. YootTower bitmaps are uncompressed BI_RGB 8bpp.
	struct YootIndexedImage
	{
		int width = 0, height = 0;
		std::vector<uint8_t> pixels;   ///< width*height indices, row 0 = top
		std::vector<YootRgb> palette;  ///< 256 entries (padded with black)

		uint8_t at(int x, int y) const { return pixels[static_cast<std::size_t>(y) * width + x]; }
	};
	/// Decodes a raw RT_BITMAP (BITMAPINFOHEADER + colour table + bits, no
	/// file header). Only 8bpp BI_RGB is accepted.
	bool decodeYootIndexedDib(const char * data, int length, YootIndexedImage & out);

	// --- BINF: background tile map (T2WorldPlugin::InitBkgndInfoProc 0x10073150) ---

	/// `rows`/`cols` count background cells of 8 units x 2 rows each. Cells
	/// are stored COLUMN-MAJOR; `value & 0xffff` is a 'Sky' tile of bitmap 1000.
	struct YootBinf
	{
		int rows = 0, cols = 0, unused = 0;
		std::vector<uint32_t> cells;   ///< column-major: index = col*rows + row

		/// Tile pattern for a cell. Cells outside the map clamp to the edge,
		/// so the backdrop extends past the world's sides.
		int tile(int row, int col) const;
	};
	bool parseYootBinf(const char * data, int length, YootBinf & out);

	/// World placement of the backdrop in OpenSky pixel space (x = cell*8,
	/// SFML y grows downwards, floor f's bottom edge at y = -f*36). A world
	/// unit u sits at OpenSky x = u - unitOrigin (origin = width/2) and row r
	/// is floor groundLine-1-r, so row r's top edge is y = (r - groundLine)*36.
	struct YootBackdropGeometry
	{
		int width = 0, height = 0;   ///< world units / rows (WORLDDEF)
		int groundLine = 0;
		int unitOrigin = 0;          ///< width / 2
		int cellUnits = 8;           ///< units per backdrop cell
		int cellRows = 2;            ///< rows per backdrop cell

		static const int kUnitPixels = 8;
		static const int kRowPixels = 36;

		struct Rect { int x = 0, y = 0, w = 0, h = 0; };
		/// Pixel rect of BINF cell (row, col).
		Rect cellRect(int row, int col) const;
		/// BINF cell containing a pixel (floor division, may be outside the map).
		int cellRowAt(double pixelY) const;
		int cellColAt(double pixelX) const;
		/// Pixel rect of the whole world (single-picture backdrops, Kegon).
		Rect worldRect() const;
	};

	// --- PTDF / PALETTE (T2PaletteAnimeDef ctor 0x10047e60) ---

	struct YootPaletteAnimeDef
	{
		int numTowerColor = 0;
		int numAnimColor = 0;
		int animStartIndex = 0;
		int numRefractColor = 0;
		int numSkyColor = 0;
		int numSEKind = 0;
		int numSEColor = 0;
		int seInterval = 0;
	};
	/// Text `(name)<tab>value` lines, read in declaration order.
	bool parseYootPtdf(const char * data, int length, YootPaletteAnimeDef & out);
	/// PALETTE resource (LoadACTPaletteEntries 0x100120e0): raw ACT, Mac
	/// `clut` (entries placed by index, ctSize of them) or Mac `pltt`
	/// (entries in order). Colours use the high byte of each 16-bit channel.
	bool parseYootPalette(const char * data, int length, std::vector<YootRgb> & out);

	// --- T2DayLightFilter (T2DLL 0x1001cd00..0x1001d230, ported from x87) ---

	/// SetTime(day, t): `t` is the time of day in 1/1000 day steps
	/// (T2PaletteAnime::AdjustSkyState passes minute*1000/1440, day 0). The
	/// result is the sun angle in degrees, clamped to +-90 and wrapped into
	/// 0..359 (sunrise 0, noon 90, midnight 270).
	int yootDayLightAngle(int day, int timeOf1000);
	/// Convenience: angle for a minute of the day (0..1439).
	int yootDayLightAngleForMinute(int minuteOfDay);
	/// CalcDayLightColor: the filter colour for a sun angle.
	YootRgb yootDayLightFilter(int angle);
	/// SkyColor: filter applied to a base sky colour (sky entries).
	YootRgb yootSkyColor(const YootRgb & filter, const YootRgb & base);
	/// RefractionColor: base * filter / 255 (refraction entries).
	YootRgb yootRefractionColor(const YootRgb & filter, const YootRgb & base);

	// --- Tokyo.t2p sky (AdjustSkyStateProc 0x10014e90, ChangeSkyColorProc 0x10015070) ---

	/// Tokyo's sky state: 1 day, 2 night, 3 rain, 4 snow night (night colours),
	/// 5..10 transitions (day>dusk, dusk>night, night>dusk, dusk>day,
	/// day>rain, rain>day). `count` is the remaining minutes of a transition.
	struct YootSkyState
	{
		int state = 1;
		int count = 0;
	};
	/// `month` is the 0-based month (T2DateTime+8), `minuteOfDay` 0..1439.
	/// Sunrise/sunset hours by quarter: Jan-Mar/Jul-Sep 5/17, Apr-Jun 4/18,
	/// Oct-Dec 6/16. Rainy days (8:00-16:00) blend into the rain block.
	YootSkyState yootTokyoSkyState(int month, int minuteOfDay, bool rainy);
	/// The 28 animated sky entries for a state: entries 0..7 take the state's
	/// 8-colour block of PALETTE 101 (day 8, night 24, rain 32; transitions
	/// interpolate with the original's (from-to)/60*count+to byte maths) and
	/// entries 8..27 repeat its first 4 colours.
	std::vector<YootRgb> yootTokyoSkyColors(const std::vector<YootRgb> & daylight, const YootSkyState & s);

	// --- The animated palette range for one moment ---

	enum class YootSkyModel
	{
		DayLightFilter,   ///< base T2PaletteAnime (Hawaii, Kegon)
		Tokyo             ///< Tokyo.t2p's own sky blocks
	};

	struct YootSkyInputs
	{
		int month = 0;          ///< 0-based
		int minuteOfDay = 720;  ///< 0..1439 (quantised to 6 minutes like MinuteChanged)
		bool rainy = false;
		int seKind = 0;         ///< SE (sign/light) cycle step
	};

	/// Fills `palette` (256 entries, normally the bitmap's colour table)
	/// from mAnimStartIndex on: sky entries, refraction entries, then SE kind
	/// `seKind` of PALETTE 102. Indices beyond 255 are dropped.
	void yootApplyAnimatedPalette(YootSkyModel model, const YootPaletteAnimeDef & def,
	                              const std::vector<YootRgb> & daylight,
	                              const std::vector<YootRgb> & se,
	                              const YootSkyInputs & in,
	                              std::vector<YootRgb> & palette);

	/// Kegon.t2p WorldDef::CalcSeasonIndex 0x10014dc0 (1-based month):
	/// 0 spring (3-5), 1 summer (6-8), 2 autumn (9-10), 3 November, 4 winter.
	int yootKegonSeasonIndex(int month1);
	/// CalcSkyPictID: backdrop bitmap for a season (spring = 1000, then 201..204).
	int yootKegonSkyPictId(int month1);
	/// CalcDayLightPaletteID: daylight PALETTE id (150..154).
	int yootKegonDayLightPaletteId(int month1);
}
