/* Copyright (c) 2026 OpenSkyscraper Project */
/* Minimal smoke test for the CTest harness. No test framework dependency. */
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <numeric>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <functional>

#include <climits>
#include <cstring>
#include <tinyxml2.h>

#include "../source/Time.h"
#include "../source/Route.h"
#include "../source/Money.h"
#include "../source/NameManager.h"
#include "../source/TenantRules.h"
#include "../source/TimeWindowStyle.h"
#include "../source/WindowsPEExecutable.h"
#include "../source/Lighting.h"
#include "../source/JudgeSystem.h"
#include "../source/TenantRules.h"
#include "../source/LevelUp.h"
#include "../source/SaveHash.h"
#include "../source/PluginManager.h"
#include "../source/PluginResources.h"
#include "../source/PluginDefs.h"
#include "../source/SubPluginDefs.h"
#include "../source/PluginResourceCodec.h"
#include "../source/ItemTypeCatalog.h"
#include "../source/PluginObjmap.h"
#include "../source/PluginBitmapLoader.h"
#include "../source/OspPlugin.h"
#include "../source/PluginT2pExport.h"
#include "../source/PluginResourceJson.h"
#include "../source/TdtLoader.h"
#include "../source/YootMisc.h"
#include "../source/BldLoader.h"
#include "../source/BldExport.h"
#include "../source/SimHarness.h"

#include "../source/BuildRules.h"
#include "../source/Skin.h"
#include "../source/YootWorld.h"
#include "../source/YootEvents.h"
#include "../source/YootCatalog.h"
#include "../source/YootRules.h"
#include "../source/YootBackdrop.h"
#include "../source/YootFacilities.h"
#include "../source/YootPlacement.h"
#include "../source/YootVenues.h"
#include "../source/YootMovie.h"
#include "../source/YootPool.h"
#include "../source/YootTenants.h"
#include "../source/YootSpecials.h"

static int g_failures = 0;

// SFML texture construction also needs a graphics context. Check before
// creating textures or Application, so the seam suite remains headless.
static bool graphicsScenariosAvailable()
{
    const char *headless = std::getenv("OPENSKY_HEADLESS");
    if (headless && std::string(headless) == "1") return false;
#if defined(__linux__) || defined(__FreeBSD__)
    const char *display = std::getenv("DISPLAY");
    if (!display || !*display) return false;
#endif
    return true;
}

#define EXPECT(cond)                                                          \
    do                                                                        \
    {                                                                         \
        if (!(cond))                                                          \
        {                                                                     \
            std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":"       \
                      << __LINE__ << "\n";                                    \
            ++g_failures;                                                     \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            std::cout << "PASS: " << #cond << "\n";                           \
        }                                                                     \
    } while (0)

#define EXPECT_NEAR(a, b, eps)                                                \
    do                                                                        \
    {                                                                         \
        double _va = (a);                                                     \
        double _vb = (b);                                                     \
        if (std::fabs(_va - _vb) > (eps))                                     \
        {                                                                     \
            std::cerr << "FAIL: " << #a << " (" << _va << ") vs " << #b       \
                      << " (" << _vb << ") |d|>" << (eps) << " at "            \
                      << __FILE__ << ":" << __LINE__ << "\n";                 \
            ++g_failures;                                                     \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            std::cout << "PASS: " << #a << " ~ " << #b << "\n";               \
        }                                                                     \
    } while (0)

static void testSmoke()
{
    EXPECT(1 + 1 == 2);
    EXPECT(true);
    EXPECT(sizeof(int) >= 4);
    EXPECT(std::string("hello").size() == 5u);
}

static void testBuildRules()
{
	using OT::BuildRules;

	BuildRules simTower(BuildRules::Ruleset::SimTower);
	BuildRules openSky(BuildRules::Ruleset::OpenSky);
	BuildRules yootTower(BuildRules::Ruleset::YootTower);

	EXPECT(std::string(simTower.identifier()) == "simtower");
	EXPECT(std::string(simTower.displayName()) == "SimTower");
	EXPECT(std::string(openSky.identifier()) == "opensky");
	EXPECT(std::string(openSky.displayName()) == "OpenSky");
	EXPECT(std::string(yootTower.identifier()) == "yoottower");
	EXPECT(std::string(yootTower.displayName()) == "YootTower");

	BuildRules::Ruleset parsed = BuildRules::Ruleset::OpenSky;
	EXPECT(BuildRules::parseIdentifier("simtower", parsed));
	EXPECT(parsed == BuildRules::Ruleset::SimTower);
	EXPECT(BuildRules::parseIdentifier("opensky", parsed));
	EXPECT(parsed == BuildRules::Ruleset::OpenSky);
	EXPECT(BuildRules::parseIdentifier("yoottower", parsed));
	EXPECT(parsed == BuildRules::Ruleset::YootTower);
	EXPECT(!BuildRules::parseIdentifier("SimTower", parsed));

	EXPECT(simTower.hasFixedHorizontalBoundary());
	EXPECT(!openSky.hasFixedHorizontalBoundary());
	// BuildRules(YootTower) without a world is the built-in Tokyo stub.
	EXPECT(yootTower.hasFixedHorizontalBoundary());

	// SimTower's 375-cell lot is exactly [-187, 188).
	EXPECT(simTower.allowsHorizontalFootprint(-187, 375));
	EXPECT(simTower.allowsHorizontalFootprint(-187, 1));
	EXPECT(simTower.allowsHorizontalFootprint(187, 1));
	EXPECT(!simTower.allowsHorizontalFootprint(-188, 1));
	EXPECT(!simTower.allowsHorizontalFootprint(187, 2));

	// OpenSky has no generic fixed horizontal boundary; the YootTower stub
	// is bounded by Tokyo's build area.
	EXPECT(openSky.allowsHorizontalFootprint(-500000, 1000000));
	EXPECT(!yootTower.allowsHorizontalFootprint(-500000, 1000000));

	EXPECT(!simTower.allowsHorizontalFootprint(0, 0));
	EXPECT(!openSky.allowsHorizontalFootprint(0, -1));
	EXPECT(!yootTower.allowsHorizontalFootprint(INT_MAX, 1));
	EXPECT(!openSky.allowsHorizontalFootprint(INT_MAX, 1));
	EXPECT(!simTower.allowsHorizontalFootprint(1, INT_MAX));
}

static void testSkin()
{
	using OT::Skin;
	using OT::parseSkinIdentifier;
	using OT::defaultSkinForRuleset;

	Skin parsed = Skin::OpenSky;
	EXPECT(parseSkinIdentifier("classic", parsed));
	EXPECT(parsed == Skin::Classic);
	EXPECT(parseSkinIdentifier("opensky", parsed));
	EXPECT(parsed == Skin::OpenSky);
	EXPECT(parseSkinIdentifier("yoottower", parsed));
	EXPECT(parsed == Skin::Yoot);
	EXPECT(!parseSkinIdentifier("Classic", parsed));
	EXPECT(!parseSkinIdentifier("", parsed));
	EXPECT(!parseSkinIdentifier("simtower", parsed));

	EXPECT(std::string(OT::skinIdentifier(Skin::Classic)) == "classic");
	EXPECT(std::string(OT::skinDisplayName(Skin::Classic)) == "Classic");
	EXPECT(std::string(OT::skinIdentifier(Skin::Yoot)) == "yoottower");

	EXPECT(defaultSkinForRuleset(OT::BuildRules::Ruleset::SimTower) == Skin::Classic);
	EXPECT(defaultSkinForRuleset(OT::BuildRules::Ruleset::YootTower) == Skin::Yoot);
	EXPECT(defaultSkinForRuleset(OT::BuildRules::Ruleset::OpenSky) == Skin::OpenSky);

	EXPECT(OT::sanctionedSkinCount(OT::BuildRules::Ruleset::SimTower) == 2);
	EXPECT(OT::sanctionedSkinCount(OT::BuildRules::Ruleset::OpenSky) == 3);
	EXPECT(OT::sanctionedSkinCount(OT::BuildRules::Ruleset::YootTower) == 2);
	EXPECT(OT::sanctionedSkin(OT::BuildRules::Ruleset::SimTower, 0) == Skin::Classic);
	EXPECT(OT::sanctionedSkin(OT::BuildRules::Ruleset::SimTower, 1) == Skin::OpenSky);
	EXPECT(OT::sanctionedSkin(OT::BuildRules::Ruleset::OpenSky, 0) == Skin::OpenSky);
	EXPECT(OT::sanctionedSkin(OT::BuildRules::Ruleset::OpenSky, 1) == Skin::Yoot);
	EXPECT(OT::sanctionedSkin(OT::BuildRules::Ruleset::YootTower, 0) == Skin::Yoot);
	EXPECT(OT::sanctionedSkin(OT::BuildRules::Ruleset::YootTower, 1) == Skin::OpenSky);

	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::SimTower, Skin::Classic));
	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::SimTower, Skin::OpenSky));
	EXPECT(!OT::isSkinSanctioned(OT::BuildRules::Ruleset::SimTower, Skin::Yoot));

	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::OpenSky, Skin::OpenSky));
	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::OpenSky, Skin::Yoot));
	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::OpenSky, Skin::Classic));

	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::YootTower, Skin::Yoot));
	EXPECT(OT::isSkinSanctioned(OT::BuildRules::Ruleset::YootTower, Skin::OpenSky));
	EXPECT(!OT::isSkinSanctioned(OT::BuildRules::Ruleset::YootTower, Skin::Classic));
}

// Time seam: hourToAbsolute / absoluteToHour are pure static conversions.
// The conversion is non-linear (SimTower-style day compression), so a
// characterization test pins down the boundaries rather than arithmetic.
static void testTimeConversion()
{
    using OT::Time;

    // Anchor points from the doc comment in Time.h.
    EXPECT_NEAR(Time::hourToAbsolute(0.0), 0.0, 1e-9);
    EXPECT_NEAR(Time::hourToAbsolute(1.0), 1.0 / 26.0, 1e-9);
    EXPECT_NEAR(Time::hourToAbsolute(7.0), 3.0 / 26.0, 1e-9);
    EXPECT_NEAR(Time::hourToAbsolute(12.0), 7.0 / 26.0, 1e-9);
    EXPECT_NEAR(Time::hourToAbsolute(13.0), 15.0 / 26.0, 1e-9);
    EXPECT_NEAR(Time::hourToAbsolute(24.0), 1.0, 1e-9);
    // Next-day UI values clamp at the end of the displayed day rather than
    // entering the old uninitialized branch; negative values clamp to dawn.
    EXPECT_NEAR(Time::hourToAbsolute(25.0), 1.0, 1e-9);
    EXPECT_NEAR(Time::hourToAbsolute(-1.0), 0.0, 1e-9);

    // Mid-segment spot check inside the long 12:00-13:00 band.
    EXPECT_NEAR(Time::hourToAbsolute(12.5),
                7.0 / 26.0 + 0.5 * (8.0 / 26.0), 1e-9);

    // Reverse direction: absoluteToHour(0) -> 0, absoluteToHour(1) -> 24.
    EXPECT_NEAR(Time::absoluteToHour(0.0), 0.0, 1e-9);
    EXPECT_NEAR(Time::absoluteToHour(1.0), 24.0, 1e-9);
    EXPECT_NEAR(Time::absoluteToHour(7.0 / 26.0), 12.0, 1e-9);
    EXPECT_NEAR(Time::absoluteToHour(15.0 / 26.0), 13.0, 1e-9);
}

static void testNightSpeedMultiplier()
{
    using OT::Time;

    struct TestTime : public Time
    {
        using Time::set;
        using Time::advance;

        void pause() { speed = 0; speed_animated = 0; }
    };

    const double dt = 1.0;

    TestTime day;
    day.set(Time::hourToAbsolute(12.0));
    const double dayStart = day.absolute;
    day.advance(dt);
    const double dayDelta = day.absolute - dayStart;

    TestTime night;
    night.set(Time::hourToAbsolute(20.0));
    const double nightStart = night.absolute;
    night.advance(dt);
    const double nightDelta = night.absolute - nightStart;
    // Without the old binary night multiplier, absolute time advances at a
    // uniform rate (no rubberbanding). The non-linear absoluteToHour mapping
    // still makes PERCEIVED night hours pass faster.
    EXPECT_NEAR(nightDelta, dayDelta, 1e-12);

    TestTime at19;
    at19.set(Time::hourToAbsolute(19.0));
    const double at19Start = at19.absolute;
    at19.advance(dt);
    // 19:00 was the old binary switch boundary; verify no speed jump now.
    EXPECT_NEAR(at19.absolute - at19Start, dayDelta, 1e-12);

    TestTime at7;
    at7.set(Time::hourToAbsolute(7.0));
    const double at7Start = at7.absolute;
    at7.advance(dt);
    EXPECT_NEAR(at7.absolute - at7Start, dayDelta, 1e-12);

    TestTime pausedNight;
    pausedNight.set(Time::hourToAbsolute(20.0));
    pausedNight.pause();
    const double pausedStart = pausedNight.absolute;
    pausedNight.advance(dt);
    EXPECT(pausedNight.absolute == pausedStart);
}

// Route seam: clear() resets all fields including the cached score and
// transport counters. score() returns the cached value.
static void testRouteClear()
{
    using OT::Route;
    Route r;
    // Manually tweak the cached score and counters to confirm clear().
    r.updateScore(42);
    // Direct field access is fine here: this is a test, not the engine.
    r.numStairs = 5;
    r.numEscalators = 7;
    r.numElevators = 9;
    EXPECT(r.score() == 42);

    r.clear();
    EXPECT(r.empty());
    EXPECT(r.score() == 0);
    EXPECT(r.numStairs == 0);
    EXPECT(r.numEscalators == 0);
    EXPECT(r.numElevators == 0);
}

static void testMoneyAccounting()
{
    OT::Money money;
    money.clear(1000);
    money.record(500, "rent_income");
    money.record(-200, "maintenance");
    EXPECT(money.balance == 1300);
    EXPECT(money.todayIncome == 500);
    EXPECT(money.todayExpenses == 200);
    EXPECT(money.todayNet() == 300);
    EXPECT(money.todayTotalsByCategory["rent_income"] == 500);
    EXPECT(money.todayTotalsByCategory["maintenance"] == -200);

    money.finalizeDay();
    EXPECT(money.todayIncome == 0);
    EXPECT(money.todayExpenses == 0);
    EXPECT(money.yesterdayIncome == 500);
    EXPECT(money.yesterdayExpenses == 200);
    EXPECT(money.yesterdayNet() == 300);
    EXPECT(money.recentIncome() == 500);
    EXPECT(money.recentExpenses() == 200);
    EXPECT(money.recentNet() == 300);
}

static void testTimeWindowModernLayout()
{
    const OT::TimeWindowLayout layout = OT::makeModernTimeWindowLayout(1.0f);

    EXPECT(layout.clientWidth == 520);
    EXPECT(layout.clientHeight == 80);
    EXPECT(layout.dateX > layout.ratingX + layout.ratingWidth);
    EXPECT(layout.metricsX > layout.dateX + 120);
    EXPECT(layout.tooltipWidth >= 298);
    EXPECT(layout.watchSize == 34);
    EXPECT(layout.backgroundTopBlue > layout.backgroundBottomBlue);
    EXPECT(layout.accentBlue > layout.backgroundTopBlue);
    EXPECT(layout.tooltipBackgroundX == 52);
    EXPECT(layout.tooltipBackgroundY == 30);
    EXPECT(layout.tooltipBackgroundWidth == 310);
    EXPECT(layout.tooltipBackgroundHeight == 22);
}

static void testNameManager()
{
    using OT::NameManager;

    NameManager::reset();

    // Person::Type enum values used here mirror source/Person.h.
    // 0=kMan, 1=kSalesman, 2=kWoman1, 5=kHousekeeper, 8=kSecurity.
    EXPECT(NameManager::makeName(0) == "Man #1");
    EXPECT(NameManager::makeName(1) == "Salesman #1");
    EXPECT(NameManager::makeName(1) == "Salesman #2");
    EXPECT(NameManager::makeName(5) == "Housekeeper #1");
    EXPECT(NameManager::makeName(8) == "Security #1");

    // Per-type counters are independent.
    NameManager::reset();
    EXPECT(NameManager::makeName(0) == "Man #1");
    EXPECT(NameManager::makeName(2) == "Woman #1");
    EXPECT(NameManager::makeName(0) == "Man #2");

    // Out-of-range type returns a generic "Person" without crashing.
    EXPECT(NameManager::makeName(-1) == "Person");
    EXPECT(NameManager::makeName(999) == "Person");

    NameManager::reset();
}

// Lighting seam: the per-channel color multiplication is a pure helper
// (declared inline in Lighting.h) and is the core of the Phase 3.4 tint
// math. Verify identity, halving, and alpha preservation.
static void testLightingColorMath()
{
	using OT::Lighting;
	const sf::Color white(255, 255, 255, 255);
	const sf::Color half(128, 128, 128, 200);

	// white * white == white (no-op tint).
	sf::Color id = Lighting::multiply(white, white);
	EXPECT(id.r == 255 && id.g == 255 && id.b == 255);

	// white * half ~= half (tint of 128/255 ~ 0.502 should darken white).
	sf::Color dark = Lighting::multiply(white, half);
	EXPECT(dark.r == 128 && dark.g == 128 && dark.b == 128);
	// Alpha of the *left* argument is preserved by contract.
	EXPECT(dark.a == 255);

	// Alpha preservation: 200 * whatever keeps 200.
	sf::Color alphaTest = Lighting::multiply(sf::Color(255, 255, 255, 200), half);
	EXPECT(alphaTest.a == 200);

	// Symmetry of the RGB channels: multiply(black, anything) == black.
	sf::Color black(0, 0, 0, 255);
	sf::Color k = Lighting::multiply(black, white);
	EXPECT(k.r == 0 && k.g == 0 && k.b == 0);

	// Halving twice equals quartering (associativity on integers).
	sf::Color quarter = Lighting::multiply(Lighting::multiply(white, half), half);
	EXPECT(quarter.r == (128 * 128) / 255);
	EXPECT(quarter.r > 60 && quarter.r < 70);
}

// LevelUp seam: the rating progression rules are pure static functions
// (LevelUp.h / LevelUp.cpp). The Phase 3.2 VIP system and level-up
// dialog both build on these. Pin down the advancement table and the
// min-rating-to-build lookup so a refactor can't silently shift the
// star thresholds.
//
// Note: rating is 0-based internally (0=1 star ... 4=5 stars).
// advancementRequirements(N) returns the requirements to advance FROM
// rating N (i.e. N stars) TO N+1 (so it indexes into the rules table
// at N+1, skipping the "starting tower" entry at index 0).
static void testLevelUpRules()
{
	using OT::LevelUp;
	using OT::JudgeSystem;

	// Authentic SimTower progression: 1->2 requires 300 population;
	// 2->3 requires 1000 + security; 3->4 requires 5000 + suite,
	// recycling, medical coverage, VIP approval, and the evening window;
	// 4->5 requires 10000 + metro, recycling, medical, and that window.
	const LevelUp::Requirements * r1to2 = LevelUp::advancementRequirements(0);
	EXPECT(r1to2 != nullptr);
	EXPECT(r1to2->population == 300);

	const LevelUp::Requirements * r2to3 = LevelUp::advancementRequirements(1);
	EXPECT(r2to3 != nullptr);
	EXPECT(r2to3->population == 1000);
	EXPECT(r2to3->needsSecurity);
	EXPECT(!r2to3->needsVip); // VIP approval moved to the 4-star gate

	const LevelUp::Requirements * r3to4 = LevelUp::advancementRequirements(2);
	EXPECT(r3to4 != nullptr);
	EXPECT(r3to4->population == 5000);
	EXPECT(!r3to4->needsSecurity);
	EXPECT(r3to4->needsMedical);
	EXPECT(r3to4->needsVip);
	EXPECT(r3to4->needsSuite);
	EXPECT(r3to4->needsRecycling);
	EXPECT(r3to4->needsWindow);
	EXPECT(!r3to4->needsMetro);

	const LevelUp::Requirements * r4to5 = LevelUp::advancementRequirements(3);
	EXPECT(r4to5 != nullptr);
	EXPECT(r4to5->population == 10000);
	EXPECT(r4to5->needsMetro);
	EXPECT(r4to5->needsMedical);
	EXPECT(r4to5->needsRecycling);
	EXPECT(r4to5->needsWindow);
	EXPECT(!r4to5->needsVip);
	EXPECT(!r4to5->needsSuite);
	EXPECT(!r4to5->needsSecurity);

	const LevelUp::Requirements * rTower = LevelUp::advancementRequirements(4);

	// 5 stars -> TOWER is the cathedral wedding special event: the EXE
	// never promotes from 5 stars in LevelUp (1150:014d-0154). The row IS
	// the landing implementation, but needsWedding holds it shut until
	// Cathedral::startWedding's mid-ceremony ratingMayIncrease crowns the
	// tower (JS parity: Requirements.needsWedding).
	EXPECT(rTower != nullptr);
	EXPECT(rTower->population == 15000);
	// The EXE reads no security/medical/metro flag on this step.
	EXPECT(!rTower->needsSecurity && !rTower->needsMedical && !rTower->needsMetro);
	EXPECT(rTower->needsWedding);
	EXPECT(!rTower->needsWindow); // the wedding crown lands weekend midday

	// TOWER (rating 5) is the max - no advancement from rating 5.
	const LevelUp::Requirements * rMax = LevelUp::advancementRequirements(5);
	EXPECT(rMax == nullptr);

	// meetsRequirements is a pure PromotionContext gate.
	LevelUp::PromotionContext ctx;
	ctx.population = 300;
	EXPECT(LevelUp::meetsRequirements(*r1to2, ctx));
	ctx.population = 299;
	EXPECT(!LevelUp::meetsRequirements(*r1to2, ctx));

	ctx = LevelUp::PromotionContext();
	ctx.population = 1000;
	EXPECT(!LevelUp::meetsRequirements(*r2to3, ctx)); // missing security
	ctx.securityOffices = 1;
	EXPECT(LevelUp::meetsRequirements(*r2to3, ctx));

	ctx = LevelUp::PromotionContext();
	ctx.population = 5000;
	ctx.hotelSuites = 1;
	ctx.recyclingAdequate = true;
	ctx.medicalAdequate = true;
	ctx.vipReviews = 1;
	ctx.hour = 18.0;
	ctx.day = 0;
	EXPECT(LevelUp::meetsRequirements(*r3to4, ctx));
	ctx.hour = 12.0;
	EXPECT(!LevelUp::meetsRequirements(*r3to4, ctx)); // outside window
	ctx.hour = 18.0;
	ctx.day = 2;
	EXPECT(!LevelUp::meetsRequirements(*r3to4, ctx)); // weekend
	ctx.day = 0;
	ctx.vipReviews = 0;
	EXPECT(!LevelUp::meetsRequirements(*r3to4, ctx)); // missing VIP

	// 5 -> TOWER: every facility flag met is NOT enough - without a held
	// wedding the row stays shut (the pre-fix bug crowned the tower at
	// 15,000 population with no cathedral).
	ctx = LevelUp::PromotionContext();
	ctx.population = 15000;
	ctx.securityOffices = 1;
	ctx.medicalAdequate = true;
	ctx.metros = 1;
	ctx.weddingCompleted = false;
	EXPECT(!LevelUp::meetsRequirements(*rTower, ctx)); // no wedding yet
	ctx.weddingCompleted = true;
	EXPECT(LevelUp::meetsRequirements(*rTower, ctx)); // crowned mid-ceremony
	ctx.metros = 0;
	ctx.securityOffices = 0;
	ctx.medicalAdequate = false;
	EXPECT(LevelUp::meetsRequirements(*rTower, ctx)); // no facility flag binds
	ctx.population = 14999;
	EXPECT(!LevelUp::meetsRequirements(*rTower, ctx)); // the population line does

	// minRatingToBuild: authentic toolbox mapping, the rest 0.
	// Retail shops gate at 3 stars (SimTower wiki star-rating table;
	// YootTower ATTR mGameLevel: the coin/mart item ships at level 3).
	EXPECT(LevelUp::minRatingToBuild("shop")             == 2);
	EXPECT(LevelUp::minRatingToBuild("elevator-service") == 1);
	EXPECT(LevelUp::minRatingToBuild("hotel_single")     == 1);
	EXPECT(LevelUp::minRatingToBuild("security")         == 1);
	EXPECT(LevelUp::minRatingToBuild("escalator")        == 2);
	EXPECT(LevelUp::minRatingToBuild("elevator-express") == 2);
	EXPECT(LevelUp::minRatingToBuild("restaurant")       == 2);
	EXPECT(LevelUp::minRatingToBuild("cinema")           == 2);
	EXPECT(LevelUp::minRatingToBuild("partyhall")        == 2);
	EXPECT(LevelUp::minRatingToBuild("hotel_double")     == 2);
	EXPECT(LevelUp::minRatingToBuild("hotel_suite")      == 2);
	EXPECT(LevelUp::minRatingToBuild("medicalcenter")    == 2);
	EXPECT(LevelUp::minRatingToBuild("recycling")        == 2);
	EXPECT(LevelUp::minRatingToBuild("parking")          == 2);
	EXPECT(LevelUp::minRatingToBuild("metro")            == 3);
	EXPECT(LevelUp::minRatingToBuild("secom")            == 99); // never offered (batch3 Q8)
	EXPECT(LevelUp::minRatingToBuild("cathedral")        == 4);
	EXPECT(LevelUp::minRatingToBuild("fastfood")         == 0);
	EXPECT(LevelUp::minRatingToBuild("office")           == 0);
	EXPECT(LevelUp::minRatingToBuild("lobby")            == 0);
	EXPECT(LevelUp::minRatingToBuild("nonexistent_prototype") == 0);
}

static void testLevelUpPromotionHelpers()
{
	using OT::LevelUp;

	EXPECT(LevelUp::promotionWindowOpen(17.0, 0));
	EXPECT(LevelUp::promotionWindowOpen(23.5, 1));
	EXPECT(LevelUp::promotionWindowOpen(3.0, 0));
	EXPECT(!LevelUp::promotionWindowOpen(16.99, 0));
	EXPECT(!LevelUp::promotionWindowOpen(7.0, 0));
	EXPECT(!LevelUp::promotionWindowOpen(17.0, 2));

	EXPECT(LevelUp::recyclingAdequate(2499, 1));
	EXPECT(!LevelUp::recyclingAdequate(2500, 1));
	EXPECT(!LevelUp::recyclingAdequate(1, 0));

	EXPECT(LevelUp::medicalBand(-5) == 0);
	EXPECT(LevelUp::medicalBand(14) == 0);
	EXPECT(LevelUp::medicalBand(15) == 1);
	EXPECT(LevelUp::medicalBand(44) == 2);
	EXPECT(LevelUp::medicalAdequate({25}, {20}));
	EXPECT(!LevelUp::medicalAdequate({25, 35}, {20}));
	EXPECT(LevelUp::medicalAdequate({25, 35}, {20, -3}));
}

static void testLevelUpStarReward()
{
	// The Phase 1a adjudication found no promotion cash or award gift:
	// the old wording was a misread of the buried-treasure payout.
	EXPECT(OT::LevelUp::starRewardMessage(1).empty());
	EXPECT(OT::LevelUp::starRewardMessage(2).empty());
	EXPECT(OT::LevelUp::starRewardMessage(3).empty());
	EXPECT(OT::LevelUp::starRewardMessage(4).empty());
	EXPECT(OT::LevelUp::starRewardMessage(5).empty());
	EXPECT(OT::LevelUp::starRewardMessage(6).empty());
	EXPECT(OT::LevelUp::starRewardMessage(99).empty());
}

static void testSaveHash()
{
	namespace SH = OT::SaveHash;

	// Injected hash verifies.
	const std::string doc = "<tower funds=\"4000000\" rating=\"0\"/>\n";
	const std::string injected = SH::injectSaveHash(doc);
	EXPECT(injected.rfind("<tower hash=\"", 0) == 0);
	const SH::VerifyResult ok = SH::verifySaveHash(injected);
	EXPECT(ok.valid);
	EXPECT(!ok.legacy);
	EXPECT(ok.stored.size() == 8);

	// Tampered content must fail.
	std::string tampered = injected;
	tampered.replace(tampered.find("4000000"), 7, "9999999");
	const SH::VerifyResult bad = SH::verifySaveHash(tampered);
	EXPECT(!bad.valid);
	EXPECT(!bad.legacy);

	// Legacy files without a hash load gracefully.
	const SH::VerifyResult legacy = SH::verifySaveHash(doc);
	EXPECT(legacy.valid);
	EXPECT(legacy.legacy);

	// Determinism + web parity of the FNV-1a digest itself.
	// fnv1a("") = 811c9dc5; fnv1a("a") = e40c292c (standard test vectors).
	EXPECT(SH::fnv1aHex("") == "811c9dc5");
	EXPECT(SH::fnv1aHex("a") == "e40c292c");
	EXPECT(SH::computeSaveHash("x") == SH::computeSaveHash("x"));
	EXPECT(SH::computeSaveHash("x") != SH::computeSaveHash("y"));
}


static void testWindowsPEExecutable()
{
    OT::WindowsPEExecutable exe;
    bool loaded = exe.load("../data/Plugins/Condo.t2p");
    EXPECT(loaded == true);
    if (loaded) {
        EXPECT(exe.resources.find("ATTR") != exe.resources.end());
        EXPECT(exe.resources.find("OBJMAP") != exe.resources.end());
        EXPECT(exe.resources.find("2") != exe.resources.end()); // RT_BITMAP
        
        auto attr_it = exe.resources.find("ATTR");
        if (attr_it != exe.resources.end()) {
            EXPECT(attr_it->second.find(128) != attr_it->second.end());
            auto res_it = attr_it->second.find(128);
            if (res_it != attr_it->second.end()) {
                EXPECT(res_it->second.length > 0);
                EXPECT(res_it->second.data != nullptr);
            }
        }
    }
}

static void testWindowsSPExecutable()
{
    OT::WindowsPEExecutable exe;
    bool loaded = exe.load("../data/Plugins/AirJamaica.t2p");
    EXPECT(loaded == true);
    if (loaded) {
        EXPECT(exe.resources.find("ATTR") != exe.resources.end());
        EXPECT(exe.resources.find("DESC") != exe.resources.end());
        EXPECT(exe.resources.find("ADDF") != exe.resources.end());
        EXPECT(exe.resources.find("RAW") != exe.resources.end());
        
        auto attr_it = exe.resources.find("ATTR");
        if (attr_it != exe.resources.end()) {
            EXPECT(attr_it->second.find(128) != attr_it->second.end());
            auto res_it = attr_it->second.find(128);
            if (res_it != attr_it->second.end()) {
                EXPECT(res_it->second.length > 0);
                EXPECT(res_it->second.data != nullptr);
            }
        }
    }
}

// PluginManager seam: catalog every .t2p under data/Plugins, lazy-load
// one plugin, and confirm the resource layout matches what the existing
// WindowsPEExecutable test expects. This is the foundation for Phase A.2
// (loadFromPlugin) and Phase D (OBJMAP parser + coverage audit).
static void testPluginManager()
{
    using OT::AbstractPlugin;
    using OT::PluginManager;

    PluginManager pm;
    const int added = pm.addPluginDirectory("../data/Plugins");
    // 156 plugins are vendored under data/Plugins. Allow some slack in
    // case future pruning drops a few, but require the bulk of them.
    EXPECT(added >= 100);

    EXPECT(pm.has("Condo.t2p"));
    EXPECT(pm.has("Stair.t2p"));
    EXPECT(!pm.has("DoesNotExist.t2p"));

    // Idempotent: re-adding the same dir discovers nothing new.
    EXPECT(pm.addPluginDirectory("../data/Plugins") == 0);

    // enumerate() is sorted alphabetically.
    std::vector<std::string> names = pm.enumerate();
    EXPECT(names.size() >= 100);
    bool sorted = true;
    for (size_t i = 1; i < names.size(); ++i)
        if (names[i - 1] > names[i]) { sorted = false; break; }
    EXPECT(sorted);

    // Lazy load: a freshly-cataloged plugin hasn't been PE-parsed yet.
    AbstractPlugin * condo = pm.get("Condo.t2p");
    EXPECT(condo != nullptr);
    if (condo) {
        EXPECT(!condo->isLoaded());
        EXPECT(condo->ensureLoaded());
        EXPECT(condo->isLoaded());
        // ensureLoaded is idempotent.
        EXPECT(condo->ensureLoaded());

        // Same resource layout as testWindowsPEExecutable checks for.
        EXPECT(condo->hasResource("ATTR", 128));
        EXPECT(condo->hasResource("OBJMAP", 1000));
        EXPECT(condo->hasResource("2", 1000));  // RT_BITMAP id 1000

        // Missing resources return nullptr, not a crash.
        EXPECT(condo->getResource("NOPE", 9999) == nullptr);
        EXPECT(condo->getResource("OBJMAP", 9999) == nullptr);

        // enumerateResources returns the same data the manager exposes.
        std::vector<AbstractPlugin::ResourceKey> res = condo->enumerateResources();
        EXPECT(!res.empty());
    }

    // countEssential: a curated subset of plugins that OpenSkyscraper
    // considers load-bearing for the YootTower fallback tier. All of
    // these are vendored, so the count must match the input size.
    std::vector<std::string> essential = {
        "Condo.t2p", "Floor.t2p", "Stair.t2p", "Office.t2p",
        "StdElev.t2p", "Burger.t2p", "Subway.t2p",
    };
    EXPECT(pm.countEssential(essential) == static_cast<int>(essential.size()));

    // countEssential with a fake entry counts only the real ones.
    std::vector<std::string> withFake = essential;
    withFake.push_back("NotAReal.t2p");
    EXPECT(pm.countEssential(withFake) == static_cast<int>(essential.size()));

    // Every cataloged plugin that loads must declare both ATTR and DESC
    // metadata resources (true for both formats - see below). Beyond
    // that, plugins split into two families that the OBJMAP spike (D.1)
    // will need to handle separately:
    //
    //   PE-format (.t2p built as PE32): bitmaps under RT_BITMAP (type
    //     "2"), object descriptors under OBJMAP. ~105 plugins including
    //     every gameplay item (Burger, Condo, Stair, Office, etc.).
    //
    //   SP-format (.t2p built as 16-bit MZ/LE): bitmaps under RAW,
    //     object descriptors under ADDF (or just RAW alone). ~51 plugins
    //     including the MV_* movie-villain pack, Pl* party pack, Yoot,
    //     Ob9003, SandalBeachResort, Segapc, AirJamaica.
    //
    // Both formats parse via WindowsPEExecutable::load's auto-detection.
    int loadedCount = 0;
    int wellFormed = 0;
    int peFormat = 0;
    int spFormat = 0;
    int total = 0;
    for (const std::string & name : names) {
        AbstractPlugin * p = pm.get(name);
        if (!p) continue;
        ++total;
        if (!p->ensureLoaded()) continue;
        ++loadedCount;

        bool hasAttr = false, hasDesc = false;
        bool hasRtBitmap = false, hasRaw = false;
        for (const auto & rk : p->enumerateResources()) {
            if (rk.type == "ATTR") hasAttr = true;
            if (rk.type == "DESC") hasDesc = true;
            if (rk.type == "2")    hasRtBitmap = true;  // RT_BITMAP
            if (rk.type == "RAW")  hasRaw = true;        // SP-format bitmap
        }
        if (hasAttr && hasDesc) ++wellFormed;
        if (hasRtBitmap) ++peFormat;
        if (hasRaw)      ++spFormat;
    }
    EXPECT(total >= 100);
    // Most plugins load; the few that don't are tracked separately.
    EXPECT(loadedCount >= total - 20);
    // Every loaded plugin has ATTR + DESC (metadata every plugin needs).
    EXPECT(wellFormed == loadedCount);
    // Both formats are represented in the vendored catalog.
    EXPECT(peFormat >= 80);
    EXPECT(spFormat >= 30);
}

// PluginBitmapLoader seam: rasterize RT_BITMAP resources from PE/SP
// plugins. Lifted from YootCondo::loadPEBitmap; the calibration oracle
// is scratch/extract_t2p_stairs.py, which knows the exact dimensions
// of the Stair.t2p spiral-staircase resource.
static void testPluginBitmapLoader()
{
    using OT::PluginBitmapLoader;
    if (!graphicsScenariosAvailable()) {
        std::cout << "SKIP: plugin bitmap texture scenarios (headless)\n";
        return;
    }

    // Condo.t2p RT_BITMAP 1000 is the YootCondo empty-room sheet. The
    // YootCondo class loads it at runtime via the previous loadPEBitmap;
    // verify the loader produces a valid texture of the expected size.
    // Known dimensions (verified against the actual sheet): the sheet
    // holds 3 variants x 5 lighting states = 15 frames of 128x24 stacked
    // vertically, for a total of 128 wide x 360 tall. The YootCondo
    // rendering code indexes it as IntRect(0, frameIndex * 24, 128, 24).
    sf::Texture condo;
    bool ok = PluginBitmapLoader::loadFromPEFile("../data/Plugins/Condo.t2p", 1000, condo);
    EXPECT(ok);
    if (ok) {
        const sf::Vector2u sz = condo.getSize();
        EXPECT(sz.x == 128);
        EXPECT(sz.y == 360);
    }

    // Stair.t2p RT_BITMAP 1000 is the spiral-staircase sheet. Per
    // scratch/extract_t2p_stairs.py:11-13, it is a single 64x288 sheet
    // containing three vertical slices:
    //   Span 2 (2-story): (0, 0,   64, 60)
    //   Span 3 (3-story): (0, 60,  64, 156)
    //   Span 4 (4-story): (0, 156, 64, 288)
    // The total dimensions are the calibration target.
    sf::Texture stair;
    ok = PluginBitmapLoader::loadFromPEFile("../data/Plugins/Stair.t2p", 1000, stair);
    EXPECT(ok);
    if (ok) {
        const sf::Vector2u sz = stair.getSize();
        EXPECT(sz.x == 64);
        EXPECT(sz.y == 288);
    }

    // Missing resource / file return false cleanly.
    sf::Texture dummy;
    EXPECT(!PluginBitmapLoader::loadFromPEFile("../data/Plugins/Condo.t2p", 99999, dummy));
    EXPECT(!PluginBitmapLoader::loadFromPEFile("../data/Plugins/DoesNotExist.t2p", 1000, dummy));

    // rasterizeBitmapResource on null/garbage fails cleanly without
    // crashing. This is the lower-level seam that takes raw DIB bytes.
    EXPECT(!PluginBitmapLoader::rasterizeBitmapResource(nullptr, 0, dummy));
    const char garbage[40] = {0};
    EXPECT(!PluginBitmapLoader::rasterizeBitmapResource(garbage, sizeof(garbage), dummy));
}

// Plugin::resourcesByType seam: the grouped-by-type view that the
// upcoming coverage audit will use to answer "which plugins have an
// OBJMAP id 1000?" cheaply. Cross-checked against enumerateResources()
// (the flat view) so the two stay in sync.
static void testPluginResourcesByType()
{
    using OT::AbstractPlugin;
    using OT::PluginManager;

    PluginManager pm;
    pm.addPluginDirectory("../data/Plugins");

    AbstractPlugin * condo = pm.get("Condo.t2p");
    EXPECT(condo != nullptr);
    if (!condo) return;

    std::map<std::string, std::vector<int>> byType = condo->resourcesByType();

    // The flat view must contain exactly the same (type, id) pairs.
    std::vector<AbstractPlugin::ResourceKey> flat = condo->enumerateResources();
    EXPECT(flat.size() == std::accumulate(byType.begin(), byType.end(), size_t{0},
        [](size_t n, const auto & kv) { return n + kv.second.size(); }));

    // Both views must agree on OBJMAP membership. Condo.t2p ships
    // OBJMAP ids 128/1000/1001/1002/1003/1100 (verified via pefile).
    auto it = byType.find("OBJMAP");
    EXPECT(it != byType.end());
    if (it != byType.end()) {
        const std::vector<int> & ids = it->second;
        EXPECT(std::find(ids.begin(), ids.end(), 1000) != ids.end());
        EXPECT(std::find(ids.begin(), ids.end(), 1100) != ids.end());
        // ids come back sorted (callers can binary-search).
        EXPECT(std::is_sorted(ids.begin(), ids.end()));
    }

    // RT_BITMAP (type "2") for Condo has ids 100/300/301/1000-1003/1100.
    auto bmp = byType.find("2");
    EXPECT(bmp != byType.end());
    if (bmp != byType.end()) {
        EXPECT(std::find(bmp->second.begin(), bmp->second.end(), 1000) != bmp->second.end());
    }

    // A plugin that fails to load returns an empty map (no crash).
    OT::Plugin broken("broken.t2p", "../data/Plugins/DoesNotExist.t2p");
    EXPECT(broken.resourcesByType().empty());
}

// DecompiledPlugin seam: folder-based OpenTower plugins are discovered
// alongside .t2p files and expose the same AbstractPlugin surface. The
// data/Plugins/OpenTower/{Office,Condo,Hotel-*} plugins are generated by
// tools/build_opentower_sheets.py; this test verifies they load and that
// the downscale block produces the expected engine cell-grid dimensions.
static void testDecompiledPlugin()
{
    using OT::AbstractPlugin;
    using OT::PluginBitmapLoader;
    using OT::PluginManager;

    PluginManager pm;
    pm.addPluginDirectory("../data/Plugins");

    // Folder plugins are cataloged by relative path (edition/plugin).
    AbstractPlugin * office = pm.get("OpenTower/Office");
    EXPECT(office != nullptr);
    if (!office) return;

    EXPECT(office->ensureLoaded());
    EXPECT(office->isLoaded());
    // ensureLoaded is idempotent.
    EXPECT(office->ensureLoaded());

    // The folder synthesises the same (type,id) keys a PE plugin exposes.
    EXPECT(office->hasResource("2", 1000));      // RT_BITMAP ingame sheet
    EXPECT(office->hasResource("OBJMAP", 1000)); // frame layout
    EXPECT(office->hasResource("ATTR", 128));    // synthesised metadata
    EXPECT(office->hasResource("DESC", 128));

    // Typed accessors decode the synthesised metadata.
    OT::PluginAttr attr;
    EXPECT(office->getAttr(attr));
    EXPECT(attr.id == "CFFO");          // v1 "itemType" = file-order plugin id
    EXPECT(attr.itemType == 1);         // v1 "id" = numeric item type
    EXPECT(attr.name == "Office");

    OT::PluginDesc desc;
    EXPECT(office->getDesc(desc));
    EXPECT(desc.kind == OT::PluginDesc::TenantPlugin);

    // Downscale target matches the engine cell grid (144x168 = 2x7 of 72x24).
    unsigned int dw = 0, dh = 0;
    EXPECT(office->downscaleTarget(dw, dh));
    EXPECT(dw == 144);
    EXPECT(dh == 168);

    // The PNG loads and is resampled to the target dimensions.
    if (graphicsScenariosAvailable()) {
        sf::Texture tex;
        EXPECT(PluginBitmapLoader::loadFromPlugin(*office, 1000, tex));
        const sf::Vector2u sz = tex.getSize();
        EXPECT(sz.x == 144);
        EXPECT(sz.y == 168);
    } else {
        std::cout << "SKIP: decompiled plugin texture scenario (headless)\n";
    }

    // enumerateResources / resourcesByType agree.
    std::vector<AbstractPlugin::ResourceKey> flat = office->enumerateResources();
    EXPECT(!flat.empty());
    auto byType = office->resourcesByType();
    EXPECT(flat.size() == std::accumulate(byType.begin(), byType.end(), size_t{0},
        [](size_t n, const auto & kv) { return n + kv.second.size(); }));

    // The other generated plugins are present too.
    EXPECT(pm.has("OpenTower/Condo"));
    EXPECT(pm.has("OpenTower/Hotel-Single"));
    EXPECT(pm.has("OpenTower/Hotel-Double"));
    EXPECT(pm.has("OpenTower/Hotel-Suite"));

    // A plain directory without plugin.json is NOT picked up as a plugin.
    EXPECT(!pm.has("OpenTower"));
    EXPECT(!pm.has("OpenTower/DoesNotExist"));
}

// PluginResources / PluginDefs / SubPluginDefs / PluginResourceCodec seam:
// typed decode + byte-exact encode of every YootTower plugin resource type
// (doc/yoottower/plugin-schema.md, world-and-sp-formats.md §3). Three levels:
//   (a) byte-level: fixtures copied from real plugins (hex below), decoded,
//       checked field by field, and re-encoded to the identical bytes.
//   (b) spot checks of known values through the Plugin accessors, taken
//       from `python3 -I tools/yoottower/t2p_schema.py dump <plugin>`.
//   (c) a population sweep: every resource of every plugin in data/Plugins
//       decodes and re-encodes byte-identically.

static std::string fromHex(const char * hex)
{
    std::string out;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
        out.push_back(char((nib(hex[i]) << 4) | nib(hex[i + 1])));
    return out;
}

static bool readFileBytes(const std::string & path, std::string & out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Burger.t2p TNDF id 1 (334 bytes)
static const char * const kBurgerTndf =
    "011505000010000000010000000100000080040001275468652063617465676f7279206f662072657374617572616e74"
    "7320796f75206d616e6167652e000b52657374617572616e747300320000001400000000000000000000000000000080"
    "0000002c0100000100000007243230303030300006243830302f4d0030546865206661737420666f6f642073746f7265"
    "2077686572652074686520796f756e672070656f706c6520636f6d652e000b427572676572206c616e64000b42757267"
    "6572206c616e6400d00700000800000081000000820000002c0100000800000014000000000000000100000000000000"
    "8c000000960000000000000000000000010000000a000000000000000300000001000000040000000300000000000000"
    "000000000e000000000000000e000000e0010000280500000f000000000000008000000081000000800000000000";
// Burger.t2p SLDF id 1 (155 bytes)
static const char * const kBurgerSldf =
    "0106000000ec03000002000000010000002c0100000657616974657200ed03000002000000010000002d010000085761"
    "69747265737300f00300000200000001000000360100000657616974657200f103000002000000010000003701000008"
    "576169747265737300ee03000002000000010000002e0100000657616974657200ef03000002000000010000002f0100"
    "0008576169747265737300";
// Billboard.t2p OODF id 1 (248 bytes)
static const char * const kBillboardOodf =
    "01d007000014000000020000000100000080040000294974656d7320746f20626520706c61636564207768696c652069"
    "6e206f757473696465206d6f64652e00124f75747369646520466163696c697469657300c80000000a00000000000000"
    "0000000049506f42800000002c01000001000000072431303030303000022430003f5468652062696c6c626f61726420"
    "77686963682063616e20626520706c61636564206f6e207468652077616c6c206f662074686520737472756374757265"
    "2e000942696c6c626f617264000942696c6c626f61726400e80300000000000081000000820000002c01000032000000"
    "0000000000000000";
// StdElev.t2p MVDF id 1 (409 bytes)
static const char * const kStdElevMvdf =
    "0112070000040000000100000001000000028900013d5468652063617465676f7279206f66207472616e736974207379"
    "7374656d7320737563682061732073746169727320616e6420656c657661746f72732e000f5472616e73697420537973"
    "74656d7300140000000a0000001300000001000000000000007f0000002c0100000100000012243135303030303a2435"
    "303030302f63617200082431323030302f510032412073696d706c6520656c657661746f722063616e20626520657874"
    "656e64656420757020746f20313520666c6f6f72732e000f53696d706c6520456c657661746f72000f53696d706c6520"
    "456c657661746f7200dc0500007800000080000000820000002c01000014000000110000000000000003000000000000"
    "000000000096000000000000000000000000000000ffffffff0000000001000000000000000f00000001000000000000"
    "000f0000000300000001000000030000000000000006000000010000000000000000000000f401000002000000020000"
    "00fefffffffeffffff71170000721700000000000000000000";
// Subway.t2p RT_STRING id 64 (80 bytes)
static const char * const kSubwayStrings64 =
    "0000000000000000000000000000000000000000000000000000000000001800430072006f0077006400650064002000"
    "77006900740068002000700061007300730065006e0067006500720073002e00";
// Hawai.t2p STRTBL id 1 (121 bytes)
static const char * const kHawaiStrtbl =
    "010b000000272746726f6d206e6f772061742074686520746f70206f66207468652043617468656472616c2c20001974"
    "68652077656464696e672077696c6c2073746172742e27270010436f6e67726174756c6174696f6e732100034d722e00"
    "034d732e00026f66000000000003616e6400026f6600012c00";
// Burger.t2p DESC id 128 (9 bytes)
static const char * const kBurgerDesc =
    "016c506e5403000000";
// PlArt.t2p HEDF id 128 (133 bytes)
static const char * const kPlArtHedf =
    "010e4172742045786869626974696f6e000000000080000000020000000300000058020000fc0300001f0f0000fa0000"
    "0080000000e8030000320000000a0000003c000000810000002e4d6f6465726e206172742065786869626974696f6e2e"
    "0d0a506f70756c617220776974682073747564656e74732e000a48455f4172742e4d4f5600";
// PlArt.t2p PPDF id 128 (25 bytes)
static const char * const kPlArtPpdf =
    "015a000000d007000000000000000000000a00000064000000";
// MV_1stContact.t2p MODF id 128 (197 bytes)
static const char * const kMv1stContactModf =
    "010b31737420436f6e746163740000000000800000000000000000000000060000001200000003000000010000002076"
    "61636361206d6f64652f5075626c6973686564206279203a204f4239303033000d53205370696e68616d62757267000d"
    "4a6f6520487572726963616e650078000000f4010000164120736369656e63652066696374696f6e2066696c6d008000"
    "00005f000000650000002c010000460000000100000000000000810000008c000000114d565f317374436f6e74616374"
    "2e4d4f5600";
// MV_1stContact.t2p TMDF id 128 (47 bytes)
static const char * const kMv1stContactTmdf =
    "01ffffffff000000000000000000000000010000000000ffffffff120000001f000000ffffffff6400000000000000";


static void testFourCC()
{
    using OT::FourCC;
    FourCC f;
    f.c[0]='G'; f.c[1]='R'; f.c[2]='U'; f.c[3]='B';
    EXPECT(f == "GRUB");
    EXPECT(f != "BURG");
    // A 5-char string must not match a 4-char code.
    EXPECT(f != "GRUBX");
    // Short string doesn't match either.
    EXPECT(f != "GRU");
    EXPECT(f.str() == "GRUB");
    // Mac source order is the reversed file order ('BURG').
    EXPECT(f.macStr() == "BURG");
    EXPECT(f.toUint32LE() == 0x42555247u);
    EXPECT(FourCC::fromMacOrder("BURG") == f);
    EXPECT(FourCC::fromUint32LE(0x42555247u) == f);
    EXPECT(FourCC::fromFileOrder("GRUB") == f);
    // lPnT as bytes == 0x546E506C, the literal ConstructProgramPlugin passes.
    FourCC lpnt; lpnt.c[0]='l'; lpnt.c[1]='P'; lpnt.c[2]='n'; lpnt.c[3]='T';
    EXPECT(lpnt.toUint32LE() == 0x546E506Cu);
    EXPECT(lpnt.macStr() == "TnPl");
    EXPECT(FourCC::fromFileOrder("****").isNone());

    FourCC a, b;
    a.c[0]='A'; a.c[1]='B'; a.c[2]='C'; a.c[3]='D';
    b = a;
    EXPECT(a == b);
}

static void testParseAttrBinary()
{
    using OT::PluginAttr;
    // Exact bytes of Burger.t2p ATTR id=128 (42 B). Read order (CFilePlugin
    // ctor 0x1000df20): mID 'BURG', mAttrib 1, mDependID '****',
    // mGameLevel 1, mSecretItem 0, mItemType 1301, mMustItem 0, name.
    const std::string bytes = fromHex(
        "0147525542010000002a2a2a2a01000000000000001505000000000000"
        "0b4275726765722" "06c616e6400");
    EXPECT(bytes.size() == 42u);
    PluginAttr a;
    const bool ok = OT::parsePluginAttr(bytes.data(), int(bytes.size()), a);
    EXPECT(ok);
    if (ok) {
        EXPECT(a.valid);
        EXPECT(!a.isTextForm);
        EXPECT(a.id == "GRUB");
        EXPECT(a.id.macStr() == "BURG");
        EXPECT(a.attrib == 1u);
        EXPECT(a.dependID.isNone());
        EXPECT(!a.hasDependency());
        EXPECT(a.gameLevel == 1);
        EXPECT(a.secretItem == 0);
        EXPECT(a.itemType == 1301);
        EXPECT(a.mustItem == 0);
        EXPECT(a.name == "Burger land");
        std::string again;
        EXPECT(OT::encodePluginAttr(a, again));
        EXPECT(again == bytes);
    }

    // Hotel rooms depend on the Front desk: mDependID bytes "NORF" = 'FRON'.
    PluginAttr room = a;
    room.dependID = OT::FourCC::fromMacOrder("FRON");
    EXPECT(room.dependID == "NORF");
    EXPECT(room.hasDependency());
    std::string roomBytes;
    EXPECT(OT::encodePluginAttr(room, roomBytes));
    EXPECT(roomBytes.compare(9, 4, "NORF") == 0);
    PluginAttr room2;
    EXPECT(OT::parsePluginAttr(roomBytes.data(), int(roomBytes.size()), room2));
    EXPECT(room2.dependID.macStr() == "FRON");

    // Truncated / null / trailing input is rejected cleanly (no crash).
    PluginAttr bad;
    EXPECT(!OT::parsePluginAttr(nullptr, 0, bad));
    EXPECT(!OT::parsePluginAttr(bytes.data(), 5, bad));
    std::string trailing = bytes + "x";
    EXPECT(!OT::parsePluginAttr(trailing.data(), int(trailing.size()), bad));
    // A non-zero string terminator is not representable -> rejected.
    std::string badTerm = bytes;
    badTerm.back() = 1;
    EXPECT(!OT::parsePluginAttr(badTerm.data(), int(badTerm.size()), bad));
}

static void testParseAttrText()
{
    using OT::PluginAttr;
    // DefaultToolPalette's text-form ATTR (the only one), byte for byte.
    static const char kText[] =
        "(mID)\t\t1000\r\n"
        "(mAttrib)\t16777215\r\n"
        "(mDependID)\t0\r\n"
        "(mGameLevel)\t1\r\n"
        "(mSecretItem)\t0\r\n"
        "(mItemType)\t1414484044\r\n"
        "(mMustItem)\t1\r\n"
        "(mPluginName)\tDefault Tool palette\r\n";
    PluginAttr a;
    const bool ok = OT::parsePluginAttr(kText, int(sizeof(kText) - 1), a);
    EXPECT(ok);
    if (ok) {
        EXPECT(a.valid);
        EXPECT(a.isTextForm);
        EXPECT(a.id.toUint32LE() == 1000u);
        EXPECT(a.attrib == 16777215u);
        EXPECT(a.dependID.toUint32LE() == 0u);
        EXPECT(a.gameLevel == 1);
        EXPECT(a.itemType == 1414484044);   // 0x544F504C == 'TOPL'
        EXPECT(a.mustItem == 1);
        EXPECT(a.name == "Default Tool palette");
        std::string again;
        EXPECT(OT::encodePluginAttr(a, again));
        EXPECT(again == std::string(kText, sizeof(kText) - 1));
        // Editing a value regenerates the text, which parses back.
        a.gameLevel = 3;
        EXPECT(OT::encodePluginAttr(a, again));
        PluginAttr b;
        EXPECT(OT::parsePluginAttr(again.data(), int(again.size()), b));
        EXPECT(b.isTextForm && b.gameLevel == 3 && b.name == "Default Tool palette");
    }
}

static void testParseDesc()
{
    using OT::PluginDesc;
    auto check = [](const char * bytes, int len, const char * tag,
                    PluginDesc::Kind kind) {
        PluginDesc d;
        const bool ok = OT::parsePluginDesc(bytes, len, d);
        EXPECT(ok);
        if (ok) {
            EXPECT(d.valid);
            EXPECT(d.version == 1);
            EXPECT(d.classTag == tag);
            EXPECT(d.value == 3u);
            EXPECT(d.kind == kind);
            std::string again;
            EXPECT(OT::encodePluginDesc(d, again));
            EXPECT(again == std::string(bytes, size_t(len)));
        }
    };
    const std::string burger = fromHex(kBurgerDesc);
    check(burger.data(), int(burger.size()), "lPnT", PluginDesc::TenantPlugin);
    static const char kMover[]  = {0x01,'l','P','v','M', 3,0,0,0};
    check(kMover, 9, "lPvM", PluginDesc::MoverPlugin);
    static const char kPalette[]= {0x01,'L','P','O','T', 3,0,0,0};
    check(kPalette, 9, "LPOT", PluginDesc::ToolPalette);
    static const char kMovie[]  = {0x01,'l','P','o','M', 3,0,0,0};
    check(kMovie, 9, "lPoM", PluginDesc::MoviePlugin);
    static const char kHall[]   = {0x01,'l','P','e','H', 3,0,0,0};
    check(kHall, 9, "lPeH", PluginDesc::HallEventPlugin);
    static const char kAd[]     = {0x01,'l','P','d','A', 3,0,0,0};
    check(kAd, 9, "lPdA", PluginDesc::AdvertisePlugin);
    EXPECT(OT::pluginTagFromKind(PluginDesc::OuterObjPlugin) == "lPoO");

    // Unknown tag still parses; kind stays Unknown.
    static const char kUnk[] = {0x01,'l','P','e','Q', 3,0,0,0};
    PluginDesc u;
    EXPECT(OT::parsePluginDesc(kUnk, 9, u));
    EXPECT(u.kind == PluginDesc::Unknown);
    EXPECT(u.classTag == "lPeQ");

    // Truncation is rejected.
    PluginDesc bad;
    EXPECT(!OT::parsePluginDesc(nullptr, 0, bad));
    EXPECT(!OT::parsePluginDesc(kMover, 5, bad));
}

// DPDF = T2DayParamDef mPercent[EDay][EWhether] (not a placement record).
static void testDayParamCodec()
{
    // Burger.t2p / Coin.t2p DPDF id=128.
    static const char kBurger[] = {0x01, 70,0,0,0, 50,0,0,0, 100,0,0,0, 80,0,0,0};
    OT::DayParamDef d;
    EXPECT(OT::decodeDayParamDef(kBurger, sizeof(kBurger), d));
    EXPECT(d.percent[0][0] == 70u && d.percent[0][1] == 50u);
    EXPECT(d.percent[1][0] == 100u && d.percent[1][1] == 80u);
    std::string again;
    EXPECT(OT::encodeDayParamDef(d, again));
    EXPECT(again == std::string(kBurger, sizeof(kBurger)));
    OT::DayParamDef bad;
    EXPECT(!OT::decodeDayParamDef(kBurger, 10, bad));
    EXPECT(!OT::decodeDayParamDef(nullptr, 0, bad));
}

// PLDF = T2PlaceParamDef 3 x {percent, score} (not a price list).
static void testPlaceParamCodec()
{
    // Burger.t2p PLDF id=128: (40,1000) (10,300) (50,100).
    static const unsigned char kBurger[] = {
        0x01, 0x28,0,0,0, 0xE8,0x03,0,0, 0x0A,0,0,0, 0x2C,0x01,0,0, 0x32,0,0,0, 0x64,0,0,0 };
    OT::PlaceParamDef p;
    EXPECT(OT::decodePlaceParamDef(reinterpret_cast<const char *>(kBurger), sizeof(kBurger), p));
    EXPECT(p.places[0].percent == 40u && p.places[0].score() == 1000);
    EXPECT(p.places[1].percent == 10u && p.places[1].score() == 300);
    EXPECT(p.places[2].percent == 50u && p.places[2].score() == 100);
    std::string again;
    EXPECT(OT::encodePlaceParamDef(p, again));
    EXPECT(again == std::string(reinterpret_cast<const char *>(kBurger), sizeof(kBurger)));

    // Restroom.t2p PLDF id=128: a negative score stored sign-extended.
    const std::string restroom = fromHex("01000000000000000000000000b4000000640000009cffffff");
    OT::PlaceParamDef r;
    EXPECT(OT::decodePlaceParamDef(restroom.data(), restroom.size(), r));
    EXPECT(r.places[1].score() == 180);
    EXPECT(r.places[2].percent == 100u);
    EXPECT(r.places[2].score() == -100);
    EXPECT(r.places[2].scoreSlot == -100);
    EXPECT(OT::encodePlaceParamDef(r, again));
    EXPECT(again == restroom);
}

// TMDF = T2TenantMemberTableDef (id 1 employees, id 2 customers).
static void testMemberTableCodec()
{
    // Burger.t2p TMDF id=1 (73 B): econo -1, 2 members.
    const std::string burger1 = fromHex(
        "01ffffffff000000000000000000000000" "02000000"
        "0000" "04000000" "12000000" "12000000" "01000000" "00000000" "01000000"
        "0000" "04000000" "13000000" "13000000" "02000000" "00000000" "02000000");
    EXPECT(burger1.size() == 73u);
    OT::MemberTableDef t;
    const bool ok = OT::decodeMemberTableDef(burger1.data(), burger1.size(), t);
    EXPECT(ok);
    if (ok) {
        EXPECT(t.econoType == 0xFFFFFFFFu);
        EXPECT(t.specialFlag == 0u);
        EXPECT(t.dayParamID == 0 && t.placeParamID == 0);
        EXPECT(t.members.size() == 2u);
        if (t.members.size() == 2u) {
            EXPECT(t.members[0].name.empty());
            EXPECT(t.members[0].demandType == 4);
            EXPECT(t.members[0].startSilhouetteType == 18 && t.members[0].endSilhouetteType == 18);
            EXPECT(t.members[0].optionType == 1);
            EXPECT(t.members[0].numOfPeople == 1u);
            EXPECT(t.members[1].optionType == 2 && t.members[1].numOfPeople == 2u);
        }
        std::string again;
        EXPECT(OT::encodeMemberTableDef(t, again));
        EXPECT(again == burger1);
    }
    // SP movie audience table (MV_1stContact TMDF 128).
    const std::string mv = fromHex(kMv1stContactTmdf);
    OT::MemberTableDef m;
    EXPECT(OT::decodeMemberTableDef(mv.data(), mv.size(), m));
    EXPECT(m.members.size() == 1u);
    std::string again;
    EXPECT(OT::encodeMemberTableDef(m, again) && again == mv);
}

// PADF = T2PeopleAnimeDef.
static void testPeopleAnimeCodec()
{
    // Burger.t2p PADF id=1 (61 B).
    // param1 1, param2 3, count 3, elems (2,2,1,13) (0,2,1,13) (4,2,1,13).
    const std::string fixed = fromHex(
        "01" "01000000" "03000000" "03000000"
        "02000000" "02000000" "01000000" "0d000000"
        "00000000" "02000000" "01000000" "0d000000"
        "04000000" "02000000" "01000000" "0d000000");
    EXPECT(fixed.size() == 61u);
    OT::PeopleAnimeDef d;
    EXPECT(OT::decodePeopleAnimeDef(fixed.data(), fixed.size(), d));
    EXPECT(d.param1 == 1 && d.param2 == 3);
    EXPECT(d.elems.size() == 3u);
    if (d.elems.size() == 3u) {
        EXPECT(d.elems[0].position == 2 && d.elems[0].pattern == 2);
        EXPECT(d.elems[0].silhouette == 1 && d.elems[0].animeID == 13);
        EXPECT(d.elems[2].position == 4);
    }
    std::string again;
    EXPECT(OT::encodePeopleAnimeDef(d, again) && again == fixed);
}

// TNDF / MVDF / OODF: the ToolDef -> EquipDef -> TenantDef/MoverDef chain.
static void testTemplateDefCodec()
{
    using OT::TemplateDef;
    {
        const std::string b = fromHex(kBurgerTndf);
        TemplateDef d;
        const bool ok = OT::decodeTemplateDef(TemplateDef::Tenant, b.data(), b.size(), d);
        EXPECT(ok);
        if (ok) {
            EXPECT(d.tool.toolType == 1301);
            EXPECT(d.tool.width == 16 && d.tool.height == 1 && d.tool.level == 1);
            EXPECT(d.tool.attribute == 16778368u);
            EXPECT(d.tool.categoryName == "Restaurants");
            EXPECT(d.tool.category == 50 && d.tool.toolNo == 20);
            EXPECT(d.tool.subPluginType.isZero());
            EXPECT(d.tool.skip3 == 128 && d.tool.skip4 == 300);
            EXPECT(d.tool.variations.size() == 1u);
            if (!d.tool.variations.empty()) {
                const OT::ToolVariation & v = d.tool.variations[0];
                EXPECT(v.priceString == "$200000");
                EXPECT(v.outMoneyString == "$800/M");
                EXPECT(v.name == "Burger land");
                EXPECT(v.price == 2000 && v.outMoney == 8);
                EXPECT(v.consumptionPower == 8);
            }
            EXPECT(d.equip.capacity == 20 && d.equip.skip8 == 1);
            EXPECT(d.equip.variations.size() == 1u);
            EXPECT(d.equip.inMoney.size() == 1u && d.equip.inMoney[0] == 10);
            EXPECT(d.tenant.downFlag == 3u);
            EXPECT(d.tenant.numOfKind == 1 && d.tenant.numOfPattern == 4);
            EXPECT(d.tenant.belongCapacity == 3);
            EXPECT(d.tenant.entranceH == 14 && d.tenant.exitH == 14);
            EXPECT(d.tenant.openTime == 480 && d.tenant.closeTime == 1320);
            EXPECT(d.tenant.bindTime == 15);
            EXPECT(d.tenant.movieName.empty());
            std::string again;
            EXPECT(OT::encodeTemplateDef(d, again));
            EXPECT(again == b);

            // Inconsistent EquipDef/ToolDef variation counts cannot encode.
            TemplateDef broken = d;
            broken.equip.variations.push_back({});
            EXPECT(!OT::encodeTemplateDef(broken, again));
            // Nor can a string longer than 255 bytes.
            TemplateDef longStr = d;
            longStr.tool.categoryHelp.assign(256, 'x');
            EXPECT(!OT::encodeTemplateDef(longStr, again));
        }
        // Decoding the tenant stream as a mover fails (strict layout).
        TemplateDef wrong;
        EXPECT(!OT::decodeTemplateDef(TemplateDef::Mover, b.data(), b.size(), wrong));
    }
    {
        const std::string b = fromHex(kStdElevMvdf);
        TemplateDef d;
        const bool ok = OT::decodeTemplateDef(TemplateDef::Mover, b.data(), b.size(), d);
        EXPECT(ok);
        if (ok) {
            EXPECT(d.tool.toolType == 1810);
            EXPECT(d.tool.width == 4);
            EXPECT(d.tool.skip1 == 19 && d.tool.skip2 == 1 && d.tool.skip3 == 127);
            EXPECT(d.tool.variations.size() == 1u);
            if (!d.tool.variations.empty()) {
                EXPECT(d.tool.variations[0].priceString == "$150000:$50000/car");
                EXPECT(d.tool.variations[0].outMoneyString == "$12000/Q");
                EXPECT(d.tool.variations[0].price == 1500);
                EXPECT(d.tool.variations[0].outMoney == 120);
            }
            EXPECT(d.equip.capacity == 17 && d.equip.skip8 == 3);
            EXPECT(d.equip.inMoney.empty());
            EXPECT(d.mover.addSize == (OT::PluginRect{-1, 0, 1, 0}));
            EXPECT(d.mover.maxLength == 15 && d.mover.minLength == 1);
            EXPECT(d.mover.score == 15 && d.mover.defaultWait == 3);
            EXPECT(d.mover.speed[0] == 1 && d.mover.speed[1] == 3 && d.mover.speed[2] == 0);
            EXPECT(d.mover.maxModule == 6);
            EXPECT(d.mover.moduleParam2 == 500);
            EXPECT(d.mover.moduleInset == (OT::PluginRect{2, 2, -2, -2}));
            EXPECT(d.mover.skip14 == 6001 && d.mover.skip15 == 6002);
            std::string again;
            EXPECT(OT::encodeTemplateDef(d, again) && again == b);
        }
    }
    {
        const std::string b = fromHex(kBillboardOodf);
        TemplateDef d;
        const bool ok = OT::decodeTemplateDef(TemplateDef::OuterObj, b.data(), b.size(), d);
        EXPECT(ok);
        if (ok) {
            EXPECT(d.tool.toolType == 2000);
            EXPECT(d.tool.subPluginType.macStr() == "BoPI");
            EXPECT(d.tool.category == 200);
            EXPECT(d.unreadA == 0 && d.unreadB == 0);
            EXPECT(std::string(d.resourceType()) == "OODF");
            std::string again;
            EXPECT(OT::encodeTemplateDef(d, again) && again == b);
        }
    }
}

// SLDF / PPDF / WODF / STRTABLE: count-prefixed record lists.
static void testListCodecs()
{
    {
        const std::string b = fromHex(kBurgerSldf);
        OT::SilhouetteList l;
        EXPECT(OT::decodeSilhouetteList(b.data(), b.size(), l));
        EXPECT(l.size() == 6u);
        if (l.size() == 6u) {
            EXPECT(l[0].silhouetteType == 1004 && l[0].width == 2 && l[0].routeType == 1);
            EXPECT(l[0].bitmapID == 300 && l[0].name == "Waiter");
            EXPECT(l[1].name == "Waitress");
        }
        std::string again;
        EXPECT(OT::encodeSilhouetteList(l, again) && again == b);
    }
    {
        // One T2PeopleDef record (Hawai/Kegon/Tokyo PPDF entry 0).
        OT::PeopleList l(1);
        l[0].peopleType = 8;
        l[0].silhouetteType = 300;
        std::string bytes;
        EXPECT(OT::encodePeopleList(l, bytes));
        EXPECT(bytes == fromHex("01" "01000000" "08000000" "00000000" "2c010000"
                                "2c010000" "64000000" "e6000000"));
        OT::PeopleList back;
        EXPECT(OT::decodePeopleList(bytes.data(), bytes.size(), back));
        EXPECT(back.size() == 1u && back[0].peopleType == 8 && back[0].stressMax == 300);
        EXPECT(back[0].stressBlueLimit == 100 && back[0].stressYellowLimit == 230);
    }
    {
        // Burger WODF entry 0: sex 3, age 0xFF, flags 0, special 0.
        const std::string text = "Let me have the potato too.";
        std::string b = fromHex("01" "01000000" "03000000" "ff000000" "00000000" "00000000");
        b.push_back(char(text.size()));
        b += text;
        b.push_back('\0');
        OT::WordList l;
        EXPECT(OT::decodeWordList(b.data(), b.size(), l));
        EXPECT(l.size() == 1u);
        if (!l.empty()) {
            EXPECT(l[0].sexMask == 3u && l[0].ageMask == 0xFFu);
            EXPECT(l[0].flags == 0u && l[0].special == 0u && l[0].text == text);
        }
        std::string again;
        EXPECT(OT::encodeWordList(l, again) && again == b);
    }
    {
        const std::string b = fromHex(kHawaiStrtbl);
        OT::StringTableDef l;
        EXPECT(OT::decodeStringTableDef(b.data(), b.size(), l));
        EXPECT(l.size() == 11u);
        if (l.size() == 11u) {
            EXPECT(l[2] == "Congratulations!");
            EXPECT(l[6].empty());
        }
        std::string again;
        EXPECT(OT::encodeStringTableDef(l, again) && again == b);
    }
}

// RT_STRING blocks and the SP sub-plugin payloads.
static void testStringAndSubPluginCodecs()
{
    {
        const std::string b = fromHex(kSubwayStrings64);
        OT::StringBlock blk;
        EXPECT(OT::decodeStringBlock(b.data(), b.size(), blk));
        EXPECT(blk.strings.size() == 16u);
        if (blk.strings.size() == 16u) {
            EXPECT(blk.strings[15] == u"Crowded with passengers.");
            EXPECT(blk.strings[0].empty());
        }
        std::string again;
        EXPECT(OT::encodeStringBlock(blk, again) && again == b);
    }
    {
        const std::string b = fromHex(kPlArtHedf);
        OT::HallEventDef h;
        EXPECT(OT::decodeHallEventDef(b.data(), b.size(), h));
        EXPECT(h.header.title == "Art Exhibition");
        EXPECT(h.header.tieupDate == 0u);
        EXPECT(h.imageID == 128);
        EXPECT(h.unkCC == 2u && h.bookingDays == 3u);
        EXPECT(h.openTime == 600u && h.closeTime == 1020u);
        EXPECT(h.monthMask == 0xF1Fu);
        EXPECT(h.cost == 250 && h.customerTableID == 128);
        EXPECT(h.unkDC == 1000 && h.unkE0 == 50 && h.unkE4 == 10);
        EXPECT(h.bindTime == 60 && h.stillImageID == 129);
        EXPECT(h.movieFile == "HE_Art.MOV");
        std::string again;
        EXPECT(OT::encodeHallEventDef(h, again) && again == b);
    }
    {
        const std::string b = fromHex(kPlArtPpdf);
        OT::SubPluginPeopleParamDef p;
        EXPECT(OT::decodeSubPluginPeopleParamDef(b.data(), b.size(), p));
        EXPECT(p.values[0] == 90 && p.values[1] == 2000 && p.values[4] == 10 && p.values[5] == 100);
        std::string again;
        EXPECT(OT::encodeSubPluginPeopleParamDef(p, again) && again == b);
        // The PE PPDF codec rejects this layout.
        OT::PeopleList pl;
        EXPECT(!OT::decodePeopleList(b.data(), b.size(), pl));
    }
    {
        const std::string b = fromHex(kMv1stContactModf);
        OT::MovieDef m;
        EXPECT(OT::decodeMovieDef(b.data(), b.size(), m));
        EXPECT(m.header.title == "1st Contact");
        EXPECT(m.imageID == 128 && m.filmKind == 0);
        EXPECT(m.phase2Months == 6 && m.phase3Months == 18);
        EXPECT(m.phase4Months == 3 && m.cycleMonths == 1);
        EXPECT(m.production == "vacca mode/Published by : OB9003");
        EXPECT(m.director == "S Spinhamburg");
        EXPECT(m.starring == "Joe Hurricane");
        EXPECT(m.runningTime == 120);
        EXPECT(m.customerTableID == 128);
        EXPECT(m.extra.empty());
        EXPECT(m.stillImageID == 129);
        EXPECT(m.movieFile == "MV_1stContact.MOV");
        std::string again;
        EXPECT(OT::encodeMovieDef(m, again) && again == b);
    }
    {
        // AirJamaica.t2p ADDF 128 (40 B).
        const std::string b = fromHex(
            "010b416972204a616d61696361000000000049506f4280000000010000000c000000f40100000000");
        OT::AdvertiseDef a;
        EXPECT(OT::decodeAdvertiseDef(b.data(), b.size(), a));
        EXPECT(a.header.title == "Air Jamaica");
        EXPECT(a.subType == "IPoB" && a.subType.macStr() == "BoPI");
        EXPECT(a.imageID == 128 && a.unkD0 == 1u && a.days == 12u && a.money == 500);
        EXPECT(a.path.empty());
        std::string again;
        EXPECT(OT::encodeAdvertiseDef(a, again) && again == b);
    }
    {
        // A synthetic SP container round-trips; offsets are recomputed.
        OT::SpContainer c;
        OT::SpContainer::Entry e1; e1.type = 0x41545452u; e1.id = 128; e1.data = "\x01" "abc";
        OT::SpContainer::Entry e2; e2.type = 0; e2.id = 129; e2.data = "dib";
        c.entries = {e1, e2};
        std::string bytes;
        EXPECT(OT::encodeSpContainer(c, bytes));
        EXPECT(bytes.size() == 6u + 32u + 4u + 3u);
        EXPECT(bytes.compare(0, 2, "SP") == 0);
        OT::SpContainer back;
        EXPECT(OT::decodeSpContainer(bytes.data(), bytes.size(), back));
        EXPECT(back.entries.size() == 2u);
        if (back.entries.size() == 2u) {
            EXPECT(back.entries[0].typeName() == "ATTR");
            EXPECT(back.entries[1].typeName() == "BITMAP");
            EXPECT(back.entries[1].offset == 6u + 32u + 4u);
            EXPECT(back.find("ATTR", 128) != nullptr);
        }
        std::string again;
        EXPECT(OT::encodeSpContainer(back, again) && again == bytes);
        EXPECT(!OT::decodeSpContainer("XX", 2, back));
    }
}

// Spot checks of known values through the Plugin accessors (values from
// `python3 -I tools/yoottower/t2p_schema.py dump` / world_sp.py dump).
static void testPluginTypedAccessors()
{
    using OT::Plugin;
    using OT::PluginDesc;
    using OT::PluginAttr;

    // Burger (restaurant tenant).
    {
        Plugin burger("Burger.t2p", "../data/Plugins/Burger.t2p");
        EXPECT(burger.ensureLoaded());
        EXPECT(burger.container() == OT::ResourceContainer::PE);
        PluginAttr a;
        EXPECT(burger.getAttr(a));
        EXPECT(a.id.macStr() == "BURG");
        EXPECT(a.itemType == 1301);
        EXPECT(a.name == "Burger land");
        OT::DayParamDef day;
        EXPECT(burger.getDayParamDef(day));
        EXPECT(day.percent[0][0] == 70u && day.percent[0][1] == 50u);
        EXPECT(day.percent[1][0] == 100u && day.percent[1][1] == 80u);
        OT::PlaceParamDef place;
        EXPECT(burger.getPlaceParamDef(place));
        EXPECT(place.places[0].percent == 40u && place.places[0].score() == 1000);
        OT::TemplateDef def;
        EXPECT(burger.getTemplateDef(def));
        EXPECT(def.kind == OT::TemplateDef::Tenant);
        EXPECT(def.tool.toolType == a.itemType);
        const auto tables = burger.getMemberTables();
        EXPECT(tables.size() == 2u);
        if (tables.count(2)) {
            const OT::MemberTableDef & c = tables.at(2);
            EXPECT(c.econoType == 1u && c.specialFlag == 1u);
            EXPECT(c.dayParamID == 128 && c.placeParamID == 128);
            EXPECT(c.members.size() == 3u);
            if (c.members.size() == 3u) EXPECT(c.members[0].percent == 70u);
        }
        OT::PeopleAnimeDef anime;
        EXPECT(burger.getPeopleAnimeDef(anime));
        EXPECT(anime.elems.size() == 3u);
        OT::SilhouetteList sil;
        EXPECT(burger.getSilhouettes(sil) && sil.size() == 6u);
        OT::WordList words;
        EXPECT(burger.getWords(words) && words.size() == 28u);
        OT::MovieDef none;
        EXPECT(!burger.getMovieDef(none));
    }
    // StdElev (mover): price $150000 (1500 x $100), per-car tier 500, max
    // length 15 floors, 6 cars.
    {
        Plugin elev("StdElev.t2p", "../data/Plugins/StdElev.t2p");
        PluginAttr a;
        EXPECT(elev.getAttr(a));
        EXPECT(a.id.macStr() == "StdE" && a.itemType == 1810);
        OT::TemplateDef def;
        EXPECT(elev.getTemplateDef(def));
        EXPECT(def.kind == OT::TemplateDef::Mover);
        EXPECT(std::string(def.resourceType()) == "MVDF");
        if (!def.tool.variations.empty()) {
            EXPECT(def.tool.variations[0].price == 1500);
            EXPECT(def.tool.variations[0].priceString == "$150000:$50000/car");
            EXPECT(def.tool.variations[0].name == "Simple Elevator");
        }
        EXPECT(def.mover.moduleParam2 == 500);
        EXPECT(def.mover.maxLength == 15);
        EXPECT(def.mover.maxModule == 6);
        EXPECT(def.equip.capacity == 17);
    }
    // Stair (mover with no shaft): 'STAR' 1800, $5000.
    {
        Plugin stair("Stair.t2p", "../data/Plugins/Stair.t2p");
        PluginAttr a;
        EXPECT(stair.getAttr(a));
        EXPECT(a.id.macStr() == "STAR" && a.itemType == 1800 && a.name == "Stairs");
        PluginDesc d;
        EXPECT(stair.getDesc(d) && d.kind == PluginDesc::MoverPlugin);
        OT::TemplateDef def;
        EXPECT(stair.getTemplateDef(def));
        EXPECT(def.mover.maxLength == 0 && def.mover.score == 50 && def.equip.stress == 30);
        if (!def.tool.variations.empty()) EXPECT(def.tool.variations[0].priceString == "$5000");
        OT::DayParamDef none;
        EXPECT(!stair.getDayParamDef(none));   // movers carry no DPDF
    }
    // Condo (residential): mID 'APRT', mAttrib 2, four sale prices.
    {
        Plugin condo("Condo.t2p", "../data/Plugins/Condo.t2p");
        PluginAttr a;
        EXPECT(condo.getAttr(a));
        EXPECT(a.id == "TRPA" && a.id.macStr() == "APRT");
        EXPECT(a.itemType == 1101 && a.attrib == 2u && a.gameLevel == 1);
        OT::TemplateDef def;
        EXPECT(condo.getTemplateDef(def));
        EXPECT(def.equip.inMoney == (std::vector<int32_t>{1000, 1200, 1500, 2000}));
    }
    // Floor: two variations (Floor / Lobby).
    {
        Plugin floor("Floor.t2p", "../data/Plugins/Floor.t2p");
        OT::TemplateDef def;
        EXPECT(floor.getTemplateDef(def));
        EXPECT(def.tool.variations.size() == 2u);
        if (def.tool.variations.size() == 2u) {
            EXPECT(def.tool.variations[0].name == "Floor" && def.tool.variations[0].price == 5);
            EXPECT(def.tool.variations[1].name == "Lobby" && def.tool.variations[1].price == 50);
        }
        EXPECT(def.equip.variations.size() == 2u);
    }
    // Billboard (outer object): OODF with the 'BoPI' ad sub-plugin type.
    {
        Plugin bb("Billboard.t2p", "../data/Plugins/Billboard.t2p");
        PluginAttr a;
        EXPECT(bb.getAttr(a) && a.id.macStr() == "BILL" && a.itemType == 2100);
        OT::TemplateDef def;
        EXPECT(bb.getTemplateDef(def));
        EXPECT(def.kind == OT::TemplateDef::OuterObj);
        EXPECT(def.tool.toolType == 2000);
        EXPECT(def.tool.subPluginType.macStr() == "BoPI");
    }
    // Hotel room: depends on the Front desk.
    {
        Plugin room("Singleroom.t2p", "../data/Plugins/Singleroom.t2p");
        PluginAttr a;
        EXPECT(room.getAttr(a));
        EXPECT(a.dependID.macStr() == "FRON");
        EXPECT(a.hasDependency());
    }
    // World plugins: people types and string tables.
    {
        Plugin hawai("Hawai.t2p", "../data/Plugins/Hawai.t2p");
        OT::PeopleList people;
        EXPECT(hawai.getPeopleDefs(people));
        EXPECT(people.size() == 23u);
        if (!people.empty()) EXPECT(people[0].peopleType == 8 && people[0].silhouetteType == 300);
        OT::StringTableDef st;
        EXPECT(hawai.getStringTable(st, 8100) && st.size() == 6u);
        EXPECT(hawai.getStringTable(st, 1) && st.size() == 11u);   // STRTBL
        OT::StringBlock blk;
        EXPECT(hawai.getStringBlock(blk, 251));
        Plugin tokyo("Tokyo.t2p", "../data/Plugins/Tokyo.t2p");
        EXPECT(tokyo.getPeopleDefs(people) && people.size() == 25u);
    }
    // Text-form ATTR: DefaultToolPalette.
    {
        Plugin pal("DefaultToolPalette.t2p", "../data/Plugins/DefaultToolPalette.t2p");
        PluginAttr pa;
        EXPECT(pal.getAttr(pa));
        EXPECT(pa.isTextForm);
        EXPECT(pa.id.toUint32LE() == 1000u);
        EXPECT(pa.itemType == 1414484044);
        PluginDesc pd;
        EXPECT(pal.getDesc(pd) && pd.kind == PluginDesc::ToolPalette);
        OT::VersionInfo vi;
        EXPECT(pal.getVersionInfo(vi));
        EXPECT(vi.root.keyString() == "VS_VERSION_INFO");
    }
    // SP sub-plugins.
    {
        Plugin mv("MV_1stContact.t2p", "../data/Plugins/MV_1stContact.t2p");
        EXPECT(mv.container() == OT::ResourceContainer::SP);
        PluginDesc d;
        EXPECT(mv.getDesc(d) && d.kind == PluginDesc::MoviePlugin);
        PluginAttr a;
        EXPECT(mv.getAttr(a) && a.name == "1st Contact");
        OT::MovieDef m;
        EXPECT(mv.getMovieDef(m) && m.movieFile == "MV_1stContact.MOV");
        OT::MemberTableDef audience;
        EXPECT(mv.getMemberTable(audience, m.customerTableID));

        Plugin art("PlArt.t2p", "../data/Plugins/PlArt.t2p");
        EXPECT(art.getDesc(d) && d.kind == PluginDesc::HallEventPlugin);
        OT::HallEventDef h;
        EXPECT(art.getHallEventDef(h) && h.openTime == 600u && h.closeTime == 1020u);
        OT::SubPluginPeopleParamDef pp;
        EXPECT(art.getSubPluginPeopleParamDef(pp) && pp.values[1] == 2000);
        OT::PeopleList notPe;
        EXPECT(!art.getPeopleDefs(notPe));
        OT::DayParamDef day;
        EXPECT(art.getDayParamDef(day));

        Plugin ad("AirJamaica.t2p", "../data/Plugins/AirJamaica.t2p");
        EXPECT(ad.getDesc(d) && d.kind == PluginDesc::AdvertisePlugin);
        OT::AdvertiseDef adv;
        EXPECT(ad.getAdvertiseDef(adv) && adv.days == 12u && adv.money == 500);
    }

    // Broken plugin: accessors return false, no crash.
    Plugin broken("broken.t2p", "../data/Plugins/DoesNotExist.t2p");
    PluginAttr ba; PluginDesc bb; OT::DayParamDef bc; OT::TemplateDef bd;
    EXPECT(!broken.getAttr(ba));
    EXPECT(!broken.getDesc(bb));
    EXPECT(!broken.getDayParamDef(bc));
    EXPECT(!broken.getTemplateDef(bd));
    EXPECT(broken.getMemberTables().empty());
}

// Population sweep: every resource of every plugin in data/Plugins (105 PE +
// 51 SP) decodes and re-encodes byte-identically, and every SP container
// re-encodes byte-identically. Prints per-type counts.
static void testPluginPopulationSweep()
{
    using namespace OT;
    PluginManager pm;
    pm.addPluginDirectory("../data/Plugins");

    struct Count { int total = 0; int typed = 0; int identical = 0; std::map<std::string, int> codecs; };
    std::map<std::string, Count> byType;
    int pePlugins = 0, spPlugins = 0, resources = 0, identical = 0, spContainersOk = 0;
    std::vector<std::string> failures;

    for (const std::string & name : pm.enumerate())
    {
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".t2p") != 0) continue;
        AbstractPlugin * ap = pm.get(name);
        Plugin * p = dynamic_cast<Plugin *>(ap);
        if (!p || !p->ensureLoaded()) { failures.push_back(name + ": load"); continue; }
        const bool sp = p->container() == ResourceContainer::SP;
        (sp ? spPlugins : pePlugins)++;

        for (const AbstractPlugin::ResourceKey & k : p->enumerateResources())
        {
            const WindowsPEExecutable::Resource * r = p->getResource(k.type, k.id);
            if (!r) continue;
            const std::string original(r->data, size_t(r->length));
            DecodedResource d = decodeResource(k.type, k.id, r->data, size_t(r->length), p->container());
            std::string again;
            const bool ok = encodeResource(d, again) && again == original;
            const std::string tkey = (sp ? "SP:" : "") + resourceTypeDisplayName(k.type);
            Count & c = byType[tkey];
            ++c.total;
            ++resources;
            const bool typed = d.codec != ResourceCodec::Raw && d.codec != ResourceCodec::Text;
            if (typed) ++c.typed;
            ++c.codecs[resourceCodecName(d.codec)];
            if (ok) { ++c.identical; ++identical; }
            else failures.push_back(name + ": " + tkey + " " + std::to_string(k.id));
            // A type we have a codec for must not fall back to Raw.
            const ResourceCodec want = codecForResourceType(k.type, p->container());
            if (want != ResourceCodec::Raw && d.codec != want)
                failures.push_back(name + ": " + tkey + " " + std::to_string(k.id) + " not typed");
        }

        if (sp)
        {
            std::string bytes;
            SpContainer c;
            std::string again;
            if (readFileBytes(p->path().str(), bytes) && decodeSpContainer(bytes.data(), bytes.size(), c)
                && encodeSpContainer(c, again) && again == bytes
                && c.entries.size() == p->enumerateResources().size())
                ++spContainersOk;
            else
                failures.push_back(name + ": SP container");
        }
    }

    std::cout << "Plugin population sweep: " << pePlugins << " PE + " << spPlugins << " SP plugins, "
              << resources << " resources, " << identical << " byte-identical, "
              << spContainersOk << " SP containers byte-identical\n";
    int typedPairs = 0;
    for (const auto & kv : byType)
    {
        std::cout << "  " << kv.first << ": " << kv.second.identical << "/" << kv.second.total
                  << " identical, " << kv.second.typed << " typed (";
        bool first = true;
        for (const auto & cc : kv.second.codecs)
        {
            std::cout << (first ? "" : ", ") << cc.first << " " << cc.second;
            first = false;
        }
        std::cout << ")\n";
        if (kv.second.typed) typedPairs += kv.second.typed;
    }
    std::cout << "  typed resources: " << typedPairs << "\n";
    for (const std::string & f : failures) std::cerr << "  sweep failure: " << f << "\n";

    EXPECT(pePlugins == 105);
    EXPECT(spPlugins == 51);
    EXPECT(resources == 2983 + 349);   // PE (t2p_schema.py) + SP (world_sp.py) resources
    EXPECT(identical == resources);
    EXPECT(spContainersOk == 51);
    EXPECT(failures.empty());
    // Every typed type is fully typed (no fallbacks) across the population.
    EXPECT(byType["TNDF"].typed == 93);
    EXPECT(byType["MVDF"].typed == 7);
    EXPECT(byType["OODF"].typed == 1);
    EXPECT(byType["TMDF"].typed == 148);
    EXPECT(byType["ATTR"].typed == 105);
    EXPECT(byType["RT_VERSION"].typed == 105);
    EXPECT(byType["SP:MODF"].typed == 38);
    EXPECT(byType["SP:HEDF"].typed == 6);
    EXPECT(byType["SP:ADDF"].typed == 7);
}

// World resource encoders: YootWorld's typed encoders and the lossless
// WorldResourceCodec both reproduce every world resource of Tokyo, Hawai and
// Kegon byte for byte, and an edited Json record re-encodes to what the
// game-side parser then reads back.
static void testYootWorldEncoders()
{
    using namespace OT;
    const char * files[3] = {"Tokyo.t2p", "Hawai.t2p", "Kegon.t2p"};
    const YootWorld::Id ids[3] = {YootWorld::Id::Tokyo, YootWorld::Id::Hawaii, YootWorld::Id::Kegon};
    int typedEncoded = 0, codecResources = 0, codecIdentical = 0, events = 0;
    for (int w = 0; w < 3; ++w)
    {
        Plugin p(files[w], std::string("../data/Plugins/") + files[w]);
        EXPECT(p.ensureLoaded());
        YootWorld world;
        EXPECT(loadYootWorld(p, ids[w], world));

        auto same = [&](const std::string & type, int id, const std::string & bytes) {
            const WindowsPEExecutable::Resource * r = p.getResource(type, id);
            return r && bytes == std::string(r->data, size_t(r->length));
        };
        std::string b;
        EXPECT(encodeYootWorldDef(world, b) && same("WORLDDEF", 1, b));
        EXPECT(encodeYootWatr(world, b) && same("WATR", 128, b));
        typedEncoded += 2;
        if (p.getResource("FINF", 1))
        {
            EXPECT(encodeYootFinf(world, b) && same("FINF", 1, b));
            ++typedEncoded;
        }
        EXPECT(encodeYootTowerEvents(world.events, b) && same("T2EV", 1, b));
        ++typedEncoded;
        for (const YootEvent & e : world.events)
        {
            if (!e.found) continue;
            const bool ok = encodeYootEventResource(e, b) && same(e.resType, e.resId, b);
            if (!ok) std::cerr << "  event mismatch: " << files[w] << " " << e.resType << " " << e.resId << "\n";
            EXPECT(ok);
            ++typedEncoded;
            ++events;
        }

        // Lossless codec: every world-family resource decodes typed.
        for (const AbstractPlugin::ResourceKey & k : p.enumerateResources())
        {
            const ResourceCodec want = codecForResourceType(k.type, ResourceContainer::PE);
            if (want != ResourceCodec::WorldRecord && want != ResourceCodec::BkgndInfo
                && want != ResourceCodec::FloorInfoMask && want != ResourceCodec::PaletteAnime
                && want != ResourceCodec::Palette)
                continue;
            const WindowsPEExecutable::Resource * r = p.getResource(k.type, k.id);
            DecodedResource d = decodeResource(k.type, k.id, r->data, size_t(r->length));
            std::string again;
            ++codecResources;
            const bool ok = d.codec == want && encodeResource(d, again)
                && again == std::string(r->data, size_t(r->length));
            if (!ok) std::cerr << "  world codec mismatch: " << files[w] << " " << k.type << " " << k.id << "\n";
            if (ok) ++codecIdentical;
        }
    }
    std::cout << "World encoders: " << typedEncoded << " YootWorld resources (" << events
              << " events) and " << codecIdentical << "/" << codecResources
              << " WorldResourceCodec resources byte-identical\n";
    EXPECT(codecIdentical == codecResources);
    EXPECT(codecResources > 60);

    // Spot checks on the typed text codecs.
    {
        Plugin tokyo("Tokyo.t2p", "../data/Plugins/Tokyo.t2p");
        DecodedResource binf;
        EXPECT(tokyo.decodeResource("BINF", 1, binf) || tokyo.decodeResource("BINF", 128, binf));
        if (binf.codec == ResourceCodec::BkgndInfo)
        {
            const BkgndInfoDef & d = std::get<BkgndInfoDef>(binf.value);
            EXPECT(d.rows == 61 && d.cols == 46 && d.cells.size() == 61u * 46u);
        }
        DecodedResource watr;
        EXPECT(tokyo.decodeResource("WATR", 128, watr) && watr.codec == ResourceCodec::WorldRecord);
        if (watr.codec == ResourceCodec::WorldRecord)
        {
            Json rec = std::get<Json>(watr.value);
            EXPECT(rec["initialFunds"].asInt() == 100000 && rec["moneyUnit"].asString() == "$");
            rec.set("initialFunds", Json(123456));
            std::string bytes;
            EXPECT(encodeWorldSchemaResource(WorldSchema::Watr, "WATR", rec, bytes));
            YootWorld w;
            EXPECT(parseYootWatr(bytes.data(), int(bytes.size()), w) && w.initialFunds == 123456);
            // A wrong-typed field is rejected with its path.
            rec.set("exchangeMul", Json("x"));
            std::string err;
            EXPECT(!encodeWorldSchemaResource(WorldSchema::Watr, "WATR", rec, bytes, &err) && err.find("exchangeMul") != std::string::npos);
        }
        DecodedResource wd;
        EXPECT(tokyo.decodeResource("WORLDDEF", 1, wd) && wd.codec == ResourceCodec::WorldRecord);
        if (wd.codec == ResourceCodec::WorldRecord)
        {
            Json rec = std::get<Json>(wd.value);
            EXPECT(rec["grades"].size() == 4 && rec["cashAmounts"].size() == 8);
            EXPECT(rec["buildArea"]["left"].asInt() == 36 && rec["buildArea"]["bottom"].asInt() == 120);
            // Removing a cash step re-derives numCash.
            rec.find("cashAmounts")->elements().pop_back();
            std::string bytes;
            EXPECT(encodeWorldSchemaResource(WorldSchema::WorldDef, "WORLDDEF", rec, bytes));
            YootWorld w;
            EXPECT(parseYootWorldDef(bytes.data(), int(bytes.size()), w) && w.cashAmounts.size() == 7);
        }
    }
    {
        Plugin kegon("Kegon.t2p", "../data/Plugins/Kegon.t2p");
        DecodedResource finf;
        EXPECT(kegon.decodeResource("FINF", 1, finf) && finf.codec == ResourceCodec::FloorInfoMask);
        if (finf.codec == ResourceCodec::FloorInfoMask)
        {
            const FloorInfoMaskDef & d = std::get<FloorInfoMaskDef>(finf.value);
            EXPECT(d.height == 44 && d.width == 192);
            int set = 0;
            for (int32_t v : d.cells) set += v != 0;
            EXPECT(set == 5849);
            FloorInfoMaskDef back;
            EXPECT(floorInfoMaskFromJson(floorInfoMaskToJson(d), back) && back.cells == d.cells);
        }
        DecodedResource ptdf;
        EXPECT(kegon.decodeResource("PTDF", 1, ptdf) || kegon.decodeResource("PTDF", 128, ptdf));
        if (ptdf.codec == ResourceCodec::PaletteAnime)
            EXPECT(std::get<PaletteAnimeDef>(ptdf.value).fields.size() == 8);
    }
}

// OSP v3 sweep: every one of the 156 shipped plugins goes t2p -> OSP v3 on
// disk -> loaded back, and every resource re-encodes to the original bytes
// (typed Json, PNG + palette sidecar, .wav, text and raw files alike). The
// export of the reloaded plugin to .t2p is then byte-identical to the input
// file, PE and SP. The PE image is not consulted for the resource check: the
// loaded resources are compared against the original Plugin.
static void testOspV3Sweep()
{
    using namespace OT;
    PluginManager pm;
    pm.addPluginDirectory("../data/Plugins");
    const std::string tmp = "osp_v3_sweep";
    std::system(("rm -rf " + tmp).c_str());
    int plugins = 0, saved = 0, resources = 0, identical = 0, exportsIdentical = 0, typed = 0;
    int bitmapsPng = 0, waves = 0;
    std::vector<std::string> failures;
    for (const std::string & name : pm.enumerate())
    {
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".t2p") != 0) continue;
        Plugin * p = dynamic_cast<Plugin *>(pm.get(name));
        if (!p || !p->ensureLoaded()) { failures.push_back(name + ": load"); continue; }
        ++plugins;
        OspPlugin osp;
        if (!convertT2pToOsp(*p, osp).ok) { failures.push_back(name + ": convert"); continue; }
        const std::string dir = tmp + "/" + name.substr(0, name.size() - 4) + ".osp-plugin";
        const OspResult sr = saveOspPlugin(dir, osp, true);
        if (!sr.ok) { failures.push_back(name + ": save: " + sr.message); continue; }
        OspPlugin back;
        const OspResult lr = loadOspPlugin(dir, back);
        if (!lr.ok) { failures.push_back(name + ": load: " + lr.message); continue; }
        ++saved;

        // Original bytes per (type, id), in container order.
        std::vector<std::pair<std::pair<std::string, int>, std::string>> originals;
        if (p->container() == ResourceContainer::SP)
        {
            std::string bytes;
            SpContainer c;
            if (readFileBytes(p->path().str(), bytes) && decodeSpContainer(bytes.data(), bytes.size(), c))
                for (const SpContainer::Entry & e : c.entries)
                    originals.push_back({{e.typeName(), int(e.id)}, e.data});
        }
        else
        {
            for (const AbstractPlugin::ResourceKey & k : p->enumerateResources())
                if (const WindowsPEExecutable::Resource * r = p->getResource(k.type, k.id))
                    originals.push_back({{k.type, k.id}, std::string(r->data, size_t(r->length))});
        }
        if (originals.size() != back.resources.size())
            failures.push_back(name + ": resource count " + std::to_string(back.resources.size()));
        for (size_t i = 0; i < originals.size() && i < back.resources.size(); ++i)
        {
            const OspResource & r = back.resources[i];
            std::string again;
            ++resources;
            const bool ok = r.res.type == originals[i].first.first && r.res.id == originals[i].first.second
                && encodeResource(r.res, again) && again == originals[i].second;
            if (ok) ++identical;
            else failures.push_back(name + ": " + resourceTypeDisplayName(originals[i].first.first) + " "
                                    + std::to_string(originals[i].first.second));
            if (r.res.codec != ResourceCodec::Raw && r.res.codec != ResourceCodec::Text) ++typed;
        }
        std::string file;
        readFileBytes(p->path().str(), file);
        std::string exported, err;
        if (exportOspToT2p(back, exported, &err) && exported == file) ++exportsIdentical;
        else failures.push_back(name + ": export " + err);
    }
    // File census of the written store.
    {
        FILE * f = popen(("find " + tmp + " -path '*/bitmaps/*.png' | wc -l; find " + tmp + " -name '*.wav' | wc -l").c_str(), "r");
        if (f) { if (std::fscanf(f, "%d %d", &bitmapsPng, &waves) != 2) bitmapsPng = waves = -1; pclose(f); }
    }
    std::cout << "OSP v3 sweep: " << plugins << " plugins, " << saved << " saved+reloaded, "
              << identical << "/" << resources << " resources byte-identical (" << typed << " typed Json, "
              << bitmapsPng << " bitmaps as PNG, " << waves << " .wav), "
              << exportsIdentical << " .t2p exports byte-identical\n";
    for (size_t i = 0; i < failures.size() && i < 40; ++i) std::cerr << "  v3 sweep failure: " << failures[i] << "\n";
    EXPECT(plugins == 156);
    EXPECT(saved == 156);
    EXPECT(resources == 2983 + 349);
    EXPECT(identical == resources);
    EXPECT(exportsIdentical == 156);
    EXPECT(bitmapsPng == 958 + 96);
    EXPECT(waves == 133);
    std::system(("rm -rf " + tmp).c_str());
}

// Edited .t2p export: Burger's name and price change; the rebuilt PE loads,
// carries the new values, and every other resource is unchanged.
static void testT2pExportEdited()
{
    using namespace OT;
    Plugin burger("Burger.t2p", "../data/Plugins/Burger.t2p");
    OspPlugin osp;
    EXPECT(convertT2pToOsp(burger, osp).ok);
    OspResource * attr = findOspResource(osp, "ATTR", 128);
    OspResource * tndf = findOspResource(osp, "TNDF", 1);
    EXPECT(attr && tndf);
    if (!attr || !tndf) return;
    std::get<PluginAttr>(attr->res.value).name = "Burger Palace Deluxe Edition";
    TemplateDef & def = std::get<TemplateDef>(tndf->res.value);
    def.tool.variations[0].price = 777;
    def.tool.variations[0].priceString = "$77700";
    // Grow a bitmap too so .rsrc changes size noticeably (and .reloc moves).
    OspResource * big = findOspResource(osp, "2", 1000);
    EXPECT(big != nullptr);
    if (big)
    {
        sf::Image img;
        Json side;
        EXPECT(dibToPng(big->res.bytes, img, side));
        sf::Image wide({img.getSize().x * 2, img.getSize().y}, sf::Color::Black);
        EXPECT(wide.copy(img, {0, 0}));
        std::string dib;
        EXPECT(replaceDibPixels(big->res.bytes, wide, dib));
        big->res.bytes = dib;
    }
    const std::string out = "Burger_edited_test.t2p";
    std::string err;
    EXPECT(writeT2pFile(osp, out, &err));
    Plugin edited("Burger_edited_test.t2p", out);
    EXPECT(edited.ensureLoaded());
    PluginAttr a;
    EXPECT(edited.getAttr(a) && a.name == "Burger Palace Deluxe Edition" && a.id == FourCC::fromMacOrder("BURG"));
    TemplateDef t;
    EXPECT(edited.getTemplateDef(t) && t.tool.variations[0].price == 777 && t.tool.variations[0].priceString == "$77700");
    sf::Image wideBack;
    const WindowsPEExecutable::Resource * wr = edited.getResource("2", 1000);
    EXPECT(wr && PluginBitmapLoader::decodeBitmapResource(wr->data, wr->length, wideBack) && wideBack.getSize().x == 2 * 128u);
    int same = 0, total = 0;
    for (const AbstractPlugin::ResourceKey & k : burger.enumerateResources())
    {
        if (k.type == "ATTR" || k.type == "TNDF" || (k.type == "2" && k.id == 1000)) continue;
        ++total;
        const WindowsPEExecutable::Resource * x = burger.getResource(k.type, k.id);
        const WindowsPEExecutable::Resource * y = edited.getResource(k.type, k.id);
        if (x && y && x->length == y->length && std::memcmp(x->data, y->data, size_t(x->length)) == 0) ++same;
    }
    EXPECT(total > 20 && same == total);
    // The relocations and the code survive: compare .text bytes and the
    // relocation table contents through the section table.
    std::string orig, now;
    EXPECT(readFileBytes("../data/Plugins/Burger.t2p", orig) && readFileBytes(out, now));
    EXPECT(now.size() > orig.size());
    EXPECT(now.compare(0x400, 0xe600, orig, 0x400, 0xe600) == 0);  // .text raw data
    std::remove(out.c_str());

    // Adding and removing resources switches to a full .rsrc rebuild (new
    // tree, canonical layout); everything else must still read back.
    {
        OspPlugin q;
        EXPECT(convertT2pToOsp(burger, q).ok);
        OspResource extra;
        extra.res.type = "OBJMAP";
        extra.res.id = 4242;
        extra.res.codec = ResourceCodec::Text;
        extra.res.bytes = "DefParts Test\r\n";
        q.resources.push_back(extra);
        OspResource extraType;
        extraType.res.type = "STRTABLE";
        extraType.res.id = 7;
        extraType.res.codec = ResourceCodec::StringTable;
        extraType.res.value = StringTableDef{"hello", "world"};
        q.resources.push_back(extraType);
        q.resources.erase(std::remove_if(q.resources.begin(), q.resources.end(), [](const OspResource & r) {
            return r.res.type == "WAVE";
        }), q.resources.end());
        const std::string out2 = "Burger_rebuilt_test.t2p";
        EXPECT(writeT2pFile(q, out2, &err));
        Plugin rebuilt("Burger_rebuilt_test.t2p", out2);
        EXPECT(rebuilt.ensureLoaded());
        int ok = 0, n = 0;
        for (const OspResource & r : q.resources)
        {
            ++n;
            std::string want;
            const WindowsPEExecutable::Resource * y = rebuilt.getResource(r.res.type, r.res.id);
            if (encodeResource(r.res, want) && y && want == std::string(y->data, size_t(y->length))) ++ok;
        }
        EXPECT(n > 20 && ok == n);
        EXPECT(rebuilt.getResource("WAVE", 165) == nullptr);
        StringTableDef st;
        EXPECT(rebuilt.getStringTable(st, 7) && st.size() == 2 && st[1] == "world");
        std::remove(out2.c_str());
    }
}

// ItemTypeCatalog: keyed by plugin id (ATTR mID) with the numeric item type.
static void testItemTypeCatalog()
{
    const OT::ItemTypeEntry * burger = OT::lookupPluginId("BURG");
    EXPECT(burger != nullptr);
    if (burger) {
        EXPECT(burger->itemType == 1301);
        EXPECT(std::string(burger->file) == "Burger");
        EXPECT(burger->id() == "GRUB");
    }
    EXPECT(OT::lookupPluginId(OT::FourCC::fromFileOrder("GRUB")) == burger);
    EXPECT(OT::lookupPluginId("burg") == burger);   // case-insensitive
    EXPECT(OT::lookupItemType(1301) == burger);
    const OT::ItemTypeEntry * elev = OT::lookupItemType(1810);
    EXPECT(elev && std::string(elev->cls) == "Mover" && elev->pluginId == OT::fourcc("StdE"));
    EXPECT(OT::lookupPluginId("ZZZZ") == nullptr);
    EXPECT(OT::lookupItemType(-5) == nullptr);

    // The catalog agrees with every PE plugin's ATTR.
    OT::PluginManager pm;
    pm.addPluginDirectory("../data/Plugins");
    int checked = 0, mismatches = 0;
    for (const OT::ItemTypeEntry & e : OT::kItemTypeCatalog)
    {
        OT::AbstractPlugin * p = pm.get(std::string(e.file) + ".t2p");
        OT::PluginAttr a;
        if (!p || !p->getAttr(a)) { ++mismatches; continue; }
        ++checked;
        if (a.id.toUint32LE() != e.pluginId || a.itemType != e.itemType || a.name != e.name) ++mismatches;
    }
    EXPECT(checked == 105);
    EXPECT(mismatches == 0);
}

// OSP manifests: v1 (legacy key names) still load and map onto the corrected
// fields; v2 round-trips the DPDF/PLDF tables.
static void testOspAttrCompat()
{
    using namespace OT;
    // A v1 manifest as the old editor wrote it for Burger: "itemType" was the
    // file-order mID, "id" the numeric item type, "marker" the dependency,
    // "placement"/"price" the DPDF/PLDF slots.
    const std::string v1 =
        "{\n  \"format\": \"openskyscraper-plugin\",\n  \"formatVersion\": 1,\n"
        "  \"class\": \"Tenant\",\n"
        "  \"attr\": {\n    \"itemType\": \"GRUB\",\n    \"name\": \"Burger land\",\n"
        "    \"gameLevel\": 1,\n    \"id\": 1301,\n    \"marker\": \"NORF\",\n"
        "    \"mustItem\": 0,\n    \"dependID\": 0,\n    \"secretItem\": 0\n  },\n"
        "  \"placement\": {\n    \"w\": 70,\n    \"h\": 50,\n    \"baseX\": 100,\n    \"baseY\": 80\n  },\n"
        "  \"price\": {\n    \"basePrice\": 1000,\n    \"f1\": 40,\n    \"f2\": 10,\n    \"f3\": 300,\n"
        "    \"f4\": 50,\n    \"f5\": 100\n  }\n}\n";
    PluginAttr a;
    EXPECT(readOspAttr(v1, a));
    EXPECT(a.id.macStr() == "BURG");
    EXPECT(a.itemType == 1301);
    EXPECT(a.dependID.macStr() == "FRON");
    EXPECT(a.name == "Burger land");

    const std::string dir = "test_osp_v1compat.osp-plugin";
    {
        std::string mk = "mkdir -p " + dir;
        (void)std::system(mk.c_str());
        std::ofstream(dir + "/plugin.json", std::ios::trunc) << v1;
    }
    OspPlugin p;
    // The v1 sample has no sprites; loadOspPlugin only requires sheets the
    // manifest declares, so it loads.
    const OspResult r = loadOspPlugin(dir, p);
    EXPECT(r.ok);
    if (r.ok) {
        EXPECT(p.attr.id.macStr() == "BURG" && p.attr.itemType == 1301);
        EXPECT(p.dayParam.has_value());
        if (p.dayParam) {
            EXPECT(p.dayParam->percent[0][0] == 70u && p.dayParam->percent[1][1] == 80u);
        }
        EXPECT(p.placeParam.has_value());
        if (p.placeParam) {
            EXPECT(p.placeParam->places[0].percent == 40u && p.placeParam->places[0].scoreSlot == 1000);
            EXPECT(p.placeParam->places[2].percent == 50u && p.placeParam->places[2].scoreSlot == 100);
        }
        // Save as v2 and reload: the new keys carry the same values.
        EXPECT(saveOspPlugin(dir, p, true).ok);
        std::string json;
        EXPECT(readFileBytes(dir + "/plugin.json", json));
        EXPECT(json.find("\"formatVersion\": 3") != std::string::npos);
        EXPECT(json.find("\"pluginId\": \"BURG\"") != std::string::npos);
        EXPECT(json.find("\"itemType\": 1301") != std::string::npos);
        EXPECT(json.find("\"dependPluginId\": \"FRON\"") != std::string::npos);
        EXPECT(json.find("\"percent\": [[70, 50], [100, 80]]") != std::string::npos);
        EXPECT(json.find("\"places\": [[40, 1000], [10, 300], [50, 100]]") != std::string::npos);
        OspPlugin q;
        EXPECT(loadOspPlugin(dir, q).ok);
        EXPECT(q.attr.id == p.attr.id && q.attr.itemType == 1301 && q.attr.dependID == p.attr.dependID);
        EXPECT(q.dayParam && q.dayParam->percent[1][0] == 100u);
        EXPECT(q.placeParam && q.placeParam->places[1].scoreSlot == 300);
    }
    std::remove((dir + "/plugin.json").c_str());
    std::remove((dir + "/meta/objmap_100.txt").c_str());
    std::remove((dir + "/meta/objmap_1000.txt").c_str());
    std::remove((dir + "/meta/objmap_200.txt").c_str());
}

// OBJMAP sprite-layout parser. Two layers:
//   (a) byte-level: a hand-crafted block reproducing the grammar, exercising
//       ID/Pattern/Span/List and the Rect + flags parsing.
//   (b) integration: Stair.t2p OBJMAP 1000 reproduces the spiral-staircase
//       calibration oracle (extract_t2p_stairs.py:11-13).
static void testPluginObjmap()
{
    using OT::ObjmapBlock;
    using OT::ObjmapRect;
    // Hand-crafted grammar sample with two blocks: a Csr master rect and a
    // two-state toolbox icon (unselected/selected).
    static const char kSample[] =
        "DefParts\tName\tCsr\r\n"
        "\t\tID\t100\r\n"
        "\t\tList\tRect 0,0,26,130 Halftone\r\n"
        "\r\n"
        "DefParts\tName\tCondo\r\n"
        "\t\tID\t101\r\n"
        "\t\tPattern\t0\r\n"
        "\t\tList\tRect 0,0,26,32 Transparent\r\n"
        "\r\n"
        "DefParts\tName\tCondo\r\n"
        "\t\tID\t101\r\n"
        "\t\tPattern\t1\r\n"
        "\t\tList\tRect 26,0,52,32 Transparent\r\n"
        "\r\n"
        "End\r\n";
    std::vector<ObjmapBlock> blocks;
    const bool ok = OT::parsePluginObjmap(kSample, int(sizeof(kSample) - 1), blocks);
    EXPECT(ok);
    if (ok) {
        EXPECT(blocks.size() == 3u);
        if (blocks.size() == 3) {
            EXPECT(blocks[0].name == "Csr");
            EXPECT(blocks[0].id == 100);
            EXPECT(blocks[0].rect == (ObjmapRect{0, 0, 26, 130}));
            EXPECT(blocks[0].flags.size() == 1u);
            EXPECT(blocks[0].flags[0] == "Halftone");
            EXPECT(blocks[1].name == "Condo");
            EXPECT(blocks[1].pattern == 0);
            EXPECT(blocks[1].rect == (ObjmapRect{0, 0, 26, 32}));
            EXPECT(blocks[2].pattern == 1);
            EXPECT(blocks[2].rect == (ObjmapRect{26, 0, 52, 32}));
        }
        // findObjmapBlock: locate the selected icon variant.
        ObjmapBlock * sel = OT::findObjmapBlock(blocks, "Condo", 1);
        EXPECT(sel != nullptr);
        if (sel) EXPECT(sel->rect.right == 52);
        EXPECT(OT::findObjmapBlock(blocks, "Condo", 99) == nullptr);
    }

    // Rejection: not an OBJMAP (no DefParts magic).
    std::vector<ObjmapBlock> bad;
    EXPECT(!OT::parsePluginObjmap(nullptr, 0, bad));
    EXPECT(!OT::parsePluginObjmap("not objmap", 10, bad));

    // --- Integration: Stair.t2p OBJMAP 1000 (the calibration oracle). ---
    OT::Plugin stair("Stair.t2p", "../data/Plugins/Stair.t2p");
    EXPECT(stair.ensureLoaded());
    const OT::WindowsPEExecutable::Resource * res = stair.getResource("OBJMAP", 1000);
    EXPECT(res != nullptr);
    if (res && res->data) {
        std::vector<ObjmapBlock> stairBlocks;
        EXPECT(OT::parsePluginObjmap(res->data, res->length, stairBlocks));
        // extract_t2p_stairs.py:11-13 knows three spiral-staircase spans:
        //   Span 2: Rect(0,0,64,60)   Span 3: Rect(0,60,64,156)
        //   Span 4: Rect(0,156,64,288)
        // Find them by span id (names are "View").
        auto bySpan = [&](int s) -> const ObjmapBlock * {
            for (const auto & b : stairBlocks) if (b.span == s) return &b;
            return nullptr;
        };
        const ObjmapBlock * s2 = bySpan(2);
        const ObjmapBlock * s3 = bySpan(3);
        const ObjmapBlock * s4 = bySpan(4);
        EXPECT(s2 != nullptr); EXPECT(s3 != nullptr); EXPECT(s4 != nullptr);
        if (s2) EXPECT(s2->rect == (ObjmapRect{0, 0, 64, 60}));
        if (s3) EXPECT(s3->rect == (ObjmapRect{0, 60, 64, 156}));
        if (s4) EXPECT(s4->rect == (ObjmapRect{0, 156, 64, 288}));
    }

    // Condo.t2p OBJMAP 100: the toolbox-icon block set. Per objmap-format.md
    // every gameplay item carries exactly 5 blocks (3 Category + 2 item).
    OT::Plugin condo("Condo.t2p", "../data/Plugins/Condo.t2p");
    EXPECT(condo.ensureLoaded());
    const OT::WindowsPEExecutable::Resource * res100 = condo.getResource("OBJMAP", 100);
    EXPECT(res100 != nullptr);
    if (res100 && res100->data) {
        std::vector<ObjmapBlock> condoIcons;
        EXPECT(OT::parsePluginObjmap(res100->data, res100->length, condoIcons));
        EXPECT(condoIcons.size() == 5u);
        int cats = 0;
        for (const auto & b : condoIcons) if (b.name == "Category") ++cats;
        EXPECT(cats == 3);
    }
}

// parse -> write -> parse must be an identity for the attributes we model
// (name, id, pattern, span, rect, flags). This is the round-trip guarantee
// the OSP writer relies on when emitting meta/objmap_*.txt.
static void testObjmapRoundTrip()
{
    using OT::ObjmapBlock;
    using OT::ObjmapRect;
    std::vector<ObjmapBlock> original;
    ObjmapBlock a; a.name = "Csr"; a.id = 100;
    a.rect = {0, 0, 26, 130}; a.flags = {"Halftone"};
    original.push_back(a);
    ObjmapBlock b; b.name = "View"; b.id = 100; b.pattern = 101;
    b.rect = {0, 0, 128, 24}; b.flags = {"Transparent"};
    original.push_back(b);
    ObjmapBlock c; c.name = "View"; c.span = 3;
    c.rect = {0, 60, 64, 156};
    original.push_back(c);

    const std::string text = OT::writePluginObjmap(original);
    std::vector<ObjmapBlock> reparsed;
    EXPECT(OT::parsePluginObjmap(text.data(), int(text.size()), reparsed));
    if (reparsed.size() == original.size()) {
        for (size_t i = 0; i < original.size(); ++i) {
            EXPECT(reparsed[i].name == original[i].name);
            EXPECT(reparsed[i].id == original[i].id);
            EXPECT(reparsed[i].pattern == original[i].pattern);
            EXPECT(reparsed[i].span == original[i].span);
            EXPECT(reparsed[i].rect == original[i].rect);
            EXPECT(reparsed[i].flags == original[i].flags);
        }
    } else {
        EXPECT(reparsed.size() == original.size());
    }
}


// OSP round-trip: convert Condo.t2p -> OspPlugin -> save to a temp dir ->
// reload -> the metadata, sprite dimensions, and OBJMAP block counts must
// survive the round trip. This is the editor's core correctness guarantee.
static void testOspRoundTrip()
{
    using namespace OT;
    // Source: a real vendored Tenant plugin.
    Plugin condo("Condo.t2p", "../data/Plugins/Condo.t2p");
    EXPECT(condo.ensureLoaded());

    OspPlugin p;
    const OspResult cr = convertT2pToOsp(condo, p);
    EXPECT(cr.ok);
    if (!cr.ok) return;

    EXPECT(p.sourceFormat == "yoottower-t2p");
    EXPECT(p.sourceFile == "Condo.t2p");
    EXPECT(p.pluginClass == PluginDesc::TenantPlugin);
    EXPECT(p.attr.valid);
    EXPECT(p.attr.id == "TRPA");
    EXPECT(p.attr.itemType == 1101);
    EXPECT(p.attr.name == "Condo");
    // Condo ships a toolbox sheet (100), primary sheet (1000), and resident
    // overlay (1100). The known magenta/white masks come from YootCondo.cpp.
    EXPECT(p.toolbox.present);
    EXPECT(p.ingame.present);
    EXPECT(p.occupants.present);
    EXPECT(p.ingame.mask.has_value());
    if (p.ingame.mask) EXPECT(*p.ingame.mask == sf::Color(105, 33, 104));
    if (p.occupants.mask) EXPECT(*p.occupants.mask == sf::Color::White);
    // Condo OBJMAP 100 has 5 blocks (3 Category + 2 item).
    EXPECT(p.toolbox.blocks.size() == 5u);

    // Save to a throwaway temp dir, then reload.
    const std::string tmpDir = "test_osp_roundtrip.osp-plugin";
    // Clean a previous run so the test is repeatable.
    if (isOspPluginDir(tmpDir)) {
        // best-effort wipe of generated files
        std::remove((tmpDir + "/plugin.json").c_str());
        std::remove((tmpDir + "/sprites/100.png").c_str());
        std::remove((tmpDir + "/sprites/1000.png").c_str());
        std::remove((tmpDir + "/sprites/1100.png").c_str());
        std::remove((tmpDir + "/meta/objmap_100.txt").c_str());
        std::remove((tmpDir + "/meta/objmap_1000.txt").c_str());
    }
    const OspResult sr = saveOspPlugin(tmpDir, p, true);
    EXPECT(sr.ok);
    if (!sr.ok) return;
    EXPECT(isOspPluginDir(tmpDir));

    OspPlugin reloaded;
    const OspResult lr = loadOspPlugin(tmpDir, reloaded);
    EXPECT(lr.ok);
    if (!lr.ok) return;

    // Metadata round-trips.
    EXPECT(reloaded.sourceFormat == "openskyscraper-plugin");
    EXPECT(reloaded.sourceFile == "Condo.t2p");
    EXPECT(reloaded.pluginClass == PluginDesc::TenantPlugin);
    EXPECT(reloaded.attr.id == "TRPA");
    EXPECT(reloaded.attr.name == "Condo");
    EXPECT(reloaded.attr.itemType == p.attr.itemType);
    EXPECT(reloaded.attr.attrib == p.attr.attrib);
    // Sprite dimensions survive PNG encode/decode.
    EXPECT(reloaded.ingame.image.getSize() == p.ingame.image.getSize());
    EXPECT(reloaded.toolbox.present && reloaded.ingame.present && reloaded.occupants.present);
    // OBJMAP block count survives parse->write->parse (already covered) + save.
    EXPECT(reloaded.toolbox.blocks.size() == 5u);

    // Refuse to load a higher format version (forward-compat guard).
    {
        const std::string badDir = "test_osp_badversion.osp-plugin";
        saveOspPlugin(badDir, p, true);
        std::string json;
        std::ifstream f(badDir + "/plugin.json");
        std::ostringstream ss; ss << f.rdbuf(); json = ss.str();
        // Bump the version number in-place.
        size_t pos = json.find("\"formatVersion\":");
        if (pos != std::string::npos) {
            json.replace(pos, std::string("\"formatVersion\":").size(), "\"formatVersion\": 999,");
            std::ofstream(badDir + "/plugin.json", std::ios::trunc) << json;
            OspPlugin dummy;
            OspResult r = loadOspPlugin(badDir, dummy);
            EXPECT(!r.ok);  // higher version must be rejected
        }
        std::remove((badDir + "/plugin.json").c_str());
        std::remove((badDir + "/sprites/100.png").c_str());
        std::remove((badDir + "/sprites/1000.png").c_str());
        std::remove((badDir + "/sprites/1100.png").c_str());
        std::remove((badDir + "/meta/objmap_100.txt").c_str());
        std::remove((badDir + "/meta/objmap_1000.txt").c_str());
    }
}

// Mover-class round trip: Stair.t2p -> OspPlugin -> save -> reload. Proves the
// Mover slice end to end: DESC class is preserved, MVDF transport pricing
// survives, the walking-frames sheet (RT_BITMAP 200 + OBJMAP 200) round-trips.
static void testOspRoundTripMover()
{
    using namespace OT;
    Plugin stair("Stair.t2p", "../data/Plugins/Stair.t2p");
    EXPECT(stair.ensureLoaded());

    OspPlugin p;
    const OspResult cr = convertT2pToOsp(stair, p);
    EXPECT(cr.ok);
    if (!cr.ok) return;

    EXPECT(p.pluginClass == PluginDesc::MoverPlugin);
    EXPECT(p.attr.id == "RATS");
    EXPECT(p.attr.name == "Stairs");
    // MVDF transport pricing came through.
    EXPECT(p.transport.valid);
    EXPECT(p.transport.priceString == "$5000");
    EXPECT(p.transport.name == "Stairs");
    // Walking-frames sheet + OBJMAP 200 (the stair SHIL) are present.
    EXPECT(p.walking.present);
    EXPECT(!p.walking.blocks.empty());
    // Stairs have no PLDF (transport uses MVDF) and no occupants sheet.
    EXPECT(!p.placeParam.has_value());

    const std::string tmpDir = "test_osp_mover.osp-plugin";
    const OspResult sr = saveOspPlugin(tmpDir, p, true);
    EXPECT(sr.ok);
    if (!sr.ok) return;

    OspPlugin reloaded;
    const OspResult lr = loadOspPlugin(tmpDir, reloaded);
    EXPECT(lr.ok);
    if (!lr.ok) return;

    EXPECT(reloaded.pluginClass == PluginDesc::MoverPlugin);
    EXPECT(reloaded.attr.name == "Stairs");
    EXPECT(reloaded.transport.valid);
    EXPECT(reloaded.transport.priceString == "$5000");
    EXPECT(reloaded.walking.present);
    EXPECT(reloaded.walking.image.getSize() == p.walking.image.getSize());
    // OBJMAP 200 block count survives the parse->write->parse round trip.
    EXPECT(reloaded.walking.blocks.size() == p.walking.blocks.size());

    // cleanup
    std::remove((tmpDir + "/plugin.json").c_str());
    std::remove((tmpDir + "/sprites/100.png").c_str());
    std::remove((tmpDir + "/sprites/1000.png").c_str());
    std::remove((tmpDir + "/sprites/200.png").c_str());
    std::remove((tmpDir + "/meta/objmap_100.txt").c_str());
    std::remove((tmpDir + "/meta/objmap_1000.txt").c_str());
    std::remove((tmpDir + "/meta/objmap_200.txt").c_str());
}


// Shipped OpenTower folder plugins use numeric sprite naming
// (sprites/1000.png) with no manifest relPath. The OSP loader must resolve
// them via the manifest -> canonical -> numeric fallback chain, so every
// shipped data/Plugins/OpenTower plugin stays loadable (and editable by the
// PluginEditor). Dimensions pin the build_opentower_sheets.py calibration.
static void testOspShippedOpenTower()
{
    using namespace OT;
    OspPlugin condo;
    const OspResult cr = loadOspPlugin("../data/Plugins/OpenTower/Condo", condo);
    EXPECT(cr.ok);
    if (cr.ok) {
        EXPECT(condo.ingame.present);
        EXPECT(condo.ingame.relPath == "sprites/1000.png");
        EXPECT(condo.ingame.image.getSize().x == 640u);
        EXPECT(condo.ingame.image.getSize().y == 72u);
        EXPECT(!condo.toolbox.present);  // shipped Condo has no toolbox sheet
    }

    OspPlugin office;
    const OspResult orr = loadOspPlugin("../data/Plugins/OpenTower/Office", office);
    EXPECT(orr.ok);
    if (orr.ok) {
        EXPECT(office.ingame.present);
        EXPECT(office.ingame.relPath == "sprites/1000.png");
        EXPECT(office.ingame.image.getSize().x == 144u);
        EXPECT(office.ingame.image.getSize().y == 168u);
    }
}

// Saving a shipped numeric-named plugin must not orphan its sheet: the
// writer records the resolved relPath in the manifest and writes the pixels
// back to sprites/1000.png, so the save stays loadable by both the editor
// and the game (DecompiledPlugin reads sprites/1000.png).
static void testOspNumericRoundTrip()
{
    using namespace OT;
    OspPlugin condo;
    const OspResult cr = loadOspPlugin("../data/Plugins/OpenTower/Condo", condo);
    EXPECT(cr.ok);
    if (!cr.ok) return;

    const std::string tmpDir = "test_osp_numeric.osp-plugin";
    // best-effort wipe of a previous run
    std::remove((tmpDir + "/plugin.json").c_str());
    std::remove((tmpDir + "/sprites/1000.png").c_str());
    std::remove((tmpDir + "/sprites/ingame.png").c_str());
    std::remove((tmpDir + "/meta/objmap_1000.txt").c_str());

    const OspResult sr = saveOspPlugin(tmpDir, condo, true);
    EXPECT(sr.ok);
    if (!sr.ok) return;

    // The manifest declares the numeric path inside the ingame entry.
    {
        std::ifstream f(tmpDir + "/plugin.json");
        std::ostringstream ss; ss << f.rdbuf();
        const std::string json = ss.str();
        EXPECT(json.find("\"relPath\": \"sprites/1000.png\"") != std::string::npos);
    }

    OspPlugin reloaded;
    const OspResult lr = loadOspPlugin(tmpDir, reloaded);
    EXPECT(lr.ok);
    if (!lr.ok) return;
    EXPECT(reloaded.ingame.present);
    EXPECT(reloaded.ingame.relPath == "sprites/1000.png");
    EXPECT(reloaded.ingame.image.getSize().x == 640u);
    EXPECT(reloaded.ingame.image.getSize().y == 72u);

    // cleanup
    std::remove((tmpDir + "/plugin.json").c_str());
    std::remove((tmpDir + "/sprites/1000.png").c_str());
    std::remove((tmpDir + "/meta/objmap_1000.txt").c_str());
}

// Backwards compatibility: a plugin that still uses the canonical sheet
// names (sprites/ingame.png) must keep loading — both with the relPath
// declared in the manifest and via the file-existence fallback when the
// manifest omits it.
static void testOspCanonicalSheetNames()
{
    using namespace OT;
    // Hand-crafted doc using the canonical pre-numeric naming.
    OspPlugin p;
    p.sourceFormat = "scratch";
    p.pluginClass = PluginDesc::TenantPlugin;
    p.attr.valid = true;
    p.attr.id = FourCC::fromFileOrder("TEST");
    p.attr.name = "Canonical";
    p.ingame.present = true;
    p.ingame.relPath = "sprites/ingame.png";
    p.ingame.image = sf::Image({4u, 4u}, sf::Color::Red);

    const std::string tmpDir = "test_osp_canonical.osp-plugin";
    // best-effort wipe of a previous run
    std::remove((tmpDir + "/plugin.json").c_str());
    std::remove((tmpDir + "/sprites/ingame.png").c_str());
    std::remove((tmpDir + "/sprites/1000.png").c_str());
    std::remove((tmpDir + "/meta/objmap_1000.txt").c_str());

    const OspResult sr = saveOspPlugin(tmpDir, p, true);
    EXPECT(sr.ok);
    if (!sr.ok) return;

    // Declared relPath: the canonical name loads.
    {
        OspPlugin reloaded;
        const OspResult lr = loadOspPlugin(tmpDir, reloaded);
        EXPECT(lr.ok);
        if (lr.ok) {
            EXPECT(reloaded.ingame.present);
            EXPECT(reloaded.ingame.relPath == "sprites/ingame.png");
            EXPECT(reloaded.ingame.image.getSize().x == 4u);
            EXPECT(reloaded.ingame.image.getSize().y == 4u);
        }
    }

    // Undeclared relPath: strip it from the manifest so the loader must
    // fall back to the canonical name on file existence alone.
    {
        std::ifstream rf(tmpDir + "/plugin.json");
        std::ostringstream rs; rs << rf.rdbuf();
        std::string json = rs.str();
        const std::string needle = "\"relPath\": \"sprites/ingame.png\", ";
        const size_t at = json.find(needle);
        EXPECT(at != std::string::npos);
        if (at != std::string::npos) json.erase(at, needle.size());
        std::ofstream(tmpDir + "/plugin.json", std::ios::trunc) << json;

        OspPlugin reloaded;
        const OspResult lr = loadOspPlugin(tmpDir, reloaded);
        EXPECT(lr.ok);
        if (lr.ok) {
            EXPECT(reloaded.ingame.present);
            EXPECT(reloaded.ingame.relPath == "sprites/ingame.png");
        }
    }

    // cleanup
    std::remove((tmpDir + "/plugin.json").c_str());
    std::remove((tmpDir + "/sprites/ingame.png").c_str());
    std::remove((tmpDir + "/meta/objmap_1000.txt").c_str());
}


// ---- TDT import fixtures ------------------------------------------------
// Builds a minimal but structurally complete 1994 .TDT save in memory:
// header, 120 floor records (tenants on several), empty people/retail blocks,
// one built standard elevator + 23 empty slots, finance/parking, and a
// 2-flight stairs table. Offsets follow doc/simtower/TDT_format.txt; the
// ground floor must land exactly at 0x9C4 (560 + 10 x 194), which the lobby
// assertion below double-checks via the converted coordinates.
static void put8(std::vector<uint8_t> & b, int v) { b.push_back((uint8_t)(v & 0xFF)); }
static void put16(std::vector<uint8_t> & b, int v) { put8(b, v); put8(b, v >> 8); }
static void put32(std::vector<uint8_t> & b, long v)
{
	put8(b, (int)(v & 0xFF)); put8(b, (int)((v >> 8) & 0xFF));
	put8(b, (int)((v >> 16) & 0xFF)); put8(b, (int)((v >> 24) & 0xFF));
}
static void putZeros(std::vector<uint8_t> & b, size_t n)
{
	for (size_t i = 0; i < n; i++) put8(b, 0);
}
static void putTenant(std::vector<uint8_t> & b, int left, int right, int type, int variant)
{
	put16(b, left); put16(b, right); put8(b, type); put8(b, 0); put8(b, variant);
	putZeros(b, 9); put8(b, 2); put8(b, 0); // rentClass 2, reserved byte
}
static std::vector<uint8_t> buildTdtFixture()
{
	std::vector<uint8_t> b;
	// Header (560 bytes).
	put16(b, 0x2400);      // version magic
	put16(b, 5);           // level: 5 stars
	put32(b, 946059L);     // balance: $100 units -> $94,605,900
	putZeros(b, 12);       // otherIncome, constructionCosts, lastQuarterMoney
	put16(b, 0);           // frameTime: 0 = 7:00 AM in TDT terms
	put32(b, 150);         // 1st WD / 3Q / year 13
	put16(b, 0);           // unknown
	put16(b, 0);           // lobbyHeight?
	putZeros(b, 8);        // unknown x8
	put16(b, 0); put16(b, 0); // lrPixel, udPixel
	putZeros(b, 518);      // unknown tail of the header
	// Floor map: 120 records, mostly empty (194 bytes each).
	for (int f = 0; f < 120; f++)
	{
		std::vector<uint8_t> tenants;
		if (f == 10) // ground floor: lobby + fastfood variant 3 + office under construction
		{
			putTenant(tenants, 180, 200, 24, 0);
			putTenant(tenants, 100, 104, 12, 3);
			putTenant(tenants, 0, 9, -7, 0);
		}
		else if (f == 11) // offices + retail shop + housekeeping + Steak House restaurant
		{
			putTenant(tenants, 100, 109, 7, 0);
			putTenant(tenants, 150, 160, 10, 0);
			putTenant(tenants, 170, 175, 15, 0);
			putTenant(tenants, 120, 129, 6, 4);
		}
		else if (f == 14) // parking ramp + burned area
		{
			putTenant(tenants, 40, 56, 44, 0);
			putTenant(tenants, 20, 30, 48, 0);
		}
		else if (f == 16) putTenant(tenants, 80, 105, 20, 0); // recycling part 1
		else if (f == 17) putTenant(tenants, 80, 105, 21, 0); // recycling part 2
		else if (f == 18) putTenant(tenants, 90, 100, 45, 0); // ground strips (stay skipped)
		else if (f == 12) putTenant(tenants, 60, 91, 18, 0); // cinema top half
		else if (f == 13) putTenant(tenants, 60, 91, 19, 0); // cinema bottom half
		else if (f == 20) putTenant(tenants, 50, 79, 31, 0); // metro part 1
		else if (f == 21) putTenant(tenants, 50, 79, 32, 0); // metro part 2
		else if (f == 22) putTenant(tenants, 50, 79, 33, 0); // metro part 3
		else if (f >= 109 && f <= 113) putTenant(tenants, 200, 228, 36 + f - 109, 0); // cathedral: 28x5
		put16(b, (int)(tenants.size() / 18));
		put16(b, 0); put16(b, 200);
		b.insert(b.end(), tenants.begin(), tenants.end());
		putZeros(b, 94 * 2); // index map
	}
	// People block (empty).
	put32(b, 0);
	// Retail table (empty slots).
	putZeros(b, 512 * 18);
	// Elevator table: slot 0 built standard, floors 10..30, 2 cars, floor 15 skipped.
	put8(b, 1);      // used
	put8(b, 1);      // type: standard
	put8(b, 21);     // capacity byte (not a kind signal)
	put8(b, 2);      // cars
	putZeros(b, 56); // schedule
	putZeros(b, 2);  // visibility + reserved
	put16(b, 180);   // x tile
	put8(b, 30);     // top floor (TDT index)
	put8(b, 10);     // bottom floor
	for (int f = 0; f < 120; f++) put8(b, f == 15 ? 0 : 1); // serviced bitmap
	put8(b, 10); put8(b, 20); putZeros(b, 6); // 8 car homes (u8 each): 10,20,10,10,10,10,10,10
	putZeros(b, 3140 + 324 * 21 + 348);          // built-shaft payload (spanned)
	// Slot 1: built express, floors 10..100, stopping only at 10/15/30/100.
	// The EXE allocates express payload by geometric slots, even if disabled:
	// 10,24,39,54,69,84,99 (7 slots), not the four enabled bitmap stops.
	put8(b, 1);      // used
	put8(b, 0);      // type: express
	put8(b, 42);     // capacity byte
	put8(b, 1);      // cars
	putZeros(b, 56 + 2);
	put16(b, 200);   // x tile
	put8(b, 100);    // top floor
	put8(b, 10);     // bottom floor
	for (int f = 0; f < 120; f++) put8(b, (f == 10 || f == 15 || f == 30 || f == 100) ? 1 : 0);
	put8(b, 10); putZeros(b, 7); // 8 car homes
	putZeros(b, 3140 + 324 * 7 + 348);
	for (int s = 2; s < 24; s++) putZeros(b, 194); // empty slots: header only
	// EXE c0ba block, then c112 accounting (ten categories + saved total).
	putZeros(b, 88);
	put32(b, 12210); put32(b, 65); put32(b, 348); put32(b, 28);
	put32(b, 480); put32(b, 1961); put32(b, 140); put32(b, 50);
	put32(b, 120); put32(b, 0); put32(b, 15402);
	putZeros(b, 132 - 44);
	putZeros(b, 2 + 512 * 2);
	// Stairs table: one stairs flight, one escalator, one two-story escalator.
	put8(b, 1); put8(b, 1); put16(b, 100); put16(b, 10); putZeros(b, 4);
	put8(b, 1); put8(b, 0); put16(b, 50); put16(b, 11); putZeros(b, 4);
	put8(b, 1); put8(b, 2); put16(b, 60); put16(b, 30); putZeros(b, 4);
	for (int s = 3; s < 64; s++) putZeros(b, 10);
	return b;
}

static void testTdtLoader()
{
	std::vector<uint8_t> bytes = buildTdtFixture();
	EXPECT((int)bytes[0] == 0x00 && (int)bytes[1] == 0x24);
	// Ground floor must land at 560 + 10 * 194 = 0x9C4 (the doc's landmark).
	size_t groundOffset = 560 + 10 * (6 + 94 * 2);
	EXPECT(groundOffset == 0x9C4);

	const std::string path = "OpenSkyTests_tdt_fixture.tdt";
	{
		std::ofstream out(path.c_str(), std::ios::binary);
		out.write((const char *)&bytes[0], (std::streamsize)bytes.size());
		EXPECT(out.good());
	}
	EXPECT(OT::Tdt::looksLikeTdt(path));

	OT::Tdt::TdtData tdt;
	std::string error;
	EXPECT(OT::Tdt::loadFile(path, tdt, error));
	EXPECT(error.empty());
	EXPECT(tdt.level == 5);
	EXPECT(tdt.balance == 946059L);
	EXPECT(tdt.frameTime == 0 && tdt.currentDay == 150);
	EXPECT(tdt.populationValid && tdt.population == 15402);
	EXPECT(tdt.countT[0] == 12210 && tdt.countT[1] == 65);
	EXPECT(tdt.elevatorsValid && tdt.stairsValid);
	EXPECT(tdt.elevators.size() == 2);
	EXPECT(tdt.elevators[0].type == 1 && tdt.elevators[0].cars == 2);
	EXPECT(tdt.elevators[0].bottomFloor == 10 && tdt.elevators[0].topFloor == 30);
	EXPECT(tdt.elevators[1].type == 0 && tdt.elevators[1].bottomFloor == 10 && tdt.elevators[1].topFloor == 100);
	EXPECT(tdt.stairs.size() == 3);

	std::vector<std::string> report;
	const std::string xml = OT::Tdt::toTowerXml(tdt, report);

	// HUD regression uses the reported SCHMITT values in a synthetic save;
	// the distributed real SCHMITT fixture is an earlier snapshot.
	EXPECT(xml.find("funds=\"94605900\"") != std::string::npos);
	EXPECT(xml.find("rating=\"4\"") != std::string::npos);
	EXPECT(xml.find("time=\"150.11538461538461\"") != std::string::npos);
	EXPECT(xml.find("tool=\"inspector\"") != std::string::npos);

	{
		OT::Tdt::TdtData state = tdt;
		state.floors.clear();
		OT::Tdt::FloorRec fr; fr.index=10; fr.leftEdge=187; fr.rightEdge=260;
		fr.tenants = {{187,196,7,16,0,1}, {199,211,9,17,0,1}, {211,214,3,16,0,1}};
		state.floors.push_back(fr);
		const std::string rooms = OT::Tdt::toTowerXml(state, report);
		EXPECT(rooms.find("occupied=\"0\"") != std::string::npos);
		EXPECT(rooms.find("occupied=\"1\"") != std::string::npos);
		EXPECT(rooms.find("hosted=\"1\" roomState=\"1\"") != std::string::npos);
	}

	// Ground lobby at checked coordinates: tile 180 -> x -7, TDT 10 -> y 0.
	EXPECT(xml.find("type=\"lobby\" x=\"-7\" y=\"0\" evaluation=\"50.0\" width=\"20\" height=\"1\"") != std::string::npos);
	// Fast food keeps its variant byte.
	EXPECT(xml.find("type=\"fastfood\" x=\"-87\" y=\"0\" evaluation=\"50.0\" variant=\"3\"") != std::string::npos);
	// Offices, incl. the under-construction one (imported finished).
	EXPECT(xml.find("type=\"office\" x=\"-87\" y=\"1\"") != std::string::npos);
	EXPECT(xml.find("type=\"office\" x=\"-187\" y=\"0\"") != std::string::npos);
	// Cinema halves on floors 12/13 merge to one item at the lower floor.
	EXPECT(xml.find("type=\"cinema\" x=\"-127\" y=\"2\"") != std::string::npos);
	EXPECT(xml.find("type=\"cinema\" x=\"-127\" y=\"3\"") == std::string::npos);
	// Metro's three parts merge to one at floor 20 -> y 10.
	EXPECT(xml.find("type=\"metro\" x=\"-137\" y=\"10\"") != std::string::npos);
	// Cathedral's five type 36..40 records merge at file floor 109 (our 99).
	EXPECT(xml.find("type=\"cathedral\" x=\"13\" y=\"99\" evaluation=\"50.0\" width=\"28\" height=\"5\"") != std::string::npos);
	EXPECT(xml.find("type=\"cathedral\" x=\"13\" y=\"100\"") == std::string::npos);
	// Elevator: kind, span, skip-floor inversion (TDT 15 -> ours 5), car homes.
	EXPECT(xml.find("type=\"elevator-standard\" x=\"-7\" y=\"0\" evaluation=\"50.0\" height=\"21\" showShaft=\"1\"") != std::string::npos);
	EXPECT(xml.find("<unserviced floor=\"5\"/>") != std::string::npos);
	EXPECT(xml.find("<car altitude=\"0.0\" homeFloor=\"0\"/>") != std::string::npos);
	EXPECT(xml.find("<car altitude=\"10.0\" homeFloor=\"10\"/>") != std::string::npos);
	// Express sized by stops parsed without desyncing the table.
	EXPECT(xml.find("type=\"elevator-express\" x=\"13\" y=\"0\" evaluation=\"50.0\" height=\"91\" showShaft=\"1\"") != std::string::npos);
	// Stairs + escalator flights; the two-story escalator stacks two flights.
	EXPECT(xml.find("type=\"stairs\" x=\"-87\" y=\"0\"") != std::string::npos);
	EXPECT(xml.find("type=\"escalator\" x=\"-137\" y=\"1\"") != std::string::npos);
	EXPECT(xml.find("type=\"escalator\" x=\"-127\" y=\"20\"") != std::string::npos);
	EXPECT(xml.find("type=\"escalator\" x=\"-127\" y=\"21\"") != std::string::npos);
	// Steak House clamps to the first available restaurant variant, with a report line.
	EXPECT(xml.find("type=\"restaurant\" x=\"-67\" y=\"1\" evaluation=\"50.0\" variant=\"0\"") != std::string::npos);
	bool sawSteak = false;
	for (size_t i = 0; i < report.size(); i++)
		if (report[i].find("Steak House") != std::string::npos) sawSteak = true;
	EXPECT(sawSteak);

	// Batch 6: shop (10), ramp (44), recycling (20/21) import; burned area (48)
	// becomes a burned span plus rubble tiles; type 45 (ground strips) stays skipped.
	EXPECT(xml.find("type=\"shop\" x=\"-37\" y=\"1\"") != std::string::npos);
	EXPECT(xml.find("type=\"parkingramp\" x=\"-147\" y=\"4\"") != std::string::npos);
	EXPECT(xml.find("type=\"recycling\" x=\"-107\" y=\"6\"") != std::string::npos);
	EXPECT(xml.find("eventBurnedSpans=\"4 -167 -157 \"") != std::string::npos);
	EXPECT(xml.find("eventRubble=\"100400833 ") != std::string::npos);
	EXPECT(xml.find("type=\"office\" x=\"-167\" y=\"4\"") == std::string::npos);

	// Fidelity report: construction site imported finished, housekeeping and ground strips skipped.
	bool sawConstruction = false, sawShop = false, sawHousekeeping = false, saw45 = false, sawBurned = false;
	for (size_t i = 0; i < report.size(); i++)
	{
		if (report[i].find("under construction") != std::string::npos) sawConstruction = true;
		if (report[i].find("kind 10") != std::string::npos || report[i].find("kind 44") != std::string::npos ||
		    report[i].find("kind 48") != std::string::npos) sawShop = true;
		if (report[i].find("kind 15") != std::string::npos) sawHousekeeping = true;
		if (report[i].find("kind 45") != std::string::npos) saw45 = true;
		if (report[i].find("burned area") != std::string::npos) sawBurned = true;
	}
	EXPECT(sawConstruction && !sawShop && sawHousekeeping && saw45 && sawBurned);
	if (!(sawConstruction && !sawShop && sawHousekeeping && saw45 && sawBurned))
	{
		for (size_t i = 0; i < report.size(); i++)
			std::cerr << "report: " << report[i] << "\n";
		std::cerr << "xml head: " << xml.substr(0, 300) << "\n";
	}

	std::remove(path.c_str());
}

static void testTdtLoaderTruncated()
{
	std::vector<uint8_t> bytes = buildTdtFixture();
	// Cut inside the elevator table's built-shaft payload: the tail walk must
	// degrade to a warning (no elevators), not crash or fail the load.
	// Layout: 560 header + 120*194 floor records + 198 tenant bytes + 4 people
	// count + 9216 retail = 33258 = elevator table start.
	size_t elevatorTableStart = 560 + 120 * 194 + 198 + 4 + 512 * 18;
	EXPECT(elevatorTableStart == 33258);
	bytes.resize(elevatorTableStart + 200);

	const std::string path = "OpenSkyTests_tdt_truncated.tdt";
	{
		std::ofstream out(path.c_str(), std::ios::binary);
		out.write((const char *)&bytes[0], (std::streamsize)bytes.size());
	}
	OT::Tdt::TdtData tdt;
	std::string error;
	EXPECT(OT::Tdt::loadFile(path, tdt, error));
	EXPECT(!tdt.elevatorsValid);
	EXPECT(!tdt.warnings.empty());
	std::vector<std::string> report;
	const std::string xml = OT::Tdt::toTowerXml(tdt, report);
	EXPECT(xml.find("elevator-standard") == std::string::npos);
	EXPECT(xml.find("type=\"lobby\"") != std::string::npos); // floor map still imported
	std::remove(path.c_str());

	// Garbage file: wrong magic must fail cleanly.
	{
		std::ofstream out(path.c_str(), std::ios::binary);
		out << "<tower></tower>";
	}
	OT::Tdt::TdtData tdt2;
	EXPECT(!OT::Tdt::looksLikeTdt(path));
	EXPECT(!OT::Tdt::loadFile(path, tdt2, error));
	std::remove(path.c_str());
}


// ---------------------------------------------------------------------------
// YootTower game rules (doc/yoottower/game-parity.md §4 steps 0-3, 5, 6).
// ---------------------------------------------------------------------------

static const OT::PluginManager & yootTestPlugins()
{
	static OT::PluginManager pm;
	static bool scanned = false;
	if (!scanned)
	{
		pm.addPluginDirectory("../data/Plugins");
		scanned = true;
	}
	return pm;
}

// Step 0: the rules-profile seam keeps SimTower/OpenSky constants.
static void testRulesProfileSeam()
{
	using OT::BuildRules;
	using OT::RulesProfile;

	const BuildRules rulesets[] = {BuildRules(BuildRules::Ruleset::SimTower),
	                               BuildRules(BuildRules::Ruleset::OpenSky)};
	for (const BuildRules & r : rulesets)
	{
		const RulesProfile & p = r.profile();
		EXPECT(p.startFunds == 2000000);
		EXPECT(p.startRating == 0);
		EXPECT(p.startDay == 0);
		EXPECT(p.lobbyHeight == 1);
		EXPECT(p.lowestFloor == -10);
		EXPECT(!p.hasBuildArea);
		EXPECT(!p.yootWorld);
		EXPECT(!p.linearClock);
		EXPECT(p.calendar == RulesProfile::Calendar::SimTower);
		EXPECT(p.rain == RulesProfile::Rain::Random);
		EXPECT(p.grades.empty());
		EXPECT(p.unlocks.empty());
		// Footprint checks reduce to the existing horizontal policy.
		EXPECT(r.allowsFootprint(0, -50, 10, 3) == r.allowsHorizontalFootprint(0, 10));
		// Lock table falls back to LevelUp.
		EXPECT(r.minRatingToBuild("metro") == OT::LevelUp::minRatingToBuild("metro"));
	}

	// YootTower without a loaded world: the built-in Tokyo stub.
	BuildRules stub(BuildRules::Ruleset::YootTower);
	EXPECT(stub.profile().yootWorld);
	EXPECT(stub.profile().worldId == "tokyo");
	EXPECT(stub.profile().startFunds == 10000000);
	EXPECT(stub.profile().grades.size() == 4u);
	EXPECT(stub.profile().lobbyHeight == 3);
	EXPECT(stub.profile().linearClock);
}

// Step 1: WORLDDEF / WATR / T2EV / FINF parsed from the shipped plugins.
// YootTower skin: BINF tile layout, backdrop placement and the sky palette.
static void testYootBackdropLayout()
{
	using namespace OT;
	// Synthetic BINF: 2 rows x 3 cols, column-major, one value with high bits.
	const std::string text = "2 3 0\r\n1 2\r\n3 4\r\n5 65542\r\n";
	YootBinf b;
	EXPECT(parseYootBinf(text.data(), int(text.size()), b));
	EXPECT(b.rows == 2 && b.cols == 3 && b.cells.size() == 6u);
	EXPECT(b.tile(0, 0) == 1 && b.tile(1, 0) == 2);
	EXPECT(b.tile(0, 1) == 3 && b.tile(1, 1) == 4);
	EXPECT(b.tile(0, 2) == 5 && b.tile(1, 2) == 6);   // & 0xffff
	EXPECT(b.tile(-3, 9) == 5 && b.tile(7, -1) == 2);  // clamps to the edges
	YootBinf shortMap;
	EXPECT(!parseYootBinf("2 3 0 1 2", 9, shortMap));

	// Tokyo: 61x46 cells, the 6 bottom cell rows are the underground tiles 100..105.
	const PluginManager & pm = yootTestPlugins();
	AbstractPlugin * tokyo = pm.get("Tokyo.t2p");
	EXPECT(tokyo != nullptr);
	if (tokyo)
	{
		const WindowsPEExecutable::Resource * r = tokyo->getResource("BINF", 1);
		EXPECT(r != nullptr);
		YootBinf tb;
		if (r)
		{
			EXPECT(parseYootBinf(r->data, r->length, tb));
			EXPECT(tb.rows == 61 && tb.cols == 46);
			EXPECT(tb.tile(53, 10) == 46 && tb.tile(54, 10) == 47);
			EXPECT(tb.tile(55, 10) == 100 && tb.tile(60, 10) == 105);
		}
	}

	// Placement: Tokyo is 368 units x 122 rows, ground row 110, origin 184.
	YootBackdropGeometry g;
	g.width = 368; g.height = 122; g.groundLine = 110; g.unitOrigin = 184;
	YootBackdropGeometry::Rect c = g.cellRect(55, 23);   // first cell under ground at x = 0
	EXPECT(c.x == 0 && c.y == 0 && c.w == 64 && c.h == 72);
	c = g.cellRect(54, 22);                               // 1F/2F, just left of the centre
	EXPECT(c.x == -64 && c.y == -72);
	c = g.cellRect(0, 0);
	EXPECT(c.x == -1472 && c.y == -3960);
	EXPECT(g.cellRowAt(0.0) == 55 && g.cellRowAt(-0.5) == 54 && g.cellRowAt(-72.0) == 54 && g.cellRowAt(-72.5) == 53);
	EXPECT(g.cellColAt(0.0) == 23 && g.cellColAt(-1.0) == 22 && g.cellColAt(63.9) == 23 && g.cellColAt(64.0) == 24);
	YootBackdropGeometry::Rect wr = g.worldRect();
	EXPECT(wr.x == -1472 && wr.y == -3960 && wr.w == 2944 && wr.h == 4392);
	YootBackdropGeometry k;   // Kegon: 192 x 44, ground row 10, one picture
	k.width = 192; k.height = 44; k.groundLine = 10; k.unitOrigin = 96;
	wr = k.worldRect();
	EXPECT(wr.x == -768 && wr.y == -360 && wr.w == 1536 && wr.h == 1584);
}

static void testYootSkyPalette()
{
	using namespace OT;
	// T2DayLightFilter::SetTime: midnight -90 (270), sunrise 0, noon 90, sunset 0.
	EXPECT(yootDayLightAngleForMinute(0) == 270);
	EXPECT(yootDayLightAngleForMinute(360) == 0);
	EXPECT(yootDayLightAngleForMinute(720) == 90);
	EXPECT(yootDayLightAngleForMinute(1080) == 0);
	EXPECT(yootDayLightAngleForMinute(420) == 23);
	EXPECT(yootDayLightAngleForMinute(1260) > 270);
	EXPECT(yootDayLightFilter(90) == YootRgb(255, 255, 255));
	EXPECT(yootDayLightFilter(270) == YootRgb(61, 73, 102));
	EXPECT(yootDayLightFilter(23) == YootRgb(255, 198, 178));
	// Oracle: Hawaii's first sky colour under the 7:00 filter (the Wine
	// screenshot oracle-shots/stage_c_yooooot.png shows exactly this pink).
	EXPECT(yootSkyColor(yootDayLightFilter(23), YootRgb(242, 250, 250)) == YootRgb(249, 195, 175));
	// ...and at 13:34 (oracle_hawaii.png, clock running) the same entry is near white.
	EXPECT(yootSkyColor(yootDayLightFilter(yootDayLightAngleForMinute(13 * 60 + 30)), YootRgb(242, 250, 250)) == YootRgb(249, 251, 251));
	EXPECT(yootSkyColor(YootRgb(255, 255, 255), YootRgb(255, 255, 255)) == YootRgb(255, 255, 255));
	EXPECT(yootRefractionColor(YootRgb(255, 128, 0), YootRgb(200, 200, 200)) == YootRgb(200, 100, 0));

	// Tokyo sky states (Jan: sunrise 5, sunset 17).
	EXPECT(yootTokyoSkyState(0, 12 * 60, false).state == 1);
	EXPECT(yootTokyoSkyState(0, 3 * 60, false).state == 2);
	EXPECT(yootTokyoSkyState(0, 10 * 60, true).state == 3);
	EXPECT(yootTokyoSkyState(0, 8 * 60 + 30, true).state == 9);
	EXPECT(yootTokyoSkyState(0, 15 * 60, true).state == 10);
	YootSkyState dawn = yootTokyoSkyState(0, 5 * 60 + 30, false);
	EXPECT(dawn.state == 7 && dawn.count == 30);
	EXPECT(yootTokyoSkyState(0, 17 * 60, false).state == 5);
	EXPECT(yootTokyoSkyState(4, 4 * 60 + 10, false).state == 7);   // Apr-Jun: sunrise 4
	EXPECT(yootTokyoSkyState(9, 5 * 60 + 59, false).state == 2);   // Oct-Dec: sunrise 6
	EXPECT(yootTokyoSkyState(11, 3 * 60, false).state == 4);       // Dec night: snow

	// Transition maths: (from-to)/60 * count + to, in bytes.
	std::vector<YootRgb> day(40, YootRgb(9, 9, 9));
	day[24] = YootRgb(0, 0, 120);   // night block
	day[16] = YootRgb(255, 100, 0); // dusk block
	YootSkyState s; s.state = 7; s.count = 30;
	std::vector<YootRgb> blended = yootTokyoSkyColors(day, s);
	EXPECT(blended.size() == 28u && blended[0] == YootRgb(135, 70, 60));
	EXPECT(blended[8] == blended[0] && blended[27] == blended[3]);

	// Real Tokyo data: PTDF and the PALETTE 101 day block.
	const PluginManager & pm = yootTestPlugins();
	AbstractPlugin * tokyo = pm.get("Tokyo.t2p");
	if (tokyo)
	{
		YootPaletteAnimeDef def;
		const WindowsPEExecutable::Resource * ptdf = tokyo->getResource("PTDF", 1);
		EXPECT(ptdf && parseYootPtdf(ptdf->data, ptdf->length, def));
		EXPECT(def.numTowerColor == 238 && def.numAnimColor == 48 && def.animStartIndex == 189);
		EXPECT(def.numRefractColor == 0 && def.numSkyColor == 32 && def.numSEKind == 6);
		EXPECT(def.numSEColor == 16 && def.seInterval == 5);
		std::vector<YootRgb> d101, d102, d100;
		const WindowsPEExecutable::Resource * p101 = tokyo->getResource("PALETTE", 101);
		const WindowsPEExecutable::Resource * p102 = tokyo->getResource("PALETTE", 102);
		const WindowsPEExecutable::Resource * p100 = tokyo->getResource("PALETTE", 100);
		EXPECT(p101 && parseYootPalette(p101->data, p101->length, d101));
		EXPECT(p102 && parseYootPalette(p102->data, p102->length, d102));
		EXPECT(p100 && parseYootPalette(p100->data, p100->length, d100));
		EXPECT(d101.size() == 45u && d102.size() == 96u && d100.size() == 256u);
		if (d101.size() == 45u && d102.size() == 96u)
		{
			EXPECT(d101[0] == YootRgb(127, 127, 127));
			// oracle-shots/stage_c_tokyo_via_open.png: the 9:38 sky is (206, 239, 255).
			EXPECT(d101[8] == YootRgb(206, 239, 255));
			std::vector<YootRgb> pal(256, YootRgb(1, 2, 3));
			YootSkyInputs in; in.month = 0; in.minuteOfDay = 9 * 60 + 38;
			yootApplyAnimatedPalette(YootSkyModel::Tokyo, def, d101, d102, in, pal);
			EXPECT(pal[189] == YootRgb(206, 239, 255));
			EXPECT(pal[189 + 7] == d101[15] && pal[189 + 8] == d101[8]);
			EXPECT(pal[189 + 32] == d102[0]);
			EXPECT(pal[188] == YootRgb(1, 2, 3) && pal[189 + 48] == YootRgb(1, 2, 3));
			in.minuteOfDay = 2 * 60;
			yootApplyAnimatedPalette(YootSkyModel::Tokyo, def, d101, d102, in, pal);
			EXPECT(pal[189] == d101[24]);
		}
	}

	// Hawaii: base T2DayLightFilter path, sky then refraction entries.
	AbstractPlugin * hawaii = pm.get("Hawai.t2p");
	if (hawaii)
	{
		YootPaletteAnimeDef def;
		std::vector<YootRgb> d101, d102;
		const WindowsPEExecutable::Resource * ptdf = hawaii->getResource("PTDF", 1);
		const WindowsPEExecutable::Resource * p101 = hawaii->getResource("PALETTE", 101);
		const WindowsPEExecutable::Resource * p102 = hawaii->getResource("PALETTE", 102);
		EXPECT(ptdf && parseYootPtdf(ptdf->data, ptdf->length, def));
		EXPECT(p101 && parseYootPalette(p101->data, p101->length, d101));
		EXPECT(p102 && parseYootPalette(p102->data, p102->length, d102));
		EXPECT(d101.size() == 39u);
		if (d101.size() == 39u)
		{
			std::vector<YootRgb> pal(256);
			YootSkyInputs in; in.minuteOfDay = 7 * 60 + 4;   // quantised to 7:00
			yootApplyAnimatedPalette(YootSkyModel::DayLightFilter, def, d101, d102, in, pal);
			EXPECT(pal[189] == YootRgb(249, 195, 175));
			EXPECT(pal[189 + 6] == yootRefractionColor(yootDayLightFilter(23), d101[6]));
		}
	}

	// Kegon seasons (1-based month).
	EXPECT(yootKegonSkyPictId(4) == 1000 && yootKegonSkyPictId(7) == 201);
	EXPECT(yootKegonSkyPictId(10) == 202 && yootKegonSkyPictId(11) == 203);
	EXPECT(yootKegonSkyPictId(1) == 204 && yootKegonSkyPictId(12) == 204);
	EXPECT(yootKegonDayLightPaletteId(4) == 150 && yootKegonDayLightPaletteId(2) == 154);
}

static void testYootWorldLoader()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();

	YootWorld::Id id = YootWorld::Id::Tokyo;
	EXPECT(parseYootWorldIdentifier("hawaii", id) && id == YootWorld::Id::Hawaii);
	EXPECT(parseYootWorldIdentifier("kegon", id) && id == YootWorld::Id::Kegon);
	EXPECT(!parseYootWorldIdentifier("Tokyo", id));
	EXPECT(std::string(yootWorldPluginFile(YootWorld::Id::Hawaii)) == "Hawai.t2p");

	std::string err;
	const YootWorld tokyo = loadYootWorld(pm, YootWorld::Id::Tokyo, &err);
	EXPECT(tokyo.fromPlugin);
	EXPECT(tokyo.initialFundsDollars() == 10000000);
	EXPECT(tokyo.initialFunds == 100000);
	EXPECT(tokyo.height == 122 && tokyo.width == 368 && tokyo.groundLine == 110);
	EXPECT(tokyo.buildArea.left == 36 && tokyo.buildArea.top == 6);
	EXPECT(tokyo.buildArea.right == 332 && tokyo.buildArea.bottom == 120);
	EXPECT(tokyo.entranceX == 160 && tokyo.entranceY == 106);
	EXPECT(tokyo.topFloorLine == 10 && tokyo.bottomFloorLine == 119);
	EXPECT(tokyo.lobbyHeight == 3);
	EXPECT(tokyo.startGrade == 1 && tokyo.startMonth == 0);
	EXPECT(tokyo.grades.size() == 4u);
	if (tokyo.grades.size() == 4u)
	{
		EXPECT(tokyo.grades[0].population == 1000);
		EXPECT(tokyo.grades[1].population == 2000);
		EXPECT(tokyo.grades[2].population == 5000);
		EXPECT(tokyo.grades[3].population == 12000);
		EXPECT(tokyo.grades[0].vipFlag == 1u && tokyo.grades[3].vipFlag == 4096u);
		EXPECT(tokyo.grades[3].animId == 1210);
		// The grade-up animation: 13 / 11 frames, looping back to 8 / 6.
		EXPECT(tokyo.grades[0].animFrames == 13 && tokyo.grades[0].animParam == 8);
		EXPECT(tokyo.grades[3].animFrames == 11 && tokyo.grades[3].animParam == 6);
	}
	EXPECT(tokyo.discarded10.size() == 10u && tokyo.discarded10[0] == 200);
	EXPECT(tokyo.cashAmounts.size() == 8u && tokyo.cashAmounts[7] == 200000);
	EXPECT(tokyo.watrInts.size() == 6u && tokyo.watrInts[3] == 100 && tokyo.watrInts[4] == 10);
	EXPECT(tokyo.worldMask == 0x7u);
	EXPECT(tokyo.unbuildable.empty());
	EXPECT(tokyo.highestFloor() == 99);
	EXPECT(tokyo.events.size() == 11u);
	bool allFound = true;
	for (const YootEvent & e : tokyo.events) allFound = allFound && e.found;
	EXPECT(allFound);
	const YootEvent * baseball = tokyo.findEvent("BBEV");
	EXPECT(baseball && baseball->header.levelBit == 8 && baseball->header.beginMin == 1200);
	const YootEvent * fire = tokyo.findEvent("FBEV");
	EXPECT(fire && fire->header.levelBit == 12 && fire->header.cycleDay == 60 &&
	       fire->header.beginMin == 720 && fire->header.endMin == 1080);
	const YootEvent * viev = tokyo.findEvent("VIEV");
	EXPECT(viev && viev->vips.size() == 3u);
	if (viev && viev->vips.size() == 3u)
	{
		EXPECT(viev->header.cycleDay == 9 && viev->header.beginMin == 600);
		EXPECT(viev->vips[0].vipFlag == 1u && viev->vips[1].vipFlag == 16u && viev->vips[2].vipFlag == 256u);
		EXPECT(viev->vips[2].arrivalMin == 900);
		EXPECT(viev->vips[0].destPluginId == 1380142916u); // Record store
	}

	const YootWorld hawaii = loadYootWorld(pm, YootWorld::Id::Hawaii, &err);
	EXPECT(hawaii.fromPlugin);
	EXPECT(hawaii.initialFundsDollars() == 5000000);
	EXPECT(hawaii.startGrade == 2 && hawaii.lobbyHeight == 2);
	EXPECT(hawaii.worldMask == 0xF01u);
	EXPECT(hawaii.highestFloor() == 44 && hawaii.lowestFloor() == -6);
	EXPECT(hawaii.grades.size() == 4u && hawaii.grades[1].population == 500 && hawaii.grades[3].population == 8000);

	const YootWorld kegon = loadYootWorld(pm, YootWorld::Id::Kegon, &err);
	EXPECT(kegon.fromPlugin);
	EXPECT(kegon.initialFundsDollars() == 2000000);
	EXPECT(kegon.startGrade == 3 && kegon.startMonth == 2 && kegon.lobbyHeight == 1);
	EXPECT(kegon.worldMask == 0xF0u);
	EXPECT(kegon.highestFloor() == 1 && kegon.lowestFloor() == -32);
	EXPECT(kegon.finfHeight == 44 && kegon.finfWidth == 192);
	EXPECT(kegon.finfHeader.size() == 7u);
	EXPECT(kegon.unbuildable.size() == 44u * 192u);
	const long blocked = std::accumulate(kegon.unbuildable.begin(), kegon.unbuildable.end(), 0L);
	EXPECT(blocked == 5849);
	EXPECT(kegon.isUnitBuildable(150, 5));    // cliff rock
	EXPECT(!kegon.isUnitBuildable(40, 5));    // waterfall gorge
	EXPECT(!kegon.isUnitBuildable(150, 42));  // rows 42-43 fully blocked

	// Built-in fallback agrees with the plugins on the rule constants.
	const YootWorld worlds[] = {tokyo, hawaii, kegon};
	for (const YootWorld & w : worlds)
	{
		const YootWorld b = builtinYootWorld(w.id);
		EXPECT(!b.fromPlugin);
		EXPECT(b.initialFundsDollars() == w.initialFundsDollars());
		EXPECT(b.groundLine == w.groundLine && b.width == w.width);
		EXPECT(b.buildArea.left == w.buildArea.left && b.buildArea.right == w.buildArea.right);
		EXPECT(b.topFloorLine == w.topFloorLine && b.bottomFloorLine == w.bottomFloorLine);
		EXPECT(b.startGrade == w.startGrade && b.startMonth == w.startMonth);
		EXPECT(b.lobbyHeight == w.lobbyHeight && b.worldMask == w.worldMask);
		bool sameGrades = b.grades.size() == w.grades.size();
		for (size_t i = 0; sameGrades && i < b.grades.size(); ++i)
			sameGrades = b.grades[i].population == w.grades[i].population && b.grades[i].vipFlag == w.grades[i].vipFlag;
		EXPECT(sameGrades);
	}

	// Garbage / truncated input is rejected without touching the output.
	YootWorld t;
	const char bad[] = {0x01, 0x00, 0x00};
	EXPECT(!parseYootWorldDef(bad, sizeof(bad), t));
	EXPECT(!parseYootWatr("text", 4, t));
	EXPECT(!parseYootFinf("2\r\n2\r\n", 6, t));

	// Missing plugin falls back to the built-in world.
	PluginManager empty;
	const YootWorld fallback = loadYootWorld(empty, YootWorld::Id::Kegon, &err);
	EXPECT(!fallback.fromPlugin && fallback.startGrade == 3);
}

// Step 2: start funds, build area, floor limits, FINF and lobby height.
static void testYootPlacement()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();
	const RulesProfile tokyoProfile = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Tokyo));
	const RulesProfile hawaiiProfile = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Hawaii));
	const RulesProfile kegonProfile = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Kegon));
	const BuildRules tokyo(BuildRules::Ruleset::YootTower, tokyoProfile);
	const BuildRules hawaii(BuildRules::Ruleset::YootTower, hawaiiProfile);
	const BuildRules kegon(BuildRules::Ruleset::YootTower, kegonProfile);

	EXPECT(tokyoProfile.startFunds == 10000000);
	EXPECT(hawaiiProfile.startFunds == 5000000);
	EXPECT(kegonProfile.startFunds == 2000000);
	EXPECT(tokyoProfile.lobbyHeight == 3 && hawaiiProfile.lobbyHeight == 2 && kegonProfile.lobbyHeight == 1);
	EXPECT(tokyoProfile.startRating == 0 && hawaiiProfile.startRating == 1 && kegonProfile.startRating == 2);
	EXPECT(kegonProfile.startDay == 2);

	// Tokyo: units [36, 332) at origin width/2 = 184.
	EXPECT(tokyoProfile.unitOriginX == 184);
	EXPECT(tokyo.allowsFootprint(36 - 184, 0, 1));
	EXPECT(!tokyo.allowsFootprint(35 - 184, 0, 1));  // x < 36 rejected
	EXPECT(tokyo.allowsFootprint(331 - 184, 0, 1));
	EXPECT(!tokyo.allowsFootprint(331 - 184, 0, 2));
	EXPECT(!tokyo.allowsHorizontalFootprint(35 - 184, 4));
	EXPECT(tokyo.allowsFootprint(0, 99, 4));    // 100F
	EXPECT(!tokyo.allowsFootprint(0, 100, 4));  // 101F
	EXPECT(!tokyo.allowsFootprint(0, 98, 4, 3));
	EXPECT(tokyo.allowsFootprint(0, -9, 4));    // B9
	EXPECT(!tokyo.allowsFootprint(0, -10, 4));

	// Hawaii: 45 floors above ground, 6 basements.
	EXPECT(hawaii.allowsFootprint(0, 44, 4));   // 45F
	EXPECT(!hawaii.allowsFootprint(0, 45, 4));  // 46F
	EXPECT(!hawaii.allowsFootprint(0, 46, 4));
	EXPECT(hawaii.allowsFootprint(0, -6, 4));
	EXPECT(!hawaii.allowsFootprint(0, -7, 4));
	EXPECT(!hawaii.allowsFootprint(56 - 200 - 1, 0, 1));

	// Kegon: FINF mask; the cliff (units ~131-184 near ground) is buildable,
	// the gorge is not; 2 floors above, 32 below.
	EXPECT(kegonProfile.unitOriginX == 96);
	EXPECT(kegon.allowsFootprint(150 - 96, 0, 4));
	EXPECT(!kegon.allowsFootprint(100 - 96, 0, 4));
	EXPECT(kegon.allowsFootprint(170 - 96, -20, 4));
	EXPECT(kegon.allowsFootprint(150 - 96, 1, 4));
	EXPECT(!kegon.allowsFootprint(150 - 96, 2, 4));
	EXPECT(!kegon.allowsFootprint(150 - 96, -33, 4));
	EXPECT(kegonProfile.lowestFloor == -32);
}

// Step 3: holiday / rain tables vs a direct port of the world plugin code.
static bool portedIsHoliday(uint32_t year, uint32_t month)
{
	// Tokyo.dll FUN_10014c10 (identical in Hawai/Kegon), verbatim.
	const int iVar2 = (int)month;
	if (iVar2 != 7 && iVar2 != 0xb)
	{
		const uint32_t uVar1 = (uint32_t)(iVar2 - 0x17) + year * 0xc;
		if (uVar1 < 2 || (uVar1 - 2) % 5 != 0)
			return false;
	}
	return true;
}

static bool portedTokyoRainy(uint32_t month)
{
	// Tokyo.dll FUN_10014c50, verbatim.
	return month / 3 != 3 && ((uint8_t)month + 1U & 3) == 0;
}

static void testYootCalendar()
{
	using OT::Time;
	bool same = true;
	std::vector<int> holidays[4];
	for (unsigned y = 1; y <= 3; ++y)
		for (unsigned m = 0; m < 12; ++m)
		{
			if (Time::yootIsHoliday(y, m) != portedIsHoliday(y, m)) same = false;
			if (Time::yootTokyoIsRainy(m) != portedTokyoRainy(m)) same = false;
			if (Time::yootIsHoliday(y, m)) holidays[y].push_back((int)m + 1);
		}
	EXPECT(same);
	// 1-based days of each 12-day year (year 1 wraps in unsigned arithmetic).
	EXPECT(holidays[1] == std::vector<int>({3, 8, 12}));
	EXPECT(holidays[2] == std::vector<int>({2, 7, 8, 12}));
	EXPECT(holidays[3] == std::vector<int>({5, 8, 10, 12}));
	std::vector<int> rain;
	for (unsigned m = 0; m < 12; ++m)
		if (Time::yootTokyoIsRainy(m)) rain.push_back((int)m + 1);
	EXPECT(rain == std::vector<int>({4, 8}));

	struct TestTime : public Time { using Time::set; };
	TestTime t;
	t.set(2.5); // day index 2: SimTower weekend
	EXPECT(t.isHoliday());
	t.calendar = Time::Calendar::YootTower;
	EXPECT(t.dayOfYear() == 2 && t.year == 1);
	EXPECT(t.isHoliday() == Time::yootIsHoliday(1, 2));
	t.set(13.5); // year 2, day index 1
	EXPECT(t.dayOfYear() == 1 && t.year == 2 && t.isHoliday());
	t.calendar = Time::Calendar::SimTower;
	EXPECT(!t.isHoliday()); // day % 3 == 1

	// Minute-linear clock (T1), process-wide; restore the SimTower table.
	Time::setLinearClock(true);
	EXPECT_NEAR(Time::hourToAbsolute(12.0), 0.5, 1e-12);
	EXPECT_NEAR(Time::hourToAbsolute(7.0), 7.0 / 24.0, 1e-12);
	EXPECT_NEAR(Time::absoluteToHour(0.25), 6.0, 1e-12);
	EXPECT_NEAR(Time::absoluteToHour(3.75), 18.0, 1e-12);
	Time::setLinearClock(false);
	EXPECT_NEAR(Time::hourToAbsolute(12.0), 7.0 / 26.0, 1e-9);
	EXPECT(!Time::linearClock());
}

// Step 5: unlocks from ATTR mGameLevel + world mask.
static void testYootUnlocks()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();

	YootPluginIdentity coin, bcd, cathedral;
	AbstractPlugin * coinPlugin = pm.get("Coin.t2p");
	AbstractPlugin * bcdPlugin = pm.get("BCDMart.t2p");
	AbstractPlugin * cathedralPlugin = pm.get("Cathedral.t2p");
	EXPECT(coinPlugin && readYootPluginIdentity(*coinPlugin, coin));
	EXPECT(bcdPlugin && readYootPluginIdentity(*bcdPlugin, bcd));
	EXPECT(cathedralPlugin && readYootPluginIdentity(*cathedralPlugin, cathedral));
	EXPECT(coin.attrib == 0x10u && coin.gameLevel == 3 && coin.name == "Coin Press Machine");
	EXPECT(coin.itemType == 1683);
	// Coin is Kegon-only: hidden in Tokyo at every grade.
	EXPECT(!yootPluginAvailable(coin, 0x7, 5));
	EXPECT(yootPluginAvailable(coin, 0xF0, 3));
	EXPECT(!yootPluginAvailable(coin, 0xF0, 2));
	// BCD-Mart (Hawaii) unlocks at grade 3.
	EXPECT(!yootPluginAvailable(bcd, 0xF01, 2));
	EXPECT(yootPluginAvailable(bcd, 0xF01, 3));
	EXPECT(!yootPluginAvailable(bcd, 0x7, 5));
	// Secret items stay hidden unless secret mode is on.
	YootPluginIdentity secret = bcd;
	secret.secretItem = 1;
	EXPECT(!yootPluginAvailable(secret, 0xF01, 5));
	EXPECT(yootPluginAvailable(secret, 0xF01, 5, true));

	const std::map<std::string, YootUnlock> tokyo = resolveYootUnlocks(pm, 0x7);
	const std::map<std::string, YootUnlock> hawaii = resolveYootUnlocks(pm, 0xF01);
	const std::map<std::string, YootUnlock> kegon = resolveYootUnlocks(pm, 0xF0);
	EXPECT(tokyo.count("office") && tokyo.at("office").available && tokyo.at("office").minGrade == 1);
	EXPECT(tokyo.count("yootcathedral") && !tokyo.at("yootcathedral").available);
	EXPECT(hawaii.count("yootcathedral") && hawaii.at("yootcathedral").available && hawaii.at("yootcathedral").minGrade == 4);
	EXPECT(tokyo.count("elevator-express") && tokyo.at("elevator-express").minGrade == 3);
	EXPECT(tokyo.count("security") && tokyo.at("security").minGrade == 2);
	EXPECT(kegon.count("stairs") && !kegon.at("stairs").available);
	EXPECT(kegon.count("elevator-standard") && kegon.at("elevator-standard").available);
	EXPECT(!hawaii.at("office").available);
	EXPECT(tokyo.count("secom") == 0u); // no YootTower counterpart

	RulesProfile profile = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Tokyo));
	for (std::map<std::string, YootUnlock>::const_iterator u = tokyo.begin(); u != tokyo.end(); ++u)
	{
		RulesProfile::Unlock unlock;
		unlock.available = u->second.available;
		unlock.minRating = u->second.minGrade - 1;
		profile.unlocks[u->first] = unlock;
	}
	const BuildRules rules(BuildRules::Ruleset::YootTower, profile);
	EXPECT(rules.minRatingToBuild("cathedral") == BuildRules::kUnavailable);
	EXPECT(rules.minRatingToBuild("office") == 0);
	EXPECT(rules.minRatingToBuild("elevator-express") == 2);
	EXPECT(rules.minRatingToBuild("secom") == LevelUp::minRatingToBuild("secom"));

	// VIEV destinations map to OpenSky prototypes by plugin id.
	EXPECT(yootPrototypesForPluginId(pm, 1213420873u) == std::vector<std::string>({"hotel_suite"}));
	// Record store: its own plugin-backed prototype first, then the generic shop.
	EXPECT(yootPrototypesForPluginId(pm, 1380142916u) == std::vector<std::string>({"yt_record", "shop"}));
	EXPECT(yootPrototypesForPluginId(pm, 0x12345678u).empty());
}

// Step 6: grade table, VIP gate, VIP verdict and the final-grade hook.
static void testYootGrades()
{
	using OT::LevelUp;
	// Tokyo 1->2: population 1000 and VIP flag 1.
	EXPECT(!LevelUp::yootPromotionReady(1, 1000, 0, 999));
	EXPECT(!LevelUp::yootPromotionReady(1, 1000, 0, 1000));
	EXPECT(!LevelUp::yootPromotionReady(1, 1000, 1, 999));
	EXPECT(LevelUp::yootPromotionReady(1, 1000, 1, 1000));
	EXPECT(LevelUp::yootPromotionReady(16, 2000, 0x11, 2000));
	EXPECT(!LevelUp::yootPromotionReady(16, 2000, 0x01, 5000));

	// VIP arrives once population exceeds half the threshold.
	EXPECT(!LevelUp::yootVipMayStart(1, 1, 4, 1, 1, 1000, 0, 500));
	EXPECT(LevelUp::yootVipMayStart(1, 1, 4, 1, 1, 1000, 0, 501));
	EXPECT(!LevelUp::yootVipMayStart(1, 1, 4, 1, 1, 1000, 1, 900)); // already visited
	EXPECT(!LevelUp::yootVipMayStart(1, 2, 4, 1, 1, 1000, 0, 900)); // wrong grade
	EXPECT(!LevelUp::yootVipMayStart(5, 5, 4, 1, 1, 1000, 0, 900)); // beyond the table

	EXPECT(!LevelUp::yootVipSucceeds(200));
	EXPECT(LevelUp::yootVipSucceeds(201));
	EXPECT(LevelUp::yootEstimateFromEvaluation(67.0) == 201);
	EXPECT(LevelUp::yootEstimateFromEvaluation(66.6) == 200);
	EXPECT(LevelUp::yootEstimateFromEvaluation(-5.0) == 1);
	EXPECT(LevelUp::yootEstimateFromEvaluation(500.0) == 300);

	// Final-item hook only at grade == numGrades.
	uint32_t flags = 0x111;
	EXPECT(!LevelUp::yootGrantFinalGrade(3, 4, 4096, flags));
	EXPECT(flags == 0x111u);
	EXPECT(LevelUp::yootGrantFinalGrade(4, 4, 4096, flags));
	EXPECT(flags == 0x1111u);
	EXPECT(LevelUp::yootPromotionReady(4096, 12000, flags, 12000));

	// Shared-accumulator VIP flags (T2VisitVIP::Init).
	std::vector<OT::YootVipDef> vips(4);
	vips[0].grade = 1; vips[1].grade = 2; vips[2].grade = 2; vips[3].grade = 3;
	OT::assignYootVipFlags(vips);
	EXPECT(vips[0].vipFlag == 1u && vips[1].vipFlag == 16u && vips[2].vipFlag == 32u && vips[3].vipFlag == 256u);

	// Profiles carry the per-world start star and VIP list.
	const OT::RulesProfile kegon = OT::RulesProfile::yootTower(OT::loadYootWorld(yootTestPlugins(), OT::YootWorld::Id::Kegon));
	EXPECT(kegon.startRating + 1 == 3);
	EXPECT(kegon.grades.size() == 4u && kegon.grades[2].population == 200);
	EXPECT(kegon.vips.size() == 3u && kegon.vips[2].vipFlag == 256u && kegon.vips[2].beginMinute == 600);
}

static void testSimTowerEventClock()
{
	using OT::Time;
	struct TestTime : public Time { using Time::set; };
	TestTime t;
	EXPECT(!Time::linearClock());
	EXPECT(Time::kSimFramesPerDay == 2600);

	const int stages[] = {0, 299, 300, 540, 700, 1499, 1500, 1800, 2300, 2599, 2600, 2900};
	const int frames[] = {2300, 2599, 0, 240, 400, 1199, 1200, 1500, 2000, 2299, 2300, 0};
	for (unsigned i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i) {
		t.set((stages[i] + 0.25) / 2600.0);
		EXPECT(t.eventFrame() == frames[i]);
	}
	t.set(Time::hourToAbsolute(10.0));
	EXPECT(t.eventFrame() == 240);
	const double ignition = t.absolute;
	t.set(Time::hourToAbsolute(13.0));
	EXPECT(t.eventFrame() == 1200);
	EXPECT_NEAR(t.eventFramesSince(ignition), 960.0, 1e-9);
	EXPECT_NEAR(t.eventFramesSince(Time::hourToAbsolute(12.0)), 800.0, 1e-9);
	t.set(Time::hourToAbsolute(21.0));
	EXPECT(t.eventFrame() == 2000);
	EXPECT_NEAR(t.eventFramesSince(ignition), 1760.0, 1e-9);
	t.set(Time::hourToAbsolute(7.0));
	EXPECT_NEAR(t.eventFramesSince(Time::hourToAbsolute(1.0)), 200.0, 1e-9);
	t.set(1.0 + 0.5 / 2600.0);
	EXPECT_NEAR(t.eventFramesSince(1.0 - 0.5 / 2600.0), 1.0, 1e-9);
	EXPECT_NEAR(t.eventFramesSince(t.absolute), 0.0, 1e-12);

	// Cleanup preserves the disaster day and does not count its skip as a tick.
	t.set(83.0 + Time::hourToAbsolute(10.0));
	t.advanceToEventFrame(1500);
	EXPECT_NEAR(t.absolute, 83.0 + Time::hourToAbsolute(16.0), 1e-12);
	EXPECT_NEAR(t.hour, 16.0, 1e-9);
	const double cleanup = t.absolute;
	t.advanceToEventFrame(1500);
	EXPECT_NEAR(t.absolute, cleanup, 1e-12);
	EXPECT_NEAR(t.dta, 0.0, 1e-12);
	// A cleanup after 16:00 never moves the clock (no 23-hour leap).
	t.set(59.0 + Time::hourToAbsolute(17.0));
	const double late = t.absolute;
	t.advanceToEventFrame(1500);
	EXPECT_NEAR(t.absolute, late, 1e-12);
	// Compound float error at an hour anchor must not drop a frame.
	t.set(Time::hourToAbsolute(8.35));
	EXPECT(t.eventFrame() == 108);
}

// Batch 3: SECOM is never offered at any star level (EXE build menu, Q8).
static void testBatch3SecomNeverUnlocks()
{
	using OT::LevelUp;
	for (int rating = 0; rating <= 4; ++rating)
		EXPECT(LevelUp::minRatingToBuild("secom") > rating);
	// The neighbouring unlocks are unchanged.
	EXPECT(LevelUp::minRatingToBuild("security") <= 2);
	EXPECT(LevelUp::minRatingToBuild("parking") == 2);
}



// Opt-in real-engine disaster scenarios. Compile this translation unit with
// -DOPENSKY_DISASTER_SCENARIOS and link the OpenSkyscraper objects except main.
// Run under Xvfb with a temporary writable cwd; normal seam tests are unchanged.
#ifdef OPENSKY_DISASTER_SCENARIOS
#include <queue>
#include <list>
#include <deque>
#include <memory>
#include <utility>
#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <tinyxml2.h>
// Test-only access to the real engine; no production debug hooks.
#define private public
#define protected public
#include "Application.h"
#include "Game.h"
#include "Item/Condo.h"
#include "Item/YootCondo.h"
#include "Item/Housekeeping.h"
#include "Item/Hotel.h"
#include "Item/Office.h"
#include "Item/Restaurant.h"
#include "Item/FastFood.h"
#include "Item/Cinema.h"
#include "Item/PartyHall.h"
#include "Item/Metro.h"
#include "Item/Cathedral.h"
#include "Item/Parking.h"
#include "Item/Shop.h"
#include "Item/Stairlike.h"
#include "Item/Elevator/Car.h"
#include "Item/Elevator/Queue.h"
#undef protected
#undef private
using namespace OT;
static int disasterChecks=0;
#define DISASTER_CHECK(x) do { ++disasterChecks; if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; std::exit(2); } } while (0)
static Item::Item *makeDisasterItem(Game &g, const char *id, int x, int y) {
 auto *item=g.itemFactory.make(id,int2(x,y)); g.addItem(item); item->underConstruction=false; return item;
}
struct DisasterScenarioClock : Time {
 using Time::set;
 void pace() { speed = speed_animated = 1; }
};
static void setDisasterTime(Game &g, double absolute) {
 DisasterScenarioClock clock;
 static_cast<Time &>(clock) = g.time;
 clock.set(absolute); clock.pace(); g.time = clock;
}
static void resetDisasterScenario(Game &g) {
 g.clearWorld(); g.money.clear(2000000);g.funds=2000000;g.rating=2;
 setDisasterTime(g,83+Time::hourToAbsolute(10));
 g.lastAccountingDay=83;g.lastAccountingQuarter=27;
 g.eventSystem.prevAbsolute=g.time.absolute;
}
static int testDisasterScenarios() {
 char exe[]="OpenSkyDisasterTests", rules[]="--ruleset", sky[]="opensky", log[]="--log", logpath[]="/tmp/opensky_disaster_scenarios.log";
 char *args[]={exe,rules,sky,log,logpath}; Application app(5,args);app.audioMuted=true;
 app.window.create(sf::VideoMode({1280,768}),"disaster scenarios");app.window.setVisible(false);app.gui.setWindow(app.window);
 app.data.addSearchPath("/home/tim/Projects/OpenSky-art-v2/data");
 Game g(app);
 // Restaurant exists as a class but is absent from the baseline factory catalog.
 auto *restaurantProto=Item::Restaurant::makePrototype();
 g.itemFactory.prototypes.push_back(restaurantProto);g.itemFactory.prototypesById["restaurant"]=restaurantProto;
 resetDisasterScenario(g);
 // Every registered prototype: common survivor predicate and whole footprint.
 for (auto *proto:g.itemFactory.prototypes) {
  resetDisasterScenario(g);auto *item=makeDisasterItem(g,proto->id.c_str(),0,1);
  bool expected=proto->id=="lobby" || proto->id=="floor" || proto->id=="security" || proto->id=="housekeeping" || proto->id=="metro" || proto->id=="cathedral" || item->canHaulPeople();
  auto r=item->getRect();int before=g.funds;
  g.destroyItemByDisaster(item);
  DISASTER_CHECK((g.items.count(item)!=0)==expected);DISASTER_CHECK(g.funds==before);
  if (!expected) {for(int f=r.minY();f<r.maxY();++f) for(int x=r.minX();x<r.maxX();++x)DISASTER_CHECK(g.eventSystem.hasRubble(f,x));g.destroyItemByDisaster(item);DISASTER_CHECK(!g.items.count(item));}
  else {DISASTER_CHECK(!g.eventSystem.rubbleIntersects(r));DISASTER_CHECK(item->evaluation==50);}
 }
 // Construction protection independent of event-start completion.
 resetDisasterScenario(g);auto *building=makeDisasterItem(g,"office",0,2);building->underConstruction=true;
 g.destroyItemByDisaster(building);DISASTER_CHECK(g.items.count(building));DISASTER_CHECK(!g.eventSystem.hasRubble(2,0));
 // Real front callback burns a multi-floor unit hit only on its upper floor.
 resetDisasterScenario(g);auto *cinema=makeDisasterItem(g,"cinema",10,2);auto r=cinema->getRect();
 auto *sec=makeDisasterItem(g,"security",10,2);sec->evaluation=73;
 g.eventSystem.destroyFireCell(3,12);DISASTER_CHECK(!g.items.count(cinema));DISASTER_CHECK(g.items.count(sec));DISASTER_CHECK(sec->evaluation==73);
 for(int f=r.minY();f<r.maxY();++f)for(int x=r.minX();x<r.maxX();++x)DISASTER_CHECK(g.eventSystem.hasRubble(f,x));
 g.eventSystem.destroyFireCell(3,12);DISASTER_CHECK(g.items.count(sec));
 // Exact sale reversal, including income rather than expense totals.
 resetDisasterScenario(g);auto *condo=static_cast<Item::Condo *>(makeDisasterItem(g,"condo",0,2));
 condo->occupied=true;condo->rentDeposit=160000;condo->createOccupants();
 g.transferFunds(160000,"condo_sale","scenario sale");int n=g.people.size();DISASTER_CHECK(n>0);
 auto *resident=*condo->occupants.begin();DISASTER_CHECK(resident->at==condo);
 g.destroyItemByDisaster(condo);DISASTER_CHECK(g.people.empty());DISASTER_CHECK(g.funds==2000000);DISASTER_CHECK(g.money.todayIncome==0);DISASTER_CHECK(g.money.todayExpenses==0);DISASTER_CHECK(g.money.quarterIncome==0);
 // Old saves without a sale/deposit field derive the exact move-in credit.
 for (const char *id : {"condo", "yoot_condo"}) {
  resetDisasterScenario(g);auto *oldCondo=makeDisasterItem(g,id,0,2);
  tinyxml2::XMLDocument saleDoc;DISASTER_CHECK(saleDoc.Parse("<item occupied=\"true\"/>")==0);oldCondo->decodeXML(*saleDoc.RootElement());
  const int credit=150000;DISASTER_CHECK(oldCondo->disasterSaleReversal()==credit);g.transferFunds(credit,"condo_sale","");g.destroyItemByDisaster(oldCondo);DISASTER_CHECK(g.funds==2000000);DISASTER_CHECK(g.money.todayIncome==0);DISASTER_CHECK(g.people.empty());
 }
 // Batch 3 (harness only; not part of OpenSkyTests): credits clamp at INT_MAX,
 // disaster-offer days are day%60==59 or day%84==83.
 resetDisasterScenario(g);g.transferFunds(9999999900LL,"clamp","");DISASTER_CHECK(g.funds==9999999900LL);g.transferFunds(1000,"clamp","");DISASTER_CHECK(g.funds==9999999900LL);
 { tinyxml2::XMLPrinter p; g.encodeXML(p); tinyxml2::XMLDocument d; DISASTER_CHECK(d.Parse(p.CStr())==tinyxml2::XML_SUCCESS); g.decodeXML(d); DISASTER_CHECK(g.funds==9999999900LL); DISASTER_CHECK(g.money.balance==9999999900LL); DISASTER_CHECK(g.money.quarterIncome==9997999900LL); }
 setDisasterTime(g,59+Time::hourToAbsolute(10));DISASTER_CHECK(g.isDisasterOfferDay());
 setDisasterTime(g,83+Time::hourToAbsolute(10));DISASTER_CHECK(g.isDisasterOfferDay());
 setDisasterTime(g,10+Time::hourToAbsolute(10));DISASTER_CHECK(!g.isDisasterOfferDay());
 // Parking deletion clears worker/guest pointers before their owners die.
 resetDisasterScenario(g);auto *office=static_cast<Item::Office *>(makeDisasterItem(g,"office",0,2));
 auto *hotel=static_cast<Item::Hotel *>(makeDisasterItem(g,"hotel_suite",15,2));auto *parking=static_cast<Item::Parking *>(makeDisasterItem(g,"parking",0,-1));
 auto *worker=*office->workers.begin();auto *guest=new Item::Hotel::Guest(hotel);hotel->guests.insert(guest);
 worker->parkingUsed=parking;guest->parkingUsed=parking;
 g.destroyItemByDisaster(parking);DISASTER_CHECK(!worker->parkingUsed);DISASTER_CHECK(!guest->parkingUsed);g.destroyItemByDisaster(office);g.destroyItemByDisaster(hotel);DISASTER_CHECK(g.people.empty());
 // Destination journey and UI/VIP handles are purged through silent deletion.
 resetDisasterScenario(g);office=static_cast<Item::Office *>(makeDisasterItem(g,"office",0,2));building=makeDisasterItem(g,"restaurant",15,2);
 worker=*office->workers.begin();Route route;route.add(office,2);route.add(building,2);worker->journey.set(route);
 g.visualizeRoute=route;g.inspectorDialog.showForItem(building);g.vipSystem.yootDestination=building;
 g.destroyItemByDisaster(building);DISASTER_CHECK(!worker->at);DISASTER_CHECK(!worker->journey.item);DISASTER_CHECK(!worker->journey.references(building));DISASTER_CHECK(g.visualizeRoute.empty());DISASTER_CHECK(!g.inspectorDialog.isVisible());DISASTER_CHECK(!g.vipSystem.yootDestination);
 // Evacuation parks permanent owner records, deletes transient patrons, and
 // empties transport state through virtual detachment before resetting cars.
 resetDisasterScenario(g);office=static_cast<Item::Office *>(makeDisasterItem(g,"office",0,2));office->occupied=true;office->population=6;
 condo=static_cast<Item::Condo *>(makeDisasterItem(g,"condo",15,2));condo->occupied=true;condo->createOccupants();
 hotel=static_cast<Item::Hotel *>(makeDisasterItem(g,"hotel_single",35,2));guest=new Item::Hotel::Guest(hotel);hotel->guests.insert(guest);hotel->addPerson(guest);
 auto *food=static_cast<Item::Restaurant *>(makeDisasterItem(g,"restaurant",50,2));auto *customer=new Item::Restaurant::Customer(food);food->customers.insert(customer);food->addPerson(customer);food->open=true;food->customersToday=15;
 auto *elevator=static_cast<Item::Elevator::Elevator *>(makeDisasterItem(g,"elevator-standard",90,0));elevator->size.y=5;
 auto *stairs=static_cast<Item::Stairlike *>(makeDisasterItem(g,"stairs",105,1));
 worker=*office->workers.begin();worker->journey.fromFloor=0;worker->journey.toFloor=2;elevator->addPerson(worker);worker->stress=81;
 auto *car=*elevator->cars.begin();car->state=Item::Elevator::Car::kMoving;car->altitude=3;car->doorTicks=4;
 resident=*condo->occupants.begin();DISASTER_CHECK(resident->at==condo);condo->removePerson(resident);DISASTER_CHECK(!resident->at);DISASTER_CHECK(condo->occupants.count(resident));stairs->addPerson(resident);building=makeDisasterItem(g,"shop",115,2);building->underConstruction=true;
 g.eventSystem.type=EventSystem::Type::Fire;g.evacuateForDisaster();
 DISASTER_CHECK(!building->underConstruction);DISASTER_CHECK(hotel->guests.empty());DISASTER_CHECK(hotel->roomState==Item::Hotel::kDirty);DISASTER_CHECK(food->customers.empty());DISASTER_CHECK(!food->open);DISASTER_CHECK(food->customersToday==0);DISASTER_CHECK(elevator->people.empty());DISASTER_CHECK(elevator->queues.empty());DISASTER_CHECK(car->state==Item::Elevator::Car::kIdle);DISASTER_CHECK(car->altitude==car->homeFloor);DISASTER_CHECK(car->doorTicks==0);DISASTER_CHECK(stairs->people.empty());DISASTER_CHECK(stairs->transitionTimes.empty());DISASTER_CHECK(condo->occupants.size()>0);DISASTER_CHECK(office->occupied);DISASTER_CHECK(condo->occupied);
 // Batch 5 owner persons (condo/yoot residents, maids) live inside their own unit
 // after evacuation; every other item is empty and nobody is mid-journey.
 auto ownerHome=[](Item::Item *it){return dynamic_cast<Item::Condo *>(it)||dynamic_cast<Item::YootCondo *>(it)||dynamic_cast<Item::Housekeeping *>(it);};
 for (auto *item : g.items) {if(ownerHome(item)){for(auto *p:item->people)DISASTER_CHECK(p->at==item);}else DISASTER_CHECK(item->people.empty());}
 for(auto *p:g.people){DISASTER_CHECK(!p->at||ownerHome(p->at));DISASTER_CHECK(!p->journey.item);DISASTER_CHECK(p->state==Person::kIdle);}
 for(auto *p:condo->occupants)DISASTER_CHECK(p->at==condo&&!p->out);DISASTER_CHECK(worker->stress==81);
 // Other venue state mappings and temporary owner teardown.
 auto *fast=static_cast<Item::FastFood *>(makeDisasterItem(g,"fastfood",130,2));
 auto *fastCustomer=new Item::FastFood::Customer(fast); fast->customers.insert(fastCustomer); fast->addPerson(fastCustomer); fast->open=true; fast->customersToday=9;
 auto *movie=static_cast<Item::Cinema *>(makeDisasterItem(g,"cinema",150,2));auto *movieCustomer=new Item::Cinema::Customer(movie);movie->customers.insert(movieCustomer);movie->addPerson(movieCustomer);movie->open=movie->playing=true;movie->patronsToday=7;
 auto *hall=static_cast<Item::PartyHall *>(makeDisasterItem(g,"partyhall",185,2));auto *partyGuest=new Item::PartyHall::Visitor(hall);hall->visitors.insert(partyGuest);hall->addPerson(partyGuest);hall->open=true;hall->patronsToday=4;
 auto *metro=static_cast<Item::Metro *>(makeDisasterItem(g,"metro",215,-3));auto *rider=new Item::Metro::Visitor(metro);metro->visitors.insert(rider);metro->addPerson(rider);metro->trainPresent=true;
 auto *church=static_cast<Item::Cathedral *>(makeDisasterItem(g,"cathedral",250,2));auto *churchGuest=new Item::Cathedral::Guest(church);church->guests.insert(churchGuest);church->addPerson(churchGuest);church->weddingPhase=Item::Cathedral::kWeddingGuestsArriving;
 auto *shop=static_cast<Item::Shop *>(building);shop->open=true;shop->patrons=12;shop->customersToday=12;
 g.evacuateForDisaster();DISASTER_CHECK(fast->customers.empty());DISASTER_CHECK(!fast->open);DISASTER_CHECK(fast->customersToday==0);DISASTER_CHECK(movie->customers.empty());DISASTER_CHECK(!movie->playing);DISASTER_CHECK(movie->patronsToday==0);DISASTER_CHECK(hall->visitors.empty());DISASTER_CHECK(!hall->open);DISASTER_CHECK(hall->patronsToday==0);DISASTER_CHECK(metro->visitors.empty());DISASTER_CHECK(!metro->trainPresent);DISASTER_CHECK(church->guests.empty());DISASTER_CHECK(church->weddingPhase==Item::Cathedral::kWeddingIdle);DISASTER_CHECK(!shop->open);DISASTER_CHECK(shop->patrons==0);
 // Real simulation freeze with an active bomb resolved but awaiting cleanup.
 g.eventSystem.type=EventSystem::Type::Terror;g.eventSystem.bombArmed=false;g.eventSystem.bombCleanupAt=g.time.absolute+100.0/2600;
 const double beforeTime=g.time.absolute;const int beforePeople=g.people.size();worker->state=Person::kHome;food->open=true;food->quotaRemaining=40;
 car->moveTo(4);const double beforeAltitude=car->altitude;
 g.advanceSimTick(2);DISASTER_CHECK(car->altitude!=beforeAltitude);DISASTER_CHECK(!metro->trainPresent);DISASTER_CHECK(g.time.absolute>beforeTime);DISASTER_CHECK(g.people.size()==beforePeople);DISASTER_CHECK(worker->stress==81);DISASTER_CHECK(food->customers.empty());DISASTER_CHECK(food->quotaRemaining==40);DISASTER_CHECK(g.eventSystem.isActive());
 g.toolPrototype=g.itemFactory.prototypesById["office"];int beforeFunds=g.funds;DISASTER_CHECK(!g.constructToolAt(int2(140,2)));DISASTER_CHECK(g.funds==beforeFunds);
 g.eventSystem.bombCleanupAt=g.time.absolute;g.advanceSimTick(0);DISASTER_CHECK(!g.eventSystem.isActive());DISASTER_CHECK(g.time.eventFrame()==1500);
 worker->state=Person::kHome;g.advanceSimTick(1);DISASTER_CHECK(worker->stress<81);
 // Real bomb box: floors -2..+3, cells -20..+19; linked units fully burn.
 resetDisasterScenario(g);auto *left=makeDisasterItem(g,"shop",-20,1);auto *edge=makeDisasterItem(g,"shop",20,1);auto *tall=makeDisasterItem(g,"recycling",0,3);sec=makeDisasterItem(g,"security",0,1);
 g.eventSystem.type=EventSystem::Type::Terror;g.eventSystem.bombFloor=0;g.eventSystem.bombSlot=0;g.eventSystem.bombArmed=true;
 g.eventSystem.armBomb();g.eventSystem.guardOffices.clear();DISASTER_CHECK(g.eventSystem.isActive());g.eventSystem.detonateBomb();
 DISASTER_CHECK(!g.items.count(left));DISASTER_CHECK(g.items.count(edge));DISASTER_CHECK(!g.items.count(tall));DISASTER_CHECK(g.eventSystem.hasRubble(4,0));DISASTER_CHECK(g.items.count(sec));DISASTER_CHECK(!g.eventSystem.hasRubble(1,20));DISASTER_CHECK(g.eventSystem.isActive());
 setDisasterTime(g,g.eventSystem.bombCleanupAt-0.25/2600);g.eventSystem.advance(0);DISASTER_CHECK(g.eventSystem.isActive());setDisasterTime(g,g.eventSystem.bombCleanupAt);g.eventSystem.advance(0);DISASTER_CHECK(!g.eventSystem.isActive());
 // Defuse keeps the lock for the same two-frame deadline.
 resetDisasterScenario(g);sec=makeDisasterItem(g,"security",0,2);g.eventSystem.type=EventSystem::Type::Terror;g.eventSystem.bombFloor=2;g.eventSystem.bombSlot=0;g.eventSystem.armBomb();
 auto &guard=g.eventSystem.guardOffices[0].g[0];guard.floor=2;guard.x=1;guard.pause=guard.transit=0;
 g.advanceSimTick(1);DISASTER_CHECK(g.eventSystem.bombCaught);DISASTER_CHECK(g.eventSystem.isActive());DISASTER_CHECK(!g.eventSystem.bombArmed);
 setDisasterTime(g,g.eventSystem.bombCleanupAt);g.eventSystem.advance(0);DISASTER_CHECK(!g.eventSystem.isActive());
 // Persistent aftermath: save reload, both placement paths, player clearing.
 resetDisasterScenario(g);building=makeDisasterItem(g,"office",0,2);g.destroyItemByDisaster(building);
 tinyxml2::XMLPrinter xml;g.encodeXML(xml);tinyxml2::XMLDocument doc;DISASTER_CHECK(doc.Parse(xml.CStr())==0);g.decodeXML(doc);
 DISASTER_CHECK(g.eventSystem.hasRubble(2,0));DISASTER_CHECK(g.itemsByType["office"].empty());g.toolPrototype=g.itemFactory.prototypesById["office"];DISASTER_CHECK(!g.constructToolAt(int2(0,2)));
 std::string reason;DISASTER_CHECK(!g.validateBatchCell(g.toolPrototype,0,2,reason));DISASTER_CHECK(reason.find("burned")!=std::string::npos);
 g.selectedTool="bulldozer";g.toolPosition=int2(0,2);g.itemBelowCursor=g.floorItems[2];sf::Event click=sf::Event::MouseButtonPressed{sf::Mouse::Button::Left,{1000,600}};
 g.eventSystem.type=EventSystem::Type::Terror;g.eventSystem.bombCleanupAt=g.time.absolute+10;DISASTER_CHECK(g.handleEvent(click));DISASTER_CHECK(g.eventSystem.hasRubble(2,0));
 g.eventSystem.type=EventSystem::Type::None;DISASTER_CHECK(g.handleEvent(click));DISASTER_CHECK(!g.eventSystem.hasRubble(2,0));DISASTER_CHECK(!g.eventSystem.rubbleIntersects(recti(int2(0,2),int2(9,1))));
 // Adjacent burned tenant records remain separate through save/load.
 resetDisasterScenario(g);auto *firstBurn=makeDisasterItem(g,"office",0,2);auto *secondBurn=makeDisasterItem(g,"office",9,2);g.destroyItemByDisaster(firstBurn);g.destroyItemByDisaster(secondBurn);
 tinyxml2::XMLPrinter spansXml;g.encodeXML(spansXml);tinyxml2::XMLDocument spansDoc;DISASTER_CHECK(spansDoc.Parse(spansXml.CStr())==0);g.decodeXML(spansDoc);
 DISASTER_CHECK(g.eventSystem.clearRubble(2,0));DISASTER_CHECK(!g.eventSystem.hasRubble(2,8));DISASTER_CHECK(g.eventSystem.hasRubble(2,9));DISASTER_CHECK(g.eventSystem.hasRubble(2,17));DISASTER_CHECK(g.eventSystem.clearRubble(2,9));DISASTER_CHECK(!g.eventSystem.hasRubble(2,17));
 // Overlay-only legacy saves still load and can be bulldozed.
 resetDisasterScenario(g);g.eventSystem.addRubble(2,0);g.eventSystem.addRubble(2,1);tinyxml2::XMLPrinter legacyXml;legacyXml.OpenElement("event");g.eventSystem.encodeXML(legacyXml);legacyXml.CloseElement();tinyxml2::XMLDocument legacyDoc;DISASTER_CHECK(legacyDoc.Parse(legacyXml.CStr())==0);legacyDoc.RootElement()->DeleteAttribute("eventBurnedSpans");g.eventSystem.decodeXML(*legacyDoc.RootElement());DISASTER_CHECK(g.eventSystem.hasRubble(2,0));DISASTER_CHECK(g.eventSystem.clearRubble(2,1));DISASTER_CHECK(!g.eventSystem.hasRubble(2,0));
 // Population recount drops destroyed tenants; repeated destruction is silent.
 resetDisasterScenario(g);office=static_cast<Item::Office *>(makeDisasterItem(g,"office",0,2));office->occupied=true;office->population=6;g.addPopulation(office->populationCategory(),office->populationContribution());g.advanceSimTick(0);DISASTER_CHECK(g.population==6);
 g.destroyItemByDisaster(office);DISASTER_CHECK(g.populationNeedsUpdate);g.advanceSimTick(0);DISASTER_CHECK(g.population==0);DISASTER_CHECK(g.people.empty());
 // Burned-only floors retain their extent and front movement after deletion.
 resetDisasterScenario(g);building=makeDisasterItem(g,"shop",0,2);g.destroyItemByDisaster(building);int extentLeft,extentRight;DISASTER_CHECK(g.eventSystem.floorExtent(2,extentLeft,extentRight));DISASTER_CHECK(extentLeft==0);DISASTER_CHECK(extentRight==12);
 g.eventSystem.type=EventSystem::Type::Fire;g.eventSystem.fireOriginFloor=2;g.eventSystem.fireOriginCell=0;g.eventSystem.fireLeft[2]=5;g.eventSystem.fireFrame=1;
 g.eventSystem.stepFireFrame();DISASTER_CHECK(g.eventSystem.isActive());DISASTER_CHECK(g.eventSystem.fireLeft.count(2));
 // Actual fire start finishes construction, evacuates, preserves ignition time.
 resetDisasterScenario(g);makeDisasterItem(g,"lobby",0,0);sec=makeDisasterItem(g,"security",0,2);building=makeDisasterItem(g,"shop",30,2);building->underConstruction=true;g.rating=2;double ignition=g.time.absolute;
 g.eventSystem.startFire();DISASTER_CHECK(g.eventSystem.getActiveType()==EventSystem::Type::Fire);DISASTER_CHECK(!building->underConstruction);DISASTER_CHECK(g.time.absolute==ignition);for(auto *p:g.people)DISASTER_CHECK(!p->at);
 g.closeEventChoice();setDisasterTime(g,83+Time::hourToAbsolute(21));g.eventSystem.advance(0);DISASTER_CHECK(!g.eventSystem.isActive());DISASTER_CHECK(g.time.hour>=21);
 std::cout<<"Batch 2 C++ scenarios: "<<disasterChecks<<" checks passed\n";
 g.clearWorld();return 0;
}

#undef DISASTER_CHECK
#endif

// Real-engine transport scenarios; OpenSkyTests links these classes and runs
// under the remote wrapper's Xvfb display, just like the disaster harness.
#ifdef OPENSKY_TRANSPORT_SCENARIOS
#include <queue>
#include <list>
#include <deque>
#include <memory>
#include <utility>
#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <tinyxml2.h>
#define private public
#define protected public
#include "Application.h"
#include "Game.h"
#include "Item/Lobby.h"
#include "Item/Stairlike.h"
#include "Item/Office.h"
#include "Item/Condo.h"
#include "Item/FastFood.h"
#include "Item/Restaurant.h"
#include "Item/Hotel.h"
#include "Item/Parking.h"
#include "Item/Housekeeping.h"
#include "Item/Cathedral.h"
#include "Item/Elevator/Car.h"
#include "Item/Elevator/Queue.h"
#include "PathFinder/MapSearchNode.h"
#include "PathFinder/FloorNode.h"
#undef protected
#undef private

static void testBatch5Engine(OT::Game &g);
static void testBatch6Placement(OT::Game &g);
static void testElevatorDispatch()
{
 if (!graphicsScenariosAvailable()) {
  std::cout << "SKIP: Batch 4 C++ transport scenarios (no display or OPENSKY_HEADLESS=1)\n";
  return;
 }
 using namespace OT;
 using namespace OT::Item::Elevator;
 char exe[]="./OpenSkyTests", rules[]="--ruleset", sky[]="opensky";
 char *args[]={exe,rules,sky};
 Application app(3,args); app.audioMuted=true;
 app.window.create(sf::VideoMode({1280,768}),"transport scenarios");
 app.window.setVisible(false); app.gui.setWindow(app.window);
 app.data.addSearchPath("../data");
 Game g(app);
 std::vector<std::unique_ptr<Person>> riders;
 auto reset = [&]() {
  riders.clear(); g.clearWorld(); g.money.clear(20000000); g.funds=20000000; g.rating=4;
  struct Clock : Time { using Time::set; };
  Clock clock; clock.set(300.0 / Time::kSimFramesPerDay); g.time=clock;
 };
 auto make = [&](const char *id, int x, int y) {
  auto *item=g.itemFactory.make(id,int2(x,y)); g.addItem(item); item->underConstruction=false; return item;
 };
 auto shaft = [&](const char *id="elevator-standard", int x=0, int bottom=0, int height=20) {
  auto *e=static_cast<Elevator *>(make(id,x,bottom)); e->size.y=height; e->updateSprite(); return e;
 };
 auto person = [&](Person::Type type=Person::kMan) {
  riders.emplace_back(new Person(&g,type)); return riders.back().get();
 };
 auto enqueue = [&](Elevator *e, int from, int to, Person::Type type=Person::kMan) {
  auto *p=person(type); p->journey.item=e; p->journey.fromFloor=from; p->journey.toFloor=to;
  e->addPerson(p); return p;
 };
 auto tick = [&](Elevator *e, int count=1) {
  for(int i=0;i<count;++i) { g.time.absolute+=1.0/2600; g.time.dta=1.0/2600; e->advance(0); }
 };
 auto place = [&](const char *id,int x,int y) {
  g.toolPrototype=g.itemFactory.prototypesById.at(id); return g.constructToolAt(int2(x,y));
 };

 // E1: the predicate, structural stops, anchors, and sky-lobby construction.
 reset();
 for(int f=1;f<100;++f) EXPECT(Item::Lobby::isSkyLobbyFloor(f)==(f%15==14));
 EXPECT(!Item::Lobby::isSkyLobbyFloor(0)); EXPECT(!Item::Lobby::isSkyLobbyFloor(-1));
 auto *e=shaft("elevator-express",0,-2,50);
 EXPECT(e->connectsFloor(-1)); EXPECT(e->connectsFloor(0)); EXPECT(e->connectsFloor(14));
 EXPECT(e->connectsFloor(29)); EXPECT(!e->connectsFloor(15));
 e->unservicedFloors.insert(14); EXPECT(!e->connectsFloor(14));
 reset(); g.extendFloor(13,-20,20); EXPECT(place("lobby",0,14)); EXPECT(!place("lobby",0,15));
 reset(); g.extendFloor(13,-20,20); EXPECT(place("elevator-express",0,14));
 reset(); g.extendFloor(14,-20,20); EXPECT(!place("elevator-express",0,15));

 // E4: all seven frame periods, including the hidden night period.
 const int starts[]={0,400,800,1200,1600,2000,2400};
 for(int i=0;i<7;++i) {
  EXPECT(Elevator::schedulePeriod((300.0+starts[i])/2600)==i);
  if(i>0) EXPECT(Elevator::schedulePeriod((299.0+starts[i])/2600)==i-1);
 }
 EXPECT(Elevator::schedulePeriod(300.0/2600+3)==0);

 // E3: working car beats idle, strict threshold boundary, reversing costs,
 // approach priority, assigned calls, and deterministic idle ties.
 reset(); e=shaft(); auto *busy=*e->cars.begin(); e->addCar(7);
 auto *idle=e->getIdleCar(7);
 busy->setAltitude(4); busy->direction=Elevator::kUp; busy->state=Car::kMoving; busy->destinationFloor=12;
 auto *onboard=person(); onboard->journey.toFloor=12; e->Item::Item::addPerson(onboard); busy->passengers.insert(onboard);
 auto *p=enqueue(e,6,10); auto *q=e->getQueue(6,Elevator::kUp);
 EXPECT(q->assignedCar==busy); EXPECT(q->answered); EXPECT(busy->destinationFloor==6);
 reset(); e=shaft(); busy=*e->cars.begin(); e->addCar(7); idle=e->getIdleCar(7);
 e->schedules[0][0].threshold=1;
 busy->setAltitude(4); busy->direction=Elevator::kUp; busy->state=Car::kMoving; busy->destinationFloor=12;
 onboard=person(); onboard->journey.toFloor=12; e->Item::Item::addPerson(onboard); busy->passengers.insert(onboard);
 enqueue(e,6,10); EXPECT(e->getQueue(6,Elevator::kUp)->assignedCar==idle);
 reset(); e=shaft(); busy=*e->cars.begin(); e->addCar(15);
 busy->setAltitude(5); busy->direction=Elevator::kUp; busy->state=Car::kMoving; busy->destinationFloor=8;
 onboard=person(); onboard->journey.toFloor=8; e->Item::Item::addPerson(onboard); busy->passengers.insert(onboard);
 enqueue(e,2,1); EXPECT(e->getQueue(2,Elevator::kDown)->assignedCar==busy); EXPECT(busy->destinationFloor==8);
 reset(); e=shaft(); auto *first=*e->cars.begin(); first->setAltitude(4); first->homeFloor=4; first->destinationFloor=4;
 e->addCar(8); enqueue(e,6,10); EXPECT(e->getQueue(6,Elevator::kUp)->assignedCar==first);

 // E5/E2: both shuttle modes run nonstop outbound and serve both queues
 // returning. A workless car returns home rather than patrolling.
 for(int mode=1;mode<=2;++mode) {
  reset(); e=shaft("elevator-standard",0,0,11); auto *c=*e->cars.begin();
  for(int day=0;day<2;++day) for(int period=0;period<7;++period) e->schedules[day][period].mode=mode;
  c->setAltitude(5); c->homeFloor=5; c->destinationFloor=5;
  auto *up=enqueue(e,5,8); auto *down=enqueue(e,5,2);
  tick(e,5); EXPECT(c->passengers.count(up)); EXPECT(c->passengers.count(down));
  EXPECT(c->destinationFloor==(mode==1?10:0));
  tick(e,120); EXPECT(!e->people.count(up)); EXPECT(!e->people.count(down));
  EXPECT(std::abs(c->altitude-5)<0.01); EXPECT(c->state==Car::kIdle); EXPECT(c->doorTicks==0);
 }

 // Calls arriving during the outbound shuttle cannot introduce a stop.
 reset(); e=shaft("elevator-standard",0,0,11); auto *moving=*e->cars.begin();
 e->schedules[0][0].mode=kScheduleExpressUp;
 enqueue(e,3,1); EXPECT(moving->destinationFloor==10);
 tick(e); enqueue(e,7,9); EXPECT(moving->destinationFloor==10);
 tick(e,200); EXPECT(e->people.empty()); EXPECT(moving->state==Car::kIdle);
 // A simulation frame advances the docking gear exactly once, including
 // fractional frames, rather than using the old seconds-per-day base.
 reset(); e=shaft(); moving=*e->cars.begin();
 auto *rider=person(); rider->journey.toFloor=19; e->Item::Item::addPerson(rider); moving->passengers.insert(rider);
 moving->moveTo(19); g.time.dta=0.5/2600; moving->advance(0); EXPECT(moving->altitude==0);
 moving->advance(0); EXPECT(moving->altitude==1);
 tick(e,5); EXPECT(moving->altitude==1); tick(e); EXPECT(moving->altitude==2);

 // E6: exclusive distance charge, express exemption, staff span exemption,
 // and a retry does not charge the route again.
 for(int distance : {79,80,124,125}) {
  reset(); auto *origin=make("floor",0,0); origin->size.x=2;
  auto *stairs=make("stairs",distance+1,0); auto *dest=make("floor",0,1);
  Route r; r.add(origin,0); r.add(stairs,1); r.add(dest,1);
  auto *walker=person(); origin->Item::Item::addPerson(walker); walker->journey.set(r);
  EXPECT_NEAR(walker->journeyStress(),35+(distance>=125?60:distance>=80?30:0),1e-6);
  auto *staff=person(Person::kHousekeeper); origin->Item::Item::addPerson(staff); staff->journey.set(r);
  EXPECT_NEAR(staff->journeyStress(),0,1e-6);
  g.time.dta=19.0/2600; stairs->advance(0); EXPECT(walker->at==stairs);
  g.time.dta=1.0/2600; stairs->advance(0); EXPECT(walker->at==dest);
 }
 reset(); auto *origin=make("floor",0,0); origin->size.x=2; e=shaft("elevator-express",150,0,30);
 auto *dest=make("floor",0,14); Route r; r.add(origin,0); r.add(e,14); r.add(dest,14);
 p=person(); origin->Item::Item::addPerson(p); p->journey.set(r); EXPECT_NEAR(p->journeyStress(),0,1e-6);

 // E7: waits bank once, capped, with ground-only flat forgiveness.
 for(int height : {1,2,3}) for(int floor : {0,14}) {
  reset(); auto *lobby=make("lobby",0,0); lobby->size.y=height;
  e=shaft(); e->clearCars(); p=enqueue(e,floor,floor+1); q=e->getQueue(floor,Elevator::kUp);
  g.time.absolute+=60.0/2600; q->advance(0); EXPECT_NEAR(p->journeyStress(),0,1e-6);
  EXPECT(q->popPerson()==p); EXPECT_NEAR(p->journeyStress(),60-(floor==0?(height==3?50:height==2?25:0):0),1e-6);
 }
 reset(); make("lobby",0,0)->size.y=3; e=shaft("elevator-service"); e->clearCars();
 p=enqueue(e,0,5,Person::kHousekeeper); q=e->getQueue(0,Elevator::kUp); g.time.absolute+=60.0/2600;
 q->popPerson(); EXPECT_NEAR(p->journeyStress(),0,1e-6);

 reset(); make("lobby",0,0)->size.y=2; e=shaft(); e->clearCars(); p=enqueue(e,0,5);
 q=e->getQueue(0,Elevator::kUp); g.time.absolute+=350.0/2600; q->popPerson();
 EXPECT_NEAR(p->journeyStress(),275,1e-6);

 // E8: every rejected rider retains the origin, accrues its own penalty,
 // retries at exactly 16 frames, and can enter once a slot is available.
 reset(); origin=make("floor",0,0); origin->size.x=2; e=shaft(); e->clearCars();
 for(int i=0;i<40;++i) enqueue(e,0,5);
 dest=make("floor",0,5); r.clear(); r.add(origin,0); r.add(e,5); r.add(dest,5);
 p=person(); origin->Item::Item::addPerson(p); p->journey.set(r);
 auto *other=person(); origin->Item::Item::addPerson(other); other->journey.set(r);
 EXPECT(p->at==origin); EXPECT(other->at==origin); EXPECT(!e->people.count(p));
 EXPECT(p->state==Person::kPlanning); EXPECT_NEAR(p->journeyStress(),5,1e-6); EXPECT_NEAR(other->journeyStress(),5,1e-6);
 g.time.absolute+=15.0/2600; p->advance(0); EXPECT_NEAR(p->journeyStress(),5,1e-6);
 g.time.absolute+=1.0/2600; p->advance(0); EXPECT_NEAR(p->journeyStress(),10,1e-6);
 q=e->getQueue(0,Elevator::kUp); q->popPerson(); g.time.absolute+=16.0/2600; p->advance(0);
 EXPECT(p->at==e); EXPECT(!p->journey.retryPending); EXPECT(q->people.size()==40);
 EXPECT_NEAR(p->journeyStress(),10,1e-6);

 // Staggered per-person watchdog ejects only expired riders, banks the
 // capped wait once and re-arms remaining calls, including service queues.
 for(bool service : {false,true}) {
  reset(); origin=make("floor",0,0); make("lobby",0,0)->size.y=3;
  e=shaft(service?"elevator-service":"elevator-standard"); e->clearCars();
  p=enqueue(e,0,5); g.time.absolute+=100.0/2600; other=enqueue(e,0,5);
  q=e->getQueue(0,Elevator::kUp); g.time.absolute+=200.0/2600; q->advance(0);
  EXPECT(q->people.size()==1); EXPECT(q->people.front()==other); EXPECT(p->at==origin);
  EXPECT(p->journey.item==nullptr); EXPECT(p->state==Person::kPlanning);
  EXPECT(p->journey.toFloor==5); EXPECT(p->journey.retryPending);
  EXPECT_NEAR(p->stress,service?0:250,1e-6); EXPECT(q->called); EXPECT(!q->answered);
  g.time.absolute+=100.0/2600; q->advance(0); EXPECT(q->people.empty()); EXPECT(!q->called);
 }

 // Watchdog recovery is shared by workers, residents, customers and guests.
 // Preserve the final destination beyond the failed leg, retry at 16 frames,
 // tolerate missing map nodes and restore the activity when service returns.
 for(int kind=0;kind<5;++kind) {
  reset();
  auto *office=static_cast<Item::Office *>(make("office",30,5));
  auto *condo=static_cast<Item::Condo *>(make("condo",30,7));
  auto *food=static_cast<Item::FastFood *>(make("fastfood",30,9));
  // Restaurant is not registered by this factory; only its customer type
  // is needed to verify the common Person retry path.
  std::unique_ptr<Item::AbstractPrototype> restaurantPrototype(Item::Restaurant::makePrototype());
  std::unique_ptr<Item::Restaurant> restaurant(new Item::Restaurant(&g,restaurantPrototype.get()));
  auto *hotel=static_cast<Item::Hotel *>(make("hotel_single",30,13));
  switch(kind) {
   case 0: p=new Item::Office::Worker(office,Person::kMan); break;
   case 1: p=new Item::Condo::CondoOccupant(condo,Person::kMan,8,18); break;
   case 2: p=new Item::FastFood::Customer(food); break;
   case 3: p=new Item::Restaurant::Customer(restaurant.get()); break;
   default: p=new Item::Hotel::Guest(hotel); break;
  }
  riders.emplace_back(p);
  g.extendFloor(0,-20,60); g.extendFloor(5,-20,60);
  origin=g.floorItems.at(0); dest=office;
  e=shaft(); e->clearCars(); auto *transfer=shaft("elevator-standard",80,3,3); transfer->clearCars();
  g.gameMap.addNode(MapNode::Point(e->position.x+e->size.x/2,0),e);
  r.clear(); r.add(origin,0); r.add(e,3); r.add(transfer,5); r.add(dest,5);
  p->state=Person::kCommuting; origin->Item::Item::addPerson(p); p->journey.set(r);
  q=e->getQueue(0,Elevator::kUp); g.time.absolute+=300.0/2600; q->advance(0);
  EXPECT(p->at==origin); EXPECT(p->journey.toFloor==5); EXPECT(p->journey.destination==dest);
  EXPECT(p->journey.retryPending); EXPECT(p->state==Person::kPlanning);
  g.removeItem(transfer);
  // An unavailable endpoint must return no route rather than dereference null.
  auto *destinationMapNode=g.gameMap.floorNodes.at(5);
  g.gameMap.floorNodes.erase(5);
  const double firstRetry=p->journey.retryFrame;
  g.time.absolute+=15.0/2600; p->advance(0); EXPECT(p->journey.retryFrame==firstRetry);
  g.time.absolute+=1.0/2600; p->advance(0);
  EXPECT(p->journey.retryPending); EXPECT(p->journey.retryFrame>firstRetry); EXPECT(p->journey.destination==dest);
  g.gameMap.floorNodes[5]=destinationMapNode;
  // A structurally present endpoint with no elevator service also retries.
  g.toggleElevatorService(e,5); g.time.absolute+=16.0/2600; p->advance(0);
  EXPECT(p->at==origin); EXPECT(p->journey.retryPending);
  g.toggleElevatorService(e,5); g.time.absolute+=15.0/2600; p->advance(0); EXPECT(p->at==origin);
  g.time.absolute+=1.0/2600; p->advance(0);
  EXPECT(p->at==e); EXPECT(!p->journey.retryPending); EXPECT(p->state==Person::kCommuting);
  EXPECT(p->journey.toFloor==5); EXPECT(p->journey.destination==dest);
  e->addCar(0); e->respondToCalls(); tick(e,180);
  EXPECT(p->at==dest); EXPECT(!p->journey.retryPending);
  // A new owner activity supersedes recovery, and removing the retained
  // destination safely cancels a pending retry.
  p->journey.failWaiting(); r.clear(); r.add(origin,0); r.add(origin,0);
  p->state=Person::kReturning; p->journey.set(r); EXPECT(!p->journey.retryPending);
  r.clear(); r.add(origin,0); r.add(e,5); r.add(dest,5); p->journey.set(r); p->journey.failWaiting();
  g.removeItem(dest); EXPECT(!p->journey.retryPending); EXPECT(p->journey.destination==nullptr);
 }

 // Expiring one rider preserves the car already dispatched for younger riders.
 reset(); e=shaft(); e->clearCars(); p=enqueue(e,0,5);
 g.time.absolute+=100.0/2600; other=enqueue(e,0,5); q=e->getQueue(0,Elevator::kUp);
 e->addCar(10); auto *assigned=*e->cars.begin(); q->assignedCar=assigned; q->called=q->answered=true;
 g.time.absolute+=200.0/2600; q->applyWatchdog();
 EXPECT(q->people.size()==1); EXPECT(q->called); EXPECT(q->answered); EXPECT(q->assignedCar==assigned);
 g.time.absolute+=100.0/2600; q->applyWatchdog();
 EXPECT(q->people.empty()); EXPECT(!q->called); EXPECT(!q->answered); EXPECT(q->assignedCar==nullptr);

 // E9: actual horizontal stairs/escalator distance and replacement full cost.
 for(const char *id : {"stairs","escalator","elevator-standard","elevator-service"}) {
  reset(); auto *transport=make(id,30,0);
  std::list<MapNode *> list; FloorNode floorNode(&list); MapNode a(&floorNode), b(&floorNode);
  a.position=MapNode::Point(34,0); b.position=MapNode::Point(34,1);
  a.neighbours[MapNode::UP]=&b; a.transportItems[MapNode::UP]=transport;
  MapSearchNode start(&a), finish(&b); start.start_point=MapNode::Point(10,0);
  if(transport->isElevator()) {
   auto *el=static_cast<Elevator *>(transport); el->clearCars();
   for(int i=0;i<40;++i) enqueue(el,0,1);
   const int cells=transport->position.x+transport->size.x/2-10;
   EXPECT_NEAR(start.GetCost(finish),8*cells+1000,1e-6);
   start.numElevators=1; EXPECT_NEAR(start.GetCost(finish),8*cells+6000,1e-6);
  } else EXPECT_NEAR(start.GetCost(finish),8*20+(std::string(id)=="stairs"?640:0),1e-6);
 }

 // E10/E11: 30-floor count and clearance between every shaft type.
 reset(); e=shaft(); EXPECT(e->canResizeTo(0,30)); EXPECT(!e->canResizeTo(0,31));
 for(const char *id : {"elevator-standard","elevator-service","elevator-express"}) {
  reset(); auto *existing=shaft(id,0,0,10); e=shaft("elevator-standard",11,0,10);
  EXPECT(!e->canResizeTo(0,10)); e->position.x=existing->size.x+8; EXPECT(e->canResizeTo(0,10));
  e->position.x=0; EXPECT(!e->canResizeTo(11,1)); EXPECT(e->canResizeTo(12,1));
  EXPECT(existing!=e);
  // Candidate inclusive top 9 plus one touches the other shaft at floor 10.
  existing->position.y=10; e->position.y=0;
  EXPECT(!e->canResizeTo(0,10)); EXPECT(e->canResizeTo(0,9));
  reset(); shaft(id,0,0,10); g.extendFloor(0,-20,30); EXPECT(!place("elevator-standard",11,0));
 }

 // E12: escalators use the stairlike path, landing whitelist, construction,
 // and one shared 64-record pool for stairs and escalators.
 reset(); g.extendFloor(0,-20,40); g.extendFloor(1,-20,40); EXPECT(place("escalator",0,0));
 reset(); g.extendFloor(1,-20,40); g.extendFloor(2,-20,40); make("office",0,1);
 EXPECT(!place("escalator",0,1)); EXPECT(place("stairs",16,1));
 reset(); g.extendFloor(1,-20,40); g.extendFloor(2,-20,40); make("shop",0,1)->underConstruction=true;
 EXPECT(!place("escalator",0,1));
 reset(); g.extendFloor(1,-20,40); g.extendFloor(2,-20,40); make("shop",0,1); EXPECT(place("escalator",0,1));
 reset(); g.extendFloor(1,-20,40); g.extendFloor(2,-20,40); make("office",7,2);
 EXPECT(!place("escalator",0,1));
 reset(); g.extendFloor(1,-20,40); EXPECT(!place("escalator",0,1));
 reset(); for(int i=0;i<64;++i) make(i%2?"stairs":"escalator",(i%16)*8-100,3+(i/16)*2);
 g.extendFloor(1,-20,40); g.extendFloor(2,-20,40);
 EXPECT(!place("stairs",0,1)); EXPECT(!place("escalator",0,1)); EXPECT(g.itemsByType["stairlike"].size()==64);

 // E13: lobby/home patience applies at sky floors and ground mezzanines.
 for(int floor : {0,1,14,29,7,8}) {
  reset(); make("lobby",0,0)->size.y=3; e=shaft("elevator-standard",0,0,30); auto *c=*e->cars.begin();
  c->homeFloor=7; c->setAltitude(floor); c->destinationFloor=floor; c->doorTicks=1; c->holdTicks=2; c->state=Car::kHauling;
  tick(e); EXPECT(c->holdTicks==(floor==8?2:1)); EXPECT(c->doorTicks==(floor==8?0:1));
 }

 // E14: service toggles cannot inject structurally impossible map stops,
 // and the dialog reflects those floors as OFF.
 reset(); g.extendFloor(0,-20,20); g.extendFloor(5,-20,20); g.extendFloor(14,-20,20);
 e=shaft("elevator-express",0,0,30); e->unservicedFloors.insert(5);
 auto *invalidQueue=e->getQueue(5,Elevator::kUp);
 const auto queueCount=e->queues.size(); const auto serviceFloors=e->unservicedFloors;
 const auto pendingRoutes=g.routeBuildQueue;
 g.toggleElevatorService(e,5); EXPECT(!e->connectsFloor(5));
 EXPECT(e->unservicedFloors==serviceFloors); EXPECT(e->queues.size()==queueCount);
 EXPECT(e->getQueue(5,Elevator::kUp)==invalidQueue); EXPECT(g.routeBuildQueue==pendingRoutes);
 g.toggleElevatorService(e,-1); g.toggleElevatorService(e,30);
 EXPECT(e->unservicedFloors==serviceFloors); EXPECT(e->queues.size()==queueCount);
 EXPECT(g.routeBuildQueue==pendingRoutes);
 EXPECT(!g.gameMap.findNode(MapNode::Point(3,5),e));
 g.elevatorDialog.currentElevator=e; EXPECT(g.elevatorDialog.buttonLabel(5).find("[OFF]")!=std::string::npos);
 g.toggleElevatorService(e,14); EXPECT(!e->connectsFloor(14));
 g.toggleElevatorService(e,14); EXPECT(e->connectsFloor(14)); EXPECT(g.gameMap.findNode(MapNode::Point(3,14),e));
 g.elevatorDialog.currentElevator=nullptr;
 riders.clear(); g.clearWorld();
 testBatch5Engine(g);
 testBatch6Placement(g);
 std::cout<<"Batch 4 C++ transport scenarios completed\n";
}
#endif


// batch5-A: tenant record rules, noise scan, population arithmetic, name registry.
static void testBatch5ATenantRules()
{
    using namespace OT::TenantRules;

    // T2: int8/int16 wrap and signed truncating mean.
    Bank b;
    EXPECT(bankedMean(b) == 0);
    bankAdd(b, 100.4); bankAdd(b, 51.6);
    EXPECT(b.count == 2 && b.total == 152);
    EXPECT(bankedMean(b) == 76);
    Bank w;
    for (int i = 0; i < 128; ++i) bankAdd(w, 1.0);
    EXPECT(w.count == -128);               // int8 wrap
    Bank big; bankAdd(big, 30000.0); bankAdd(big, 30000.0);
    EXPECT(big.total == (int16_t)60000);   // int16 wrap
    EXPECT(bankedMean((int8_t)3, (int16_t)-10) == -3);
    EXPECT(bankedMean((int8_t)0, (int16_t)500) == 0);

    // T3: divisors, class adjustment, noise, clamp, bars.
    Bank office[6];
    for (int i = 0; i < 6; ++i) { office[i].count = 1; office[i].total = 100; }
    EXPECT(rawMetric(office, 6, divisorFor(kOffice)) == 100);
    Bank travelled[2]; travelled[0].count = 1; travelled[0].total = 90;
    EXPECT(rawMetric(travelled, 2, divisorFor(kTwin)) == 45);       // nontraveller contributes 0
    EXPECT(rawMetric(travelled, 1, divisorFor(kSingle)) == 90);
    EXPECT(divisorFor(kCondo) == 3 && divisorFor(kSuite) == 2);
    EXPECT(adjustMetric(100, 0, false) == 130);
    EXPECT(adjustMetric(100, 1, false) == 100);
    EXPECT(adjustMetric(100, 2, false) == 70);
    EXPECT(adjustMetric(100, 3, false) == 0);
    EXPECT(adjustMetric(100, 3, true) == 60);                       // class 3 then noise
    EXPECT(adjustMetric(10, 2, false) == 0);                        // clamped at 0
    EXPECT(adjustMetric(500, 0, true) == 590);                      // no upper clamp
    EXPECT(categoryFor(79, 0) == kHappy && categoryFor(80, 0) == kOk);
    EXPECT(categoryFor(149, 0) == kOk && categoryFor(150, 0) == kStressed);
    EXPECT(categoryFor(150, 3) == kOk && categoryFor(199, 3) == kOk && categoryFor(200, 3) == kStressed);
    EXPECT(categoryFor(-1, 0) == kUnjudged);
    EXPECT(judgeCategory(office, kOffice, 1, false, false, 0) == kUnjudged);
    EXPECT(judgeCategory(office, kOffice, 1, false, true, 0) == kOk);
    EXPECT(judgeCategory(office, kOffice, 1, true, true, 0) == kStressed);   // 160 >= 150
    EXPECT(evaluationFor(kHappy) == 90 && evaluationFor(kOk) == 60 &&
           evaluationFor(kStressed) == 20 && evaluationFor(kUnjudged) == 50);

    // T4: noise scan. Slots in slot order; victim is a hotel (range 20).
    {
        Slot s[5] = {
            {0, 10, kSlotRestaurant}, {10, 30, kSlotOther}, {30, 40, kSlotHotel},
            {40, 50, kSlotOther}, {50, 60, kSlotShop}};
        // Left: the first record is tested after the decrement, the loop stops on the
        // first source found. 30-20=10: slot 1 (left 10) passes, slot 0 is tested.
        EXPECT(noiseSourceIndex(s, 5, 2) == 0);   // left source wins over the right shop
        s[0].type = kSlotOther;
        EXPECT(noiseSourceIndex(s, 5, 2) == 4);     // right: shop at 50 <= 40+20
        // Cutoff: the record after the decrement is tested, then the range check stops the scan.
        Slot g[3] = {{0, 10, kSlotShop}, {25, 35, kSlotOther}, {60, 70, kSlotHotel}};
        EXPECT(noiseSourceIndex(g, 3, 2) == -1);    // slot 1 tested, left 25 < 40 stops the scan
        Slot h[3] = {{0, 10, kSlotShop}, {45, 55, kSlotShop}, {60, 70, kSlotHotel}};
        EXPECT(noiseSourceIndex(h, 3, 2) == 1);
        // Source sets by victim.
        EXPECT(isNoiseSource(kSlotOffice, kSlotOffice) == false);
        EXPECT(isNoiseSource(kSlotOffice, kSlotHotel) && isNoiseSource(kSlotOffice, kSlotCondo));
        EXPECT(!isNoiseSource(kSlotHotel, kSlotHotel) && isNoiseSource(kSlotHotel, kSlotCondo));
        EXPECT(isNoiseSource(kSlotCinema, kSlotOffice) && isNoiseSource(kSlotParty, kSlotCondo));
        EXPECT(noiseRange(kSlotOffice) == 10 && noiseRange(kSlotHotel) == 20 && noiseRange(kSlotCondo) == 30);
    }

    // T17/T18: roach spread, neglect to infested at tenure 3, arm / rescue.
    {
        Room r[5];
        for (int i = 0; i < 5; ++i) r[i].hotel = true;
        r[2].state = kRoomInfested;
        roachSpread(r, 5);
        EXPECT(r[1].state == kRoomInfested && r[3].state == kRoomInfested);
        EXPECT(r[0].state == kRoomClean && r[4].state == kRoomClean);   // no snapshot cascade
        EXPECT(!r[1].armed && r[1].category == kUnjudged && r[1].redraw);
        Room gap[3]; gap[0].hotel = true; gap[1].hotel = false; gap[2].hotel = true;
        gap[0].state = kRoomInfested;
        roachSpread(gap, 3);
        EXPECT(gap[2].state == kRoomClean);                              // gap blocks the spread
        Room n[1]; n[0].hotel = true; n[0].state = kRoomDirty; n[0].armed = true; n[0].tenure = 5;
        hotelNeglect(n, 1);
        EXPECT(!n[0].armed && n[0].tenure == 0 && n[0].category == kStressed);
        hotelNeglect(n, 1); hotelNeglect(n, 1); EXPECT(n[0].tenure == 2 && n[0].state == kRoomDirty);
        hotelNeglect(n, 1); EXPECT(n[0].tenure == 3 && n[0].state == kRoomInfested);
        Room a[3];
        for (int i = 0; i < 3; ++i) { a[i].hotel = true; a[i].type = 1; }
        a[0].category = 0; a[1].category = 2; a[2].category = 255;
        hotelArmRescue(a, 3);
        EXPECT(a[0].armed && a[0].category == kOk && a[1].category == kOk && a[0].bankReset);
        EXPECT(!a[2].armed && !a[2].bankReset);                          // 255 does neither
        a[0].armed = false; a[0].category = 0; a[1].category = 1;
        hotelArmRescue(a, 3);
        EXPECT(!a[0].armed);                                             // no twin: disarm
    }

    // T24: maid pick slices and exact dirty state.
    {
        const int f[4] = {11, 12, 17, 5};
        const int st[4] = {kRoomDirty, kRoomInfested, kRoomDirty, kRoomDirty};
        const bool hotel[4] = {true, true, true, true};
        EXPECT(pickDirtyRoom(f, st, hotel, 4, 10, 5) == 0);   // f%6==5 -> floor 11 first
        EXPECT(pickDirtyRoom(f, st, hotel, 4, 12, 5) == 2);   // 17 above the anchor
        EXPECT(pickDirtyRoom(f, st, hotel, 4, 12, 0) == -1);  // floor 12 is infested only
        EXPECT(pickDirtyRoom(f, st, hotel, 4, 4, 5) == 3);    // floor 5 is first from below the anchor
    }

    // T14: CountT arithmetic.
    {
        CountT c;
        c.add(kCountOffice, 6); c.add(kCountCondo, 3); c.add(kCountShop, 10);
        EXPECT(c.population == 19 && c.dirty);
        c.add(kCountOffice, -6);
        EXPECT(c.population == 13);
        c.reset(kCountShop);                      // the shop +10 leaks until the daily reset
        EXPECT(c.population == 3 && c.cat[kCountShop] == 0);
        EXPECT(c.resetOnce(kCountFastFood, 4) && !c.resetOnce(kCountFastFood, 4) && c.resetOnce(kCountFastFood, 5));
        c.add(-1, 5);                             // unrecognised still changes the global
        EXPECT(c.population == 8);
    }

    // T7/T8: shop and food categories.
    EXPECT(shopCategory(false, 99, 1) == kUnjudged);
    EXPECT(shopCategory(true, 25, 1) == kHappy && shopCategory(true, 24, 1) == kOk);
    EXPECT(shopCategory(true, 19, 1) == kStressed);
    EXPECT(shopCategory(true, 30, 0) == kHappy && shopCategory(true, 29, 0) == kOk);   // +5 shift
    EXPECT(shopCategory(true, 13, 3) == kHappy && shopCategory(true, 12, 3) == kOk);    // -12 shift
    EXPECT(foodCategory(50) == 3 && foodCategory(35) == 2 && foodCategory(25) == 1 && foodCategory(24) == 0);

    // T42, T48, zones, strings.
    EXPECT(recyclingRequired(499) == 1 && recyclingRequired(500) == 2 && recyclingRequired(2500) == 6);
    EXPECT(vipVerdict(300, 2) == 1 && vipVerdict(302, 2) == 2 && vipVerdict(0, 0) == 1);
    EXPECT(registrationZone(10) == 1 && registrationZone(5) == -1 && registrationZone(-5) == 0);
    EXPECT(registrationZone(-6) == 0 && registrationZone(-20) == -1 && registrationZone(100) == -1);
    EXPECT(lookupZone(-3) == 0 && lookupZone(14) == 1 && lookupZone(0) == 0);
    EXPECT(std::string(businessLine(50)) == "Business is very good!");
    EXPECT(std::string(businessLine(10)) == "Very few customers");
    EXPECT(std::string(cinemaLine(0, 0)) == "New movie showing!");
    EXPECT(std::string(cinemaLine(5, 60)) == "It's a Sellout!");
    EXPECT(std::string(cinemaLine(5, 10)) == "Terrible sales! Change the movie!");
    EXPECT(distanceTier(79) == 0 && distanceTier(80) == 1 && distanceTier(125) == 2);
}

static void testBatch5ANames()
{
    using OT::NameManager;
    using Role = NameManager::Role;
    NameManager::reset();
    EXPECT(NameManager::genericLabel(Role::HotelGuest, 2, 0, 9) == "Woman");
    EXPECT(NameManager::genericLabel(Role::HotelGuest, 1, 0, 9) == "Man");
    EXPECT(NameManager::genericLabel(Role::Office, 1, 1, 9) == "Salesman");
    EXPECT(NameManager::genericLabel(Role::Office, 3, 0, 9) == "Man");
    EXPECT(NameManager::genericLabel(Role::Office, 5, 4, 9) == "Woman");
    EXPECT(NameManager::genericLabel(Role::Condo, 1, 6, 9) == "Mother with Baby");
    EXPECT(NameManager::genericLabel(Role::Condo, 2, 3, 9) == "Child");
    EXPECT(NameManager::genericLabel(Role::Visitor, 9, 2, 9) == "Woman");        // 9 & 7 = 1
    EXPECT(NameManager::genericLabel(Role::Visitor, 5, 2, 9) == "Woman with Kid");
    EXPECT(NameManager::genericLabel(Role::Visitor, 7, 2, 9) == "Mother with Baby");
    EXPECT(NameManager::genericLabel(Role::Visitor, 4, 2, 9) == "Man");
    EXPECT(NameManager::genericLabel(Role::Staff, 0, 8, 9) == "Security");
    EXPECT(NameManager::genericLabel(Role::Staff, 0, 5, 16.5) == "Housekeeper");
    EXPECT(NameManager::genericLabel(Role::Staff, 0, 5, 17.5) == "Janitor");
    EXPECT(NameManager::personLabel("Bob", true, Role::Office, 0, 0, 9) == "Bob");
    EXPECT(NameManager::personLabel("", true, Role::Office, 0, 0, 9) == "VIP");
    EXPECT(NameManager::personLabel("", false, Role::Office, 0, 0, 9) == "Salesman");

    EXPECT(NameManager::whereabouts("Lobby to eat", "Office", "Floor 3") == "Lobby to eat");
    EXPECT(NameManager::whereabouts("Medical Center", "Office", "Floor 9") == "Medical Center, Floor 9");
    EXPECT(NameManager::whereabouts("", "Condo", "Floor 12") == "Condo, Floor 12");

    // Registry: 20 slots, 15 chars, empty deletes, replace keeps the slot.
    int tenants[25];
    for (int i = 0; i < 20; ++i) EXPECT(NameManager::setCustomName(&tenants[i], 0, "A", NameManager::kNameHotel));
    EXPECT(!NameManager::setCustomName(&tenants[20], 0, "B"));                 // full: reject new
    EXPECT(NameManager::setCustomName(&tenants[3], 0, "0123456789ABCDEFGHI"));  // replace still works
    EXPECT(NameManager::customName(&tenants[3], 0) == "0123456789ABCDE");
    EXPECT(NameManager::customNameCount() == 20);
    EXPECT(NameManager::setCustomName(&tenants[3], 0, ""));
    EXPECT(NameManager::customName(&tenants[3], 0).empty() && NameManager::customNameCount() == 19);
    EXPECT(NameManager::setCustomName(&tenants[20], 1, "Z", NameManager::kNameVisitor));
    NameManager::purgeClass(NameManager::kNameHotel);
    EXPECT(NameManager::customNameCount() == 1);
    NameManager::removeTenant(&tenants[20]);
    EXPECT(NameManager::customNameCount() == 0);
    NameManager::reset();
}

// batch5-B
// Pure logic of the hotel / maid / batch-5 rules (TenantRules.h).
static OT::TenantRules::Room b5Room(bool hotel, int state, int type = 0)
{
	OT::TenantRules::Room r;
	r.hotel = hotel; r.state = state; r.type = type;
	return r;
}

static void testBatch5BRoaches()
{
	using namespace OT::TenantRules;
	// Source in the middle stamps both neighbours (the following one is skipped).
	{
		Room r[5] = {b5Room(true, 0), b5Room(true, 0), b5Room(true, kRoomInfested), b5Room(true, 0), b5Room(true, 0)};
		roachSpread(r, 5);
		EXPECT(r[1].state == kRoomInfested && r[3].state == kRoomInfested);
		EXPECT(r[0].state == 0 && r[4].state == 0);
		EXPECT(r[1].category == 255 && !r[1].armed && r[1].redraw);
	}
	// A chain: a stamped following room is not itself a source this pass.
	{
		Room r[4] = {b5Room(true, kRoomInfested), b5Room(true, 0), b5Room(true, 0), b5Room(true, 0)};
		roachSpread(r, 4);
		EXPECT(r[1].state == kRoomInfested && r[2].state == 0);
	}
	// An already infested follower becomes the next source in the same scan.
	{
		Room r[5] = {b5Room(true, kRoomInfested), b5Room(true, kRoomInfested), b5Room(true, 0), b5Room(true, 0), b5Room(true, 0)};
		roachSpread(r, 5);
		EXPECT(r[2].state == kRoomInfested && r[3].state == 0);
	}
	// Non-hotel records are never stamped; a non-hotel follower is stepped over.
	{
		Room r[4] = {b5Room(false, 0), b5Room(true, kRoomInfested), b5Room(false, 0), b5Room(true, 0)};
		roachSpread(r, 4);
		EXPECT(r[0].state == 0 && r[2].state == 0 && r[3].state == 0);
	}
	// The preceding room is stamped whatever its old state (even occupied).
	{
		Room r[2] = {b5Room(true, kRoomOccupied), b5Room(true, kRoomInfested)};
		r[0].armed = true;
		roachSpread(r, 2);
		EXPECT(r[0].state == kRoomInfested && !r[0].armed);
	}
}

static void testBatch5BNeglectAndRescue()
{
	using namespace OT::TenantRules;
	// An armed dirty room is disarmed and stressed with tenure reset; then
	// tenure climbs once per pass and the third pass infests (tenure == 3).
	Room r[1] = {b5Room(true, kRoomDirty)};
	r[0].armed = true; r[0].tenure = 2;
	hotelNeglect(r, 1);
	EXPECT(!r[0].armed && r[0].category == 0 && r[0].tenure == 0);
	hotelNeglect(r, 1); EXPECT(r[0].tenure == 1 && r[0].state == kRoomDirty);
	hotelNeglect(r, 1); EXPECT(r[0].tenure == 2 && r[0].state == kRoomDirty);
	hotelNeglect(r, 1); EXPECT(r[0].tenure == 3 && r[0].state == kRoomInfested && r[0].category == 255);
	// Clean rooms are not touched by the neglect pass.
	Room clean[1] = {b5Room(true, kRoomClean)};
	hotelNeglect(clean, 1);
	EXPECT(clean[0].tenure == 0 && clean[0].state == kRoomClean);

	// Arm / rescue: category >= 1 arms; 0 pairs with a happy same-type room.
	Room a[4] = {b5Room(true, kRoomClean, 1), b5Room(true, kRoomClean, 1), b5Room(true, kRoomClean, 2), b5Room(true, kRoomDirty, 1)};
	a[0].category = 1; a[1].category = 0; a[2].category = 255; a[3].category = 1;
	hotelArmRescue(a, 4);
	EXPECT(a[0].armed && a[0].bankReset);
	EXPECT(!a[1].armed);                    // dirty rooms are not rescue partners
	EXPECT(!a[2].armed && !a[2].bankReset); // 255 neither arms nor rescues
	Room b[3] = {b5Room(true, kRoomClean, 1), b5Room(true, kRoomClean, 1), b5Room(true, kRoomClean, 2)};
	b[0].category = 0; b[1].category = 2; b[2].category = 2;
	hotelArmRescue(b, 3);
	EXPECT(b[0].category == 1 && b[1].category == 1);
	EXPECT(b[0].armed && b[0].bankReset);
}

static void testBatch5BMaidPickAndVenueZones()
{
	using namespace OT::TenantRules;
	// Maid k only serves floors with f % 6 == k; infested rooms are never picked.
	const int floorF[5] = {12, 12, 18, 13, 7};
	const int state[5]  = {kRoomInfested, kRoomDirty, kRoomDirty, kRoomDirty, kRoomDirty};
	const bool hotel[5] = {true, true, true, true, true};
	EXPECT(pickDirtyRoom(floorF, state, hotel, 5, 10, 0) == 1); // f12 % 6 == 0, first dirty, not the infested one
	EXPECT(pickDirtyRoom(floorF, state, hotel, 5, 13, 0) == 2); // upward from 13 reaches f18
	EXPECT(pickDirtyRoom(floorF, state, hotel, 5, 19, 0) == 2); // nothing above: scans downward
	EXPECT(pickDirtyRoom(floorF, state, hotel, 5, 10, 1) == 3); // f13 % 6 == 1 (and f7)
	EXPECT(pickDirtyRoom(floorF, state, hotel, 5, 10, 2) == -1); // no fallback to other slices

	// Zone formulas: ground floor registers in zone 0; floors 5..9 fail registration.
	EXPECT(registrationZone(0) == 0 && registrationZone(4) == 0);
	for (int f = 5; f <= 9; ++f) EXPECT(registrationZone(f) == -1);
	EXPECT(registrationZone(10) == 1 && registrationZone(-3) == 0);
	EXPECT(lookupZone(0) == 0 && lookupZone(-20) == 0 && lookupZone(14) == 1);
}

static void testBatch5BRecyclingAndVip()
{
	using namespace OT::TenantRules;
	EXPECT(recyclingRequired(0) == 1 && recyclingRequired(499) == 1 && recyclingRequired(500) == 2);
	EXPECT(recyclingRequired(1499) == 3 && recyclingRequired(1999) == 4 && recyclingRequired(2499) == 5);
	EXPECT(recyclingRequired(2500) == 6 && recyclingRequired(99999) == 6);
	EXPECT(vipVerdict(0, 0) == 1);           // no trips averages 0
	EXPECT(vipVerdict(300, 2) == 1);         // 150 is still favorable
	EXPECT(vipVerdict(302, 2) == 2);         // 151 is unfavorable
	EXPECT(vipVerdict(301, 2) == 1);         // 150 (truncated) is favorable
}

#ifdef OPENSKY_TRANSPORT_SCENARIOS
// Engine scenarios for the batch-5 fixes (F1-F9); runs in OpenSkyTransportTests.
static void testBatch5Engine(OT::Game &g)
{
 using namespace OT;
 auto setTime=[&](double absolute) {
  struct Clock : Time { using Time::set; };
  Clock clock; clock.set(absolute); g.time=clock; g.time.dta=1.0/2600;
 };
 auto reset=[&](double absolute=3.0+Time::hourToAbsolute(23)) {
  g.clearWorld(); g.money.clear(20000000); g.funds=20000000; g.rating=2;
  g.vipSystem.reset(); setTime(absolute);
 };
 auto make=[&](const char *id,int x,int y) {
  auto *item=g.itemFactory.make(id,int2(x,y)); g.addItem(item); item->underConstruction=false; return item;
 };
 auto makeParking=[&]() {
  auto *park=static_cast<Item::Parking *>(make("parking",0,-1));
  park->bayList[0].usable=true; return park;
 };
 auto makeCondo=[&](int x,int total) {
  auto *c=static_cast<Item::Condo *>(make("condo",x,1));
  c->occupied=true; c->tenant.armed=true;
  for (int k=0;k<6;++k) { c->tenant.bank[k].count=1; c->tenant.bank[k].total=(int16_t)total; }
  return c;
 };

 // F3: a journey banks exactly once, even if next() is driven past the end.
 {
  reset(); auto *lobby=make("lobby",0,0); auto *office=make("office",10,1);
  g.mainLobby=lobby;
  std::unique_ptr<Person> p(new Person(&g,Person::kMan)); p->home=office;
  Route r; r.add(lobby); r.add(office);
  p->journey.set(r);
  EXPECT(p->at==office); EXPECT(p->bankCount==1);
  p->journey.next();
  EXPECT(p->bankCount==1);
 }

 // F1: the judge's quarter predicate is the caller's flag, not time.day, and
 // buybacks land in the quarter opened first.
 {
  reset(3.0+Time::hourToAbsolute(5)); // weekday index != 0
  auto *condo=makeCondo(0,2000);
  g.judgeSystem.judgeDaily(&g,false);
  EXPECT(condo->occupied);                       // no move-out on a non-quarter day
  reset(7.0+Time::hourToAbsolute(5));            // day 0 of the week, not a quarter day
  condo=makeCondo(0,2000);
  g.judgeSystem.judgeDaily(&g,false);
  EXPECT(condo->occupied);                       // the old `day == 0` test would have evicted here
  reset(3.0+Time::hourToAbsolute(5));
  condo=makeCondo(0,2000);
  g.money.finalizeQuarter();                     // (a) new quarter opens first
  const int before=g.money.quarterExpenses;
  g.judgeSystem.judgeDaily(&g,true);             // (b) judge + move-out
  EXPECT(!condo->occupied);
  EXPECT(g.money.quarterExpenses>before);        // buyback booked in the new quarter
 }

 // F5: the rescuing peer's evaluation is refreshed with its category.
 {
  reset(3.0+Time::hourToAbsolute(5));
  auto *leaver=makeCondo(0,2000); auto *peer=makeCondo(8,10);
  g.judgeSystem.judgeOne(leaver); g.judgeSystem.judgeOne(peer);
  g.judgeSystem.judgeDaily(&g,true);
  EXPECT(!leaver->occupied || leaver->tenant.category==TenantRules::kOk);
  EXPECT(peer->tenant.category==TenantRules::kOk);
  EXPECT(peer->evaluation==TenantRules::evaluationFor(TenantRules::kOk));
 }

 // F2: the bay is held through errands and the trip home; it frees only when
 // the driver reaches the parking item.
 {
  reset(); auto *park=makeParking(); auto *lobby=make("lobby",0,0); g.mainLobby=lobby;
  auto *office=static_cast<Item::Office *>(make("office",10,1));
  office->occupied=true; office->tenant.armed=true;
  Item::Office::Worker *w=office->crewMember(2);
  EXPECT(park->assignSpace(w,Item::Parking::kOfficeCar)); w->parkingUsed=park;
  // Out to lunch (away from the desk): car stays.
  w->state=Person::kLunch; lobby->Item::Item::addPerson(w); w->lunchReturnTime=g.time.absolute+1;
  setTime(g.time.absolute); office->advance(0);
  EXPECT(w->parkingUsed==park); EXPECT(park->usedSpaces()==1);
  w->at->removePerson(w);
  // Heading home but still inside the tower: car stays.
  w->state=Person::kReturning; w->lunchReturnTime=0; office->addPerson(w);
  office->advance(0);
  EXPECT(w->parkingUsed==park); EXPECT(park->usedSpaces()==1);
  office->removePerson(w);
  // Reaches the parking item: bay frees.
  Route r; r.add(office); r.add(park); w->state=Person::kReturning; w->journey.set(r);
  EXPECT(w->at==park); EXPECT(park->usedSpaces()==1);
  office->advance(0);
  EXPECT(w->parkingUsed==nullptr); EXPECT(park->usedSpaces()==0);
 }

 // F4: parked cars and their drivers survive a save/load round trip.
 {
  reset(); auto *park=makeParking(); auto *lobby=make("lobby",0,0); g.mainLobby=lobby;
  auto *office=static_cast<Item::Office *>(make("office",10,1)); office->occupied=true;
  auto *hotel=static_cast<Item::Hotel *>(make("hotel_suite",30,2));
  auto *guest=new Item::Hotel::Guest(hotel,1); hotel->guests.insert(guest);
  Item::Office::Worker *w=office->crewMember(2);
  EXPECT(park->assignSpace(w,Item::Parking::kOfficeCar)); w->parkingUsed=park;
  EXPECT(park->assignSpace(guest,Item::Parking::kSuiteCar)); guest->parkingUsed=park;
  tinyxml2::XMLPrinter pr; pr.OpenElement("parking"); park->encodeXML(pr); pr.CloseElement();
  tinyxml2::XMLDocument doc; EXPECT(doc.Parse(pr.CStr())==tinyxml2::XML_SUCCESS);
  // "Load": drivers forgotten, then the saved element decoded and relinked.
  park->freeSpace(w); park->freeSpace(guest); w->parkingUsed=nullptr; guest->parkingUsed=nullptr;
  EXPECT(park->usedSpaces()==0);
  park->decodeXML(*doc.RootElement());
  Item::Parking::relinkAll(&g);
  EXPECT(park->usedSpaces()==2);
  EXPECT(w->parkingUsed==park); EXPECT(guest->parkingUsed==park);
  EXPECT(Item::Parking::carsParked(&g,Item::Parking::kOfficeCar)==1);
  EXPECT(Item::Parking::carsParked(&g,Item::Parking::kSuiteCar)==1);
 }

 // F7: a checkout with no exit route still frees the bay.
 {
  reset(); auto *park=makeParking();
  auto *hotel=static_cast<Item::Hotel *>(make("hotel_suite",30,2));
  auto *guest=new Item::Hotel::Guest(hotel,0); hotel->guests.insert(guest);
  EXPECT(park->assignSpace(guest,Item::Parking::kSuiteCar)); guest->parkingUsed=park; guest->entry=park;
  hotel->checkout(true);
  EXPECT(guest->parkingUsed==nullptr); EXPECT(park->usedSpaces()==0);
 }

 // F8: the VIP visit arms from the daily tick, not from a clock poll.
 {
  reset(3.0+Time::hourToAbsolute(5)); g.rating=2;
  g.vipSystem.advance(0);
  EXPECT(!g.vipSystem.watch);
  g.vipSystem.armDaily();
  EXPECT(g.vipSystem.watch);
 }

 // F9: destroyed owners leave no dangling driver, VIP or maid pointers.
 {
  reset(); auto *park=makeParking(); auto *lobby=make("lobby",0,0); g.mainLobby=lobby;
  Person *d=new Person(&g,Person::kMan);
  EXPECT(park->assignSpace(d,Item::Parking::kOfficeCar));
  delete d;
  EXPECT(park->usedSpaces()==0);
  auto *office=static_cast<Item::Office *>(make("office",10,1)); office->occupied=true;
  EXPECT(park->assignSpace(office->crewMember(2),Item::Parking::kOfficeCar));
  office->crewMember(2)->parkingUsed=park;
  g.removeItem(office);
  EXPECT(park->usedSpaces()==0);
  auto *hotel=static_cast<Item::Hotel *>(make("hotel_suite",30,2));
  auto *guest=new Item::Hotel::Guest(hotel,0); hotel->guests.insert(guest);
  EXPECT(park->assignSpace(guest,Item::Parking::kSuiteCar)); guest->parkingUsed=park;
  g.vipSystem.vipGuest=guest;
  auto *hk=static_cast<Item::Housekeeping *>(make("housekeeping",50,1));
  hk->maidStaff[0]=new Item::Housekeeping::Maid(hk,0); hk->maidStaff[0]->target=hotel; hk->maidStaff[0]->mode=1;
  g.removeItem(hotel);
  EXPECT(park->usedSpaces()==0);
  EXPECT(g.vipSystem.vipGuest==nullptr);
  EXPECT(hk->maidStaff[0]->target==nullptr);
 }
 g.clearWorld();
 std::cout<<"Batch 5 C++ engine scenarios completed\n";
}

// Batch 6: build/placement rules, demolish rules, grand-lobby height lock.
static void testBatch6Placement(OT::Game &g)
{
 using namespace OT;
 auto reset=[&]() {
  g.clearWorld(); g.money.clear(20000000); g.funds=20000000; g.rating=5;
  struct Clock : Time { using Time::set; };
  Clock clock; clock.set(3.0+Time::hourToAbsolute(12)); g.time=clock;
 };
 auto make=[&](const char *id,int x,int y) {
  auto *item=g.itemFactory.make(id,int2(x,y)); g.addItem(item); item->underConstruction=false; return item;
 };
 std::string msg;
 auto place=[&](const char *id,int x,int y) {
  g.toolPrototype=g.itemFactory.prototypesById.at(id); g.timeWindow.message.clear();
  const bool ok=g.constructToolAt(int2(x,y)); msg=g.timeWindow.message; return ok;
 };
 auto rejects=[&](const char *id,int x,int y,const char *text) {
  const bool ok=place(id,x,y);
  const bool match=!ok && msg.find(text)!=std::string::npos;
  if (!match) std::cerr<<"B6 reject mismatch: "<<id<<" at "<<x<<","<<y<<" ok="<<ok<<" msg='"<<msg<<"' want '"<<text<<"'\n";
  return match;
 };
 auto accepts=[&](const char *id,int x,int y) {
  const bool ok=place(id,x,y);
  if (!ok) std::cerr<<"B6 accept failed: "<<id<<" at "<<x<<","<<y<<" msg='"<<msg<<"'\n";
  return ok;
 };

 // B1: ceiling F100, basements to B10, shafts stop at B9.
 reset(); g.extendFloor(99,-20,20);
 EXPECT(accepts("office",0,100));
 EXPECT(rejects("office",0,101,"Maximum height has been reached"));
 reset(); g.extendFloor(-9,-20,20);
 EXPECT(accepts("fastfood",0,-10));
 reset(); g.extendFloor(-10,-20,20);
 EXPECT(rejects("fastfood",0,-11,"Maximum height has been reached"));
 EXPECT(rejects("elevator-standard",0,-10,"Elevators cannot go below B9"));

 // B2: cathedral only on floor 100, only one.
 reset(); g.extendFloor(99,0,28); // exactly the original footprint's support
 EXPECT(g.itemFactory.prototypesById.at("cathedral")->size == int2(28,5));
 EXPECT(rejects("cathedral",0,99,"Cathedral is available only on 100th floor"));
 EXPECT(accepts("cathedral",0,100));
 auto *church=static_cast<Item::Cathedral *>(*g.itemsByType["cathedral"].begin());
 EXPECT(church->size == int2(28,5));
 EXPECT(church->getRect().maxY()==105);
 EXPECT(g.itemsByFloor[104].count(church)==1);
 EXPECT(g.itemsByFloor[105].count(church)==0);
 EXPECT(church->sprite.getTexture().getSize()==sf::Vector2u(448,180));
 EXPECT(church->sprite.getOrigin()==sf::Vector2f(0.f,180.f));
 unsigned cathedralScale=0;
 const sf::Texture *cathedralTwin=g.app.bitmaps.hiresFor(&church->sprite.getTexture(),cathedralScale);
 EXPECT(cathedralTwin && cathedralScale==4 && cathedralTwin->getSize()==sf::Vector2u(1792,720));
 church->underConstruction=false;
 for(int state : {0,2,4,0}) {
  g.sky.from=g.sky.to=state; g.sky.progress=1.0; g.lighting.advance(0);
  church->render(g.app.window);
  EXPECT(church->sprite.getTextureRect()==sf::IntRect({state==2?224:0,0},{224,180}));
 }
 EXPECT(rejects("cathedral",60,100,"Only one Cathedral allowed"));
 EXPECT(rejects("cathedral",0,101,"Maximum height has been reached"));

 // Native fixed-size saves may omit dimensions, or carry the old 48x4.
 // Migration happens before init and indexing, preserving y and clamping x.
 const BuildRules previousRules=g.activeRules;
 // This scenario app has a CLI OpenSky override; explicitly select the
 // bounded policy before decoding so the lot-clamping checks exercise it.
 g.setRules(BuildRules(BuildRules::Ruleset::SimTower));
 for(const std::string dimensions : {std::string(" width=\"48\" height=\"4\""),std::string()}) {
 for(int x : {13,-200,180}) {
  reset();
  tinyxml2::XMLDocument legacy;
  std::string xml="<tower ruleset=\"simtower\" funds=\"20000000\" rating=\"4\" tool=\"inspector\"><item type=\"cathedral\" x=\""+std::to_string(x)+"\" y=\"100\""+dimensions+" weddingDone=\"1\"/></tower>";
  EXPECT(legacy.Parse(xml.c_str())==tinyxml2::XML_SUCCESS); g.decodeXML(legacy);
  church=static_cast<Item::Cathedral *>(*g.itemsByType["cathedral"].begin());
  const int expectedX=x==-200?-187:x==180?160:13;
  EXPECT(church->position==int2(expectedX,100)); EXPECT(church->size==int2(28,5));
  EXPECT(church->weddingDone); EXPECT(g.activeRules.allowsHorizontalFootprint(church->position.x,church->size.x));
  EXPECT(church->sprite.getPosition()==sf::Vector2f(expectedX*8.f,-3600.f));
  EXPECT(g.itemsByFloor[104].count(church)==1);
  EXPECT(g.itemsByFloor[105].count(church)==0);
  tinyxml2::XMLPrinter saved;g.encodeXML(saved);tinyxml2::XMLDocument roundtrip;
  EXPECT(roundtrip.Parse(saved.CStr())==tinyxml2::XML_SUCCESS);g.decodeXML(roundtrip);
  church=static_cast<Item::Cathedral *>(*g.itemsByType["cathedral"].begin());
  EXPECT(church->position==int2(expectedX,100) && church->size==int2(28,5));
 }
 }
 // OpenSky has no fixed lot: an old anchor beyond SimTower's edge stays put.
 g.setRules(BuildRules(BuildRules::Ruleset::OpenSky)); reset();
 tinyxml2::XMLDocument unbounded;
 EXPECT(unbounded.Parse("<tower ruleset=\"opensky\" tool=\"inspector\"><item type=\"cathedral\" x=\"180\" y=\"100\" width=\"48\" height=\"4\"/></tower>")==tinyxml2::XML_SUCCESS);
 g.decodeXML(unbounded);
 church=static_cast<Item::Cathedral *>(*g.itemsByType["cathedral"].begin());
 EXPECT(church->position==int2(180,100) && church->size==int2(28,5));
 g.setRules(BuildRules(BuildRules::Ruleset::SimTower));
 // WP7b: a synthetic pre-parity save (fixed-size items carry no width). The
 // parking space (new width 4) and suite (10) now sit side by side; the
 // medical center (26) at x=10 overlaps the earlier suite, so the later one is
 // dropped and its $500,000 price refunded exactly once.
 reset();
 {
  const long long fundsBefore=1000000;
  tinyxml2::XMLDocument oldSave;
  EXPECT(oldSave.Parse("<tower ruleset=\"simtower\" funds=\"1000000\" rating=\"4\" tool=\"inspector\">"
   "<item type=\"parking\" x=\"0\" y=\"5\"/><item type=\"hotel_suite\" x=\"4\" y=\"5\"/>"
   "<item type=\"medicalcenter\" x=\"10\" y=\"5\"/></tower>")==tinyxml2::XML_SUCCESS);
  g.decodeXML(oldSave);
  EXPECT(g.itemsByType["parking"].size()==1 && g.itemsByType["hotel_suite"].size()==1);
  EXPECT(g.itemsByType["medicalcenter"].empty());
  EXPECT((*g.itemsByType["parking"].begin())->size==int2(4,1));
  EXPECT((*g.itemsByType["hotel_suite"].begin())->size==int2(10,1));
  EXPECT(g.funds==fundsBefore+500000 && g.money.balance==fundsBefore+500000);
  tinyxml2::XMLPrinter resaved;g.encodeXML(resaved);tinyxml2::XMLDocument again;
  EXPECT(again.Parse(resaved.CStr())==tinyxml2::XML_SUCCESS);g.decodeXML(again);
  EXPECT(g.itemsByType["medicalcenter"].empty() && g.funds==fundsBefore+500000);
 }
 // Real TDT cathedral rows decode to a single canonical building at the
 // original file anchor (file 109 -> 99); native placement remains F100.
 reset(); Tdt::TdtData cathedralTdt={}; cathedralTdt.level=5;cathedralTdt.balance=2000000000;
 cathedralTdt.floors.resize(120);
 for(int f=0;f<120;++f) cathedralTdt.floors[f].index=f;
 for(int f=109;f<=113;++f) cathedralTdt.floors[f].tenants.push_back({200,228,36+f-109,0,0,0});
 std::vector<std::string> importReport;const std::string imported=Tdt::toTowerXml(cathedralTdt,importReport);
 tinyxml2::XMLDocument importDoc;EXPECT(importDoc.Parse(imported.c_str())==tinyxml2::XML_SUCCESS);g.decodeXML(importDoc);
 EXPECT(g.itemsByType["cathedral"].size()==1);
 church=static_cast<Item::Cathedral *>(*g.itemsByType["cathedral"].begin());
 EXPECT(church->position==int2(13,99));EXPECT(church->size==int2(28,5));
 EXPECT(g.itemsByFloor[103].count(church)==1);
 g.setRules(previousRules);

 // B3: underground-only / not-underground classes; cinema now allowed above ground.
 reset(); g.extendFloor(1,-40,40); g.extendFloor(-1,-60,60); g.extendFloor(0,-60,60);
 EXPECT(rejects("parking",0,1,"Item unavailable above ground"));
 EXPECT(rejects("recycling",0,1,"Item unavailable above ground"));
 EXPECT(rejects("metro",0,1,"Item unavailable above ground"));
 EXPECT(rejects("parkingramp",0,1,"Item unavailable above ground"));
 EXPECT(rejects("office",0,-2,"Item not available underground"));
 EXPECT(rejects("condo",0,-2,"Item not available underground"));
 EXPECT(rejects("hotel_single",0,-2,"Item not available underground"));
 EXPECT(accepts("cinema",0,2));
 // Recycling after the first must sit within 25 cells (floors -2..+1) of an existing center.
 reset(); g.extendFloor(-1,-100,100);
 EXPECT(accepts("recycling",0,-3));
 EXPECT(rejects("recycling",60,-3,"Must be placed near an existing Recycling Center")); // gap 36 > 25
 EXPECT(accepts("recycling",45,-3)); // gap 21 is inside the envelope

 // B4: metro on the bottom floor; nothing at or below it (no elevator exemption).
 reset(); g.extendFloor(-4,-60,60); g.extendFloor(-1,-60,60);
 make("fastfood",50,-6);
 EXPECT(rejects("metro",0,-4,"Place Metro station on bottom floor"));
 reset(); g.extendFloor(-7,-100,100); g.extendFloor(-8,-100,100);
 EXPECT(accepts("metro",0,-10));
 EXPECT(rejects("fastfood",50,-10,"Cannot place items under Metro"));
 EXPECT(rejects("metro",60,-10,"Only one Metro Station allowed"));
 EXPECT(g.metroStation && !static_cast<Item::Elevator::Elevator *>(make("elevator-standard",100,-9))->canResizeTo(-10,3));
 EXPECT(g.metroStation && static_cast<Item::Elevator::Elevator *>(make("elevator-standard",120,-9))->canResizeTo(-9,3));

 // B5: count caps.
 reset(); g.extendFloor(2,-100,100);
 for (int i=0;i<10;++i) make("cinema",i*40,10+i*3);
 for (int i=0;i<6;++i) make("partyhall",i*30,60+i*3);
 EXPECT(rejects("cinema",0,3,"Item no longer available"));
 reset(); g.extendFloor(1,-100,100);
 for (int i=0;i<10;++i) make("medicalcenter",i*40,30+i);
 for (int i=0;i<10;++i) make("security",i*20,60+i);
 EXPECT(rejects("medicalcenter",0,2,"Item no longer available"));
 EXPECT(rejects("security",0,2,"Item no longer available"));
 reset(); g.extendFloor(1,-100,100);
 for (int i=0;i<300;++i) make("fastfood",0,2);
 for (int i=0;i<212;++i) make("shop",0,2);
 EXPECT(rejects("restaurant",0,2,"Item no longer available"));
 EXPECT(rejects("shop",0,2,"Item no longer available"));
 reset(); g.extendFloor(0,-100,100);
 for (int i=0;i<64;++i) make(i%2?"stairs":"escalator",-300+i*10,2);
 EXPECT(rejects("stairs",0,3,"No more stairs available"));
 reset(); g.extendFloor(0,-100,100);
 for (int i=0;i<512;++i) make("parking",0,-1);
 EXPECT(rejects("parking",0,-1,"Item no longer available"));

 // B6: bays need a ramp on their floor; ramps chain down from B1 at the same x.
 reset(); g.extendFloor(0,-60,60); g.extendFloor(-1,-60,60);
 EXPECT(rejects("parking",20,-1,"Parking Ramps must be placed on this level"));
 EXPECT(rejects("parkingramp",0,-2,"Parking Ramps must connect to the 1st floor"));
 EXPECT(accepts("parkingramp",0,-1));
 EXPECT(rejects("parkingramp",0,-3,"Parking Ramps must connect to the 1st floor"));
 EXPECT(rejects("parkingramp",16,-2,"Parking Ramps must be connected vertically"));
 EXPECT(rejects("parkingramp",16,-1,"This floor already has a Parking Ramp"));
 EXPECT(rejects("parkingramp",0,-1,"This floor already has a Parking Ramp"));
 EXPECT(accepts("parkingramp",0,-2));
 EXPECT(accepts("parking",20,-1));

 // Downward ramp: B2 with only a B3 ramp is not on the chain from B1.
 reset(); g.extendFloor(-2,-60,60); g.extendFloor(-3,-60,60); make("parkingramp",0,-3);
 EXPECT(rejects("parkingramp",0,-2,"Parking Ramps must connect to the 1st floor"));

 // Cap recovery: at the 16-venue cap, bulldozing one frees a slot.
 reset(); g.extendFloor(2,-100,100);
 Item::Item *venue=nullptr;
 for (int i=0;i<16;++i) { auto *v=make("cinema",i*40,10+i*3); if(i==0) venue=v; }
 EXPECT(rejects("cinema",0,3,"Item no longer available"));
 EXPECT(g.bulldozeItem(venue));
 EXPECT(accepts("cinema",0,3));

 // B7: an express segment touching its own column is allowed off a sky-lobby floor.
 reset(); g.extendFloor(13,-20,20);
 EXPECT(accepts("elevator-express",0,14));
 EXPECT(accepts("elevator-express",0,15));
 EXPECT(rejects("elevator-express",40,16,"New express shafts anchor at lobby floors"));

 // B8: indestructible items, items under construction, condo buyout.
 reset();
 const char *fixed[]={"lobby","security","housekeeping","cathedral","metro"};
 for (const char *id : fixed) {
  auto *it=make(id,0,id[0]=='m'?-10:(id[0]=='l'?0:1));
  g.timeWindow.message.clear();
  EXPECT(!g.bulldozeItem(it)); EXPECT(g.items.count(it)==1);
  EXPECT(g.timeWindow.message=="Cannot destroy this item");
 }
 reset(); g.extendFloor(0,-20,20);
 auto *site=make("office",0,1); site->underConstruction=true;
 g.timeWindow.message.clear();
 EXPECT(!g.bulldozeItem(site)); EXPECT(g.timeWindow.message=="Cannot destroy items under construction");
 site->underConstruction=false;
 EXPECT(g.bulldozeItem(site)); EXPECT(g.items.count(site)==0);
 auto *condo=static_cast<Item::Condo *>(make("condo",0,2));
 condo->tenant.rentClass=1; condo->occupied=true;
 const int buyout=condo->demolitionBuyout(); const int before=g.money.balance;
 EXPECT(buyout>0); EXPECT(g.bulldozeItem(condo));
 EXPECT(g.money.balance==before-buyout);
 auto *vacant=static_cast<Item::Condo *>(make("condo",20,2));
 const int before2=g.money.balance; EXPECT(g.bulldozeItem(vacant)); EXPECT(g.money.balance==before2);

 // B9: the first ground-lobby build click locks the grand-lobby height.
 reset(); EXPECT(g.lobbyHeightLock==0);
 g.money.clear(0); g.funds=0;
 EXPECT(!place("lobby",0,0)); // unaffordable, but the choice is still made
 EXPECT(g.lobbyHeightLock==g.activeRules.profile().lobbyHeight);
 reset(); g.lobbyHeightLock=3;
 EXPECT(g.lobbyToolHeight()==3);
 EXPECT(accepts("lobby",0,0));
 bool tall=false; for (auto *it : g.itemsByType["lobby"]) tall=it->size.y==3;
 EXPECT(tall);
 { tinyxml2::XMLPrinter pr; g.encodeXML(pr); EXPECT(std::string(pr.CStr()).find("lobbyHeightLock=\"3\"")!=std::string::npos); }
 g.clearWorld(); EXPECT(g.lobbyHeightLock==0);
 // Real-fixture TDT load (only where the Concilia fixture exists; never copied into the repo).
 {
  struct Snapshot { const char *name; int64_t funds; int population, rating, day, frame; };
  const Snapshot snapshots[] = {
   {"SCHMITT", 93560900LL, 15401, 4, 148, 2174},
   {"BARKLE4D", 278104600LL, 5807, 3, 2675, 1439},
   {"THEECSTA", 1419732100LL, 23176, 5, 66, 1256}
  };
  for (const Snapshot &snapshot : snapshots) {
  const std::string fixture=std::string("/tmp/concilia/repo/tests/fixtures/")+snapshot.name+".TDT";
  std::ifstream probe(fixture.c_str(), std::ios::binary);
  if (!probe.good()) {
   std::cout << "SKIP: real-fixture TDT load (" << fixture << " not present)\n";
  } else {
   Tdt::TdtData tdt; std::string err;
   EXPECT(Tdt::loadFile(fixture, tdt, err));
   int tdtFloors=0, tdtTenants=0;
   for (const auto &fr : tdt.floors) for (const auto &tn : fr.tenants) { if (tn.type==0) ++tdtFloors; else ++tdtTenants; }
   std::vector<std::string> report; const std::string xml=Tdt::toTowerXml(tdt, report);
   auto occurrences=[&](const std::string &needle) { int n=0; for (size_t at=xml.find(needle); at!=std::string::npos; at=xml.find(needle,at+1)) ++n; return n; };
   EXPECT(tdtFloors>0 && tdtTenants>0);
   EXPECT(occurrences("type=\"floor\"")==tdtFloors);
   EXPECT(occurrences("<item type=\"")>=tdtFloors+1);
   const int ramps=occurrences("type=\"parkingramp\""), bays=occurrences("type=\"parking\"");
   tinyxml2::XMLDocument doc; EXPECT(doc.Parse(xml.c_str(), xml.size())==tinyxml2::XML_SUCCESS);
   reset(); g.decodeXML(doc);
   EXPECT(g.funds==snapshot.funds && g.money.balance==snapshot.funds);
   EXPECT(g.population==snapshot.population && g.countT.population==snapshot.population);
   for (int c=0;c<10;++c) EXPECT(g.countT.cat[c]==tdt.countT[c]);
   EXPECT(g.rating==snapshot.rating);
   EXPECT(g.time.day==snapshot.day%3);
   EXPECT(g.time.quarter==(snapshot.day/3)%4+1);
   EXPECT(g.time.year==snapshot.day/12+1);
   EXPECT(g.time.eventFrame()==snapshot.frame);
   EXPECT(g.time.dta==0);
   EXPECT((int)g.itemsByType["parkingramp"].size()==ramps);
   Item::Parking::recomputeAllChains(&g);
   bool rampOnB1=false; for (auto *r : g.itemsByType["parkingramp"]) if (r->position.y==-1) rampOnB1=true;
   int usable=0; for (auto *p : g.itemsByType["parking"]) usable+=static_cast<Item::Parking *>(p)->usableSpaces();
   std::cout << snapshot.name << ".TDT: funds=" << g.funds << " pop=" << g.population << " stars=" << g.rating+1 << " day=" << snapshot.day << " frame=" << g.time.eventFrame() << " floors=" << tdtFloors << " tenants=" << tdtTenants << " ramps=" << ramps << " bays=" << bays << " usable=" << usable << "\n";
   if (ramps>0 && bays>0 && rampOnB1) EXPECT(usable>0);
   if (std::string(snapshot.name)=="SCHMITT") {
    // Same layout with the supplied HUD snapshot's accounting values.
    // Do not rewrite the real fixture or special-case its name in the loader.
    tdt.balance=946059; tdt.currentDay=150; tdt.frameTime=0;
    tdt.countT[1]=65; tdt.population=15402;
    const std::string hudXml=Tdt::toTowerXml(tdt,report);
    tinyxml2::XMLDocument hudDoc; EXPECT(hudDoc.Parse(hudXml.c_str())==tinyxml2::XML_SUCCESS);
    reset(); g.decodeXML(hudDoc);
    EXPECT(g.funds==94605900 && g.money.balance==94605900);
    EXPECT(g.population==15402 && g.countT.population==15402);
    EXPECT(g.rating==4);
    EXPECT(g.time.day==0 && g.time.quarter==3 && g.time.year==13);
    EXPECT(g.time.eventFrame()==0);
   }
  }
  }
 }
 std::cout << "Batch 6 C++ placement scenarios completed\n";
}
#endif

// Steps 10-11: T2EV scheduler (grade bits, cycle days, begin minutes),
// event bodies and the T2BlackOut power model.
static void testYootEventsAndPower()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();
	const YootWorld::Id ids[3] = {YootWorld::Id::Tokyo, YootWorld::Id::Hawaii, YootWorld::Id::Kegon};

	for (int w = 0; w < 3; ++w)
	{
		const YootWorld world = loadYootWorld(pm, ids[w]);
		const std::vector<YootEventSpec> plugin = yootEventSpecs(world);
		const std::vector<YootEventSpec> builtin = builtinYootEventSpecs(w);
		// The built-in table matches the plugin headers, in T2EV order.
		EXPECT(plugin.size() == builtin.size());
		for (size_t i = 0; i < plugin.size() && i < builtin.size(); ++i)
		{
			EXPECT(plugin[i].type == builtin[i].type && plugin[i].resId == builtin[i].resId);
			EXPECT(plugin[i].levelBit == builtin[i].levelBit && plugin[i].cycleDay == builtin[i].cycleDay);
			EXPECT(plugin[i].beginMin == builtin[i].beginMin && plugin[i].endMin == builtin[i].endMin);
			EXPECT(plugin[i].exclusive == builtin[i].exclusive && plugin[i].text == builtin[i].text);
		}
		// 120-day schedule at every grade == levelBit & (totalDays - 11) % cycle.
		for (size_t i = 0; i < plugin.size(); ++i)
			for (int grade = 1; grade <= 5; ++grade)
			{
				std::vector<int> expect;
				if (plugin[i].levelBit & (1 << (grade - 1)))
					for (int d = 0; d < 120; ++d)
						if ((d - 11) % plugin[i].cycleDay == 0) expect.push_back(d);
				EXPECT(yootEventDays(plugin[i], grade, 0, 119) == expect);
			}
		// The profile carries the same list (VIEV stays with VipSystem).
		const RulesProfile profile = RulesProfile::yootTower(world);
		EXPECT(profile.events.size() == plugin.size() && profile.worldWidth == world.width);
	}

	// Concrete cadences (game-parity V2-V5).
	const std::vector<YootEventSpec> tokyo = builtinYootEventSpecs(0);
	const std::vector<YootEventSpec> hawaii = builtinYootEventSpecs(1);
	const std::vector<YootEventSpec> kegon = builtinYootEventSpecs(2);
	EXPECT(yootEventDays(tokyo[0], 3, 0, 119) == std::vector<int>({11, 71}));  // fire, 60 days
	EXPECT(yootEventDays(tokyo[0], 2, 0, 119).empty());                        // fire only at 3-4
	EXPECT(yootEventDays(tokyo[1], 2, 0, 119) == std::vector<int>({11, 47, 83, 119})); // terrorist, 36
	EXPECT(yootEventDays(hawaii[1], 2, 0, 119).empty());                       // Hawaii: grade 3 only
	EXPECT(yootEventDays(tokyo[4], 1, 0, 119).size() == 10u);                  // Santa: day 12 of each year
	EXPECT(yootEventDays(tokyo[4], 1, 0, 119)[0] == 11);
	EXPECT(yootEventDays(tokyo[9], 2, 0, 119).empty());                        // Reggae: grade 3+
	EXPECT(yootEventDays(tokyo[9], 3, 0, 119).size() == 5u);                   // 25 days: 11,36,61,86,111
	bool kegonCalm = true;
	for (size_t i = 0; i < kegon.size(); ++i)
		kegonCalm = kegonCalm && kegon[i].type != "FBEV" && kegon[i].type != "TREV" &&
		            kegon[i].type != "BOEV" && kegon[i].type != "SAEV";
	EXPECT(kegonCalm);
	EXPECT(yootEventBeginMinute(tokyo[4], 368) == 1440 - 184);  // Santa ctor override
	EXPECT(yootEventBeginMinute(tokyo[0], 368) == 720);
	EXPECT(yootEventBeginDay(12, -1) && !yootEventBeginDay(12, 0) && yootEventBeginDay(1, 5));

	// Scheduler over 120 days at grade 3, 10-minute steps: begins land on
	// the begin minute of each begin day; a running event does not restart.
	YootEventScheduler sched;
	sched.setEvents(tokyo, 368);
	std::vector<int> fireDays, blackoutDays, santaMinutes;
	for (int d = 0; d < 120; ++d)
		for (int m = 0; m < 1440; m += 10)
		{
			const std::vector<int> b = sched.advance(d, m, 3);
			for (size_t k = 0; k < b.size(); ++k)
			{
				const std::string & t = tokyo[b[k]].type;
				if (t == "FBEV") { fireDays.push_back(d); EXPECT(m == 720); }
				if (t == "BOEV") blackoutDays.push_back(d);
				if (t == "SAEV") santaMinutes.push_back(m);
			}
		}
	EXPECT(fireDays == std::vector<int>({11, 71}));
	EXPECT(blackoutDays.size() == 120u);
	EXPECT(santaMinutes.size() == 10u && santaMinutes[0] == 1260); // first step >= 1256
	sched.reset();
	sched.advance(0, 650, 3);
	sched.setRunning(sched.find("BOEV"), true);
	EXPECT(sched.advance(0, 670, 3).empty());

	// Event bodies.
	YootFireDef fire;
	EXPECT(decodeYootFire(tokyo[0], fire) && fire.heliPrice == 3000 && fire.bitmaps.size() == 4u);
	YootTerroristDef terror;
	EXPECT(decodeYootTerrorist(tokyo[1], terror) && terror.ransom == 3000 && terror.deadlineMin == 720);
	YootMetroDef metro;
	EXPECT(decodeYootMetro(tokyo[2], metro) && metro.trains.size() == 12u &&
	       metro.trains[0] == std::make_pair(410, 1) && metro.trains[11] == std::make_pair(1420, 1));
	YootOutDecorationDef deco;
	EXPECT(decodeYootOutDecoration(tokyo[6], deco) && deco.items.size() == 8u && deco.items[0].first == 9102);
	YootCloudDef clouds;
	EXPECT(decodeYootCloud(tokyo[5], clouds) && clouds.bitmaps.size() == 4u && clouds.zones.size() == 4u);
	YootReggaeDef maru, reggae;
	EXPECT(decodeYootReggae(tokyo[8], maru) && !maru.isReggae());
	EXPECT(decodeYootReggae(tokyo[9], reggae) && reggae.isReggae() && reggae.tenantCategory == 50);
	const YootWorld tokyoWorld = loadYootWorld(pm, YootWorld::Id::Tokyo);
	const YootEvent * maev = 0;
	for (size_t i = 0; i < tokyoWorld.events.size(); ++i)
		if (tokyoWorld.events[i].resType == "MAEV" && tokyoWorld.events[i].resId == 129) maev = &tokyoWorld.events[i];
	EXPECT(maev && maev->text == "'Uncle Reggae is noisy.'" && maev->body.size() == 4u);
	YootHotSpringDef spring;
	EXPECT(decodeYootHotSpring(kegon[1], spring) && spring.a == 100 && spring.tenantA == 0x546e5049u);

	// Power: supply = 5000 + 5000 x transformers (max 5), warn under 50 spare.
	YootPowerModel power;
	YootBlackoutDef bo;
	EXPECT(decodeYootBlackout(tokyo[3], bo) && bo.defSupply == 5000 && bo.addSupply == 5000 &&
	       bo.transformerPrice == 5000);
	power.def = bo;
	power.reset();
	EXPECT(power.supply() == 5000);
	power.setTransformers(1);
	EXPECT(power.supply() == 10000);
	power.setTransformers(5);
	EXPECT(power.supply() == 30000);
	power.setTransformers(9);
	EXPECT(power.supply() == 30000);
	power.setTransformers(1);
	power.setConsumption(9950);
	EXPECT(!power.shortage());
	power.setConsumption(9951);
	EXPECT(power.shortage());
	power.setConsumption(10000);
	EXPECT(power.start() == YootPowerModel::None && !power.isBlackOut()); // needs supply < demand
	// Blackout window: 11:00 start, escape at 14:00 (+1 minute), lasts until fixed.
	power.setConsumption(10001);
	EXPECT(power.start() == YootPowerModel::Started && power.status() == YootPowerModel::Dark);
	bool quiet = true;
	for (int m = 661; m < 840; ++m) quiet = quiet && power.minute(m) == YootPowerModel::None;
	EXPECT(quiet && power.status() == YootPowerModel::Dark);
	EXPECT(power.minute(840) == YootPowerModel::None && power.status() == YootPowerModel::EscapeDue);
	EXPECT(power.minute(841) == YootPowerModel::Escape && power.status() == YootPowerModel::Escaped);
	EXPECT(power.minute(900) == YootPowerModel::None && power.isBlackOut());
	power.setTransformers(2);
	EXPECT(power.minute(901) == YootPowerModel::None && power.status() == YootPowerModel::Ending);
	EXPECT(power.minute(902) == YootPowerModel::Ended && !power.isBlackOut());
	// Fixed before 14:00: no escape.
	power.setTransformers(1);
	EXPECT(power.start() == YootPowerModel::Started);
	power.setConsumption(9000);
	EXPECT(power.minute(700) == YootPowerModel::None && power.minute(701) == YootPowerModel::Ended);
	// Breakers back on end it too.
	power.setConsumption(20000);
	EXPECT(power.start() == YootPowerModel::Started);
	power.breakerOn();
	EXPECT(power.minute(800) == YootPowerModel::Ended);

	// A headless tick can skip 14:00; escape still runs once and the
	// blackout persists through midnight until breakers are restored.
	EXPECT(power.start() == YootPowerModel::Started);
	EXPECT(power.minute(839) == YootPowerModel::None);
	EXPECT(power.minute(841) == YootPowerModel::None);
	EXPECT(power.minute(842) == YootPowerModel::Escape);
	EXPECT(power.minute(0) == YootPowerModel::None && power.isBlackOut());
	power.breakerOn();
	EXPECT(power.minute(1) == YootPowerModel::Ended && !power.isBlackOut());

	// Per-item demand comes from the plugin ToolDef (+0xf0).
	TemplateDef def;
	AbstractPlugin * burger = pm.get("Burger.t2p");
	EXPECT(burger && burger->ensureLoaded() && burger->getTemplateDef(def) &&
	       !def.tool.variations.empty() && def.tool.variations[0].consumptionPower == 8);
	AbstractPlugin * room = pm.get("PowerRoom.t2p");
	EXPECT(room && room->ensureLoaded() && room->getTemplateDef(def) &&
	       def.tool.variations[0].consumptionPower == 0 && def.tool.variations[0].price == 5000);

	// Power Room: YootTower-only, grade 3, absent from Kegon.
	EXPECT(BuildRules(BuildRules::Ruleset::SimTower).minRatingToBuild("powerroom") == BuildRules::kUnavailable);
	EXPECT(BuildRules(BuildRules::Ruleset::OpenSky).minRatingToBuild("powerroom") == BuildRules::kUnavailable);
	EXPECT(BuildRules(BuildRules::Ruleset::YootTower).minRatingToBuild("powerroom") == 2);
	const std::map<std::string, YootUnlock> tu = resolveYootUnlocks(pm, 0x7);
	const std::map<std::string, YootUnlock> ku = resolveYootUnlocks(pm, 0xF0);
	EXPECT(tu.count("powerroom") && tu.at("powerroom").available && tu.at("powerroom").minGrade == 3);
	EXPECT(ku.count("powerroom") && !ku.at("powerroom").available);
	// SimTower/OpenSky profiles carry no T2EV events.
	EXPECT(RulesProfile::simTower().events.empty() && RulesProfile::openSky().events.empty());
	EXPECT(RulesProfile::yootTowerStub().events.size() == tokyo.size());
}

// Step 4: data-driven item catalog from TNDF/MVDF.
static void testYootCatalog()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();

	// The parsed StdElev: $150,000 + $50,000/car, 15 floors, 6 cars of 17.
	YootItemStats elev;
	AbstractPlugin * stdElev = pm.get("StdElev.t2p");
	EXPECT(stdElev && loadYootItemStats(*stdElev, 0, elev));
	EXPECT(elev.mover && elev.plugin == "StdElev.t2p" && elev.name == "Simple Elevator");
	EXPECT(elev.price == 150000);
	EXPECT(elev.carPrice == 50000);
	EXPECT(elev.maxLength == 15);
	EXPECT(elev.maxCars == 6);
	EXPECT(elev.capacity == 17);
	EXPECT(elev.outMoney == 12000 && elev.routeScore == 15 && elev.power == 20);
	EXPECT(elev.paysQuarterly());

	const std::map<std::string, YootItemStats> tokyo = resolveYootCatalog(pm, 0x7);
	const std::map<std::string, YootItemStats> kegon = resolveYootCatalog(pm, 0xF0);
	EXPECT(tokyo.count("elevator-standard") && tokyo.at("elevator-standard").plugin == "StdElev.t2p");
	// Large Elevator: any height, at most 8 stops, 8 cars of 40.
	EXPECT(tokyo.count("elevator-express"));
	if (tokyo.count("elevator-express"))
	{
		const YootItemStats & big = tokyo.at("elevator-express");
		EXPECT(big.price == 500000 && big.carPrice == 150000);
		EXPECT(big.maxLength == 0 && big.maxStops == 8 && big.maxCars == 8 && big.capacity == 40);
		EXPECT(big.routeScore == 5);
	}
	if (tokyo.count("office"))
	{
		const YootItemStats & office = tokyo.at("office");
		EXPECT(office.price == 100000 && office.outMoney == 0);
		EXPECT(office.inMoney == std::vector<int>({1000, 2000, 3000, 4000}));
		EXPECT(office.senseOfNoise == 100 && office.capacity == 6);
	}
	else EXPECT(false);
	// Lobby is variation 1 of Floor.t2p, maintained per unit.
	if (tokyo.count("lobby"))
	{
		const YootItemStats & lobby = tokyo.at("lobby");
		EXPECT(lobby.price == 5000 && lobby.outMoney == 1000 && lobby.maintenancePerUnit());
	}
	else EXPECT(false);
	if (tokyo.count("floor")) EXPECT(tokyo.at("floor").price == 500);
	if (tokyo.count("stairs")) EXPECT(tokyo.at("stairs").price == 5000 && tokyo.at("stairs").routeScore == 50);
	if (tokyo.count("fastfood"))
	{
		const YootItemStats & burger = tokyo.at("fastfood");
		EXPECT(burger.price == 200000 && burger.outMoney == 800 && !burger.paysQuarterly());
		EXPECT(burger.openMinute == 8 * 60 && burger.closeMinute == 22 * 60);
	}
	else EXPECT(false);
	if (tokyo.count("theater")) EXPECT(tokyo.at("theater").maxBuild == 1);
	// Kegon has no Stair.t2p; its standard elevator is the K-variant.
	EXPECT(kegon.count("stairs") == 0u);
	EXPECT(kegon.count("elevator-standard") && kegon.at("elevator-standard").plugin == "KElev.t2p");
	// Every catalogued plugin decodes; no OuterObj sneaks in.
	for (std::map<std::string, YootItemStats>::const_iterator it = tokyo.begin(); it != tokyo.end(); ++it)
		EXPECT(it->second.price > 0);
}

// Step 7: economy timing and values, with a ledger fixture over one quarter.
static void testYootEconomy()
{
	using namespace OT;
	namespace Y = OT::Yoot;
	const std::map<std::string, YootItemStats> cat = resolveYootCatalog(yootTestPlugins(), 0x7);
	EXPECT(cat.count("office") && cat.count("lobby") && cat.count("elevator-standard") &&
	       cat.count("fastfood") && cat.count("hotel_single") && cat.count("escalator"));
	if (!(cat.count("office") && cat.count("lobby") && cat.count("elevator-standard") &&
	      cat.count("fastfood") && cat.count("hotel_single") && cat.count("escalator")))
		return;
	const YootItemStats & office = cat.at("office");
	const YootItemStats & lobby = cat.at("lobby");
	const YootItemStats & elev = cat.at("elevator-standard");
	const YootItemStats & burger = cat.at("fastfood");
	const YootItemStats & single = cat.at("hotel_single");
	const YootItemStats & esc = cat.at("escalator");

	// E3 maintenance per kind.
	EXPECT(Y::maintenanceCost(lobby, 20, 0) == 20000);    // $1,000 per unit
	EXPECT(Y::maintenanceCost(elev, 4, 3) == 36000);      // $12,000 per car
	EXPECT(Y::maintenanceCost(esc, 8, 2) == 5000);        // escalator: attr 0x2000 halves 2 modules
	EXPECT(Y::maintenanceCost(esc, 8, 1) == 0);
	EXPECT(Y::maintenanceCost(office, 9, 0) == 0);        // outMoney 0
	EXPECT(Y::maintenanceCost(burger, 16, 0) == 0);       // no attr 0x100: paid daily
	// Power Room (attr 0x401900): $50,000 per quarter through the same path.
	EXPECT(cat.count("powerroom") && Y::maintenanceCost(cat.at("powerroom"), 23, 0) == 50000);
	EXPECT(Y::opensQuarter(0) && !Y::opensQuarter(1) && !Y::opensQuarter(2) && Y::opensQuarter(3));

	// E4 rent window: 04:52..04:59 on day 1 only.
	EXPECT(!Y::officeRentWindow(0, 291) && Y::officeRentWindow(0, 292));
	EXPECT(Y::officeRentWindow(3, 299) && !Y::officeRentWindow(3, 300));
	EXPECT(!Y::officeRentWindow(1, 295) && !Y::officeRentWindow(2, 295));
	EXPECT(Y::officeQuarterRent(office, 0) == 3000 && Y::officeQuarterRent(office, 3) == 12000);

	// E5 price index / rent class mapping.
	EXPECT(Y::priceIndex(0, 4) == 1 && Y::priceIndex(3, 4) == 4 && Y::priceIndex(0, 1) == 0);
	EXPECT(Y::selectionFromRentClass(0, 4) == 3 && Y::selectionFromRentClass(3, 4) == 0);
	EXPECT(Y::selectionFromRentClass(1, 1) == 0);

	// E6 commercial: customers x inMoney / 100 - outMoney (units), estimate 70 x gross / outMoney.
	EXPECT(Y::calcInMoneyUnits(100, 10, 8) == 2);
	EXPECT(Y::commercialIncome(100, burger) == 200);
	EXPECT(Y::commercialIncome(50, burger) == -300);
	EXPECT(Y::profitEstimate(80, 10, 8) == 70);           // break-even
	EXPECT(Y::profitEstimate(160, 10, 8) == 140);
	EXPECT(Y::profitEstimate(0, 10, 8) == 1);
	EXPECT(Y::profitEstimate(10000, 10, 8) == 300);
	EXPECT(Y::profitEstimate(5, 10, 0) == 1);             // no maintenance: 0 x 70 -> clamp

	// E7 hotel per-room income.
	EXPECT(Y::hotelRoomIncome(single, 0) == 800 && Y::hotelRoomIncome(single, 3) == 3200);

	// Ledger over the quarter that opens on day 3 (days 3..5), from 23:00 on day 2.
	std::vector<Y::LedgerItem> tower(6);
	tower[0].kind = Y::LedgerItem::Maintained; tower[0].stats = &lobby; tower[0].widthUnits = 30;
	tower[1].kind = Y::LedgerItem::Maintained; tower[1].stats = &elev; tower[1].cars = 2;
	tower[2].kind = Y::LedgerItem::Office; tower[2].stats = &office; tower[2].selection = 1;
	tower[3].kind = Y::LedgerItem::Office; tower[3].stats = &office; tower[3].selection = 2;
	tower[4].kind = Y::LedgerItem::Commercial; tower[4].stats = &burger; tower[4].customersPerDay = 120;
	tower[5].kind = Y::LedgerItem::HotelRoom; tower[5].stats = &single; tower[5].selection = 1;
	const long long from = 2 * Y::kMinutesPerDay + 23 * 60;
	const long long to = 6 * Y::kMinutesPerDay - 1;
	const std::vector<Y::LedgerEntry> ledger = Y::simulateLedger(tower, from, to);
	int maintenance = 0, rent = 0, retail = 0, hotel = 0;
	long long maintenanceAt = -1, firstRentAt = -1;
	int maintenanceCount = 0, rentCount = 0;
	for (size_t i = 0; i < ledger.size(); ++i)
	{
		const Y::LedgerEntry & e = ledger[i];
		if (e.category == "maintenance") { maintenance += e.amount; maintenanceAt = e.minute; ++maintenanceCount; }
		else if (e.category == "rent_income") { rent += e.amount; ++rentCount; if (firstRentAt < 0) firstRentAt = e.minute; }
		else if (e.category == "retail_income") retail += e.amount;
		else if (e.category == "hotel_income") hotel += e.amount;
	}
	EXPECT(maintenanceCount == 1 && maintenanceAt == 3 * Y::kMinutesPerDay);  // midnight, day 1
	EXPECT(maintenance == -(30 * 1000 + 2 * 12000));
	EXPECT(rentCount == 2 && firstRentAt == 3 * Y::kMinutesPerDay + 292);     // 04:52, day 1
	EXPECT(rent == 3 * 2000 + 3 * 3000);
	EXPECT(retail == 3 * 400);                                                 // (120x10/100 - 8) x $100, 3 days
	EXPECT(hotel == 3 * 1200);
	// The opening maintenance precedes the rent of the same quarter.
	EXPECT(maintenanceAt < firstRentAt);

	// Oracle (T2.exe under Wine, Sample Tokyo Map.bld, Y1 Mar -> Apr): at
	// 00:10 the settlement showed lobby 46 units x $1,000, 7 restrooms x
	// $1,000 and the Simple Elevator 2 used cars x $12,000 = $77,000; by
	// 05:24 the 9 offices (inMoneyIndex 2) had paid 9 x 3 x $3,000.
	YootItemStats restroom;
	OT::AbstractPlugin * restroomPlugin = yootTestPlugins().get("Restroom.t2p");
	EXPECT(restroomPlugin && loadYootItemStats(*restroomPlugin, 0, restroom));
	std::vector<Y::LedgerItem> sample(2 + 7 + 9);
	sample[0].stats = &lobby; sample[0].widthUnits = 46;
	sample[1].stats = &elev; sample[1].cars = 2;
	for (int i = 2; i < 9; ++i) sample[i].stats = &restroom;
	for (int i = 9; i < 18; ++i)
	{
		sample[i].kind = Y::LedgerItem::Office;
		sample[i].stats = &office;
		sample[i].selection = 2;
	}
	const std::vector<Y::LedgerEntry> q2 = Y::simulateLedger(sample,
		2 * Y::kMinutesPerDay + 20 * 60 + 48, 3 * Y::kMinutesPerDay + 5 * 60 + 24);
	int q2Maintenance = 0, q2Rent = 0;
	for (size_t i = 0; i < q2.size(); ++i)
	{
		if (q2[i].category == "maintenance") q2Maintenance += q2[i].amount;
		if (q2[i].category == "rent_income") q2Rent += q2[i].amount;
	}
	EXPECT(q2Maintenance == -77000);
	EXPECT(q2Rent == 81000);
}

// Step 8: 1..300 tenant estimate and the 70/200 decisions.
static void testYootEvaluation()
{
	namespace Y = OT::Yoot;
	Y::ResidentialInputs in;
	in.peopleEstimates = {250, 230, 210};       // mean 230
	in.senseOfNoise = 100;
	in.trafficRange = 0;
	in.priceIndex = 2;                          // +30
	in.yearsSinceBuilt = 0;
	int people = 0;
	EXPECT(Y::residentialEstimate(in, &people) == 260 && people == 230);
	in.trafficRange = 1;                        // -100 (office/apartment noise sense)
	EXPECT(Y::residentialEstimate(in) == 160);
	in.priceIndex = 4;                          // most expensive: -30
	EXPECT(Y::residentialEstimate(in) == 100);
	in.yearsSinceBuilt = 5;
	EXPECT(Y::residentialEstimate(in) == 95);
	in.trafficRange = 3; in.senseOfNoise = 150; // condo next to heavy traffic
	EXPECT(Y::residentialEstimate(in) == 1);
	in.priceIndex = 0; in.trafficRange = 0; in.yearsSinceBuilt = 0;
	in.peopleEstimates = {300, 300};
	EXPECT(Y::residentialEstimate(in) == 300);
	in.hasBelongs = false;
	EXPECT(Y::residentialEstimate(in) == 1);
	// No residents: the previous mean is kept.
	Y::ResidentialInputs empty;
	empty.previousPeopleEstimate = 180;
	empty.priceIndex = 1;                       // +60
	EXPECT(Y::residentialEstimate(empty) == 240);

	int counter = 0;
	EXPECT(Y::decideResidential(69, counter) == Y::Decision::MoveOut);
	EXPECT(Y::decideResidential(201, counter) == Y::Decision::Grow);
	EXPECT(counter == 0);
	for (int i = 0; i < 4; ++i)
		EXPECT(Y::decideResidential(150, counter) == Y::Decision::Stay);
	EXPECT(Y::decideResidential(70, counter) == Y::Decision::Grow);   // every 5th call
	EXPECT(counter == 0);
	EXPECT(Y::decideResidential(200, counter) == Y::Decision::Stay);

	EXPECT(Y::estimateColor(69) == 14 && Y::estimateColor(70) == 9 &&
	       Y::estimateColor(200) == 9 && Y::estimateColor(201) == 12 && Y::estimateColor(0) == -1);

	// People estimate: running average over up to 4 trips.
	EXPECT(Y::calcCurrEstimate(300, 0, 50) == 250);
	EXPECT(Y::calcCurrEstimate(250, 1, 100) == 225);       // (250 - 100 + 300) / 2
	EXPECT(Y::calcCurrEstimate(250, 1, 100, true) == 150); // residents: (250 - 100) / 1
	EXPECT(Y::calcCurrEstimate(10, 0, 300) == 1);
	int est = Y::kPeopleEstimateStart, trips = 0, stress = 60;
	Y::updateEstimation(est, trips, stress);
	EXPECT(est == 240 && trips == 1 && stress == 0);
	for (int i = 0; i < 6; ++i) Y::updateEstimation(est, trips, stress);
	EXPECT(trips == Y::kMaxTrips);
	EXPECT(Y::incEstimate(40, -50) == 1 && Y::incEstimate(280, 50) == 300);
}

// Step 9: wait stress, no-route stress and the mover route score.
static void testYootStress()
{
	namespace Y = OT::Yoot;
	int stress = 0;
	int ticks = 0;
	bool gaveUp = false;
	while (!gaveUp && ticks < 1000)
	{
		gaveUp = Y::waitTick(stress);
		++ticks;
	}
	EXPECT(ticks == 60 && stress == 300);
	// 60 people-idle ticks = 120 game minutes of waiting.
	EXPECT(ticks * Y::kPeopleIdleMinutes == 120);

	stress = 0;
	int tries = 0;
	while (!Y::noRouteTry(stress)) ++tries;
	EXPECT(tries + 1 == 10 && stress == 300);
	EXPECT(Y::incStress(290, 30) == 300 && Y::incStress(10, -30) == 0);

	// Route cost = mover score x floors (Stairs 50, Escalator 30, Simple 15, Large 5).
	EXPECT(Y::moverRouteScore(15, 10) == 150);
	// Floor walk (IsReachDestination): 16 units a people idle, the rest on
	// arrival; StopOnHisWay's window is open on both ends.
	{
		int h = 0;
		EXPECT(Y::walkStep(h, 40) == 16 && h == 16);
		EXPECT(Y::walkStep(h, 40) == 16 && h == 32);
		EXPECT(Y::walkStep(h, 40) == 8 && h == 40);
		EXPECT(Y::walkStep(h, 40) == 0 && h == 40);
		EXPECT(Y::walkStep(h, 30) == 10 && h == 30);
		EXPECT(Y::passesTenant(10, 20, 30, 15) && !Y::passesTenant(5, 20, 30, 15) && !Y::passesTenant(45, 20, 30, 15));
		EXPECT(Y::passesTenant(44, 20, 30, 15) && Y::passesTenant(25, 20, 30, 0));
	}
	EXPECT(Y::moverRouteScore(5, -10) == 50);
	EXPECT(Y::moverRouteScore(50, 1) == 50);
}

// ---- YootTower .bld import -------------------------------------------------
// The shipped sample saves are game data and are not vendored: they are read
// from $OPENSKY_YOOT_DIR (default ~/Downloads/Yoot-Tower_Win_EN/YOOT) and the
// sample checks are skipped when absent. The synthetic fixture always runs.

static std::string yootSamplePath(const char * name)
{
	std::string dir;
	if (const char * env = std::getenv("OPENSKY_YOOT_DIR")) dir = env;
	else if (const char * home = std::getenv("HOME")) dir = std::string(home) + "/Downloads/Yoot-Tower_Win_EN/YOOT";
	return dir + "/" + name;
}

static bool readAllBytes(const std::string & path, std::vector<uint8_t> & out)
{
	std::ifstream in(path.c_str(), std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

/// Footprints of the OpenSky prototypes (Item/*.h makePrototype).
static bool bldTestPrototypes(const std::string & id, int & w, int & h)
{
	static const std::map<std::string, std::pair<int, int> > sizes = {
		{"lobby", {4, 1}}, {"floor", {1, 1}}, {"stairs", {8, 2}}, {"escalator", {8, 2}},
		{"elevator-express", {6, 1}}, {"elevator-service", {4, 1}}, {"elevator-standard", {4, 1}},
		{"fastfood", {16, 1}}, {"restaurant", {24, 1}}, {"shop", {12, 1}}, {"office", {9, 1}},
		{"condo", {16, 1}}, {"yoot_condo", {16, 1}}, {"hotel_single", {4, 1}},
		{"restroom", {9, 1}}, {"yt_icecream", {12, 1}}, {"apartment", {4, 1}},
		{"skybridge", {8, 1}}, {"billboard", {20, 2}}, {"eventcenter", {31, 2}},
	};
	std::map<std::string, std::pair<int, int> >::const_iterator i = sizes.find(id);
	if (i == sizes.end()) return false;
	w = i->second.first;
	h = i->second.second;
	return true;
}

static int countOf(const std::string & hay, const std::string & needle)
{
	int n = 0;
	for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) n++;
	return n;
}

static int pluginCount(const OT::Bld::BldData & d, const char * macId)
{
	int n = 0;
	for (const OT::Bld::Tenant & t : d.tenants)
	{
		const OT::Bld::PluginInfo * p = t.hdr.used ? d.pluginForItemType(t.equipType) : nullptr;
		if (p && OT::Bld::macOrder(p->id) == macId) n++;
	}
	return n;
}

/// A tiny structurally complete save built through the writer: proves the
/// writer and reader agree and that the parse is strict.
static void testBldSyntheticRoundTrip()
{
	using namespace OT::Bld;
	BldData d;
	d.world = "KJNS";
	d.level = 3;
	d.speed = -2;
	d.year = 2; d.month0 = 5; d.minutes = 600;
	PluginInfo office;
	office.type = "lPnT"; office.id = "CFFO"; office.itemType = 1200; office.name = "Office";
	office.fileName = "C:\\YOOT\\PLUGINS\\OFFICE.T2P";
	d.plugins.push_back(office);
	d.people.resize(256);
	d.vRange = 2; d.hRange = 3;
	d.unitMap.assign(2 * 3 * 4, 0);
	d.tenants.resize(128);
	Tenant & t = d.tenants[1];
	t.hdr.used = 1; t.hdr.status = 13;
	t.equipID = 1001; t.equipType = 1200; t.area = {150, 108, 159, 109};
	t.ce.tag = "AECT"; t.ce.ids.items = {1000}; t.ce.ids.h2 = t.ce.ids.h3 = t.ce.ids.dataBytes = 2;
	t.estimate = 150; t.inMoneyIndex = 2; t.f8c = 300;
	t.hasBelongList = 1; t.belongList = {1, 2, 3};
	d.tenants[5].hdr.status = 1; // unused slot that still carries a status
	d.movers.resize(32);
	d.requests.resize(64);
	d.outObjs.resize(64);
	d.outside.assign(6, 0);
	d.pool.population = 3;
	d.trafficMap.assign(8066 * 4, 0); // Tokyo's cell count
	d.settlement.funds = 12345; d.settlement.previousFunds = 12000;
	d.unusedRect = Rect();
	for (uint8_t & f : d.uiFlags) f = 1;
	d.hasWorldStats = true;
	d.wsA = 4; d.wsB = 8; d.wsF49 = 2;

	std::vector<uint8_t> bytes;
	std::string error;
	EXPECT(writeBytes(d, bytes, error));
	BldData back;
	EXPECT(parseBytes(bytes, back, error));
	EXPECT(error.empty());
	EXPECT(back.usedTenants() == 1 && back.tenants.size() == 128);
	EXPECT(back.tenants[1].belongList.size() == 3 && back.tenants[1].estimate == 150);
	EXPECT(back.tenants[5].hdr.status == 1 && !back.tenants[5].hdr.used);
	EXPECT(back.tenants[1].area.left == 150 && back.tenants[1].area.bottom == 109);
	EXPECT(back.trafficMap.size() == 8066 * 4 && back.hasWorldStats && back.wsB == 8);
	std::vector<uint8_t> again;
	EXPECT(writeBytes(back, again, error));
	EXPECT(again == bytes);

	// Strict: a truncated file and a damaged marker fail with a location.
	BldData broken;
	std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + bytes.size() / 2);
	EXPECT(!parseBytes(cut, broken, error) && !error.empty());
	std::vector<uint8_t> bad = bytes;
	bad[0] = 'X';
	EXPECT(!parseBytes(bad, broken, error) && error.find("expected") != std::string::npos);
	error.clear();

	// Import: x = unit - 184 (Tokyo width / 2), y = 110 - bottom row.
	std::string xml;
	ImportReport report;
	EXPECT(toTowerXml(back, bldTestPrototypes, xml, report, error));
	EXPECT(report.worldId == "tokyo");
	EXPECT(xml.find("type=\"office\" x=\"-34\" y=\"1\" evaluation=\"50.000\" yootEstimate=\"150\" occupied=\"1\" population=\"3\"") != std::string::npos);
	EXPECT(xml.find("funds=\"1234500\" rating=\"2\" time=\"17.416667\"") != std::string::npos);
	EXPECT(xml.find("lastQuarterBalance=\"1200000\" quarterIncome=\"34500\"") != std::string::npos);
}

/// Plugin-specific save data: Stadium.t2p's extra record (n = 0x405 over a
/// shorter record), Billboard.t2p's out-object record, and the tower events
/// without a length prefix (HotSpring: nothing; metro/cruise: people ids).
static void testBldPluginSaveData()
{
	using namespace OT::Bld;
	BldData d;
	d.world = "KJNS";
	d.level = 4;
	PluginInfo stadium, billboard;
	stadium.type = "lPnT"; stadium.id = "MUDS"; stadium.itemType = 2001; stadium.name = "Final";
	billboard.type = "lPoO"; billboard.id = "LLIB"; billboard.itemType = 2100; billboard.name = "Billboard";
	d.plugins = {stadium, billboard};
	d.people.resize(256);
	d.vRange = 2; d.hRange = 3;
	d.unitMap.assign(2 * 3 * 4, 0);
	d.tenants.resize(128);
	Tenant & s = d.tenants[0];
	s.hdr.used = 1; s.hdr.status = 30; s.equipID = 1000; s.equipType = 2001;
	s.ce.tag = "AECx";
	// pstr "Towers", u8, u16, two teams (pstr "Tigers" / "", u8, 126 bytes), u8, u8, u16.
	const char * stadiumName = "Towers";
	s.extra.push_back(6);
	s.extra.insert(s.extra.end(), stadiumName, stadiumName + 6);
	s.extra.insert(s.extra.end(), 3, 0);
	s.extra.push_back(6);
	s.extra.insert(s.extra.end(), stadiumName, stadiumName + 6);
	s.extra.insert(s.extra.end(), 127, 7);
	s.extra.push_back(0);
	s.extra.insert(s.extra.end(), 127, 0);
	s.extra.insert(s.extra.end(), 4, 1);
	s.extraCount = 0x405;
	d.tenants[1] = s;
	d.tenants[1].equipID = 1001;
	d.tenants[1].extra.clear();
	d.tenants[1].extraCount = 0; // a Stadium record without its game record
	d.movers.resize(32);
	d.requests.resize(64);
	d.outObjs.resize(64);
	OutObj & b = d.outObjs[0];
	b.hdr.used = 1; b.outObjID = 1000; b.outObjType = 2100; b.area = {10, 0, 14, 2};
	b.extra = {'D', 'A', 'W', 'G', 1, 0}; // ad sub-plugin mID + s16
	d.outside.assign(6, 0);
	d.trafficMap.assign(4284 * 4, 0);
	TowerEventRec hot, metroFull, metroEmpty, vip;
	hot.type = "vESH"; hot.id = 128; hot.standby = 1;
	metroFull.type = "vERM"; metroFull.id = 129; metroFull.firstPeople = 1000; metroFull.lastPeople = 77;
	metroEmpty.type = "vERM"; metroEmpty.id = 130;
	vip.type = "vEiV"; vip.id = 128; vip.data.assign(14, 0);
	d.events = {hot, metroFull, metroEmpty, vip};
	for (uint8_t & f : d.uiFlags) f = 1;
	d.hasWorldStats = true;

	std::vector<uint8_t> bytes, again;
	std::string error;
	EXPECT(writeBytes(d, bytes, error));
	BldData back;
	EXPECT(parseBytes(bytes, back, error));
	if (!error.empty()) std::cerr << "bld plugin data: " << error << "\n";
	EXPECT(back.tenants[0].extraCount == 0x405 && back.tenants[0].extra.size() == s.extra.size());
	EXPECT(back.tenants[1].hdr.used && back.tenants[1].extra.empty());
	EXPECT(back.outObjs[0].extra.size() == 6 && back.outObjs[0].outObjType == 2100);
	EXPECT(back.events.size() == 4 && back.events[1].firstPeople == 1000 && back.events[1].lastPeople == 77);
	EXPECT(back.events[2].firstPeople == 0 && back.events[3].data.size() == 14);
	EXPECT(back.trafficMap.size() == 4284 * 4);
	EXPECT(writeBytes(back, again, error) && again == bytes);

	// Game-written saves (oracle, ~/yoottower-decomp/save2-agent/saves):
	// Stadium, Kegon (HotSpring event) and Billboard towers.
	const char * home = std::getenv("HOME");
	static const char * saves[] = {"stadium.bld", "kegon.bld", "billboard.bld", "bridge.bld", "bridge_fresh.bld"};
	for (const char * name : saves)
	{
		std::vector<uint8_t> in, out;
		if (!home || !readAllBytes(std::string(home) + "/yoottower-decomp/save2-agent/saves/" + name, in)) continue;
		BldData g;
		const bool ok = parseBytes(in, g, error) && writeBytes(g, out, error) && out == in;
		EXPECT(ok);
		if (!ok) std::cerr << name << ": " << error << "\n";
		if (std::string(name) == "stadium.bld")
		{
			int stadiums = 0;
			for (const Tenant & t : g.tenants)
				if (t.hdr.used && t.equipType == 2001 && t.extraCount == 0x405 && t.extra.size() >= 264 && t.extra.size() < 0x405) stadiums++;
			EXPECT(stadiums == 1);
		}
		if (std::string(name) == "kegon.bld" && ok)
		{
			// Kegon round trip: import, export on the save as template. The
			// unit map's 0x8000 FINF flags come back from Kegon.t2p's mask.
			std::string xml;
			ImportReport report;
			EXPECT(toTowerXml(g, bldTestPrototypes, xml, report, error));
			EXPECT(report.worldId == "kegon" && xml.find("world=\"kegon\"") != std::string::npos);
			TowerState state;
			BldData exported;
			ImportReport exportReport;
			EXPECT(parseTowerXml(xml, state, error));
			EXPECT(exportTower(state, g, exported, exportReport, error, &yootTestPlugins()));
			if (!error.empty()) std::cerr << "kegon export: " << error << "\n";
			int flagsWant = 0, flagsGot = 0, flagsSame = 0;
			for (size_t c = 0; c * 4 < g.unitMap.size() && c * 4 < exported.unitMap.size(); c++)
			{
				const bool a = (g.unitMap[c * 4 + 1] & 0x80) != 0, b = (exported.unitMap[c * 4 + 1] & 0x80) != 0;
				flagsWant += a; flagsGot += b; flagsSame += a == b;
			}
			EXPECT(flagsWant > 5000 && flagsGot == flagsWant && flagsSame * 4 == (int)g.unitMap.size());
			EXPECT(exported.usedTenants() > 0 && exported.level == g.level);
			std::string again;
			ImportReport againReport;
			EXPECT(toTowerXml(exported, bldTestPrototypes, again, againReport, error));
			EXPECT(againReport.imported == report.imported);
		}
		if ((std::string(name) == "bridge.bld" || std::string(name) == "bridge_fresh.bld") && ok)
		{
			// Two towers, a 40-unit Sky Bridge on 9F (row 101) between units
			// 164 and 204. Once built the game keeps one Floor tenant from the
			// left tower through the bridge to the right tower and the bridge
			// is a room over it (status 10, pattern 3); while it is built the
			// bridge has a status-1 Floor of its own. Both export like the
			// finished bridge, also when the floor item has a hole under it.
			std::string xml;
			ImportReport report;
			EXPECT(toTowerXml(g, bldTestPrototypes, xml, report, error));
			EXPECT(xml.find("<item type=\"skybridge\" x=\"-20\" y=\"8\" evaluation=\"50.0\" width=\"40\"") != std::string::npos);
			std::string holed = xml;
			const std::string row = "<item type=\"floor\" x=\"-40\" y=\"8\" evaluation=\"50.0\" width=\"80\"";
			const size_t at = holed.find(row);
			EXPECT(at != std::string::npos);
			if (at != std::string::npos) holed.insert(at + row.size(), " holes=\"-20:20\"");
			for (const std::string * src : {&xml, &holed})
			{
				TowerState state;
				BldData exported;
				ImportReport exportReport;
				EXPECT(parseTowerXml(*src, state, error));
				EXPECT(exportTower(state, g, exported, exportReport, error, &yootTestPlugins()));
				EXPECT(exportReport.imported["skybridge"] == 1 && exportReport.dropped.empty());
				const Tenant * bridge = nullptr;
				int slabsOnRow = 0;
				uint16_t slabId = 0;
				for (const Tenant & t : exported.tenants)
				{
					if (!t.hdr.used || t.area.top != 101) continue;
					const PluginInfo * p = exported.pluginForItemType(t.equipType);
					if (p && macOrder(p->id) == "SKYB") bridge = &t;
					if (t.equipType == 1000) { slabsOnRow++; slabId = t.equipID; }
				}
				EXPECT(slabsOnRow == 1 && bridge && bridge->hdr.status == 10 && bridge->patIndex == 3 &&
				       bridge->area.left == 164 && bridge->area.right == 204 && bridge->ce.ids.items.size() == 1 &&
				       bridge->ce.ids.items[0] == slabId);
				const size_t c = ((size_t)101 * exported.hRange + 180) * 4;
				EXPECT(bridge && exported.unitMap[c + 2] == (bridge->equipID & 0xff) && exported.unitMap[c + 3] == (bridge->equipID >> 8));
			}
		}
		if (std::string(name) == "billboard.bld" && ok)
		{
			// Sample Tokyo Map + a Billboard on 7F-8F with the GAMEWORKS ad
			// (12 days): imported with its ad, exported back as the game wrote it.
			std::string xml;
			ImportReport report;
			EXPECT(toTowerXml(g, bldTestPrototypes, xml, report, error, subPluginLookup(yootTestPlugins())));
			EXPECT(xml.find("<item type=\"billboard\" x=\"-31\" y=\"6\" evaluation=\"50.0\" ad=\"GAMEWORKSAD.t2p\" daysLeft=\"12\"/>") != std::string::npos);
			TowerState state;
			BldData exported;
			ImportReport exportReport;
			EXPECT(parseTowerXml(xml, state, error));
			EXPECT(exportTower(state, g, exported, exportReport, error, &yootTestPlugins()));
			const OutObj * o = nullptr, * want = nullptr;
			for (const OutObj & e : exported.outObjs) if (e.hdr.used) o = &e;
			for (const OutObj & e : g.outObjs) if (e.hdr.used) want = &e;
			EXPECT(o && want && o->outObjType == 2000 && o->hdr.status == want->hdr.status && o->extra == want->extra &&
			       o->area.left == want->area.left && o->area.top == want->area.top && o->area.right == want->area.right &&
			       o->area.bottom == want->area.bottom);
			EXPECT(exported.outside == g.outside);
			std::vector<uint8_t> bytes2;
			BldData reread;
			EXPECT(writeBytes(exported, bytes2, error) && parseBytes(bytes2, reread, error));
		}
	}
}

// Every plugin that overrides its def's SaveExtraData/LoadExtraData (the
// table in doc/yoottower/bld-format.md 4.2) keeps its record through
// parse -> write byte for byte: the s32 n + n bytes path reads the record
// whatever plugin wrote it, so each record size gets its own tenant here.
static void testBldExtraRecordSizes()
{
	using namespace OT::Bld;
	struct Case { const char * plugin; int itemType; size_t n; };
	static const Case cases[] = {
		{"Theater", 2010, 20}, {"ClassicTheater", 2011, 20}, {"EventCenter", 2012, 64 + 9},
		{"Front", 1100, 20}, {"RFront", 1101, 20}, {"PowerRoom", 2013, 9}, {"Subway", 2014, 2},
		{"Attraction", 2015, 2}, {"Attraction (no record)", 2016, 0}, {"Boatdeck", 2017, 0},
	};
	BldData d;
	d.world = "KJNS";
	d.level = 4;
	d.people.resize(256);
	d.vRange = 2; d.hRange = 3;
	d.unitMap.assign(2 * 3 * 4, 0);
	d.tenants.resize(128);
	d.movers.resize(32);
	d.requests.resize(64);
	d.outObjs.resize(64);
	d.outside.assign(6, 0);
	d.trafficMap.assign(4284 * 4, 0);
	for (uint8_t & f : d.uiFlags) f = 1;
	d.hasWorldStats = true;
	uint16_t id = 1000;
	size_t slot = 0;
	for (const Case & c : cases)
	{
		Tenant & t = d.tenants[slot++];
		t.hdr.used = 1; t.hdr.status = 30; t.equipID = id++; t.equipType = (int16_t)c.itemType;
		t.ce.tag = "AECx";
		for (size_t k = 0; k < c.n; k++) t.extra.push_back((uint8_t)(0x30 + 7 * k));
	}
	std::vector<uint8_t> bytes, again;
	std::string error;
	EXPECT(writeBytes(d, bytes, error));
	BldData back;
	EXPECT(parseBytes(bytes, back, error));
	if (!error.empty()) std::cerr << "bld extra records: " << error << "\n";
	slot = 0;
	for (const Case & c : cases)
	{
		const Tenant & t = back.tenants[slot++];
		bool same = t.hdr.used && t.extra.size() == c.n;
		for (size_t k = 0; same && k < c.n; k++) same = t.extra[k] == (uint8_t)(0x30 + 7 * k);
		if (!same) std::cerr << "extra record of " << c.plugin << " changed\n";
		EXPECT(same);
	}
	EXPECT(writeBytes(back, again, error) && again == bytes);
}

static void testBldShippedSaves()
{
	using namespace OT::Bld;
	const std::string tokyoPath = yootSamplePath("Sample Tokyo Map.bld");
	const std::string hawaiiPath = yootSamplePath("Yooooot.bld");
	std::vector<uint8_t> tokyoBytes, hawaiiBytes;
	if (!readAllBytes(tokyoPath, tokyoBytes) || !readAllBytes(hawaiiPath, hawaiiBytes))
	{
		std::cout << "SKIP: YootTower sample saves not found (set OPENSKY_YOOT_DIR)\n";
		return;
	}
	EXPECT(looksLikeBld(tokyoPath));
	std::string error;

	// --- Sample Tokyo Map.bld --------------------------------------------
	BldData tokyo;
	EXPECT(parseBytes(tokyoBytes, tokyo, error));
	if (!error.empty()) std::cerr << "bld: " << error << "\n";
	EXPECT(macOrder(tokyo.world) == "SNJK");
	EXPECT(tokyo.level == 1 && tokyo.speed == -2);
	EXPECT(tokyo.year == 1 && tokyo.month0 == 0 && tokyo.minutes == 574 && tokyo.seconds == 2);
	EXPECT(tokyo.settlement.funds == 78770 && tokyo.fundsDollars() == 7877000);
	EXPECT(tokyo.settlement.previousFunds == 100000 && tokyo.settlement.totals[1] == 21230);
	EXPECT(tokyo.pool.population == 2 && tokyo.usedPeople() == 2);
	EXPECT(tokyo.vRange == 122 && tokyo.hRange == 368);
	EXPECT(tokyo.plugins.size() == 69);
	EXPECT(tokyo.usedTenants() == 26);
	EXPECT(pluginCount(tokyo, "FLOR") == 8);
	EXPECT(pluginCount(tokyo, "OFFC") == 9);
	EXPECT(pluginCount(tokyo, "TOIL") == 7);
	EXPECT(pluginCount(tokyo, "ICES") == 1);
	EXPECT(pluginCount(tokyo, "HREN") == 1);
	EXPECT(tokyo.trafficMap.size() == 8066 * 4);
	EXPECT(tokyo.events.size() == 24 && tokyo.hasWorldStats);
	EXPECT(tokyo.usedMovers() == 1);
	const Mover * elev = nullptr;
	for (const Mover & m : tokyo.movers) if (m.hdr.used) elev = &m;
	EXPECT(elev && elev->equipType == 1810 && macOrder(tokyo.pluginForItemType(1810)->id) == "StdE");
	if (elev)
	{
		EXPECT(elev->length == 10 && elev->area.left == 184 && elev->area.top == 99 && elev->area.bottom == 111);
		EXPECT(elev->modules.kind == "LMlE" && elev->modules.modules.size() == 6);
		int cars = 0;
		for (const MoverModule & c : elev->modules.modules) cars += c.hdr.used ? 1 : 0;
		EXPECT(cars == 2);
		EXPECT(elev->modules.modules[1].position == 9 && elev->modules.modules[1].stops.items.size() == 20);
		EXPECT(elev->reqIDs.items.size() == 20 && elev->reqIDs.items[2] == 0 && elev->reqIDs.items[6] == 1002);
	}
	std::vector<uint8_t> rebuilt;
	EXPECT(writeBytes(tokyo, rebuilt, error));
	EXPECT(rebuilt == tokyoBytes); // byte-identical round trip

	std::string xml;
	ImportReport report;
	EXPECT(toTowerXml(tokyo, bldTestPrototypes, xml, report, error));
	EXPECT(xml.find("<tower funds=\"7877000\" rating=\"0\" time=\"0.398611\"") != std::string::npos);
	EXPECT(xml.find("ruleset=\"yoottower\" world=\"tokyo\"") != std::string::npos);
	EXPECT(xml.find("lastQuarterBalance=\"10000000\" quarterIncome=\"0\" quarterExpenses=\"2123000\"") != std::string::npos);
	// 3-storey atrium lobby (Floor.t2p variation 1) on 1F; unit 147 -> x -37.
	EXPECT(xml.find("type=\"lobby\" x=\"-37\" y=\"0\" evaluation=\"50.0\" width=\"46\" height=\"3\"") != std::string::npos);
	EXPECT(xml.find("type=\"floor\" x=\"-37\" y=\"3\" evaluation=\"50.0\" width=\"46\"") != std::string::npos);
	EXPECT(xml.find("type=\"floor\" x=\"-31\" y=\"9\" evaluation=\"50.0\" width=\"40\"") != std::string::npos);
	EXPECT(countOf(xml, "type=\"floor\"") == 7);
	EXPECT(countOf(xml, "type=\"office\"") == 9);
	// Status 10 offices of kind 0 stand empty (no SetKind yet): not let.
	EXPECT(xml.find("type=\"office\" x=\"-13\" y=\"4\" evaluation=\"100.000\" yootEstimate=\"300\" occupied=\"0\"") != std::string::npos);
	EXPECT(xml.find("type=\"office\" x=\"-31\" y=\"4\" evaluation=\"50.0\" occupied=\"0\"") != std::string::npos);
	// Ice Cream (ICES) is its own plugin-backed shop, carrying the T2Tenant status.
	EXPECT(xml.find("type=\"yt_icecream\" x=\"-37\" y=\"3\" evaluation=\"50.0\" tenantStatus=\"12\"") != std::string::npos);
	// Simple Elevator: shaft x 0, 1F..10F, no stops on the atrium's 2F/3F, two cars.
	EXPECT(xml.find("type=\"elevator-standard\" x=\"0\" y=\"0\" evaluation=\"50.0\" height=\"10\" showShaft=\"1\"><unserviced floor=\"1\"/><unserviced floor=\"2\"/><car altitude=\"0.0\" homeFloor=\"0\"/><car altitude=\"9.0\" homeFloor=\"9\"/></item>") != std::string::npos);
	EXPECT(report.imported["lobby"] == 1 && report.imported["floor"] == 7 && report.imported["office"] == 9);
	EXPECT(report.imported["yt_icecream"] == 1 && report.imported["elevator-standard"] == 1);
	EXPECT(report.imported.count("shop") == 0);
	// Restroom.t2p (TOIL) maps to the native YootTower restroom (step 12).
	EXPECT(report.imported["restroom"] == 7 && countOf(xml, "type=\"restroom\"") == 7);
	EXPECT(report.dropped.count("TOIL Restroom (no OpenSky counterpart)") == 0);
	// Apartment.t2p (HREN) maps to the native YootTower apartment (step 14).
	EXPECT(report.imported["apartment"] == 1);
	EXPECT(xml.find("type=\"apartment\" x=\"5\" y=\"9\" evaluation=\"100.000\" yootEstimate=\"300\" occupied=\"1\" population=\"0\"") != std::string::npos);
	EXPECT(report.dropped.empty());
	if (!report.dropped.empty())
		for (const auto & d : report.dropped) std::cerr << "dropped: " << d.first << " x" << d.second << "\n";

	// --- Yooooot.bld: the empty Hawaii lot -------------------------------
	BldData hawaii;
	EXPECT(parseBytes(hawaiiBytes, hawaii, error));
	EXPECT(macOrder(hawaii.world) == "HWAI");
	EXPECT(hawaii.level == 2 && hawaii.pool.gameLevel == 2);
	EXPECT(hawaii.year == 1 && hawaii.month0 == 1 && hawaii.minutes == 647);
	EXPECT(hawaii.fundsDollars() == 5000000 && hawaii.pool.population == 0);
	EXPECT(hawaii.vRange == 62 && hawaii.hRange == 400);
	EXPECT(hawaii.usedTenants() == 0 && hawaii.usedMovers() == 0 && hawaii.usedPeople() == 0);
	EXPECT(hawaii.nextTenantNumber == 12);
	EXPECT(hawaii.plugins.size() == 73 && hawaii.events.size() == 11);
	EXPECT(hawaii.trafficMap.size() == 4284 * 4);
	EXPECT(writeBytes(hawaii, rebuilt, error));
	EXPECT(rebuilt == hawaiiBytes);
	report = ImportReport();
	EXPECT(toTowerXml(hawaii, bldTestPrototypes, xml, report, error));
	EXPECT(xml.find("<tower funds=\"5000000\" rating=\"1\" time=\"1.449306\"") != std::string::npos);
	EXPECT(xml.find("world=\"hawaii\"") != std::string::npos);
	EXPECT(xml.find("<item") == std::string::npos);

	// --- A game-written save with live state, when present ---------------
	std::vector<uint8_t> runBytes;
	const char * home = std::getenv("HOME");
	if (home && readAllBytes(std::string(home) + "/yoottower-decomp/save-agent/saves/tokyo_run1.bld", runBytes))
	{
		BldData run;
		EXPECT(parseBytes(runBytes, run, error));
		EXPECT(run.settlement.funds == 79047 && run.pool.population == 96 && run.usedPeople() == 63);
		EXPECT(writeBytes(run, rebuilt, error) && rebuilt == runBytes);
		report = ImportReport();
		EXPECT(toTowerXml(run, bldTestPrototypes, xml, report, error));
		// Working offices carry their 6 staff as population.
		EXPECT(xml.find("type=\"office\" x=\"-13\" y=\"4\" evaluation=\"100.000\" yootEstimate=\"300\" occupied=\"1\" population=\"6\"") != std::string::npos);
	}
	// --- Large Elevator export: one EgiB mover on the BigElev plugin ------
	{
		report = ImportReport();
		EXPECT(toTowerXml(tokyo, bldTestPrototypes, xml, report, error));
		TowerState large;
		EXPECT(parseTowerXml(xml, large, error));
		for (TowerItem & it : large.items)
			if (it.type == "elevator-standard") it.type = "elevator-express";
		BldData exported, reread;
		ImportReport exportReport;
		EXPECT(exportTower(large, tokyo, exported, exportReport, error, &yootTestPlugins()));
		EXPECT(exportReport.dropped.empty() && exportReport.imported["elevator-express"] == 1);
		EXPECT(exported.usedMovers() == 1);
		const Mover * big = nullptr;
		for (const Mover & m : exported.movers)
			if (m.hdr.used) big = &m;
		// BigElev.t2p: EgiB, itemType 1812, 8 car slots, 2 x 10 three-s16 table (n = 120).
		EXPECT(big && big->equipType == 1812);
		EXPECT(big && big->modules.modules.size() == 8 && big->modules.f20 == 8);
		EXPECT(big && big->extra.size() == 120 && big->length == 10);
		bool plugin = false;
		for (const PluginInfo & p : exported.plugins)
			plugin |= p.itemType == 1812 && p.id == "EgiB" && p.name == "Large Elevator";
		EXPECT(plugin);
		EXPECT(writeBytes(exported, rebuilt, error));
		EXPECT(parseBytes(rebuilt, reread, error));
		const Mover * back = nullptr;
		for (const Mover & m : reread.movers)
			if (m.hdr.used) back = &m;
		EXPECT(back && back->equipType == 1812 && back->modules.modules.size() == 8 && back->extra.size() == 120);
	}
	// --- Event Center booking: export -> import keeps the event -------------
	{
		// The exporter writes the booked id in all 12 month slots; the import
		// maps the coming month's slot back to the plugin, closed on the load
		// day and open for bookingDays - 1 days from the next one.
		report = ImportReport();
		EXPECT(toTowerXml(tokyo, bldTestPrototypes, xml, report, error));
		TowerState state;
		EXPECT(parseTowerXml(xml, state, error));
		TowerItem hall;
		for (const TowerItem & it : state.items)
			if (it.type == "floor" && it.width >= 31 && hall.type.empty())
			{
				hall.type = "eventcenter";    // 31 x 2 on the slab's row
				hall.x = it.x;
				hall.y = it.y;
				hall.event = "PlArt.t2p";
			}
		EXPECT(!hall.type.empty());
		state.items.push_back(hall);
		BldData exported, reread;
		ImportReport exportReport;
		EXPECT(exportTower(state, tokyo, exported, exportReport, error, &yootTestPlugins()));
		EXPECT(exportReport.imported["eventcenter"] == 1);
		EXPECT(writeBytes(exported, rebuilt, error) && parseBytes(rebuilt, reread, error));
		const Tenant * hallTenant = nullptr;
		for (const Tenant & t : reread.tenants)
			if (t.hdr.used && t.extra.size() >= 64 && t.equipType == 1722) hallTenant = &t;
		EXPECT(hallTenant && hallTenant->extra.size() == 64 + 9 * 0);
		if (hallTenant)
		{
			const uint32_t booked = hallTenant->extra[0] | (hallTenant->extra[1] << 8) | (hallTenant->extra[2] << 16) | ((uint32_t)hallTenant->extra[3] << 24);
			EXPECT(booked == 0x41727473 && hallTenant->extra[16 + 4 * 11] == 0x73 && hallTenant->extra[16 + 4 * 11 + 1] == 0x74);
		}
		std::string back;
		ImportReport backReport;
		EXPECT(toTowerXml(reread, bldTestPrototypes, back, backReport, error, SubPluginLookup(), hallEventLookup(yootTestPlugins())));
		const int loadDay = (int)std::max<int>(0, (int)tokyo.year - 1) * 12 + (int)(tokyo.month0 % 12);
		const std::string want = "event=\"PlArt.t2p\" startDay=\"" + std::to_string(loadDay + 1) + "\" span=\"2\"";
		EXPECT(backReport.imported["eventcenter"] == 1);
		EXPECT(back.find(want) != std::string::npos);
		// Without a lookup the booking cannot be named, so the hall is imported empty.
		std::string bare;
		ImportReport bareReport;
		EXPECT(toTowerXml(reread, bldTestPrototypes, bare, bareReport, error));
		EXPECT(bare.find("event=\"") == std::string::npos);
	}
	// --- Exporter: import -> export -> import is a fixed point -----------
	{
		report = ImportReport();
		EXPECT(toTowerXml(tokyo, bldTestPrototypes, xml, report, error));
		TowerState state;
		EXPECT(parseTowerXml(xml, state, error));
		EXPECT(state.world == "tokyo" && state.funds == 7877000 && state.items.size() == 27);
		BldData exported;
		ImportReport exportReport;
		EXPECT(exportTower(state, tokyo, exported, exportReport, error, &yootTestPlugins()));
		EXPECT(exportReport.dropped.empty());
		EXPECT(writeBytes(exported, rebuilt, error));
		BldData reread;
		EXPECT(parseBytes(rebuilt, reread, error));
		EXPECT(reread.usedTenants() == 26 && reread.usedMovers() == 1 && reread.usedPeople() == 0);
		EXPECT(reread.settlement.funds == 78770 && reread.minutes == 574 && reread.level == 1);
		// The game's own layout: 16 waiting areas identical to the sample's,
		// and StdElev's fixed 2 x 10 extra-data table.
		int requests = 0;
		bool sameRequests = true;
		for (size_t i = 0; i < reread.requests.size() && i < tokyo.requests.size(); i++)
		{
			const Request & a = reread.requests[i], & b = tokyo.requests[i];
			requests += a.hdr.used ? 1 : 0;
			sameRequests = sameRequests && a.hdr.used == b.hdr.used && a.requestID == b.requestID && a.upDown == b.upDown &&
				a.f18 == b.f18 && a.f1c == b.f1c && a.drawWidth == b.drawWidth;
		}
		EXPECT(requests == 16 && sameRequests);
		const Mover * m = nullptr;
		for (const Mover & mv : reread.movers) if (mv.hdr.used) m = &mv;
		EXPECT(m && m->extra.size() == 80 && m->reqIDs.items == elev->reqIDs.items && m->area.top == 99);
		std::string again;
		ImportReport againReport;
		EXPECT(toTowerXml(reread, bldTestPrototypes, again, againReport, error));
		// Plugin tenants come back just built (status 9): no staff people
		// are exported, so the game hires them again.
		std::string expected = xml;
		const std::string iceStatus = "type=\"yt_icecream\" x=\"-37\" y=\"3\" evaluation=\"50.0\" tenantStatus=\"";
		const size_t at = expected.find(iceStatus + "12\"");
		EXPECT(at != std::string::npos);
		if (at != std::string::npos) expected.replace(at, iceStatus.size() + 3, iceStatus + "9\"");
		// The visitor pool comes back with the exporter's population 0.
		auto stripPool = [](std::string x) {
			const size_t p = x.find(" yootPool=\"");
			if (p != std::string::npos) x.erase(p, x.find('"', p + 11) + 1 - p);
			return x;
		};
		EXPECT(again.find(" yootPool=\"1 1 ") != std::string::npos);
		// The tower's own pool is exported as it is (only the people out
		// of it are not).
		{
			auto poolOf = [](const std::string & x) {
				const size_t p = x.find(" yootPool=\"") + 11;
				OT::Yoot::Pool pool;
				pool.decode(x.substr(p, x.find('"', p) - p));
				return pool;
			};
			const OT::Yoot::Pool a = poolOf(xml), b = poolOf(again);
			bool same = !a.demands.empty() && a.demands.size() == b.demands.size() && b.population == 0;
			for (size_t d = 0; same && d < a.demands.size(); ++d)
			{
				same = a.demands[d].current == b.demands[d].current && a.demands[d].hourInZone == b.demands[d].hourInZone &&
					a.demands[d].zones.size() == b.demands[d].zones.size();
				for (size_t z = 0; same && z < a.demands[d].zones.size(); ++z)
					same = a.demands[d].zones[z].types == b.demands[d].zones[z].types &&
						a.demands[d].zones[z].limit == b.demands[d].zones[z].limit;
			}
			EXPECT(same);
		}
		again = stripPool(again);
		expected = stripPool(expected);
		EXPECT(again == expected);
		if (again != expected) std::cerr << "re-import differs:\n" << again << "\nvs\n" << expected << "\n";

		// Two shafts must keep their own boarding cells. The original's
		// FillRequestID splits an odd gap toward the right shaft.
		{
			TowerState two;
			EXPECT(parseTowerXml("<tower world=\"tokyo\"><item type=\"lobby\" x=\"-20\" y=\"0\" width=\"40\" height=\"1\"/>"
				"<item type=\"floor\" x=\"-20\" y=\"1\" width=\"40\"/>"
				"<item type=\"elevator-standard\" x=\"-4\" y=\"0\" height=\"2\"/>"
				"<item type=\"elevator-standard\" x=\"5\" y=\"0\" height=\"2\"/></tower>", two, error));
			BldData queues;
			ImportReport qr;
			EXPECT(exportTower(two, tokyo, queues, qr, error, &yootTestPlugins()));
			const Mover & left = queues.movers[0], & right = queues.movers[1];
			const int row = queues.requests[0].f1c;
			auto moverCell = [&](int x) {
				const size_t at = ((size_t)row * queues.hRange + x) * 4;
				return queues.unitMap[at] | (queues.unitMap[at + 1] << 8);
			};
			EXPECT(moverCell(left.area.left) == left.equipID);
			EXPECT(moverCell(left.area.left - 4) == left.reqIDs.items[0]);
			EXPECT(moverCell(left.area.right + 2) == right.reqIDs.items[0]);
			EXPECT(moverCell(right.area.left) == right.equipID);
			EXPECT(queues.requests[1].drawWidth == 0 && queues.requests[4].drawWidth == 1);
		}

		// An OpenSky-authored Hawaii tower against the empty-lot template.
		const std::string authored =
			"<tower funds=\"4321000\" rating=\"1\" time=\"13.5\" ruleset=\"yoottower\" world=\"hawaii\">"
			"<item type=\"lobby\" x=\"-20\" y=\"0\" width=\"40\" height=\"1\"/>"
			"<item type=\"floor\" x=\"-20\" y=\"1\" width=\"40\"/><item type=\"floor\" x=\"-20\" y=\"2\" width=\"40\"/>"
			"<item type=\"shop\" x=\"-20\" y=\"1\" let=\"1\"/>"
			"<item type=\"elevator-standard\" x=\"10\" y=\"0\" height=\"3\"><car altitude=\"2.0\" homeFloor=\"0\"/></item>"
			"<item type=\"cinema\" x=\"-20\" y=\"3\"/></tower>";
		EXPECT(parseTowerXml(authored, state, error));
		EXPECT(exportTower(state, hawaii, exported, exportReport, error, &yootTestPlugins()));
		EXPECT(exportReport.dropped["cinema (no YootTower counterpart in the exporter)"] == 1);
		EXPECT(exportReport.imported["lobby"] == 1 && exportReport.imported["elevator-standard"] == 1);
		EXPECT(exported.year == 2 && exported.month0 == 1 && exported.minutes == 720 && exported.level == 2);
		// No pool of its own: the template's pool runs on the save's clock
		// (12:00, not the template's 10:47) with fresh search windows.
		bool poolOnClock = !exported.pool.timeZones.empty();
		for (const TimeZone & z : exported.pool.timeZones)
		{
			if (z.f28 > 0) poolOnClock = poolOnClock && z.currentItem == 12 / z.f28 && z.f30 == 12 % z.f28;
			for (const TypeArray & a : z.typeArrays) poolOnClock = poolOnClock && a.f28 == 1 && a.f2c == (int32_t)a.types.size();
		}
		EXPECT(poolOnClock);
		{
			// Star levels outside the world's grades do not load in T2.exe
			// (level 6 -> "File load error"): clamp to startGrade..5.
			TowerState graded = state;
			BldData clamped;
			ImportReport clampReport;
			graded.rating = 5;
			EXPECT(exportTower(graded, hawaii, clamped, clampReport, error, &yootTestPlugins()));
			EXPECT(clamped.level == 5 && clamped.pool.gameLevel == 5);
			EXPECT(std::any_of(clampReport.notes.begin(), clampReport.notes.end(), [](const std::string & n) { return n.find("Star level 6 written as 5") == 0; }));
			graded.rating = 0;   // Hawaii starts at grade 2
			EXPECT(exportTower(graded, hawaii, clamped, clampReport, error, &yootTestPlugins()));
			EXPECT(clamped.level == 2 && clamped.pool.gameLevel == 2);
		}
		{
			// A Gathering Area (ATTR lvl 4) is refused by the original at three stars
			// ("Plugin Check Error" at load): the export raises the star level to the
			// highest grade of the plugins it writes.
			const std::string gathering =
				"<tower funds=\"4321000\" rating=\"2\" time=\"13.5\" ruleset=\"yoottower\" world=\"hawaii\">"
				"<item type=\"lobby\" x=\"-40\" y=\"0\" width=\"80\" height=\"1\"/>"
				"<item type=\"floor\" x=\"-40\" y=\"1\" width=\"80\"/>"
				"<item type=\"gatheringarea\" x=\"-20\" y=\"1\"/></tower>";
			TowerState gatheringState;
			BldData gatheringOut;
			ImportReport gatheringReport;
			EXPECT(parseTowerXml(gathering, gatheringState, error));
			EXPECT(exportTower(gatheringState, hawaii, gatheringOut, gatheringReport, error, &yootTestPlugins()));
			EXPECT(gatheringReport.imported["gatheringarea"] == 1);
			EXPECT(gatheringOut.level == 4 && gatheringOut.pool.gameLevel == 4);
			EXPECT(gatheringOut.pluginForItemType(1741) != nullptr);
			EXPECT(std::any_of(gatheringReport.notes.begin(), gatheringReport.notes.end(), [](const std::string & n) { return n.find("Star level 3 raised to 4") == 0; }));
		}
		{
			// The plugin table is per save: plugins the template lacks are
			// added (in id order) when the world can install them; Kegon's
			// Attraction is refused by Hawaii and dropped.
			const std::string venues =
				"<tower funds=\"4321000\" rating=\"3\" time=\"13.5\" ruleset=\"yoottower\" world=\"hawaii\">"
				"<item type=\"lobby\" x=\"-40\" y=\"0\" width=\"80\" height=\"1\"/>"
				"<item type=\"floor\" x=\"-40\" y=\"1\" width=\"80\"/>"
				"<item type=\"yootcathedral\" x=\"-40\" y=\"1\"/><item type=\"attraction\" x=\"0\" y=\"1\"/></tower>";
			TowerState venueState;
			BldData withVenues;
			ImportReport venueReport;
			EXPECT(parseTowerXml(venues, venueState, error));
			EXPECT(exportTower(venueState, hawaii, withVenues, venueReport, error, &yootTestPlugins()));
			EXPECT(venueReport.imported["yootcathedral"] == 1 && venueReport.imported.count("attraction") == 0);
			int cathedral = 0;
			for (const PluginInfo & p : withVenues.plugins)
				if (p.id == "HTAC") cathedral++;
			EXPECT(cathedral == 1 && withVenues.pluginForItemType(2000) && !withVenues.pluginForItemType(1702));
			EXPECT(std::is_sorted(withVenues.plugins.begin(), withVenues.plugins.end(), [](const PluginInfo & a, const PluginInfo & b) {
				return macOrder(a.id) < macOrder(b.id);
			}));
		}
		EXPECT(writeBytes(exported, rebuilt, error) && parseBytes(rebuilt, reread, error));
		EXPECT(toTowerXml(reread, bldTestPrototypes, again, againReport, error));
		EXPECT(again.find("type=\"elevator-standard\" x=\"10\" y=\"0\" evaluation=\"50.0\" height=\"3\" showShaft=\"1\"><car altitude=\"2.0\" homeFloor=\"0\"/></item>") != std::string::npos);
		// The generic shop exports as Ice Cream and comes back as yt_icecream.
		EXPECT(again.find("type=\"yt_icecream\" x=\"-20\" y=\"1\" evaluation=\"50.0\" tenantStatus=\"9\"") != std::string::npos);
		// Unit map: the shaft column carries mover 1 from machine row to pit.
		const size_t pitCell = ((size_t)(55 + 1 - 1) * 400 + 210) * 4;
		EXPECT(reread.unitMap[pitCell] == 1 && reread.unitMap[pitCell + 1] == 0);

		// Two towers on one row (floor holes), a Sky Bridge across the gap,
		// build days, the secret flag and the traffic map survive the trip.
		const std::string bridged =
			"<tower funds=\"9000000\" rating=\"1\" time=\"30.5\" ruleset=\"yoottower\" world=\"tokyo\" yootSecretItems=\"1\" yootTraffic=\",3:400\">"
			"<item type=\"lobby\" x=\"-40\" y=\"0\" width=\"80\" height=\"1\"/>"
			"<item type=\"floor\" x=\"-40\" y=\"7\" width=\"80\" holes=\"-20:20\" buildDay=\"12\"/>"
			"<item type=\"floor\" x=\"-40\" y=\"8\" width=\"80\"/>"
			"<item type=\"skybridge\" x=\"-20\" y=\"8\" width=\"40\" buildDay=\"25\"/></tower>";
		EXPECT(parseTowerXml(bridged, state, error));
		EXPECT(state.items.size() == 4 && state.items[1].holes.size() == 1 && state.items[3].buildDay == 25);
		EXPECT(exportTower(state, tokyo, exported, exportReport, error, &yootTestPlugins()));
		EXPECT(exportReport.imported["skybridge"] == 1 && exportReport.dropped.empty());
		EXPECT(exported.f9c == 1 && exported.trafficMap.size() >= 8 && exported.trafficMap[4] == 3 && exported.trafficMap[6] == (400 & 0xff));
		EXPECT(writeBytes(exported, rebuilt, error) && parseBytes(rebuilt, reread, error));
		int floorRuns = 0;
		for (const Tenant & t : reread.tenants)
			if (t.hdr.used && t.equipType == 1000 && t.area.right - t.area.left == 20) floorRuns++;
		EXPECT(floorRuns == 2);
		EXPECT(toTowerXml(reread, bldTestPrototypes, again, againReport, error));
		EXPECT(again.find("type=\"skybridge\" x=\"-20\" y=\"8\" evaluation=\"50.0\" width=\"40\" buildDay=\"25\"") != std::string::npos);
		EXPECT(again.find("type=\"floor\" x=\"-40\" y=\"7\" evaluation=\"50.0\" width=\"80\" holes=\"-20:20\"") != std::string::npos);
		EXPECT(again.find("type=\"floor\" x=\"-40\" y=\"8\" evaluation=\"50.0\" width=\"80\"/>") != std::string::npos);
		EXPECT(again.find("yootSecretItems=\"1\"") != std::string::npos && again.find("yootTraffic=\",3:400") != std::string::npos);
	}

	// Every other oracle-written save round-trips byte-identical.
	static const char * oracleSaves[] = {
		"save-agent/saves/tokyo_run2.bld", "econ-agent/s1_paused_load.bld", "econ-agent/s2_run.bld",
		"econ-agent/s3_m2_late.bld", "econ-agent/s4_m3_0010.bld", "econ-agent/s5_m3_0524.bld",
		"events-agent/blackout.bld", "events-agent/after_1602.bld",
	};
	for (const char * rel : oracleSaves)
	{
		if (!home || !readAllBytes(std::string(home) + "/yoottower-decomp/" + rel, runBytes)) continue;
		BldData run;
		const bool ok = parseBytes(runBytes, run, error) && writeBytes(run, rebuilt, error) && rebuilt == runBytes;
		EXPECT(ok);
		if (!ok) std::cerr << rel << ": " << error << "\n";
	}
}

// Step 12: trash, restrooms, colds and doctors (I2, I3, P5, P6, R6).
// Plugin-backed tenants (shops, restaurants, vending, service): catalog,
// plugin data, the family state machines and the toolbox grouping.
// Plan step 15 (P4): the T2Pool visitor pool, ported from T2DLL.
static void testYootPool()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();
	Yoot::PoolDef tokyo, kegon;
	const AbstractPlugin * tp = pm.get("Tokyo.t2p");
	const AbstractPlugin * kp = pm.get("Kegon.t2p");
	EXPECT(tp && tp->ensureLoaded() && Yoot::loadPoolDef(*tp, tokyo));
	EXPECT(kp && kp->ensureLoaded() && Yoot::loadPoolDef(*kp, kegon));
	if (!tokyo.valid || !kegon.valid) return;

	// PODF: per-grade PGDF ids, five demands (4/12/12/6/1 time zones; the
	// last is fixed), the special flag 0x83.
	EXPECT(tokyo.gradePoolId(1) == 128 && tokyo.gradePoolId(5) == 132 && tokyo.gradePoolId(6) == 0);
	EXPECT(kegon.gradePoolId(1) == 0 && kegon.gradePoolId(2) == 0 && kegon.gradePoolId(3) == 128);
	EXPECT(tokyo.demands.size() == 5 && tokyo.demands[0].numZones == 4 && tokyo.demands[1].numZones == 12 &&
	       tokyo.demands[3].numZones == 6 && tokyo.demands[4].numZones == 1 && tokyo.demands[4].fixed == 1);
	EXPECT(tokyo.specialFlag == 0x83 && tokyo.grades.size() == 5);
	// PZDF 1000: 8 types, counts raw x 20 / 100 (15 -> 3, 20 -> 4).
	EXPECT(tokyo.zones.count(1000) && tokyo.zones[1000].elems.size() == 8 &&
	       tokyo.zones[1000].elems[0].silhouette == 8 && tokyo.zones[1000].elems[0].count == 3 &&
	       tokyo.zones[1000].elems[4].count == 4);

	// Pool selection per grade: grade 1 gives demand 0 zone 1 (06:00-12:00)
	// PZDF 1000 x 10 = 200 types; grades 2 and 3 add 1000 x 5 and 1010 x 5.
	Yoot::Pool g1, g3, k1, k3;
	g1.create(tokyo, 1, 0);
	g3.create(tokyo, 3, 0);
	k1.create(kegon, 1, 0);
	k3.create(kegon, 3, 0);
	EXPECT(g1.gameLevel == 1 && g1.demands.size() == 5 && g1.demands[0].hoursPerZone == 6 && g1.demands[1].hoursPerZone == 2);
	EXPECT(g1.demands[0].zones[1].count() == 200 && g3.demands[0].zones[1].count() == 400);
	EXPECT(k1.totalTypes() == 0 && k3.totalTypes() > 0);
	// A new tower: Init with no season table opens no window (factor 0)
	// except the fixed demand's.
	EXPECT(g1.available(0) == 0 && g1.available(4) == 4);
	for (int h = 1; h <= 6; ++h) g1.incHour(h);
	EXPECT(g1.demands[0].current == 1 && g1.available(0) == 200 && g1.demands[1].current == 3);

	// T2PeopleDemandList::Call: the first type of the window that passes
	// T2PeopleType::Check; types are inserted in front, so PZDF order is
	// reversed (silhouette 36 first). The called type leaves the array.
	MemberDef any;
	any.demandType = 0;
	any.startSilhouetteType = -1;
	auto rng = []() { return 7; };
	Yoot::PeopleType t;
	EXPECT(g1.call(any, -1, Yoot::kRouteLobby, t, rng) && t.silhouette == 36 && t.econo == 1 && t.immortal());
	EXPECT(g1.population == 1 && g1.available(0) == 199 && g1.demands[0].zones[1].count() == 199);
	// Transport bits (CheckRouteFromPool mask): a lobby walker needs bit 0.
	EXPECT(!g1.call(any, -1, Yoot::kRouteParking | Yoot::kRouteSubway, t, rng));
	// Econo: [econo, econo + 2] unless econo < 0; Tokyo grade 1 zone 1 is all econo 1.
	EXPECT(!g1.call(any, 2, Yoot::kRouteLobby, t, rng) && g1.call(any, 0, Yoot::kRouteLobby, t, rng));
	// Demand -1 picks a random non-fixed list but never list 0 (engine quirk).
	MemberDef anyDemand = any;
	anyDemand.demandType = -1;
	Yoot::Pool only0 = g1;
	for (size_t d = 1; d < only0.demands.size(); ++d)
		for (Yoot::PeopleTypeArray & z : only0.demands[d].zones) { z.types.clear(); z.limit = 0; }
	EXPECT(!only0.call(anyDemand, -1, 0xff, t, rng));

	// T2PeopleType::Check rules.
	Yoot::PeopleType child, woman;
	child.silhouette = 9; child.econo = 2; child.transport = Yoot::kTransportSubway;
	woman.silhouette = 31; woman.econo = 3; woman.transport = Yoot::kTransportLobbyLeft;
	MemberDef kids = any; kids.startSilhouetteType = -3;            // ages 8..15
	MemberDef band = any; band.startSilhouetteType = 30; band.endSilhouetteType = 40;
	MemberDef men = any; men.optionType = 1;
	EXPECT(Yoot::checkPeopleType(child, kids, -1, Yoot::kRouteSubway) && !Yoot::checkPeopleType(child, kids, -1, Yoot::kRouteLobby));
	EXPECT(!Yoot::checkPeopleType(woman, kids, -1, 0xff) && Yoot::checkPeopleType(woman, band, 1, 0xff) && !Yoot::checkPeopleType(woman, band, 4, 0xff));
	MemberDef other = any; other.demandType = 2;
	EXPECT(!Yoot::checkPeopleType(woman, men, -1, 0xff) && Yoot::checkPeopleType(child, any, 0, 0xff));
	EXPECT(Yoot::checkWithDemand(woman, any, -1) && !Yoot::checkWithDemand(woman, other, -1));
	Yoot::PeopleType dup = Yoot::duplicateType(woman, 5); // 5 % 3 - 1 = +1
	EXPECT(dup.life == 1 && dup.econo == 4 && Yoot::duplicateType(child, 3).econo == 1);

	// T2Tenant::CheckRouteFromPool.
	Yoot::RouteFromPoolInputs ri;
	EXPECT(Yoot::checkRouteFromPool(ri) == 0);
	ri.lobby = true; ri.subway = true; ri.ship = true; ri.cruiseAvailable = true;
	EXPECT(Yoot::checkRouteFromPool(ri) == (Yoot::kRouteLobby | Yoot::kRouteShip));
	ri.trainAvailable = true; ri.parking = true;
	EXPECT(Yoot::checkRouteFromPool(ri) == 0x1f);

	// T2Pool::Enter: back in front of the window (callable from the next
	// zone change); an estimate above 200 also adds a mortal duplicate
	// behind it; 71..200 duplicates every 5th time; mortals at <= 70 vanish.
	Yoot::Pool e = g1;
	const int before = e.demands[0].zones[1].count();
	Yoot::PeopleType back = t;
	e.enter(back, 250, rng);
	EXPECT(e.shrinkCount == 1 && e.demands[0].zones[1].count() == before + 2 && e.demands[0].zones[1].start == 2);
	EXPECT(e.demands[0].zones[1].types.back().life == 1);
	Yoot::PeopleType mortal = back; mortal.life = 3;
	e.enter(mortal, 50, rng);
	EXPECT(e.demands[0].zones[1].count() == before + 2);
	for (int k = 0; k < 5; ++k) e.enter(back, 100, rng);
	EXPECT(e.demands[0].zones[1].count() == before + 2 + 5 + 1);
	// Midnight: the people who went home leave the population.
	const int pop = e.population;
	for (int h = 7; h <= 24; ++h) e.incHour(h % 24);
	EXPECT(e.population == pop - 7);
	EXPECT(e.shrinkCount == 0);

	// CollectEmployeeFromPool: the whole table must be in the pool; each
	// member is called numOfPeople times (the fixed demand 4 keeps its
	// types); the population is the tenant's, and immortals go back.
	{
		Yoot::Pool s = g1;
		MemberTableDef staff;
		MemberDef cook = any; cook.demandType = 4; cook.startSilhouetteType = 18; cook.endSilhouetteType = 18; cook.numOfPeople = 1;
		MemberDef waiter = cook; waiter.startSilhouetteType = waiter.endSilhouetteType = 19; waiter.numOfPeople = 2;
		staff.members = {cook, waiter};
		std::vector<Yoot::PeopleType> hired;
		const int pop0 = s.population;
		EXPECT(s.callBelong(staff, -1, Yoot::kRouteLobby, false, hired, rng) && hired.size() == 3);
		EXPECT(s.population == pop0 && s.available(4) == 4 && hired[1].silhouette == 19);
		MemberDef workers = any; workers.startSilhouetteType = 8; workers.endSilhouetteType = 9; workers.numOfPeople = 2;
		MemberDef nobody = any; nobody.startSilhouetteType = 60; nobody.endSilhouetteType = 61;
		MemberTableDef office; office.members = {workers, nobody};
		const int left = s.demands[0].zones[1].count();
		EXPECT(!s.callBelong(office, -1, Yoot::kRouteLobby, false, hired, rng) && hired.empty() && s.demands[0].zones[1].count() == left);
		office.members = {workers};
		EXPECT(s.callBelong(office, -1, Yoot::kRouteLobby, false, hired, rng) && hired.size() == 2 && s.demands[0].zones[1].count() == left - 2);
		s.releaseBelong(hired);
		EXPECT(s.demands[0].zones[1].count() == left && s.shrinkCount == 0);
	}

	// Save / load.
	std::string a, b;
	g3.encode(a);
	Yoot::Pool r;
	EXPECT(r.decode(a));
	r.encode(b);
	EXPECT(a == b && r.totalTypes() == g3.totalTypes());
	EXPECT(!r.decode("2 0"));
}

static void testYootTenants()
{
	using namespace OT;
	const PluginManager & pm = yootTestPlugins();

	// 57 covered plugins, unique prototype ids, all present and decodable.
	const std::vector<YootTenantPlugin> & covered = yootTenantPlugins();
	EXPECT(covered.size() == 56);
	std::set<std::string> ids;
	for (const YootTenantPlugin & t : covered) ids.insert(t.prototypeId);
	EXPECT(ids.size() == covered.size());
	const std::vector<YootTenantDef> defs = loadYootTenantDefs(pm);
	EXPECT(defs.size() == covered.size());
	int families[4] = {0, 0, 0, 0};
	for (const YootTenantDef & d : defs) families[(int)d.family]++;
	EXPECT(families[(int)YootTenantFamily::Shop] == 29 && families[(int)YootTenantFamily::Restaurant] == 22);
	EXPECT(families[(int)YootTenantFamily::Vending] == 5 && families[(int)YootTenantFamily::None] == 0);
	EXPECT(isYootTenantPrototypeId("yt_icecream") && !isYootTenantPrototypeId("shop") && !isYootTenantPrototypeId("yt_nothing"));

	// Prototypes offered per world (ATTR world mask through the unlock table).
	auto offered = [&](YootWorld::Id id) {
		const YootWorld w = loadYootWorld(pm, id);
		const std::map<std::string, YootUnlock> unlocks = resolveYootUnlocks(pm, w.worldMask);
		int n = 0;
		for (const auto & u : unlocks) if (isYootTenantPrototypeId(u.first) && u.second.available && !u.second.secret) ++n;
		return n;
	};
	EXPECT(offered(YootWorld::Id::Tokyo) == 34);
	EXPECT(offered(YootWorld::Id::Hawaii) == 26);
	EXPECT(offered(YootWorld::Id::Kegon) == 12);
	{
		// The Kegon Spa (ATTR spec+0x24 = 1) waits for the hot spring (doc+0x9c).
		const std::map<std::string, YootUnlock> ku = resolveYootUnlocks(pm, loadYootWorld(pm, YootWorld::Id::Kegon).worldMask);
		EXPECT(ku.count("yt_spa") && ku.at("yt_spa").available && ku.at("yt_spa").secret);
		RulesProfile kp = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Kegon));
		RulesProfile::Unlock su;
		su.minRating = 2;
		su.secret = true;
		kp.unlocks["yt_spa"] = su;
		BuildRules kr(BuildRules::Ruleset::YootTower, kp);
		EXPECT(kr.minRatingToBuild("yt_spa") == BuildRules::kUnavailable);
		kr.setSecretUnlocked(true);
		EXPECT(kr.minRatingToBuild("yt_spa") == 2);
		const std::map<std::string, YootUnlock> tu2 = resolveYootUnlocks(pm, loadYootWorld(pm, YootWorld::Id::Tokyo).worldMask);
		EXPECT(tu2.count("school") && tu2.at("school").available && !tu2.at("school").secret && tu2.at("school").minGrade == 4);
		EXPECT(tu2.count("skybridge") && tu2.at("skybridge").available && tu2.at("skybridge").minGrade == 1);
		EXPECT(ku.count("school") && !ku.at("school").available);
	}

	// A sample shop straight from Icecream.t2p.
	const YootTenantDef * ice = nullptr;
	const YootTenantDef * drink = nullptr;
	const YootTenantDef * burger = nullptr;
	for (const YootTenantDef & d : defs)
	{
		if (d.prototypeId == "yt_icecream") ice = &d;
		if (d.prototypeId == "yt_drink") drink = &d;
		if (d.prototypeId == "yt_burger") burger = &d;
	}
	EXPECT(ice && drink && burger);
	if (!ice || !drink || !burger) return;
	EXPECT(ice->name == "Ice Cream Stand" && ice->family == YootTenantFamily::Shop);
	EXPECT(ice->price == 100000 && ice->outMoney == 300 && ice->width == 12 && ice->height == 1);
	EXPECT(ice->openMinute == 720 && ice->closeMinute == 1200 && ice->bindMinutes == 20);
	EXPECT(ice->capacity == 15 && ice->inMoney == std::vector<int>({1000}) && ice->belongCapacity == 2);
	EXPECT(ice->gameLevel == 1 && ice->worldMask == 1 && ice->category == 80 && ice->toolNo == 10);
	EXPECT(ice->hasCustomers && ice->hasDayParam && ice->hasPlaceParam);
	// CalcInMoney: $1,000 x customers / 100 - $300 maintenance.
	Yoot::TenantModel probe;
	probe.customers = 50;
	EXPECT(Yoot::tenantIncome(probe, *ice) == 200);
	EXPECT(Yoot::tenantEstimate(probe, *ice) == 116); // 70 x (2 + 3) / 3

	// Engine helpers.
	EXPECT(Yoot::withinMinutes(720, 720, 1200) && !Yoot::withinMinutes(1200, 720, 1200));
	EXPECT(Yoot::withinMinutes(1300, 1080, 0) && !Yoot::withinMinutes(600, 1080, 0)); // Cocktail 18:00-24:00
	EXPECT(Yoot::withinMinutes(5, 300, 300) && Yoot::withinMinutes(1439, 300, 300));  // open == close: all day
	EXPECT(Yoot::withinMinutes(1436, 1435, 3) && Yoot::withinMinutes(2, -5, 3));       // wraps both ways
	EXPECT(Yoot::float2Int(0.89f, 7) == 1 && Yoot::float2Int(0.89f, 8) == 0 && Yoot::float2Int(2.0f, 0) == 2);
	EXPECT(std::fabs(Yoot::customerUnits(15, 20) - 6.0f) < 1e-4f && Yoot::customerUnits(5, 8) == 5.0f);
	EXPECT(Yoot::commercialPattern(Yoot::kStatusOpen, 0, 15) == 1 && Yoot::commercialPattern(Yoot::kStatusOpen, 1, 15) == 2);
	EXPECT(Yoot::commercialPattern(Yoot::kStatusOpen, 7, 15) == 3 && Yoot::commercialPattern(Yoot::kStatusClosed2, 0, 15) == 0);

	// Two simulated days: staff are hired, the shop opens at 12:00, fills,
	// pays CalcInMoney at 20:00 and closes; the vending machine runs round
	// the clock; the restaurant passes
	// through status 15 after closing.
	struct Run { int opened = -1, closed = -1, paid = 0, income = 0, maxInside = 0, maxPattern = 0, statusAfterClose = 0, called = 0, entered = 0; };
	// The customers come from the Tokyo pool (grade 1) through
	// CollectCustomerFromPool and walk for 4 minutes.
	Yoot::PoolDef tokyoPool;
	{
		const AbstractPlugin * w = pm.get("Tokyo.t2p");
		EXPECT(w && w->ensureLoaded() && Yoot::loadPoolDef(*w, tokyoPool));
	}
	struct PoolSource : Yoot::CustomerSources
	{
		Yoot::Pool * pool = nullptr;
		std::function<int()> rng;
		std::vector<long long> due;
		int called = 0;
		int fromPool(const YootTenantDef & def, int free, const Yoot::TenantInputs & in) override
		{
			const int n = Yoot::collectFromPool(def, *pool, Yoot::kRouteLobby, free, in.minute, rng,
				[&](const Yoot::PeopleType &, int start) { due.push_back(in.absMinute + (start - in.minute) + 4); });
			called += n;
			return n;
		}
	};
	auto simulate = [&tokyoPool](const YootTenantDef & def, int days) {
		Run r;
		Yoot::TenantModel m;
		unsigned seed = 12345;
		auto rng = [&seed]() { seed = seed * 1103515245u + 12345u; return (int)((seed >> 16) & 0x7fff); };
		Yoot::Pool pool;
		pool.create(tokyoPool, 1, 0);
		PoolSource src;
		src.pool = &pool;
		src.rng = rng;
		for (long long t = 0; t < days * 1440LL; ++t)
		{
			Yoot::TenantEvents ev;
			if (t % 60 == 0 && t > 0) pool.incHour((int)((t % 1440) / 60));
			for (size_t k = 0; k < src.due.size();)
				if (src.due[k] <= t) { src.due.erase(src.due.begin() + k); if (Yoot::customerArrives(m, def, t, ev)) r.entered++; }
				else ++k;
			Yoot::tenantMinute(m, def, t, ev);
			if (t % Yoot::kTenantIdleMinutes == 0)
			{
				Yoot::TenantInputs in;
				in.minute = (int)(t % 1440);
				in.absMinute = t;
				in.sources = &src;
				const int before = m.status;
				Yoot::tenantIdle(m, def, in, rng, ev);
				if (ev.callStaff) { ev.callStaff = false; m.staffIn = m.staff; }  // the staff walk in at once
				if (before < Yoot::kStatusOpen && m.status >= Yoot::kStatusOpen && r.opened < 0) r.opened = in.minute;
				if (before >= Yoot::kStatusOpen && m.status < Yoot::kStatusOpen && r.closed < 0)
				{
					r.closed = in.minute;
					r.statusAfterClose = m.status;
				}
			}
			if (ev.paid) { r.paid++; r.income += ev.income; }
			r.maxInside = std::max(r.maxInside, m.insideCount());
			r.maxPattern = std::max(r.maxPattern, m.pattern);
		}
		r.called = src.called;
		return r;
	};
	const Run shop = simulate(*ice, 2);
	EXPECT(shop.opened == 720 && shop.closed == 1200 && shop.paid == 2);
	EXPECT(shop.maxInside > 0 && shop.maxInside <= ice->capacity && shop.maxPattern >= 2);
	EXPECT(shop.statusAfterClose == Yoot::kStatusClosed2);
	if (shop.opened != 720 || shop.closed != 1200 || shop.paid != 2)
		std::cerr << "shop run: opened " << shop.opened << " closed " << shop.closed << " paid " << shop.paid << "\n";
	const Run vend = simulate(*drink, 2);
	EXPECT(vend.opened == 0 && vend.closed < 0 && vend.paid == 2); // Juice: 05:00-05:00
	const Run food = simulate(*burger, 2);
	EXPECT(food.opened == 480 && food.closed == 1320 && food.statusAfterClose == Yoot::kStatusAfterClose);

	// YootTower-only, and they replace the generic shop / restaurant / fast food.
	EXPECT(BuildRules(BuildRules::Ruleset::OpenSky).minRatingToBuild("yt_icecream") == BuildRules::kUnavailable);
	EXPECT(BuildRules(BuildRules::Ruleset::SimTower).minRatingToBuild("yt_burger") == BuildRules::kUnavailable);
	auto yootRules = [&](YootWorld::Id id) {
		RulesProfile profile = RulesProfile::yootTower(loadYootWorld(pm, id));
		for (const auto & u : resolveYootUnlocks(pm, profile.worldMask))
		{
			RulesProfile::Unlock unlock;
			unlock.available = u.second.available;
			unlock.minRating = u.second.minGrade - 1;
			profile.unlocks[u.first] = unlock;
		}
		return BuildRules(BuildRules::Ruleset::YootTower, profile);
	};
	const BuildRules tokyo = yootRules(YootWorld::Id::Tokyo);
	EXPECT(tokyo.minRatingToBuild("yt_icecream") == 0 && tokyo.minRatingToBuild("shop") == BuildRules::kUnavailable);
	EXPECT(tokyo.minRatingToBuild("restaurant") == BuildRules::kUnavailable && tokyo.minRatingToBuild("fastfood") == BuildRules::kUnavailable);
	EXPECT(tokyo.minRatingToBuild("yt_kicecream") == BuildRules::kUnavailable);
	const BuildRules kegon = yootRules(YootWorld::Id::Kegon);
	EXPECT(kegon.minRatingToBuild("yt_icecream") == BuildRules::kUnavailable && kegon.minRatingToBuild("yt_kicecream") != BuildRules::kUnavailable);

	// Toolbox: groups by category, items by tool number (GetSortKey).
	std::vector<YootToolboxEntry> entries;
	for (const YootTenantDef & d : defs)
	{
		YootToolboxEntry e;
		e.prototypeId = d.prototypeId;
		e.category = d.category;
		e.categoryName = d.categoryName;
		e.toolNo = d.toolNo;
		entries.push_back(e);
	}
	YootToolboxEntry lobby;
	lobby.prototypeId = "lobby"; lobby.category = 10; lobby.categoryName = "Basic"; lobby.toolNo = 20;
	entries.push_back(lobby);
	const std::vector<YootToolboxGroup> groups = yootToolboxGroups(entries);
	EXPECT(groups.size() >= 4 && groups[0].category == 10 && groups[0].prototypeIds == std::vector<std::string>({"lobby"}));
	bool sorted = true;
	for (size_t g = 1; g < groups.size(); ++g) sorted = sorted && groups[g - 1].category < groups[g].category;
	EXPECT(sorted);
	const YootToolboxGroup * shops = nullptr;
	const YootToolboxGroup * restaurants = nullptr;
	for (const YootToolboxGroup & g : groups)
	{
		if (g.category == 80) shops = &g;
		if (g.category == 50) restaurants = &g;
	}
	EXPECT(shops && shops->name == "Shops" && restaurants && restaurants->name == "Restaurants");
	EXPECT(shops && shops->prototypeIds.size() == 33); // 28 shops + 5 vending machines
	EXPECT(restaurants && restaurants->prototypeIds.size() == 22 && restaurants->prototypeIds.front() == "yt_coffee");
}

static void testYootFacilities()
{
	using namespace OT;
	namespace Y = OT::Yoot;
	const PluginManager & pm = yootTestPlugins();
	const RulesProfile tokyo = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Tokyo));
	const RulesProfile kegon = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Kegon));
	const RulesProfile stub = RulesProfile::yootTowerStub();
	const YootFacilityRules & r = tokyo.facilities;

	// WORLDDEF +0x170..0x178 and CODF id 1 (same in all worlds).
	EXPECT(r.dustOnGrade == 3 && r.toiletGrade == 1 && r.toiletDenomi == 10);
	EXPECT(r.hasColdTable && r.coldMinGrade == 3 && r.coldMonthly[0] == 20 && r.coldMonthly[1] == 30);
	EXPECT(kegon.facilities.dustOnGrade == 3 && kegon.facilities.coldMonthly[11] == 20);
	EXPECT(stub.facilities.dustOnGrade == 3 && stub.facilities.toiletDenomi == 10);

	// R6: IsDustOn / GetToiletDenomi / GetColdDenomi by grade.
	EXPECT(!Y::isDustOn(r, 2) && Y::isDustOn(r, 3) && Y::isDustOn(r, 5));
	EXPECT(Y::toiletDenomi(r, 0) == 0 && Y::toiletDenomi(r, 1) == 10);
	EXPECT(Y::coldDenomi(r, 2, 1) == 0);      // below grade 3
	EXPECT(Y::coldDenomi(r, 3, 0 + 1) == 30); // day 0 reads monthly[1]
	EXPECT(Y::coldDenomi(r, 3, 10 + 1) == 20);
	EXPECT(Y::coldDenomi(r, 4, 11 + 1) == 0); // last day of the year: index 12
	YootFacilityRules off = r;
	off.dustOnGrade = 0; off.toiletGrade = 0;
	EXPECT(!Y::isDustOn(off, 5) && Y::toiletDenomi(off, 5) == 0);
	EXPECT(Y::rollDenomi(10, 20) && !Y::rollDenomi(10, 21) && !Y::rollDenomi(0, 0));

	// I2: the warning window is 06:50-06:59 and 21:50-21:59 (exclusive bounds).
	EXPECT(!Y::trashWarningMinute(409) && Y::trashWarningMinute(410));
	EXPECT(Y::trashWarningMinute(419) && !Y::trashWarningMinute(420));
	EXPECT(!Y::trashWarningMinute(1309) && Y::trashWarningMinute(1310) && !Y::trashWarningMinute(1320));

	// Fill stages at 1, cap/4, 2cap/4, 3cap/4 and cap (TrashFacility cap 50).
	Y::TrashFacilityModel t;
	t.capacity = 50;
	int stages[51] = {0};
	for (int k = 1; k <= 50; ++k) { EXPECT(t.deliver()); stages[k] = t.pattern; }
	EXPECT(stages[1] == 1 && stages[11] == 1 && stages[12] == 2 && stages[24] == 3);
	EXPECT(stages[36] == 4 && stages[49] == 4 && stages[50] == 5);
	EXPECT(!t.accepts() && !t.deliver());
	// Close 07:00 empties it; open 07:30 reopens (TNDF 420/450).
	EXPECT(!t.idle(419, 450, 420) && t.idle(420, 450, 420));
	EXPECT(t.status == Y::TrashFacilityModel::kEmptying && t.pattern == 6 && t.garbage == 0 && !t.accepts());
	EXPECT(!t.idle(449, 450, 420) && t.idle(450, 450, 420) && t.pattern == 0 && t.accepts());
	Y::TrashFacilityModel empty;
	EXPECT(!empty.idle(420, 450, 420)); // an empty facility does not cycle

	// P5: SearchToilet.
	std::vector<Y::RestroomCandidate> rooms(3);
	int idx = 7;
	EXPECT(Y::searchToilet(rooms, idx) == Y::ToiletResult::NoneNearby && idx == -1);
	rooms[1].reachable = true; rooms[1].users = 5;
	EXPECT(Y::searchToilet(rooms, idx) == Y::ToiletResult::Crowded);
	rooms[2].reachable = true; rooms[2].users = 4;
	EXPECT(Y::searchToilet(rooms, idx) == Y::ToiletResult::Found && idx == 2);
	EXPECT(std::string(Y::toiletFailureMessage(Y::ToiletResult::Crowded)) == "Restroom is crowded.");
	EXPECT(std::string(Y::toiletFailureMessage(Y::ToiletResult::NoneNearby)) == "No restroom nearby.");
	EXPECT(Y::restroomPattern(0, 1, 5, true) == 1 && Y::restroomPattern(1, 2, 5, true) == 2);
	EXPECT(Y::restroomPattern(2, 3, 5, true) == 2 && Y::restroomPattern(2, 1, 5, false) == 1);
	EXPECT(Y::restroomPattern(1, 0, 5, false) == 0);

	// P6: doctors.
	std::vector<Y::DoctorCandidate> docs(2);
	EXPECT(Y::searchDoctor(docs) == -3);
	docs[0].reachable = true; // closed
	EXPECT(Y::searchDoctor(docs) == -2);
	docs[1].reachable = true; docs[1].open = true; docs[1].patients = 20;
	EXPECT(Y::searchDoctor(docs) == -1);
	docs[1].patients = 3;
	EXPECT(Y::searchDoctor(docs) == 1);
	EXPECT(!Y::doctorCallMinute(479) && Y::doctorCallMinute(480) && Y::doctorCallMinute(487));
	EXPECT(!Y::doctorCallMinute(488) && Y::doctorCallMinute(1020) && !Y::doctorCallMinute(1080));
	EXPECT(Y::doctorPattern(1, 1, 20, true) == 2 && Y::doctorPattern(2, 10, 20, true) == 3);
	EXPECT(Y::doctorPattern(3, 9, 20, false) == 2 && Y::doctorPattern(2, 0, 20, false) == 1);
	EXPECT(std::string(Y::trashCommentText(Y::TrashComment::Full)) == "Full of garbage.");

	// The native facilities exist only in YootTower and replace the
	// SimTower Recycling Center / Medical Center there.
	const BuildRules sim(BuildRules::Ruleset::SimTower);
	const BuildRules yoot(BuildRules::Ruleset::YootTower, tokyo);
	EXPECT(sim.minRatingToBuild("trashfacility") == BuildRules::kUnavailable);
	EXPECT(sim.minRatingToBuild("restroom") == BuildRules::kUnavailable);
	EXPECT(sim.minRatingToBuild("doctor") == BuildRules::kUnavailable);
	EXPECT(sim.minRatingToBuild("recycling") == LevelUp::minRatingToBuild("recycling"));
	EXPECT(yoot.minRatingToBuild("trashfacility") == 2 && yoot.minRatingToBuild("restroom") == 0);
	EXPECT(yoot.minRatingToBuild("doctor") == 2);
	EXPECT(yoot.minRatingToBuild("recycling") == BuildRules::kUnavailable);
	EXPECT(yoot.minRatingToBuild("medicalcenter") == BuildRules::kUnavailable);
	const std::map<std::string, YootUnlock> tu = resolveYootUnlocks(pm, 0x7);
	const std::map<std::string, YootUnlock> ku = resolveYootUnlocks(pm, 0xF0);
	EXPECT(tu.count("trashfacility") && tu.at("trashfacility").available && tu.at("trashfacility").minGrade == 3);
	EXPECT(tu.count("restroom") && tu.at("restroom").minGrade == 1);
	EXPECT(tu.count("doctor") && tu.at("doctor").available && tu.at("doctor").minGrade == 3);
	EXPECT(ku.count("trashfacility") && ku.at("trashfacility").available); // KTrashFacility
	EXPECT(ku.count("doctor") && !ku.at("doctor").available);

	// Catalog flags the facility rules read: TNDF attr 0x2000000 (needs a
	// toilet), category 100 (final item), plugin capacity and hours.
	const std::map<std::string, YootItemStats> tc = resolveYootCatalog(pm, 0x7);
	const std::map<std::string, YootItemStats> hc = resolveYootCatalog(pm, 0xF01);
	EXPECT(tc.count("office") && tc.at("office").needsToilet() && !tc.at("office").finalItem());
	EXPECT(tc.count("restroom") && !tc.at("restroom").needsToilet() && tc.at("restroom").capacity == 5);
	EXPECT(tc.count("trashfacility") && tc.at("trashfacility").capacity == 50);
	EXPECT(tc.at("trashfacility").openMinute == 450 && tc.at("trashfacility").closeMinute == 420);
	EXPECT(tc.count("doctor") && tc.at("doctor").capacity == 20 && tc.at("doctor").bindMinutes == 30);
	EXPECT(hc.count("yootcathedral") && hc.at("yootcathedral").finalItem());
}

// YootTower skin: guest room patterns and the tower-palette repaint.
static void testYootItemArt()
{
	using namespace OT;
	using Yoot::RoomLook;
	// Vacant rooms show pattern 0 day and night (oracle: status 30, patIndex 0).
	EXPECT(Yoot::roomLook(false, false, 0, 0, 0) == RoomLook::Vacant);
	EXPECT(Yoot::roomLook(true, false, 0, 0, 0) == RoomLook::Lit);       // let, guests out
	EXPECT(Yoot::roomLook(true, false, 2, 0, 0) == RoomLook::Lit);       // in, awake
	EXPECT(Yoot::roomLook(true, false, 2, 1, 0) == RoomLook::Lit);       // one still up
	EXPECT(Yoot::roomLook(true, false, 2, 2, 0) == RoomLook::Asleep);
	EXPECT(Yoot::roomLook(true, false, 1, 0, 1) == RoomLook::Morning);
	EXPECT(Yoot::roomLook(true, true, 0, 0, 0) == RoomLook::Dirty);
	EXPECT(Yoot::roomLook(false, true, 0, 0, 0) == RoomLook::Dirty);
	// 5 patterns per kind: Twinroom has 4 kinds (20 bands), Suiteroom 1.
	EXPECT(Yoot::roomPatternIndex(RoomLook::Asleep, 3, 4) == 17);
	EXPECT(Yoot::roomPatternIndex(RoomLook::Dirty, 5, 4) == 9);
	EXPECT(Yoot::roomPatternIndex(RoomLook::Lit, 7, 1) == 1);
	EXPECT(Yoot::roomPatternIndex(RoomLook::Vacant, -1, 2) == 5);
	// SetKind: kind = rand() % mNumOfKind once at BuildFinish; the stored
	// roll gives the same kind for every plugin kind count 1..6.
	for (int draw : {0, 7, 59, 60, 12345, 32767, 2147483647})
		for (int k = 1; k <= 6; ++k)
			EXPECT(Yoot::roomPatternIndex(RoomLook::Lit, Yoot::roomKindRoll(draw), k) == 5 * (draw % k) + 1);
	EXPECT(Yoot::roomKindRoll(-1) == 59);

	// Only indices inside the animated range take the palette colour.
	std::vector<YootRgb> pal(256);
	pal[189] = YootRgb{10, 20, 30};
	pal[200] = YootRgb{40, 50, 60};
	const std::vector<uint8_t> idx = {0, 189, 200, 188};
	std::vector<uint8_t> rgba(16, 7);
	rgba[7] = 0; // a masked window pixel becomes opaque sky colour
	EXPECT(yootPaintAnimatedIndices(idx, pal, 189, 48, rgba) == 2);
	EXPECT(rgba[0] == 7 && rgba[3] == 7);
	EXPECT(rgba[4] == 10 && rgba[5] == 20 && rgba[6] == 30 && rgba[7] == 255);
	EXPECT(rgba[8] == 40 && rgba[12] == 7);
	EXPECT(yootPaintAnimatedIndices(idx, pal, 93, 96, rgba) == 1); // Kegon: 93..188

	// Floor.t2p DrawProc: the 14-unit fixed part ends on a unit u % 8 == 7
	// (oracle: Tokyo lobby at unit 36 shows 11 units, the loop from 47).
	EXPECT(yootFloorFixedUnits(14, 36) == 11);
	EXPECT(yootFloorFixedUnits(14, 33) == 14);
	EXPECT(yootFloorFixedUnits(14, 32) == 7);
	for (int u = 0; u < 16; ++u)
		EXPECT((u + yootFloorFixedUnits(14, u)) % 8 == 7);
	// Outside view parts: Floor.t2p 1003 (Tokyo, kind 2) and Office.t2p.
	EXPECT(100000 + yootOutViewPattern(1, 0, 2) == 130100);
	EXPECT(100000 + yootOutViewPattern(3, 1, 2) == 131300);
	EXPECT(yootOutViewPattern(1, 0, 2) == 30100 && yootOutViewPattern(1, 0, 0) == 10100);
}

// Step 14 part A: hotel back office, parking, fire department, Kegon sizes.
static void testYootSpecials()
{
	using namespace OT;
	namespace Y = OT::Yoot;
	const PluginManager & pm = yootTestPlugins();

	// W4 room linking: Single/Twin/Suite depend on the Front Desk (NORF);
	// the Kegon rooms do not. Front.t2p FUN_10013b70 links every unlinked
	// room in route range of an opening desk.
	YootPluginIdentity front, single, small;
	EXPECT(pm.get("Front.t2p") && readYootPluginIdentity(*pm.get("Front.t2p"), front) && front.id == Y::kFrontDeskId);
	EXPECT(pm.get("Singleroom.t2p") && readYootPluginIdentity(*pm.get("Singleroom.t2p"), single));
	EXPECT(pm.get("Smallroom.t2p") && readYootPluginIdentity(*pm.get("Smallroom.t2p"), small));
	EXPECT(Y::roomNeedsFrontDesk(single.dependId) && !Y::roomNeedsFrontDesk(small.dependId));
	std::vector<Y::RoomLink> rooms(4);
	rooms[0].reachable = true; rooms[1].reachable = false;
	rooms[2].reachable = true; rooms[2].linkedDesk = 7; rooms[3].reachable = true;
	EXPECT(Y::linkRooms(9, rooms) == 2);
	EXPECT(rooms[0].linkedDesk == 9 && rooms[1].linkedDesk == 0 && rooms[2].linkedDesk == 7 && rooms[3].linkedDesk == 9);
	EXPECT(Y::linkRooms(9, rooms) == 0); // linked rooms stay with their desk
	EXPECT(Y::guestroomType(1401) == 0 && Y::guestroomType(1481) == 0 && Y::guestroomType(1402) == 1);
	EXPECT(Y::guestroomType(1482) == 1 && Y::guestroomType(1403) == 2 && Y::guestroomType(1400) == -1);
	EXPECT(Y::frontDeskWarningMinute(420) && Y::frontDeskWarningMinute(427) && !Y::frontDeskWarningMinute(428));
	EXPECT(Y::frontDeskWarningMinute(1320) && !Y::frontDeskWarningMinute(1319));

	// E7 check-in income: booked per room type at the desk, paid at 12:00
	// (FUN_10015540), cleared at 17:00 (FUN_10015450).
	const std::map<std::string, YootItemStats> tc = resolveYootCatalog(pm, 0x7);
	const std::map<std::string, YootItemStats> kc = resolveYootCatalog(pm, 0xF0);
	EXPECT(tc.count("hotel_single") && tc.count("hotel_suite") && tc.count("frontdesk") && tc.count("housekeeping"));
	if (tc.count("hotel_single") && tc.count("hotel_suite")) {
		const YootItemStats & s = tc.at("hotel_single");
		EXPECT(Y::roomNeedsFrontDesk(s.dependId) && s.toolType == 1401);
		Y::FrontDeskLedger ledger;
		EXPECT(ledger.selection[0] == 3); // GetInMoneyIndex default -> inMoney[2]
		ledger.checkIn(Y::guestroomType(s.toolType), Y::hotelRoomIncome(s, 2));
		ledger.checkIn(Y::guestroomType(s.toolType), Y::hotelRoomIncome(s, 2));
		const YootItemStats & suite = tc.at("hotel_suite");
		ledger.checkIn(Y::guestroomType(suite.toolType), Y::hotelRoomIncome(suite, 2));
		ledger.checkIn(-1, 5000); // not a guest room
		EXPECT(ledger.accrued[0] == 4000 && ledger.accrued[2] == 9000);
		EXPECT(ledger.noonIncome() == 13000 && ledger.noonIncome() == 13000); // paying keeps the table
		ledger.reset17();
		EXPECT(ledger.noonIncome() == 0);
	}
	EXPECT(Y::tenantWindow(720, Y::kHotelPayMinute) && Y::tenantWindow(727, Y::kHotelPayMinute));
	EXPECT(!Y::tenantWindow(728, Y::kHotelPayMinute) && !Y::tenantWindow(719, Y::kHotelPayMinute));
	if (tc.count("frontdesk")) {
		const YootItemStats & d = tc.at("frontdesk");
		EXPECT(d.price == 100000 && d.outMoney == 10000 && d.width == 15 && d.belongCapacity == 3);
		EXPECT(d.openMinute == 360 && d.closeMinute == 60);
	}
	if (kc.count("frontdesk")) EXPECT(kc.at("frontdesk").plugin == "RFront.t2p" && kc.at("frontdesk").price == 50000);
	else EXPECT(false);

	// Housekeeping cycle (RMaintenance FUN_1000bd00 / FUN_1000bf70): maids go
	// out 07:00..14:00 (open .. close - 120), up to three rooms per trip.
	EXPECT(!Y::housekeepingDispatchMinute(419, 420, 960) && Y::housekeepingDispatchMinute(420, 420, 960));
	EXPECT(Y::housekeepingDispatchMinute(840, 420, 960) && !Y::housekeepingDispatchMinute(841, 420, 960));
	const std::vector<bool> dirty = {false, true, true, false, true, true};
	EXPECT(Y::maidTrip(dirty) == std::vector<int>({1, 2, 4}));
	EXPECT(Y::maidTrip(std::vector<bool>(3, false)).empty());
	if (tc.count("housekeeping") && kc.count("housekeeping")) {
		EXPECT(tc.at("housekeeping").belongCapacity == 6 && tc.at("housekeeping").openMinute == 420);
		EXPECT(Y::roomNeedsFrontDesk(tc.at("housekeeping").dependId));
		// Kegon variant with its own size (RMaintenance 12x1, 4 maids).
		EXPECT(kc.at("housekeeping").width == 12 && kc.at("housekeeping").belongCapacity == 4);
	}
	else EXPECT(false);

	// I6 parking: the gate/lot placement rules are live with the gate item.
	const BuildRules yoot(BuildRules::Ruleset::YootTower, RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Tokyo)));
	const BuildRules kegonRules(BuildRules::Ruleset::YootTower, RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Kegon)));
	const BuildRules sim(BuildRules::Ruleset::SimTower);
	EXPECT(yoot.minRatingToBuild("parkinggate") == 2 && yoot.minRatingToBuild("parkinglot") == 2);
	EXPECT(yoot.minRatingToBuild("parking") == BuildRules::kUnavailable);
	EXPECT(sim.minRatingToBuild("parkinggate") == BuildRules::kUnavailable);
	EXPECT(sim.minRatingToBuild("parking") == LevelUp::minRatingToBuild("parking"));
	EXPECT(std::string(BuildRules::yootOnlyPlugin("parkinglot")) == "Parking.t2p" && !BuildRules::yootOnlyPlugin("office"));
	if (tc.count("parkinglot") && tc.count("parkinggate")) {
		EXPECT(tc.at("parkinglot").price == 5000 && tc.at("parkinglot").variation == 0 && tc.at("parkinglot").width == 4);
		EXPECT(tc.at("parkinggate").price == 50000 && tc.at("parkinggate").outMoney == 10000 && tc.at("parkinggate").variation == 1);
	}
	else EXPECT(false);
	std::vector<Y::ParkingPiece> park;
	Y::ParkingPiece gate; gate.gate = true; gate.floor = -1; gate.height = 2; gate.minX = 0; gate.maxX = 16;
	park.push_back(gate);
	Y::ParkingPiece lotB1; lotB1.floor = -1; lotB1.minX = 16; lotB1.maxX = 20;
	EXPECT(Y::parkingPlaceRule(park, lotB1) == 0); // next to the gate's B1 row
	Y::ParkingPiece lotB2 = lotB1; lotB2.floor = -2;
	EXPECT(Y::parkingPlaceRule(park, lotB2) == Y::kErrLotNotAdjacent);
	park.push_back(lotB1);
	EXPECT(Y::parkingRemoveRule(park, gate) == Y::kErrGateRemoveLot);
	EXPECT(Y::parkingIsGate(16) && !Y::parkingIsGate(4));
	EXPECT(Y::gatePattern(true, true) == 0 && Y::gatePattern(false, true) == 1 && Y::gatePattern(false, false) == 2);
	Y::ParkingLotModel lot;
	for (int id = 1; id <= 5; ++id) EXPECT(lot.park(id * 100));
	EXPECT(!lot.hasSpace() && !lot.park(42) && lot.cars() == 5);
	EXPECT(lot.pattern == 500 % 13 + 1);
	EXPECT(lot.leave(500) && lot.pattern == 400 % 13 + 1 && lot.hasSpace());
	EXPECT(!lot.leave(500) && lot.park(42) && lot.cars() == 5);
	EXPECT(Y::parkingWarningMinute(370) && Y::parkingWarningMinute(379) && !Y::parkingWarningMinute(380));
	EXPECT(Y::parkingWarningMinute(1270) && !Y::parkingWarningMinute(1280));

	// I4/I5 fire department: registry 5 like the Security Room; CallFireman
	// sends six people in total, in list order. Kegon only, grade 4, and
	// Kegon's T2EV list has no FBEv, so it never meets a fire there.
	EXPECT(Y::callFiremen({6, 8}) == std::vector<int>({6, 0}));
	EXPECT(Y::callFiremen({2, 8}) == std::vector<int>({2, 4}));
	EXPECT(Y::callFiremen({}).empty());
	EXPECT(kegonRules.minRatingToBuild("firedept") == 3 && sim.minRatingToBuild("firedept") == BuildRules::kUnavailable);
	const std::map<std::string, YootUnlock> tu = resolveYootUnlocks(pm, 0x7), ku = resolveYootUnlocks(pm, 0xF0);
	EXPECT(ku.count("firedept") && ku.at("firedept").available && ku.at("firedept").minGrade == 4);
	EXPECT(tu.count("firedept") && !tu.at("firedept").available);
	EXPECT(tu.count("frontdesk") && tu.at("frontdesk").minGrade == 2 && ku.at("frontdesk").minGrade == 3);
	EXPECT(tu.count("parkinggate") && tu.at("parkinggate").minGrade == 3 && !ku.at("parkinggate").available);
	if (kc.count("firedept")) EXPECT(kc.at("firedept").belongCapacity == 8 && kc.at("firedept").width == 12);
	else EXPECT(false);
	bool kegonFire = false;
	for (const YootEvent & e : loadYootWorld(pm, YootWorld::Id::Kegon).events) kegonFire = kegonFire || e.resType == "FBEV";
	EXPECT(!kegonFire);

	// Residential: Apartment.t2p (Tokyo) rents for 3 x inMoney a quarter.
	EXPECT(yoot.minRatingToBuild("apartment") == 0);
	if (tc.count("apartment")) {
		const YootItemStats & a = tc.at("apartment");
		EXPECT(a.inMoney == std::vector<int>({300, 500, 700, 1000}) && a.width == 4 && a.belongCapacity == 1);
		EXPECT(Y::officeQuarterRent(a, 2) == 2100); // oracle: settlement 1102 -21 units
	}
	else EXPECT(false);
	const std::map<std::string, YootItemStats> hc = resolveYootCatalog(pm, 0xF01);
	EXPECT(hc.count("yoot_condo") && hc.at("yoot_condo").plugin == "HawaianCondo.t2p");
	EXPECT(hc.count("apartment") == 0u);

	// M4 + Kegon variants with per-world sizes (catalog overlay).
	if (kc.count("elevator-standard") && kc.count("elevator-service")) {
		const YootItemStats & ke = kc.at("elevator-standard");
		EXPECT(ke.plugin == "KElev.t2p" && ke.maxLength == 50 && ke.capacity == 20);
		EXPECT(ke.price == 50000 && ke.carPrice == 20000 && ke.outMoney == 5000);
		EXPECT(kc.at("elevator-service").plugin == "KServ.t2p" && kc.at("elevator-service").maxLength == 1000);
	}
	else EXPECT(false);
	if (kc.count("restroom") && kc.count("trashfacility") && kc.count("floor")) {
		EXPECT(kc.at("restroom").plugin == "KRestroom.t2p" && kc.at("restroom").width == 10);
		EXPECT(kc.at("trashfacility").width == 17 && kc.at("trashfacility").height == 2);
		EXPECT(kc.at("floor").plugin == "KFloor.t2p");
	}
	else EXPECT(false);
}

// Step 13: one placement predicate test per rule (L1, L2, M6, I7).
static void testYootPlacementRules()
{
	using namespace OT;
	namespace Y = OT::Yoot;
	const PluginManager & pm = yootTestPlugins();
	const RulesProfile tokyoProfile = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Tokyo));
	const RulesProfile hawaiiProfile = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Hawaii));
	const BuildRules tokyo(BuildRules::Ruleset::YootTower, tokyoProfile);
	const BuildRules hawaii(BuildRules::Ruleset::YootTower, hawaiiProfile);

	// L2 final item: bottom on the top floor (Tokyo 100F, Hawaii 45F), the
	// rest rises above the floor area inside the build area.
	EXPECT(Y::finalItemRule(true, 99, 5, tokyoProfile.highestFloor) == 0);
	EXPECT(Y::finalItemRule(true, 98, 5, tokyoProfile.highestFloor) == Y::kErrFinalTopFloor);
	EXPECT(Y::finalItemRule(true, 44, 4, hawaiiProfile.highestFloor) == 0);
	EXPECT(!hawaii.allowsFootprint(0, 44, 48, 4));
	EXPECT(hawaii.allowsSpecialFootprint(0, 44, 48, 4));   // rows 7..10 are in the build area
	EXPECT(!hawaii.allowsSpecialFootprint(0, 44, 48, 9));  // row 2 is not
	EXPECT(tokyo.allowsSpecialFootprint(0, 99, 56, 5));    // Stadium 56x5: rows 6..10
	// ... and only final items may touch the top floor.
	EXPECT(Y::finalItemRule(false, 98, 1, tokyoProfile.highestFloor) == 0);
	EXPECT(Y::finalItemRule(false, 99, 1, tokyoProfile.highestFloor) == Y::kErrOnlyFinalOnTop);
	EXPECT(Y::finalItemRule(false, 98, 2, tokyoProfile.highestFloor) == Y::kErrOnlyFinalOnTop);
	EXPECT(std::string(Y::buildErrorText(Y::kErrOnlyFinalOnTop)) == "Only Final item can be placed on the top floor of building.");

	// I7 subway: bottom row just below the lowest floor (Tokyo B10).
	EXPECT(tokyoProfile.lowestFloor == -9);
	EXPECT(Y::subwayRule(-10, tokyoProfile.lowestFloor) == 0);
	EXPECT(Y::subwayRule(-9, tokyoProfile.lowestFloor) == Y::kErrSubwayLowest);
	EXPECT(Y::subwayRule(-11, tokyoProfile.lowestFloor) == Y::kErrSubwayLowest);
	EXPECT(!tokyo.allowsFootprint(0, -10, 40, 3) && tokyo.allowsSpecialFootprint(0, -10, 40, 3));
	EXPECT(!tokyo.allowsSpecialFootprint(0, -11, 40, 3)); // row 120 is outside
	EXPECT(std::string(Y::buildErrorText(Y::kErrSubwayLowest)) == "The Subway can only be placed on the lowest floor.");

	// M6 no sky lobby: lobbies on 1F only.
	EXPECT(Y::lobbyRule(0) == 0 && Y::lobbyRule(15) == Y::kErrLobbyFirstFloorOnly && Y::lobbyRule(-1) != 0);
	EXPECT(std::string(Y::buildErrorText(Y::kErrLobbyFirstFloorOnly)) == "The lobby item can be only placed on the first floor.");

	// L1 parking: gates stack down from 1F at one x; lots touch a gate/lot.
	std::vector<Y::ParkingPiece> park;
	Y::ParkingPiece gate; gate.gate = true; gate.floor = -1; gate.height = 2; gate.minX = 0; gate.maxX = 4;
	EXPECT(Y::parkingPlaceRule(park, gate) == 0);
	Y::ParkingPiece deepGate = gate; deepGate.floor = -3; deepGate.height = 2;
	EXPECT(Y::parkingPlaceRule(park, deepGate) == Y::kErrGateNotFromFirst);
	park.push_back(gate);
	EXPECT(Y::parkingPlaceRule(park, deepGate) == 0);
	Y::ParkingPiece shifted = deepGate; shifted.minX = 8; shifted.maxX = 12;
	EXPECT(Y::parkingPlaceRule(park, shifted) == Y::kErrGateNotFromFirst);
	Y::ParkingPiece lot; lot.floor = 0; lot.minX = 4; lot.maxX = 8;
	EXPECT(Y::parkingPlaceRule(park, lot) == 0);      // next to the gate's top row
	Y::ParkingPiece far = lot; far.minX = 20; far.maxX = 24;
	EXPECT(Y::parkingPlaceRule(park, far) == Y::kErrLotNotAdjacent);
	park.push_back(lot);
	Y::ParkingPiece lot2 = lot; lot2.minX = 8; lot2.maxX = 12;
	park.push_back(lot2);
	// ... removal: outer spaces first; gates last and bottom-up.
	EXPECT(Y::parkingRemoveRule(park, lot) == Y::kErrLotRemoveOrder);
	EXPECT(Y::parkingRemoveRule(park, lot2) == 0);
	EXPECT(Y::parkingRemoveRule(park, gate) == Y::kErrGateRemoveLot);
	std::vector<Y::ParkingPiece> gatesOnly(1, gate);
	gatesOnly.push_back(deepGate);
	EXPECT(Y::parkingRemoveRule(gatesOnly, gate) == Y::kErrGateRemoveOrder);
	EXPECT(Y::parkingRemoveRule(gatesOnly, deepGate) == 0);

	// L1 floor removal order.
	Y::FloorRemoval f;
	f.floor = 5;
	EXPECT(Y::floorRemoveRule(f) == 0);
	f.tenantAbove = true;
	EXPECT(Y::floorRemoveRule(f) == Y::kErrFloorNotTop);
	f.tenantAbove = false; f.tenantBelow = true;
	EXPECT(Y::floorRemoveRule(f) == 0);                // above ground only "above" counts
	f.floor = -2;
	EXPECT(Y::floorRemoveRule(f) == Y::kErrFloorNotLowest);
	f.tenantBelow = false; f.tenantOnFloor = true;
	EXPECT(Y::floorRemoveRule(f) == Y::kErrFloorHasTenant);
	f.peopleOnFloor = true;
	EXPECT(Y::floorRemoveRule(f) == Y::kErrFloorHasPeople);
	f.moverOnFloor = true;
	EXPECT(Y::floorRemoveRule(f) == Y::kErrFloorHasTransit);
	Y::FloorRemoval lobby;
	EXPECT(Y::floorRemoveRule(lobby) == Y::kErrFloorCannotRemove);
}

// Step 14 part B: venues, final items and the subway (S1, S2, R4, V6, I7, M8).
static std::string qtAtom(const char * type, const std::string & body)
{
	std::string a;
	const uint32_t n = (uint32_t)(body.size() + 8);
	a += (char)(n >> 24); a += (char)(n >> 16); a += (char)(n >> 8); a += (char)n;
	a.append(type, 4);
	return a + body;
}

static std::string qtBe32(uint32_t v)
{
	std::string s;
	s += (char)(v >> 24); s += (char)(v >> 16); s += (char)(v >> 8); s += (char)v;
	return s;
}

static void testYootVenues()
{
	using namespace OT;
	namespace Y = OT::Yoot;
	const PluginManager & pm = yootTestPlugins();
	auto movieDef = [&](const char * name) {
		MovieDef d;
		AbstractPlugin * p = pm.get(name);
		EXPECT(p && p->ensureLoaded() && p->getMovieDef(d));
		return d;
	};

	// MODF life cycle (GetMovieLifePhase 0x10043320): 1st Contact p = 0/6/18.
	const MovieDef contact = movieDef("MV_1stContact.t2p");
	EXPECT(contact.phase1Months == 0 && contact.phase2Months == 6 && contact.phase3Months == 18);
	EXPECT(contact.famousAudience == 300 && contact.remakeAudience == 70 && contact.unkFC == 500);
	const Y::MovieCycle cc = Y::movieCycle(contact);
	EXPECT(cc.p1 == 0 && cc.p2 == 6 && cc.p3 == 24 && cc.cycle() == 24);
	Y::MovieState s;
	EXPECT(Y::movieLifePhase(contact, s, 10, 10) == 1);
	EXPECT(Y::movieLifePhase(contact, s, 15, 10) == 1 && Y::movieLifePhase(contact, s, 16, 10) == 2);
	EXPECT(Y::movieLifePhase(contact, s, 34, 10) == 1); // next cycle
	const MovieDef hours = movieDef("MV_49h.t2p");     // 12/6/18
	EXPECT(Y::movieLifePhase(hours, s, 0, 0) == 0 && Y::movieLifePhase(hours, s, 12, 0) == 1);
	EXPECT(Y::movieLifePhase(hours, s, 17, 0) == 1 && Y::movieLifePhase(hours, s, 18, 0) == 2);
	EXPECT(Y::isRoadshowFilm(contact) && !Y::isFamousFilm(contact, s));
	EXPECT(Y::isFamousFilm(movieDef("MV_Shakeit.t2p"), s)); // filmKind 2

	// ReMake 0x100434c0: famous at R >= 300, sequel at R >= 70, else retired.
	Y::MovieState hit; hit.bestR = 320;
	Y::movieReMake(contact, hit);
	EXPECT((hit.flags & Y::MovieState::kFamous) && (hit.flags & Y::MovieState::kSequel));
	EXPECT(hit.bestF == 320 && hit.bestR == -1 && hit.remakeCount == 1);
	EXPECT(Y::isFamousFilm(contact, hit));
	EXPECT(Y::movieTitle(contact, hit, false) == "1st Contact PART II");
	EXPECT(Y::movieTitle(contact, hit, true) == "1st Contact");
	Y::MovieState ok; ok.bestR = 100;
	Y::movieReMake(contact, ok);
	EXPECT(!(ok.flags & Y::MovieState::kFamous) && (ok.flags & Y::MovieState::kSequel) && ok.bestF == -1);
	Y::MovieState flop; flop.bestR = 50;
	Y::movieReMake(contact, flop);
	EXPECT((flop.flags & Y::MovieState::kRetired) && Y::movieLifePhase(contact, flop, 10, 10) == 4);
	const MovieDef cellmax = movieDef("MV_Cellmax.t2p");  // famous 30, remake 200
	Y::MovieState cm; cm.bestR = 100;
	Y::movieReMake(cellmax, cm);
	EXPECT((cm.flags & Y::MovieState::kFamous) && (cm.flags & Y::MovieState::kRetired) && cm.bestF == 100);
	// ChkReMake 0x10043720: commit the pending record; judge on day 24, 48...
	Y::MovieState p; p.pendingR = 90;
	EXPECT(!Y::movieChkReMake(contact, p, 23) && p.bestR == 90 && p.pendingR == -1);
	EXPECT(!Y::movieChkReMake(contact, p, 0));
	EXPECT(Y::movieChkReMake(contact, p, 24) && (p.flags & Y::MovieState::kSequel));
	Y::MovieState never;
	EXPECT(!Y::movieChkReMake(contact, never, 24) && never.flags == 0); // never shown: not judged
	EXPECT(Y::movieOffered(contact, s, false, 1) && !Y::movieOffered(contact, s, false, 2));
	EXPECT(!Y::movieOffered(contact, s, true, 1) && Y::movieOffered(contact, hit, true, 2));

	// Show schedule (Theater.t2p 0x1000e5e0): 10:00-22:00, 120-minute films.
	EXPECT(Y::theaterShowCount(600, 1320, 120) == 4 && Y::theaterShowCount(720, 1320, 120) == 4);
	EXPECT(Y::theaterShowAt(599, 600, 1320, 120).status == Y::TheaterShow::Closed);
	EXPECT(Y::theaterShowAt(600, 600, 1320, 120).status == Y::TheaterShow::Doors);
	EXPECT(Y::theaterShowAt(630, 600, 1320, 120).status == Y::TheaterShow::Playing);
	EXPECT(Y::theaterShowAt(750, 600, 1320, 120).status == Y::TheaterShow::Doors && Y::theaterShowAt(750, 600, 1320, 120).show == 1);
	EXPECT(Y::theaterShowAt(1199, 600, 1320, 120).show == 3 && Y::theaterShowAt(1200, 600, 1320, 120).status == Y::TheaterShow::Closed);
	// Audience target 0x1000e640: record, then the movie's initial audience, then 95%.
	EXPECT(Y::theaterAudienceTarget(-1, 0, -1, contact.clipSeconds, 500, contact.unk104) == 140);
	EXPECT(Y::theaterAudienceTarget(-1, 0, 250, 140, 500, 95) == 250);
	EXPECT(Y::theaterAudienceTarget(-1, 0, -1, 0, 500, 95) == 500);
	EXPECT(Y::theaterAudienceTarget(140, 200, 250, 140, 500, 95) == 190);
	// Oracle (Classic, 12:00-22:00, bind 120): the 17:30 doors collect, the
	// 20:00 show does not; 3 full houses of 40 paid $1,400 at 22:00.
	EXPECT(Y::theaterCollects(1139, 720, 1320, 120) && !Y::theaterCollects(1140, 720, 1320, 120));
	EXPECT(Y::theaterCollects(1259, 600, 1320, 0) && !Y::theaterCollects(1260, 600, 1320, 0));
	EXPECT(Y::calcInMoneyUnits(120, 20, 10) * kYootMoneyUnit == 1400);
	// CalcInMoney: Theater inMoney 30 ($3,000), outMoney 20: 500 tickets = $13,000.
	const std::map<std::string, YootItemStats> tc = resolveYootCatalog(pm, 0x7);
	EXPECT(tc.count("theater") && tc.at("theater").capacity == 500);
	if (tc.count("theater")) EXPECT(Yoot::commercialIncome(500, tc.at("theater")) == 13000);

	// HEDF month mask (bit d = day-of-year d) and the settlement.
	auto hallDef = [&](const char * name) {
		HallEventDef d;
		AbstractPlugin * h = pm.get(name);
		EXPECT(h && h->ensureLoaded() && h->getHallEventDef(d));
		return d;
	};
	const HallEventDef tennis = hallDef("PlTennis.t2p");
	EXPECT(tennis.monthMask == 0x1F8u && tennis.bookingDays == 3 && tennis.cost == 400);
	EXPECT(!Y::hallEventInMonth(tennis, 2) && Y::hallEventInMonth(tennis, 3));
	EXPECT(Y::hallEventInMonth(tennis, 8) && !Y::hallEventInMonth(tennis, 9) && !Y::hallEventInMonth(tennis, 12));
	const HallEventDef art = hallDef("PlArt.t2p");
	EXPECT(art.monthMask == 0xF1Fu && Y::hallEventInMonth(art, 4) && !Y::hallEventInMonth(art, 5));
	EXPECT(Y::hallEventSettlement(tennis.cost, 100) == 30000 && Y::hallEventSettlement(0, 100) == -10000);
	EXPECT(Y::hallEventFlop(art, 49) && !Y::hallEventFlop(art, 50));
	EXPECT(Y::hallEventNextValue(art, 10, 10, 0) == 1000 && Y::hallEventNextValue(art, 60, 50, 10) == 110);
	EXPECT(Y::hallEventCollects(540, 540, 1080, 0) && Y::hallEventCollects(1019, 540, 1080, 0));
	EXPECT(!Y::hallEventCollects(1020, 540, 1080, 0)); // evening events draw nobody
	const HallEventDef rock = hallDef("PlConcert.t2p");
	EXPECT(rock.openTime == 1080 && rock.bookingDays == 1 && rock.monthMask == 0xFFFu);

	// ADDF: 12 campaign days, $50,000 up front, only on a 'BoPI' medium.
	static const char * ads[] = {"AirJamaica.t2p", "Aquazone.t2p", "GAMEWORKSAD.t2p", "Ob9003.t2p",
	                             "SandalBeachResort.t2p", "Segapc.t2p", "Yoot.t2p"};
	TemplateDef board;
	AbstractPlugin * bp = pm.get("Billboard.t2p");
	EXPECT(bp && bp->ensureLoaded() && bp->getTemplateDef(board) && board.kind == TemplateDef::OuterObj);
	uint32_t host = 0;
	for (int k = 3; k >= 0; --k) host = (host << 8) | (unsigned char)board.tool.subPluginType.c[k];
	EXPECT(host == 0x426F5049u); // 'BoPI'
	for (const char * name : ads)
	{
		AdvertiseDef d;
		AbstractPlugin * a = pm.get(name);
		EXPECT(a && a->ensureLoaded() && a->getAdvertiseDef(d));
		EXPECT(d.days == 12 && d.money * kYootMoneyUnit == 50000);
		EXPECT(Y::advertiseFits(d, host) && !Y::advertiseFits(d, 0x4D6F5049u)); // not on a 'MoPI' theater
	}

	// Final items: each world's item is category 100, and its event grants the
	// top grade only at grade == numGrades (doc vt+0x188, FUN_00422670).
	struct World { YootWorld::Id id; const char * item; uint32_t mask; };
	const World worlds[] = {{YootWorld::Id::Tokyo, "stadium", 0x7}, {YootWorld::Id::Hawaii, "yootcathedral", 0xF01},
	                        {YootWorld::Id::Kegon, "attraction", 0xF0}};
	for (const World & w : worlds)
	{
		const RulesProfile prof = RulesProfile::yootTower(loadYootWorld(pm, w.id));
		const std::map<std::string, YootItemStats> cat = resolveYootCatalog(pm, w.mask);
		EXPECT(cat.count(w.item) && cat.at(w.item).finalItem() && cat.at(w.item).maxBuild == 1);
		const int top = (int)prof.grades.size();
		EXPECT(top == 4);
		uint32_t flags = 0;
		EXPECT(!LevelUp::yootGrantFinalGrade(top - 1, top, prof.grades[top - 2].vipFlag, flags) && flags == 0);
		EXPECT(LevelUp::yootGrantFinalGrade(top, top, prof.grades[top - 1].vipFlag, flags));
		EXPECT(LevelUp::yootPromotionReady(prof.grades[top - 1].vipFlag, prof.grades[top - 1].population,
		                                   flags, prof.grades[top - 1].population));
	}
	EXPECT(!Y::stadiumSeason(2) && Y::stadiumSeason(3) && Y::stadiumSeason(9) && !Y::stadiumSeason(10));
	EXPECT(Y::baseballBeginDay(false, 5, true) && !Y::baseballBeginDay(true, 5, true));
	EXPECT(!Y::baseballBeginDay(false, 11, true) && !Y::baseballBeginDay(false, 5, false));
	EXPECT(Y::kBaseballVisitors == 200 && Y::kBaseballMinute == 1200);
	EXPECT(Y::stadiumStatus(10, 720, 720, 1380, true, 0) == 30 && Y::stadiumStatus(10, 720, 720, 1380, false, 0) == 10);
	EXPECT(Y::stadiumStatus(30, 960, 720, 1380, true, 0) == 31 && Y::stadiumStatus(31, 1080, 720, 1380, true, 1) == 33);
	EXPECT(Y::stadiumStatus(33, 1100, 720, 1380, true, 10) == 34 && Y::stadiumStatus(34, 1260, 720, 1380, true, 1) == 35);
	EXPECT(Y::stadiumStatus(35, 1379, 720, 1380, true, 1) == 35 && Y::stadiumStatus(35, 1380, 720, 1380, true, 1) == 10);
	EXPECT(Y::weddingBeginDay(false, true, true) && !Y::weddingBeginDay(false, false, true) && !Y::weddingBeginDay(true, true, true));
	EXPECT(Y::weddingReady(true, 30, true) && !Y::weddingReady(true, 29, true) && !Y::weddingReady(false, 40, true) && !Y::weddingReady(true, 40, false));
	EXPECT(Y::weddingAnnouncement("3F", "A", "4F", "B") ==
	       "'From now at the top of the Cathedral,,3F of A Mr. and ,4F of B Ms. of the wedding will start.'");
	EXPECT(Y::cathedralOpen(true, 660, 660, 1020) && !Y::cathedralOpen(false, 700, 660, 1020) && !Y::cathedralOpen(true, 1020, 660, 1020));
	EXPECT(Y::attractionRenovates(4, 200, false) && !Y::attractionRenovates(4, 199, false));
	EXPECT(!Y::attractionRenovates(5, 300, false) && !Y::attractionRenovates(4, 300, true));
	EXPECT(Y::attractionPattern(false, 5) == 0 && Y::attractionPattern(true, 0) == 1);
	EXPECT(Y::attractionPattern(true, 39) == 2 && Y::attractionPattern(true, 40) == 3);

	// Subway: MREV 12 trains; level L runs the types < L (4/8/10/12 trains).
	const RulesProfile tokyo = RulesProfile::yootTower(loadYootWorld(pm, YootWorld::Id::Tokyo));
	YootMetroDef metro;
	bool haveMetro = false;
	for (const YootEventSpec & e : tokyo.events) if (decodeYootMetro(e, metro)) { haveMetro = true; break; }
	EXPECT(haveMetro && metro.unk == 5000 && metro.trains.size() == 12u);
	EXPECT(Y::subwayArrivals(metro.trains, 1) == std::vector<int>({490, 650, 1060, 1270}));
	EXPECT(Y::subwayArrivals(metro.trains, 2).size() == 8u && Y::subwayArrivals(metro.trains, 3).size() == 10u);
	const std::vector<int> all = Y::subwayArrivals(metro.trains, 4);
	EXPECT(all.size() == 12u && all.front() == 410 && all.back() == 1420);
	EXPECT(Y::subwayStoppedTrain(all, 409) == -1 && Y::subwayStoppedTrain(all, 410) == 0);
	EXPECT(Y::subwayStoppedTrain(all, 417) == 0 && Y::subwayStoppedTrain(all, 418) == -1);
	EXPECT(Y::subwayStoppedTrain(all, 1425) == 11);
	// The stopped train's type picks MET1..MET4: 490 is a type-0 train.
	EXPECT(Y::subwayStoppedTrainType(metro.trains, 1, 495) == 0);
	EXPECT(Y::subwayStoppedTrainType(metro.trains, 1, 410) == -1);  // type >= 1 not enabled
	EXPECT(Y::subwayStoppedTrainType(metro.trains, 4, 410) >= 1);
	EXPECT(Y::subwayStoppedTrainType(metro.trains, 4, 409) == -1);
	{
		// MREV run (FUN_1003dc80), Tokyo 368 units, station at unit 84.
		const std::vector<std::pair<int, int> > one = {{600, 0}};
		auto at = [&](int level, int minute, int tick) {
			const std::vector<Y::SubwayTrainPos> v = Y::subwayTrains(one, level, 368, 84, minute, tick);
			return v.empty() ? INT_MIN : v[0].unit;
		};
		EXPECT(at(1, 507, 3) == INT_MIN);         // starts 92 minutes early
		EXPECT(at(1, 508, 0) == 84 - 368);
		EXPECT(at(1, 599, 3) == 83);
		EXPECT(at(1, 600, 0) == 84 && at(1, 607, 3) == 84);   // stops 8 minutes
		EXPECT(at(1, 609, 1) == 89);
		EXPECT(at(0, 600, 0) == 84 && at(0, 601, 0) == 88);   // passes through
		EXPECT(at(1, 678, 3) == 367 && at(1, 679, 0) == INT_MIN);
	}
	EXPECT(Y::subwayOpen(390, 390, 1439) && !Y::subwayOpen(389, 390, 1439) && !Y::subwayOpen(1439, 390, 1439));
	EXPECT(Y::subwayPattern(false, 5, 100) == 0 && Y::subwayPattern(true, 0, 100) == 1);
	EXPECT(Y::subwayPattern(true, 49, 100) == 2 && Y::subwayPattern(true, 50, 100) == 3);

	// Sky bridge (SkyBrdge.t2p AreaCheck FUN_1000bcc0) and fixed sites.
	EXPECT(Y::skyBridgeRule(3, true, true, 10, true) == Y::SkyBridgeError::TooLow);
	EXPECT(Y::skyBridgeRule(4, true, false, 10, true) == Y::SkyBridgeError::NotBetweenTowers);
	EXPECT(Y::skyBridgeRule(4, true, true, 73, true) == Y::SkyBridgeError::TooLong);
	EXPECT(Y::skyBridgeRule(4, true, true, 72, false) == Y::SkyBridgeError::NotClear);
	EXPECT(Y::skyBridgeRule(4, true, true, 72, true) == Y::SkyBridgeError::None);
	EXPECT(Y::fixedSiteRule(56, 55, 56, 55, Y::kErrBoatDeckSite) == 0 && Y::fixedSiteRule(57, 55, 56, 55, Y::kErrBoatDeckSite) == 74);
	EXPECT(std::string(Y::buildErrorText(Y::kErrObservatorySite)) == "The observatory can only be placed at left edge of the B32 floor.");

	// Clouds.
	for (int r = 0; r < 2000; r += 37) { const int f = Y::cloudFloor(95, r * 7919); EXPECT(f >= 1 && f <= 95); }
	EXPECT(Y::cloudFloor(10, 0) == 10 && Y::cloudFloor(10, 99) == 1);

	// The venues exist only in YootTower and replace the SimTower ones there.
	const BuildRules sim(BuildRules::Ruleset::SimTower);
	const BuildRules yoot(BuildRules::Ruleset::YootTower, tokyo);
	EXPECT(sim.minRatingToBuild("theater") == BuildRules::kUnavailable && sim.minRatingToBuild("subway") == BuildRules::kUnavailable);
	EXPECT(sim.minRatingToBuild("cinema") == LevelUp::minRatingToBuild("cinema"));
	EXPECT(yoot.minRatingToBuild("cinema") == BuildRules::kUnavailable && yoot.minRatingToBuild("metro") == BuildRules::kUnavailable);
	const std::map<std::string, YootUnlock> tu = resolveYootUnlocks(pm, 0x7);
	EXPECT(tu.count("subway") && tu.at("subway").available && tu.at("subway").minGrade == 2);
	EXPECT(tu.count("stadium") && tu.at("stadium").available && tu.at("stadium").minGrade == 4);
	EXPECT(tu.count("observatory") && !tu.at("observatory").available);
	EXPECT(tu.count("billboard") && tu.at("billboard").available && tu.at("billboard").minGrade == 1);

	// QuickTime sound track: a synthetic 'raw ' movie, and a shipped clip if present.
	std::string hdlr = qtBe32(0) + "mhlr" + "soun" + std::string(12, '\0');
	std::string entry = qtBe32(36) + "raw " + std::string(6, '\0') + std::string("\0\1", 2) +
		std::string(8, '\0') + std::string("\0\1\0\x08", 4) + std::string(4, '\0') + qtBe32(11025u << 16);
	std::string stsd = qtBe32(0) + qtBe32(1) + entry;
	std::string stsc = qtBe32(0) + qtBe32(1) + qtBe32(1) + qtBe32(4) + qtBe32(1);
	const std::string pcm("\x80\xff\x00\x80\x40\x40\x40\x40", 8);
	std::string moov;
	// mdat goes first so its offset is known: 8 (mdat header).
	std::string stco = qtBe32(0) + qtBe32(2) + qtBe32(8) + qtBe32(12);
	std::string stbl = qtAtom("stsd", stsd) + qtAtom("stsc", stsc) + qtAtom("stco", stco);
	moov = qtAtom("moov", qtAtom("trak", qtAtom("mdia", qtAtom("hdlr", hdlr) + qtAtom("minf", qtAtom("stbl", stbl)))));
	const std::string movie = qtAtom("mdat", pcm) + moov;
	YootMovieSound snd;
	EXPECT(decodeQuickTimeSound(movie.data(), movie.size(), snd));
	EXPECT(snd.channels == 1 && snd.sampleRate == 11025 && snd.samples.size() == 8u);
	EXPECT(snd.samples[0] == 0 && snd.samples[1] == 127 * 256 && snd.samples[2] == -128 * 256);
	EXPECT(!decodeQuickTimeSound(pcm.data(), pcm.size(), snd));
	YootMovieSound clip;
	if (loadQuickTimeSound("../data/Movie/HE_Tennis.MOV", clip))
		EXPECT(clip.channels == 1 && clip.sampleRate == 11025 && clip.samples.size() == 44100u);
}

static void testYootGapClosers()
{
	using namespace OT;
	// Grade-up messages (Tokyo.t2p 0x10015500, Kegon 0x100153d0, T2WorldDef 0x10072490).
	EXPECT(yootGradeUpMessage(YootWorld::Id::Tokyo, 2) == "Congratulations! Your building has been upgraded to a two star level.");
	EXPECT(yootGradeUpMessage(YootWorld::Id::Tokyo, 5) == "Congratulations! Your building has been upgraded to The Tower Grade, the highest level.");
	EXPECT(yootGradeUpMessage(YootWorld::Id::Kegon, 4) == "Congratulations! Your building has been upgraded to the \"Bamboo\" grade.");
	EXPECT(yootGradeUpMessage(YootWorld::Id::Kegon, 5) == "Congratulations! Your building has been upgraded to the \"Pine\" grade, the highest level.");
	EXPECT(yootGradeUpMessage(YootWorld::Id::Hawaii, 3) == "Your building has been upgraded to a 3 star level");
	// Calendar (T2WorldDef::DrawCalendar 0x10070a80).
	EXPECT(yootCalendarText(1, 0, false) == "YEAR 1 Jan. WD");
	EXPECT(yootCalendarText(3, 11, true) == "YEAR 3 Dec. HD");

	namespace Y = OT::Yoot;
	// School (School.t2p ChangeStatusOnIdle 0x1000bf30, Enter/LeaveTenant).
	{
		Y::SchoolModel s;
		int r = 0;
		auto rnd = [&r]() { return r++; };
		s.idle(0, 0, 480, 1080, false, true, rnd);
		EXPECT(s.status == Y::SchoolModel::Hiring);
		s.idle(8, 8, 480, 1080, false, true, rnd);
		EXPECT(s.status == Y::SchoolModel::Closed);
		s.idle(480, 480, 480, 1080, true, true, rnd);   // holiday: nobody called
		EXPECT(s.arrivals.empty());
		s.idle(480, 480, 480, 1080, false, true, rnd);
		EXPECT(s.arrivals.size() == 60u);
		s.arrive(480);                                    // rand % 6 == 0: 10 now
		EXPECT(s.inside == 10 && s.status == Y::SchoolModel::Some && s.pattern == 1);
		s.arrive(530);
		EXPECT(s.inside == 60 && s.status == Y::SchoolModel::Half && s.pattern == 3);
		s.idle(530, 512, 480, 1080, false, true, rnd);    // 08:30 class
		EXPECT(s.status == Y::SchoolModel::InClass && s.pattern == 2);
		s.idle(720, 720, 480, 1080, false, true, rnd);    // lunch
		EXPECT(s.status == Y::SchoolModel::Half && s.pattern == 3);
		s.idle(780, 780, 480, 1080, false, true, rnd);    // 13:00 class
		EXPECT(s.status == Y::SchoolModel::InClass);
		s.idle(900, 900, 480, 1080, false, true, rnd);    // dismissal
		EXPECT(s.status == Y::SchoolModel::Half);
		for (int t = 908; s.inside > 29; t += 8) s.idle(t, t, 480, 1080, false, true, rnd);
		EXPECT(s.inside == 27 && s.status == Y::SchoolModel::Some && s.pattern == 1); // 30 -> 29 flips it
		s.idle(1080, 1080, 480, 1080, false, true, rnd);  // 18:00 PushOutAll
		EXPECT(s.inside == 0 && s.status == Y::SchoolModel::Closed && s.pattern == 0);
	}
	// Sky Bridge AreaCheck (FUN_1000bcc0) and split (FUN_1000c0b0).
	{
		// Towers at units [40,60) and [90,110), rows 30..60; Tokyo-like area.
		auto towers = [](int row, int u) { return row >= 30 && row <= 60 && ((u >= 40 && u < 60) || (u >= 90 && u < 110)); };
		const Y::BuildRect area{36, 6, 332, 120};
		Y::BuildRect r{70, 39, 78, 40};
		EXPECT(Y::skyBridgeAreaCheck(r, area, 120, towers) == 0 && r.left == 60 && r.right == 90);
		Y::BuildRect low{70, 116, 78, 117};
		EXPECT(Y::skyBridgeAreaCheck(low, area, 120, towers) == 62);
		Y::BuildRect edge{20 + 16, 39, 44, 40};
		Y::BuildRect lone{120, 39, 128, 40};
		EXPECT(Y::skyBridgeAreaCheck(lone, area, 120, towers) == 63);
		(void)edge;
		auto blocked = [&](int row, int u) { return towers(row, u) || (row == 42 && u == 75); };
		Y::BuildRect r2{70, 39, 78, 40};
		EXPECT(Y::skyBridgeAreaCheck(r2, area, 120, blocked) == 59);
		auto wide = [](int row, int u) { return row >= 30 && row <= 60 && (u < 50 || u >= 123); };
		Y::BuildRect r3{70, 39, 78, 40};
		EXPECT(Y::skyBridgeAreaCheck(r3, area, 120, wide) == 60 && r3.right - r3.left == 73);
		auto ok72 = [](int row, int u) { return row >= 30 && row <= 60 && (u < 50 || u >= 122); };
		Y::BuildRect r4{70, 39, 78, 40};
		EXPECT(Y::skyBridgeAreaCheck(r4, area, 120, ok72) == 0);
		const auto runs = Y::skyBridgeSplit(60, 90, [](int u) { return u >= 70 && u < 75; });
		EXPECT(runs.size() == 2u && runs[0] == std::make_pair(60, 70) && runs[1] == std::make_pair(75, 90));
		EXPECT(Y::skyBridgeSplit(60, 90, [](int) { return true; }).empty());
	}
	// Tonbi / Dolphin flights (T2Tonbi::Exec, TonbiAnim::Fly).
	{
		const Y::TonbiDef kite = Y::builtinYootTonbi(false);
		EXPECT(kite.layer == 1 && kite.points.size() == 3u && kite.points[0] == std::make_pair(5, 123));
		EXPECT(kite.anims.size() == 14u && kite.minPict == 8600 && kite.maxPict == 8611);
		const Y::TonbiDef dol = Y::builtinYootTonbi(true);
		EXPECT(dol.layer == 2 && dol.points.size() == 1u && dol.anims.size() == 40u && dol.minPict == 8901);
		auto flight = [](const Y::TonbiDef & d, int & minX, int & maxX, int & ticks) {
			Y::TonbiFlight f;
			f.start(d);
			minX = 1 << 30; maxX = -(1 << 30); ticks = 0;
			bool seen = false;
			for (ticks = 1; ticks < 20000; ++ticks)
			{
				f.tick(600, 1020, true, []() { return 0; });
				if (f.visible()) { seen = true; minX = std::min(minX, f.x()); maxX = std::max(maxX, f.x()); }
				else if (seen) break;
			}
			return f.steps();
		};
		int lo, hi, t;
		EXPECT(flight(kite, lo, hi, t) == 206);
		EXPECT(lo == 989 && hi == 1267);
		EXPECT(flight(dol, lo, hi, t) == 260);
		EXPECT(lo == -59 && hi == 333);
		// Tries every 512 ticks; ends after a flight past endMin.
		Y::TonbiFlight f;
		f.start(kite);
		EXPECT(!f.tick(900, 1020, false, []() { return 0; })); // not ready: no flight, tick 1
		bool stopped = false;
		int k = 2;
		for (; k <= 2000 && !stopped; ++k) stopped = f.tick(1100, 1020, false, []() { return 0; });
		EXPECT(stopped && k - 1 == 513); // next try 512 ticks later, past 17:00
		YootEventSpec spec;
		spec.type = "TOEV";
		spec.body = {1, 1, 5, 123, 1, 8600, 2, -4, 5};
		Y::TonbiDef dd;
		EXPECT(Y::decodeYootTonbi(spec, dd) && dd.anims.size() == 1u && dd.points[0].second == 123);
	}
	// Mr. Maru, hot spring.
	EXPECT(Y::maruPhase(0) == 0 && Y::maruPhase(30) == 1 && Y::maruPhase(37) == 1 && Y::maruPhase(38) == 2 && Y::maruPhase(68) == 3);
	EXPECT(Y::hotSpringTriggers(false, 4, 3, 100, 100, -1));
	EXPECT(!Y::hotSpringTriggers(false, 4, 3, 99, 100, -1) && !Y::hotSpringTriggers(false, 4, 4, 200, 100, -1));
	EXPECT(!Y::hotSpringTriggers(false, 4, 3, 100, 100, 0) && !Y::hotSpringTriggers(true, 4, 3, 100, 100, -5));
	// Traffic map (T2TrafficInfo).
	{
		Y::TrafficMap m;
		m.configure(36, 6, 332, 120);
		EXPECT(m.cols() == 74 && m.rows() == 114);
		EXPECT(Y::TrafficMap::calcRange(24) == 0 && Y::TrafficMap::calcRange(25) == 1 && Y::TrafficMap::calcRange(125) == 2 && Y::TrafficMap::calcRange(250) == 3);
		m.pass(50, 100, 108);  // cells 16, 17, 18 of row 50
		EXPECT(m.cell(50, 100)->count == 1 && m.cell(50, 108)->count == 1 && m.cell(50, 112)->count == 0);
		m.pass(6, 36, 39);     // cell 0 never counts (GetUnit quirk)
		EXPECT(m.cell(6, 36) == nullptr);
		for (int h = 0; h < 200; ++h) { for (int k = 0; k < 10; ++k) m.pass(50, 100, 100); m.hourChanged(); }
		EXPECT(m.range(50, 100) == 3 && m.range(50, 112) == 0);
		const int acc = m.cell(50, 100)->accum;
		Y::TrafficMap m2;
		m2.configure(36, 6, 332, 120);
		m2.decode(m.encode());
		EXPECT(m2.cell(50, 100)->accum == acc && m2.range(50, 100) == 3);
	}
	EXPECT(Y::yearsSinceBuilt(30, 5) == 2 && Y::yearsSinceBuilt(5, 5) == 0 && Y::yearsSinceBuilt(5, -1) == 0);
	EXPECT(Y::floorCharge(500, 12) == 6000 && Y::floorCharge(500, 0) == 0);
}

// SimLess orders by creation serial in deterministic --sim runs and by plain
// pointer otherwise; erasing a tracked key (destruction deregistration) must
// leave a clean container (leak-review finding: dangling keys were compared
// against after the object was freed).
static void testSimLessContainers()
{
    struct Probe { unsigned long long simSerial; };
    Probe a, b, c;
    a.simSerial = 30; b.simSerial = 10; c.simSerial = 20;

    OT::Sim::setDeterministic(true);
    {
        std::set<Probe *, OT::SimLess> s;
        s.insert(&a); s.insert(&b); s.insert(&c);
        std::vector<Probe *> order(s.begin(), s.end());
        EXPECT(order.size() == 3 && order[0] == &b && order[1] == &c && order[2] == &a);
        // A destroyed tracked key deregisters: no stale entry remains.
        s.erase(&c);
        EXPECT(s.size() == 2 && s.find(&c) == s.end());
        EXPECT(s.insert(&c).second); // re-insertion of a live key works
    }
    OT::Sim::setDeterministic(false);
    {
        std::set<Probe *, OT::SimLess> s;
        s.insert(&a); s.insert(&b); s.insert(&c);
        std::set<Probe *, std::less<Probe *>> plain;
        plain.insert(&a); plain.insert(&b); plain.insert(&c);
        EXPECT(std::equal(s.begin(), s.end(), plain.begin())); // pointer order unchanged
    }
}

// The YootTower-only prototype gate that Factory::make(XML) enforces when a
// save from another ruleset contains YootTower items.
static void testYootOnlyPrototypeGate()
{
    EXPECT(OT::BuildRules::isYootOnlyPrototype("powerroom"));
    EXPECT(OT::BuildRules::isYootOnlyPrototype("stadium"));
    EXPECT(OT::BuildRules::isYootOnlyPrototype("billboard"));
    EXPECT(OT::BuildRules::isYootOnlyPrototype("hotel_single") == false);
    EXPECT(OT::BuildRules::isYootOnlyPrototype("office") == false);
    EXPECT(OT::BuildRules::isYootOnlyPrototype("floor") == false);
    EXPECT(OT::BuildRules::isYootOnlyPrototype("shop") == false);
    EXPECT(OT::BuildRules::isYootOnlyPrototype("elevator-standard") == false);
}

// YootTower hotel stays across save and reload (Hotel::encodeXML/decodeXML,
// yootStayStatus / yootStayParty). A room whose guests are in (status 32..34)
// saves its party with its occupied state; the reload restores the stay, so
// the room is still occupied, and an old save (no stay attributes) still
// checks the room out.
static void testYootHotelStaySaveReload()
{
    OT::Yoot::PeopleType a, b;
    a.demand = 3; a.timeZone = 2; a.silhouette = 17; a.econo = 1; a.life = 40; a.transport = 0;
    b = a; b.silhouette = 24; b.life = 0x7f;
    const std::vector<OT::Yoot::PeopleType> party = {a, b};

    // Save: the room element as Hotel::encodeXML writes it (roomState 1 = occupied).
    tinyxml2::XMLPrinter printer;
    printer.OpenElement("item");
    printer.PushAttribute("roomState", 1);
    printer.PushAttribute("yootStayStatus", 33);
    printer.PushAttribute("yootStayParty", OT::Yoot::encodeStayParty(party).c_str());
    printer.CloseElement();

    // Reload: decodeXML reads the attributes back and PeopleSystem restores them.
    tinyxml2::XMLDocument doc;
    EXPECT(doc.Parse(printer.CStr()) == tinyxml2::XML_SUCCESS);
    const tinyxml2::XMLElement * e = doc.FirstChildElement("item");
    EXPECT(e != nullptr);
    if (!e) return;
    EXPECT(e->IntAttribute("roomState", 0) == 1);
    std::vector<OT::Yoot::PeopleType> back;
    const int status = e->IntAttribute("yootStayStatus", 0);
    EXPECT(status == 33);
    EXPECT(OT::Yoot::decodeStay(status, e->Attribute("yootStayParty") ? e->Attribute("yootStayParty") : "", back));
    EXPECT(back.size() == 2 && back[0] == a && back[1] == b);
    EXPECT(back.size() == 2 && back[1].immortal());

    // Every stay state the room can be in, and a stay with nobody in it.
    std::vector<OT::Yoot::PeopleType> none;
    EXPECT(!OT::Yoot::decodeStay(31, OT::Yoot::encodeStayParty(party), none));  // queued at the desk
    EXPECT(!OT::Yoot::decodeStay(35, OT::Yoot::encodeStayParty(party), none));  // checked out, dirty
    EXPECT(!OT::Yoot::decodeStay(34 + 1, OT::Yoot::encodeStayParty(party), none));
    EXPECT(OT::Yoot::decodeStay(32, OT::Yoot::encodeStayParty(party), none) && none.size() == 2);
    EXPECT(!OT::Yoot::decodeStay(33, "", none));
    EXPECT(OT::Yoot::encodeStayParty({}).empty());

    // Old save: occupied room with no stay attributes is checked out on reload.
    tinyxml2::XMLPrinter old;
    old.OpenElement("item");
    old.PushAttribute("roomState", 1);
    old.CloseElement();
    tinyxml2::XMLDocument oldDoc;
    EXPECT(oldDoc.Parse(old.CStr()) == tinyxml2::XML_SUCCESS);
    const tinyxml2::XMLElement * oe = oldDoc.FirstChildElement("item");
    EXPECT(oe != nullptr);
    if (!oe) return;
    const int oldStatus = oe->IntAttribute("yootStayStatus", 0);
    EXPECT(oldStatus == 0);
    EXPECT(!OT::Yoot::decodeStay(oldStatus, oe->Attribute("yootStayParty") ? oe->Attribute("yootStayParty") : "", none));
}

// DIB validation: a height of INT32_MIN must be rejected before rows()
// negates it (signed overflow UB otherwise).
static void testOspDibValidation()
{
    auto put32 = [](std::string & d, size_t p, uint32_t v)
    {
        for (int i = 0; i < 4; ++i) d[p + i] = char(uint8_t(v >> (8 * i)));
    };
    auto put16 = [](std::string & d, size_t p, uint16_t v)
    {
        d[p] = char(uint8_t(v)); d[p + 1] = char(uint8_t(v >> 8));
    };
    auto header = [&](int height)
    {
        std::string d(40, '\0');
        put32(d, 0, 40);          // BITMAPINFOHEADER
        put32(d, 4, 16);          // width
        put32(d, 8, uint32_t(height));
        put16(d, 12, 1);          // planes
        put16(d, 14, 8);          // 8 bpp
        put32(d, 16, 0);          // BI_RGB
        put32(d, 32, 0);          // default palette (256)
        return d;
    };
    OT::DibLayout L;
    EXPECT(OT::parseDib(header(INT32_MIN), L) == false); // UB before the fix
    EXPECT(OT::parseDib(header(0), L) == false);         // zero height
    // Valid top-down 16x4 bitmap with its 256-entry palette + pixel data.
    std::string ok = header(-4);
    ok.resize(40 + 4 * 256 + 16 * 4);
    EXPECT(OT::parseDib(ok, L));
    EXPECT(L.rows() == 4 && L.width == 16 && L.bpp == 8 && L.colors == 256);
}

int main()
{
#ifdef OPENSKY_TRANSPORT_ONLY
    testElevatorDispatch();
    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All transport tests passed\n";
    return 0;
#elif defined(OPENSKY_DISASTER_SCENARIOS)
    return testDisasterScenarios();
#else
	testBldSyntheticRoundTrip();
	testBldPluginSaveData();
	testBldExtraRecordSizes();
	testBldShippedSaves();
	testTdtLoader();
	testTdtLoaderTruncated();
	testSmoke();
	testBuildRules();
	testSkin();
	testTimeConversion();
	testNightSpeedMultiplier();
	testRouteClear();
	testMoneyAccounting();
	testTimeWindowModernLayout();
	testNameManager();
	testLightingColorMath();
	testLevelUpRules();
	testLevelUpPromotionHelpers();
	testSaveHash();
	testWindowsPEExecutable();
	testWindowsSPExecutable();
	testPluginManager();
	testPluginBitmapLoader();
	testPluginResourcesByType();
	testDecompiledPlugin();
	testFourCC();
	testParseAttrBinary();
	testParseAttrText();
	testParseDesc();
	testDayParamCodec();
	testPlaceParamCodec();
	testMemberTableCodec();
	testPeopleAnimeCodec();
	testTemplateDefCodec();
	testListCodecs();
	testStringAndSubPluginCodecs();
	testPluginTypedAccessors();
	testPluginPopulationSweep();
	testItemTypeCatalog();
	testPluginObjmap();
	testObjmapRoundTrip();
	testOspRoundTrip();
	testOspRoundTripMover();
	testOspShippedOpenTower();
	testOspNumericRoundTrip();
	testOspCanonicalSheetNames();
	testRulesProfileSeam();
	testYootWorldLoader();
	testYootBackdropLayout();
	testYootSkyPalette();
	testYootPlacement();
	testYootCalendar();
	testYootUnlocks();
	testYootGrades();
	testYootEventsAndPower();
	testYootCatalog();
	testYootEconomy();
	testYootEvaluation();
	testYootStress();
	testYootFacilities();
	testYootTenants();
	testYootPool();
	testYootPlacementRules();
	testYootVenues();
	testYootSpecials();
	testYootItemArt();
	testYootGapClosers();
	testOspAttrCompat();
	testSimTowerEventClock();
	testBatch3SecomNeverUnlocks();
	testBatch5BRoaches();
	testBatch5BNeglectAndRescue();
	testBatch5BMaidPickAndVenueZones();
	testBatch5BRecyclingAndVip();
#ifdef OPENSKY_TRANSPORT_TEST_EXECUTABLE
    if (graphicsScenariosAvailable()) {
        if (std::system("\"" OPENSKY_TRANSPORT_TEST_EXECUTABLE "\"") != 0) {
            std::cerr << "FAIL: transport scenario process\n";
            ++g_failures;
        }
    } else {
        std::cout << "SKIP: Batch 4 C++ transport scenarios (no display or OPENSKY_HEADLESS=1)\n";
    }
#endif
		testYootWorldEncoders();
	testOspV3Sweep();
	testT2pExportEdited();
	testSimLessContainers();
	testYootOnlyPrototypeGate();
	testYootHotelStaySaveReload();
	testOspDibValidation();
    if (g_failures > 0)
    {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
#endif
}
