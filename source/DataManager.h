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
		
	protected:
		friend class Application;
		Application * const app;
		DataManager(Application * app);
		
		void init();
		Path getPrimaryDataPath() const {
			if (dirs.empty()) {
				return Path("."); // Fallback
			}
			return dirs.front(); // Return the first, most preferred data directory
		}
	private:
		Paths dirs;
	};
}
