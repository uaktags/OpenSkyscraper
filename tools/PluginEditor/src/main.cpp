/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "EditorApp.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void printUsage()
	{
		std::cerr << "Usage: PluginEditor [<plugin>] [options]\n"
		          << "  <plugin>                 a YootTower .t2p file or an .osp-plugin directory\n"
		          << "  --select TYPE:ID         select a resource (TNDF:1, WORLDDEF:1, BITMAP:1000,\n"
		          << "                           OBJMAP:1000, BINF:1) or a group (Sounds)\n"
		          << "  --set TYPE:ID:PATH=VALUE edit a field, e.g. ATTR:128:name=Burger\n"
		          << "                           or TNDF:1:variations[0].price=777 (repeatable)\n"
		          << "  --advanced               show advanced (unused / unknown) fields\n"
		          << "  --part N                 select OBJMAP part N in the sprite preview\n"
		          << "  --export-t2p <out.t2p>   write the (edited) plugin as a YootTower .t2p\n"
		          << "  --save-osp <dir>         write the (edited) plugin as an .osp-plugin\n"
		          << "  --screenshot <path.png>  (alias --capture) save a frame after loading and\n"
		          << "                           exit (headless smoke test)\n";
	}
}

int main(int argc, char ** argv)
{
	editors::EditorApp app;
	std::string openPath;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		auto value = [&](std::string & out) -> bool {
			if (i + 1 >= argc) return false;
			out = argv[++i];
			return true;
		};
		std::string v;
		if (arg == "--screenshot" || arg == "--capture") { if (!value(app.cliScreenshotPath)) { printUsage(); return 1; } }
		else if (arg == "--select") { if (!value(app.cliSelect)) { printUsage(); return 1; } }
		else if (arg == "--set") { if (!value(v)) { printUsage(); return 1; } app.cliSets.push_back(v); }
		else if (arg == "--export-t2p") { if (!value(app.cliExportT2p)) { printUsage(); return 1; } }
		else if (arg == "--save-osp") { if (!value(app.cliSaveOsp)) { printUsage(); return 1; } }
		else if (arg == "--advanced") app.cliAdvanced = true;
		else if (arg == "--part") { if (!value(v)) { printUsage(); return 1; } app.cliPart = std::atoi(v.c_str()); }
		else if (arg == "--help" || arg == "-h") { printUsage(); return 0; }
		else if (!arg.empty() && arg[0] == '-')
		{
			std::cerr << "PluginEditor: unknown option: " << arg << "\n";
			printUsage();
			return 1;
		}
		else if (openPath.empty()) openPath = arg;
		else
		{
			std::cerr << "PluginEditor: unexpected extra argument: " << arg << "\n";
			printUsage();
			return 1;
		}
	}

	if (!openPath.empty())
	{
		const bool isT2p = openPath.size() >= 4 && openPath.compare(openPath.size() - 4, 4, ".t2p") == 0;
		if (!isT2p && !OT::isOspPluginDir(openPath))
		{
			std::cerr << "PluginEditor: not a .t2p file or .osp-plugin directory: " << openPath << "\n";
			printUsage();
			return 1;
		}
		app.cliOpenPath = openPath;
	}
	if ((!app.cliExportT2p.empty() || !app.cliSaveOsp.empty() || !app.cliSets.empty()) && openPath.empty())
	{
		std::cerr << "PluginEditor: --set/--export-t2p/--save-osp need a plugin to open\n";
		return 1;
	}
	return app.run();
}
