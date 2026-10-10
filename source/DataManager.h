#pragma once
#include <vector>
#include "Path.h"

namespace OT
{
	class Application;
	
	class DataManager
	{
	public:
		typedef std::vector<Path> Paths;
		
		Paths paths(Path resource) const;
		bool exists(Path resource) const;
		void addSearchPath(Path dir, bool front = true);

		/// Read-only view of the currently-configured base search dirs.
		/// Exposed so PluginManager can scan each base dir's Plugins/
		/// subdirectory without being coupled to DataManager internals.
		const Paths & searchDirs() const { return dirs; }
		
	protected:
		friend class Application;
		Application * const app;
		DataManager(Application * app);
		
		void init();
		
	private:
		Paths dirs;
	};
}
