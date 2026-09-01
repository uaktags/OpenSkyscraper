/* Copyright (c) 2012-2015 Fabian Schuiki */
#include "NativeFileDialog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace OT
{
	namespace NativeFileDialog
	{
		static std::string stripTrailingNewline(std::string value)
		{
			while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
				value.pop_back();
			return value;
		}

		// Filter description used by the platform dialogs. kind selects the
		// extensions and default extension offered to the user.
		enum class FileKind { Tower, TowerLegacy, Executable, Plugin, Png };
		struct FilterInfo
		{
			std::string winFilter; // double-NUL terminated pair list for Win32
			std::string defExt;
			std::string zenityFilter;
			std::string kdialogFilter;
			std::string appleType;
		};
		// Build a Win32 lpstrFilter buffer (desc\0spec\0...\0\0) from pairs.
		static std::string buildWinFilter(const char *desc, const char *spec)
		{
			std::string out;
			out += desc;  out += '\0';
			out += spec;  out += '\0';
			out += "All files"; out += '\0';
			out += "*.*"; out += '\0';
			out += '\0';
			return out;
		}
			static FilterInfo makeFilter(FileKind kind)
		{
			if (kind == FileKind::Executable)
			{
				FilterInfo f;
				f.winFilter = buildWinFilter("Executables (*.exe;*.EX_)", "*.exe;*.EX_");
				f.defExt = "exe";
				f.zenityFilter = "Executables (*.exe *.EX_) | *.exe *.EX_";
				f.kdialogFilter = "*.exe *.EX_|SimTower executable";
				f.appleType = "";
				return f;
			}
			if (kind == FileKind::Plugin)
			{
				// YootTower plugins ship as .t2p (Win32 PE32 DLLs).
				FilterInfo f;
				f.winFilter = buildWinFilter("YootTower plugins (*.t2p)", "*.t2p");
				f.defExt = "t2p";
				f.zenityFilter = "YootTower plugins (*.t2p) | *.t2p";
				f.kdialogFilter = "*.t2p|YootTower plugins";
				f.appleType = "";
				return f;
			}
			if (kind == FileKind::Png)
			{
				FilterInfo f;
				f.winFilter = buildWinFilter("PNG images (*.png)", "*.png");
				f.defExt = "png";
				f.zenityFilter = "PNG images (*.png) | *.png";
				f.kdialogFilter = "*.png|PNG images";
				f.appleType = "";
				return f;
			}
			if (kind == FileKind::TowerLegacy)
			{
				// Opening also accepts original 1994 SimTower .TDT saves
				// (imported via TdtLoader); saving stays .tower-only.
				FilterInfo f;
				f.winFilter = buildWinFilter("Tower saves (*.tower;*.tdt)", "*.tower;*.tdt");
				f.defExt = "tower";
				f.zenityFilter = "Tower saves (*.tower *.tdt) | *.tower *.tdt";
				f.kdialogFilter = "*.tower *.tdt|Tower saves";
				f.appleType = "";
				return f;
			}
			FilterInfo f;
			f.winFilter = buildWinFilter("Tower saves (*.tower)", "*.tower");
			f.defExt = "tower";
			f.zenityFilter = "Tower saves (*.tower) | *.tower";
			f.kdialogFilter = "*.tower|Tower saves";
			f.appleType = "tower";
			return f;
		}

#ifdef _WIN32
		static Result showWindowsDialog(const std::string &title, const std::string &defaultName, bool save, FileKind kind, std::string &path)
		{
			char filename[MAX_PATH] = {0};
			strncpy(filename, defaultName.c_str(), sizeof(filename) - 1);

			const FilterInfo filter = makeFilter(kind);
			OPENFILENAMEA ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GetActiveWindow();
			ofn.lpstrTitle = title.c_str();
			ofn.lpstrFilter = filter.winFilter.c_str();
			ofn.lpstrDefExt = filter.defExt.c_str();
			ofn.lpstrFile = filename;
			ofn.nMaxFile = sizeof(filename);
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
			if (save)
				ofn.Flags |= OFN_OVERWRITEPROMPT;
			else
				ofn.Flags |= OFN_FILEMUSTEXIST;

			BOOL accepted = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
			if (!accepted)
				return CommDlgExtendedError() == 0 ? Result::Cancelled : Result::Unavailable;

			path = filename;
			return path.empty() ? Result::Cancelled : Result::Selected;
		}
#else
		static std::string shellQuote(const std::string &value)
		{
			std::string out = "'";
			for (char c : value)
			{
				if (c == '\'')
					out += "'\\''";
				else
					out += c;
			}
			out += "'";
			return out;
		}

		static std::string appleScriptQuote(const std::string &value)
		{
			std::string out = "\"";
			for (char c : value)
			{
				if (c == '\\' || c == '"')
					out += '\\';
				out += c;
			}
			out += "\"";
			return out;
		}

		static bool commandExists(const char *command)
		{
			std::string probe = "command -v ";
			probe += command;
			probe += " >/dev/null 2>&1";
			return system(probe.c_str()) == 0;
		}

		static Result runSelectionCommand(const std::string &command, std::string &path)
		{
			FILE *pipe = popen(command.c_str(), "r");
			if (!pipe)
				return Result::Unavailable;

			char buffer[4096];
			std::string result;
			while (fgets(buffer, sizeof(buffer), pipe))
				result += buffer;
			const int status = pclose(pipe);
			if (status != 0)
				return Result::Cancelled;

			path = stripTrailingNewline(result);
			return path.empty() ? Result::Cancelled : Result::Selected;
		}

		static Result showAppleDialog(const std::string &title, const std::string &defaultName, bool save, FileKind kind, std::string &path)
		{
			if (!commandExists("osascript"))
				return Result::Unavailable;

			const FilterInfo filter = makeFilter(kind);
			std::string command = "osascript -e ";
			if (save)
			{
				command += shellQuote("set f to choose file name with prompt " + appleScriptQuote(title) +
					" default name " + appleScriptQuote(defaultName));
			}
			else
			{
				std::string typeClause;
				if (!filter.appleType.empty())
					typeClause = " of type {\"" + filter.appleType + "\"}";
				command += shellQuote("set f to choose file with prompt " + appleScriptQuote(title) + typeClause);
			}
			command += " -e ";
			command += shellQuote("POSIX path of f");
			return runSelectionCommand(command, path);
		}

		static Result showUnixDialog(const std::string &title, const std::string &defaultName, bool save, FileKind kind, std::string &path)
		{
#ifdef __APPLE__
			return showAppleDialog(title, defaultName, save, kind, path);
#else
			const FilterInfo filter = makeFilter(kind);
			if (commandExists("zenity"))
			{
				std::string command = "zenity --file-selection --title=" + shellQuote(title);
				if (!defaultName.empty())
					command += " --filename=" + shellQuote(defaultName);
				command += " --file-filter=" + shellQuote(filter.zenityFilter);
				command += " --file-filter=" + shellQuote("All files | *");
				if (save)
					command += " --save --confirm-overwrite";
				return runSelectionCommand(command, path);
			}

			if (commandExists("kdialog"))
			{
				std::string command = "kdialog ";
				command += save ? "--getsavefilename " : "--getopenfilename ";
				command += shellQuote(defaultName.empty() ? "." : defaultName);
				command += " ";
				command += shellQuote(filter.kdialogFilter);
				command += " --title ";
				command += shellQuote(title);
				return runSelectionCommand(command, path);
			}

			return Result::Unavailable;
#endif
		}

		// Directory selection (no FileKind — directories have no filter).
		static Result showUnixDirectoryDialog(const std::string &title,
		                                      const std::string &defaultName,
		                                      bool save, std::string &path)
		{
#ifdef __APPLE__
			(void)save;
			if (!commandExists("osascript"))
				return Result::Unavailable;
			std::string command = "osascript -e ";
			command += shellQuote("set f to choose folder with prompt " + appleScriptQuote(title));
			command += " -e ";
			command += shellQuote("POSIX path of f");
			return runSelectionCommand(command, path);
#else
			if (commandExists("zenity"))
			{
				std::string command = "zenity --file-selection --directory --title=" + shellQuote(title);
				if (!defaultName.empty())
					command += " --filename=" + shellQuote(defaultName);
				if (save)
					command += " --save --confirm-overwrite";
				return runSelectionCommand(command, path);
			}
			if (commandExists("kdialog"))
			{
				// --getexistingdirectory for open; kdialog has no separate
				// "save directory" verb, so save uses it too (caller creates).
				std::string command = "kdialog --getexistingdirectory ";
				command += shellQuote(defaultName.empty() ? "." : defaultName);
				command += " --title ";
				command += shellQuote(title);
				return runSelectionCommand(command, path);
			}
			return Result::Unavailable;
#endif
		}
#endif

		Result openTowerFile(const std::string &title, const std::string &defaultName, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, defaultName, false, FileKind::TowerLegacy, path);
#else
			return showUnixDialog(title, defaultName, false, FileKind::TowerLegacy, path);
#endif
		}

		Result saveTowerFile(const std::string &title, const std::string &defaultName, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, defaultName, true, FileKind::Tower, path);
#else
			return showUnixDialog(title, defaultName, true, FileKind::Tower, path);
#endif
		}

		Result openExecutableFile(const std::string &title, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, "SIMTOWER.EXE", false, FileKind::Executable, path);
#else
			return showUnixDialog(title, "", false, FileKind::Executable, path);
#endif
		}

		Result openPluginFile(const std::string &title, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, "", false, FileKind::Plugin, path);
#else
			return showUnixDialog(title, "", false, FileKind::Plugin, path);
#endif
		}

		Result openDirectory(const std::string &title, std::string &path)
		{
			// No Win32 directory picker is wired yet (would need IFileDialog
			// with FOS_PICKFOLDERS); fall through to the unix/apple backends,
			// which cover the platforms the editor targets today.
#ifndef _WIN32
			return showUnixDirectoryDialog(title, "", false, path);
#else
			(void)title; (void)path;
			return Result::Unavailable;
#endif
		}

		Result saveDirectory(const std::string &title, const std::string &defaultName, std::string &path)
		{
#ifndef _WIN32
			return showUnixDirectoryDialog(title, defaultName, true, path);
#else
			(void)title; (void)defaultName; (void)path;
			return Result::Unavailable;
#endif
		}

		Result openPngFile(const std::string &title, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, "", false, FileKind::Png, path);
#else
			return showUnixDialog(title, "", false, FileKind::Png, path);
#endif
		}

		Result savePngFile(const std::string &title, const std::string &defaultName, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, defaultName, true, FileKind::Png, path);
#else
			return showUnixDialog(title, defaultName, true, FileKind::Png, path);
#endif
		}
	}
}
