#pragma once

// Single source of truth for the melon version number: CMakeLists.txt and
// conanfile.py parse these macros, do not change their formatting.
#define MELON_VERSION_MAJOR 1
#define MELON_VERSION_MINOR 0
#define MELON_VERSION_PATCH 0

#define MELON_VERSION                                          \
    (MELON_VERSION_MAJOR * 10000 + MELON_VERSION_MINOR * 100 + \
     MELON_VERSION_PATCH)
