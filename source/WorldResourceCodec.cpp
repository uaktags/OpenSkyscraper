/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "WorldResourceCodec.h"

#include <cctype>
#include <climits>
#include <map>

namespace OT
{
	namespace
	{
		// -----------------------------------------------------------------
		// Schema tables (world_sp.py SCHEMAS / EVENT_BODIES)
		// -----------------------------------------------------------------
		enum class K { I32, U32, Tag, Str, Rect, Point, Ints, Count, List, Strs, IList };

		struct F
		{
			const char * name;
			K kind;
			int n = 0;                       // Ints: element count
			const char * ref = nullptr;      // Count: the list it counts; List/Strs/IList: the count field
			const std::vector<F> * sub = nullptr;
		};
		using Schema = std::vector<F>;

		// T2WorldDef grade row (+0x88, 6 ints).
		const Schema kGrade = {
			{"vipFlag", K::U32},             // CheckGameLevel 0x10073290: (doc+0xa0 & vipFlag) == vipFlag
			{"population", K::I32},          // promotion needs population >= value
			{"gradeUpAnimID", K::I32},       // SetupGradeUpAnimation 0x10072e70
			{"gradeUpAnimFrames", K::I32},   // bitmap height / 96
			{"unk98", K::I32},               // always 0
			{"gradeUpAnimParam", K::I32},    // T2DlgItemAnimation::SetAnimationParam(100, v)
		};
		const Schema kWorldDef = {
			{"sortKey", K::I32},
			{"height", K::I32},
			{"width", K::I32},
			{"groundLine", K::I32},
			{"buildArea", K::Rect},
			{"entrance", K::Point},
			{"topFloorLine", K::I32},
			{"bottomFloorLine", K::I32},
			{"startGrade", K::I32},
			{"unk74", K::I32},
			{"lobbyHeight", K::I32},
			{"outViewKind", K::I32},
			{"unk17c", K::I32},
			{"tiledBkgnd", K::I32},
			{"numGrades", K::Count, 0, "grades"},
			{"grades", K::List, 0, "numGrades", &kGrade},
			{"bkgndTileHeight", K::I32},
			{"bkgndTileWidth", K::I32},
			{"unk188", K::I32},
			{"unk18c", K::I32},
			{"discarded", K::Ints, 10},
			{"gradeIconAnimIDs", K::Ints, 5},
			{"towerNamePrefix", K::Str},
			{"dustOnAt", K::U32},
			{"toiletAt", K::U32},
			{"toiletDenomi", K::U32},
			{"discarded2", K::I32},
			{"numCash", K::Count, 0, "cashAmounts"},
			{"cashAmounts", K::IList, 0, "numCash"},
		};
		const Schema kWatr = {
			{"unk", K::Ints, 6},
			{"initialFunds", K::I32},
			{"exchangeMul", K::I32},
			{"exchangeDiv", K::I32},
			{"moneyUnit", K::Str},
		};
		const Schema kDemand = {{"a", K::I32}, {"b", K::I32}};
		const Schema kPoolDef = {
			{"unk08", K::I32},
			{"gradePoolIDs", K::Ints, 5},
			{"unk20", K::I32},
			{"numDemand", K::Count, 0, "demand"},
			{"demand", K::List, 0, "numDemand", &kDemand},
		};
		const Schema kColdTable = {{"unk", K::U32}, {"monthly", K::Ints, 12}};
		const Schema kSeasonParam = {{"monthlyPercent", K::Ints, 12}};
		const Schema kTowerEvent = {{"type", K::Tag}, {"resID", K::I32}};
		const Schema kTowerEvents = {
			{"count", K::Count, 0, "events"},
			{"events", K::List, 0, "count", &kTowerEvent},
		};
		const Schema kVipPeople = {{"values", K::Ints, 6}};
		const Schema kEventHeader = {
			{"levelBit", K::U32},   // GetLevelBit: grades (bit g-1) the event may run on
			{"exclusive", K::I32},  // IsExclusive
			{"cycleDay", K::I32},   // IsBeginDay: day % cycleDay == 0
			{"beginTime", K::I32},  // minute of day
			{"endTime", K::I32},
		};

		const Schema kOdItem = {{"imageID", K::I32}, {"value", K::U32}};
		const Schema kVip = {
			{"grade", K::I32}, {"tenantType", K::Tag}, {"unk38", K::I32}, {"unk3c", K::I32},
			{"unk40", K::I32}, {"unk44", K::I32}, {"viplID", K::I32}, {"comment", K::Str},
			{"comment2", K::Str},
		};
		const Schema kZone = {{"rect", K::Rect}};
		const std::map<std::string, Schema> & eventBodies()
		{
			static const std::map<std::string, Schema> m = {
				{"BOEV", {{"unk30", K::I32}, {"unk34", K::I32}, {"unk38", K::I32}}},
				{"TREV", {{"unk30", K::I32}, {"unk34", K::I32}, {"unk38", K::I32}}},
				{"SAEV", {{"unk30", K::I32}}},
				{"FBEV", {{"unk38", K::I32}, {"count", K::Count, 0, "values"}, {"values", K::IList, 0, "count"}}},
				{"ODEV", {{"count", K::Count, 0, "items"}, {"items", K::List, 0, "count", &kOdItem}}},
				{"VIEV", {{"count", K::Count, 0, "vips"}, {"vips", K::List, 0, "count", &kVip}}},
				{"BBEV", {{"unk58", K::I32}, {"unk5a", K::I32}, {"unk5c", K::I32}, {"unk5e", K::I32},
				          {"unk60", K::I32}, {"unk62", K::I32}, {"unk64", K::I32}, {"unk68", K::I32}}},
				{"CLEV", {{"numImages", K::Count, 0, "imageIDs"}, {"imageIDs", K::IList, 0, "numImages"},
				          {"numZones", K::Count, 0, "zones"}, {"zones", K::List, 0, "numZones", &kZone}}},
				{"HSEV", {{"unk68", K::I32}, {"unk6c", K::I32}, {"tenantType1", K::Tag}, {"tenantType2", K::Tag}}},
				{"WDEV", {}},
				{"MAEV", {{"unk30", K::I32}, {"viplID", K::I32}, {"unk38", K::I32}, {"unk3c", K::I32},
				          {"message", K::Str}}},
			};
			return m;
		}

		const Schema * schemaFor(WorldSchema s)
		{
			switch (s)
			{
				case WorldSchema::WorldDef:    return &kWorldDef;
				case WorldSchema::Watr:        return &kWatr;
				case WorldSchema::PoolDef:     return &kPoolDef;
				case WorldSchema::ColdTable:   return &kColdTable;
				case WorldSchema::SeasonParam: return &kSeasonParam;
				case WorldSchema::TowerEvents: return &kTowerEvents;
				case WorldSchema::VipPeople:   return &kVipPeople;
				case WorldSchema::Event:       return &kEventHeader;
			}
			return nullptr;
		}

		// -----------------------------------------------------------------
		// Tag <-> Json
		// -----------------------------------------------------------------
		Json tagToJson(uint32_t v)
		{
			// Mac order = the big-endian bytes of the LE value ("BOEv").
			std::string s;
			for (int shift = 24; shift >= 0; shift -= 8)
			{
				const unsigned char c = uint8_t(v >> shift);
				if (c < 0x20 || c > 0x7e) return Json((long long)v);
				s.push_back(char(c));
			}
			return Json(s);
		}

		bool tagFromJson(const Json & j, uint32_t & out)
		{
			if (j.isInt())
			{
				if (j.asInt() < 0 || j.asInt() > 0xFFFFFFFFll) return false;
				out = uint32_t(j.asInt());
				return true;
			}
			if (!j.isString() || j.asString().size() != 4) return false;
			const std::string & s = j.asString();
			out = (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16)
			    | (uint32_t(uint8_t(s[2])) << 8) | uint32_t(uint8_t(s[3]));
			return true;
		}

		// -----------------------------------------------------------------
		// Engine: read
		// -----------------------------------------------------------------
		bool readSchema(CResReader & r, const Schema & schema, Json & out)
		{
			out = Json::object();
			std::map<std::string, int32_t> counts;
			for (const F & f : schema)
			{
				switch (f.kind)
				{
					case K::I32: out.set(f.name, Json(r.i32())); break;
					case K::U32: out.set(f.name, Json(r.u32())); break;
					case K::Tag: out.set(f.name, tagToJson(r.u32())); break;
					case K::Str: out.set(f.name, Json(latin1ToUtf8(r.str()))); break;
					case K::Rect:
					{
						const PluginRect rc = r.rect();
						Json o = Json::object();
						o.set("left", Json(rc.left)).set("top", Json(rc.top))
						 .set("right", Json(rc.right)).set("bottom", Json(rc.bottom));
						out.set(f.name, std::move(o));
						break;
					}
					case K::Point:
					{
						const int32_t y = r.i32();
						const int32_t x = r.i32();
						Json o = Json::object();
						o.set("x", Json(x)).set("y", Json(y));
						out.set(f.name, std::move(o));
						break;
					}
					case K::Ints:
					{
						Json a = Json::array();
						for (int k = 0; k < f.n; ++k) a.push(Json(r.i32()));
						out.set(f.name, std::move(a));
						break;
					}
					case K::Count:
						counts[f.name] = r.count();
						break;
					case K::List:
					case K::Strs:
					case K::IList:
					{
						const int32_t n = counts[f.ref];
						Json a = Json::array();
						for (int32_t k = 0; k < n && r.ok(); ++k)
						{
							if (f.kind == K::List)
							{
								Json e;
								if (!readSchema(r, *f.sub, e)) return false;
								a.push(std::move(e));
							}
							else if (f.kind == K::Strs) a.push(Json(latin1ToUtf8(r.str())));
							else a.push(Json(r.i32()));
						}
						out.set(f.name, std::move(a));
						break;
					}
				}
				if (!r.ok()) return false;
			}
			return r.ok();
		}

		// -----------------------------------------------------------------
		// Engine: write
		// -----------------------------------------------------------------
		bool fieldError(std::string * error, const std::string & path, const char * what)
		{
			if (error) *error = path + ": " + what;
			return false;
		}

		bool intField(const Json & j, bool isUnsigned, int64_t & v)
		{
			if (!j.isInt()) return false;
			v = j.asInt();
			return isUnsigned ? (v >= 0 && v <= 0xFFFFFFFFll) : (v >= INT32_MIN && v <= INT32_MAX);
		}

		bool writeSchema(CResWriter & w, const Schema & schema, const Json & rec, const std::string & path,
		                 std::string * error)
		{
			if (!rec.isObject()) return fieldError(error, path, "expected an object");
			for (const F & f : schema)
			{
				const std::string fp = path.empty() ? std::string(f.name) : path + "." + f.name;
				switch (f.kind)
				{
					case K::I32:
					case K::U32:
					{
						int64_t v = 0;
						if (!intField(rec[f.name], f.kind == K::U32, v))
							return fieldError(error, fp, f.kind == K::U32 ? "expected an unsigned 32-bit integer"
							                                              : "expected a 32-bit integer");
						w.u32(uint32_t(v));
						break;
					}
					case K::Tag:
					{
						uint32_t v = 0;
						if (!tagFromJson(rec[f.name], v)) return fieldError(error, fp, "expected a 4-character code");
						w.u32(v);
						break;
					}
					case K::Str:
					{
						std::string bytes;
						if (!rec[f.name].isString() || !utf8ToLatin1(rec[f.name].asString(), bytes))
							return fieldError(error, fp, "expected a Latin-1 string");
						if (bytes.size() > 255) return fieldError(error, fp, "string longer than 255 bytes");
						w.str(bytes);
						break;
					}
					case K::Rect:
					{
						const Json & o = rec[f.name];
						int64_t l = 0, t = 0, rr = 0, b = 0;
						if (!intField(o["left"], false, l) || !intField(o["top"], false, t)
						    || !intField(o["right"], false, rr) || !intField(o["bottom"], false, b))
							return fieldError(error, fp, "expected {left, top, right, bottom}");
						w.i32(int32_t(t)); w.i32(int32_t(l)); w.i32(int32_t(b)); w.i32(int32_t(rr));
						break;
					}
					case K::Point:
					{
						const Json & o = rec[f.name];
						int64_t x = 0, y = 0;
						if (!intField(o["x"], false, x) || !intField(o["y"], false, y))
							return fieldError(error, fp, "expected {x, y}");
						w.i32(int32_t(y)); w.i32(int32_t(x));
						break;
					}
					case K::Ints:
					{
						const Json & a = rec[f.name];
						if (!a.isArray() || int(a.size()) != f.n)
							return fieldError(error, fp, ("expected " + std::to_string(f.n) + " integers").c_str());
						for (const Json & e : a.elements())
						{
							int64_t v = 0;
							if (!intField(e, false, v)) return fieldError(error, fp, "expected 32-bit integers");
							w.i32(int32_t(v));
						}
						break;
					}
					case K::Count:
					{
						const Json & a = rec[f.ref];
						if (!a.isArray()) return fieldError(error, path.empty() ? f.ref : path + "." + f.ref, "expected a list");
						w.count(a.size());
						break;
					}
					case K::List:
					case K::Strs:
					case K::IList:
					{
						const Json & a = rec[f.name];
						if (!a.isArray()) return fieldError(error, fp, "expected a list");
						for (size_t k = 0; k < a.size(); ++k)
						{
							const std::string ep = fp + "[" + std::to_string(k) + "]";
							const Json & e = a.at(k);
							if (f.kind == K::List)
							{
								if (!writeSchema(w, *f.sub, e, ep, error)) return false;
							}
							else if (f.kind == K::Strs)
							{
								std::string bytes;
								if (!e.isString() || !utf8ToLatin1(e.asString(), bytes) || bytes.size() > 255)
									return fieldError(error, ep, "expected a Latin-1 string (<= 255 bytes)");
								w.str(bytes);
							}
							else
							{
								int64_t v = 0;
								if (!intField(e, false, v)) return fieldError(error, ep, "expected a 32-bit integer");
								w.i32(int32_t(v));
							}
						}
						break;
					}
				}
			}
			return true;
		}

		std::string upper(const std::string & s)
		{
			std::string u = s;
			for (char & c : u) c = char(std::toupper(uint8_t(c)));
			return u;
		}

		// -----------------------------------------------------------------
		// Text helpers (BINF / FINF / PTDF)
		// -----------------------------------------------------------------
		/// One decimal per CRLF line, CRLF terminated (world_sp decode_numbers_text).
		bool decodeNumbersText(const char * data, size_t length, std::vector<int32_t> & out)
		{
			out.clear();
			if (length < 2 || data[length - 2] != '\r' || data[length - 1] != '\n') return false;
			size_t p = 0;
			while (p < length)
			{
				bool neg = false;
				if (data[p] == '-') { neg = true; ++p; }
				if (p >= length || !std::isdigit(uint8_t(data[p]))) return false;
				long long v = 0;
				while (p < length && std::isdigit(uint8_t(data[p])))
				{
					v = v * 10 + (data[p] - '0');
					if (v > 0x80000000ll) return false;
					++p;
				}
				if (p + 1 >= length || data[p] != '\r' || data[p + 1] != '\n') return false;
				p += 2;
				out.push_back(int32_t(neg ? -v : v));
			}
			return true;
		}

		void encodeNumbersText(const std::vector<int32_t> & nums, std::string & out)
		{
			out.clear();
			for (int32_t n : nums)
			{
				out += std::to_string(n);
				out += "\r\n";
			}
		}

		bool sameBytes(const std::string & a, const char * data, size_t length)
		{
			return a.size() == length && (length == 0 || a.compare(0, length, data, length) == 0);
		}

		uint16_t be16(const unsigned char * p) { return uint16_t((p[0] << 8) | p[1]); }
		uint32_t be32(const unsigned char * p)
		{
			return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
		}
		void putBe16(std::string & s, uint16_t v) { s.push_back(char(v >> 8)); s.push_back(char(v & 0xFF)); }
		void putBe32(std::string & s, uint32_t v)
		{
			putBe16(s, uint16_t(v >> 16));
			putBe16(s, uint16_t(v & 0xFFFF));
		}

		bool jsonInt(const Json & j, int64_t lo, int64_t hi, int64_t & v)
		{
			if (!j.isInt()) return false;
			v = j.asInt();
			return v >= lo && v <= hi;
		}

		bool intArray(const Json & j, std::vector<int32_t> & out)
		{
			if (!j.isArray()) return false;
			out.clear();
			out.reserve(j.size());
			for (const Json & e : j.elements())
			{
				int64_t v = 0;
				if (!jsonInt(e, INT32_MIN, INT32_MAX, v)) return false;
				out.push_back(int32_t(v));
			}
			return true;
		}

		bool setError(std::string * error, const std::string & msg)
		{
			if (error) *error = msg;
			return false;
		}
	}

	bool worldSchemaForType(const std::string & type, WorldSchema & out)
	{
		if (type == "WORLDDEF") { out = WorldSchema::WorldDef; return true; }
		if (type == "WATR")     { out = WorldSchema::Watr; return true; }
		if (type == "PODF")     { out = WorldSchema::PoolDef; return true; }
		if (type == "CODF")     { out = WorldSchema::ColdTable; return true; }
		if (type == "SPDF")     { out = WorldSchema::SeasonParam; return true; }
		if (type == "T2EV")     { out = WorldSchema::TowerEvents; return true; }
		if (type == "VIPL")     { out = WorldSchema::VipPeople; return true; }
		if (type.size() == 4 && type[2] == 'E' && type[3] == 'V') { out = WorldSchema::Event; return true; }
		return false;
	}

	bool worldEventHasTypedBody(const std::string & type)
	{
		return eventBodies().count(upper(type)) != 0;
	}

	bool decodeWorldSchemaResource(WorldSchema schema, const std::string & type,
	                               const char * data, size_t length, Json & out)
	{
		const Schema * sch = schemaFor(schema);
		if (!sch) return false;
		CResReader r(data, length);
		Json rec;
		if (!readSchema(r, *sch, rec)) return false;
		if (schema == WorldSchema::Event)
		{
			// Header read; now the body. A known layout must consume the
			// rest exactly, otherwise the tail is kept as raw ints/hex.
			const unsigned char * tail = reinterpret_cast<const unsigned char *>(data) + (length - r.remaining());
			const size_t tailLen = r.remaining();
			bool typed = false;
			auto it = eventBodies().find(upper(type));
			if (it != eventBodies().end())
			{
				std::string sub(1, char(1));
				sub.append(reinterpret_cast<const char *>(tail), tailLen);
				CResReader br(sub.data(), sub.size());
				Json body;
				if (readSchema(br, it->second, body) && br.atEnd())
				{
					rec.set("body", std::move(body));
					typed = true;
				}
			}
			if (!typed)
			{
				if (tailLen % 4 == 0)
				{
					Json a = Json::array();
					for (size_t k = 0; k < tailLen; k += 4)
						a.push(Json(int32_t(uint32_t(tail[k]) | (uint32_t(tail[k + 1]) << 8)
						                    | (uint32_t(tail[k + 2]) << 16) | (uint32_t(tail[k + 3]) << 24))));
					rec.set("bodyInts", std::move(a));
				}
				else
					rec.set("bodyHex", Json(toHex(std::string(reinterpret_cast<const char *>(tail), tailLen))));
			}
		}
		else if (!r.atEnd())
			return false;
		// Strict: must re-encode to the same bytes.
		std::string again;
		if (!encodeWorldSchemaResource(schema, type, rec, again) || !sameBytes(again, data, length)) return false;
		out = std::move(rec);
		return true;
	}

	bool encodeWorldSchemaResource(WorldSchema schema, const std::string & type,
	                               const Json & record, std::string & out, std::string * error)
	{
		const Schema * sch = schemaFor(schema);
		if (!sch) return setError(error, "not a world schema resource");
		CResWriter w;
		if (!writeSchema(w, *sch, record, "", error)) return false;
		if (schema == WorldSchema::Event)
		{
			if (const Json * body = record.find("body"))
			{
				auto it = eventBodies().find(upper(type));
				if (it == eventBodies().end()) return setError(error, "body: no typed layout for " + type);
				if (!writeSchema(w, it->second, *body, "body", error)) return false;
			}
			else if (const Json * ints = record.find("bodyInts"))
			{
				std::vector<int32_t> v;
				if (!intArray(*ints, v)) return setError(error, "bodyInts: expected 32-bit integers");
				for (int32_t x : v) w.i32(x);
			}
			else if (const Json * hex = record.find("bodyHex"))
			{
				std::string bytes;
				if (!hex->isString() || !fromHex(hex->asString(), bytes)) return setError(error, "bodyHex: bad hex");
				std::string head;
				if (!w.finish(head)) return setError(error, "string too long");
				out = head + bytes;
				return true;
			}
		}
		if (!w.finish(out)) return setError(error, "string too long");
		return true;
	}

	// ---------------------------------------------------------------------
	// BINF
	// ---------------------------------------------------------------------
	bool decodeBkgndInfoDef(const char * data, size_t length, BkgndInfoDef & out)
	{
		std::vector<int32_t> n;
		if (!decodeNumbersText(data, length, n) || n.size() < 3) return false;
		BkgndInfoDef d;
		d.rows = n[0];
		d.cols = n[1];
		d.unused = n[2];
		if (d.rows < 0 || d.cols < 0 || n.size() != 3 + size_t(d.rows) * size_t(d.cols)) return false;
		d.cells.resize(size_t(d.rows) * size_t(d.cols));
		// Stored column-major: for col: for row.
		for (int32_t c = 0; c < d.cols; ++c)
			for (int32_t r = 0; r < d.rows; ++r)
				d.cells[size_t(r) * size_t(d.cols) + size_t(c)] = n[3 + size_t(c) * size_t(d.rows) + size_t(r)];
		std::string again;
		if (!encodeBkgndInfoDef(d, again) || !sameBytes(again, data, length)) return false;
		out = std::move(d);
		return true;
	}

	bool encodeBkgndInfoDef(const BkgndInfoDef & d, std::string & out)
	{
		if (d.rows < 0 || d.cols < 0 || d.cells.size() != size_t(d.rows) * size_t(d.cols)) return false;
		std::vector<int32_t> n = {d.rows, d.cols, d.unused};
		n.reserve(3 + d.cells.size());
		for (int32_t c = 0; c < d.cols; ++c)
			for (int32_t r = 0; r < d.rows; ++r)
				n.push_back(d.at(r, c));
		encodeNumbersText(n, out);
		return true;
	}

	// ---------------------------------------------------------------------
	// FINF
	// ---------------------------------------------------------------------
	bool decodeFloorInfoMaskDef(const char * data, size_t length, FloorInfoMaskDef & out)
	{
		std::vector<int32_t> n;
		if (!decodeNumbersText(data, length, n) || n.size() < 7) return false;
		FloorInfoMaskDef d;
		d.height = n[0];
		d.width = n[1];
		d.unk = n[2];
		for (int k = 0; k < 4; ++k) d.rectLTRB[size_t(k)] = n[3 + size_t(k)];
		if (d.height < 0 || d.width < 0 || n.size() != 7 + size_t(d.height) * size_t(d.width)) return false;
		d.cells.assign(n.begin() + 7, n.end());
		std::string again;
		if (!encodeFloorInfoMaskDef(d, again) || !sameBytes(again, data, length)) return false;
		out = std::move(d);
		return true;
	}

	bool encodeFloorInfoMaskDef(const FloorInfoMaskDef & d, std::string & out)
	{
		if (d.height < 0 || d.width < 0 || d.cells.size() != size_t(d.height) * size_t(d.width)) return false;
		std::vector<int32_t> n = {d.height, d.width, d.unk, d.rectLTRB[0], d.rectLTRB[1], d.rectLTRB[2], d.rectLTRB[3]};
		n.insert(n.end(), d.cells.begin(), d.cells.end());
		encodeNumbersText(n, out);
		return true;
	}

	// ---------------------------------------------------------------------
	// PTDF
	// ---------------------------------------------------------------------
	bool decodePaletteAnimeDef(const char * data, size_t length, PaletteAnimeDef & out)
	{
		const std::string txt(data, length);
		PaletteAnimeDef d;
		size_t p = 0;
		while (p < txt.size())
		{
			const size_t eol = txt.find("\r\n", p);
			if (eol == std::string::npos) return false;
			const std::string ln = txt.substr(p, eol - p);
			p = eol + 2;
			if (ln.empty() || ln[0] != '(') return false;
			const size_t close = ln.find(')');
			if (close == std::string::npos) return false;
			PaletteAnimeDef::Field f;
			f.name = ln.substr(1, close - 1);
			size_t q = close + 1;
			while (q < ln.size() && (ln[q] == '\t' || ln[q] == ' ')) ++q;
			f.sep = ln.substr(close + 1, q - close - 1);
			std::vector<int32_t> v;
			const std::string num = ln.substr(q) + "\r\n";
			if (!decodeNumbersText(num.data(), num.size(), v) || v.size() != 1) return false;
			f.value = v[0];
			d.fields.push_back(std::move(f));
		}
		std::string again;
		if (!encodePaletteAnimeDef(d, again) || !sameBytes(again, data, length)) return false;
		out = std::move(d);
		return true;
	}

	bool encodePaletteAnimeDef(const PaletteAnimeDef & d, std::string & out)
	{
		out.clear();
		for (const PaletteAnimeDef::Field & f : d.fields)
		{
			if (f.name.find(')') != std::string::npos || f.name.find('\r') != std::string::npos) return false;
			for (char c : f.sep)
				if (c != '\t' && c != ' ') return false;
			out += "(" + f.name + ")" + f.sep + std::to_string(f.value) + "\r\n";
		}
		return true;
	}

	// ---------------------------------------------------------------------
	// PALETTE
	// ---------------------------------------------------------------------
	bool decodePaletteDef(const char * data, size_t length, PaletteDef & out)
	{
		const unsigned char * b = reinterpret_cast<const unsigned char *>(data);
		PaletteDef d;
		if (length == 0x300)
		{
			d.format = PaletteDef::Act;
			for (size_t k = 0; k < 256; ++k)
			{
				PaletteDef::Entry e;
				e.v[0] = b[3 * k]; e.v[1] = b[3 * k + 1]; e.v[2] = b[3 * k + 2];
				d.entries.push_back(e);
			}
		}
		else if (length >= 8 && b[0] == 0 && b[1] == 0)
		{
			d.format = PaletteDef::Clut;
			d.seed = be32(b);
			d.flags = be16(b + 4);
			d.ctSize = be16(b + 6);
			if ((length - 8) % 8) return false;
			for (size_t k = 8; k < length; k += 8)
			{
				PaletteDef::Entry e;
				for (int i = 0; i < 4; ++i) e.v[size_t(i)] = be16(b + k + 2 * size_t(i));
				d.entries.push_back(e);
			}
		}
		else if (length >= 16)
		{
			d.format = PaletteDef::Pltt;
			const uint16_t count = be16(b);
			if (16 + 16 * size_t(count) != length) return false;
			d.plttHeader.assign(data + 2, 14);
			for (size_t k = 0; k < count; ++k)
			{
				const unsigned char * e8 = b + 16 + 16 * k;
				PaletteDef::Entry e;
				for (int i = 0; i < 6; ++i) e.v[size_t(i)] = be16(e8 + 2 * size_t(i));
				e.priv = be32(e8 + 12);
				d.entries.push_back(e);
			}
		}
		else return false;
		std::string again;
		if (!encodePaletteDef(d, again) || !sameBytes(again, data, length)) return false;
		out = std::move(d);
		return true;
	}

	bool encodePaletteDef(const PaletteDef & d, std::string & out)
	{
		out.clear();
		switch (d.format)
		{
			case PaletteDef::Act:
				if (d.entries.size() != 256) return false;
				for (const PaletteDef::Entry & e : d.entries)
					for (int i = 0; i < 3; ++i)
					{
						if (e.v[size_t(i)] > 255) return false;
						out.push_back(char(e.v[size_t(i)]));
					}
				return true;
			case PaletteDef::Clut:
				putBe32(out, d.seed);
				putBe16(out, d.flags);
				putBe16(out, d.ctSize);
				for (const PaletteDef::Entry & e : d.entries)
					for (int i = 0; i < 4; ++i) putBe16(out, e.v[size_t(i)]);
				return true;
			case PaletteDef::Pltt:
				if (d.entries.size() > 0xFFFF || d.plttHeader.size() != 14) return false;
				putBe16(out, uint16_t(d.entries.size()));
				out += d.plttHeader;
				for (const PaletteDef::Entry & e : d.entries)
				{
					for (int i = 0; i < 6; ++i) putBe16(out, e.v[size_t(i)]);
					putBe32(out, e.priv);
				}
				return true;
		}
		return false;
	}

	std::vector<std::array<uint8_t, 3>> PaletteDef::rgb() const
	{
		std::vector<std::array<uint8_t, 3>> pal(256, std::array<uint8_t, 3>{{0, 0, 0}});
		if (format == Act)
		{
			for (size_t k = 0; k < entries.size() && k < 256; ++k)
				pal[k] = {{uint8_t(entries[k].v[0]), uint8_t(entries[k].v[1]), uint8_t(entries[k].v[2])}};
		}
		else if (format == Clut)
		{
			for (size_t k = 0; k < entries.size() && k < ctSize; ++k)
			{
				const PaletteDef::Entry & e = entries[k];
				pal[e.v[0] & 0xFF] = {{uint8_t(e.v[1] >> 8), uint8_t(e.v[2] >> 8), uint8_t(e.v[3] >> 8)}};
			}
		}
		else
		{
			for (size_t k = 0; k < entries.size() && k < 256; ++k)
				pal[k] = {{uint8_t(entries[k].v[0] >> 8), uint8_t(entries[k].v[1] >> 8), uint8_t(entries[k].v[2] >> 8)}};
		}
		return pal;
	}

	// ---------------------------------------------------------------------
	// Json views
	// ---------------------------------------------------------------------
	namespace
	{
		Json gridToJson(const std::vector<int32_t> & cells, int32_t rows, int32_t cols)
		{
			Json g = Json::array();
			for (int32_t r = 0; r < rows; ++r)
			{
				Json row = Json::array();
				for (int32_t c = 0; c < cols; ++c) row.push(Json(cells[size_t(r) * size_t(cols) + size_t(c)]));
				g.push(std::move(row));
			}
			return g;
		}

		bool gridFromJson(const Json & g, int32_t rows, int32_t cols, std::vector<int32_t> & cells)
		{
			if (!g.isArray() || int64_t(g.size()) != rows) return false;
			cells.clear();
			cells.reserve(size_t(rows) * size_t(cols));
			for (const Json & row : g.elements())
			{
				std::vector<int32_t> v;
				if (!intArray(row, v) || int64_t(v.size()) != cols) return false;
				cells.insert(cells.end(), v.begin(), v.end());
			}
			return true;
		}
	}

	Json bkgndInfoToJson(const BkgndInfoDef & d)
	{
		Json j = Json::object();
		j.set("rows", Json(d.rows)).set("cols", Json(d.cols)).set("unused", Json(d.unused));
		j.set("grid", gridToJson(d.cells, d.rows, d.cols));
		return j;
	}

	bool bkgndInfoFromJson(const Json & j, BkgndInfoDef & out, std::string * error)
	{
		BkgndInfoDef d;
		int64_t v = 0;
		if (!jsonInt(j["rows"], 0, 100000, v)) return setError(error, "rows: expected 0..100000");
		d.rows = int32_t(v);
		if (!jsonInt(j["cols"], 0, 100000, v)) return setError(error, "cols: expected 0..100000");
		d.cols = int32_t(v);
		if (!jsonInt(j["unused"], INT32_MIN, INT32_MAX, v)) return setError(error, "unused: expected an integer");
		d.unused = int32_t(v);
		if (!gridFromJson(j["grid"], d.rows, d.cols, d.cells)) return setError(error, "grid: expected rows x cols integers");
		out = std::move(d);
		return true;
	}

	Json floorInfoMaskToJson(const FloorInfoMaskDef & d)
	{
		Json j = Json::object();
		j.set("height", Json(d.height)).set("width", Json(d.width)).set("unk", Json(d.unk));
		Json r = Json::array();
		for (int32_t v : d.rectLTRB) r.push(Json(v));
		j.set("rectLTRB", std::move(r));
		j.set("grid", gridToJson(d.cells, d.height, d.width));
		return j;
	}

	bool floorInfoMaskFromJson(const Json & j, FloorInfoMaskDef & out, std::string * error)
	{
		FloorInfoMaskDef d;
		int64_t v = 0;
		if (!jsonInt(j["height"], 0, 100000, v)) return setError(error, "height: expected 0..100000");
		d.height = int32_t(v);
		if (!jsonInt(j["width"], 0, 100000, v)) return setError(error, "width: expected 0..100000");
		d.width = int32_t(v);
		if (!jsonInt(j["unk"], INT32_MIN, INT32_MAX, v)) return setError(error, "unk: expected an integer");
		d.unk = int32_t(v);
		std::vector<int32_t> rect;
		if (!intArray(j["rectLTRB"], rect) || rect.size() != 4) return setError(error, "rectLTRB: expected 4 integers");
		for (size_t k = 0; k < 4; ++k) d.rectLTRB[k] = rect[k];
		if (!gridFromJson(j["grid"], d.height, d.width, d.cells)) return setError(error, "grid: expected height x width integers");
		out = std::move(d);
		return true;
	}

	Json paletteAnimeToJson(const PaletteAnimeDef & d)
	{
		Json a = Json::array();
		for (const PaletteAnimeDef::Field & f : d.fields)
		{
			Json o = Json::object();
			o.set("name", Json(latin1ToUtf8(f.name))).set("value", Json(f.value)).set("sep", Json(f.sep));
			a.push(std::move(o));
		}
		return Json::object().set("fields", std::move(a));
	}

	bool paletteAnimeFromJson(const Json & j, PaletteAnimeDef & out, std::string * error)
	{
		const Json & a = j["fields"];
		if (!a.isArray()) return setError(error, "fields: expected a list");
		PaletteAnimeDef d;
		for (size_t k = 0; k < a.size(); ++k)
		{
			const Json & o = a.at(k);
			PaletteAnimeDef::Field f;
			int64_t v = 0;
			if (!o["name"].isString() || !utf8ToLatin1(o["name"].asString(), f.name))
				return setError(error, "fields[" + std::to_string(k) + "].name: expected a string");
			if (!jsonInt(o["value"], INT32_MIN, INT32_MAX, v))
				return setError(error, "fields[" + std::to_string(k) + "].value: expected an integer");
			f.value = int32_t(v);
			f.sep = o["sep"].isString() ? o["sep"].asString() : std::string("\t");
			d.fields.push_back(std::move(f));
		}
		std::string probe;
		if (!encodePaletteAnimeDef(d, probe)) return setError(error, "fields: name must not contain ')' and sep must be tabs/spaces");
		out = std::move(d);
		return true;
	}

	Json paletteToJson(const PaletteDef & d)
	{
		Json j = Json::object();
		j.set("format", Json(d.format == PaletteDef::Act ? "act" : d.format == PaletteDef::Clut ? "clut" : "pltt"));
		const int width = d.format == PaletteDef::Act ? 3 : d.format == PaletteDef::Clut ? 4 : 6;
		if (d.format == PaletteDef::Clut)
			j.set("seed", Json(d.seed)).set("flags", Json(d.flags)).set("ctSize", Json(d.ctSize));
		if (d.format == PaletteDef::Pltt) j.set("header", Json(toHex(d.plttHeader)));
		Json a = Json::array();
		for (const PaletteDef::Entry & e : d.entries)
		{
			Json row = Json::array();
			for (int i = 0; i < width; ++i) row.push(Json(e.v[size_t(i)]));
			if (d.format == PaletteDef::Pltt) row.push(Json(e.priv));
			a.push(std::move(row));
		}
		// act: [r,g,b] 8-bit; clut: [index, r16, g16, b16]; pltt: [r16, g16, b16, usage, tolerance, flags, private]
		j.set("entries", std::move(a));
		return j;
	}

	bool paletteFromJson(const Json & j, PaletteDef & out, std::string * error)
	{
		PaletteDef d;
		const std::string fmt = j["format"].asString();
		if (fmt == "act") d.format = PaletteDef::Act;
		else if (fmt == "clut") d.format = PaletteDef::Clut;
		else if (fmt == "pltt") d.format = PaletteDef::Pltt;
		else return setError(error, "format: expected act, clut or pltt");
		int64_t v = 0;
		if (d.format == PaletteDef::Clut)
		{
			if (!jsonInt(j["seed"], 0, 0xFFFFFFFFll, v)) return setError(error, "seed: expected u32");
			d.seed = uint32_t(v);
			if (!jsonInt(j["flags"], 0, 0xFFFF, v)) return setError(error, "flags: expected u16");
			d.flags = uint16_t(v);
			if (!jsonInt(j["ctSize"], 0, 0xFFFF, v)) return setError(error, "ctSize: expected u16");
			d.ctSize = uint16_t(v);
		}
		if (d.format == PaletteDef::Pltt)
		{
			if (!j["header"].isString() || !fromHex(j["header"].asString(), d.plttHeader) || d.plttHeader.size() != 14)
				return setError(error, "header: expected 14 hex bytes");
		}
		const int width = d.format == PaletteDef::Act ? 3 : d.format == PaletteDef::Clut ? 4 : 6;
		const Json & a = j["entries"];
		if (!a.isArray()) return setError(error, "entries: expected a list");
		for (size_t k = 0; k < a.size(); ++k)
		{
			const Json & row = a.at(k);
			const size_t want = size_t(width) + (d.format == PaletteDef::Pltt ? 1 : 0);
			if (!row.isArray() || row.size() != want) return setError(error, "entries[" + std::to_string(k) + "]: wrong arity");
			PaletteDef::Entry e;
			const int64_t hi = d.format == PaletteDef::Act ? 255 : 0xFFFF;
			for (int i = 0; i < width; ++i)
			{
				if (!jsonInt(row.at(size_t(i)), 0, hi, v)) return setError(error, "entries[" + std::to_string(k) + "]: value out of range");
				e.v[size_t(i)] = uint16_t(v);
			}
			if (d.format == PaletteDef::Pltt)
			{
				if (!jsonInt(row.at(6), 0, 0xFFFFFFFFll, v)) return setError(error, "entries[" + std::to_string(k) + "]: private out of range");
				e.priv = uint32_t(v);
			}
			d.entries.push_back(e);
		}
		if (d.format == PaletteDef::Act && d.entries.size() != 256) return setError(error, "entries: act needs 256 colours");
		out = std::move(d);
		return true;
	}
}
