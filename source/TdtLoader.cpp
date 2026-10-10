/* Copyright (c) 2026 OpenSkyscraper Project */
#include "TdtLoader.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace OT {
namespace Tdt {

	// ---- Layout constants (doc/simtower/TDT_format.txt + verticopolis
	// ---- harness-verified measurements; see TdtLoader.h). ----
	static const int TDT_MAGIC = 0x2400;
	static const size_t HEADER_SIZE = 0x230; // 560
	static const int FLOOR_COUNT = 120;      // 0-9 = B10..B1, 10 = ground
	static const size_t TENANT_RECORD = 18;
	static const int FLOOR_INDEX_ENTRIES = 94;
	static const int MAX_TENANTS_PER_FLOOR = 256;
	static const int PERSON_RECORD = 16;
	static const int MAX_PEOPLE = 100000;
	static const int RETAIL_SLOTS = 512;
	static const int RETAIL_RECORD = 18;
	static const int ELEVATOR_SLOTS = 24;
	static const size_t ELEVATOR_HEADER = 194;
	static const size_t ELEVATOR_BUILT_FIXED = 3140;
	static const size_t ELEVATOR_PER_FLOOR = 324;
	static const size_t ELEVATOR_CAR_BLOCK = 348;
	static const int TDT_TYPE_EXPRESS = 0;
	static const size_t FINANCE_SIZE = 132;
	static const size_t PARKING_SIZE = 2 + 512 * 2;
	static const int STAIR_SLOTS = 64;
	static const size_t STAIR_RECORD = 10;
	static const size_t STAIR_SCAN_WINDOW = 4096;
	static const int MAX_FILE_BYTES = 8 * 1024 * 1024;

	// Coordinate conversion between TDT and our world grid.
	static const int X_ORIGIN = 187;  // our x = tdt tile - 187 (375-tile lot, centered)
	static const int Y_ORIGIN = 10;   // our y = tdt floor index - 10 (TDT 10 = ground)

	/// Bounds-checked little-endian byte reader. Accessors clamp by returning
	/// 0 past the end; every loop guards with canRead() so damage degrades to
	/// a warning instead of a crash.
	class Reader
	{
	public:
		Reader(const std::vector<uint8_t> & bytes) : bytes(bytes), pos(0) {}

		size_t offset() const { return pos; }
		size_t remaining() const { return bytes.size() - std::min(pos, bytes.size()); }
		bool canRead(size_t n) const { return remaining() >= n; }
		void skip(size_t n) { pos = std::min(pos + n, bytes.size()); }
		void seek(size_t offset) { pos = std::min(offset, bytes.size()); }

		uint8_t u8() { if (!canRead(1)) return 0; return bytes[pos++]; }
		int8_t i8() { return (int8_t)u8(); }
		uint16_t u16() { if (!canRead(2)) { pos = bytes.size(); return 0; } uint16_t v = (uint16_t)(bytes[pos] | (bytes[pos+1] << 8)); pos += 2; return v; }
		uint32_t u32() { if (!canRead(4)) { pos = bytes.size(); return 0; } uint32_t v = bytes[pos] | (bytes[pos+1] << 8) | (bytes[pos+2] << 16) | ((uint32_t)bytes[pos+3] << 24); pos += 4; return v; }
		int32_t i32() { return (int32_t)u32(); }

	private:
		const std::vector<uint8_t> & bytes;
		size_t pos;
	};

	// ---- Tenant id tables -------------------------------------------------
	// jcranmer's doc §5, with verticopolis's harness-corrected readings
	// (44 = parking ramp, 45 = metro tunnel; the doc's "45 = ramp" predates
	// that correction). Types absent from both tables are reported, skipped.

	static bool singleFloorKind(int type, const char ** id)
	{
		switch (type)
		{
			case 7:  *id = "office";        return true;
			case 9:  *id = "condo";         return true;
			case 3:  *id = "hotel_single";  return true;
			case 4:  *id = "hotel_double";  return true;
			case 5:  *id = "hotel_suite";   return true;
			case 6:  *id = "restaurant";    return true;
			case 12: *id = "fastfood";      return true;
			case 11: *id = "parking";       return true;
			case 13: *id = "medicalcenter"; return true;
			case 14: *id = "security";      return true;
			case 15: *id = "housekeeping";  return true; // 1180:0bbe bills row 15 ($10,000) per room
			case 10: *id = "shop";          return true;
			case 17: *id = "secom";         return true; // cut 1994 feature; closest match
			case 44: *id = "parkingramp";   return true; // parking ramp (not the metro tunnel; 45 stays skipped)
			default: return false;
		}
	}

	/// Multi-story units arrive as one part per floor; these form clusters.
	static bool partFamily(int type, const char ** id)
	{
		switch (type)
		{
			case 18: case 19: case 34: case 35: *id = "cinema";    return true;
			case 20: case 21:                   *id = "recycling"; return true;
			case 29: case 30:                   *id = "partyhall"; return true;
			case 31: case 32: case 33:          *id = "metro";     return true;
			case 36: case 37: case 38: case 39: case 40:
				*id = "cathedral"; return true;
			default: return false;
		}
	}

	// ---- File access -------------------------------------------------------

	static bool readFileBytes(const std::string & path, std::vector<uint8_t> & out, std::string & error)
	{
		FILE * f = fopen(path.c_str(), "rb");
		if (!f) { error = "could not open file"; return false; }
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (size < 0 || size > MAX_FILE_BYTES) { fclose(f); error = "file is not a plausible SimTower save (size)"; return false; }
		out.resize((size_t)size);
		size_t got = size > 0 ? fread(&out[0], 1, (size_t)size, f) : 0;
		fclose(f);
		if (got != (size_t)size) { error = "could not read file"; return false; }
		return true;
	}

	bool looksLikeTdt(const std::string & path)
	{
		std::vector<uint8_t> bytes;
		std::string error;
		if (!readFileBytes(path, bytes, error)) return false;
		if (bytes.size() < 2) return false;
		return (int)(bytes[0] | (bytes[1] << 8)) == TDT_MAGIC;
	}

	// ---- Floor map ---------------------------------------------------------

	static bool parseFloorMap(Reader & r, TdtData & out)
	{
		out.floors.resize(FLOOR_COUNT);
		for (int i = 0; i < FLOOR_COUNT; i++)
		{
			FloorRec & fr = out.floors[i];
			fr.index = i;
			if (!r.canRead(6)) return false;
			int tenantCount = r.u16();
			fr.leftEdge = r.u16();
			fr.rightEdge = r.u16();
			if (tenantCount > MAX_TENANTS_PER_FLOOR) return false;
			if (!r.canRead((size_t)tenantCount * TENANT_RECORD)) return false;
			fr.tenants.resize(tenantCount);
			for (int t = 0; t < tenantCount; t++)
			{
				Tenant & tn = fr.tenants[t];
				tn.left = r.u16();
				tn.right = r.u16();
				tn.type = r.i8();
				tn.status = r.u8();
				tn.variant = r.u8(); // byte 6: index into the retail table (variety is retail +11)
				r.skip(7);           // bytes 7-13: people offset, index in floor, reserved
				tn.armed = r.u8();   // byte 14 (+0x14)
				tn.category = r.u8();// byte 15 (+0x15)
				tn.rentClass = r.u8(); // byte 16
				tn.tenure = r.u8();  // byte 17
			}
			fr.indexMap.resize(FLOOR_INDEX_ENTRIES);
			for (int m = 0; m < FLOOR_INDEX_ENTRIES; m++) fr.indexMap[m] = r.u16(); // tile slot -> tenant index
		}
		return true;
	}

	// ---- Elevator table ----------------------------------------------------

	static int stopCount(const std::vector<uint8_t> &, int bottom, int top)
	{
		int stops = 0;
		for (int f = std::max(0, bottom); f <= std::min(FLOOR_COUNT - 1, top); f++)
			// 10a8:17ee: allocated express stop slots ignore the enabled-stop bitmap.
			if ((f >= 1 && f <= 10) || (f - 10) % 15 == 14) stops++;
		return stops;
	}

	static size_t payloadSize(bool expressByStops, int type, int bottom, int top, int stops)
	{
		size_t perFloor = (expressByStops && type == TDT_TYPE_EXPRESS)
			? (size_t)stops * ELEVATOR_PER_FLOOR
			: (size_t)(top - bottom + 1) * ELEVATOR_PER_FLOOR;
		return ELEVATOR_BUILT_FIXED + perFloor + ELEVATOR_CAR_BLOCK;
	}

	/// Walk the 24-slot elevator table with one payload layout. Returns false
	/// when the table doesn't parse (caller retries the other layout, then
	/// gives up with elevatorsValid = false). Layouts: the retail game sizes an
	/// express shaft's payload by the floors it STOPS at and every other kind
	/// by the floors it SPANS ("gameDefault", harness-measured by
	/// verticopolis); "spanned" sizes every kind by span and is tried second.
	static bool walkElevatorTable(Reader & r, bool gameDefaultLayout, std::vector<ElevatorRec> & out)
	{
		out.clear();
		for (int slot = 0; slot < ELEVATOR_SLOTS; slot++)
		{
			if (!r.canRead(ELEVATOR_HEADER)) return false;
			ElevatorRec e;
			int used = r.u8();
			e.type = r.u8();
			r.u8();              // byte 2: not a reliable kind signal (measured)
			e.cars = r.u8();
			r.skip(56);          // per-day-type car schedule (not imported)
			r.skip(2);           // visibility flag + reserved
			e.x = r.u16();
			e.topFloor = r.u8();
			e.bottomFloor = r.u8();
			e.serviced.resize(FLOOR_COUNT);
			for (int f = 0; f < FLOOR_COUNT; f++) e.serviced[f] = r.u8();
			e.carHomes.resize(8);
			for (int c = 0; c < 8; c++) e.carHomes[c] = r.u8();
			size_t payload = payloadSize(gameDefaultLayout, e.type, e.bottomFloor, e.topFloor, stopCount(e.serviced, e.bottomFloor, e.topFloor));
			if (used == 0) continue;
			if (used != 1 || e.type > 2 || e.cars < 1 || e.cars > 8 || e.topFloor < e.bottomFloor || e.topFloor >= FLOOR_COUNT)
				return false;
			if (!r.canRead(payload))
				return false;
			r.skip(payload);
			out.push_back(e);
		}
		return true;
	}

	// ---- Stairs table (signature scan) -------------------------------------

	static int classifyStairRecord(const std::vector<uint8_t> & b, size_t o)
	{
		if (o + STAIR_RECORD > b.size()) return -1;
		int built = b[o];
		if (built == 0) return 0;
		if (built != 1) return -1;
		int type = b[o + 1];
		int x = b[o + 2] | (b[o + 3] << 8);
		int floor = b[o + 4] | (b[o + 5] << 8);
		int crowdA = b[o + 6] | (b[o + 7] << 8);
		int crowdB = b[o + 8] | (b[o + 9] << 8);
		if (type > 5) return -1;
		if (x < 1 || x > 800) return -1;
		if (floor >= FLOOR_COUNT) return -1;
		if (crowdA > 4000 || crowdB > 4000) return -1;
		return 1;
	}

	static void locateStairs(const std::vector<uint8_t> & bytes, size_t from, std::vector<StairRec> & out)
	{
		out.clear();
		if (bytes.size() < STAIR_RECORD) return;
		size_t last = bytes.size() - STAIR_RECORD;
		if (last > from + STAIR_SCAN_WINDOW) last = from + STAIR_SCAN_WINDOW;
		size_t bestBase = (size_t)-1;
		int bestBuilt = 0;
		for (size_t base = from; base <= last; base++)
		{
			int built = 0;
			bool ok = true;
			for (int s = 0; s < STAIR_SLOTS; s++)
			{
				size_t o = base + (size_t)s * STAIR_RECORD;
				if (o + STAIR_RECORD > bytes.size()) break; // table ends at EOF
				int c = classifyStairRecord(bytes, o);
				if (c < 0) { ok = false; break; }
				built += c;
			}
			if (ok && built > bestBuilt) { bestBuilt = built; bestBase = base; }
		}
		if (bestBase == (size_t)-1) return; // no flights: the common stairless case
		for (int s = 0; s < STAIR_SLOTS; s++)
		{
			size_t o = bestBase + (size_t)s * STAIR_RECORD;
			if (o + STAIR_RECORD > bytes.size()) break;
			if (bytes[o] != 1) continue;
			StairRec sr;
			sr.type = bytes[o + 1];
			sr.x = bytes[o + 2] | (bytes[o + 3] << 8);
			sr.floor = bytes[o + 4] | (bytes[o + 5] << 8);
			out.push_back(sr);
		}
	}

	/// Reads the 512 x 18B retail table and attaches each live record to its
	/// tenant (record +0 = floor, +1 = index-map slot on that floor; see
	/// 11b0:0184 and 11b0:02f2, all.c:23492 and 23564).
	static void linkRetailTable(Reader & r, TdtData & out)
	{
		for (int i = 0; i < RETAIL_SLOTS; i++)
		{
			int floor = r.i8();
			int slot = r.u8();
			int state = r.u8();
			int s3 = r.u8(), s4 = r.u8(), s5 = r.u8();
			r.skip(1);
			int today = r.u8();
			int yesterday = r.u8();
			r.skip(2);
			int variety = r.u8();
			r.skip(RETAIL_RECORD - 12);
			if (floor < 0 || floor >= (int)out.floors.size() || slot == 0xff) continue;
			FloorRec & fr = out.floors[floor];
			if (slot >= (int)fr.indexMap.size()) continue;
			int ti = fr.indexMap[slot];
			if (ti < 0 || ti >= (int)fr.tenants.size()) continue;
			Tenant & tn = fr.tenants[ti];
			if (tn.type != 6 && tn.type != 10 && tn.type != 12) continue;
			tn.hasRetail = true;
			tn.retailState = state;
			tn.retailScore[0] = s3; tn.retailScore[1] = s4; tn.retailScore[2] = s5;
			tn.walkinsToday = today;
			tn.walkinsYesterday = yesterday;
			tn.retailVariety = variety;
		}
	}

	/// Cinema / party hall table: 16 x 12B just before the three trailing
	/// blocks of 10d8:0b8c (0x50+0x28, 0x1102, 0x842, 0xca2, then 8 bytes).
	static void readCinemaTable(const std::vector<uint8_t> & bytes, TdtData & out)
	{
		const size_t tail = 8 + 0xca2 + 0x842 + 0x1102 + 0x78;
		if (bytes.size() < tail + 16 * 12) return;
		size_t base = bytes.size() - tail - 16 * 12;
		for (int i = 0; i < 16; i++)
		{
			const uint8_t * rec = &bytes[base + (size_t)i * 12];
			if (rec[0] >= 0x80 || rec[0] >= 120) continue;
			CinemaRec c;
			c.floor = rec[0];
			c.movie = (int8_t)rec[7];
			c.age = (int8_t)rec[9];
			out.cinemas.push_back(c);
		}
	}

	// ---- Tolerant tail walk -------------------------------------------------

	static void walkTail(const std::vector<uint8_t> & bytes, Reader & r, TdtData & out)
	{
		// People: count + 16-byte records; the live crowd re-simulates, so skipped.
		if (!r.canRead(4)) { out.warnings.push_back("File ends right after the floor map; no transport data was present."); return; }
		int peopleCount = (int)r.u32();
		if (peopleCount > MAX_PEOPLE) { out.warnings.push_back("People table claims an impossible head count; the rest of the file was skipped."); return; }
		if (!r.canRead((size_t)peopleCount * PERSON_RECORD)) { out.warnings.push_back("People table runs past the end of the file; the rest of the file was skipped."); return; }
		r.skip((size_t)peopleCount * PERSON_RECORD);

		// Retail table: 512 x 18B; retail variety comes from the unit records instead.
		if (!r.canRead(RETAIL_SLOTS * RETAIL_RECORD)) { out.warnings.push_back("Retail table missing or cut short; transport data could not be reached."); return; }
		linkRetailTable(r, out);

		// Elevator table: 24 slots; payload layout disambiguated by retry.
		size_t elevatorStart = r.offset();
		if (walkElevatorTable(r, true, out.elevators) ||
			(r.seek(elevatorStart), walkElevatorTable(r, false, out.elevators)))
		{
			out.elevatorsValid = true;
		}
		else
		{
			out.elevatorsValid = false;
			out.warnings.push_back("The elevator table could not be read; the tower was imported without elevators. Build shafts so tenants can reach their destinations.");
			return;
		}

		// 10d8:0ac4 reads 0x58 bytes at c0ba, then 0x84 at c112.
		// The first eleven signed longs are ten CountT categories and the HUD total.
		size_t afterElevators = r.offset();
		if (r.canRead(0x58 + FINANCE_SIZE)) {
			r.skip(0x58);
			for (int c = 0; c < 10; ++c) out.countT[c] = r.i32();
			out.population = r.i32();
			out.populationValid = true;
			r.skip(FINANCE_SIZE - 11 * 4);
			if (r.canRead(PARKING_SIZE)) r.skip(PARKING_SIZE);
		}

		readCinemaTable(bytes, out);

		// Stairs: located by record signature (finance/parking sizes drift between saves).
		locateStairs(bytes, afterElevators, out.stairs);
		out.stairsValid = true;
	}

	// ---- Top-level parse -----------------------------------------------------

	bool loadFile(const std::string & path, TdtData & out, std::string & error)
	{
		std::vector<uint8_t> bytes;
		if (!readFileBytes(path, bytes, error)) return false;
		if (bytes.size() < 2 || (int)(bytes[0] | (bytes[1] << 8)) != TDT_MAGIC)
		{
			error = "not a SimTower .TDT save (missing 0x2400 version word)";
			return false;
		}
		if (bytes.size() < HEADER_SIZE + FLOOR_COUNT * (6 + FLOOR_INDEX_ENTRIES * 2))
		{
			error = "file too small to be a SimTower save";
			return false;
		}

		out = TdtData();
		Reader r(bytes);
		r.u16(); // magic re-checked above
		out.level = r.u16();
		out.balance = r.i32();
		r.skip(12); // otherIncome, constructionCosts, lastQuarterMoney (finance history not imported)
		out.frameTime = r.u16();
		out.currentDay = r.i32();
		out.retailRainyDay = r.u16() != 0; // 10d8:0ac4, all.c:12107: bc58
		out.lobbyHeight = r.u16();
		r.skip(0x2a - r.offset());
		out.viewX = r.u16();
		out.viewY = r.u16();
		r.skip(HEADER_SIZE - r.offset());

		if (!parseFloorMap(r, out)) { error = "floor map is corrupt or truncated"; return false; }
		walkTail(bytes, r, out);
		return true;
	}

	// ---- Conversion to native XML -------------------------------------------

	static int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

	struct PartRef
	{
		const char * family;
		int left, right, floor;
		int type;
	};

	std::string toTowerXml(const TdtData & in, std::vector<std::string> & report)
	{
		// EXE 1120:0143 displays signed long bc42 in $100 units.
		int64_t funds = (int64_t)in.balance * 100;
		int rating = clampInt(in.level - 1, 0, 5);
		// Time: TDT frames run 0..2599 with 0 = 7:00 AM and the day rolling at
		// frame 2300 (midnight). Our clock runs frame 0 = midnight, i.e. a
		// fixed +300 frame offset (same per-period rates; see Time.cpp).
		int day = std::max(0, in.currentDay);
		int frame = ((in.frameTime % 2600) + 2600) % 2600;
		int ourFrame = (frame + 300) % 2600;
		double absolute = (double)day + (double)ourFrame / 2600.0;

		// View: TDT stores the window's top-left in world px (8 px/tile,
		// 36 px/floor, origin top-left; the ground floor's bottom edge is
		// world row 3960). Convert to our poi (tile*8, floor*36).
		int poiX = 0, poiY = 0;
		if (in.viewX != 0 || in.viewY != 0)
		{
			int centerTile = (in.viewX + 320) / 8 - X_ORIGIN;
			int tdtFloor = 119 - (in.viewY + 234) / 36;
			poiX = centerTile * 8;
			poiY = (tdtFloor - Y_ORIGIN) * 36;
		}

		char head[512];
		snprintf(head, sizeof(head),
			"<tower funds=\"%lld\" rating=\"%d\" time=\"%.17g\" speed=\"1\" rainy=\"0\" tool=\"inspector\" x=\"%d\" y=\"%d\">",
			(long long)funds, rating, absolute, poiX, poiY);
		std::string xml = head;
		xml.pop_back();
		xml += in.retailRainyDay ? " retailRainy=\"1\">" : " retailRainy=\"0\">";
		if (in.lobbyHeight >= 1 && in.lobbyHeight <= 3) {
			xml.pop_back();
			snprintf(head, sizeof(head), " lobbyHeightLock=\"%d\">", in.lobbyHeight);
			xml += head;
		}
		if (in.populationValid) {
			xml.pop_back();
			char counts[256];
			snprintf(counts, sizeof(counts), " countT=\"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\" countPopulation=\"%d\">",
				in.countT[0], in.countT[1], in.countT[2], in.countT[3], in.countT[4],
				in.countT[5], in.countT[6], in.countT[7], in.countT[8], in.countT[9], in.population);
			xml += counts;
		}
		xml += "\n";

		std::vector<PartRef> parts;
		std::vector<std::pair<int,int> > skippedTypes; // (tdt type, count)
		int constructionSites = 0;
		std::vector<std::pair<int,int> > burnedRows; // (our floor, ...) paired with burnedSpan below
		std::vector<std::pair<int,int> > burnedSpan; // [left, right) in our x, parallel to burnedRows

		auto noteSkipped = [&](int type)
		{
			for (size_t i = 0; i < skippedTypes.size(); i++)
				if (skippedTypes[i].first == type) { skippedTypes[i].second++; return; }
			skippedTypes.push_back(std::make_pair(type, 1));
		};

		int emittedItems = 0;
		char buf[512];
		for (size_t fi = 0; fi < in.floors.size(); fi++)
		{
			const FloorRec & fr = in.floors[fi];
			int y = fr.index - Y_ORIGIN;
			for (size_t ti = 0; ti < fr.tenants.size(); ti++)
			{
				const Tenant & tn = fr.tenants[ti];
				int type = tn.type;
				bool underConstruction = false;
				if (type < 0) { underConstruction = true; type = -type; }

				if (type == 0) // floor slab (coalesced width)
				{
					snprintf(buf, sizeof(buf), "<item type=\"floor\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" width=\"%d\"/>",
						tn.left - X_ORIGIN, y, tn.right - tn.left);
					xml += buf; xml += "\n"; emittedItems++;
					continue;
				}
				if (type == 24) // lobby (ground or sky; height-1 slice per floor)
				{
					snprintf(buf, sizeof(buf), "<item type=\"lobby\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" width=\"%d\" height=\"1\"/>",
						tn.left - X_ORIGIN, y, tn.right - tn.left);
					xml += buf; xml += "\n"; emittedItems++;
					continue;
				}

				const char * id = NULL;
				if (singleFloorKind(type, &id))
				{
					if (underConstruction) constructionSites++;
					if (type == 12 || type == 6) // fastfood / restaurant carry a variety
					{
						int variant = clampInt(tn.hasRetail ? tn.retailVariety : tn.variant, 0, 4);
						int maxVariant = (type == 12) ? 4 : 3; // our Restaurant lacks the 5th (Steak House)
						if (variant > maxVariant)
						{
							variant = 0;
							report.push_back("A \"Steak House\" restaurant was imported as an English Pub (variant unavailable here).");
						}
						snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" variant=\"%d\"/>",
							id, tn.left - X_ORIGIN, y, variant);
					}
					else
					{
						snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>", id, tn.left - X_ORIGIN, y);
					}
					std::string room = buf;
					room.resize(room.size() - 2); // append state before />
					if (type == 7 || type == 9) {
						// EXE 1200:369d: office vacant band starts at 0x10, condo at 0x18.
						const bool occupied = !underConstruction && tn.status < (type == 7 ? 16 : 24);
						room += occupied ? " occupied=\"1\"" : " occupied=\"0\"";
					}
					if (type >= 3 && type <= 5) {
						const int hosted = !underConstruction && tn.status < 24 ? (type == 3 ? 1 : 2) : 0;
						const int state = hosted ? 1 : tn.status >= 56 ? 3 : tn.status >= 40 ? 2 : 0;
						snprintf(buf, sizeof(buf), " hosted=\"%d\" roomState=\"%d\"", hosted, state);
						room += buf;
					}
					if (!underConstruction && (type == 3 || type == 4 || type == 5 || type == 6 || type == 7 ||
					                           type == 9 || type == 10 || type == 12)) {
						// EXE tenant record bytes 14-17 (batch 5 T1): armed, judged
						// category, tenure and the rent-table row (4 = no rate).
						snprintf(buf, sizeof(buf), " armed=\"%d\" category=\"%d\" tenure=\"%d\"",
							tn.armed ? 1 : 0, tn.category, tn.tenure);
						room += buf;
						if (tn.rentClass >= 0 && tn.rentClass <= 3) {
							snprintf(buf, sizeof(buf), " rentClass=\"%d\"", tn.rentClass);
							room += buf;
						}
					}
					if (tn.hasRetail && !underConstruction) {
						// Retail table record (11b0:02f2): yesterday/today walk-ins feed
						// CountT at the first opening, the service scores set the quota.
						snprintf(buf, sizeof(buf), " walkinsToday=\"%d\" walkinsYesterday=\"%d\" serviceScore0=\"%d\" serviceScore1=\"%d\" serviceScore2=\"%d\"",
							tn.walkinsToday, tn.walkinsYesterday, tn.retailScore[0], tn.retailScore[1], tn.retailScore[2]);
						room += buf;
						if (type == 10) room += tn.retailState == 255 ? " let=\"0\"" : " let=\"1\"";
					}
					room += "/>";
					xml += room; xml += "\n"; emittedItems++;
					continue;
				}

				if (partFamily(type, &id))
				{
					PartRef pr;
					pr.family = id;
					pr.left = tn.left;
					pr.right = tn.right;
					pr.floor = fr.index;
					pr.type = type;
					parts.push_back(pr);
					if (underConstruction) constructionSites++;
					continue;
				}

				if (type == 45) { noteSkipped(type); continue; } // ground strips / metro tunnel
				if (type == 48) // burned area: becomes a burned span (rubble) the player bulldozes
				{
					burnedRows.push_back(std::make_pair(y, 0));
					burnedSpan.push_back(std::make_pair(tn.left - X_ORIGIN, tn.right - X_ORIGIN));
					continue;
				}
				noteSkipped(type);
			}
		}

		// Multi-story part clusters -> one item each (lowest floor, leftmost
		// extent). Parts of one family merge when floors are contiguous and
		// x-ranges overlap or sit flush (the cinema screen adjoins the hall).
		// Recycling centers are 25x2 units, one type-20 and one type-21 record
		// each; a tower can stack many of them (SCHMITT.TDT has nine). Pair them
		// by left edge and adjacent floor instead of merging the whole column,
		// so the quarterly upkeep (row 20, $50,000 per record, 1180:0bbe) and the
		// center count behind the recycling ratio (1090:0000) both match.
		{
			std::vector<PartRef> recycling, others;
			for (size_t i = 0; i < parts.size(); i++)
				(std::string(parts[i].family) == "recycling" ? recycling : others).push_back(parts[i]);
			std::vector<bool> used(recycling.size(), false);
			for (size_t i = 0; i < recycling.size(); i++)
			{
				if (used[i] || recycling[i].type != 20) continue;
				int mate = -1;
				for (size_t j = 0; j < recycling.size(); j++)
					if (!used[j] && j != i && recycling[j].type == 21 && recycling[j].left == recycling[i].left &&
					    std::abs(recycling[j].floor - recycling[i].floor) == 1) { mate = (int)j; break; }
				used[i] = true;
				int low = recycling[i].floor;
				if (mate >= 0) { used[mate] = true; low = std::min(low, recycling[mate].floor); }
				snprintf(buf, sizeof(buf), "<item type=\"recycling\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>",
					recycling[i].left - X_ORIGIN, low - Y_ORIGIN);
				xml += buf; xml += "\n"; emittedItems++;
			}
			for (size_t i = 0; i < recycling.size(); i++)
			{
				if (used[i]) continue; // a lone type-21 record still stands for a center
				snprintf(buf, sizeof(buf), "<item type=\"recycling\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>",
					recycling[i].left - X_ORIGIN, recycling[i].floor - Y_ORIGIN);
				xml += buf; xml += "\n"; emittedItems++;
			}
			parts.swap(others);
		}
		std::stable_sort(parts.begin(), parts.end(), [](const PartRef & a, const PartRef & b)
		{
			if (a.floor != b.floor) return a.floor < b.floor;
			return a.left < b.left;
		});
		for (size_t i = 0; i < parts.size(); i++)
		{
			PartRef cluster = parts[i];
			int maxFloor = cluster.floor;
			bool grew = true;
			while (grew)
			{
				grew = false;
				for (size_t j = i + 1; j < parts.size(); j++)
				{
					if (parts[j].family != cluster.family) continue;
					bool xNear = parts[j].left <= cluster.right && cluster.left <= parts[j].right;
					bool floorNear = parts[j].floor <= maxFloor + 1 && parts[j].floor >= cluster.floor;
					if (xNear && floorNear)
					{
						cluster.left = std::min(cluster.left, parts[j].left);
						cluster.right = std::max(cluster.right, parts[j].right);
						maxFloor = std::max(maxFloor, parts[j].floor);
						parts.erase(parts.begin() + j);
						grew = true;
						break;
					}
				}
			}
			if (std::string(cluster.family) == "cathedral")
				snprintf(buf, sizeof(buf), "<item type=\"cathedral\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" width=\"28\" height=\"5\"/>",
					cluster.left - X_ORIGIN, cluster.floor - Y_ORIGIN);
			else if (std::string(cluster.family) == "cinema")
			{
				// Cinema table record (1188:05af): film number and age in days.
				int movie = -1, age = 0;
				for (size_t c = 0; c < in.cinemas.size(); c++)
					if (in.cinemas[c].movie >= 0 && in.cinemas[c].floor >= cluster.floor && in.cinemas[c].floor <= maxFloor)
					{ movie = in.cinemas[c].movie; age = in.cinemas[c].age; break; }
				if (movie >= 0 && movie < 14)
					snprintf(buf, sizeof(buf), "<item type=\"cinema\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" movie=\"%d\" movieAgeDays=\"%d\"/>",
						cluster.left - X_ORIGIN, cluster.floor - Y_ORIGIN, movie, age);
				else
					snprintf(buf, sizeof(buf), "<item type=\"cinema\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>",
						cluster.left - X_ORIGIN, cluster.floor - Y_ORIGIN);
			}
			else
				snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>",
					cluster.family, cluster.left - X_ORIGIN, cluster.floor - Y_ORIGIN);
			xml += buf; xml += "\n"; emittedItems++;
		}

		// Transports.
		if (in.elevatorsValid)
		{
			static const char * elevatorIds[3] = { "elevator-express", "elevator-standard", "elevator-service" };
			for (size_t i = 0; i < in.elevators.size(); i++)
			{
				const ElevatorRec & e = in.elevators[i];
				int bottom = e.bottomFloor - Y_ORIGIN;
				int top = e.topFloor - Y_ORIGIN;
				snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\" height=\"%d\" showShaft=\"1\">",
					elevatorIds[e.type % 3], e.x - X_ORIGIN, bottom, e.topFloor - e.bottomFloor + 1);
				xml += buf;
				// Stop configuration: the save's serviced bitmap is "stops
				// here"; ours lists unserviced floors. Endpoints always stop.
				for (int f = e.bottomFloor + 1; f < e.topFloor; f++)
				{
					if (f >= 0 && f < FLOOR_COUNT && e.serviced[f] == 0)
					{
						char u[64];
						snprintf(u, sizeof(u), "<unserviced floor=\"%d\"/>", f - Y_ORIGIN);
						xml += u;
					}
				}
				int cars = clampInt(e.cars, 1, 8);
				for (int c = 0; c < cars; c++)
				{
					int home = clampInt(e.carHomes[c] - Y_ORIGIN, bottom, top);
					char car[96];
					snprintf(car, sizeof(car), "<car altitude=\"%d.0\" homeFloor=\"%d\"/>", home, home);
					xml += car;
				}
				xml += "</item>\n";
				emittedItems++;
			}
		}
		if (in.stairsValid)
		{
			for (size_t i = 0; i < in.stairs.size(); i++)
			{
				const StairRec & s = in.stairs[i];
				if (s.type < 0 || s.type > 5) continue;
				static const int stories[6] = { 1, 1, 2, 2, 3, 3 };
				const char * id = (s.type % 2 == 1) ? "stairs" : "escalator";
				for (int st = 0; st < stories[s.type]; st++)
				{
					snprintf(buf, sizeof(buf), "<item type=\"%s\" x=\"%d\" y=\"%d\" evaluation=\"50.0\"/>",
						id, s.x - X_ORIGIN, s.floor - Y_ORIGIN + st);
					xml += buf; xml += "\n"; emittedItems++;
				}
			}
		}

		// Burned areas -> EventSystem rubble tiles + recorded spans, stored as
		// root attributes (key = (floor + 1000) * 100000 + x + 1000, as
		// EventSystem::encodeTile).
		if (!burnedRows.empty())
		{
			std::string rubble, spans;
			for (size_t i = 0; i < burnedRows.size(); i++)
			{
				const int floor = burnedRows[i].first;
				const int left = burnedSpan[i].first, right = burnedSpan[i].second;
				if (right <= left) continue;
				snprintf(buf, sizeof(buf), "%d %d %d ", floor, left, right);
				spans += buf;
				for (int cx = left; cx < right; cx++)
				{
					snprintf(buf, sizeof(buf), "%ld ", (long)(floor + 1000) * 100000L + (cx + 1000));
					rubble += buf;
				}
			}
			const size_t gt = xml.find('>');
			if (gt != std::string::npos && !spans.empty())
				xml.insert(gt, " eventRubble=\"" + rubble + "\" eventBurnedSpans=\"" + spans + "\"");
			snprintf(buf, sizeof(buf), "%d burned area(s) were imported as rubble.", (int)burnedRows.size());
			report.push_back(buf);
		}

		// Aggregated fidelity report.
		if (constructionSites > 0)
		{
			snprintf(buf, sizeof(buf), "%d site(s) under construction were imported as finished.", constructionSites);
			report.push_back(buf);
		}
		for (size_t i = 0; i < skippedTypes.size(); i++)
		{
			snprintf(buf, sizeof(buf), "%d structure(s) of unimported kind %d were skipped.", skippedTypes[i].second, skippedTypes[i].first);
			report.push_back(buf);
		}

		xml += "</tower>\n";
		return xml;
	}

}
}
