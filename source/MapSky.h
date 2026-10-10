/* Copyright (c) 2026 OpenSkyscraper contributors */
// Pure frame selection for the minimap sky strip (simtower/ui/map/sky, four
// 200x264 frames: 0 day, 1 dusk/dawn, 2 night, 3 overcast). Mirrors the phase
// windows of Sky::advance (night <05 / >=19, dawn 05-07, dusk 17-19, rain
// 07-17) so the overview goes dark exactly when the main sky does, independent
// of the Sky object's cached from/to state. The EXE builds frames 1..3 of the
// strip from bitmap 0x8160 with replacement palettes 0x83E9/0x83EA/0x83EB
// (SimTowerLoader), i.e. the same dusk/night/rain CLUT swaps as the main sky.
#pragma once

namespace OT {

struct MapSkyPhase
{
	int from;
	int to;
	double progress; // 0 = all 'from', 1 = all 'to'
};

inline MapSkyPhase mapSkyPhaseForHour(double hour, bool rainyDay)
{
	if (hour < 5 || hour >= 19) return {2, 2, 0};
	if (hour < 6)  return {2, 1, hour - 5};
	if (hour < 7)  return {1, 0, hour - 6};
	if (hour >= 17 && hour < 18) return {0, 1, hour - 17};
	if (hour >= 18) return {1, 2, hour - 18};
	if (!rainyDay) return {0, 0, 0};
	if (hour < 8)  return {0, 3, hour - 7};
	if (hour >= 16) return {3, 0, hour - 16};
	return {3, 3, 0};
}

/// Dominant frame (what a single-frame draw should show).
inline int mapSkyFrameForHour(double hour, bool rainyDay)
{
	const MapSkyPhase p = mapSkyPhaseForHour(hour, rainyDay);
	return p.progress >= 0.5 ? p.to : p.from;
}

} // namespace OT
