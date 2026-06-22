#pragma once

namespace OT {

struct TimeWindowLayout
{
    int clientWidth;
    int clientHeight;
    int watchX;
    int watchY;
    int watchSize;
    int ratingX;
    int ratingY;
    int ratingWidth;
    int ratingHeight;
    int dateX;
    int dateY;
    int tooltipX;
    int tooltipY;
    int tooltipWidth;
    int tooltipHeight;
    int messageX;
    int messageY;
    int messageWidth;
    int messageHeight;
    int metricsX;
    int metricsY;
    int metricsWidth;
    int metricRowHeight;
    int moneyStatsX;
    int moneyStatsY;
    int moneyStatsWidth;
    int backgroundTopBlue;
    int backgroundBottomBlue;
    int accentBlue;
    int tooltipBackgroundX;
    int tooltipBackgroundY;
    int tooltipBackgroundWidth;
    int tooltipBackgroundHeight;
    int messageBackgroundY;
    int messageBackgroundHeight;
};

inline TimeWindowLayout makeModernTimeWindowLayout(float uiScale)
{
    auto s = [uiScale](int value) { return static_cast<int>(value * uiScale); };
    return {
        s(520), s(80),
        s(10), s(11), s(34),
        s(58), s(6), s(108), s(18),
        s(184), s(8),
        s(58), s(33), s(298), s(14),
        s(58), s(55), s(298), s(14),
        s(370), s(7), s(132), s(26),
        s(370), s(55), s(132),
        36, 18, 214,
        s(52), s(30), s(310), s(22),
        s(52), s(22)
    };
}

}
