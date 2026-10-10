/* Copyright (c) 2012-2015 Fabian Schuiki */
#pragma once

#include <string>

namespace OT
{
	namespace NativeFileDialog
	{
		enum class Result
		{
			Selected,
			Cancelled,
			Unavailable
		};

		Result openTowerFile(const std::string &title, const std::string &defaultName, std::string &path);
		Result saveTowerFile(const std::string &title, const std::string &defaultName, std::string &path);
		Result openExecutableFile(const std::string &title, std::string &path);

		/// Pick an existing YootTower plugin file (.t2p). Used by the plugin
		/// editor's "Open .t2p" action.
		Result openPluginFile(const std::string &title, std::string &path);

		/// Pick an existing OpenSkyscraper plugin directory (.osp-plugin).
		Result openDirectory(const std::string &title, std::string &path);

		/// Choose a path for a new/existing directory (Save-As style). The
		/// directory need not exist; the caller creates it. Used by the plugin
		/// editor's "Save" / "Convert" actions.
		Result saveDirectory(const std::string &title, const std::string &defaultName, std::string &path);

		/// Pick an existing PNG image file. Used by the plugin editor's
		/// "Import PNG" action for sprite sheets.
		Result openPngFile(const std::string &title, std::string &path);

		/// Choose a path to save a PNG to. Used by the plugin editor's
		/// "Export PNG" action.
		Result savePngFile(const std::string &title, const std::string &defaultName, std::string &path);
	}
}
