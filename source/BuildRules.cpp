/* Copyright (c) 2026 OpenSkyscraper Project */
#include "BuildRules.h"

#include <climits>

namespace {

const int kSimTowerMinX = -187;
const int kSimTowerMaxX = 188;

bool hasRepresentableEnd(int x, int width)
{
	return width > 0 && x <= INT_MAX - width;
}

} // namespace

namespace OT {

BuildRules::BuildRules(Ruleset ruleset) :
	m_ruleset(ruleset)
{
}

BuildRules::Ruleset BuildRules::ruleset() const
{
	return m_ruleset;
}

const char * BuildRules::identifier() const
{
	switch (m_ruleset)
	{
	case Ruleset::SimTower: return "simtower";
	case Ruleset::OpenSky:  return "opensky";
	case Ruleset::YootTower:return "yoottower";
	}
	return "";
}

const char * BuildRules::displayName() const
{
	switch (m_ruleset)
	{
	case Ruleset::SimTower: return "SimTower";
	case Ruleset::OpenSky:  return "OpenSky";
	case Ruleset::YootTower:return "YootTower";
	}
	return "";
}

bool BuildRules::hasFixedHorizontalBoundary() const
{
	return m_ruleset == Ruleset::SimTower;
}

bool BuildRules::allowsHorizontalFootprint(int x, int width) const
{
	if (!hasRepresentableEnd(x, width)) return false;

	if (m_ruleset != Ruleset::SimTower)
		return true;

	const int end = x + width;
	return x >= kSimTowerMinX && end <= kSimTowerMaxX;
}

bool BuildRules::parseIdentifier(const std::string & identifier, Ruleset & ruleset)
{
	if (identifier == "simtower")
	{
		ruleset = Ruleset::SimTower;
		return true;
	}
	if (identifier == "opensky")
	{
		ruleset = Ruleset::OpenSky;
		return true;
	}
	if (identifier == "yoottower")
	{
		ruleset = Ruleset::YootTower;
		return true;
	}
	return false;
}

} // namespace OT
