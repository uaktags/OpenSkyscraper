/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower skin art taken straight from the World and Floor plugins:
///
///  - backdrop (sky, skyline, ground): world RT_BITMAP 1000 laid out by the
///    BINF tile map (Tokyo, Hawaii) or drawn as one picture stretched over
///    the world (Kegon, seasonal 1000/201..204), with the palette-animated
///    sky range refilled from PALETTE 101/102 for the time of day;
///  - floor/roof slab strips (world 1004 `Floor`/`Roof`), fire-escape sides
///    (1002 `SideL`/`SideR`), lobby awnings (1003 `TentL`/`TentR`) and the
///    rooftop crane (3000 `OutCrane`);
///  - empty-floor and lobby interiors from Floor.t2p (KFloor.t2p in Kegon)
///    RT_BITMAP 1000, OBJMAP `View` patterns 100 / 1100 / 1200 / 1300.
///
/// Every piece is optional: callers check the has*() accessors and keep
/// the OpenSky art for anything a plugin lacks.
#pragma once

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <string>
#include <vector>

#include "PluginObjmap.h"
#include "YootBackdrop.h"
#include "YootWorld.h"

namespace OT
{
	class AbstractPlugin;
	class PluginManager;

	class YootWorldArt
	{
	public:
		/// Plugin that holds the lobby/floor views for a world.
		static const char * floorPluginFor(YootWorld::Id id);

		/// Loads everything available for `id`. Returns true when at least
		/// the backdrop loaded.
		bool load(const PluginManager & plugins, YootWorld::Id id);
		YootWorld::Id worldId() const { return m_id; }
		/// WORLDDEF outViewKind (+0x218): the Outside view facades are
		/// bitmap / OBJMAP 1001 + kind of the floor and tenant plugins.
		int outViewKind() const { return m_outViewKind; }
		/// The Outside view: Kegon swaps its backdrop to 210 + season
		/// (WorldDef::CalcSkyPictID 0x10014e20), the inn facade.
		void setOutside(bool outside) { m_outside = outside; }
		bool loaded() const { return m_loaded; }

		/// Re-fills the animated palette (and Kegon's seasonal picture) for
		/// the given moment; re-uploads the backdrop only when it changed.
		void update(const YootSkyInputs & in);

		bool hasBackdrop() const { return m_backdropReady; }
		/// Draws the backdrop over the whole view (cells outside the world
		/// repeat the edge tiles).
		void drawBackdrop(sf::RenderTarget & target) const;

		// --- Tower pieces (OpenSky pixel space; y grows downwards) ---

		bool hasSlab() const { return m_slabReady; }
		/// YootTower rows are 36 px: an 8 px `Roof` strip at the top, 24 px of
		/// interior, then a 4 px `Floor` strip (UT2Coordinate::UnitToQDRoof /
		/// UnitToQDFloor). `top` is the row's top edge, x0/x1 in pixels.
		void drawRoof(sf::RenderTarget & target, float x0, float x1, float top, sf::Color tint) const;
		/// The 4 px `Floor` strip with its top edge at `y`.
		void drawFloorStrip(sf::RenderTarget & target, float x0, float x1, float y, sf::Color tint) const;

		bool hasFloorView() const { return m_floorViewReady; }
		/// Empty-floor interior (24 px) between x0 and x1 starting at y `top`.
		/// `floorStart`: the span begins at the floor's left edge, where the
		/// fixed `View` 100 part goes (Floor.t2p DrawProc 0x1000bc60).
		void drawFloorView(sf::RenderTarget & target, float x0, float x1, float top, sf::Color tint, bool floorStart = false) const;

		/// True when Floor.t2p has the lobby view for `height` rows (1..3).
		bool hasLobbyView(int height) const;
		/// Lobby interior: the fixed left part (cut so the loop starts on a
		/// unit u % 8 == 7) then the 256 px loop, phase-locked to unit 0.
		/// `lobbyStart` false: a run after a tenant, the loop only.
		void drawLobbyView(sf::RenderTarget & target, int height, float x0, float x1, float top, sf::Color tint, bool lobbyStart = true) const;

		bool hasSides() const { return m_sidesReady; }
		/// Kegon overrides T2WorldDef::DrawOptionObj (0x100725e0) and
		/// DrawCrane with empty functions: no fire escapes, awnings or crane
		/// (its wooden lobby post is painted into the backdrop).
		bool hidesOptionObjects() const { return m_loaded && m_id == YootWorld::Id::Kegon; }
		const sf::Texture & sidesTexture() const { return m_sides; }
		sf::IntRect sideRect(bool right) const { return right ? m_sideR : m_sideL; }

		bool hasTents() const { return m_tentsReady; }
		/// Lobby awnings: TentL ends at x `left`, TentR starts at `right`;
		/// both sit on the row whose bottom edge is `bottom`.
		void drawTents(sf::RenderTarget & target, float left, float right, float bottom, sf::Color tint) const;

		bool hasCrane() const { return m_craneReady; }
		const sf::Texture & craneTexture() const { return m_crane; }
		sf::IntRect craneRect() const { return m_craneRect; }

		/// The tower palette of the moment (base colours plus the animated
		/// sky/refraction/SE range) and that range, for the plugin sheets.
		const std::vector<YootRgb> & palette() const { return m_lastPalette.empty() ? m_basePalette : m_lastPalette; }
		int animStart() const { return m_ptdf.animStartIndex; }
		int animCount() const { return m_ptdf.numAnimColor; }

		const YootBackdropGeometry & geometry() const { return m_geom; }

	private:
		void drawViewRun(sf::RenderTarget & target, const sf::IntRect * fixed, const sf::IntRect & loop,
		                 float x0, float x1, float top, sf::Color tint) const;
		bool loadBackdropPicture(int bitmapId);
		bool loadDaylightPalette(int paletteId);
		void recolour();

		YootWorld::Id m_id = YootWorld::Id::Tokyo;
		bool m_loaded = false;
		const AbstractPlugin * m_world = nullptr;

		// Backdrop
		YootBackdropGeometry m_geom;
		bool m_tiled = false;
		YootBinf m_binf;
		int m_tileW = 0, m_tileH = 0, m_tileCount = 0, m_atlasCols = 1;
		YootIndexedImage m_backdrop;      ///< tiled: repacked atlas; picture: as decoded
		std::vector<YootRgb> m_basePalette;
		YootPaletteAnimeDef m_ptdf;
		std::vector<YootRgb> m_daylight, m_se;
		YootSkyModel m_model = YootSkyModel::DayLightFilter;
		int m_pictureId = 1000, m_daylightId = 101;
		int m_outViewKind = 0;
		bool m_outside = false;
		std::vector<YootRgb> m_lastPalette;
		std::vector<uint8_t> m_rgba;
		sf::Texture m_backdropTex;
		bool m_backdropReady = false;

		// Tower pieces
		sf::Texture m_slabTex;
		sf::IntRect m_roofRect, m_floorRect;
		bool m_slabReady = false;
		sf::Texture m_floorTex;
		std::vector<ObjmapBlock> m_floorMap;
		bool m_floorViewReady = false;
		sf::Texture m_sides;
		sf::IntRect m_sideL, m_sideR;
		bool m_sidesReady = false;
		sf::Texture m_tents;
		sf::IntRect m_tentL, m_tentR;
		bool m_tentsReady = false;
		sf::Texture m_crane;
		sf::IntRect m_craneRect;
		bool m_craneReady = false;
	};
}
