/* Copyright (c) 2026 OpenSkyscraper contributors */
// Main-menu backdrop choice. The title screen is the 640x480 ui/menubg sheet
// (first-party remaster when installed, otherwise the SIMTOWER.EXE bitmap
// 0x8100 loaded by SimTowerLoader as simtower/ui/menubg); the generic
// splashscreen is only the last resort.
#pragma once

namespace OT {

inline const char *menuBackgroundKey(unsigned menubgWidth, unsigned menubgHeight)
{
	return (menubgWidth == 640 && menubgHeight == 480) ? "simtower/ui/menubg" : "splashscreen";
}

} // namespace OT
