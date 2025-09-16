#include "Filesystem.h"
#include <sys/stat.h>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

using namespace OT;

bool Filesystem::exists(const Path &p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

bool Filesystem::ensureDirectory(const Path &dir)
{
    if (exists(dir)) return true;
    Path parent = dir.up();
    if (!exists(parent)) {
        if (!ensureDirectory(parent)) return false;
    }
    if (mkdir(dir.c_str(), 0777) != 0) {
        return false;
    }
    return true;
}

std::string Filesystem::userDataDir()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string dir = std::string(path) + "\\OpenSkyscraper";
        return dir;
    }
    return ".";
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/Library/Application Support/OpenSkyscraper";
    return ".";
#else
    const char* home = getenv("HOME");
    if (home) return std::string(home) + "/.openskyscraper";
    return ".";
#endif
}

void Filesystem::ensureDirExists(const std::string &dir)
{
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), NULL);
#else
    mkdir(dir.c_str(), 0755);
#endif
}
