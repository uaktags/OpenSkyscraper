/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "EditorApp.h"

#include <iostream>
#include <string>

namespace
{
	void printUsage()
	{
		std::cerr << "Usage: PluginEditor [<plugin>] [--screenshot <path.png>]\n"
		          << "  <plugin>            a YootTower .t2p file or an OpenSkyscraper\n"
		          << "                      .osp-plugin directory\n"
		          << "  --screenshot <path> save the first rendered frame to <path.png> and\n"
		          << "                      exit (headless smoke test)\n";
	}
}

int main(int argc, char ** argv)
{
	editors::EditorApp app;
	std::string openPath, screenshotPath;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "--screenshot")
		{
			if (i + 1 >= argc) { printUsage(); return 1; }
			screenshotPath = argv[++i];
		}
		else if (!arg.empty() && arg[0] == '-')
		{
			std::cerr << "PluginEditor: unknown option: " << arg << "\n";
			printUsage();
			return 1;
		}
		else if (openPath.empty())
		{
			openPath = arg;
		}
		else
		{
			std::cerr << "PluginEditor: unexpected extra argument: " << arg << "\n";
			printUsage();
			return 1;
		}
	}

	// Validate the open target up front: .t2p files open as t2p, anything
	// else must be an .osp-plugin directory. Unknown args are usage errors.
	if (!openPath.empty())
	{
		const bool isT2p = openPath.size() >= 4
			&& openPath.compare(openPath.size() - 4, 4, ".t2p") == 0;
		if (!isT2p && !OT::isOspPluginDir(openPath))
		{
			std::cerr << "PluginEditor: not a .t2p file or .osp-plugin directory: "
			          << openPath << "\n";
			printUsage();
			return 1;
		}
		app.cliOpenPath = openPath;
	}
	app.cliScreenshotPath = screenshotPath;
	return app.run();
}
