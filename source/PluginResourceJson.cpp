/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginResourceJson.h"

#include <climits>

namespace OT
{
	// ---------------------------------------------------------------------
	// Shared helpers
	// ---------------------------------------------------------------------
	Json fourccToJson(const FourCC & f)
	{
		if (f.isZero()) return Json("");
		const std::string mac = f.macStr();
		bool printable = mac.size() == 4;
		for (char c : mac)
			if (uint8_t(c) < 0x20 || uint8_t(c) > 0x7e) printable = false;
		if (printable) return Json(mac);
		return Json((long long)f.toUint32LE());
	}

	bool fourccFromJson(const Json & j, FourCC & out)
	{
		if (j.isInt())
		{
			if (j.asInt() < 0 || j.asInt() > 0xFFFFFFFFll) return false;
			out = FourCC::fromUint32LE(uint32_t(j.asInt()));
			return true;
		}
		if (!j.isString()) return false;
		if (j.asString().empty()) { out = FourCC::fromUint32LE(0); return true; }
		std::string bytes;
		if (!utf8ToLatin1(j.asString(), bytes) || bytes.size() != 4) return false;
		out = FourCC::fromMacOrder(bytes);
		return true;
	}

	std::string utf16ToWtf8(const std::u16string & s)
	{
		std::string out;
		for (size_t i = 0; i < s.size(); ++i)
		{
			uint32_t cp = s[i];
			if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000)
			{
				cp = 0x10000 + ((cp - 0xD800) << 10) + (uint32_t(s[i + 1]) - 0xDC00);
				++i;
			}
			if (cp < 0x80) out.push_back(char(cp));
			else if (cp < 0x800)
			{
				out.push_back(char(0xC0 | (cp >> 6)));
				out.push_back(char(0x80 | (cp & 0x3F)));
			}
			else if (cp < 0x10000)
			{
				out.push_back(char(0xE0 | (cp >> 12)));
				out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
				out.push_back(char(0x80 | (cp & 0x3F)));
			}
			else
			{
				out.push_back(char(0xF0 | (cp >> 18)));
				out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
				out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
				out.push_back(char(0x80 | (cp & 0x3F)));
			}
		}
		return out;
	}

	bool wtf8ToUtf16(const std::string & s, std::u16string & out)
	{
		std::u16string r;
		for (size_t i = 0; i < s.size();)
		{
			const unsigned char c = uint8_t(s[i]);
			uint32_t cp = 0;
			size_t n = 0;
			if (c < 0x80) { cp = c; n = 1; }
			else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
			else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
			else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
			else return false;
			if (i + n > s.size()) return false;
			for (size_t k = 1; k < n; ++k)
			{
				const unsigned char cc = uint8_t(s[i + k]);
				if ((cc & 0xC0) != 0x80) return false;
				cp = (cp << 6) | (cc & 0x3F);
			}
			i += n;
			if (cp >= 0x10000)
			{
				cp -= 0x10000;
				r.push_back(char16_t(0xD800 + (cp >> 10)));
				r.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
			}
			else r.push_back(char16_t(cp));
		}
		out = std::move(r);
		return true;
	}

	namespace
	{
		Json str(const std::string & bytes) { return Json(latin1ToUtf8(bytes)); }

		Json rectToJson(const PluginRect & r)
		{
			Json o = Json::object();
			o.set("left", Json(r.left)).set("top", Json(r.top)).set("right", Json(r.right)).set("bottom", Json(r.bottom));
			return o;
		}

		/// Json reader with field-path error reporting. The first error wins.
		struct JState
		{
			std::string error;
			bool ok = true;
		};

		class JIn
		{
		public:
			JIn(const Json & j, std::string path, JState & st) : j_(j), path_(std::move(path)), st_(st)
			{
				if (!j_.isObject()) fail("", "expected an object");
			}

			bool ok() const { return st_.ok; }
			const Json & raw(const char * k) const { return j_[k]; }
			bool has(const char * k) const { return j_.has(k); }
			std::string at(const std::string & k) const { return path_.empty() ? k : path_ + "." + k; }

			void fail(const std::string & k, const std::string & what)
			{
				if (!st_.ok) return;
				st_.ok = false;
				st_.error = (k.empty() ? (path_.empty() ? std::string("document") : path_) : at(k)) + ": " + what;
			}

			int64_t integer(const char * k, int64_t lo, int64_t hi, const char * what)
			{
				const Json & v = j_[k];
				if (!v.isInt() || v.asInt() < lo || v.asInt() > hi) { fail(k, what); return 0; }
				return v.asInt();
			}
			int32_t i32(const char * k) { return int32_t(integer(k, INT32_MIN, INT32_MAX, "expected a 32-bit integer")); }
			uint32_t u32(const char * k) { return uint32_t(integer(k, 0, 0xFFFFFFFFll, "expected an unsigned 32-bit integer")); }
			/// Signed view of an unsigned slot (-1 == 0xFFFFFFFF).
			uint32_t u32s(const char * k) { return uint32_t(int32_t(integer(k, INT32_MIN, INT32_MAX, "expected a 32-bit integer"))); }
			bool boolean(const char * k)
			{
				const Json & v = j_[k];
				if (!v.isBool() && !v.isInt()) { fail(k, "expected true/false"); return false; }
				return v.asBool();
			}
			std::string text(const char * k, size_t maxBytes = 255)
			{
				const Json & v = j_[k];
				std::string bytes;
				if (!v.isString() || !utf8ToLatin1(v.asString(), bytes)) { fail(k, "expected a Latin-1 string"); return ""; }
				if (bytes.size() > maxBytes) { fail(k, "longer than " + std::to_string(maxBytes) + " bytes"); return ""; }
				return bytes;
			}
			std::string hex(const char * k)
			{
				const Json & v = j_[k];
				std::string bytes;
				if (v.isNull()) return "";
				if (!v.isString() || !fromHex(v.asString(), bytes)) { fail(k, "expected hex bytes"); return ""; }
				return bytes;
			}
			FourCC tag(const char * k)
			{
				FourCC f;
				if (!fourccFromJson(j_[k], f)) fail(k, "expected a 4-character code");
				return f;
			}
			PluginRect rect(const char * k)
			{
				PluginRect r;
				JIn o = sub(k);
				r.left = o.i32("left");
				r.top = o.i32("top");
				r.right = o.i32("right");
				r.bottom = o.i32("bottom");
				return r;
			}
			JIn sub(const char * k) { return JIn(j_[k], at(k), st_); }
			/// Elements of an array member; fails when it is not an array or
			/// has more than maxCount entries.
			const std::vector<Json> & list(const char * k, size_t maxCount = 100000)
			{
				static const std::vector<Json> kEmpty;
				const Json & v = j_[k];
				if (!v.isArray()) { fail(k, "expected a list"); return kEmpty; }
				if (v.size() > maxCount) { fail(k, "at most " + std::to_string(maxCount) + " entries"); return kEmpty; }
				return v.elements();
			}
			JIn item(const char * k, size_t i, const Json & e) { return JIn(e, at(k) + "[" + std::to_string(i) + "]", st_); }
			std::vector<int32_t> ints(const char * k, size_t exact = size_t(-1))
			{
				std::vector<int32_t> out;
				const Json & v = j_[k];
				if (!v.isArray() || (exact != size_t(-1) && v.size() != exact))
				{
					fail(k, exact == size_t(-1) ? std::string("expected a list of integers")
					                             : "expected " + std::to_string(exact) + " integers");
					return out;
				}
				for (const Json & e : v.elements())
				{
					if (!e.isInt() || e.asInt() < INT32_MIN || e.asInt() > INT32_MAX) { fail(k, "expected 32-bit integers"); break; }
					out.push_back(int32_t(e.asInt()));
				}
				return out;
			}

		private:
			const Json & j_;
			std::string path_;
			JState & st_;
		};

		// -----------------------------------------------------------------
		// ATTR / DESC
		// -----------------------------------------------------------------
		Json attrToJson(const PluginAttr & a)
		{
			Json j = Json::object();
			j.set("pluginId", fourccToJson(a.id));
			j.set("attrib", Json(a.attrib));
			j.set("dependPluginId", fourccToJson(a.dependID));
			j.set("gameLevel", Json(a.gameLevel));
			j.set("secretItem", Json(a.secretItem));
			j.set("itemType", Json(a.itemType));
			j.set("mustItem", Json(a.mustItem));
			j.set("name", str(a.name));
			if (a.isTextForm)
			{
				j.set("textForm", Json(true));
				j.set("text", str(a.text));
			}
			else j.set("version", Json(a.version));
			return j;
		}

		void attrFromJson(JIn & in, PluginAttr & a)
		{
			a.valid = true;
			a.id = in.tag("pluginId");
			a.attrib = in.u32("attrib");
			a.dependID = in.tag("dependPluginId");
			a.gameLevel = in.i32("gameLevel");
			a.secretItem = in.i32("secretItem");
			a.itemType = in.i32("itemType");
			a.mustItem = in.i32("mustItem");
			a.name = in.text("name");
			a.isTextForm = in.has("textForm") && in.boolean("textForm");
			if (a.isTextForm) a.text = in.text("text", 1u << 20);
			else a.version = int(in.integer("version", 0, 255, "expected 0..255"));
		}

		Json descToJson(const PluginDesc & d)
		{
			Json j = Json::object();
			j.set("classTag", fourccToJson(d.classTag));
			j.set("version", Json(d.value));
			j.set("leadByte", Json(d.version));
			return j;
		}

		void descFromJson(JIn & in, PluginDesc & d)
		{
			d.valid = true;
			d.classTag = in.tag("classTag");
			d.value = in.u32("version");
			d.version = int(in.integer("leadByte", 0, 255, "expected 0..255"));
			d.kind = pluginKindFromTag(d.classTag);
		}

		// -----------------------------------------------------------------
		// TNDF / MVDF / OODF
		// -----------------------------------------------------------------
		const char * kindName(TemplateDef::Kind k)
		{
			return k == TemplateDef::Mover ? "Mover" : k == TemplateDef::OuterObj ? "OuterObj" : "Tenant";
		}

		Json templateToJson(const TemplateDef & d)
		{
			Json j = Json::object();
			j.set("kind", Json(kindName(d.kind)));
			const ToolDefPart & t = d.tool;
			Json tool = Json::object();
			tool.set("toolType", Json(t.toolType)).set("width", Json(t.width)).set("height", Json(t.height))
			    .set("level", Json(t.level)).set("attribute", Json(t.attribute))
			    .set("categoryHelp", str(t.categoryHelp)).set("categoryName", str(t.categoryName))
			    .set("category", Json(t.category)).set("toolNo", Json(t.toolNo))
			    .set("subPluginType", fourccToJson(t.subPluginType))
			    .set("skip1", Json(t.skip1)).set("skip2", Json(t.skip2))
			    .set("skip3", Json(t.skip3)).set("skip4", Json(t.skip4));
			j.set("tool", std::move(tool));

			const bool hasEquip = d.kind != TemplateDef::OuterObj;
			Json vars = Json::array();
			for (size_t i = 0; i < t.variations.size(); ++i)
			{
				const ToolVariation & v = t.variations[i];
				Json o = Json::object();
				o.set("name", str(v.name)).set("helpTitle", str(v.helpTitle)).set("help", str(v.help))
				 .set("price", Json(v.price)).set("priceString", str(v.priceString))
				 .set("outMoney", Json(v.outMoney)).set("outMoneyString", str(v.outMoneyString))
				 .set("consumptionPower", Json(v.consumptionPower))
				 .set("skip5", Json(v.skip5)).set("skip6", Json(v.skip6)).set("skip7", Json(v.skip7));
				if (hasEquip && i < d.equip.variations.size())
				{
					const EquipVariation & e = d.equip.variations[i];
					o.set("outSpan", Json(e.outSpan)).set("skip9", Json(e.skip9)).set("skip10", Json(e.skip10));
				}
				vars.push(std::move(o));
			}
			j.set("variations", std::move(vars));

			if (hasEquip)
			{
				const EquipDefPart & e = d.equip;
				Json eq = Json::object();
				Json in = Json::array();
				for (int32_t v : e.inMoney) in.push(Json(v));
				eq.set("capacity", Json(e.capacity)).set("stress", Json(e.stress)).set("inMoney", std::move(in))
				  .set("inSpan", Json(e.inSpan)).set("buildDays", Json(e.buildDays)).set("skip8", Json(e.skip8));
				j.set("equip", std::move(eq));
			}
			if (d.kind == TemplateDef::Tenant)
			{
				const TenantDefPart & n = d.tenant;
				Json o = Json::object();
				o.set("registID", Json(n.registID)).set("downFlag", Json(n.downFlag))
				 .set("numOfKind", Json(n.numOfKind)).set("numOfPattern", Json(n.numOfPattern))
				 .set("belongCapacity", Json(n.belongCapacity)).set("senseOfNoise", Json(n.senseOfNoise))
				 .set("entranceV", Json(n.entranceV)).set("entranceH", Json(n.entranceH))
				 .set("exitV", Json(n.exitV)).set("exitH", Json(n.exitH))
				 .set("openTime", Json(n.openTime)).set("closeTime", Json(n.closeTime))
				 .set("bindTime", Json(n.bindTime)).set("maxBuildCount", Json(n.maxBuildCount))
				 .set("movieName", str(n.movieName))
				 .set("skip11", Json(n.skip11)).set("skip12", Json(n.skip12)).set("skip13", Json(n.skip13));
				j.set("tenant", std::move(o));
			}
			if (d.kind == TemplateDef::Mover)
			{
				const MoverDefPart & m = d.mover;
				Json o = Json::object();
				Json speed = Json::array();
				for (int32_t v : m.speed) speed.push(Json(v));
				o.set("addSize", rectToJson(m.addSize)).set("maxLength", Json(m.maxLength))
				 .set("minLength", Json(m.minLength)).set("maxStop", Json(m.maxStop)).set("score", Json(m.score))
				 .set("defaultWait", Json(m.defaultWait)).set("speed", std::move(speed))
				 .set("maxModule", Json(m.maxModule)).set("moduleInset", rectToJson(m.moduleInset))
				 .set("moduleUnitH", Json(m.moduleUnitH)).set("moduleUnitV", Json(m.moduleUnitV))
				 .set("moduleParam1", Json(m.moduleParam1)).set("moduleParam2", Json(m.moduleParam2))
				 .set("skip14", Json(m.skip14)).set("skip15", Json(m.skip15))
				 .set("skip16", Json(m.skip16)).set("skip17", Json(m.skip17));
				j.set("mover", std::move(o));
			}
			if (d.kind == TemplateDef::OuterObj)
			{
				Json o = Json::object();
				o.set("unreadA", Json(d.unreadA)).set("unreadB", Json(d.unreadB));
				j.set("outer", std::move(o));
			}
			return j;
		}

		void templateFromJson(JIn & in, const std::string & type, TemplateDef & d)
		{
			d.kind = type == "MVDF" ? TemplateDef::Mover : type == "OODF" ? TemplateDef::OuterObj : TemplateDef::Tenant;
			JIn t = in.sub("tool");
			ToolDefPart & tool = d.tool;
			tool.toolType = t.i32("toolType");
			tool.width = t.i32("width");
			tool.height = t.i32("height");
			tool.level = t.i32("level");
			tool.attribute = t.u32("attribute");
			tool.categoryHelp = t.text("categoryHelp");
			tool.categoryName = t.text("categoryName");
			tool.category = t.i32("category");
			tool.toolNo = t.i32("toolNo");
			tool.subPluginType = t.tag("subPluginType");
			tool.skip1 = t.i32("skip1");
			tool.skip2 = t.i32("skip2");
			tool.skip3 = t.i32("skip3");
			tool.skip4 = t.i32("skip4");

			const bool hasEquip = d.kind != TemplateDef::OuterObj;
			tool.variations.clear();
			d.equip.variations.clear();
			const std::vector<Json> & vars = in.list("variations", 4);
			if (in.ok() && vars.empty()) in.fail("variations", "at least one variation is required");
			for (size_t i = 0; i < vars.size() && in.ok(); ++i)
			{
				JIn v = in.item("variations", i, vars[i]);
				ToolVariation tv;
				tv.name = v.text("name");
				tv.helpTitle = v.text("helpTitle");
				tv.help = v.text("help");
				tv.price = v.i32("price");
				tv.priceString = v.text("priceString");
				tv.outMoney = v.i32("outMoney");
				tv.outMoneyString = v.text("outMoneyString");
				tv.consumptionPower = v.i32("consumptionPower");
				tv.skip5 = v.i32("skip5");
				tv.skip6 = v.i32("skip6");
				tv.skip7 = v.i32("skip7");
				tool.variations.push_back(tv);
				if (hasEquip)
				{
					EquipVariation ev;
					ev.outSpan = v.i32("outSpan");
					ev.skip9 = v.i32("skip9");
					ev.skip10 = v.i32("skip10");
					d.equip.variations.push_back(ev);
				}
			}
			if (hasEquip)
			{
				JIn e = in.sub("equip");
				d.equip.capacity = e.i32("capacity");
				d.equip.stress = e.i32("stress");
				d.equip.inMoney = e.ints("inMoney");
				if (e.ok() && d.equip.inMoney.size() > 4) e.fail("inMoney", "at most 4 entries");
				d.equip.inSpan = e.i32("inSpan");
				d.equip.buildDays = e.i32("buildDays");
				d.equip.skip8 = e.i32("skip8");
			}
			if (d.kind == TemplateDef::Tenant)
			{
				JIn n = in.sub("tenant");
				TenantDefPart & p = d.tenant;
				p.registID = n.u32("registID");
				p.downFlag = n.u32("downFlag");
				p.numOfKind = n.i32("numOfKind");
				p.numOfPattern = n.i32("numOfPattern");
				p.belongCapacity = n.i32("belongCapacity");
				p.senseOfNoise = n.i32("senseOfNoise");
				p.entranceV = n.i32("entranceV");
				p.entranceH = n.i32("entranceH");
				p.exitV = n.i32("exitV");
				p.exitH = n.i32("exitH");
				p.openTime = n.i32("openTime");
				p.closeTime = n.i32("closeTime");
				p.bindTime = n.i32("bindTime");
				p.maxBuildCount = n.i32("maxBuildCount");
				p.movieName = n.text("movieName");
				p.skip11 = n.u32("skip11");
				p.skip12 = n.u32("skip12");
				p.skip13 = n.u32("skip13");
			}
			if (d.kind == TemplateDef::Mover)
			{
				JIn m = in.sub("mover");
				MoverDefPart & p = d.mover;
				p.addSize = m.rect("addSize");
				p.maxLength = m.i32("maxLength");
				p.minLength = m.i32("minLength");
				p.maxStop = m.i32("maxStop");
				p.score = m.i32("score");
				p.defaultWait = m.i32("defaultWait");
				const std::vector<int32_t> sp = m.ints("speed", 3);
				for (size_t i = 0; i < sp.size() && i < 3; ++i) p.speed[i] = sp[i];
				p.maxModule = m.i32("maxModule");
				p.moduleInset = m.rect("moduleInset");
				p.moduleUnitH = m.i32("moduleUnitH");
				p.moduleUnitV = m.i32("moduleUnitV");
				p.moduleParam1 = m.i32("moduleParam1");
				p.moduleParam2 = m.i32("moduleParam2");
				p.skip14 = m.i32("skip14");
				p.skip15 = m.i32("skip15");
				p.skip16 = m.i32("skip16");
				p.skip17 = m.i32("skip17");
			}
			if (d.kind == TemplateDef::OuterObj)
			{
				JIn o = in.sub("outer");
				d.unreadA = o.i32("unreadA");
				d.unreadB = o.i32("unreadB");
			}
		}

		// -----------------------------------------------------------------
		// TMDF / DPDF / PLDF / PADF
		// -----------------------------------------------------------------
		Json memberTableToJson(const MemberTableDef & d)
		{
			Json j = Json::object();
			j.set("econoType", Json(int32_t(d.econoType))).set("specialFlag", Json(d.specialFlag))
			 .set("dayParamID", Json(d.dayParamID)).set("placeParamID", Json(d.placeParamID));
			Json a = Json::array();
			for (const MemberDef & m : d.members)
			{
				Json o = Json::object();
				o.set("name", str(m.name)).set("demandType", Json(m.demandType))
				 .set("startSilhouetteType", Json(m.startSilhouetteType)).set("endSilhouetteType", Json(m.endSilhouetteType))
				 .set("optionType", Json(m.optionType)).set("percent", Json(m.percent)).set("numOfPeople", Json(m.numOfPeople));
				a.push(std::move(o));
			}
			j.set("members", std::move(a));
			return j;
		}

		void memberTableFromJson(JIn & in, MemberTableDef & d)
		{
			d.econoType = in.u32s("econoType");
			d.specialFlag = in.u32("specialFlag");
			d.dayParamID = in.i32("dayParamID");
			d.placeParamID = in.i32("placeParamID");
			d.members.clear();
			const std::vector<Json> & a = in.list("members", 64);
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				JIn m = in.item("members", i, a[i]);
				MemberDef md;
				md.name = m.text("name", 15);
				md.demandType = m.i32("demandType");
				md.startSilhouetteType = m.i32("startSilhouetteType");
				md.endSilhouetteType = m.i32("endSilhouetteType");
				md.optionType = m.i32("optionType");
				md.percent = m.u32("percent");
				md.numOfPeople = m.u32("numOfPeople");
				d.members.push_back(md);
			}
		}

		Json dayParamToJson(const DayParamDef & d)
		{
			Json j = Json::object();
			j.set("weekdayFine", Json(d.percent[0][0])).set("weekdayRain", Json(d.percent[0][1]))
			 .set("holidayFine", Json(d.percent[1][0])).set("holidayRain", Json(d.percent[1][1]));
			return j;
		}

		void dayParamFromJson(JIn & in, DayParamDef & d)
		{
			d.percent[0][0] = in.u32("weekdayFine");
			d.percent[0][1] = in.u32("weekdayRain");
			d.percent[1][0] = in.u32("holidayFine");
			d.percent[1][1] = in.u32("holidayRain");
		}

		const char * const kPlaceKeys[3] = {"floor", "tenant", "pool"};

		Json placeParamToJson(const PlaceParamDef & d)
		{
			Json j = Json::object();
			for (int i = 0; i < 3; ++i)
			{
				Json o = Json::object();
				o.set("percent", Json(d.places[i].percent)).set("score", Json(d.places[i].scoreSlot));
				j.set(kPlaceKeys[i], std::move(o));
			}
			return j;
		}

		void placeParamFromJson(JIn & in, PlaceParamDef & d)
		{
			for (int i = 0; i < 3; ++i)
			{
				JIn o = in.sub(kPlaceKeys[i]);
				d.places[i].percent = o.u32("percent");
				d.places[i].scoreSlot = o.i32("score");
			}
		}

		Json peopleAnimeToJson(const PeopleAnimeDef & d)
		{
			Json j = Json::object();
			j.set("param1", Json(d.param1)).set("param2", Json(d.param2));
			Json a = Json::array();
			for (const PeopleAnimeDef::Elem & e : d.elems)
			{
				Json o = Json::object();
				o.set("position", Json(e.position)).set("pattern", Json(e.pattern))
				 .set("silhouette", Json(e.silhouette)).set("animeID", Json(e.animeID));
				a.push(std::move(o));
			}
			j.set("elems", std::move(a));
			return j;
		}

		void peopleAnimeFromJson(JIn & in, PeopleAnimeDef & d)
		{
			d.param1 = in.i32("param1");
			d.param2 = in.i32("param2");
			d.elems.clear();
			const std::vector<Json> & a = in.list("elems", 1000);
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				JIn o = in.item("elems", i, a[i]);
				PeopleAnimeDef::Elem e;
				e.position = o.i32("position");
				e.pattern = o.i32("pattern");
				e.silhouette = o.i32("silhouette");
				e.animeID = o.i32("animeID");
				d.elems.push_back(e);
			}
		}

		// -----------------------------------------------------------------
		// SLDF / PPDF / WODF / STRTABLE
		// -----------------------------------------------------------------
		Json silhouettesToJson(const SilhouetteList & l)
		{
			Json a = Json::array();
			for (const SilhouetteDef & s : l)
			{
				Json o = Json::object();
				o.set("silhouetteType", Json(s.silhouetteType)).set("width", Json(s.width))
				 .set("routeType", Json(s.routeType)).set("bitmapID", Json(s.bitmapID)).set("name", str(s.name));
				a.push(std::move(o));
			}
			return Json::object().set("entries", std::move(a));
		}

		void silhouettesFromJson(JIn & in, SilhouetteList & l)
		{
			l.clear();
			const std::vector<Json> & a = in.list("entries");
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				JIn o = in.item("entries", i, a[i]);
				SilhouetteDef s;
				s.silhouetteType = o.i32("silhouetteType");
				s.width = o.i32("width");
				s.routeType = o.i32("routeType");
				s.bitmapID = o.i32("bitmapID");
				s.name = o.text("name");
				l.push_back(s);
			}
		}

		Json peopleToJson(const PeopleList & l)
		{
			Json a = Json::array();
			for (const PeopleDef & p : l)
			{
				Json o = Json::object();
				o.set("peopleType", Json(p.peopleType)).set("attribute", Json(p.attribute))
				 .set("silhouetteType", Json(p.silhouetteType)).set("stressMax", Json(p.stressMax))
				 .set("stressBlueLimit", Json(p.stressBlueLimit)).set("stressYellowLimit", Json(p.stressYellowLimit));
				a.push(std::move(o));
			}
			return Json::object().set("entries", std::move(a));
		}

		void peopleFromJson(JIn & in, PeopleList & l)
		{
			l.clear();
			const std::vector<Json> & a = in.list("entries");
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				JIn o = in.item("entries", i, a[i]);
				PeopleDef p;
				p.peopleType = o.i32("peopleType");
				p.attribute = o.i32("attribute");
				p.silhouetteType = o.i32("silhouetteType");
				p.stressMax = o.i32("stressMax");
				p.stressBlueLimit = o.i32("stressBlueLimit");
				p.stressYellowLimit = o.i32("stressYellowLimit");
				l.push_back(p);
			}
		}

		Json wordsToJson(const WordList & l)
		{
			Json a = Json::array();
			for (const WordDef & w : l)
			{
				Json o = Json::object();
				o.set("text", str(w.text)).set("sexMask", Json(w.sexMask)).set("ageMask", Json(w.ageMask))
				 .set("flags", Json(w.flags)).set("special", Json(w.special));
				a.push(std::move(o));
			}
			return Json::object().set("words", std::move(a));
		}

		void wordsFromJson(JIn & in, WordList & l)
		{
			l.clear();
			const std::vector<Json> & a = in.list("words");
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				JIn o = in.item("words", i, a[i]);
				WordDef w;
				w.text = o.text("text");
				w.sexMask = o.u32("sexMask");
				w.ageMask = o.u32("ageMask");
				w.flags = o.u32("flags");
				w.special = o.u32("special");
				l.push_back(w);
			}
		}

		Json stringTableToJson(const StringTableDef & l)
		{
			Json a = Json::array();
			for (const std::string & s : l) a.push(str(s));
			return Json::object().set("strings", std::move(a));
		}

		void stringTableFromJson(JIn & in, StringTableDef & l)
		{
			l.clear();
			const std::vector<Json> & a = in.list("strings");
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				std::string bytes;
				if (!a[i].isString() || !utf8ToLatin1(a[i].asString(), bytes) || bytes.size() > 255)
				{
					in.fail("strings[" + std::to_string(i) + "]", "expected a Latin-1 string (<= 255 bytes)");
					break;
				}
				l.push_back(bytes);
			}
		}

		// -----------------------------------------------------------------
		// SP sub-plugins
		// -----------------------------------------------------------------
		Json movieToJson(const MovieDef & d)
		{
			Json j = Json::object();
			Json extra = Json::array();
			for (const std::string & s : d.extra) extra.push(str(s));
			j.set("title", str(d.header.title)).set("tieupDate", Json(d.header.tieupDate))
			 .set("imageID", Json(d.imageID)).set("filmKind", Json(d.filmKind))
			 .set("phase1Months", Json(d.phase1Months)).set("phase2Months", Json(d.phase2Months))
			 .set("phase3Months", Json(d.phase3Months)).set("phase4Months", Json(d.phase4Months))
			 .set("cycleMonths", Json(d.cycleMonths)).set("production", str(d.production))
			 .set("director", str(d.director)).set("starring", str(d.starring))
			 .set("runningTime", Json(d.runningTime)).set("unkFC", Json(d.unkFC))
			 .set("description", str(d.description)).set("customerTableID", Json(d.customerTableID))
			 .set("unk104", Json(d.unk104)).set("unk108", Json(d.unk108))
			 .set("famousAudience", Json(d.famousAudience)).set("remakeAudience", Json(d.remakeAudience))
			 .set("unk114", Json(d.unk114)).set("extra", std::move(extra))
			 .set("stillImageID", Json(d.stillImageID)).set("clipSeconds", Json(d.clipSeconds))
			 .set("movieFile", str(d.movieFile));
			return j;
		}

		void movieFromJson(JIn & in, MovieDef & d)
		{
			d.header.title = in.text("title");
			d.header.tieupDate = in.u32("tieupDate");
			d.imageID = in.i32("imageID");
			d.filmKind = in.i32("filmKind");
			d.phase1Months = in.i32("phase1Months");
			d.phase2Months = in.i32("phase2Months");
			d.phase3Months = in.i32("phase3Months");
			d.phase4Months = in.i32("phase4Months");
			d.cycleMonths = in.i32("cycleMonths");
			d.production = in.text("production");
			d.director = in.text("director");
			d.starring = in.text("starring");
			d.runningTime = in.i32("runningTime");
			d.unkFC = in.i32("unkFC");
			d.description = in.text("description");
			d.customerTableID = in.i32("customerTableID");
			d.unk104 = in.i32("unk104");
			d.unk108 = in.i32("unk108");
			d.famousAudience = in.i32("famousAudience");
			d.remakeAudience = in.i32("remakeAudience");
			d.unk114 = in.i32("unk114");
			d.extra.clear();
			const std::vector<Json> & a = in.list("extra", 64);
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				std::string bytes;
				if (!a[i].isString() || !utf8ToLatin1(a[i].asString(), bytes) || bytes.size() > 255)
				{
					in.fail("extra[" + std::to_string(i) + "]", "expected a Latin-1 string");
					break;
				}
				d.extra.push_back(bytes);
			}
			d.stillImageID = in.i32("stillImageID");
			d.clipSeconds = in.i32("clipSeconds");
			d.movieFile = in.text("movieFile");
		}

		Json hallEventToJson(const HallEventDef & d)
		{
			Json j = Json::object();
			j.set("title", str(d.header.title)).set("tieupDate", Json(d.header.tieupDate))
			 .set("imageID", Json(d.imageID)).set("unkCC", Json(d.unkCC)).set("bookingDays", Json(d.bookingDays))
			 .set("openTime", Json(d.openTime)).set("closeTime", Json(d.closeTime)).set("monthMask", Json(d.monthMask))
			 .set("cost", Json(d.cost)).set("customerTableID", Json(d.customerTableID))
			 .set("unkDC", Json(d.unkDC)).set("unkE0", Json(d.unkE0)).set("unkE4", Json(d.unkE4))
			 .set("bindTime", Json(d.bindTime)).set("stillImageID", Json(d.stillImageID))
			 .set("description", str(d.description)).set("movieFile", str(d.movieFile));
			return j;
		}

		void hallEventFromJson(JIn & in, HallEventDef & d)
		{
			d.header.title = in.text("title");
			d.header.tieupDate = in.u32("tieupDate");
			d.imageID = in.i32("imageID");
			d.unkCC = in.u32("unkCC");
			d.bookingDays = in.u32("bookingDays");
			d.openTime = in.u32("openTime");
			d.closeTime = in.u32("closeTime");
			d.monthMask = in.u32("monthMask");
			d.cost = in.i32("cost");
			d.customerTableID = in.i32("customerTableID");
			d.unkDC = in.i32("unkDC");
			d.unkE0 = in.i32("unkE0");
			d.unkE4 = in.i32("unkE4");
			d.bindTime = in.i32("bindTime");
			d.stillImageID = in.i32("stillImageID");
			d.description = in.text("description");
			d.movieFile = in.text("movieFile");
		}

		Json advertiseToJson(const AdvertiseDef & d)
		{
			Json j = Json::object();
			j.set("title", str(d.header.title)).set("tieupDate", Json(d.header.tieupDate))
			 .set("subType", fourccToJson(d.subType)).set("imageID", Json(d.imageID)).set("unkD0", Json(d.unkD0))
			 .set("days", Json(d.days)).set("money", Json(d.money)).set("path", str(d.path));
			return j;
		}

		void advertiseFromJson(JIn & in, AdvertiseDef & d)
		{
			d.header.title = in.text("title");
			d.header.tieupDate = in.u32("tieupDate");
			d.subType = in.tag("subType");
			d.imageID = in.i32("imageID");
			d.unkD0 = in.u32("unkD0");
			d.days = in.u32("days");
			d.money = in.i32("money");
			d.path = in.text("path");
		}

		// -----------------------------------------------------------------
		// RT_STRING / RT_VERSION
		// -----------------------------------------------------------------
		Json stringBlockToJson(const StringBlock & b)
		{
			Json a = Json::array();
			for (const std::u16string & s : b.strings) a.push(Json(utf16ToWtf8(s)));
			Json j = Json::object();
			j.set("strings", std::move(a));
			if (!b.trailing.empty()) j.set("trailingHex", Json(toHex(b.trailing)));
			return j;
		}

		void stringBlockFromJson(JIn & in, StringBlock & b)
		{
			b.strings.clear();
			const std::vector<Json> & a = in.list("strings", 16);
			for (size_t i = 0; i < a.size() && in.ok(); ++i)
			{
				std::u16string s;
				if (!a[i].isString() || !wtf8ToUtf16(a[i].asString(), s))
				{
					in.fail("strings[" + std::to_string(i) + "]", "expected a string");
					break;
				}
				b.strings.push_back(std::move(s));
			}
			b.trailing = in.hex("trailingHex");
		}

		Json versionNodeToJson(const VersionNode & n)
		{
			Json j = Json::object();
			j.set("key", Json(utf16ToWtf8(n.key)));
			if (n.type == 1) j.set("text", Json(n.textValue()));  // informational; "value" is authoritative
			j.set("length", Json(n.length)).set("valueLength", Json(n.valueLength)).set("type", Json(n.type));
			j.set("value", Json(toHex(n.value)));
			if (!n.pad1.empty()) j.set("pad1", Json(toHex(n.pad1)));
			if (!n.pad2.empty()) j.set("pad2", Json(toHex(n.pad2)));
			if (!n.padAfter.empty()) j.set("padAfter", Json(toHex(n.padAfter)));
			if (!n.extra.empty()) j.set("extra", Json(toHex(n.extra)));
			if (!n.children.empty())
			{
				Json a = Json::array();
				for (const VersionNode & c : n.children) a.push(versionNodeToJson(c));
				j.set("children", std::move(a));
			}
			return j;
		}

		void versionNodeFromJson(JIn & in, VersionNode & n, int depth)
		{
			if (depth > 16) { in.fail("", "nesting too deep"); return; }
			const Json & key = in.raw("key");
			if (!key.isString() || !wtf8ToUtf16(key.asString(), n.key)) in.fail("key", "expected a string");
			n.length = uint16_t(in.integer("length", 0, 0xFFFF, "expected u16"));
			n.valueLength = uint16_t(in.integer("valueLength", 0, 0xFFFF, "expected u16"));
			n.type = uint16_t(in.integer("type", 0, 0xFFFF, "expected u16"));
			n.value = in.hex("value");
			n.pad1 = in.hex("pad1");
			n.pad2 = in.hex("pad2");
			n.padAfter = in.hex("padAfter");
			n.extra = in.hex("extra");
			n.children.clear();
			if (in.has("children"))
			{
				const std::vector<Json> & a = in.list("children", 1000);
				for (size_t i = 0; i < a.size() && in.ok(); ++i)
				{
					JIn c = in.item("children", i, a[i]);
					VersionNode child;
					versionNodeFromJson(c, child, depth + 1);
					n.children.push_back(std::move(child));
				}
			}
		}
	}

	Json resourceToJson(const DecodedResource & r)
	{
		switch (r.codec)
		{
			case ResourceCodec::Raw:
			case ResourceCodec::Text:          return Json();
			case ResourceCodec::Attr:          return attrToJson(std::get<PluginAttr>(r.value));
			case ResourceCodec::Desc:          return descToJson(std::get<PluginDesc>(r.value));
			case ResourceCodec::TemplateDef:   return templateToJson(std::get<TemplateDef>(r.value));
			case ResourceCodec::MemberTable:   return memberTableToJson(std::get<MemberTableDef>(r.value));
			case ResourceCodec::DayParam:      return dayParamToJson(std::get<DayParamDef>(r.value));
			case ResourceCodec::PlaceParam:    return placeParamToJson(std::get<PlaceParamDef>(r.value));
			case ResourceCodec::PeopleAnime:   return peopleAnimeToJson(std::get<PeopleAnimeDef>(r.value));
			case ResourceCodec::Silhouettes:   return silhouettesToJson(std::get<SilhouetteList>(r.value));
			case ResourceCodec::People:        return peopleToJson(std::get<PeopleList>(r.value));
			case ResourceCodec::Words:         return wordsToJson(std::get<WordList>(r.value));
			case ResourceCodec::StringTable:   return stringTableToJson(std::get<StringTableDef>(r.value));
			case ResourceCodec::Movie:         return movieToJson(std::get<MovieDef>(r.value));
			case ResourceCodec::HallEvent:     return hallEventToJson(std::get<HallEventDef>(r.value));
			case ResourceCodec::Advertise:     return advertiseToJson(std::get<AdvertiseDef>(r.value));
			case ResourceCodec::SubPluginPeopleParam:
			{
				Json a = Json::array();
				for (int32_t v : std::get<SubPluginPeopleParamDef>(r.value).values) a.push(Json(v));
				return Json::object().set("values", std::move(a));
			}
			case ResourceCodec::StringBlock:   return stringBlockToJson(std::get<StringBlock>(r.value));
			case ResourceCodec::VersionInfo:
			{
				const VersionInfo & v = std::get<VersionInfo>(r.value);
				Json j = Json::object();
				j.set("root", versionNodeToJson(v.root));
				if (!v.trailing.empty()) j.set("trailingHex", Json(toHex(v.trailing)));
				return j;
			}
			case ResourceCodec::WorldRecord:   return std::get<Json>(r.value);
			case ResourceCodec::BkgndInfo:     return bkgndInfoToJson(std::get<BkgndInfoDef>(r.value));
			case ResourceCodec::FloorInfoMask: return floorInfoMaskToJson(std::get<FloorInfoMaskDef>(r.value));
			case ResourceCodec::PaletteAnime:  return paletteAnimeToJson(std::get<PaletteAnimeDef>(r.value));
			case ResourceCodec::Palette:       return paletteToJson(std::get<PaletteDef>(r.value));
		}
		return Json();
	}

	bool resourceFromJson(const Json & j, DecodedResource & r, std::string * error)
	{
		JState st;
		auto finish = [&](auto && value) -> bool {
			if (!st.ok)
			{
				if (error) *error = st.error;
				return false;
			}
			r.value = std::move(value);
			// The value must be encodable (string lengths, count limits).
			std::string probe;
			if (!encodeResource(r, probe))
			{
				if (error) *error = "the value cannot be encoded (a string over 255 bytes or a count mismatch)";
				return false;
			}
			return true;
		};
		switch (r.codec)
		{
			case ResourceCodec::Raw:
			case ResourceCodec::Text:
				if (error) *error = "raw and text resources have no Json form";
				return false;
			case ResourceCodec::Attr: { JIn in(j, "", st); PluginAttr v; attrFromJson(in, v); return finish(v); }
			case ResourceCodec::Desc: { JIn in(j, "", st); PluginDesc v; descFromJson(in, v); return finish(v); }
			case ResourceCodec::TemplateDef:
			{
				JIn in(j, "", st);
				TemplateDef v;
				if (std::holds_alternative<TemplateDef>(r.value)) v = std::get<TemplateDef>(r.value);
				templateFromJson(in, resourceTypeDisplayName(r.type), v);
				return finish(v);
			}
			case ResourceCodec::MemberTable: { JIn in(j, "", st); MemberTableDef v; memberTableFromJson(in, v); return finish(v); }
			case ResourceCodec::DayParam:    { JIn in(j, "", st); DayParamDef v; dayParamFromJson(in, v); return finish(v); }
			case ResourceCodec::PlaceParam:  { JIn in(j, "", st); PlaceParamDef v; placeParamFromJson(in, v); return finish(v); }
			case ResourceCodec::PeopleAnime: { JIn in(j, "", st); PeopleAnimeDef v; peopleAnimeFromJson(in, v); return finish(v); }
			case ResourceCodec::Silhouettes: { JIn in(j, "", st); SilhouetteList v; silhouettesFromJson(in, v); return finish(v); }
			case ResourceCodec::People:      { JIn in(j, "", st); PeopleList v; peopleFromJson(in, v); return finish(v); }
			case ResourceCodec::Words:       { JIn in(j, "", st); WordList v; wordsFromJson(in, v); return finish(v); }
			case ResourceCodec::StringTable: { JIn in(j, "", st); StringTableDef v; stringTableFromJson(in, v); return finish(v); }
			case ResourceCodec::Movie:       { JIn in(j, "", st); MovieDef v; movieFromJson(in, v); return finish(v); }
			case ResourceCodec::HallEvent:   { JIn in(j, "", st); HallEventDef v; hallEventFromJson(in, v); return finish(v); }
			case ResourceCodec::Advertise:   { JIn in(j, "", st); AdvertiseDef v; advertiseFromJson(in, v); return finish(v); }
			case ResourceCodec::SubPluginPeopleParam:
			{
				JIn in(j, "", st);
				SubPluginPeopleParamDef v;
				const std::vector<int32_t> a = in.ints("values", 6);
				for (size_t i = 0; i < a.size() && i < 6; ++i) v.values[i] = a[i];
				return finish(v);
			}
			case ResourceCodec::StringBlock: { JIn in(j, "", st); StringBlock v; stringBlockFromJson(in, v); return finish(v); }
			case ResourceCodec::VersionInfo:
			{
				JIn in(j, "", st);
				VersionInfo v;
				JIn root = in.sub("root");
				versionNodeFromJson(root, v.root, 0);
				v.trailing = in.hex("trailingHex");
				return finish(v);
			}
			case ResourceCodec::WorldRecord:
			{
				WorldSchema ws = WorldSchema::Event;
				std::string bytes, err;
				if (!worldSchemaForType(r.type, ws) || !encodeWorldSchemaResource(ws, r.type, j, bytes, &err))
				{
					if (error) *error = err.empty() ? "not a world record" : err;
					return false;
				}
				r.value = j;
				return true;
			}
			case ResourceCodec::BkgndInfo:
			{
				BkgndInfoDef v;
				if (!bkgndInfoFromJson(j, v, error)) return false;
				return finish(v);
			}
			case ResourceCodec::FloorInfoMask:
			{
				FloorInfoMaskDef v;
				if (!floorInfoMaskFromJson(j, v, error)) return false;
				return finish(v);
			}
			case ResourceCodec::PaletteAnime:
			{
				PaletteAnimeDef v;
				if (!paletteAnimeFromJson(j, v, error)) return false;
				return finish(v);
			}
			case ResourceCodec::Palette:
			{
				PaletteDef v;
				if (!paletteFromJson(j, v, error)) return false;
				return finish(v);
			}
		}
		if (error) *error = "unknown codec";
		return false;
	}
}
