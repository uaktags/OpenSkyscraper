/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "BldLoader.h"

#include <string>
#include <vector>

namespace OT {
	class PluginManager;
namespace Bld {

	/**
	 * Exporter: an OpenSky YootTower tower (native <tower> XML) -> `.bld`.
	 *
	 * The save's world-wide sections (plugin table, visitor pool, movies,
	 * tower events, window layout, world statistics, traffic-map size) come
	 * from the world plugins, which OpenSky does not fully model, so they
	 * are copied from a template `.bld` of the same world (any save the
	 * game wrote, e.g. the shipped sample). Everything the tower itself
	 * owns is generated: date, stars, VIP flags, funds, the unit map,
	 * tenants (floors, lobby, offices, shops), Simple Elevators with their
	 * cars, stop lists and waiting areas (requests), and the cross-equip
	 * links between them, laid out like the game's own saves
	 * (doc/yoottower/bld-format.md §4). People are not written (the game
	 * repopulates), so the pool population is 0 and offices are written as
	 * contracted-but-unstaffed (status 10).
	 */

	struct TowerCar
	{
		double altitude = 0;
		int homeFloor = 0;
	};

	struct TowerItem
	{
		std::string type;          ///< prototype id
		int x = 0, y = 0;          ///< OpenSky cells / floor (0 = 1F)
		int width = 0, height = 0; ///< floor/lobby/elevator; 0 = prototype size
		bool occupied = false;     ///< office/condo occupied, shop let
		int yootEstimate = 0;
		int rentClass = 1;         ///< shops
		int tenantStatus = -1;     ///< plugin-backed tenants: T2Tenant status (-1 = unknown)
		int variant = 0;           ///< office look (0..5 = Office.t2p kind 1..6)
		int buildDay = -1;         ///< YootTower build day (T2Equip +0x4c), -1 = unknown
		std::vector<std::pair<int, int> > holes; ///< floor: [l, r) gaps (Floor::holes)
		std::vector<int> unserviced;
		std::vector<TowerCar> cars;
	};

	struct TowerState
	{
		std::string ruleset;       ///< "yoottower" expected
		std::string world;         ///< "tokyo" / "hawaii" / "kegon"
		long long funds = 0;       ///< dollars
		long long lastQuarterBalance = 0;
		int rating = 0;            ///< zero-based
		double time = 0;           ///< absolute days
		unsigned vipFlags = 0;
		unsigned secretItems = 0;  ///< doc+0x9c
		std::string traffic;       ///< YootExtrasSystem cells, "count:accum" comma list
		std::string pool;          ///< PeopleSystem visitor pool (yootPool, Yoot::Pool::encode)
		std::vector<TowerItem> items;
	};

	/// Reads the subset of a native <tower> XML save the exporter needs.
	bool parseTowerXml(const std::string & xml, TowerState & out, std::string & error);

	/// Builds a .bld from the tower using `templ` (same world) for the
	/// world-wide sections. Items without a YootTower counterpart are
	/// counted in report.dropped. `plugins` supplies the TNDF footprint and
	/// staff places of the plugin-backed tenants ("yt_*", YootTenants.h);
	/// without it they are dropped. They are written as just built (status
	/// 9, empty staff places) because no people are exported: the game
	/// hires their staff again.
	bool exportTower(const TowerState & tower, const BldData & templ, BldData & out,
		ImportReport & report, std::string & error, const PluginManager * plugins = nullptr);

}
}
