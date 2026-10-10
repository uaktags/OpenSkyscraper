/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Catalog of the 105 PE-format YootTower plugins vendored in data/Plugins,
/// keyed by the plugin id (ATTR mID) with its numeric item type (ATTR
/// mItemType).
///
/// mID is the plugin's unique 4cc ('BURG' for Burger). The save files
/// reference plugins by it and map it to the numeric item type through a
/// plugin table. mItemType is the numeric item type (Floor 1000, Office 1200,
/// Burger 1301, Simple Elevator 1810) and equals the ToolDef mToolType.
/// Earlier versions of this table called the 4cc "itemType"; that was the
/// mislabel corrected in doc/yoottower/plugin-schema.md §0.
///
/// `pluginId` is the Mac 4cc constant, i.e. `FourCC::toUint32LE()` of the
/// file bytes ("GRUB" in the file == 'BURG'). DefaultToolPalette is the one
/// text-form ATTR and its mID is the plain number 1000.
///
/// Regenerated from the plugins with tools/yoottower/t2p_schema.py; the
/// human-readable table is doc/yoottower/itemtype-catalog.md.
#pragma once
#include "PluginResources.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace OT
{
	/// The Mac 4cc constant of a 4-char literal: fourcc("BURG") == 0x42555247.
	constexpr uint32_t fourcc(const char (&s)[5])
	{
		return (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16)
		     | (uint32_t(uint8_t(s[2])) << 8) | uint32_t(uint8_t(s[3]));
	}

	struct ItemTypeEntry
	{
		uint32_t pluginId;   // ATTR mID as the Mac 4cc constant (== FourCC::toUint32LE())
		int32_t itemType;    // ATTR mItemType (numeric item type, == ToolDef mToolType)
		const char * name;   // ATTR mPluginName as shipped
		const char * cls;    // DESC class: Tenant / Mover / World / OuterObj / ToolPalette
		const char * file;   // plugin file stem in data/Plugins

		/// The plugin id as a FourCC (file byte order).
		FourCC id() const { return FourCC::fromUint32LE(pluginId); }
	};

	/// Every PE plugin, sorted by plugin id (case-insensitive). Both columns
	/// are unique across the population.
	inline constexpr std::array<ItemTypeEntry, 105> kItemTypeCatalog = {{
		{1000u,           1414484044, "Default Tool palette",        "ToolPalette", "DefaultToolPalette"},
		{fourcc("ALHS"),        1625, "Aloha Shop",                  "Tenant",      "Aloha"},
		{fourcc("APRT"),        1101, "Condo",                       "Tenant",      "Condo"},
		{fourcc("BABA"),        1603, "Barber Shop",                 "Tenant",      "Barber"},
		{fourcc("BCDM"),        1645, "BCD-Mart",                    "Tenant",      "BCDMart"},
		{fourcc("BigE"),        1812, "Large Elevator",              "Mover",       "BigElev"},
		{fourcc("BILL"),        2100, "Billboard",                   "OuterObj",    "Billboard"},
		{fourcc("BOOK"),        1601, "Book Store",                  "Tenant",      "Bookstore"},
		{fourcc("BTKP"),        1723, "Boat Deck",                   "Tenant",      "Boatdeck"},
		{fourcc("BURG"),        1301, "Burger land",                 "Tenant",      "Burger"},
		{fourcc("BUTQ"),        1641, "Boutique",                    "Tenant",      "Boutique"},
		{fourcc("CAFE"),        1300, "coffee.com",                  "Tenant",      "Coffee"},
		{fourcc("CATH"),        2000, "Final",                       "Tenant",      "Cathedral"},
		{fourcc("CINA"),        1346, "Uncle Chow's",                "Tenant",      "Chinese"},
		{fourcc("CINE"),        1720, "Movie Theater",               "Tenant",      "Theater"},
		{fourcc("CKTL"),        1348, "Cocktail Lounge",             "Tenant",      "Cocktail"},
		{fourcc("CONB"),        1622, "Convenience Store",           "Tenant",      "Convenience"},
		{fourcc("COND"),        1120, "Hawaiian Condo",              "Tenant",      "HawaianCondo"},
		{fourcc("DANG"),        1381, "Japanese Snacks",             "Tenant",      "J-Snack"},
		{fourcc("DFSX"),        1644, "Duty Free Shop",              "Tenant",      "DutyFree"},
		{fourcc("DRUG"),        1620, "Drug Store",                  "Tenant",      "Drug"},
		{fourcc("DUST"),        1542, "Trash Facility",              "Tenant",      "TrashFacility"},
		{fourcc("ELEC"),        1640, "Electronic Store",            "Tenant",      "Electric"},
		{fourcc("ESCA"),        1801, "Escalators",                  "Mover",       "Escalator"},
		{fourcc("EVNT"),        1722, "Event Center",                "Tenant",      "EventCenter"},
		{fourcc("FAMI"),        1323, "Jenny's",                     "Tenant",      "Jenny's"},
		{fourcc("FCOT"),        1360, "Food Court",                  "Tenant",      "FoodCoat"},
		{fourcc("FLOR"),        1000, "Floor",                       "Tenant",      "Floor"},
		{fourcc("FLOW"),        1602, "1-800 FLOWERS",               "Tenant",      "Flower"},
		{fourcc("FRCH"),        1347, "Eiffel Tower",                "Tenant",      "French"},
		{fourcc("FRON"),        1400, "Front Desk",                  "Tenant",      "Front"},
		{fourcc("FRUT"),        1646, "Fruit Stand",                 "Tenant",      "Fruits"},
		{fourcc("GAME"),        1660, "Gameworks",                   "Tenant",      "Gameworks"},
		{fourcc("HEND"),        1541, "Power Room",                  "Tenant",      "PowerRoom"},
		{fourcc("HIHO"),        2002, "Final",                       "Tenant",      "Attraction"},
		{fourcc("HOSP"),        1544, "Doctor's Office",             "Tenant",      "Doctor's"},
		{fourcc("HREN"),        1102, "Apartment",                   "Tenant",      "Apartment"},
		{fourcc("HSGL"),        1401, "Single rooms",                "Tenant",      "Singleroom"},
		{fourcc("HSUI"),        1403, "Suite rooms",                 "Tenant",      "Suiteroom"},
		{fourcc("HTWN"),        1402, "Twin rooms",                  "Tenant",      "Twinroom"},
		{fourcc("HWAI"),          20, "Hawaii",                      "World",       "Hawai"},
		{fourcc("ICES"),        1600, "Ice Cream Stand",             "Tenant",      "Icecream"},
		{fourcc("IKOI"),        1741, "Gathering Area",              "Tenant",      "GatheringArea"},
		{fourcc("INDO"),        1343, "Maharaja",                    "Tenant",      "Indian"},
		{fourcc("ITAL"),        1342, "Mama Pizano's",               "Tenant",      "Itarian"},
		{fourcc("IZAK"),        1305, "J-Pub",                       "Tenant",      "J-Pub"},
		{fourcc("JCIG"),        1604, "Cigarette Vending Machine",   "Tenant",      "Cigar"},
		{fourcc("JDNK"),        1605, "Juice Vending Machine",       "Tenant",      "Drink"},
		{fourcc("JPNF"),        1345, "Japanese Cuisine",            "Tenant",      "Japanese"},
		{fourcc("KCIG"),        1684, "Cigarette Vending Machine",   "Tenant",      "kCigar"},
		{fourcc("KDUS"),        1582, "Trash Facility",              "Tenant",      "KTrashFacility"},
		{fourcc("KEBI"),        1521, "Security Room",               "Tenant",      "Securityroom"},
		{fourcc("KEIS"),        1380, "Old Time Cafe",               "Tenant",      "OldTimeCafe"},
		{fourcc("KELV"),        1818, "K.F.Simple Elevator",         "Mover",       "KElev"},
		{fourcc("KFLO"),        1010, "Floor",                       "Tenant",      "KFloor"},
		{fourcc("KGON"),          10, "The Kegon Falls",             "World",       "Kegon"},
		{fourcc("KICE"),        1681, "Ice Cream Stand",             "Tenant",      "KIcecream"},
		{fourcc("KJUC"),        1685, "Juice Vending Machine",       "Tenant",      "KDrink"},
		{fourcc("KOBE"),        1481, "Small Room",                  "Tenant",      "Smallroom"},
		{fourcc("KSER"),        1819, "K.F.Service Elevator",        "Mover",       "KServ"},
		{fourcc("KTOI"),        1581, "Restroom",                    "Tenant",      "KRestroom"},
		{fourcc("MARK"),        1648, "Supermarket",                 "Tenant",      "SuperMarket"},
		{fourcc("MEDA"),        1683, "Coin Press Machine",          "Tenant",      "Coin"},
		{fourcc("MEGA"),        1721, "Classic Movie Theatre",       "Tenant",      "ClassicTheater"},
		{fourcc("MENS"),        1642, "Men's Clothing Shop",         "Tenant",      "Men'sClothing"},
		{fourcc("MENT"),        1404, "Housekeeping",                "Tenant",      "Maintenance"},
		{fourcc("MEXC"),        1322, "Loco Taco",                   "Tenant",      "Mexcan"},
		{fourcc("MILK"),        1680, "Juice Bar",                   "Tenant",      "Juicebar"},
		{fourcc("MIYA"),        1687, "Souvenirs",                   "Tenant",      "Souvenirs"},
		{fourcc("OBEY"),        1482, "Large Room",                  "Tenant",      "Largeroom"},
		{fourcc("OFFC"),        1200, "Office",                      "Tenant",      "Office"},
		{fourcc("ORJU"),        1610, "Orange Julius",               "Tenant",      "OrangeJulius"},
		{fourcc("PARK"),        1543, "Parking",                     "Tenant",      "Parking"},
		{fourcc("PETS"),        1621, "Pet Shop",                    "Tenant",      "Petshop"},
		{fourcc("RAMN"),        1303, "Ramen Noodle House",          "Tenant",      "Ramen"},
		{fourcc("RCOD"),        1624, "Record store",                "Tenant",      "Record"},
		{fourcc("RFRO"),        1480, "Front Desk",                  "Tenant",      "RFront"},
		{fourcc("RMEN"),        1483, "Housekeeping",                "Tenant",      "RMaintenance"},
		{fourcc("SCOL"),        1740, "Elementary School",           "Tenant",      "School"},
		{fourcc("SDUM"),        2001, "Final",                       "Tenant",      "Stadium"},
		{fourcc("SEIN"),        1580, "Fire Department",             "Tenant",      "FireDepartment"},
		{fourcc("SerE"),        1811, "Service Elevator",            "Mover",       "ServElev"},
		{fourcc("SFOD"),        1321, "Ahabs",                       "Tenant",      "SeaFood"},
		{fourcc("SHAS"),        1682, "Photo Studio",                "Tenant",      "Photo"},
		{fourcc("SHOK"),        1382, "Traditional Japanese",        "Tenant",      "Trad-Japanese"},
		{fourcc("SKYB"),        1001, "Sky Bridge",                  "Tenant",      "SkyBrdge"},
		{fourcc("SNJK"),           0, "Tokyo",                       "World",       "Tokyo"},
		{fourcc("SPOR"),        1643, "Sporting Goods",              "Tenant",      "Sportsshop"},
		{fourcc("SSOB"),        1304, "Soba Noodle Stand",           "Tenant",      "SobaNoodle"},
		{fourcc("STAR"),        1800, "Stairs",                      "Mover",       "Stair"},
		{fourcc("StdE"),        1810, "Simple Elevator",             "Mover",       "StdElev"},
		{fourcc("STEK"),        1320, "Big Country",                 "Tenant",      "Steak"},
		{fourcc("SUBW"),        1701, "Subway Station",              "Tenant",      "Subway"},
		{fourcc("SURF"),        1626, "Surf Shop",                   "Tenant",      "SurfShop"},
		{fourcc("SUSI"),        1341, "Sushi-Man",                   "Tenant",      "Sushibar"},
		{fourcc("TAKI"),        1920, "Kegon Spa",                   "Tenant",      "Spa"},
		{fourcc("TARE"),        1688, "Kids Store",                  "Tenant",      "Kids"},
		{fourcc("TENB"),        1901, "Observatory",                 "Tenant",      "Observatory"},
		{fourcc("TICK"),        1900, "Ticket Booth",                "Tenant",      "Ticket"},
		{fourcc("TOIL"),        1500, "Restroom",                    "Tenant",      "Restroom"},
		{fourcc("TOYS"),        1647, "ToyWorld",                    "Tenant",      "Toyworld"},
		{fourcc("TUKE"),        1686, "Local Pickle Farm",           "Tenant",      "LocalPickle"},
		{fourcc("VIDO"),        1623, "Video & DVD",                 "Tenant",      "Video&DVD"},
		{fourcc("WETO"),        1351, "Mrs.Weinstein'sToffee",       "Tenant",      "Weinstein'sToffee"},
		{fourcc("YAKI"),        1344, "Korean B-BQ",                 "Tenant",      "KoreanBBQ"},
	}};

	/// Looks up a plugin by its id. Returns nullptr if unknown (e.g. a
	/// hand-authored or SP-format plugin).
	inline const ItemTypeEntry * lookupPluginId(const FourCC & id)
	{
		const uint32_t v = id.toUint32LE();
		for (const auto & e : kItemTypeCatalog)
			if (e.pluginId == v) return &e;
		return nullptr;
	}

	/// Looks up a plugin by its id spelled in Mac order ("BURG", "StdE").
	/// Case-insensitive, as the 4ccs vary in case across the population.
	inline const ItemTypeEntry * lookupPluginId(std::string_view macOrder)
	{
		if (macOrder.size() != 4) return nullptr;
		auto upper = [](uint8_t c) -> uint8_t { return (c >= 'a' && c <= 'z') ? uint8_t(c - 'a' + 'A') : c; };
		for (const auto & e : kItemTypeCatalog)
		{
			bool same = true;
			for (int i = 0; i < 4 && same; ++i)
				same = upper(uint8_t(e.pluginId >> (24 - 8 * i))) == upper(uint8_t(macOrder[size_t(i)]));
			if (same) return &e;
		}
		return nullptr;
	}

	/// Looks up a plugin by its numeric item type (ATTR mItemType).
	inline const ItemTypeEntry * lookupItemType(int32_t itemType)
	{
		for (const auto & e : kItemTypeCatalog)
			if (e.itemType == itemType) return &e;
		return nullptr;
	}
}
