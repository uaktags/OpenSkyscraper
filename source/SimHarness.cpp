/* Copyright (c) 2026 OpenSkyscraper contributors */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "Game.h"
#include "Logger.h"
#include "SimHarness.h"

using namespace OT;

// The flag itself lives in SimFlag.cpp (linked into the unit tests too).

bool Sim::parseArg(int argc, char * argv[], int & i, Options & options)
{
	const char * arg = argv[i];
	if (strcmp(arg, "--sim") == 0) {
		if (i + 1 >= argc) { LOG(ERROR, "--sim is missing a length (ticks, or game days as <n>d)"); return true; }
		const std::string length = argv[++i];
		options.enabled = true;
		if (!length.empty() && length[length.size() - 1] == 'd')
			options.days = atof(length.c_str());
		else
			options.ticks = atol(length.c_str());
		return true;
	}
	if (strcmp(arg, "--sim-out") == 0) {
		if (i + 1 < argc) options.outPath = argv[++i];
		return true;
	}
	if (strcmp(arg, "--sim-summary") == 0) {
		if (i + 1 < argc) options.summaryPath = argv[++i];
		return true;
	}
	if (strcmp(arg, "--sim-seed") == 0) {
		if (i + 1 < argc) options.seed = (unsigned)strtoul(argv[++i], NULL, 10);
		return true;
	}
	if (strcmp(arg, "--sim-speed") == 0) {
		if (i + 1 < argc) options.speed = atoi(argv[++i]);
		return true;
	}
	if (strcmp(arg, "--sim-dump-every") == 0) {
		if (i + 2 < argc) {
			options.dumpEvery = atol(argv[++i]);
			options.dumpDir = argv[++i];
		}
		return true;
	}
	return false;
}

static std::string jsonEscape(const std::string & s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		const unsigned char c = (unsigned char)s[i];
		if (c == '"' || c == '\\') { out += '\\'; out += (char)c; }
		else if (c == '\n') out += "\\n";
		else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
		else out += (char)c;
	}
	return out;
}

static std::string num(double v)
{
	char b[64];
	snprintf(b, sizeof(b), "%.9g", v);
	return b;
}

std::string Sim::summaryJson(const Game & game, const Options & options, long ticksRun)
{
	std::ostringstream j;
	const RulesProfile & profile = game.activeRules.profile();
	j << "{\n";
	j << "  \"ruleset\": \"" << game.activeRules.identifier() << "\",\n";
	j << "  \"world\": \"" << jsonEscape(profile.worldId) << "\",\n";
	j << "  \"seed\": " << options.seed << ",\n";
	j << "  \"ticks\": " << ticksRun << ",\n";
	j << "  \"dt\": " << num(options.dt) << ",\n";
	j << "  \"speed\": " << options.speed << ",\n";
	j << "  \"time\": {\"absolute\": " << num(game.time.absolute)
	  << ", \"hour\": " << num(game.time.hour)
	  << ", \"day\": " << game.time.day
	  << ", \"quarter\": " << game.time.quarter
	  << ", \"year\": " << game.time.year << "},\n";
	j << "  \"funds\": " << game.funds << ",\n";
	j << "  \"rating\": " << game.rating << ",\n";
	j << "  \"population\": " << game.population << ",\n";
	j << "  \"people\": " << game.people.size() << ",\n";
	j << "  \"rainy\": " << (game.sky.rainyDay ? "true" : "false") << ",\n";
	j << "  \"money\": {\"todayIncome\": " << game.money.todayIncome
	  << ", \"todayExpenses\": " << game.money.todayExpenses
	  << ", \"quarterIncome\": " << game.money.quarterIncome
	  << ", \"quarterExpenses\": " << game.money.quarterExpenses << "},\n";
	j << "  \"items\": {";
	bool first = true;
	for (Game::ItemSetByString::const_iterator i = game.itemsByType.begin(); i != game.itemsByType.end(); ++i) {
		if (i->second.empty()) continue;
		j << (first ? "" : ", ") << "\"" << jsonEscape(i->first) << "\": " << i->second.size();
		first = false;
	}
	j << "},\n";
	j << "  \"events\": [";
	for (size_t i = 0; i < game.simEvents.size(); i++)
		j << (i ? ",\n    " : "\n    ") << "\"" << jsonEscape(game.simEvents[i]) << "\"";
	j << (game.simEvents.empty() ? "]\n" : "\n  ]\n");
	j << "}\n";
	return j.str();
}

static bool writeText(const std::string & path, const std::string & text)
{
	std::ofstream out(path.c_str(), std::ios::binary);
	if (!out) {
		LOG(ERROR, "sim: could not write %s", path.c_str());
		return false;
	}
	out << text;
	return (bool)out;
}

static bool snapshot(Game & game, const Sim::Options & options, const std::string & towerPath,
                     const std::string & summaryPath, long ticksRun)
{
	if (!game.writeTowerFile(towerPath)) return false;
	return writeText(summaryPath, Sim::summaryJson(game, options, ticksRun));
}

int Sim::run(Game & game, const Options & options)
{
	if (options.outPath.empty()) {
		LOG(ERROR, "sim: --sim-out <path.tower> is required");
		return 2;
	}
	if (options.dt <= 0 || (options.ticks <= 0 && options.days <= 0)) {
		LOG(ERROR, "sim: nothing to run (use --sim <ticks> or --sim <n>d)");
		return 2;
	}
	game.simRecording = true;
	game.setSpeedMode(options.speed);
	const double start = game.time.absolute;
	// Guard against a stalled clock (speed 0) in day mode.
	const long maxTicks = options.days > 0 ?
		(long)std::ceil(options.days / (options.dt * Time::kBaseSpeed)) + 1 : options.ticks;
	long tick = 0;
	while (tick < maxTicks) {
		if (options.days > 0 && game.time.absolute - start >= options.days) break;
		game.advanceHeadless(options.dt);
		tick++;
		if (options.dumpEvery > 0 && tick % options.dumpEvery == 0) {
			char name[64];
			snprintf(name, sizeof(name), "/tick_%09ld", tick);
			const std::string base = options.dumpDir + name;
			snapshot(game, options, base + ".tower", base + ".json", tick);
		}
	}
	if (options.days > 0 && game.time.absolute - start < options.days)
		LOG(WARNING, "sim: clock stalled; ran %ld ticks to %f", tick, game.time.absolute);
	const std::string summary = options.summaryPath.empty() ? options.outPath + ".json" : options.summaryPath;
	if (!snapshot(game, options, options.outPath, summary, tick))
		return 3;
	LOG(IMPORTANT, "sim: %ld ticks, t=%f -> %s", tick, game.time.absolute, options.outPath.c_str());
	return 0;
}

void Game::recordSimEvent(const std::string & message)
{
	if (!simRecording) return;
	char stamp[48];
	snprintf(stamp, sizeof(stamp), "%.6f ", time.absolute);
	simEvents.push_back(stamp + message);
}
