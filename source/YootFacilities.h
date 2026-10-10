/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower building facilities: trash, restrooms, colds and doctors
/// (game-parity.md I2, I3, I5, P5, P6, R6; plan step 12).
///
/// Verbatim ports of the T2DLL.dll / T2.exe / plugin functions named on each
/// declaration, kept pure so they can be unit-tested without a Game.
/// FacilitySystem feeds them from the tower; the items live in
/// Item/YootFacilities.*.
///
/// Grade-dependent rates (R6) are re-read by T2.exe FUN_00419460 on every
/// grade change and, for colds, at every day change (FUN_0041dc40):
///   doc+0x1e4 = T2WorldDef::IsDustOn(grade)          0x10072100
///   doc+0x1e8 = T2WorldDef::GetToiletDenomi(grade)   0x100720c0
///   doc+0x1ec = T2WorldDef::GetColdDenomi(grade, m+1) 0x100720e0
/// and the doc rolls them with rand() % denomi == 0 (FUN_00421e60 toilet,
/// FUN_004234f0 cold; T2TowerDoc vtable +0x154 / +0x158).
#pragma once

#include <array>
#include <vector>

namespace OT
{
	/// WORLDDEF +0x170..+0x178 and CODF id 1 (world-and-sp-formats.md §1, §2.8).
	struct YootFacilityRules
	{
		int dustOnGrade = 3;      ///< WORLDDEF +0x170 dustOnAt (0 = never)
		int toiletGrade = 1;      ///< WORLDDEF +0x174 toiletAt (0 = never)
		int toiletDenomi = 10;    ///< WORLDDEF +0x178
		bool hasColdTable = true; ///< CODF present (T2WorldDef +0x224 != 0)
		int coldMinGrade = 3;     ///< CODF first uint
		std::array<int, 12> coldMonthly{{20, 30, 30, 30, 30, 30, 30, 30, 30, 30, 20, 20}};
	};

	namespace Yoot
	{
		// ---- R6 grade-dependent rates ----
		/// T2WorldDef::IsDustOn 0x10072100: dustOnAt != 0 && dustOnAt <= grade.
		bool isDustOn(const YootFacilityRules & r, int grade);
		/// T2WorldDef::GetToiletDenomi 0x100720c0: toiletDenomi when
		/// toiletAt != 0 && toiletAt <= grade, else 0 (nobody needs one).
		int toiletDenomi(const YootFacilityRules & r, int grade);
		/// T2WorldDef::GetColdDenomi 0x100720e0 -> FUN_1001c470:
		/// monthly[month] when grade >= minGrade and month < 12, else 0.
		/// The doc passes the 0-based day index + 1, so the last day of the
		/// year (index 11 -> 12) has no colds.
		int coldDenomi(const YootFacilityRules & r, int grade, int monthPlusOne);
		/// FUN_00421e60 / FUN_004234f0: a non-zero denomi rolls
		/// `roll % denomi == 0` (roll = rand()).
		bool rollDenomi(int denomi, int roll);

		// ---- T2People special flags (+0x78) ----
		const int kPeopleNeedsToilet = 2; ///< set on entering the tower (T2People::Idle case 1)
		const int kPeopleSick = 4;        ///< Office.t2p FUN_1000b850

		// ---- I2 trash ----
		/// TrashFacility.t2p DefIdleProc FUN_1000bde0: "Building requires a
		/// Trash facility" while 0x199 < minute < 0x1a4 or 0x51d < minute <
		/// 0x528 (06:50-06:59, 21:50-21:59) and none is built. The tool def
		/// idles only once its plugin is loaded, i.e. from its mGameLevel on.
		bool trashWarningMinute(int minuteOfDay);
		const char * const kTrashWarning = "Building requires a Trash facility.";

		/// T2Tenant statuses and patterns used by TrashFacility.t2p.
		struct TrashFacilityModel
		{
			static const int kOpen = 0x1e;      ///< stage 0..4: 0x1e..0x22
			static const int kFull = 0x23;
			static const int kEmptying = 0x24;
			static const int kEmptyingPattern = 6;

			int capacity = 50;   ///< T2EquipDef +0x40 (TNDF capacity)
			int garbage = 0;     ///< T2Tenant +0xa8 (customers entered)
			int status = kOpen;  ///< T2Tenant +0x08
			int pattern = 0;     ///< T2Tenant +0x3c: 0 empty .. 5 full, 6 emptying

			/// EnterTenant FUN_1000bcc0 for one delivery: count it and step
			/// the fill pattern at 1, cap/4, 2 cap/4, 3 cap/4 and cap.
			/// Returns false when the facility does not accept garbage.
			bool deliver();
			/// T2Tenant::WillBeFull / emptying: no more customers.
			bool accepts() const { return status != kEmptying && garbage < capacity; }
			/// ChangeStatusOnIdle FUN_1000bba0 at minute-of-day `minute`:
			/// a filled facility starts emptying within [close, close+8)
			/// (07:00), and reopens empty within [open, open+8) (07:30).
			/// Returns true when the status changed.
			bool idle(int minute, int openMinute, int closeMinute);
		};

		// ---- P5 restrooms ----
		/// One registry-7 tenant as T2RegistedTenantDB::SearchToilet sees it.
		struct RestroomCandidate
		{
			bool reachable = false; ///< T2RouteNavigator::CheckRoute within search score
			bool open = true;       ///< 9 < status < 10000
			int users = 0;          ///< +0x98 reserved + +0xa0 inside
			int capacity = 5;       ///< +0x40
		};
		enum class ToiletResult { Found, NoneNearby = -1, Crowded = -2 };
		/// T2RegistedTenantDB::SearchToilet 0x10051f30: the first reachable,
		/// open restroom with room wins; a reachable full one only reports
		/// "crowded". `index` receives the winner (or -1).
		ToiletResult searchToilet(const std::vector<RestroomCandidate> & candidates, int & index);
		/// T2People::SearchToilet 0x100499c0 balloon on failure.
		const char * toiletFailureMessage(ToiletResult r);
		const int kToiletFailEstimate = 30; ///< IncEstimate(-0x1e)

		/// Restroom.t2p EnterTenant FUN_1000cd50 / LeaveTenant FUN_1000ce20:
		/// view pattern for `users` inside after a change (0 empty, 1 in
		/// use, 2 at half capacity and above). `entering` picks the rule.
		int restroomPattern(int previousPattern, int users, int capacity, bool entering);

		// ---- P6 colds and doctors ----
		/// One registry-4 tenant as Office.t2p FUN_1000ba80 sees it.
		struct DoctorCandidate
		{
			bool reachable = false;
			bool open = false;      ///< status > 0xd (staff in, open)
			int patients = 0;       ///< +0x98 reserved + +0xa0 inside
			int capacity = 20;
		};
		/// Office.t2p FUN_1000ba80: walks the doctors in route range until
		/// one has room. Returns the index (>= 0), -1 when every reachable
		/// open office is full, -2 when the reachable ones are closed and
		/// -3 when none is in range. (The plugin returns the tenant id, -1,
		/// -2 or 0; -3 stands for its 0 here.)
		int searchDoctor(const std::vector<DoctorCandidate> & candidates);
		/// Office.t2p FUN_1000b900: sick employees are sent to a doctor in
		/// 08:00..18:00 (WithinMinutes 0x1e0..0x438) at the top of each hour
		/// (minute % 60 < 8, once per tenant idle).
		bool doctorCallMinute(int minuteOfDay);
		/// Office.t2p FUN_1000b8b0: an employee still sick at midnight
		/// loses 50 estimate (IncEstimate(-0x32)) and recovers.
		const int kSickEstimate = 50;
		/// T2.exe string 1034 (T2InfoDialog comment 11).
		const char * const kNeedsDoctor = "Requires a Doctor's office nearby.";

		/// Doctor's.t2p EnterTenant FUN_1000bbb0 / LeaveTenant FUN_1000bc90:
		/// pattern 0 closed, 1 open and empty, 2 with patients, 3 from half
		/// capacity on.
		int doctorPattern(int previousPattern, int patients, int capacity, bool entering);

		/// T2.exe strings 1036-1038: restaurant (category 50) trash comments.
		enum class TrashComment { None, NoFacility, NotConnected, Full };
		const char * trashCommentText(TrashComment c);
	}
}
