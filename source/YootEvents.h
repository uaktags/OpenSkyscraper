/* Copyright (c) 2026 OpenSkyscraper Project */
///
/// YootTower scenario events (T2EV) and the power model, as pure logic.
///
/// The world plugin's T2EV lists `(type, resId)`; each `*EV` resource starts
/// with the common 5-int header read by `T2EventItem::T2EventItem`
/// (T2DLL 0x10030050). `T2TowerEvent::Idle` (0x10069970) moves an event from
/// standby to the wait list when the grade bit `1 << (grade-1)` is in its
/// levelBit and `IsBeginDay(CalcLapseDays(12) + 1)` holds, then starts it
/// when the clock reaches its begin minute. `CalcLapseDays(12)`
/// (0x1001cb10) is `CalcTotalDays() - 12`, so the begin-day rule is
/// `(totalDays - 11) % cycleDay == 0` with C (signed) remainder.
///
/// The power model is `T2BlackOut` (0x1001b580..0x1001ba90) plus the
/// PowerRoom.t2p transformer dialog (FUN_1000c2f0/c430/c4a0).
/// See doc/yoottower/game-parity.md rows V1-V7 and I1.
///
/// Pure module: no Game dependency, linked into OpenSkyTests.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace OT
{
	struct YootWorld;

	/// One T2EV entry reduced to what the scheduler and the event bodies need.
	struct YootEventSpec
	{
		std::string type;      ///< resource type, e.g. "FBEV"
		int resId = 0;
		int levelBit = 0;      ///< grade mask, bit g-1
		int exclusive = 0;     ///< T2EventItem::IsExclusive
		int cycleDay = 1;      ///< ctor clamps < 1 to 1
		int beginMin = 0;      ///< minute of day
		int endMin = 0;
		std::vector<int> body; ///< ints after the header
		std::string text;      ///< MAEV claim pstr
	};

	/// Converts a parsed world's T2EV list (VIEV excluded: VipSystem owns it).
	std::vector<YootEventSpec> yootEventSpecs(const YootWorld & world);
	/// Event list decoded from the shipped world plugins, for running without
	/// the .t2p. Long animation scripts (DOEV, TOEV, CLEV zones) are omitted.
	/// world: 0 = Tokyo, 1 = Hawaii, 2 = Kegon.
	std::vector<YootEventSpec> builtinYootEventSpecs(int world);

	/// T2EventItem::IsBeginDay(lapse + 1) as called from T2TowerEvent::Idle.
	bool yootEventBeginDay(int cycleDay, int totalDays);
	/// levelBit & (1 << (grade - 1)); grade is 1-based.
	bool yootEventGradeMatches(int levelBit, int grade);
	/// Begin minute after per-class overrides: T2SantaClaus ctor 0x100560b0
	/// sets beginTime = 1440 - worldWidth / 2.
	int yootEventBeginMinute(const YootEventSpec & e, int worldWidth);
	/// Days in [firstDay, lastDay] on which `e` begins at a fixed grade.
	std::vector<int> yootEventDays(const YootEventSpec & e, int grade, int firstDay, int lastDay);

	/// T2EV scheduler (T2TowerEvent::Idle, reduced): reports each event whose
	/// begin minute is crossed on a begin day at a matching grade, unless it is
	/// still running (an executing event is not in standby).
	class YootEventScheduler
	{
	public:
		void setEvents(const std::vector<YootEventSpec> & events, int worldWidth);
		const std::vector<YootEventSpec> & events() const { return events_; }
		/// Advance the clock to (day, minute); returns indices of events that
		/// begin in (previous minute, minute]. A new day restarts at minute -1.
		std::vector<int> advance(int day, int minute, int grade);
		void setRunning(int index, bool running);
		bool isRunning(int index) const;
		void reset();
		/// First event of a type, or -1.
		int find(const std::string & type) const;

	private:
		std::vector<YootEventSpec> events_;
		std::vector<bool> running_;
		int worldWidth_ = 0;
		int lastDay_ = -1;
		int lastMinute_ = -1;
	};

	// --- Event bodies (after the 5-int header) ---

	/// FBEV, T2FireBurning ctor 0x10030c10: helicopter price, n bitmap ids.
	struct YootFireDef { int heliPrice = 3000; std::vector<int> bitmaps; };
	/// TREV, T2Terrorist ctor 0x100638e0: ransom, deadline minute (Exec
	/// 0x10064570 compares the clock with +0x34), third int (5000).
	struct YootTerroristDef { int ransom = 3000; int deadlineMin = 720; int unk = 5000; };
	/// BOEV, T2BlackOut ctor 0x1001b580.
	struct YootBlackoutDef { int defSupply = 5000; int addSupply = 5000; int transformerPrice = 5000; };
	/// SAEV, T2SantaClaus ctor: vertical divisor (0 becomes 32).
	struct YootSantaDef { int divisor = 32; };
	/// ODEV, T2OutDecoration ctor 0x10045760: {imageId, flags} roof ornaments.
	struct YootOutDecorationDef { std::vector<std::pair<int, uint32_t>> items; };
	/// MAEV, T2Maru_Reggae ctor 0x1003a770. An empty claim selects Mr. Maru's
	/// harmless visit (MaruReception); a claim selects Uncle Reggae.
	struct YootReggaeDef
	{
		int dialogId = 0;        ///< +0x30: "caught" alert id
		int viplId = 0;          ///< VIPL resource (silhouette)
		int tenantCategory = 0;  ///< +0x38: ToolDef category filter (50 = Restaurants; 0 = any)
		int animId = 0;          ///< +0x3c
		std::string claim;
		bool isReggae() const { return !claim.empty(); }
	};
	/// MREV, FUN_1003d690 + T2Transport::Init: metro timetable.
	struct YootMetroDef { int unk = 5000; std::vector<std::pair<int, int>> trains; }; ///< {minute, track}
	/// BBEV, Tokyo.t2p T2Baseball 0x100118d0: three dialog ids, then ints.
	struct YootBaseballDef { int dialogs[3] = {0, 0, 0}; std::vector<int> params; };
	/// HSEV, Kegon.t2p T2HotSpring 0x10012940: two ints and two tenant 4ccs.
	struct YootHotSpringDef { int a = 0, b = 0; uint32_t tenantA = 0, tenantB = 0; };
	/// CLEV, Tokyo/Hawai T2Cloud: cloud bitmap ids and spawn zones (top, left, bottom, right).
	struct YootCloudDef { std::vector<int> bitmaps; std::vector<std::vector<int>> zones; };

	bool decodeYootFire(const YootEventSpec & e, YootFireDef & out);
	bool decodeYootTerrorist(const YootEventSpec & e, YootTerroristDef & out);
	bool decodeYootBlackout(const YootEventSpec & e, YootBlackoutDef & out);
	bool decodeYootSanta(const YootEventSpec & e, YootSantaDef & out);
	bool decodeYootOutDecoration(const YootEventSpec & e, YootOutDecorationDef & out);
	bool decodeYootReggae(const YootEventSpec & e, YootReggaeDef & out);
	bool decodeYootMetro(const YootEventSpec & e, YootMetroDef & out);
	bool decodeYootBaseball(const YootEventSpec & e, YootBaseballDef & out);
	bool decodeYootHotSpring(const YootEventSpec & e, YootHotSpringDef & out);
	bool decodeYootCloud(const YootEventSpec & e, YootCloudDef & out);

	/// T2BlackOut state, minus the drawing and the message plumbing.
	/// Units: power units for supply/demand, $100 for the transformer price.
	class YootPowerModel
	{
	public:
		enum Status { Normal = 0, Dark = 1, EscapeDue = 2, Escaped = 3, Ending = 4 };
		enum Action { None, Started, Escape, Ended };
		static const int kMaxTransformers = 5;   ///< PowerRoom FUN_1000c2f0
		static const int kWarnMargin = 50;       ///< AddConsumptionPower: supply - demand - 50 < 0
		static const int kStartStress = 30;      ///< Start: AddStress(0x1e)
		static const int kEscapeStress = 300;    ///< Exec case 2: AddStress(300)

		YootBlackoutDef def;
		int endMin = 840;

		void reset();
		/// AddSupplyPower(n) / SubSupplyPower: n = transformers switched on;
		/// 0 means no Power Room (supply falls back to defSupply).
		void setTransformers(int n);
		int supply() const { return supply_; }
		void setConsumption(int c) { consumption_ = c; }
		int consumption() const { return consumption_; }
		Status status() const { return status_; }
		bool isBlackOut() const { return status_ != Normal; }
		/// AddConsumptionPower's warning test.
		bool shortage() const { return supply_ - consumption_ - kWarnMargin < 0; }
		/// T2BlackOut::Start 0x1001b620: blackout when supply < consumption.
		Action start();
		/// T2BlackOut::Exec 0x1001b750, once per new minute.
		Action minute(int now);
		/// BreakerOn 0x1001b9f0 (all transformers switched back on).
		void breakerOn() { if (status_ != Normal) status_ = Ending; }
		void restore(int status) { status_ = status >= Normal && status <= Ending ? Status(status) : Normal; }

	private:
		int supply_ = 5000;
		int consumption_ = 0;
		Status status_ = Normal;
		int lastMinute_ = -1;
	};
}
