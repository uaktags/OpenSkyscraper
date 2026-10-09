/* Copyright (c) 2026 OpenSkyscraper contributors */
/// The deterministic-run flag lives in its own translation unit so the
/// unit-test binary (which exercises SimLess ordering without the full
/// SimHarness) can link it.
#include "SimHarness.h"

using namespace OT;

static bool s_deterministic = false;

bool Sim::deterministic()
{
	return s_deterministic;
}

void Sim::setDeterministic(bool on)
{
	s_deterministic = on;
}
