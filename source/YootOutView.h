/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// The YootTower Control palette's "Outside" view under Skin::Yoot
/// (T2WorldDef::ClickOutViewButton 0x10071c00 sets view mode 1): the tower
/// is drawn as its exterior. Floors and lobbies tile Floor.t2p's `OutView`
/// loop (64 px, phase-locked to world unit 0) for their height; tenants
/// draw their plugin's `OutView` part over their whole unit rect; nothing
/// below the ground line, no people, roofs or floor slabs. The parts come
/// from bitmap / OBJMAP 1001 + WORLDDEF outViewKind. Kegon draws no tower:
/// its backdrop becomes the inn facade (YootWorldArt::setOutside).
#pragma once

namespace sf { class RenderTarget; }

namespace OT
{
	class Game;

	void drawYootOutView(Game & game, sf::RenderTarget & target);
}
