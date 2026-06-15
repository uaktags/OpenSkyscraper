/* Copyright (c) 2026 OpenSkyscraper Project */
/* Minimal smoke test for the CTest harness. No test framework dependency. */
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "../source/Time.h"
#include "../source/Route.h"
#include "../source/Money.h"
#include "../source/TimeWindowStyle.h"
#include "../source/WindowsPEExecutable.h"

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
    EXPECT(nightDelta > dayDelta * 1.5);

    TestTime at19;
    at19.set(Time::hourToAbsolute(19.0));
    const double at19Start = at19.absolute;
    at19.advance(dt);
    EXPECT(at19.absolute - at19Start > dayDelta * 1.5);

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
    EXPECT(layout.clientHeight == 56);
    EXPECT(layout.dateX > layout.ratingX + layout.ratingWidth);
    EXPECT(layout.metricsX > layout.dateX + 120);
    EXPECT(layout.tooltipWidth >= 300);
    EXPECT(layout.watchSize == 34);
    EXPECT(layout.backgroundTopBlue > layout.backgroundBottomBlue);
    EXPECT(layout.accentBlue > layout.backgroundTopBlue);
    EXPECT(layout.tooltipBackgroundX == 52);
    EXPECT(layout.tooltipBackgroundY == 28);
    EXPECT(layout.tooltipBackgroundWidth == 310);
    EXPECT(layout.tooltipBackgroundHeight == 22);
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

int main()
{
    testSmoke();
    testTimeConversion();
    testNightSpeedMultiplier();
    testRouteClear();
    testMoneyAccounting();
    testTimeWindowModernLayout();
    testWindowsPEExecutable();
    testWindowsSPExecutable();

    if (g_failures > 0)
    {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
