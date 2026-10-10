#include <cassert>
#include <sys/stat.h>

#include "Application.h"
#include "DataManager.h"

using namespace OT;
using std::string;

bool DirectoryExists(const char * path)
{
	struct stat st;
	if(stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
		return true;
	return false;
}

DataManager::DataManager(Application * app)
:	app(app)
{
	assert(app != NULL && "DataManager requires an app");
}

void DataManager::init()
{
	LOG(INFO, "initializing data dirs");

	Path path = app->getPath();
	string name = /*path.name()*/"OpenSkyscraper";
#ifdef __APPLE__
# ifdef BUILD_DEBUG
	//TODO: there's an issue with climbing up paths that go like ../../, since path.up() will first erase these ../ segments, instead of directly appending to them.
	//dirs.push_back(path.up(0).down("data"));
	dirs.push_back(Path("data"));
	dirs.push_back(path.up(1).down("Resources"));
# endif
	dirs.push_back(Path("~/Library/Application Support/OpenSkyscraper"));
	dirs.push_back(Path("/Library/Application Support/OpenSkyscraper"));
	dirs.push_back(".");
#else
	dirs.push_back(path.up(2).down("data"));
	dirs.push_back(path.up().down("data"));
	dirs.push_back(path.down("data"));
	dirs.push_back(Path("data"));
# ifndef _WIN32
	dirs.push_back(Path("~").down(".openskyscraper"));
	dirs.push_back(Path("/usr/local/share/openskyscraper"));
	dirs.push_back(Path("/usr/share/openskyscraper"));
# endif
#endif

	Paths existing;
	for (Paths::const_iterator it = dirs.begin(); it != dirs.end(); ++it) {
		bool exists = DirectoryExists(it->c_str());
		LOG(DEBUG, (exists ? "  %s" : "  %s (not found)"), it->c_str());
		if (exists)
			existing.push_back(*it);
	}
	std::swap(dirs, existing);
}

/** Returns a list of possible paths to the given resource. */
DataManager::Paths DataManager::paths(Path resource) const
{
	Paths p;
	for (Paths::const_iterator d = dirs.begin(); d != dirs.end(); d++)
		p.push_back(*d + resource);
	return p;
}

/** Returns true if the given resource exists in any of the data dirs. */
bool DataManager::exists(Path resource) const
{
	for (Paths::const_iterator d = dirs.begin(); d != dirs.end(); d++) {
		Path p = *d + resource;
		struct stat st;
		if (stat(p.c_str(), &st) == 0)
			return true;
	}
	return false;
}

/** Adds a directory to the runtime search path. Directories picked by the
 *  user at runtime (e.g. the folder containing a located SIMTOWER.EXE) are
 *  injected here so subsequent resource lookups find them. */
void DataManager::addSearchPath(Path dir, bool front)
{
	if (dir.str().empty())
		return;
	for (Paths::const_iterator it = dirs.begin(); it != dirs.end(); ++it) {
		if (*it == dir)
			return;
	}
	struct stat st;
	if (stat(dir.c_str(), &st) != 0 || (st.st_mode & S_IFMT) != S_IFDIR) {
		LOG(DEBUG, "  not adding search dir %s (missing)", dir.c_str());
		return;
	}
	if (front)
		dirs.insert(dirs.begin(), dir);
	else
		dirs.push_back(dir);
	LOG(INFO, "added data search dir %s", dir.c_str());
}
