/* Copyright (c) 2026 OpenSkyscraper Project */
/* Minimal smoke test for the CTest harness. No test framework dependency. */
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <algorithm>
#include <numeric>

#include "../source/Time.h"
#include "../source/Route.h"
#include "../source/Money.h"
#include "../source/NameManager.h"
#include "../source/TimeWindowStyle.h"
#include "../source/WindowsPEExecutable.h"
#include "../source/Lighting.h"
#include "../source/JudgeSystem.h"
#include "../source/LevelUp.h"
#include "../source/PluginManager.h"
#include "../source/PluginResources.h"
#include "../source/PluginBitmapLoader.h"

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

	// 1 star -> 2 stars needs 300 population.
	const LevelUp::Requirements * r1to2 = LevelUp::advancementRequirements(0);
	EXPECT(r1to2 != nullptr);
	EXPECT(r1to2->population == 300);

	// 2 stars -> 3 stars needs 1000 + security.
	const LevelUp::Requirements * r2to3 = LevelUp::advancementRequirements(1);
	EXPECT(r2to3 != nullptr);
	EXPECT(r2to3->population == 1000);
	EXPECT(r2to3->needsSecurity);

	// 5 stars is the max - no advancement from rating 4.
	const LevelUp::Requirements * rMax = LevelUp::advancementRequirements(4);
	EXPECT(rMax == nullptr);

	// meetsRequirements: pure helper, takes a Counts struct.
	JudgeSystem::Counts c = {};
	c.securityOffices = 0; c.medicalCenters = 0; c.metros = 0;

	EXPECT(LevelUp::meetsRequirements(*r1to2, 300, c, 0));  // pop only
	EXPECT(!LevelUp::meetsRequirements(*r1to2, 299, c, 0)); // pop short

	// 2 -> 3 stars needs security + VIP approval.
	EXPECT(!LevelUp::meetsRequirements(*r2to3, 1000, c, 0)); // missing security & VIP
	c.securityOffices = 1;
	EXPECT(!LevelUp::meetsRequirements(*r2to3, 1000, c, 0)); // missing VIP
	EXPECT(LevelUp::meetsRequirements(*r2to3, 1000, c, 1));  // security + VIP present

	// minRatingToBuild: locked items have explicit ratings, the rest 0.
	EXPECT(LevelUp::minRatingToBuild("cinema")    == 4);
	EXPECT(LevelUp::minRatingToBuild("metro")     == 4);
	EXPECT(LevelUp::minRatingToBuild("hotel_single") == 3);
	EXPECT(LevelUp::minRatingToBuild("security")  == 2);
	EXPECT(LevelUp::minRatingToBuild("fastfood")  == 1);
	EXPECT(LevelUp::minRatingToBuild("office")    == 0);
	EXPECT(LevelUp::minRatingToBuild("lobby")     == 0);
	EXPECT(LevelUp::minRatingToBuild("nonexistent_prototype") == 0);
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
    using OT::Plugin;
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
    Plugin * condo = pm.get("Condo.t2p");
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
        std::vector<Plugin::ResourceKey> res = condo->enumerateResources();
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
        Plugin * p = pm.get(name);
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
    using OT::Plugin;
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
    using OT::Plugin;
    using OT::PluginManager;

    PluginManager pm;
    pm.addPluginDirectory("../data/Plugins");

    Plugin * condo = pm.get("Condo.t2p");
    EXPECT(condo != nullptr);
    if (!condo) return;

    std::map<std::string, std::vector<int>> byType = condo->resourcesByType();

    // The flat view must contain exactly the same (type, id) pairs.
    std::vector<Plugin::ResourceKey> flat = condo->enumerateResources();
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
    Plugin broken("broken.t2p", "../data/Plugins/DoesNotExist.t2p");
    EXPECT(broken.resourcesByType().empty());
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

int main()
{
	testSmoke();
	testTimeConversion();
	testNightSpeedMultiplier();
	testRouteClear();
	testMoneyAccounting();
	testTimeWindowModernLayout();
	testNameManager();
	testLightingColorMath();
	testLevelUpRules();
	testWindowsPEExecutable();
	testWindowsSPExecutable();
	testPluginManager();
	testPluginBitmapLoader();
	testPluginResourcesByType();
	testFourCC();
	testParseAttrBinary();
	testParseAttrText();
	testParseDesc();
	testParseDpdf();
	testPluginTypedAccessors();

    if (g_failures > 0)
    {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
