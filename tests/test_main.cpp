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

#include <climits>

#include "../source/Time.h"
#include "../source/Route.h"
#include "../source/Money.h"
#include "../source/NameManager.h"
#include "../source/TimeWindowStyle.h"
#include "../source/WindowsPEExecutable.h"
#include "../source/Lighting.h"
#include "../source/JudgeSystem.h"
#include "../source/LevelUp.h"
#include "../source/SaveHash.h"
#include "../source/PluginManager.h"
#include "../source/PluginResources.h"
#include "../source/PluginObjmap.h"
#include "../source/PluginBitmapLoader.h"
#include "../source/OspPlugin.h"
#include "../source/TdtLoader.h"

#include "../source/BuildRules.h"

static int g_failures = 0;

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
	EXPECT(!yootTower.hasFixedHorizontalBoundary());

	// SimTower's 375-cell lot is exactly [-187, 188).
	EXPECT(simTower.allowsHorizontalFootprint(-187, 375));
	EXPECT(simTower.allowsHorizontalFootprint(-187, 1));
	EXPECT(simTower.allowsHorizontalFootprint(187, 1));
	EXPECT(!simTower.allowsHorizontalFootprint(-188, 1));
	EXPECT(!simTower.allowsHorizontalFootprint(187, 2));

	// The other rulesets have no generic fixed horizontal boundary.
	EXPECT(openSky.allowsHorizontalFootprint(-500000, 1000000));
	EXPECT(yootTower.allowsHorizontalFootprint(-500000, 1000000));

	EXPECT(!simTower.allowsHorizontalFootprint(0, 0));
	EXPECT(!openSky.allowsHorizontalFootprint(0, -1));
	EXPECT(!yootTower.allowsHorizontalFootprint(INT_MAX, 1));
	EXPECT(!openSky.allowsHorizontalFootprint(INT_MAX, 1));
	EXPECT(!simTower.allowsHorizontalFootprint(1, INT_MAX));
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

	// Authentic SimTower progression (SimTower wiki, OpenSkyWeb 46372a2):
	// 1->2: 300 pop; 2->3: 1000 + security; 3->4: 5000 + medical + VIP;
	// 4->5: 10000 + metro.
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
	EXPECT(r3to4->needsSecurity);
	EXPECT(r3to4->needsMedical);
	EXPECT(r3to4->needsVip);

	const LevelUp::Requirements * r4to5 = LevelUp::advancementRequirements(3);
	EXPECT(r4to5 != nullptr);
	EXPECT(r4to5->population == 10000);
	EXPECT(r4to5->needsMetro);

	// 5 stars -> TOWER needs 15,000 population + facilities (the Cathedral
	// itself grants TOWER instantly on completion; see Cathedral::advance).
	const LevelUp::Requirements * rTower = LevelUp::advancementRequirements(4);
	EXPECT(rTower != nullptr);
	EXPECT(rTower->population == 15000);
	EXPECT(rTower->needsSecurity);
	EXPECT(rTower->needsMedical);
	EXPECT(rTower->needsMetro);

	// TOWER (rating 5) is the max - no advancement from rating 5.
	const LevelUp::Requirements * rMax = LevelUp::advancementRequirements(5);
	EXPECT(rMax == nullptr);

	// meetsRequirements: pure helper, takes a Counts struct.
	JudgeSystem::Counts c = {};
	c.securityOffices = 0; c.medicalCenters = 0; c.metros = 0;

	EXPECT(LevelUp::meetsRequirements(*r1to2, 300, c, 0));  // pop only
	EXPECT(!LevelUp::meetsRequirements(*r1to2, 299, c, 0)); // pop short

	// 2 -> 3 stars needs security only (no VIP since the authentic table).
	EXPECT(!LevelUp::meetsRequirements(*r2to3, 1000, c, 0)); // missing security
	c.securityOffices = 1;
	EXPECT(LevelUp::meetsRequirements(*r2to3, 1000, c, 0));  // security suffices

	// 3 -> 4 stars additionally needs medical + VIP approval.
	EXPECT(!LevelUp::meetsRequirements(*r3to4, 5000, c, 0)); // missing medical & VIP
	c.medicalCenters = 1;
	EXPECT(!LevelUp::meetsRequirements(*r3to4, 5000, c, 0)); // missing VIP
	EXPECT(LevelUp::meetsRequirements(*r3to4, 5000, c, 1));  // all present

	// minRatingToBuild: authentic toolbox mapping, the rest 0.
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
	EXPECT(LevelUp::minRatingToBuild("secom")            == 2);
	EXPECT(LevelUp::minRatingToBuild("cathedral")        == 4);
	EXPECT(LevelUp::minRatingToBuild("fastfood")         == 0);
	EXPECT(LevelUp::minRatingToBuild("office")           == 0);
	EXPECT(LevelUp::minRatingToBuild("lobby")            == 0);
	EXPECT(LevelUp::minRatingToBuild("nonexistent_prototype") == 0);
}

static void testLevelUpStarReward()
{
	// Authentic award gift wording (OpenSkyWeb ISSUE-034).
	EXPECT(OT::LevelUp::starRewardMessage(1).empty());
	EXPECT(OT::LevelUp::starRewardMessage(2) == "Your tower has been given a Two-Star Award!");
	EXPECT(OT::LevelUp::starRewardMessage(3) == "Your tower has been given a Three-Star Award!");
	EXPECT(OT::LevelUp::starRewardMessage(4) == "Your tower has been given a Four-Star Award!");
	EXPECT(OT::LevelUp::starRewardMessage(5) == "Your tower has been given a Five-Star Award!");
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
    EXPECT(attr.itemType == "CFFO");
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
    sf::Texture tex;
    EXPECT(PluginBitmapLoader::loadFromPlugin(*office, 1000, tex));
    const sf::Vector2u sz = tex.getSize();
    EXPECT(sz.x == 144);
    EXPECT(sz.y == 168);

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

// PluginResources seam: typed parsers for the HIGH-confidence resource
// types documented in doc/yoottower/resource-formats.md. Tests run at two
// levels:
//   (a) byte-level: hand-crafted buffers from the calibration hex dumps
//       (the proof that the layouts reproduce known game data).
//   (b) integration: Plugin::getAttr/getDesc/getDpdf on real vendored
//       plugins (the proof that PluginManager wires the parsers correctly).
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
    // lPnT as bytes == 0x546E506C, the literal ConstructProgramPlugin passes.
    FourCC lpnt; lpnt.c[0]='l'; lpnt.c[1]='P'; lpnt.c[2]='n'; lpnt.c[3]='T';
    EXPECT(lpnt.toUint32LE() == 0x546E506Cu);

    FourCC a, b;
    a.c[0]='A'; a.c[1]='B'; a.c[2]='C'; a.c[3]='D';
    b = a;
    EXPECT(a == b);
}

static void testParseAttrBinary()
{
    using OT::PluginAttr;
    // Exact bytes of Coin.t2p ATTR id=128 (49 B incl. trailing pad NUL):
    //   v=1 mItemType=ADEM f5=16 "****" gameLevel=3 mustItem=0 mID=1683
    //   name="Coin Press Machine"
    static const unsigned char kCoin[] = {
        0x01,
        'A','D','E','M',
        0x10,0x00,0x00,0x00,
        '*','*','*','*',
        0x03,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x93,0x06,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x12,
        'C','o','i','n',' ','P','r','e','s','s',' ','M','a','c','h','i','n','e',
        0x00  // trailing pad
    };
    PluginAttr a;
    const bool ok = OT::parsePluginAttr(reinterpret_cast<const char *>(kCoin),
                                        int(sizeof(kCoin)), a);
    EXPECT(ok);
    if (ok) {
        EXPECT(a.valid);
        EXPECT(!a.isTextForm);
        EXPECT(a.version == 1);
        EXPECT(a.itemType == "ADEM");
        EXPECT(a.marker == "****");
        EXPECT(a.gameLevel == 3u);      // Coin unlocks at Star 3
        EXPECT(a.mustItem == 0u);
        EXPECT(a.id == 1683u);
        EXPECT(a.name == "Coin Press Machine");
    }

    // The marker field is variable: "****" on most plugins, "NORF" on the
    // hotel-room family (Singleroom/Suiteroom/Twinroom/Maintenance). The
    // parser must accept either (discovered in the population sweep).
    static const unsigned char kNorf[] = {
        0x01, 'N','E','R','H',              // Apartment-style itemType
        0x02,0x00,0x00,0x00,
        'N','O','R','F',                    // hotel-room marker
        0x01,0x00,0x00,0x00, 0,0,0,0, 0x4e,0x04,0,0, 0,0,0,0,
        0x09, 'R','o','o','m',' ','t','e','s','t'
    };
    PluginAttr norf;
    const bool norfOk = OT::parsePluginAttr(reinterpret_cast<const char *>(kNorf),
                                            int(sizeof(kNorf)), norf);
    EXPECT(norfOk);
    if (norfOk) {
        EXPECT(norf.marker == "NORF");
        EXPECT(norf.name == "Room test");
    }

    // Truncated / null input is rejected cleanly (no crash).
    PluginAttr bad;
    EXPECT(!OT::parsePluginAttr(nullptr, 0, bad));
    EXPECT(!OT::parsePluginAttr(reinterpret_cast<const char *>(kCoin), 5, bad));
}

static void testParseAttrText()
{
    using OT::PluginAttr;
    // DefaultToolPalette's text-form ATTR. The integer mItemType decodes
    // back to the "LPOT" 4cc (the same value as its DESC classTag).
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
        EXPECT(a.id == 1000u);
        EXPECT(a.attrib == 16777215u);
        EXPECT(a.gameLevel == 1u);
        EXPECT(a.mustItem == 1u);
        EXPECT(a.itemType == "LPOT");              // 1414484044 == 0x544F504C
        EXPECT(a.name == "Default Tool palette");
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
        }
    };
    // lPnT (0x546E506C) -> TenantPlugin
    static const char kTenant[] = {0x01,'l','P','n','T', 3,0,0,0};
    check(kTenant, 9, "lPnT", PluginDesc::TenantPlugin);
    // lPvM (0x4D76506C) -> MoverPlugin
    static const char kMover[]  = {0x01,'l','P','v','M', 3,0,0,0};
    check(kMover, 9, "lPvM", PluginDesc::MoverPlugin);
    // LPOT (0x544F504C) -> ToolPalette
    static const char kPalette[]= {0x01,'L','P','O','T', 3,0,0,0};
    check(kPalette, 9, "LPOT", PluginDesc::ToolPalette);

    // Unknown tag still parses; kind stays Unknown (forward-compatible for
    // plugin kinds we haven't calibrated yet, e.g. T2EquipPlugin).
    static const char kUnk[] = {0x01,'l','P','e','Q', 3,0,0,0};
    PluginDesc u;
    EXPECT(OT::parsePluginDesc(kUnk, 9, u));
    EXPECT(u.kind == PluginDesc::Unknown);
    EXPECT(u.classTag == "lPeQ");

    // Truncation is rejected.
    PluginDesc bad;
    EXPECT(!OT::parsePluginDesc(nullptr, 0, bad));
    EXPECT(!OT::parsePluginDesc(kTenant, 5, bad));
}

static void testParseDpdf()
{
    using OT::PluginDpdf;
    // Coin.t2p / Burger.t2p DPDF id=128: 70, 50, 100, 80.
    static const char kCoin[] = {
        0x01,
        70,0,0,0, 50,0,0,0, 100,0,0,0, 80,0,0,0
    };
    PluginDpdf d;
    const bool ok = OT::parsePluginDpdf(kCoin, int(sizeof(kCoin)), d);
    EXPECT(ok);
    if (ok) {
        EXPECT(d.valid);
        EXPECT(d.version == 1);
        EXPECT(d.w == 70u);
        EXPECT(d.h == 50u);
        EXPECT(d.baseX == 100u);
        EXPECT(d.baseY == 80u);
    }
    // Truncation.
    PluginDpdf bad;
    EXPECT(!OT::parsePluginDpdf(nullptr, 0, bad));
    EXPECT(!OT::parsePluginDpdf(kCoin, 10, bad));
}

// PLDF price list. Calibration: resource-formats.md §3.10 — Burger.t2p
// PLDF id=128 decodes to [40, 1000, 10, 300, 50, 100], where 1000 is the
// construction cost. The on-disk resource is 25 bytes (version + 6 u32s).
static void testParsePldf()
{
    using OT::PluginPldf;
    // Exact bytes of Burger.t2p PLDF id=128 (25 B):
    //   v=1  f1=40  basePrice=1000  f2=10  f3=300  f4=50  f5=100
    static const unsigned char kBurger[] = {
        0x01,
        0x28,0x00,0x00,0x00,                       // 40
        0xE8,0x03,0x00,0x00,                       // 1000
        0x0A,0x00,0x00,0x00,                       // 10
        0x2C,0x01,0x00,0x00,                       // 300
        0x32,0x00,0x00,0x00,                       // 50
        0x64,0x00,0x00,0x00                        // 100
    };
    PluginPldf p;
    const bool ok = OT::parsePluginPldf(reinterpret_cast<const char *>(kBurger),
                                        int(sizeof(kBurger)), p);
    EXPECT(ok);
    if (ok) {
        EXPECT(p.valid);
        EXPECT(p.version == 1);
        EXPECT(p.f1 == 40u);
        EXPECT(p.basePrice == 1000u);
        EXPECT(p.f2 == 10u);
        EXPECT(p.f3 == 300u);
        EXPECT(p.f4 == 50u);
        EXPECT(p.f5 == 100u);
    }
    // Truncation: below the 21-byte minimum is rejected.
    PluginPldf bad;
    EXPECT(!OT::parsePluginPldf(nullptr, 0, bad));
    EXPECT(!OT::parsePluginPldf(reinterpret_cast<const char *>(kBurger), 20, bad));

    // Integration: Burger.t2p carries PLDF; Stair (transport) does not.
    OT::Plugin burger("Burger.t2p", "../data/Plugins/Burger.t2p");
    EXPECT(burger.ensureLoaded());
    PluginPldf bp;
    EXPECT(burger.getPldf(bp));
    if (bp.valid) {
        EXPECT(bp.basePrice == 1000u);
        EXPECT(bp.f1 == 40u);
    }
    OT::Plugin stair("Stair.t2p", "../data/Plugins/Stair.t2p");
    EXPECT(stair.ensureLoaded());
    PluginPldf sp;
    EXPECT(!stair.getPldf(sp));
}

// MVDF mover definition. The header is 18 bytes; the body carries an
// irregular tagged-pstr array whose exact framing is confirmed by T2DLL.dll
// disassembly (Ghidra-pending). We parse the header fields and extract the
// body strings robustly (printable-ASCII runs) so price/name/category survive.
// Calibration: resource-formats.md §3.9 + the known transport prices.
static void testParseMvdf()
{
    using OT::PluginMvdf;
    // Integration against the 7 vendored Mover plugins. Stair carries the
    // simplest price ("$5000"); StdElev carries the two-part elevator price
    // ("$150000:$50000/car") plus a quarterly ("$12000/Q").
    {
        OT::Plugin stair("Stair.t2p", "../data/Plugins/Stair.t2p");
        EXPECT(stair.ensureLoaded());
        PluginMvdf m;
        EXPECT(stair.getMvdf(m));
        if (m.valid) {
            EXPECT(m.version == 1);
            EXPECT(m.itemID == 1800u);             // == Stair's ATTR mID
            EXPECT(!m.priceString.empty());
            EXPECT(m.priceString == "$5000");      // Stair's construction cost
            EXPECT(m.name == "Stairs");
        }
    }
    {
        OT::Plugin elev("StdElev.t2p", "../data/Plugins/StdElev.t2p");
        EXPECT(elev.ensureLoaded());
        PluginMvdf m;
        EXPECT(elev.getMvdf(m));
        if (m.valid) {
            EXPECT(m.itemID == 1810u);             // == StdElev's ATTR mID
            EXPECT(m.priceString == "$150000:$50000/car");
            EXPECT(m.quarterlyString == "$12000/Q");
            EXPECT(m.name == "Simple Elevator");
        }
    }
    // Escalator: quarterly present ($5000/Q), no per-car price.
    {
        OT::Plugin esc("Escalator.t2p", "../data/Plugins/Escalator.t2p");
        EXPECT(esc.ensureLoaded());
        PluginMvdf m;
        EXPECT(esc.getMvdf(m));
        if (m.valid) {
            EXPECT(m.priceString == "$20000");
            EXPECT(m.quarterlyString == "$5000/Q");
            EXPECT(m.name == "Escalators");
        }
    }
    // A Tenant plugin has no MVDF (it uses PLDF instead).
    {
        OT::Plugin condo("Condo.t2p", "../data/Plugins/Condo.t2p");
        EXPECT(condo.ensureLoaded());
        PluginMvdf m;
        EXPECT(!condo.getMvdf(m));
    }
    // Truncation: below the 18-byte header is rejected.
    PluginMvdf bad;
    EXPECT(!OT::parsePluginMvdf(nullptr, 0, bad));
    EXPECT(!OT::parsePluginMvdf("short", 5, bad));
}

// Integration: the Plugin accessors reach into the real vendored plugins
// and reproduce the calibration values end-to-end.
static void testPluginTypedAccessors()
{
    using OT::Plugin;
    using OT::PluginDesc;
    using OT::PluginAttr;
    using OT::PluginDpdf;

    Plugin condo("Condo.t2p", "../data/Plugins/Condo.t2p");
    EXPECT(condo.ensureLoaded());

    PluginAttr a;
    EXPECT(condo.getAttr(a));
    if (a.valid) {
        EXPECT(a.itemType == "TRPA");
        EXPECT(a.id == 1101u);
        EXPECT(a.gameLevel == 1u);
        EXPECT(a.name == "Condo");
    }
    PluginDesc d;
    EXPECT(condo.getDesc(d));
    EXPECT(d.kind == PluginDesc::TenantPlugin);

    Plugin stair("Stair.t2p", "../data/Plugins/Stair.t2p");
    EXPECT(stair.ensureLoaded());
    PluginAttr sa;
    EXPECT(stair.getAttr(sa));
    if (sa.valid) {
        EXPECT(sa.itemType == "RATS");
        EXPECT(sa.id == 1800u);
        EXPECT(sa.name == "Stairs");
    }
    PluginDesc sd;
    EXPECT(stair.getDesc(sd));
    EXPECT(sd.kind == PluginDesc::MoverPlugin);

    // Coin carries DPDF; Stair does not (transport uses MVDF instead).
    Plugin coin("Coin.t2p", "../data/Plugins/Coin.t2p");
    EXPECT(coin.ensureLoaded());
    PluginDpdf dp;
    EXPECT(coin.getDpdf(dp));
    if (dp.valid) {
        EXPECT(dp.w == 70u);
        EXPECT(dp.h == 50u);
    }
    PluginDpdf nodp;
    EXPECT(!stair.getDpdf(nodp));

    // Text-form ATTR integration: DefaultToolPalette.
    Plugin pal("DefaultToolPalette.t2p", "../data/Plugins/DefaultToolPalette.t2p");
    EXPECT(pal.ensureLoaded());
    PluginAttr pa;
    EXPECT(pal.getAttr(pa));
    if (pa.valid) {
        EXPECT(pa.isTextForm);
        EXPECT(pa.itemType == "LPOT");
        EXPECT(pa.id == 1000u);
    }
    PluginDesc pd;
    EXPECT(pal.getDesc(pd));
    EXPECT(pd.kind == PluginDesc::ToolPalette);

    // Broken plugin: accessors return false, no crash.
    Plugin broken("broken.t2p", "../data/Plugins/DoesNotExist.t2p");
    PluginAttr ba; PluginDesc bb; PluginDpdf bc;
    EXPECT(!broken.getAttr(ba));
    EXPECT(!broken.getDesc(bb));
    EXPECT(!broken.getDpdf(bc));
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
    EXPECT(p.attr.itemType == "TRPA");
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
    EXPECT(reloaded.attr.itemType == "TRPA");
    EXPECT(reloaded.attr.name == "Condo");
    EXPECT(reloaded.attr.id == p.attr.id);
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
    EXPECT(p.attr.itemType == "RATS");
    EXPECT(p.attr.name == "Stairs");
    // MVDF transport pricing came through.
    EXPECT(p.transport.valid);
    EXPECT(p.transport.priceString == "$5000");
    EXPECT(p.transport.name == "Stairs");
    // Walking-frames sheet + OBJMAP 200 (the stair SHIL) are present.
    EXPECT(p.walking.present);
    EXPECT(!p.walking.blocks.empty());
    // Stairs have no PLDF (transport uses MVDF) and no occupants sheet.
    EXPECT(!p.price.valid);

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
    p.attr.itemType.c[0]='T'; p.attr.itemType.c[1]='E'; p.attr.itemType.c[2]='S'; p.attr.itemType.c[3]='T';
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
	put16(b, 2);           // level: 2 stars
	put32(b, 2000000000L); // balance: stored x100 -> $20,000,000
	putZeros(b, 12);       // otherIncome, constructionCosts, lastQuarterMoney
	put16(b, 0);           // frameTime: 0 = 7:00 AM in TDT terms
	put32(b, 5);           // currentDay
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
		else if (f == 12) putTenant(tenants, 60, 91, 18, 0); // cinema top half
		else if (f == 13) putTenant(tenants, 60, 91, 19, 0); // cinema bottom half
		else if (f == 20) putTenant(tenants, 50, 79, 31, 0); // metro part 1
		else if (f == 21) putTenant(tenants, 50, 79, 32, 0); // metro part 2
		else if (f == 22) putTenant(tenants, 50, 79, 33, 0); // metro part 3
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
	// The retail game sizes an express payload by STOPS, not span
	// (harness-measured): 3140 + 324*4 + 348 here. A wrong rule desyncs slot 2.
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
	putZeros(b, 3140 + 324 * 4 + 348);
	for (int s = 2; s < 24; s++) putZeros(b, 194); // empty slots: header only
	// Finance + parking blocks (not imported, walked past).
	putZeros(b, 132);
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
	EXPECT(tdt.level == 2);
	EXPECT(tdt.balance == 2000000000L);
	EXPECT(tdt.frameTime == 0 && tdt.currentDay == 5);
	EXPECT(tdt.elevatorsValid && tdt.stairsValid);
	EXPECT(tdt.elevators.size() == 2);
	EXPECT(tdt.elevators[0].type == 1 && tdt.elevators[0].cars == 2);
	EXPECT(tdt.elevators[0].bottomFloor == 10 && tdt.elevators[0].topFloor == 30);
	EXPECT(tdt.elevators[1].type == 0 && tdt.elevators[1].bottomFloor == 10 && tdt.elevators[1].topFloor == 100);
	EXPECT(tdt.stairs.size() == 3);

	std::vector<std::string> report;
	const std::string xml = OT::Tdt::toTowerXml(tdt, report);

	// Root attributes: funds /100, rating = level-1, time = day + (frame+300)/2600.
	EXPECT(xml.find("funds=\"20000000\"") != std::string::npos);
	EXPECT(xml.find("rating=\"1\"") != std::string::npos);
	EXPECT(xml.find("time=\"5.115385\"") != std::string::npos);
	EXPECT(xml.find("tool=\"inspector\"") != std::string::npos);

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

	// Fidelity report: construction site imported finished, shop + housekeeping skipped.
	bool sawConstruction = false, sawShop = false, sawHousekeeping = false;
	for (size_t i = 0; i < report.size(); i++)
	{
		if (report[i].find("under construction") != std::string::npos) sawConstruction = true;
		if (report[i].find("kind 10") != std::string::npos) sawShop = true;
		if (report[i].find("kind 15") != std::string::npos) sawHousekeeping = true;
	}
	EXPECT(sawConstruction && sawShop && sawHousekeeping);
	if (!(sawConstruction && sawShop && sawHousekeeping))
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


int main()
{
	testTdtLoader();
	testTdtLoaderTruncated();
	testSmoke();
	testBuildRules();
	testTimeConversion();
	testNightSpeedMultiplier();
	testRouteClear();
	testMoneyAccounting();
	testTimeWindowModernLayout();
	testNameManager();
	testLightingColorMath();
	testLevelUpRules();
	testLevelUpStarReward();
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
	testParseDpdf();
	testParsePldf();
	testParseMvdf();
	testPluginTypedAccessors();
	testPluginObjmap();
	testObjmapRoundTrip();
	testOspRoundTrip();
	testOspRoundTripMover();
	testOspShippedOpenTower();
	testOspNumericRoundTrip();
	testOspCanonicalSheetNames();
    if (g_failures > 0)
    {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
