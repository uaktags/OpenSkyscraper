/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "PluginDefs.h"

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace OT
{
	class Game;
	class Person;

	/** YootTower people art (Skin::Yoot).
	 *
	 * Walking people: the world plugin's PPDF maps a people type to its
	 * silhouette type, SLDF maps that to an RT_BITMAP (300..355): six 16x36
	 * frames stacked vertically (T2People::Draw 0x10049b70 draws the
	 * silhouette object of the person's style). Rows 0..2 face right
	 * (standing, two walking frames), rows 3..5 face left.
	 *
	 * People inside a tenant: the tenant plugin's OBJMAP 1100 `SHIL` parts
	 * (ID 1003) over RT_BITMAP 1100, 16x24 each; T2People::Draw(doc, equip)
	 * 0x10049de0 draws pattern people+0x68 at people+0x38, both set by the
	 * PADF (T2PeopleAnimeDefElem::Set 0x1004af20: pattern = base + rand %
	 * range, x = tenant left + offset + rand % range). */
	struct YootSilhouetteFrame
	{
		const sf::Texture * texture = nullptr;
		int width = 1;  ///< SLDF mWidth (units of 8 px)
	};
	/// The world silhouette of a person (a YootVisitor by its people type,
	/// other people by their OpenSky type); null outside Skin::Yoot.
	YootSilhouetteFrame yootPersonSilhouette(Game * game, const Person * person);
	/// Frame rect: row 0..5 of a 16x36 silhouette sheet.
	sf::IntRect yootSilhouetteRect(int row);

	struct YootTenantPeopleArt
	{
		bool loaded = false;
		sf::Texture texture;               ///< RT_BITMAP 1100 (white keyed)
		std::vector<sf::IntRect> shil;     ///< OBJMAP 1100 SHIL patterns, by pattern number
		bool hasAnime = false;
		PeopleAnimeDef anime;              ///< PADF id 1
	};
	const YootTenantPeopleArt & yootTenantPeopleArt(Game * game, const std::string & plugin);

	/// One T2PeopleAnimeDefElem::Set roll (rand drawn only for non-zero
	/// ranges): returns false when the element has no animation (-1).
	template <typename Rng>
	bool rollPeopleAnime(const PeopleAnimeDef & def, int index, int tenantLeft, Rng && rng, int & pattern, int & x)
	{
		const int n = (int)def.elems.size();
		if (index >= n) return false;
		if (index < 0) index = n == 0 ? 0 : rng() % n;
		const PeopleAnimeDef::Elem & e = def.elems[index];
		// Field names follow PluginDefs; the engine reads them as
		// {pattern base, pattern range, x offset, x range}.
		if (e.position < 0) return false;
		pattern = e.position + (e.pattern != 0 ? rng() % e.pattern : 0);
		x = e.silhouette + tenantLeft + (e.animeID != 0 ? rng() % e.animeID : 0);
		return true;
	}
}
