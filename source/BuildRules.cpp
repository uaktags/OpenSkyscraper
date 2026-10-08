/* Copyright (c) 2026 OpenSkyscraper Project */
#include "BuildRules.h"

#include "LevelUp.h"
#include "YootTenants.h"
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

	// T2EV events: the plugin's list, or the decoded built-in one when the
	// world came from builtinYootWorld() (no plugin, so no events).
	p.worldWidth = world.width;
	p.facilities.dustOnGrade = world.dustOnAt;
	p.facilities.toiletGrade = world.toiletAt;
	p.facilities.toiletDenomi = world.toiletDenomi;
	p.facilities.hasColdTable = world.hasColdTable && world.coldMonthly.size() == 12;
	p.facilities.coldMinGrade = world.coldMinGrade;
	if (p.facilities.hasColdTable)
		for (int k = 0; k < 12; ++k) p.facilities.coldMonthly[k] = world.coldMonthly[k];
	p.events = yootEventSpecs(world);
	if (!world.fromPlugin && p.events.empty())
		p.events = builtinYootEventSpecs((int)world.id);

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

bool RulesProfile::allowsBuildAreaCell(int x, int floor) const
{
	if (!hasBuildArea) return false;
	if (x < buildMinX || x >= buildMaxX) return false;
	const int row = rowForFloor(floor);
	if (row < buildTopRow || row >= buildBottomRow) return false;
	if (!unbuildable.empty())
	{
		const int unit = unitForX(x);
		if (unit >= 0 && row >= 0 && unit < maskWidth && row < maskHeight &&
		    unbuildable[(size_t)row * maskWidth + unit] != 0)
			return false;
	}
	return true;
}

bool BuildRules::allowsSpecialFootprint(int x, int floor, int width, int height) const
{
	if (!m_profile.yootWorld) return allowsFootprint(x, floor, width, height);
	if (!allowsHorizontalFootprint(x, width)) return false;
	if (height < 1 || floor > INT_MAX - height) return false;
	for (int f = floor; f < floor + height; ++f)
		for (int cx = x; cx < x + width; ++cx)
			if (!m_profile.allowsBuildAreaCell(cx, f)) return false;
	return true;
}

namespace {
	struct YootOnly { const char * id; int defaultRating; const char * plugin; };
	// ATTR mGameLevel - 1 when no world unlock data is loaded.
	const YootOnly kYootOnly[] = {
		{"powerroom", 2, "PowerRoom.t2p"},          // grade 3
		{"trashfacility", 2, "TrashFacility.t2p"},  // / KTrashFacility.t2p, grade 3
		{"restroom", 0, "Restroom.t2p"},            // / KRestroom.t2p, grade 1
		{"doctor", 2, "Doctor's.t2p"},              // grade 3
		// Plan step 14.
		{"frontdesk", 1, "Front.t2p"},              // / RFront.t2p (grade 3), grade 2
		{"housekeeping", 1, "Maintenance.t2p"},     // / RMaintenance.t2p (grade 3), grade 2
		{"parkinglot", 2, "Parking.t2p"},          // grade 3
		{"parkinggate", 2, "Parking.t2p"},         // grade 3
		{"firedept", 3, "FireDepartment.t2p"},     // Kegon, grade 4
		{"apartment", 0, "Apartment.t2p"},         // Tokyo, grade 1
		// Venues, final items and transport (plan step 14 part B).
		{"theater", 2, "Theater.t2p"},        // Theater.t2p, grade 3
		{"classictheater", 2, "ClassicTheater.t2p"}, // ClassicTheater.t2p, grade 3
		{"eventcenter", 2, "EventCenter.t2p"},    // EventCenter.t2p, grade 3
		{"billboard", 0, "Billboard.t2p"},      // Billboard.t2p, grade 1
		{"stadium", 3, "Stadium.t2p"},        // Stadium.t2p, grade 4 (Tokyo final item)
		{"yootcathedral", 3, "Cathedral.t2p"},  // Cathedral.t2p, grade 4 (Hawaii final item)
		{"attraction", 3, "Attraction.t2p"},     // Attraction.t2p, grade 4 (Kegon final item)
		{"observatory", 2, "Observatory.t2p"},    // Observatory.t2p, grade 3
		{"ticketbooth", 2, "Ticket.t2p"},    // Ticket.t2p, grade 3
		{"gatheringarea", 3, "GatheringArea.t2p"},  // GatheringArea.t2p, grade 4
		{"boatdeck", 2, "Boatdeck.t2p"},       // Boatdeck.t2p, grade 3
		{"subway", 1, "Subway.t2p"},         // Subway.t2p, grade 2
		{"school", 3, "School.t2p"},         // School.t2p, grade 4 (Tokyo)
		{"skybridge", 0, "SkyBrdge.t2p"},    // SkyBrdge.t2p, grade 1
	};
}

bool BuildRules::isYootOnlyPrototype(const std::string & prototypeId)
{
	for (const YootOnly & y : kYootOnly)
		if (prototypeId == y.id) return true;
	// Plugin-backed tenants (YootTenants.h): one prototype per plugin.
	return isYootTenantPrototypeId(prototypeId);
}

bool BuildRules::isReplacedInYoot(const std::string & prototypeId)
{
	// The YootTower plugins behind them are the Trash Facility and the
	// Doctor's Office, the Theater, Event Center, Subway and Cathedral,
	// which are native items there. The generic shop, restaurant and fast
	// food give way to one plugin-backed prototype per shop / restaurant
	// plugin (YootTenants.h).
	return prototypeId == "recycling" || prototypeId == "medicalcenter" ||
	       prototypeId == "cinema" || prototypeId == "partyhall" ||
	       prototypeId == "metro" || prototypeId == "cathedral" ||
	       prototypeId == "shop" || prototypeId == "restaurant" || prototypeId == "fastfood" ||
	       prototypeId == "parking"; // native Parking Gate + Parking Lot
}

const char * BuildRules::yootOnlyPlugin(const std::string & prototypeId)
{
	for (const YootOnly & y : kYootOnly)
		if (prototypeId == y.id) return y.plugin;
	return nullptr;
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
	// Power Room, Trash Facility, Restroom and Doctor's Office exist only in
	// YootTower; without world unlock data they keep their ATTR mGameLevel.
	for (const YootOnly & y : kYootOnly)
	{
		if (prototypeId != y.id) continue;
		if (m_ruleset != Ruleset::YootTower) return kUnavailable;
		if (m_profile.unlocks.find(prototypeId) == m_profile.unlocks.end()) return y.defaultRating;
	}
	if (isYootTenantPrototypeId(prototypeId))
	{
		if (m_ruleset != Ruleset::YootTower) return kUnavailable;
		// Without world unlock data the plugin's ATTR level is unknown here.
		if (m_profile.unlocks.find(prototypeId) == m_profile.unlocks.end()) return 0;
	}
	if (m_ruleset == Ruleset::YootTower && isReplacedInYoot(prototypeId))
		return kUnavailable;
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
