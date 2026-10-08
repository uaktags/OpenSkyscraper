/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginManager.h"
#include "DecompiledPlugin.h"

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

	ResourceContainer Plugin::container() const
	{
		if (ensureLoaded() && exe_->format == WindowsPEExecutable::Format::SP)
			return ResourceContainer::SP;
		return ResourceContainer::PE;
	}

	std::vector<AbstractPlugin::ResourceKey> Plugin::enumerateResources() const
	{
		std::vector<ResourceKey> out;
		if (!ensureLoaded()) return out;
		out.reserve(32);
		for (const auto & tp : exe_->resources)
			for (const auto & ip : tp.second)
				out.push_back({tp.first, ip.first});
		return out;
	}

	std::map<std::string, std::vector<int>> Plugin::resourcesByType() const
	{
		std::map<std::string, std::vector<int>> out;
		if (!ensureLoaded()) return out;
		for (const auto & tp : exe_->resources) {
			std::vector<int> & ids = out[tp.first];
			ids.reserve(tp.second.size());
			// WindowsPEExecutable::Resources is a std::map<int, ...>,
			// so iteration already yields ascending ids. We copy in
			// that order so callers can binary-search the result.
			for (const auto & ip : tp.second)
				ids.push_back(ip.first);
		}
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

	/// Builds the catalog key for a folder plugin discovered at `child`
	/// whose scan root is `root`. The key is the relative path with
	/// forward slashes (e.g. root=".../Plugins", child=".../Plugins/OpenTower/Office"
	/// → "OpenTower/Office"). On Windows the path uses backslashes; we
	/// normalise so the key is stable cross-platform and cannot collide
	/// with a "*.t2p" filename (no extension).
	static std::string folderPluginName(const fs::path & root, const fs::path & child)
	{
		std::string rel = fs::relative(child, root).string();
		for (char & c : rel)
			if (c == '\\') c = '/';
		return rel;
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
			auto p = entry.path();

			// .t2p PE files.
			if (entry.is_regular_file() && lowerExt(p.extension().string()) == ".t2p")
			{
				std::string name = p.filename().string();
				if (plugins_.count(name)) continue;
				plugins_.emplace(name, std::make_unique<Plugin>(name, Path(p.string())));
				++added;
				continue;
			}

			// Folder plugins: a subdirectory containing plugin.json. Only
			// nested one level deep — we don't recurse arbitrarily, to keep
			// the catalog predictable and the key simple. Edition authors
			// place plugins directly under <root>/<Edition>/<Plugin>.
			if (entry.is_directory())
			{
				for (const auto & sub : fs::directory_iterator(p, ec))
				{
					if (ec) break;
					if (!sub.is_directory()) continue;
					fs::path manifest = sub.path() / "plugin.json";
					std::error_code existsEc;
					if (!fs::exists(manifest, existsEc)) continue;

					std::string name = folderPluginName(dirStr, sub.path());
					if (plugins_.count(name)) continue;
					plugins_.emplace(name, std::make_unique<DecompiledPlugin>(
					    name, Path(sub.path().string())));
					++added;
				}
			}
		}
		return added;
	}

	bool PluginManager::has(const std::string & name) const
	{
		return plugins_.count(name) > 0;
	}

	AbstractPlugin * PluginManager::get(const std::string & name) const
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
