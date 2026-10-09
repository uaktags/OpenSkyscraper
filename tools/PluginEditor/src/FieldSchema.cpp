/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "FieldSchema.h"

#include <cctype>
#include <climits>
#include <map>

namespace editors
{
	namespace
	{
		using Opt = std::vector<std::pair<int64_t, std::string>>;
		using Bits = std::vector<std::pair<uint32_t, std::string>>;

		/// Fluent builder for one table entry.
		struct M
		{
			FieldMeta m;
			M(const char * label, const char * tip = "") { m.label = label; m.tip = tip; }
			M & range(int64_t lo, int64_t hi) { m.hasRange = true; m.min = lo; m.max = hi; return *this; }
			M & opts(Opt o) { m.options = std::move(o); return *this; }
			M & tags(std::vector<std::string> t) { m.tagOptions = std::move(t); return *this; }
			M & bits(Bits b) { m.bits = std::move(b); return *this; }
			M & adv() { m.advanced = true; return *this; }
			M & ro() { m.readOnly = true; return *this; }
			M & minutes() { m.minutes = true; m.hasRange = true; m.min = 0; m.max = 1440; return *this; }
			M & flag() { m.boolean = true; m.hasRange = true; m.min = 0; m.max = 1; return *this; }
			M & text() { m.multiline = true; return *this; }
			M & var(size_t maxItems = 0) { m.variableList = true; m.maxItems = maxItems; return *this; }
		};

		const Opt kCategory = {
			{10, "Basic"}, {20, "Transit Systems"}, {30, "Residential"}, {40, "Office"}, {50, "Restaurants"},
			{60, "Hotel"}, {70, "Bldg. Facilities"}, {80, "Shops"}, {90, "Entertainment"}, {100, "Special"},
			{200, "Outside"},
		};
		const Opt kRegist = {
			{0, "none"}, {1, "hotel room"}, {2, "trash"}, {3, "parking"}, {4, "doctor"}, {5, "security / fire"},
			{6, "school"}, {7, "restroom"}, {8, "subway"}, {9, "boat"}, {10, "ticket"},
		};
		const Bits kToolAttr = {
			{0x2, "mover (IsMover)"}, {0x80, "drag making"}, {0x100, "pays maintenance"},
			{0x2000, "floor type / stair"}, {0x4000, "variable length"}, {0x8000, "home"},
			{0x20000, "concat"}, {0x800000, "people anime"},
		};
		const Bits kWorldMask = {
			{0x1, "Tokyo (0x1)"}, {0x2, "Tokyo (0x2)"}, {0x4, "Tokyo (0x4)"}, {0x10, "Kegon (0x10)"},
			{0x20, "Kegon (0x20)"}, {0x40, "Kegon (0x40)"}, {0x80, "Kegon (0x80)"}, {0x100, "Hawaii (0x100)"},
			{0x200, "Hawaii (0x200)"}, {0x400, "Hawaii (0x400)"}, {0x800, "Hawaii (0x800)"},
			{0x1000000, "bit 24 (0x1000000)"},
		};
		const Bits kGrades = {
			{0x1, "grade 1"}, {0x2, "grade 2"}, {0x4, "grade 3"}, {0x8, "grade 4"}, {0x10, "grade 5"},
			{0x20, "grade 6"}, {0x40, "grade 7"}, {0x80, "grade 8"},
		};
		const Bits kMonths = {
			{0x1, "Jan"}, {0x2, "Feb"}, {0x4, "Mar"}, {0x8, "Apr"}, {0x10, "May"}, {0x20, "Jun"},
			{0x40, "Jul"}, {0x80, "Aug"}, {0x100, "Sep"}, {0x200, "Oct"}, {0x400, "Nov"}, {0x800, "Dec"},
		};
		const std::vector<std::string> kSubPlugin = {"", "MoPI", "HePI", "BoPI"};
		const std::vector<std::string> kClassTags = {"TnPl", "MvPl", "OoPl", "TOPL", "WdPl", "MoPl", "HePl", "AdPl"};

		std::map<std::string, FieldMeta> build()
		{
			std::map<std::string, FieldMeta> t;
			auto put = [&](const std::string & k, const M & m) { t[k] = m.m; };
			const int64_t I = INT32_MAX;

			// ---- ATTR / DESC (all classes) ----
			put("Attr:pluginId", M("Plugin ID (mID)", "Unique 4-character plugin id, Mac order (BURG = Burger).\nSave files reference the plugin by it."));
			put("Attr:name", M("Display name", "mPluginName: the name in the toolbox and inspector."));
			put("Attr:itemType", M("Item type (mItemType)", "Numeric item type; must equal the definition's toolType\n(Floor 1000, Office 1200, Burger 1301). Worlds: sort key 0/10/20.").range(0, 100000));
			put("Attr:gameLevel", M("Stars required (mGameLevel)", "Tower star level that unlocks the item.").range(1, 6));
			put("Attr:attrib", M("Worlds (mAttrib)", "World-availability mask: the item shows in a world when\nworld.mAttrib & attrib is non-zero.").bits(kWorldMask));
			put("Attr:dependPluginId", M("Requires plugin (mDependID)", "\"****\" = none; FRON = needs a hotel Front desk.").tags({"****", "FRON"}));
			put("Attr:mustItem", M("Always available (mMustItem)").flag());
			put("Attr:secretItem", M("Secret item (mSecretItem)", "Hidden unless secret mode is on.").flag());
			put("Attr:version", M("Lead byte", "Binary-form marker (always 1).").adv());
			put("Attr:textForm", M("Text form", "DefaultToolPalette stores ATTR as text.").adv().ro());
			put("Attr:text", M("Original text", "Kept verbatim while it still parses to the same values.").adv().text());
			put("Desc:classTag", M("Plugin class (mType)", "Fixed by the plugin class.").tags(kClassTags));
			put("Desc:version", M("Loader version (mVersion)", "T2PluginLoader only accepts 3.").range(3, 3));

			// ---- TNDF / MVDF / OODF ----
			put("TemplateDef:kind", M("Definition kind").ro());
			put("TemplateDef:tool.toolType", M("Tool type (mToolType)", "== ATTR mItemType; prefix of the sound ids.").range(0, 100000));
			put("TemplateDef:tool.width", M("Width (units)", "Footprint width in 8-px units.").range(1, 400));
			put("TemplateDef:tool.height", M("Height (floors)", "Footprint height in floors.").range(1, 20));
			put("TemplateDef:tool.level", M("Level (mLevel)", "Equals ATTR mGameLevel; no reader found.").range(0, 6));
			put("TemplateDef:tool.attribute", M("Attributes (mAttribute)", "Behaviour bits proven in T2DLL.").bits(kToolAttr));
			put("TemplateDef:tool.categoryName", M("Category name", "Toolbox group title (\"Shops\")."));
			put("TemplateDef:tool.categoryHelp", M("Category help", "Category description.").text());
			put("TemplateDef:tool.category", M("Toolbox category", "Toolbox group; sort key = category << 16 | toolNo.").opts(kCategory));
			put("TemplateDef:tool.toolNo", M("Order in group (mToolNo)").range(0, 100000));
			put("TemplateDef:tool.subPluginType", M("Sub-plugins (mSubPluginType)", "Which SP plugins attach: MoPI movies, HePI hall events, BoPI ads.").tags(kSubPlugin));
			put("TemplateDef:variations", M("Variations", "1 to 4 build variations (ToolDef + EquipDef records).").var(4));
			put("TemplateDef:variations[].name", M("Name", "GetToolName: used in messages."));
			put("TemplateDef:variations[].helpTitle", M("Help title"));
			put("TemplateDef:variations[].help", M("Help text").text());
			put("TemplateDef:variations[].price", M("Price ($100)", "Construction cost in $100 units (Floor: per unit).").range(0, 10000000));
			put("TemplateDef:variations[].priceString", M("Price text", "Help \"Cons. Cost\" text, e.g. $200000."));
			put("TemplateDef:variations[].outMoney", M("Maintenance ($100)", "Per period, in $100 units.").range(0, 10000000));
			put("TemplateDef:variations[].outMoneyString", M("Maintenance text", "Help \"Maint. Cost\" text, e.g. $800/M."));
			put("TemplateDef:variations[].consumptionPower", M("Power use", "GetConsumptionPower.").range(0, 100000));
			put("TemplateDef:variations[].outSpan", M("Out span", "EquipDef +0x154 (the last variation wins); 0 sets a flag.").range(0, 100000));
			put("TemplateDef:equip.capacity", M("Capacity", "Customers per unit / car capacity.").range(0, 100000));
			put("TemplateDef:equip.stress", M("Stress", "No reader found (name from TENANTDEF).").range(0, 1000));
			put("TemplateDef:equip.inMoney", M("Income per price setting ($100)", "GetInMoney(i); up to 4 price settings.").var(4));
			put("TemplateDef:equip.inSpan", M("In span", "No reader found.").adv());
			put("TemplateDef:equip.buildDays", M("Build days", "No reader found.").adv());
			put("TemplateDef:equip.skip8", M("skip8", "Discarded; normally the variation count.").adv());
			put("TemplateDef:tenant.registID", M("Registry class (mRegistID)").opts(kRegist));
			put("TemplateDef:tenant.downFlag", M("Down flag", "GetDownFlag.").range(0, 4));
			put("TemplateDef:tenant.numOfKind", M("Sprite kinds", "Keep consistent with OBJMAP 1000.").range(1, 64));
			put("TemplateDef:tenant.numOfPattern", M("Patterns per kind").range(1, 64));
			put("TemplateDef:tenant.belongCapacity", M("Residents / staff").range(0, 1000));
			put("TemplateDef:tenant.senseOfNoise", M("Noise sensitivity").range(0, 1000));
			put("TemplateDef:tenant.entranceV", M("Entrance row").range(0, 20));
			put("TemplateDef:tenant.entranceH", M("Entrance column").range(0, 400));
			put("TemplateDef:tenant.exitV", M("Exit row").range(0, 20));
			put("TemplateDef:tenant.exitH", M("Exit column").range(0, 400));
			put("TemplateDef:tenant.openTime", M("Opens at", "Minute of day.").minutes());
			put("TemplateDef:tenant.closeTime", M("Closes at", "Minute of day.").minutes());
			put("TemplateDef:tenant.bindTime", M("Stay length (min)", "GetBindTime; customer-unit divisor.").range(0, 1440));
			put("TemplateDef:tenant.maxBuildCount", M("Max count (0 = no limit)").range(0, 10000));
			put("TemplateDef:tenant.movieName", M("Movie tag", "QuickTime movie tag (\"\" / \"[QT]:OF:\")."));
			put("TemplateDef:mover.addSize", M("Shaft margin (mAddSize)"));
			put("TemplateDef:mover.maxLength", M("Max floors (0 = unlimited)").range(0, 100000));
			put("TemplateDef:mover.minLength", M("Min floors").range(0, 1000));
			put("TemplateDef:mover.maxStop", M("Max stops (express)").range(0, 1000));
			put("TemplateDef:mover.score", M("Route score").range(0, 1000));
			put("TemplateDef:mover.defaultWait", M("Default wait").range(0, 1000));
			put("TemplateDef:mover.speed", M("Speeds [3]", "GetSpeed(0..2)."));
			put("TemplateDef:mover.maxModule", M("Max cars").range(0, 100));
			put("TemplateDef:mover.moduleInset", M("Car inset (mModuleInset)"));
			put("TemplateDef:mover.moduleUnitH", M("Module unit H", "No known consumer (T2DLL, T2.exe, mover code).").ro());
			put("TemplateDef:mover.moduleUnitV", M("Module unit V", "No known consumer.").ro());
			put("TemplateDef:mover.moduleParam1", M("Module param 1", "No known consumer.").ro());
			put("TemplateDef:mover.moduleParam2", M("Module param 2", "No known consumer; tracks the per-car price tier.").ro());
			put("TemplateDef:outer.unreadA", M("unreadA", "Never read by T2OuterObjDef.").adv());
			put("TemplateDef:outer.unreadB", M("unreadB", "Never read by T2OuterObjDef.").adv());

			// ---- TMDF / DPDF / PLDF / PADF / SLDF / PPDF / WODF ----
			put("MemberTable:econoType", M("Economy type", "-1 none; 0..4; bit 4 = check only the first.").range(-1, 0xFF));
			put("MemberTable:specialFlag", M("Special flag").range(0, 0xFFFF));
			put("MemberTable:dayParamID", M("Day table (DPDF id)", "0 = none; normally 128.").range(0, 32767));
			put("MemberTable:placeParamID", M("Place table (PLDF id)", "0 = none; normally 128.").range(0, 32767));
			put("MemberTable:members[].name", M("Name", "At most 15 characters."));
			put("MemberTable:members[].demandType", M("Demand class").range(-1, 16));
			put("MemberTable:members[].startSilhouetteType", M("People type from").range(-100, 1000));
			put("MemberTable:members[].endSilhouetteType", M("People type to").range(-100, 1000));
			put("MemberTable:members[].optionType", M("Option type").range(-1, 100));
			put("MemberTable:members[].percent", M("Percent").range(0, 100));
			put("MemberTable:members[].numOfPeople", M("People").range(0, 1000));
			put("DayParam:weekdayFine", M("Weekday, fine (%)").range(0, 100));
			put("DayParam:weekdayRain", M("Weekday, rain (%)").range(0, 100));
			put("DayParam:holidayFine", M("Holiday, fine (%)").range(0, 100));
			put("DayParam:holidayRain", M("Holiday, rain (%)").range(0, 100));
			for (const char * place : {"floor", "tenant", "pool"})
			{
				put(std::string("PlaceParam:") + place + ".percent", M("Percent").range(0, 100));
				put(std::string("PlaceParam:") + place + ".score", M("Score", "Read as a short by the engine.").range(-32768, 32767));
			}
			put("PeopleAnime:param1", M("Param 1", "Always 1.").adv());
			put("PeopleAnime:param2", M("Param 2", "Always the element count.").range(0, 1000));
			put("PeopleAnime:elems[].position", M("Position").range(-1, 16));
			put("PeopleAnime:elems[].pattern", M("Pattern").range(0, 16));
			put("PeopleAnime:elems[].silhouette", M("Silhouette index").range(0, 255));
			put("PeopleAnime:elems[].animeID", M("Animation id").range(0, 255));
			put("Silhouettes:entries[].silhouetteType", M("Silhouette type").range(0, 10000));
			put("Silhouettes:entries[].width", M("Width (units)").range(1, 2));
			put("Silhouettes:entries[].routeType", M("Route type").range(-1, 1));
			put("Silhouettes:entries[].bitmapID", M("Bitmap id", "RT_BITMAP with 16x36 frames, 6 rows.").range(0, 65535));
			put("People:entries[].peopleType", M("People type").range(0, 1000));
			put("People:entries[].silhouetteType", M("Silhouette type").range(0, 10000));
			put("People:entries[].stressMax", M("Stress max").range(0, 10000));
			put("People:entries[].stressBlueLimit", M("Stress blue limit").range(0, 10000));
			put("People:entries[].stressYellowLimit", M("Stress yellow limit").range(0, 10000));
			put("Words:words[].text", M("Speech line"));
			put("Words:words[].sexMask", M("Sex", "Bit per sex.").bits({{1, "male"}, {2, "female"}}));
			put("Words:words[].ageMask", M("Age class", "Bit per age class (silhouette >> 3).").bits({{1, "0"}, {2, "1"}, {4, "2"}, {8, "3"}, {16, "4"}, {32, "5"}, {64, "6"}, {128, "7"}}));
			put("Words:words[].flags", M("Situation flags", "Exact match.").bits({{1, "at own tenant"}, {2, "mood 1"}, {4, "mood 2"}, {8, "home / lobby"}, {16, "VIP verdict"}}));
			put("Words:words[].special", M("Special", "Exact match.").opts({{0, "normal"}, {1, "status 1"}, {2, "status 2"}, {3, "VIP grade 1"}, {4, "VIP grade 2"}, {5, "VIP grade 3"}, {6, "Maru"}, {7, "Reggae"}}));
			put("StringTable:strings", M("Strings").var());
			put("StringBlock:strings", M("Strings (16 per block)").var(16));

			// ---- SP sub-plugins ----
			put("Movie:title", M("Title", "== ATTR name."));
			put("Movie:tieupDate", M("Tie-up date (BCD 0xYYYYMMDD)", "0 = no real-world tie-up expiry."));
			put("Movie:imageID", M("Poster bitmap id").range(0, 65535));
			put("Movie:filmKind", M("Film kind").opts({{0, "roadshow"}, {2, "famous"}}));
			put("Movie:phase1Months", M("Phase 1 (months)").range(0, 120));
			put("Movie:phase2Months", M("Phase 2 (months)").range(0, 120));
			put("Movie:phase3Months", M("Phase 3 (months)").range(0, 120));
			put("Movie:phase4Months", M("Phase 4", "Read, then zeroed by the loader.").adv());
			put("Movie:cycleMonths", M("Cycle", "Read, then zeroed by the loader.").adv());
			put("Movie:production", M("Production"));
			put("Movie:director", M("Director"));
			put("Movie:starring", M("Starring"));
			put("Movie:runningTime", M("Running time (min)").range(0, 1000));
			put("Movie:description", M("Description").text());
			put("Movie:customerTableID", M("Audience table (TMDF id)").range(0, 65535));
			put("Movie:famousAudience", M("Famous at audience").range(0, 100000));
			put("Movie:remakeAudience", M("Re-run at audience").range(0, 100000));
			put("Movie:extra", M("Extra strings").var(64).adv());
			put("Movie:stillImageID", M("Still bitmap id").range(0, 65535));
			put("Movie:clipSeconds", M("Clip seconds", "Trailer length (low confidence).").range(0, 10000));
			put("Movie:movieFile", M("Movie file", "File inside Movie/ (e.g. MV_1stContact.MOV)."));
			put("HallEvent:title", M("Title"));
			put("HallEvent:tieupDate", M("Tie-up date (BCD 0xYYYYMMDD)"));
			put("HallEvent:imageID", M("Image bitmap id", "4 stacked frames.").range(0, 65535));
			put("HallEvent:bookingDays", M("Booking days").range(0, 365));
			put("HallEvent:openTime", M("Opens at").minutes());
			put("HallEvent:closeTime", M("Closes at").minutes());
			put("HallEvent:monthMask", M("Months").bits(kMonths));
			put("HallEvent:cost", M("Cost", "At close: funds += tenant value - cost.").range(-I, I));
			put("HallEvent:customerTableID", M("Visitor table (TMDF id)").range(0, 65535));
			put("HallEvent:bindTime", M("Visit length (min)").range(0, 1440));
			put("HallEvent:stillImageID", M("Still bitmap id").range(0, 65535));
			put("HallEvent:description", M("Description").text());
			put("HallEvent:movieFile", M("Movie file", "File inside Movie/ (HE_*.MOV)."));
			put("Advertise:title", M("Title"));
			put("Advertise:tieupDate", M("Tie-up date (BCD 0xYYYYMMDD)"));
			put("Advertise:subType", M("Medium (subType)", "Must equal the host's mSubPluginType (BoPI = Billboard).").tags({"BoPI"}));
			put("Advertise:imageID", M("Ad bitmap id", "150x50 picture.").range(0, 65535));
			put("Advertise:days", M("Campaign days").range(0, 10000));
			put("Advertise:money", M("Cost").range(0, I));
			put("Advertise:path", M("Path"));
			put("SubPluginPeopleParam:values", M("Values (unknown consumer)", "Six ints named by position.").adv());

			// ---- World ----
			put("WORLDDEF:sortKey", M("Sort key", "World list order; equals ATTR itemType."));
			put("WORLDDEF:height", M("Height (rows)").range(1, 10000));
			put("WORLDDEF:width", M("Width (units)").range(1, 10000));
			put("WORLDDEF:groundLine", M("Ground line (row)", "Row ground-1 is 1F, row ground is B1.").range(0, 10000));
			put("WORLDDEF:buildArea", M("Build area", "Hard clamp for any construction (units / rows)."));
			put("WORLDDEF:entrance", M("Entrance point", "No consumer found (probably the initial view).").adv());
			put("WORLDDEF:topFloorLine", M("Top floor line (row)").range(0, 10000));
			put("WORLDDEF:bottomFloorLine", M("Bottom floor line (row, exclusive)").range(0, 10000));
			put("WORLDDEF:startGrade", M("Start grade (inferred)").range(1, 8));
			put("WORLDDEF:lobbyHeight", M("Lobby height (rows)").range(1, 3));
			put("WORLDDEF:outViewKind", M("Exterior view kind").range(0, 10));
			put("WORLDDEF:tiledBkgnd", M("Tiled background", "1 = BINF tile map, 0 = bitmap 1000 stretched.").flag());
			put("WORLDDEF:grades", M("Grade table", "Row L-1 is used when upgrading from grade L (<= 8 rows).").var(8));
			put("WORLDDEF:grades[].vipFlag", M("VIP flag", "Must be set in the VIP-visited bits."));
			put("WORLDDEF:grades[].population", M("Population needed").range(0, I));
			put("WORLDDEF:grades[].gradeUpAnimID", M("Grade-up animation bitmap").range(0, 65535));
			put("WORLDDEF:grades[].gradeUpAnimFrames", M("Animation frames", "Bitmap height / 96.").range(0, 1000));
			put("WORLDDEF:grades[].gradeUpAnimParam", M("Animation param").range(0, 1000));
			put("WORLDDEF:bkgndTileHeight", M("Background tile height (px)").range(1, 1000));
			put("WORLDDEF:bkgndTileWidth", M("Background tile width (px)").range(1, 1000));
			put("WORLDDEF:gradeIconAnimIDs", M("Grade icon bitmaps [5]", "0 = none."));
			put("WORLDDEF:towerNamePrefix", M("Tower name prefix"));
			put("WORLDDEF:dustOnAt", M("Dust on at"));
			put("WORLDDEF:toiletAt", M("Toilet at"));
			put("WORLDDEF:toiletDenomi", M("Toilet denominator"));
			put("WORLDDEF:cashAmounts", M("Cash steps", "Funds-dialog steps (first 8 kept).").var(8));
			put("WATR:unk", M("unk[6]", "Discarded by T2WorldDef; [3] = floors above, [4] = basements.").adv());
			put("WATR:initialFunds", M("Initial funds (internal)", "x exchangeMul / exchangeDiv = dollars.").range(0, I));
			put("WATR:exchangeMul", M("Exchange multiplier").range(1, I));
			put("WATR:exchangeDiv", M("Exchange divisor").range(1, I));
			put("WATR:moneyUnit", M("Money unit"));
			put("PODF:gradePoolIDs", M("Pool def per grade [5]", "PGDF id loaded at grade 1..5 (0 = none)."));
			put("PODF:demand", M("Demand pairs").var());
			put("CODF:monthly", M("Cold rate per month [12]"));
			put("SPDF:monthlyPercent", M("Season % per month [12]", "The engine opens SPDF id 1 only."));
			put("T2EV:events", M("Events", "Tower events: type tag + resource id.").var());
			put("T2EV:events[].type", M("Event type", "4cc (Mac order), e.g. FBEv.").tags({"FBEv", "BOEv", "TrEv", "SaEv", "OdEv", "ViEv", "MrEv", "MaEv", "BBEv", "ClEv", "CrEv", "DoEv", "WdEv", "ToEv", "HSEv"}));
			put("T2EV:events[].resID", M("Resource id").range(0, 65535));
			put("VIPL:values", M("Values [6]", "Only the first (grade) is read."));
			put("EVENT:levelBit", M("Grades", "Grades on which the event may run.").bits(kGrades));
			put("EVENT:exclusive", M("Exclusive").flag());
			put("EVENT:cycleDay", M("Every N days", "Begins when day % N == 0.").range(0, 10000));
			put("EVENT:beginTime", M("Begins at").minutes());
			put("EVENT:endTime", M("Ends at").minutes());
			put("EVENT:bodyInts", M("Body (raw ints)", "Layout read by the world plugin's own code.").adv());
			put("FBEV:body.unk38", M("Helicopter price"));
			put("FBEV:body.values", M("Fire bitmap ids").var());
			put("BOEV:body.unk30", M("Default supply"));
			put("BOEV:body.unk34", M("Added supply"));
			put("BOEV:body.unk38", M("Transformer price"));
			put("TREV:body.unk30", M("Ransom"));
			put("TREV:body.unk34", M("Deadline").minutes());
			put("SAEV:body.unk30", M("Vertical divisor", "0 becomes 32."));
			put("ODEV:body.items", M("Decorations").var());
			put("VIEV:body.vips", M("VIPs").var());
			put("VIEV:body.vips[].grade", M("Grade").range(1, 8));
			put("VIEV:body.vips[].tenantType", M("Destination plugin (4cc)"));
			put("VIEV:body.vips[].unk38", M("Arrival minute").minutes());
			put("VIEV:body.vips[].viplID", M("VIPL id").range(0, 65535));
			put("VIEV:body.vips[].comment", M("Good verdict"));
			put("VIEV:body.vips[].comment2", M("Bad verdict"));
			put("MAEV:body.unk30", M("Alert dialog id"));
			put("MAEV:body.viplID", M("VIPL id"));
			put("MAEV:body.unk38", M("Tenant category filter", "50 = Restaurants, 0 = any.").opts({{0, "any"}, {50, "Restaurants"}, {80, "Shops"}}));
			put("MAEV:body.unk3c", M("Animation id"));
			put("MAEV:body.message", M("Claim text", "Empty = Mr. Maru, else Uncle Reggae."));
			put("CLEV:body.imageIDs", M("Cloud bitmaps").var());
			put("CLEV:body.zones", M("Spawn zones").var());
			put("PaletteAnime:fields", M("Palette animation fields"));
			put("FloorInfoMask:grid", M("Mask grid", "Edit in the grid view (click cells)."));
			put("BkgndInfo:grid", M("Tile grid", "Edit in the grid view (click cells)."));
			return t;
		}
	}

	const FieldMeta * findFieldMeta(const std::string & schema, const std::string & path)
	{
		static const std::map<std::string, FieldMeta> table = build();
		auto it = table.find(schema + ":" + path);
		if (it != table.end()) return &it->second;
		// Event resources share the header metadata.
		if (schema.size() == 4 && schema[2] == 'E' && schema[3] == 'V')
		{
			it = table.find("EVENT:" + path);
			if (it != table.end()) return &it->second;
		}
		return nullptr;
	}

	bool isAdvancedKey(const std::string & key)
	{
		auto starts = [&](const char * p) { return key.compare(0, std::char_traits<char>::length(p), p) == 0; };
		return starts("skip") || starts("unread") || starts("unk") || starts("discarded") || key == "leadByte"
		    || key == "sep" || key == "trailingHex" || key == "bodyHex" || key == "pad1" || key == "pad2"
		    || key == "padAfter" || key == "extra";
	}

	std::string prettyKey(const std::string & key)
	{
		std::string out;
		for (size_t i = 0; i < key.size(); ++i)
		{
			const char c = key[i];
			if (i == 0) { out.push_back(char(std::toupper(uint8_t(c)))); continue; }
			if (std::isupper(uint8_t(c)) && !std::isupper(uint8_t(key[i - 1]))) out.push_back(' ');
			out.push_back(std::isupper(uint8_t(c)) && i + 1 < key.size() && std::islower(uint8_t(key[i + 1]))
			              ? char(std::tolower(uint8_t(c))) : c);
		}
		return out;
	}
}
