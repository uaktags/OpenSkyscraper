/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include <string>

#include "BuildRules.h"

namespace OT
{
	/// Independent art-set selector. The skin decides which provider's art
	/// wins for each texture key; missing art falls back per-key to whatever
	/// else is available. It is orthogonal to the ruleset: any ruleset can
	/// play with any skin, defaulting to the skin of the same era.
	enum class Skin
	{
		Classic,   ///< SIMTOWER.EXE extracted art (requires the EXE).
		OpenSky,   ///< First-party OpenTower plugin / data/bitmaps art.
		Yoot       ///< YootTower .t2p art (thin coverage today; falls back per-key).
	};

	/// Exact lowercase identifiers: "classic", "opensky", "yoottower".
	bool parseSkinIdentifier(const std::string & identifier, Skin & skin);
	const char * skinIdentifier(Skin skin);
	const char * skinDisplayName(Skin skin);
	/// Per-ruleset default when no explicit --skin was given.
	Skin defaultSkinForRuleset(BuildRules::Ruleset ruleset);

	/// The sanctioned mode×skin editions (review matrix). SimTower and
	/// YootTower each offer their era art plus first-party OpenSky art;
	/// OpenSky mode is unrestricted and can use all three.
	///   SimTower  → Classic, OpenSky
	///   OpenSky   → OpenSky, Yoot, Classic
	///   YootTower → Yoot, OpenSky
	std::size_t sanctionedSkinCount(BuildRules::Ruleset ruleset);
	Skin sanctionedSkin(BuildRules::Ruleset ruleset, std::size_t index);
	bool isSkinSanctioned(BuildRules::Ruleset ruleset, Skin skin);
}
