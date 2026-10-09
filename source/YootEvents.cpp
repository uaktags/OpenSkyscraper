/* Copyright (c) 2026 OpenSkyscraper Project */
#include "YootEvents.h"

#include "YootWorld.h"

namespace OT
{
	namespace
	{
		YootEventSpec spec(const char * type, int levelBit, int exclusive, int cycleDay,
		                   int beginMin, int endMin, std::vector<int> body = std::vector<int>(),
		                   const char * text = "", int resId = 128)
		{
			YootEventSpec e;
			e.type = type;
			e.resId = resId;
			e.levelBit = levelBit;
			e.exclusive = exclusive;
			e.cycleDay = cycleDay < 1 ? 1 : cycleDay;
			e.beginMin = beginMin;
			e.endMin = endMin;
			e.body = body;
			e.text = text;
			return e;
		}
	}

	std::vector<YootEventSpec> yootEventSpecs(const YootWorld & world)
	{
		std::vector<YootEventSpec> out;
		for (const YootEvent & w : world.events)
		{
			if (!w.found || w.resType == "VIEV") continue;
			YootEventSpec e = spec(w.resType.c_str(), w.header.levelBit, w.header.exclusive,
			                       w.header.cycleDay, w.header.beginMin, w.header.endMin,
			                       w.body, w.text.c_str(), w.resId);
			out.push_back(e);
		}
		return out;
	}

	std::vector<YootEventSpec> builtinYootEventSpecs(int world)
	{
		// world_rules.py --events on the shipped Tokyo/Hawai/Kegon.t2p
		// (~/yoottower-decomp/events-agent/events_*.txt). T2EV order kept.
		std::vector<YootEventSpec> v;
		const std::vector<int> fire = {3000, 4, 8100, 8101, 8102, 8103};
		const std::vector<int> blackout = {5000, 5000, 5000};
		switch (world)
		{
		case 0: // Tokyo
			v.push_back(spec("FBEV", 12, 1, 60, 720, 1080, fire));
			v.push_back(spec("TREV", 6, 1, 36, 540, 900, {3000, 720, 5000}));
			v.push_back(spec("MREV", 255, 0, 1, 0, 1440, {5000, 12, 410, 1, 490, 0, 570, 3, 650, 0,
				730, 2, 850, 1, 980, 2, 1060, 0, 1130, 1, 1190, 3, 1270, 0, 1420, 1}));
			v.push_back(spec("BOEV", 255, 1, 1, 660, 840, blackout));
			v.push_back(spec("SAEV", 255, 0, 12, 0, 1440, {32}));
			v.push_back(spec("CLEV", 255, 0, 1, 0, 1440, {4, 9001, 9002, 9003, 9004, 4,
				0, 32, 45, 342, 46, 32, 66, 308, 67, 120, 84, 244, 85, 192, 96, 235}));
			v.push_back(spec("ODEV", 255, 0, 1, 0, 1440, {8, 9102, 2, 9103, 2, 9104, 2, 9105, 2,
				9106, 2, 9107, 2, 9108, 2, 9109, 2}));
			v.push_back(spec("BBEV", 8, 1, 1, 1200, 1260, {12005, 12006, 12007, 1024, 11001, 10001, 50, 300}));
			v.push_back(spec("MAEV", 252, 0, 20, 360, 1380, {0, 223, 0, 9200}, "", 128));
			v.push_back(spec("MAEV", 252, 0, 25, 660, 1380, {9201, 224, 50, 9210},
				"'Uncle Reggae is noisy.'", 129));
			break;
		case 1: // Hawaii
			v.push_back(spec("FBEV", 12, 1, 60, 720, 1080, fire));
			v.push_back(spec("TREV", 4, 1, 36, 540, 900, {3000, 720, 5000}));
			v.push_back(spec("BOEV", 255, 1, 1, 660, 840, blackout));
			v.push_back(spec("SAEV", 255, 0, 12, 0, 1440, {32}));
			v.push_back(spec("DOEV", 255, 0, 1, 480, 1020)); // animation script: plugin only
			v.push_back(spec("ODEV", 255, 0, 1, 0, 1440, {7, 9101, 2, 9104, 2, 9105, 2, 9106, 2,
				9107, 2, 9108, 2, 9109, 2}));
			v.push_back(spec("WDEV", 8, 1, 1, 900, 1020));
			v.push_back(spec("CLEV", 255, 0, 1, 0, 1440, {4, 9001, 9002, 9003, 9004, 1, 0, 32, 42, 342}));
			break;
		case 2: // Kegon: no fire, terrorist, blackout or Santa.
			v.push_back(spec("TOEV", 255, 0, 1, 540, 1020)); // animation script: plugin only
			v.push_back(spec("HSEV", 4, 0, 1, 0, 1440, {100, 8, 1416515657 /* 'IPnT' */, 1413565257 /* 'IKAT' */}));
			break;
		}
		return v;
	}

	bool yootEventBeginDay(int cycleDay, int totalDays)
	{
		// IsBeginDay 0x10030280: param % cycleDay == 0, param = lapse + 1 =
		// totalDays - 11 (negative in year 1; C remainder 0 iff divisible).
		const int c = cycleDay < 1 ? 1 : cycleDay;
		return (totalDays - 11) % c == 0;
	}

	bool yootEventGradeMatches(int levelBit, int grade)
	{
		if (grade < 1 || grade > 31) return false;
		return (levelBit & (1 << (grade - 1))) != 0;
	}

	int yootEventBeginMinute(const YootEventSpec & e, int worldWidth)
	{
		if (e.type == "SAEV" && worldWidth > 0)
			return 1440 - worldWidth / 2;
		return e.beginMin;
	}

	std::vector<int> yootEventDays(const YootEventSpec & e, int grade, int firstDay, int lastDay)
	{
		std::vector<int> days;
		if (!yootEventGradeMatches(e.levelBit, grade)) return days;
		for (int d = firstDay; d <= lastDay; ++d)
			if (yootEventBeginDay(e.cycleDay, d)) days.push_back(d);
		return days;
	}

	// ------------------------------------------------------------ scheduler

	void YootEventScheduler::setEvents(const std::vector<YootEventSpec> & events, int worldWidth)
	{
		events_ = events;
		worldWidth_ = worldWidth;
		running_.assign(events_.size(), false);
		lastDay_ = -1;
		lastMinute_ = -1;
	}

	void YootEventScheduler::reset()
	{
		running_.assign(events_.size(), false);
		lastDay_ = -1;
		lastMinute_ = -1;
	}

	std::vector<int> YootEventScheduler::advance(int day, int minute, int grade)
	{
		std::vector<int> begins;
		if (day != lastDay_)
		{
			// The first sight of a day (or a load) starts the window at the
			// current minute, so a reload never replays the morning.
			lastMinute_ = lastDay_ < 0 ? minute - 1 : -1;
			lastDay_ = day;
		}
		if (minute <= lastMinute_) return begins;
		for (size_t i = 0; i < events_.size(); ++i)
		{
			const YootEventSpec & e = events_[i];
			if (running_[i]) continue;
			const int begin = yootEventBeginMinute(e, worldWidth_);
			if (begin <= lastMinute_ || begin > minute) continue;
			if (!yootEventGradeMatches(e.levelBit, grade)) continue;
			if (!yootEventBeginDay(e.cycleDay, day)) continue;
			begins.push_back((int)i);
		}
		lastMinute_ = minute;
		return begins;
	}

	void YootEventScheduler::setRunning(int index, bool running)
	{
		if (index >= 0 && index < (int)running_.size()) running_[index] = running;
	}

	bool YootEventScheduler::isRunning(int index) const
	{
		return index >= 0 && index < (int)running_.size() && running_[index];
	}

	int YootEventScheduler::find(const std::string & type) const
	{
		for (size_t i = 0; i < events_.size(); ++i)
			if (events_[i].type == type) return (int)i;
		return -1;
	}

	// --------------------------------------------------------------- bodies

	bool decodeYootFire(const YootEventSpec & e, YootFireDef & out)
	{
		const std::vector<int> & b = e.body;
		if (e.type != "FBEV" || b.size() < 2) return false;
		const int n = b[1];
		if (n < 0 || b.size() < size_t(2 + n)) return false;
		out.heliPrice = b[0];
		out.bitmaps.assign(b.begin() + 2, b.begin() + 2 + n);
		return true;
	}

	bool decodeYootTerrorist(const YootEventSpec & e, YootTerroristDef & out)
	{
		if (e.type != "TREV" || e.body.size() < 3) return false;
		out.ransom = e.body[0];
		out.deadlineMin = e.body[1];
		out.unk = e.body[2];
		return true;
	}

	bool decodeYootBlackout(const YootEventSpec & e, YootBlackoutDef & out)
	{
		if (e.type != "BOEV" || e.body.size() < 3) return false;
		out.defSupply = e.body[0];
		out.addSupply = e.body[1];
		out.transformerPrice = e.body[2];
		return true;
	}

	bool decodeYootSanta(const YootEventSpec & e, YootSantaDef & out)
	{
		if (e.type != "SAEV" || e.body.empty()) return false;
		out.divisor = e.body[0] == 0 ? 32 : e.body[0];
		return true;
	}

	bool decodeYootOutDecoration(const YootEventSpec & e, YootOutDecorationDef & out)
	{
		const std::vector<int> & b = e.body;
		if (e.type != "ODEV" || b.empty()) return false;
		const int n = b[0];
		if (n < 0 || b.size() < size_t(1 + 2 * n)) return false;
		out.items.clear();
		for (int k = 0; k < n; ++k)
			out.items.push_back(std::make_pair(b[1 + 2 * k], uint32_t(b[2 + 2 * k])));
		return true;
	}

	bool decodeYootReggae(const YootEventSpec & e, YootReggaeDef & out)
	{
		if (e.type != "MAEV" || e.body.size() < 4) return false;
		out.dialogId = e.body[0];
		out.viplId = e.body[1];
		out.tenantCategory = e.body[2];
		out.animId = e.body[3];
		out.claim = e.text;
		return true;
	}

	bool decodeYootMetro(const YootEventSpec & e, YootMetroDef & out)
	{
		const std::vector<int> & b = e.body;
		if (e.type != "MREV" || b.size() < 2) return false;
		const int n = b[1];
		if (n < 0 || b.size() < size_t(2 + 2 * n)) return false;
		out.unk = b[0];
		out.trains.clear();
		for (int k = 0; k < n; ++k)
			out.trains.push_back(std::make_pair(b[2 + 2 * k], b[3 + 2 * k]));
		return true;
	}

	bool decodeYootBaseball(const YootEventSpec & e, YootBaseballDef & out)
	{
		if (e.type != "BBEV" || e.body.size() < 3) return false;
		for (int k = 0; k < 3; ++k) out.dialogs[k] = e.body[k];
		out.params.assign(e.body.begin() + 3, e.body.end());
		return true;
	}

	bool decodeYootHotSpring(const YootEventSpec & e, YootHotSpringDef & out)
	{
		if (e.type != "HSEV" || e.body.size() < 4) return false;
		out.a = e.body[0];
		out.b = e.body[1];
		out.tenantA = uint32_t(e.body[2]);
		out.tenantB = uint32_t(e.body[3]);
		return true;
	}

	bool decodeYootCloud(const YootEventSpec & e, YootCloudDef & out)
	{
		const std::vector<int> & b = e.body;
		if (e.type != "CLEV" || b.empty()) return false;
		const int n = b[0];
		if (n < 0 || b.size() < size_t(2 + n)) return false;
		out.bitmaps.assign(b.begin() + 1, b.begin() + 1 + n);
		const int m = b[1 + n];
		if (m < 0 || b.size() < size_t(2 + n + 4 * m)) return false;
		out.zones.clear();
		for (int k = 0; k < m; ++k)
			out.zones.push_back(std::vector<int>(b.begin() + 2 + n + 4 * k, b.begin() + 6 + n + 4 * k));
		return true;
	}

	// ---------------------------------------------------------- power model

	const int YootPowerModel::kMaxTransformers;
	const int YootPowerModel::kWarnMargin;
	const int YootPowerModel::kStartStress;
	const int YootPowerModel::kEscapeStress;

	void YootPowerModel::reset()
	{
		supply_ = def.defSupply;
		consumption_ = 0;
		status_ = Normal;
		lastMinute_ = -1;
	}

	void YootPowerModel::setTransformers(int n)
	{
		if (n > kMaxTransformers) n = kMaxTransformers;
		supply_ = n > 0 ? def.defSupply + def.addSupply * n : def.defSupply;
	}

	YootPowerModel::Action YootPowerModel::start()
	{
		if (status_ != Normal) return None;
		if (supply_ < consumption_)
		{
			status_ = Dark;
			return Started;
		}
		return None;
	}

	YootPowerModel::Action YootPowerModel::minute(int now)
	{
		if (now == lastMinute_) return None;
		lastMinute_ = now;
		switch (status_)
		{
		case Dark:
			// Simulation ticks can skip the exact BOEV end minute.
			if (now >= endMin) status_ = EscapeDue;
			// Exec falls through to the case-3 recovery test.
			[[fallthrough]];
		case Escaped:
			if (consumption_ < supply_) status_ = Ending;
			return None;
		case EscapeDue:
			status_ = Escaped;
			return Escape;
		case Ending:
			status_ = Normal;
			return Ended;
		case Normal:
			break;
		}
		return None;
	}
}
