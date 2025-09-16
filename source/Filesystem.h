/* Small cross-platform filesystem helpers to replace ad-hoc code.
 */
#pragma once

#include <string>
#include "Path.h"

namespace OT {

class Filesystem {
public:
    // Returns true if the given path exists on disk.
    static bool exists(const Path &p);

    // Ensures the given directory exists, creating parents as necessary.
    static bool ensureDirectory(const Path &dir);

    // Cross-platform user data directory for storing user files (saves, cache).
    static std::string userDataDir();

    // Ensure a generic string directory exists (helper for Application compatibility).
    static void ensureDirExists(const std::string &dir);
};

}
