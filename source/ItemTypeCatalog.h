/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Catalog of every YootTower itemType (mItemType 4cc) extracted from the 105
/// PE-format plugins vendored in data/Plugins. The SP-format plugins (51,
/// mostly the MV_* movie / Pl* event / Ob* object sets) use an older container
/// with no ATTR resource and are out of scope here.
///
/// Each entry maps the 4-char code -> the human-readable name the plugin ships
/// with. Used by the PluginEditor to annotate the "Type code" field (so an
/// author sees "Apartment" beside "NERH"), and by the audit/docs.
///
/// Derived by sweeping all 156 plugins with scratch/plugin_resource_audit.py;
/// the source-of-truth sweep lives in doc/yoottower/itemtype-catalog.md.
#pragma once
#include <array>
#include <string_view>

namespace OT
{
	struct ItemTypeEntry
	{
		const char * code;   // 4-char mItemType (display order)
		const char * name;   // plugin's mPluginName
		const char * cls;    // Tenant / Mover / World / OuterObj / ToolPalette
	};

	/// Every known itemType. 105 distinct codes across the 105 PE-format
	/// plugins (each plugin carries a unique mItemType). The 51 SP-format
	/// plugins use an older container with no ATTR and are not cataloged.
	inline constexpr std::array<ItemTypeEntry, 105> kItemTypeCatalog = {{
		{"ABAB", "Barber Shop",              "Tenant"},
		{"ACSE", "Escalators",               "Mover"},
		{"ADEM", "Coin Press Machine",       "Tenant"},
		{"AGEM", "Classic Movie Theatre",    "Tenant"},
		{"ANIC", "Uncle Chow's",             "Tenant"},
		{"AYIM", "Souvenirs",                "Tenant"},
		{"BNOC", "Convenience Store",        "Tenant"},
		{"BOSS", "Soba Noodle Stand",        "Tenant"},
		{"BNET", "Observatory",              "Tenant"},
		{"BYKS", "Sky Bridge",               "Tenant"},
		{"CFFO", "Office",                   "Tenant"},
		{"CELE", "Electronic Store",         "Tenant"},
		{"CUJK", "Juice Vending Machine",    "Tenant"},
		{"CXEM", "Loco Taco",                "Tenant"},
		{"DOCR", "Record store",             "Tenant"},
		{"DOFS", "Ahabs",                    "Tenant"},
		{"DNOC", "Hawaiian Condo",           "Tenant"},
		{"DNEH", "Power Room",               "Tenant"},
		{"EBOK", "Small Room",               "Tenant"},
		{"ECIK", "Ice Cream Stand",          "Tenant"},
		{"EdtS", "Simple Elevator",          "Mover"},
		{"EgiB", "Large Elevator",           "Mover"},
		{"EFAC", "coffee.com",               "Tenant"},
		{"EKUT", "Local Pickle Farm",        "Tenant"},
		{"EMAG", "Gameworks",                "Tenant"},
		{"ENIC", "Movie Theater",            "Tenant"},
		{"EreS", "Service Elevator",         "Mover"},
		{"ERAT", "Kids Store",               "Tenant"},
		{"FNPJ", "Japanese Cuisine",         "Tenant"},
		{"FRUS", "Surf Shop",                "Tenant"},
		{"GICK", "Cigarette Vending Machine","Tenant"},
		{"GICJ", "Cigarette Vending Machine","Tenant"},
		{"GNAD", "Japanese Snacks",          "Tenant"},
		{"GURD", "Drug Store",               "Tenant"},
		{"GRUB", "Burger land",              "Tenant"},
		{"HCRF", "Eiffel Tower",             "Tenant"},
		{"HTAC", "Cathedral",                "Tenant"},  // plugin's mPluginName is "Final" (placeholder); file is Cathedral.t2p
		{"IAWH", "Hawaii",                   "World"},
		{"IBEK", "Security Room",            "Tenant"},
		{"IKAT", "Kegon Spa",                "Tenant"},
		{"IKAY", "Korean B-BQ",              "Tenant"},
		{"IMAF", "Jenny's",                  "Tenant"},
		{"IOKI", "Gathering Area",           "Tenant"},
		{"IOTK", "Restroom",                 "Tenant"},
		{"ISUS", "Sushi-Man",                "Tenant"},
		{"IUSH", "Suite rooms",              "Tenant"},
		{"KAZI", "J-Pub",                    "Tenant"},
		{"KCIT", "Ticket Booth",             "Tenant"},
		{"KETS", "Big Country",              "Tenant"},
		{"KLIM", "Juice Bar",                "Tenant"},
		{"KNDJ", "Juice Vending Machine",    "Tenant"},
		{"KOOB", "Book Store",               "Tenant"},
		{"KOHS", "Traditional Japanese",     "Tenant"},
		{"KJNS", "Tokyo",                    "World"},
		{"KRAP", "Parking",                  "Tenant"},
		{"KRAM", "Supermarket",              "Tenant"},
		{"LATI", "Mama Pizano's",            "Tenant"},
		{"LGSH", "Single rooms",             "Tenant"},
		{"LIOT", "Restroom",                 "Tenant"},
		{"LLIB", "Billboard",                "OuterObj"},
		{"LTKC", "Cocktail Lounge",          "Tenant"},
		{"LOCS", "Elementary School",        "Tenant"},
		{"LPOT", "Default Tool palette",     "ToolPalette"},
		{"MDCB", "BCD-Mart",                 "Tenant"},
		{"MUDS", "Stadium",                  "Tenant"},  // plugin's mPluginName is "Final" (placeholder); file is Stadium.t2p
		{"NMAR", "Ramen Noodle House",       "Tenant"},
		{"NERH", "Apartment",                "Tenant"},
		{"NEMR", "Housekeeping",             "Tenant"},
		{"NIES", "Fire Department",          "Tenant"},
		{"NOGK", "The Kegon Falls",          "World"},
		{"NORF", "Front Desk",               "Tenant"},
		{"NWTH", "Twin rooms",               "Tenant"},
		{"ODIV", "Video & DVD",              "Tenant"},
		{"ODNI", "Maharaja",                 "Tenant"},
		{"OLFK", "Floor",                    "Tenant"},
		{"ORFR", "Front Desk",               "Tenant"},
		{"OTEW", "Mrs.Weinstein'sToffee",    "Tenant"},
		{"OHIH", "Final",                    "Tenant"},
		{"PKTB", "Boat Deck",                "Tenant"},
		{"PSOH", "Doctor's Office",          "Tenant"},
		{"QTUB", "Boutique",                 "Tenant"},
		{"RESK", "K.F.Service Elevator",     "Mover"},
		{"RATS", "Stairs",                   "Mover"},
		{"ROPS", "Sporting Goods",           "Tenant"},
		{"ROLF", "Floor",                    "Tenant"},
		{"SAHS", "Photo Studio",             "Tenant"},
		{"SECI", "Ice Cream Stand",          "Tenant"},
		{"SHLA", "Aloha Shop",               "Tenant"},
		{"SIEK", "Old Time Cafe",            "Tenant"},
		{"SNEM", "Men's Clothing Shop",      "Tenant"},
		{"STEP", "Pet Shop",                 "Tenant"},
		{"SUDK", "Trash Facility",           "Tenant"},
		{"SYOT", "ToyWorld",                 "Tenant"},
		{"TNEM", "Housekeeping",             "Tenant"},
		{"TNVE", "Event Center",             "Tenant"},
		{"TOCF", "Food Court",               "Tenant"},
		{"TRPA", "Condo",                    "Tenant"},
		{"TSUD", "Trash Facility",           "Tenant"},
		{"TURF", "Fruit Stand",              "Tenant"},
		{"UJRO", "Orange Julius",            "Tenant"},
		{"VLEK", "K.F.Simple Elevator",      "Mover"},
		{"WBUS", "Subway Station",           "Tenant"},
		{"WOLF", "1-800 FLOWERS",            "Tenant"},
		{"XSFD", "Duty Free Shop",           "Tenant"},
		{"YEBO", "Large Room",               "Tenant"},
	}};

	/// Looks up a 4-char itemType code in the catalog. Returns nullptr if the
	/// code isn't known (e.g. a hand-authored or SP-format plugin). The match
	/// is case-insensitive because codes vary in case across the population
	/// (e.g. ENIC vs Enic, EdtS, EgiB).
	inline const ItemTypeEntry * lookupItemType(std::string_view code)
	{
		if (code.size() != 4) return nullptr;
		auto upper = [](char c) -> char { return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c; };
		char c0 = upper(code[0]), c1 = upper(code[1]), c2 = upper(code[2]), c3 = upper(code[3]);
		for (const auto & e : kItemTypeCatalog)
		{
			if (!e.code) continue;  // defensive: skip null entries
			if (upper(e.code[0])==c0 && upper(e.code[1])==c1
			    && upper(e.code[2])==c2 && upper(e.code[3])==c3)
				return &e;
		}
		return nullptr;
	}
}
