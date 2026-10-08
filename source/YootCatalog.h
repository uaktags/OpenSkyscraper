/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower data-driven item catalog (game-parity.md §1.4 E1, §1.7 M1-M5,
/// S6, W6; plan step 4).
///
/// Reduces a plugin's template definition (TNDF / MVDF, PluginDefs.h) to the
/// numbers the rules need and resolves one plugin per OpenSky prototype for
/// a world. Money is converted from the engine's $100 units to dollars
/// (T2WorldDef::ExchangeMoney 0x1000ab50: x100 in all three worlds).
///
/// Field sources (T2DLL.dll constructors):
///   price      T2ToolDef +0xd8 (CalcPayment 0x10004e70)
///   outMoney   T2ToolDef +0xa8 (GetOutMoney; maintenance, see E3)
///   power      T2ToolDef +0xf0
///   capacity   T2EquipDef +0x11c
///   inMoney    T2EquipDef +0x144[i] (GetInMoney 0x1002f8a0)
///   hours      T2TenantDef +0x174 / +0x178 (minutes), stay +0x17c
///   maxBuild   T2TenantDef +0x104 (GetMaxBuildCount 0x10004ef0)
///   mover      T2MoverDef +0x168 maxLength, +0x16c minLength, +0x170 maxStop,
///              +0x1a8 score (T2Mover::CalcScore 0x1003ef20), +0x178 speed[3],
///              +0x184 maxModule (cars), +0x194 per-car price tier
///
/// Pure module: no Game dependency, linked into OpenSkyTests.
#pragma once

#include "PluginDefs.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace OT
{
	class AbstractPlugin;
	class PluginManager;

	struct YootItemStats
	{
		std::string plugin;            ///< e.g. "StdElev.t2p"
		std::string name;              ///< variation name ("Simple Elevator")
		TemplateDef::Kind kind = TemplateDef::Tenant;
		int variation = 0;             ///< ToolDef variation used for price/outMoney
		uint32_t attribute = 0;        ///< ToolDef mAttribute (ToolAttribute bits)
		int category = 0;              ///< ToolDef +0x64 (50 restaurants, 100 final items)
		std::string categoryName;      ///< ToolDef +0x68 (toolbox group title)
		int toolNo = 0;                ///< ToolDef +0x70 (order inside the group)

		int price = 0;                 ///< dollars, construction cost of `variation`
		int outMoney = 0;              ///< dollars, maintenance of `variation`
		int power = 0;                 ///< consumption of `variation`
		int capacity = 0;
		std::vector<int> inMoney;      ///< dollars per price selection (0 = cheapest)

		int senseOfNoise = 0;
		int openMinute = 0;            ///< minutes from 0:00
		int closeMinute = 0;
		int bindMinutes = 0;           ///< stay length
		int maxBuild = 0;              ///< 0 = unlimited
		int belongCapacity = 0;        ///< T2TenantDef +0x160 residents / staff
		int toolType = 0;              ///< ToolDef +0x50 (== ATTR mItemType)
		int width = 0, height = 0;     ///< ToolDef +0x54 / +0x58 footprint units
		uint32_t dependId = 0;         ///< ATTR mDependID (W4: Front Desk "NORF")

		bool mover = false;
		int maxLength = 0;             ///< floors; 0 = unlimited
		int minLength = 0;
		int maxStops = 0;              ///< 0 = every floor
		int routeScore = 0;            ///< route cost per floor (P3)
		int maxCars = 0;
		int carPrice = 0;              ///< dollars per extra car
		std::array<int, 3> speed{{0, 0, 0}};

		/// E3: the item is charged quarterly maintenance (attr 0x100).
		bool paysQuarterly() const { return (attribute & ToolAttribute::PaysMaintenance) != 0; }
		/// E3: lobby / sky bridge charge outMoney per unit of width (attr 0x4000).
		bool maintenancePerUnit() const { return (attribute & ToolAttribute::VariableLength) != 0; }
		/// L2: TNDF category 100 "Final Item" (Stadium, Cathedral, Attraction).
		bool finalItem() const { return category == 100; }
		/// P5: T2Tenant::IsNeedToilet (attr bit 0x2000000).
		bool needsToilet() const { return (attribute & 0x2000000u) != 0; }
		/// inMoney (dollars) for a price selection, clamped to the table.
		int inMoneyFor(int selection) const;
	};

	/// Engine money unit -> dollars (ExchangeMoney: x100 / 1).
	const int kYootMoneyUnit = 100;

	/// Builds the stats for one ToolDef variation of a decoded template.
	bool yootItemStatsFromTemplate(const TemplateDef & def, int variation, YootItemStats & out);
	/// Decodes the plugin's TNDF/MVDF and builds the stats.
	bool loadYootItemStats(const AbstractPlugin & plugin, int variation, YootItemStats & out);

	/// ToolDef variation an OpenSky prototype maps to inside its plugin
	/// (Floor.t2p: 0 = Floor, 1 = Lobby; Parking.t2p: 0 = lot, 1 = gate).
	int yootPrototypeVariation(const std::string & prototypeId);

	/// Resolves yootPrototypeLinks() against the catalog: for every
	/// prototype, the first linked plugin available in `worldMask` (ATTR
	/// mAttrib) supplies the stats. Prototypes with no available plugin are
	/// left out (they keep their OpenSky numbers).
	std::map<std::string, YootItemStats> resolveYootCatalog(const PluginManager & plugins, uint32_t worldMask);
}
