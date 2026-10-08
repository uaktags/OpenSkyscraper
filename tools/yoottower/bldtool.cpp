/* Copyright (c) 2026 OpenSkyscraper Project */
// Command-line front end for the C++ .bld codec (source/BldLoader,
// source/BldExport):
//
//   BldTool summary SAVE.bld
//   BldTool roundtrip SAVE.bld...            parse + write, compare bytes
//   BldTool import SAVE.bld OUT.tower         native XML, as --load-game sees it
//   BldTool export IN.tower TEMPLATE.bld OUT.bld
//
// TEMPLATE.bld is any save of the tower's world; it supplies the sections
// OpenSky does not model (plugin table, visitor pool, events, ...).
#include "AbstractPlugin.h"
#include "BldExport.h"
#include "BldLoader.h"
#include "PluginManager.h"
#include "YootTenants.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace OT::Bld;

static bool readFile(const char * path, std::vector<uint8_t> & out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) return false;
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

static bool writeFile(const char * path, const void * data, size_t n)
{
	std::ofstream out(path, std::ios::binary);
	out.write((const char *)data, (std::streamsize)n);
	return out.good();
}

/// Footprints of the OpenSky prototypes the importer can emit.
static bool prototypeSize(const std::string & id, int & w, int & h)
{
	static const std::map<std::string, std::pair<int, int> > sizes = {
		{"lobby", {4, 1}}, {"floor", {1, 1}}, {"stairs", {8, 2}}, {"escalator", {8, 2}},
		{"elevator-express", {6, 1}}, {"elevator-service", {4, 1}}, {"elevator-standard", {4, 1}},
		{"fastfood", {16, 1}}, {"restaurant", {24, 1}}, {"shop", {12, 1}}, {"office", {9, 1}},
		{"condo", {16, 1}}, {"yoot_condo", {16, 1}}, {"hotel_single", {4, 1}}, {"hotel_double", {6, 1}},
		{"hotel_suite", {7, 1}}, {"partyhall", {24, 2}}, {"cinema", {31, 2}}, {"metro", {30, 3}},
		{"parking", {8, 1}}, {"security", {16, 1}}, {"medicalcenter", {32, 1}}, {"recycling", {25, 2}},
		{"cathedral", {48, 4}}, {"powerroom", {23, 2}},
	};
	auto i = sizes.find(id);
	if (i == sizes.end())
	{
		// Plugin-backed tenants ("yt_*"): the TNDF footprint, read from
		// $OPENSKY_PLUGINS or ./data/Plugins.
		const OT::YootTenantPlugin * t = OT::isYootTenantPrototypeId(id) ? OT::findYootTenantPlugin(id) : nullptr;
		if (!t) return false;
		static OT::PluginManager plugins;
		static bool scanned = false;
		if (!scanned)
		{
			const char * dir = std::getenv("OPENSKY_PLUGINS");
			plugins.addPluginDirectory(OT::Path(dir ? dir : "data/Plugins"));
			scanned = true;
		}
		OT::AbstractPlugin * p = plugins.get(t->plugin);
		OT::TemplateDef def;
		if (!p || !p->ensureLoaded() || !p->getTemplateDef(def)) return false;
		w = def.tool.width;
		h = def.tool.height;
		return true;
	}
	w = i->second.first;
	h = i->second.second;
	return true;
}

static void printReport(const ImportReport & r)
{
	for (const auto & i : r.imported) printf("  placed  %3d x %s\n", i.second, i.first.c_str());
	for (const auto & d : r.dropped) printf("  dropped %3d x %s\n", d.second, d.first.c_str());
	for (const std::string & n : r.notes) printf("  note    %s\n", n.c_str());
}

int main(int argc, char ** argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: %s summary|roundtrip|import|export ...\n", argv[0]);
		return 2;
	}
	const std::string cmd = argv[1];
	std::string error;
	if (cmd == "summary" || cmd == "roundtrip")
	{
		int failures = 0;
		for (int i = 2; i < argc; i++)
		{
			std::vector<uint8_t> bytes, again;
			BldData d;
			if (!readFile(argv[i], bytes) || !parseBytes(bytes, d, error))
			{
				printf("FAIL %s: %s\n", argv[i], error.c_str());
				failures++;
				continue;
			}
			if (cmd == "summary")
			{
				std::map<std::string, int> byPlugin;
				for (const Tenant & t : d.tenants)
					if (t.hdr.used)
					{
						const PluginInfo * p = d.pluginForItemType(t.equipType);
						byPlugin[p ? macOrder(p->id) + " " + p->name : std::to_string(t.equipType)]++;
					}
				printf("%s: world %s, %d star(s), year %d month %d %02d:%02d, $%lld, population %d, %d people, grid %dx%d\n",
					argv[i], macOrder(d.world).c_str(), d.level, d.year, d.month0 + 1, d.minutes / 60, d.minutes % 60,
					d.fundsDollars(), d.pool.population, d.usedPeople(), d.vRange, d.hRange);
				for (const auto & b : byPlugin) printf("  %3d x %s\n", b.second, b.first.c_str());
				printf("  %d mover(s), %zu plugin(s), %zu event(s), %zu traffic cells\n",
					d.usedMovers(), d.plugins.size(), d.events.size(), d.trafficMap.size() / 4);
				continue;
			}
			const bool same = writeBytes(d, again, error) && again == bytes;
			printf("%s %s (%zu bytes)\n", same ? "OK  " : "FAIL", argv[i], bytes.size());
			failures += same ? 0 : 1;
		}
		return failures ? 1 : 0;
	}
	if (cmd == "import" && argc == 4)
	{
		BldData d;
		if (!loadFile(argv[2], d, error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
		std::string xml;
		ImportReport report;
		if (!toTowerXml(d, prototypeSize, xml, report, error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
		printReport(report);
		return writeFile(argv[3], xml.data(), xml.size()) ? 0 : 1;
	}
	if (cmd == "export" && argc == 5)
	{
		std::vector<uint8_t> xmlBytes;
		if (!readFile(argv[2], xmlBytes)) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
		TowerState tower;
		if (!parseTowerXml(std::string(xmlBytes.begin(), xmlBytes.end()), tower, error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
		BldData templ, out;
		if (!loadFile(argv[3], templ, error)) { fprintf(stderr, "template: %s\n", error.c_str()); return 1; }
		ImportReport report;
		if (!exportTower(tower, templ, out, report, error, prototypeSize)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
		printReport(report);
		std::vector<uint8_t> bytes;
		if (!writeBytes(out, bytes, error)) { fprintf(stderr, "%s\n", error.c_str()); return 1; }
		return writeFile(argv[4], bytes.data(), bytes.size()) ? 0 : 1;
	}
	fprintf(stderr, "bad arguments\n");
	return 2;
}
