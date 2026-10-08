/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "Skin.h"

namespace OT
{

bool parseSkinIdentifier(const std::string & identifier, Skin & skin)
{
	if (identifier == "classic")   { skin = Skin::Classic; return true; }
	if (identifier == "opensky")   { skin = Skin::OpenSky; return true; }
	if (identifier == "yoottower") { skin = Skin::Yoot;    return true; }
	return false;
}

const char * skinIdentifier(Skin skin)
{
	switch (skin) {
		case Skin::Classic: return "classic";
		case Skin::OpenSky: return "opensky";
		case Skin::Yoot:    return "yoottower";
	}
	return "opensky";
}

const char * skinDisplayName(Skin skin)
{
	switch (skin) {
		case Skin::Classic: return "Classic";
		case Skin::OpenSky: return "OpenSky";
		case Skin::Yoot:    return "YootTower";
	}
	return "OpenSky";
}

Skin defaultSkinForRuleset(BuildRules::Ruleset ruleset)
{
	switch (ruleset) {
		case BuildRules::Ruleset::SimTower:  return Skin::Classic;
		case BuildRules::Ruleset::YootTower: return Skin::Yoot;
		case BuildRules::Ruleset::OpenSky:   return Skin::OpenSky;
	}
	return Skin::OpenSky;
}

std::size_t sanctionedSkinCount(BuildRules::Ruleset ruleset)
{
	return ruleset == BuildRules::Ruleset::OpenSky ? 3 : 2;
}

Skin sanctionedSkin(BuildRules::Ruleset ruleset, std::size_t index)
{
	switch (ruleset) {
		case BuildRules::Ruleset::SimTower:
			return index == 0 ? Skin::Classic : Skin::OpenSky;
		case BuildRules::Ruleset::OpenSky:
			switch (index) {
				case 0:  return Skin::OpenSky;
				case 1:  return Skin::Yoot;
				default: return Skin::Classic;
			}
		case BuildRules::Ruleset::YootTower:
			return index == 0 ? Skin::Yoot : Skin::OpenSky;
	}
	return Skin::OpenSky;
}

bool isSkinSanctioned(BuildRules::Ruleset ruleset, Skin skin)
{
	const std::size_t count = sanctionedSkinCount(ruleset);
	for (std::size_t i = 0; i < count; ++i) {
		if (sanctionedSkin(ruleset, i) == skin) {
			return true;
		}
	}
	return false;
}

} // namespace OT
