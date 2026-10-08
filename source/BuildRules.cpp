/* Copyright (c) 2026 OpenSkyscraper Project */
#include "BuildRules.h"

#include "LevelUp.h"
#include "YootWorld.h"

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

bool RulesProfile::allowsCell(int x, int floor) const
{
	if (floor < lowestFloor || floor > highestFloor) return false;
	if (!hasBuildArea) return true;
	if (x < buildMinX || x >= buildMaxX) return false;
	const int row = rowForFloor(floor);
	if (row < buildTopRow || row >= buildBottomRow) return false;
	if (!unbuildable.empty())
	{
		// T2FloorInfo::IsAreaBuildable: every unit must lack flag 0x8000.
		const int unit = unitForX(x);
		if (unit >= 0 && row >= 0 && unit < maskWidth && row < maskHeight &&
		    unbuildable[(size_t)row * maskWidth + unit] != 0)
			return false;
	}
	return true;
}

RulesProfile RulesProfile::simTower()
{
	return RulesProfile();
}

RulesProfile RulesProfile::openSky()
{
	return RulesProfile();
}

RulesProfile RulesProfile::yootTower(const YootWorld & world)
{
	RulesProfile p;
	p.yootWorld = true;
	p.worldId = yootWorldIdentifier(world.id);
	p.worldName = yootWorldDisplayName(world.id);
	p.worldMask = world.worldMask;

	const long long funds = world.initialFundsDollars();
	p.startFunds = funds > INT_MAX ? INT_MAX : (int)funds;
	p.startRating = world.startGrade > 0 ? world.startGrade - 1 : 0;
	p.startDay = world.startMonth;
	p.lobbyHeight = world.lobbyHeight > 0 ? world.lobbyHeight : 1;

	// Floor area [topFloorLine, bottomFloorLine) in rows; build area RECT
	// (T2FloorInfo::IsValidRange: rect inside, right/bottom exclusive).
	p.highestFloor = world.highestFloor();
	p.lowestFloor = world.lowestFloor();
	p.hasBuildArea = true;
	p.unitOriginX = world.width / 2;
	p.groundRow = world.groundLine;
	p.buildMinX = world.buildArea.left - p.unitOriginX;
	p.buildMaxX = world.buildArea.right - p.unitOriginX;
	p.buildTopRow = world.buildArea.top;
	p.buildBottomRow = world.buildArea.bottom;
	p.unbuildable = world.unbuildable;
	p.maskWidth = world.finfWidth;
	p.maskHeight = world.finfHeight;

	// Holidays are identical in all worlds; only Tokyo overrides IsRainyDay
	// (game-parity.md T4/T5).
	p.calendar = Calendar::YootTower;
	p.rain = world.id == YootWorld::Id::Tokyo ? Rain::YootTokyo : Rain::Never;
	p.linearClock = true;

	for (size_t i = 0; i < world.grades.size(); ++i)
	{
		Grade g;
		g.vipFlag = world.grades[i].vipFlag;
		g.population = world.grades[i].population;
		p.grades.push_back(g);
	}

	if (const YootEvent * viev = world.findEvent("VIEV"))
	{
		for (size_t i = 0; i < viev->vips.size(); ++i)
		{
			const YootVipDef & d = viev->vips[i];
			Vip v;
			v.grade = d.grade;
			v.vipFlag = d.vipFlag;
			v.destPluginId = d.destPluginId;
			v.beginMinute = viev->header.beginMin;
			v.arrivalMinute = d.arrivalMin;
			v.goodMsg = d.goodMsg;
			v.badMsg = d.badMsg;
			p.vips.push_back(v);
		}
	}
	return p;
}

RulesProfile RulesProfile::yootTowerStub()
{
	return yootTower(builtinYootWorld(YootWorld::Id::Tokyo));
}

static RulesProfile defaultProfile(BuildRules::Ruleset ruleset)
{
	switch (ruleset)
	{
	case BuildRules::Ruleset::SimTower:  return RulesProfile::simTower();
	case BuildRules::Ruleset::OpenSky:   return RulesProfile::openSky();
	case BuildRules::Ruleset::YootTower: return RulesProfile::yootTowerStub();
	}
	return RulesProfile();
}

const int BuildRules::kUnavailable;

BuildRules::BuildRules(Ruleset ruleset) :
	m_ruleset(ruleset),
	m_profile(defaultProfile(ruleset))
{
}

BuildRules::BuildRules(Ruleset ruleset, const RulesProfile & profile) :
	m_ruleset(ruleset),
	m_profile(profile)
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
	return m_ruleset == Ruleset::SimTower || m_profile.hasBuildArea;
}

bool BuildRules::allowsHorizontalFootprint(int x, int width) const
{
	if (!hasRepresentableEnd(x, width)) return false;

	const int end = x + width;
	if (m_profile.hasBuildArea)
		return x >= m_profile.buildMinX && end <= m_profile.buildMaxX;

	if (m_ruleset != Ruleset::SimTower)
		return true;

	return x >= kSimTowerMinX && end <= kSimTowerMaxX;
}

bool BuildRules::allowsFootprint(int x, int floor, int width, int height) const
{
	if (!allowsHorizontalFootprint(x, width)) return false;
	// SimTower/OpenSky keep their existing per-site floor checks.
	if (!m_profile.yootWorld) return true;
	if (height < 1 || floor > INT_MAX - height) return false;
	for (int f = floor; f < floor + height; ++f)
		for (int cx = x; cx < x + width; ++cx)
			if (!m_profile.allowsCell(cx, f)) return false;
	return true;
}

int BuildRules::minRatingToBuild(const std::string & prototypeId) const
{
	std::map<std::string, RulesProfile::Unlock>::const_iterator u = m_profile.unlocks.find(prototypeId);
	if (u == m_profile.unlocks.end())
		return LevelUp::minRatingToBuild(prototypeId);
	return u->second.available ? u->second.minRating : kUnavailable;
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
