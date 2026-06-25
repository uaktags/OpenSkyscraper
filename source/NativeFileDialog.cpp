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

#ifdef _WIN32
		static Result showWindowsDialog(const std::string &title, const std::string &defaultName, bool save, std::string &path)
		{
			char filename[MAX_PATH] = {0};
			strncpy(filename, defaultName.c_str(), sizeof(filename) - 1);

			OPENFILENAMEA ofn;
			memset(&ofn, 0, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GetActiveWindow();
			ofn.lpstrTitle = title.c_str();
			ofn.lpstrFilter = "Tower saves (*.tower)\0*.tower\0All files (*.*)\0*.*\0";
			ofn.lpstrDefExt = "tower";
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

		static Result showAppleDialog(const std::string &title, const std::string &defaultName, bool save, std::string &path)
		{
			if (!commandExists("osascript"))
				return Result::Unavailable;

			std::string command = "osascript -e ";
			if (save)
			{
				command += shellQuote("set f to choose file name with prompt " + appleScriptQuote(title) +
					" default name " + appleScriptQuote(defaultName));
			}
			else
			{
				command += shellQuote("set f to choose file with prompt " + appleScriptQuote(title) +
					" of type {\"tower\"}");
			}
			command += " -e ";
			command += shellQuote("POSIX path of f");
			return runSelectionCommand(command, path);
		}

		static Result showUnixDialog(const std::string &title, const std::string &defaultName, bool save, std::string &path)
		{
#ifdef __APPLE__
			return showAppleDialog(title, defaultName, save, path);
#else
			if (commandExists("zenity"))
			{
				std::string command = "zenity --file-selection --title=" + shellQuote(title) +
					" --filename=" + shellQuote(defaultName) +
					" --file-filter=" + shellQuote("Tower saves (*.tower) | *.tower") +
					" --file-filter=" + shellQuote("All files | *");
				if (save)
					command += " --save --confirm-overwrite";
				return runSelectionCommand(command, path);
			}

			if (commandExists("kdialog"))
			{
				std::string command = "kdialog ";
				command += save ? "--getsavefilename " : "--getopenfilename ";
				command += shellQuote(defaultName);
				command += " ";
				command += shellQuote("*.tower|Tower saves");
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
			return showWindowsDialog(title, defaultName, false, path);
#else
			return showUnixDialog(title, defaultName, false, path);
#endif
		}

		Result saveTowerFile(const std::string &title, const std::string &defaultName, std::string &path)
		{
#ifdef _WIN32
			return showWindowsDialog(title, defaultName, true, path);
#else
			return showUnixDialog(title, defaultName, true, path);
#endif
		}
	}
}
