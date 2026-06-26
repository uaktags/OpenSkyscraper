/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace OT
{
	bool Plugin::ensureLoaded() const
	{
		if (loaded_) return exe_ != nullptr;
		auto e = std::make_unique<WindowsPEExecutable>();
		if (!e->load(path_)) {
			// Leave loaded_ false so a future call can retry; useful when
			// the failure is transient (e.g. file locked on Windows).
			exe_.reset();
			return false;
		}
		exe_ = std::move(e);
		loaded_ = true;
		return true;
	}

	const WindowsPEExecutable::Resource * Plugin::getResource(const std::string & type, int id) const
	{
		if (!ensureLoaded()) return nullptr;
		auto tit = exe_->resources.find(type);
		if (tit == exe_->resources.end()) return nullptr;
		auto iit = tit->second.find(id);
		if (iit == tit->second.end()) return nullptr;
		return &iit->second;
	}

	std::vector<Plugin::ResourceKey> Plugin::enumerateResources() const
	{
		std::vector<ResourceKey> out;
		if (!ensureLoaded()) return out;
		out.reserve(32);
		for (const auto & tp : exe_->resources)
			for (const auto & ip : tp.second)
				out.push_back({tp.first, ip.first});
		return out;
	}

	static std::string lowerExt(const std::string & s)
	{
		std::string out;
		out.reserve(s.size());
		for (char c : s)
			out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		return out;
	}

	int PluginManager::addPluginDirectory(const Path & dir)
	{
		// Skip directories we've already scanned so re-init is a no-op.
		for (const Path & scanned : scannedDirs_)
			if (scanned == dir) return 0;
		scannedDirs_.push_back(dir);

		const std::string dirStr = dir.str();
		std::error_code ec;
		if (!fs::is_directory(dirStr, ec)) return 0;

		int added = 0;
		for (const auto & entry : fs::directory_iterator(dirStr, ec))
		{
			if (ec) break;
			if (!entry.is_regular_file()) continue;
			auto p = entry.path();
			if (lowerExt(p.extension().string()) != ".t2p") continue;

			std::string name = p.filename().string();
			if (plugins_.count(name)) continue;
			plugins_.emplace(name, std::make_unique<Plugin>(name, Path(p.string())));
			++added;
		}
		return added;
	}

	bool PluginManager::has(const std::string & name) const
	{
		return plugins_.count(name) > 0;
	}

	Plugin * PluginManager::get(const std::string & name) const
	{
		auto it = plugins_.find(name);
		return it == plugins_.end() ? nullptr : it->second.get();
	}

	std::vector<std::string> PluginManager::enumerate() const
	{
		std::vector<std::string> names;
		names.reserve(plugins_.size());
		for (const auto & p : plugins_) names.push_back(p.first);
		std::sort(names.begin(), names.end());
		return names;
	}

	int PluginManager::countEssential(const std::vector<std::string> & essential) const
	{
		int count = 0;
		for (const std::string & name : essential)
			if (plugins_.count(name)) ++count;
		return count;
	}
}
