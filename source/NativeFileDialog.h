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
	}
}
