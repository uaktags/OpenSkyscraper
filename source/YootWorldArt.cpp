/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootWorldArt.h"

#include "AbstractPlugin.h"
#include "Application.h"
#include "PluginManager.h"
#include "WindowsPEExecutable.h"

#include <SFML/Graphics/Image.hpp>

#include <algorithm>
#include <cmath>

using namespace OT;

namespace
{
	const WindowsPEExecutable::Resource * resource(const AbstractPlugin * plugin, const char * type, int id)
	{
		if (!plugin) return nullptr;
		const WindowsPEExecutable::Resource * r = plugin->getResource(type, id);
		return (r && r->data && r->length > 0) ? r : nullptr;
	}

	bool loadIndexed(const AbstractPlugin * plugin, int id, YootIndexedImage & out)
	{
		const WindowsPEExecutable::Resource * r = resource(plugin, "2", id);
		return r && decodeYootIndexedDib(r->data, r->length, out);
	}

	bool loadObjmap(const AbstractPlugin * plugin, int id, std::vector<ObjmapBlock> & out)
	{
		const WindowsPEExecutable::Resource * r = resource(plugin, "OBJMAP", id);
		return r && parsePluginObjmap(r->data, r->length, out);
	}

	/// Palette-indexed image to a texture. YootTower keys transparent
	/// parts (OBJMAP `TRANSPARENT`) on palette index 0.
	bool toTexture(const YootIndexedImage & img, bool transparentZero, sf::Texture & dst)
	{
		if (img.width <= 0 || img.height <= 0) return false;
		std::vector<uint8_t> rgba(std::size_t(img.width) * img.height * 4);
		for (std::size_t i = 0; i < img.pixels.size(); ++i)
		{
			const uint8_t index = img.pixels[i];
			const YootRgb & c = img.palette[index];
			rgba[i * 4 + 0] = c.r;
			rgba[i * 4 + 1] = c.g;
			rgba[i * 4 + 2] = c.b;
			rgba[i * 4 + 3] = (transparentZero && index == 0) ? 0 : 255;
		}
		sf::Image image({unsigned(img.width), unsigned(img.height)}, rgba.data());
		return dst.loadFromImage(image);
	}

	sf::IntRect toRect(const ObjmapRect & r)
	{
		return sf::IntRect({r.left, r.top}, {r.width(), r.height()});
	}

	bool findRect(const std::vector<ObjmapBlock> & map, const char * name, int pattern, sf::IntRect & out)
	{
		const ObjmapBlock * b = findObjmapBlock(map, name, pattern);
		if (!b || b->rect.width() <= 0 || b->rect.height() <= 0) return false;
		out = toRect(b->rect);
		return true;
	}

	void pushQuad(sf::VertexArray & va, float x, float y, float w, float h,
	              float u, float v, float uw, float vh, sf::Color color)
	{
		const sf::Vector2f p0(x, y), p1(x + w, y), p2(x + w, y + h), p3(x, y + h);
		const sf::Vector2f t0(u, v), t1(u + uw, v), t2(u + uw, v + vh), t3(u, v + vh);
		va.append(sf::Vertex{p0, color, t0});
		va.append(sf::Vertex{p1, color, t1});
		va.append(sf::Vertex{p2, color, t2});
		va.append(sf::Vertex{p0, color, t0});
		va.append(sf::Vertex{p2, color, t2});
		va.append(sf::Vertex{p3, color, t3});
	}

	/// Tiles `src` horizontally over [x0, x1), phase-locked to world x = 0
	/// so neighbouring floors line up.
	void pushLoop(sf::VertexArray & va, const sf::IntRect & src, float x0, float x1, float y, sf::Color color)
	{
		const float w = float(src.size.x);
		if (w <= 0 || x1 <= x0) return;
		float x = x0;
		while (x < x1)
		{
			float phase = std::fmod(x, w);
			if (phase < 0) phase += w;
			const float run = std::min(w - phase, x1 - x);
			pushQuad(va, x, y, run, float(src.size.y),
			         float(src.position.x) + phase, float(src.position.y), run, float(src.size.y), color);
			x += run;
		}
	}
}

const char * YootWorldArt::floorPluginFor(YootWorld::Id id)
{
	return id == YootWorld::Id::Kegon ? "KFloor.t2p" : "Floor.t2p";
}

bool YootWorldArt::load(const PluginManager & plugins, YootWorld::Id id)
{
	*this = YootWorldArt();
	m_id = id;
	m_world = plugins.get(yootWorldPluginFile(id));
	if (!m_world)
	{
		LOG(WARNING, "YootTower skin: %s not cataloged; keeping OpenSky sky", yootWorldPluginFile(id));
		return false;
	}

	// Geometry from WORLDDEF (T2WorldDef ctor 0x1006e4e0).
	YootWorld world;
	std::string error;
	if (!loadYootWorld(*m_world, id, world, &error))
	{
		LOG(WARNING, "YootTower skin: %s WORLDDEF unreadable (%s)", yootWorldPluginFile(id), error.c_str());
		return false;
	}
	m_geom.width = world.width;
	m_geom.height = world.height;
	m_geom.groundLine = world.groundLine;
	m_geom.unitOrigin = world.width / 2;
	m_tiled = world.tiledBkgnd != 0;
	m_outViewKind = world.outViewKind;
	m_tileW = world.bkgndTileWidth;
	m_tileH = world.bkgndTileHeight;
	m_model = (id == YootWorld::Id::Tokyo) ? YootSkyModel::Tokyo : YootSkyModel::DayLightFilter;

	if (const WindowsPEExecutable::Resource * r = resource(m_world, "PTDF", 1))
		parseYootPtdf(r->data, r->length, m_ptdf);
	if (const WindowsPEExecutable::Resource * r = resource(m_world, "PALETTE", 102))
		parseYootPalette(r->data, r->length, m_se);
	if (m_tiled)
	{
		const WindowsPEExecutable::Resource * r = resource(m_world, "BINF", 1);
		if (!r || !parseYootBinf(r->data, r->length, m_binf) || m_tileW <= 0 || m_tileH <= 0)
			m_tiled = false;
	}
	// Kegon starts on the spring picture; update() swaps seasons.
	m_backdropReady = loadBackdropPicture(1000) && loadDaylightPalette(id == YootWorld::Id::Kegon ? 150 : 101);
	if (!m_backdropReady)
		LOG(WARNING, "YootTower skin: %s backdrop (bitmap 1000) missing", yootWorldPluginFile(id));

	// Slabs: 1004 Roof / Floor.
	{
		YootIndexedImage img;
		std::vector<ObjmapBlock> map;
		if (loadIndexed(m_world, 1004, img) && loadObjmap(m_world, 1004, map) &&
		    findRect(map, "Roof", -1, m_roofRect) && findRect(map, "Floor", -1, m_floorRect))
			m_slabReady = toTexture(img, false, m_slabTex);
	}
	// Fire-escape sides: 1002 SideL / SideR pattern 0.
	if (id != YootWorld::Id::Kegon)
	{
		YootIndexedImage img;
		std::vector<ObjmapBlock> map;
		if (loadIndexed(m_world, 1002, img) && loadObjmap(m_world, 1002, map) &&
		    findRect(map, "SideL", 0, m_sideL) && findRect(map, "SideR", 0, m_sideR))
			m_sidesReady = toTexture(img, true, m_sides);
	}
	// Lobby awnings: 1003 TentL / TentR pattern 0.
	if (id != YootWorld::Id::Kegon)
	{
		YootIndexedImage img;
		std::vector<ObjmapBlock> map;
		if (loadIndexed(m_world, 1003, img) && loadObjmap(m_world, 1003, map) &&
		    findRect(map, "TentL", 0, m_tentL) && findRect(map, "TentR", 0, m_tentR))
			m_tentsReady = toTexture(img, true, m_tents);
	}
	// Rooftop crane: 3000 OutCrane.
	if (id != YootWorld::Id::Kegon)
	{
		YootIndexedImage img;
		std::vector<ObjmapBlock> map;
		if (loadIndexed(m_world, 3000, img) && loadObjmap(m_world, 3000, map) &&
		    findRect(map, "OutCrane", -1, m_craneRect))
			m_craneReady = toTexture(img, true, m_crane);
	}
	// Floor / lobby views from Floor.t2p (KFloor.t2p).
	{
		const AbstractPlugin * floor = plugins.get(floorPluginFor(id));
		YootIndexedImage img;
		sf::IntRect left, loop;
		if (floor && loadIndexed(floor, 1000, img) && loadObjmap(floor, 1000, m_floorMap) &&
		    findRect(m_floorMap, "View", 100, left) && findRect(m_floorMap, "View", 100100, loop))
			m_floorViewReady = toTexture(img, false, m_floorTex);
		else
			LOG(WARNING, "YootTower skin: %s lobby/floor views missing; keeping OpenSky floors", floorPluginFor(id));
	}

	m_loaded = m_backdropReady;
	LOG(INFO, "YootTower skin: %s art (backdrop %d, slab %d, sides %d, tents %d, crane %d, floor views %d)",
	    yootWorldDisplayName(id), m_backdropReady, m_slabReady, m_sidesReady, m_tentsReady, m_craneReady, m_floorViewReady);
	return m_loaded;
}

bool YootWorldArt::loadBackdropPicture(int bitmapId)
{
	YootIndexedImage sheet;
	if (!loadIndexed(m_world, bitmapId, sheet)) return false;
	m_pictureId = bitmapId;
	m_basePalette = sheet.palette;
	if (!m_tiled)
	{
		m_backdrop = sheet;
	}
	else
	{
		// The 'Sky' tiles are stacked vertically (OBJMAP Rect 0,k*tileH,..);
		// repack them into a wide atlas so no texture side gets huge
		// (Hawaii's sheet is 13896 px tall).
		if (sheet.width < m_tileW) return false;
		m_tileCount = sheet.height / m_tileH;
		if (m_tileCount <= 0) return false;
		m_atlasCols = std::max(1, std::min(m_tileCount, 2048 / m_tileW));
		const int atlasRows = (m_tileCount + m_atlasCols - 1) / m_atlasCols;
		m_backdrop.width = m_atlasCols * m_tileW;
		m_backdrop.height = atlasRows * m_tileH;
		m_backdrop.palette = sheet.palette;
		m_backdrop.pixels.assign(std::size_t(m_backdrop.width) * m_backdrop.height, 0);
		for (int t = 0; t < m_tileCount; ++t)
		{
			const int ax = (t % m_atlasCols) * m_tileW, ay = (t / m_atlasCols) * m_tileH;
			for (int y = 0; y < m_tileH; ++y)
				std::copy_n(&sheet.pixels[std::size_t(t * m_tileH + y) * sheet.width], m_tileW,
				            &m_backdrop.pixels[std::size_t(ay + y) * m_backdrop.width + ax]);
		}
	}
	m_rgba.assign(std::size_t(m_backdrop.width) * m_backdrop.height * 4, 255);
	if (!m_backdropTex.resize({unsigned(m_backdrop.width), unsigned(m_backdrop.height)}))
		return false;
	m_lastPalette.clear();
	recolour();
	return true;
}

bool YootWorldArt::loadDaylightPalette(int paletteId)
{
	const WindowsPEExecutable::Resource * r = resource(m_world, "PALETTE", paletteId);
	if (!r && paletteId != 101)
		r = resource(m_world, "PALETTE", 101);
	if (!r || !parseYootPalette(r->data, r->length, m_daylight)) return false;
	m_daylightId = paletteId;
	return true;
}

void YootWorldArt::update(const YootSkyInputs & in)
{
	if (!m_backdropReady) return;
	if (m_id == YootWorld::Id::Kegon)
	{
		// WorldDef::DayChanged 0x10015110: seasonal picture and daylight palette.
		const int month1 = in.month % 12 + 1;
		const int pict = m_outside ? 210 + yootKegonSeasonIndex(month1) : yootKegonSkyPictId(month1);
		if (pict != m_pictureId && !loadBackdropPicture(pict))
			loadBackdropPicture(1000);
		const int pal = yootKegonDayLightPaletteId(month1);
		if (pal != m_daylightId)
			loadDaylightPalette(pal);
	}
	std::vector<YootRgb> palette = m_basePalette;
	yootApplyAnimatedPalette(m_model, m_ptdf, m_daylight, m_se, in, palette);
	if (palette != m_lastPalette)
	{
		m_lastPalette = palette;
		recolour();
	}
}

void YootWorldArt::recolour()
{
	const std::vector<YootRgb> & pal = m_lastPalette.empty() ? m_basePalette : m_lastPalette;
	const std::size_t n = m_backdrop.pixels.size();
	for (std::size_t i = 0; i < n; ++i)
	{
		const YootRgb & c = pal[m_backdrop.pixels[i]];
		m_rgba[i * 4 + 0] = c.r;
		m_rgba[i * 4 + 1] = c.g;
		m_rgba[i * 4 + 2] = c.b;
	}
	m_backdropTex.update(m_rgba.data());
}

void YootWorldArt::drawBackdrop(sf::RenderTarget & target) const
{
	if (!m_backdropReady) return;
	const sf::View & view = target.getView();
	const sf::Vector2f vmin = view.getCenter() - view.getSize() / 2.f;
	const sf::Vector2f vmax = view.getCenter() + view.getSize() / 2.f;
	sf::VertexArray va(sf::PrimitiveType::Triangles);

	if (!m_tiled)
	{
		// One picture over the world rect. Texture coordinates run past the
		// edges and the (non-repeated) texture clamps to its border pixels,
		// so the sky and the rock extend beyond the world.
		const YootBackdropGeometry::Rect wr = m_geom.worldRect();
		const float sx = float(m_backdrop.width) / float(wr.w);
		const float sy = float(m_backdrop.height) / float(wr.h);
		pushQuad(va, vmin.x, vmin.y, vmax.x - vmin.x, vmax.y - vmin.y,
		         (vmin.x - wr.x) * sx, (vmin.y - wr.y) * sy,
		         (vmax.x - vmin.x) * sx, (vmax.y - vmin.y) * sy, sf::Color::White);
	}
	else
	{
		const int r0 = m_geom.cellRowAt(vmin.y), r1 = m_geom.cellRowAt(vmax.y);
		const int c0 = m_geom.cellColAt(vmin.x), c1 = m_geom.cellColAt(vmax.x);
		for (int row = r0; row <= r1; ++row)
		{
			for (int col = c0; col <= c1; ++col)
			{
				int t = m_binf.tile(row, col);
				// Tokyo.t2p ViewModeChanged 0x100151c0: the Outside view adds 6 to
				// every tile id >= 100, the street under the tower (106..111).
				if (m_outside && m_id == YootWorld::Id::Tokyo && t >= 100) t += 6;
				t = std::min(t, m_tileCount - 1);
				const YootBackdropGeometry::Rect r = m_geom.cellRect(row, col);
				// Half-scale art (Hawaii 32x36 tiles) is stretched to the
				// 64x72 cell (UT2Coordinate::BkgndToQD).
				pushQuad(va, float(r.x), float(r.y), float(r.w), float(r.h),
				         float((t % m_atlasCols) * m_tileW), float((t / m_atlasCols) * m_tileH),
				         float(m_tileW), float(m_tileH), sf::Color::White);
			}
		}
	}
	sf::RenderStates states;
	states.texture = &m_backdropTex;
	target.draw(va, states);
}

void YootWorldArt::drawRoof(sf::RenderTarget & target, float x0, float x1, float top, sf::Color tint) const
{
	if (!m_slabReady) return;
	sf::VertexArray va(sf::PrimitiveType::Triangles);
	pushLoop(va, m_roofRect, x0, x1, top, tint);
	sf::RenderStates states;
	states.texture = &m_slabTex;
	target.draw(va, states);
}

void YootWorldArt::drawFloorStrip(sf::RenderTarget & target, float x0, float x1, float y, sf::Color tint) const
{
	if (!m_slabReady) return;
	sf::VertexArray va(sf::PrimitiveType::Triangles);
	pushLoop(va, m_floorRect, x0, x1, y, tint);
	sf::RenderStates states;
	states.texture = &m_slabTex;
	target.draw(va, states);
}

void YootWorldArt::drawViewRun(sf::RenderTarget & target, const sf::IntRect * fixed, const sf::IntRect & loop,
                               float x0, float x1, float top, sf::Color tint) const
{
	sf::VertexArray va(sf::PrimitiveType::Triangles);
	float x = x0;
	if (fixed)
	{
		const int leftUnit = (int)std::lround(x0 / 8.0f) + m_geom.unitOrigin;
		const float w = std::min(float(yootFloorFixedUnits(fixed->size.x / 8, leftUnit) * 8), x1 - x0);
		pushQuad(va, x0, top, w, float(fixed->size.y),
		         float(fixed->position.x), float(fixed->position.y), w, float(fixed->size.y), tint);
		x += w;
	}
	// The loop is phase-locked to world unit 0 (its source column is
	// unit % loop width), so it lines up across floors and runs.
	const float shift = float(m_geom.unitOrigin * 8);
	sf::VertexArray shifted(sf::PrimitiveType::Triangles);
	pushLoop(shifted, loop, x + shift, x1 + shift, top, tint);
	for (std::size_t i = 0; i < shifted.getVertexCount(); ++i)
	{
		sf::Vertex v = shifted[i];
		v.position.x -= shift;
		va.append(v);
	}
	sf::RenderStates states;
	states.texture = &m_floorTex;
	target.draw(va, states);
}

void YootWorldArt::drawFloorView(sf::RenderTarget & target, float x0, float x1, float top, sf::Color tint, bool floorStart) const
{
	sf::IntRect left, loop;
	if (!m_floorViewReady || !findRect(m_floorMap, "View", 100100, loop)) return;
	const bool fixed = floorStart && findRect(m_floorMap, "View", 100, left);
	drawViewRun(target, fixed ? &left : nullptr, loop, x0, x1, top, tint);
}

bool YootWorldArt::hasLobbyView(int height) const
{
	sf::IntRect left, loop;
	return m_floorViewReady && height >= 1 && height <= 3 &&
	       findRect(m_floorMap, "View", 1000 + height * 100, left) &&
	       findRect(m_floorMap, "View", 101000 + height * 100, loop);
}

void YootWorldArt::drawLobbyView(sf::RenderTarget & target, int height, float x0, float x1, float top, sf::Color tint) const
{
	sf::IntRect left, loop;
	if (!m_floorViewReady || !findRect(m_floorMap, "View", 1000 + height * 100, left) ||
	    !findRect(m_floorMap, "View", 101000 + height * 100, loop))
		return;
	drawViewRun(target, &left, loop, x0, x1, top, tint);
}

void YootWorldArt::drawTents(sf::RenderTarget & target, float left, float right, float bottom, sf::Color tint) const
{
	if (!m_tentsReady) return;
	sf::VertexArray va(sf::PrimitiveType::Triangles);
	pushQuad(va, left - m_tentL.size.x, bottom - m_tentL.size.y, float(m_tentL.size.x), float(m_tentL.size.y),
	         float(m_tentL.position.x), float(m_tentL.position.y), float(m_tentL.size.x), float(m_tentL.size.y), tint);
	pushQuad(va, right, bottom - m_tentR.size.y, float(m_tentR.size.x), float(m_tentR.size.y),
	         float(m_tentR.position.x), float(m_tentR.position.y), float(m_tentR.size.x), float(m_tentR.size.y), tint);
	sf::RenderStates states;
	states.texture = &m_tents;
	target.draw(va, states);
}
