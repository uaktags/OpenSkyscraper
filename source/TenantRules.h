#pragma once
// Pure, dependency-free tenant rules for batch 5 (J1..J12). Everything here
// is plain data in / plain data out so OpenSkyTests can compile it without
// SFML, Game or Item. EXE addresses are in /tmp/audit/batch5_asm.md.
#include <cstdint>
#include <cstdlib>

namespace OT
{
	namespace TenantRules
	{
		// EXE 1230:07cb (all.c:33438-33442): food/shop allocate 0x30
		// people, independently of rating; the door's separate cap is 40.
		constexpr int kRetailPoolSize = 48;
		// Hotel state 0x20 handler, 1228:2c8c-2cb2, recovered from the
		// jump table omitted at all.c:31996-32001: armed, period 4, rand%12.
		constexpr int kHotelCheckInOdds = 12;

		// ---- banked wait -------------------------------------------------
		struct Bank
		{
			int8_t  count = 0;
			int16_t total = 0;
		};

		/// Round half away from zero without <cmath>.
		inline int roundInt(double v) { return (int)(v < 0 ? v - 0.5 : v + 0.5); }

		/// One banking operation: count++ (int8 wrap), total += pending (int16 wrap).
		inline void bankAdd(Bank &b, double pending)
		{
			b.count = (int8_t)(uint8_t)((uint8_t)b.count + 1u);
			b.total = (int16_t)(uint16_t)((uint16_t)b.total + (uint16_t)roundInt(pending));
		}

		/// Signed total / signed count, truncated toward zero; 0 without trips.
		inline int bankedMean(int8_t count, int16_t total)
		{
			return count == 0 ? 0 : (int)total / (int)count;
		}
		inline int bankedMean(const Bank &b) { return bankedMean(b.count, b.total); }

		// ---- category / metric ------------------------------------------
		enum Category { kStressed = 0, kOk = 1, kHappy = 2, kUnjudged = 255 };

		/// Metric divisor by member capacity.
		enum Kind { kOffice, kCondo, kSingle, kTwin, kSuite };
		inline int divisorFor(Kind k)
		{
			switch (k) {
				case kOffice: return 6;
				case kCondo:  return 3;
				case kSingle: return 1;
				default:      return 2;
			}
		}
		inline int memberCount(Kind k) { return divisorFor(k); }

		/// Sum of members' individual means / divisor, truncated toward zero.
		inline int rawMetric(const Bank *banks, int members, int divisor)
		{
			int sum = 0;
			for (int i = 0; i < members; ++i) sum += bankedMean(banks[i]);
			return sum / divisor;
		}

		/// Class adjustment, noise add and clamp (EXE 1138:064b).
		inline int adjustMetric(int metric, int rentClass, bool noisy)
		{
			if (rentClass == 0) metric += 30;
			else if (rentClass == 2) metric -= 30;
			else if (rentClass == 3) metric = 0;
			if (noisy) metric += 60;
			return metric < 0 ? 0 : metric;
		}

		inline int bar1() { return 80; }
		/// `rating` is zero-based (stars-1): stars 1..3 -> 150, otherwise 200.
		inline int bar2(int rating) { return rating <= 2 ? 150 : 200; }

		/// Category for a metric; negative metric = unjudgeable.
		inline int categoryFor(int metric, int rating)
		{
			if (metric < 0) return kUnjudged;
			if (metric < bar1()) return kHappy;
			if (metric < bar2(rating)) return kOk;
			return kStressed;
		}

		/// Whole judge for a unit; `eligible=false` returns the unjudged category.
		inline int judgeCategory(const Bank *banks, Kind kind, int rentClass, bool noisy,
		                         bool eligible, int rating)
		{
			if (!eligible) return kUnjudged;
			const int metric = adjustMetric(
				rawMetric(banks, memberCount(kind), divisorFor(kind)), rentClass, noisy);
			return categoryFor(metric, rating);
		}

		/// Display value kept for the overlay/inspector.
		inline int evaluationFor(int category)
		{
			return category == kHappy ? 90 : category == kOk ? 60 :
			       category == kStressed ? 20 : 50;
		}

		// ---- noise -------------------------------------------------------
		enum SlotType { kSlotOther = 0, kSlotOffice, kSlotHotel, kSlotCondo,
		                kSlotRestaurant, kSlotShop, kSlotFastFood, kSlotCinema,
		                kSlotParty };
		struct Slot { int left; int right; int type; };

		inline bool isNoiseSource(int sourceType, int victimType)
		{
			switch (sourceType) {
				case kSlotRestaurant: case kSlotShop: case kSlotFastFood:
				case kSlotCinema: case kSlotParty:
					return victimType == kSlotOffice || victimType == kSlotHotel ||
					       victimType == kSlotCondo;
				case kSlotOffice:
					return victimType == kSlotHotel || victimType == kSlotCondo;
				case kSlotHotel:
					return victimType == kSlotCondo;
				default:
					return false;
			}
		}
		inline int noiseRange(int victimType)
		{
			return victimType == kSlotOffice ? 10 : victimType == kSlotHotel ? 20 :
			       victimType == kSlotCondo ? 30 : 0;
		}

		/// Index of the first noisy neighbour found by the EXE scan (left wins,
		/// one record beyond the cutoff is examined), or -1.
		inline int noiseSourceIndex(const Slot *slots, int n, int v)
		{
			if (!slots || v < 0 || v >= n) return -1;
			const int vt = slots[v].type;
			const int range = noiseRange(vt);
			if (range == 0) return -1;
			const int L = slots[v].left, R = slots[v].right;
			int s = v;
			while (s > 0 && slots[s].left >= L - range) {
				--s;
				if (isNoiseSource(slots[s].type, vt)) return s;
			}
			s = v;
			while (s < n - 1 && slots[s].right <= R + range) {
				++s;
				if (isNoiseSource(slots[s].type, vt)) return s;
			}
			return -1;
		}

		// ---- hotel passes (J2) --------------------------------------------
		enum { kRoomClean = 0, kRoomOccupied = 1, kRoomDirty = 2, kRoomInfested = 3 };
		struct Room
		{
			bool    hotel = false;
			int     type = 0;        ///< single/twin/suite discriminator
			int     state = 0;
			int     category = 255;
			bool    armed = false;
			int     tenure = 0;
			bool    redraw = false;
			bool    bankReset = false;
		};
		inline void stampInfested(Room &r)
		{
			r.state = kRoomInfested; r.category = kUnjudged; r.armed = false; r.redraw = true;
		}
		/// Roach scan over the flat record list of one floor (no snapshot).
		inline void roachSpread(Room *r, int n)
		{
			int i = 0;
			while (i < n) {
				if (!(r[i].hotel && r[i].state >= kRoomInfested)) { ++i; continue; }
				if (i > 0 && r[i - 1].hotel) stampInfested(r[i - 1]);
				if (i + 1 >= n || !r[i + 1].hotel) { i += 2; continue; }
				if (r[i + 1].state < kRoomInfested) { stampInfested(r[i + 1]); i += 2; continue; }
				++i;
			}
		}
		/// Neglect pass: every hotel with state >= dirty.
		inline void hotelNeglect(Room *r, int n)
		{
			for (int i = 0; i < n; ++i) {
				if (!r[i].hotel || r[i].state < kRoomDirty) continue;
				if (r[i].armed) { r[i].armed = false; r[i].category = kStressed; r[i].tenure = 0; }
				else r[i].tenure = (r[i].tenure + 1) & 0xff;
				if (r[i].tenure == 3) stampInfested(r[i]);
			}
		}
		/// Arm / rescue pass: every hotel with state < dirty.
		inline void hotelArmRescue(Room *r, int n)
		{
			for (int i = 0; i < n; ++i) {
				if (!r[i].hotel || r[i].state >= kRoomDirty) continue;
				const int8_t c = (int8_t)(uint8_t)r[i].category;
				if (c >= 1) { r[i].armed = true; r[i].bankReset = true; }
				else if (c == 0) {
					int j = -1;
					for (int k = 0; k < n; ++k)
						if (k != i && r[k].hotel && r[k].type == r[i].type && r[k].category == kHappy) { j = k; break; }
					if (j >= 0) {
						r[i].category = kOk; r[j].category = kOk;
						r[i].armed = true; r[i].bankReset = true;
					} else r[i].armed = false;
				}
			}
		}
		/// Maid pick: records sorted by (f, slot); first dirty hotel on an
		/// eligible floor (f % 6 == member), scanning anchor..top then anchor-1..0.
		inline int pickDirtyRoom(const int *floorF, const int *state, const bool *hotel,
		                         int n, int anchor, int member, int topF = 119)
		{
			for (int f = anchor; f <= topF; ++f) {
				if (f % 6 != member) continue;
				for (int i = 0; i < n; ++i)
					if (floorF[i] == f && hotel[i] && state[i] == kRoomDirty) return i;
			}
			for (int f = anchor - 1; f >= 0; --f) {
				if (f % 6 != member) continue;
				for (int i = 0; i < n; ++i)
					if (floorF[i] == f && hotel[i] && state[i] == kRoomDirty) return i;
			}
			return -1;
		}

		// ---- CountT (J3) ---------------------------------------------------
		enum { kCountOffice = 0, kCountSingle, kCountTwin, kCountSuite, kCountShop,
		       kCountFastFood, kCountRestaurant, kCountParty, kCountCinema, kCountCondo,
		       kCountCategories };
		struct CountT
		{
			int cat[kCountCategories] = {0};
			int population = 0;
			bool dirty = false;
			int resetDayPlus1[kCountCategories] = {0}; ///< day+1 of the last resetOnce
			/// Reset at most once per `day` (items sharing a venue type each
			/// call this at open; the first call wins, later ones add on top).
			bool resetOnce(int c, int day)
			{
				if (c < 0 || c >= kCountCategories || resetDayPlus1[c] == day + 1) return false;
				resetDayPlus1[c] = day + 1;
				reset(c);
				return true;
			}
			void add(int c, int delta)
			{
				if (c >= 0 && c < kCountCategories) cat[c] += delta;
				population += delta;
				dirty = true;
			}
			void reset(int c)
			{
				if (c < 0 || c >= kCountCategories) return;
				population -= cat[c];
				cat[c] = 0;
			}
		};

		// ---- recycling (J10) -----------------------------------------------
		inline int recyclingRequired(int ratio)
		{
			return ratio < 500 ? 1 : ratio < 1000 ? 2 : ratio < 1500 ? 3 :
			       ratio < 2000 ? 4 : ratio < 2500 ? 5 : 6;
		}

		// ---- VIP (J12) -----------------------------------------------------
		/// 1 = favorable, 2 = unfavorable. Zero trips average 0.
		inline int vipVerdict(int total, int trips)
		{
			const int avg = trips > 0 ? total / trips : 0;
			return avg <= 150 ? 1 : 2;
		}

		// ---- shop / food judge (J1) ------------------------------------------
		inline int shopCategory(bool let, int demand, int rentClass)
		{
			if (!let) return kUnjudged;
			static const int shift[4] = {5, 0, -5, -12};
			const int s = shift[rentClass < 0 ? 0 : rentClass > 3 ? 3 : rentClass];
			return demand >= 25 + s ? kHappy : demand >= 20 + s ? kOk : kStressed;
		}
		inline int foodCategory(int customers)
		{
			return customers >= 50 ? 3 : customers >= 35 ? 2 : customers >= 25 ? 1 : 0;
		}

		// ---- inspector strings (J11) -----------------------------------------
		inline const char *businessLine(int customers)
		{
			return customers >= 50 ? "Business is very good!" :
			       customers >= 35 ? "Business is good" :
			       customers >= 25 ? "Business is average" : "Very few customers";
		}
		inline const char *cinemaLine(int filmAge, int quota)
		{
			if (filmAge == 0) return "New movie showing!";
			return quota >= 60 ? "It's a Sellout!" : quota >= 40 ? "Ticket sales are average" :
			       "Terrible sales! Change the movie!";
		}
		/// 0 none, 1 "far", 2 "very far" for a walking distance in cells.
		inline int distanceTier(int distance) { return distance >= 125 ? 2 : distance >= 80 ? 1 : 0; }

		// ---- venue zones (J4/J7) ----------------------------------------------
		/// Registration zone, or -1 when the venue is not registered.
		inline int registrationZone(int floor)
		{
			const int v = floor + 5;
			const int z = v / 15, rem = v % 15;
			return (z >= 0 && z <= 6 && rem <= 9) ? z : -1;
		}
		inline int lookupZone(int floor)
		{
			const int z = (floor + 1) / 15;
			return z < 0 ? 0 : z;
		}
	}
}
