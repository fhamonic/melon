# * Try to find SCIP See http://scip.zib.de/ for more information on SCIP
#
# Once done, this will define
#
# SCIP_INCLUDE_DIRS   - where to find scip/scip.h, etc. SCIP_LIBRARIES      -
# List of libraries when using scip. SCIP_FOUND          - True if scip found.
#
# SCIP_VERSION        - The version of scip found (x.y.z) SCIP_VERSION_MAJOR  -
# The major version of scip SCIP_VERSION_MINOR  - The minor version of scip
# SCIP_VERSION_PATCH  - The patch version of scip
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
# SCIP_LPS             - Set to SPX to force SOPLEX as the LP-Solver and to CPX
# for CPLEX. If not set, first CPLEX is tested, if this is not available SOPLEX
# is tried. SCIP_ROOT            - The preferred installation prefix for
# searching for Scip.  Set this if the module has problems finding the proper
# SCIP installation. SCIP_ROOT is also available as an environment variable.
#
# Author: Wolfgang A. Welz <welz@math.tu-berlin.de>
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

include(LibFindMacros)

# Dependencies
libfind_package(SCIP ZLIB)
# libfind_package(SCIP Readline)

message(STATUS "Check for working SCIP installation")

# If SCIP_ROOT is not set, look for the environment variable
if(NOT SCIP_ROOT AND NOT "$ENV{SCIP_ROOT}" STREQUAL "")
    set(SCIP_ROOT $ENV{SCIP_ROOT})
endif()

set(_SCIP_SEARCHES)

# Search SCIP_ROOT first if it is set.
if(SCIP_ROOT)
    set(_SCIP_SEARCH_ROOT PATHS ${SCIP_ROOT} NO_DEFAULT_PATH)
    list(APPEND _SCIP_SEARCHES _SCIP_SEARCH_ROOT)
endif()

# Normal search.
set(_SCIP_SEARCH_NORMAL PATHS "")
list(APPEND _SCIP_SEARCHES _SCIP_SEARCH_NORMAL)

# Try each search configuration.
foreach(search ${_SCIP_SEARCHES})
    find_path(
        SCIP_INCLUDE_DIR
        NAMES scip/scip.h ${${search}}
        PATH_SUFFIXES src include)
    find_library(
        SCIP_LIBRARY
        NAMES scip ${${search}}
        PATH_SUFFIXES lib)
    find_library(
        OBJSCIP_LIBRARY
        NAMES objscip ${${search}}
        PATH_SUFFIXES lib)
    find_library(
        NLPI_LIBRARY
        NAMES nlpi ${${search}}
        PATH_SUFFIXES lib)
    # now we still need an lp solver
    find_library(
        LPISPX_LIBRARY
        NAMES lpispx ${${search}}
        PATH_SUFFIXES lib)
    find_library(
        LPICPX_LIBRARY
        NAMES lpicpx ${${search}}
        PATH_SUFFIXES lib)
endforeach()

if(SCIP_INCLUDE_DIR AND EXISTS "${SCIP_INCLUDE_DIR}/scip/def.h")
    file(STRINGS "${SCIP_INCLUDE_DIR}/scip/def.h" SCIP_DEF_H
         REGEX "^#define SCIP_VERSION +[0-9]+")
    string(REGEX REPLACE "^#define SCIP_VERSION +([0-9]+).*" "\\1" SVER
                         ${SCIP_DEF_H})

    string(REGEX REPLACE "([0-9]).*" "\\1" SCIP_VERSION_MAJOR ${SVER})
    string(REGEX REPLACE "[0-9]([0-9]).*" "\\1" SCIP_VERSION_MINOR ${SVER})
    string(REGEX REPLACE "[0-9][0-9]([0-9]).*" "\\1" SCIP_VERSION_PATCH ${SVER})
    set(SCIP_VERSION
        "${SCIP_VERSION_MAJOR}.${SCIP_VERSION_MINOR}.${SCIP_VERSION_PATCH}")
endif()

# Now check LP-Solver dependencies
if(SCIP_LIBRARY)
    if(NOT SCIP_LPS STREQUAL "SPX"
       AND NOT LPI_LIBRARIES
       AND LPICPX_LIBRARY)
        find_package(CPLEX QUIET)
        if(CPLEX_FOUND)
            set(LPI_LIBRARIES ${LPICPX_LIBRARY} ${CPLEX_LIBRARIES})
            message(STATUS "  using CPLEX ${CPLEX_VERSION} as the LP-Solver")
        else()
            message(STATUS "  CPLEX not found")
        endif()
    endif(
        NOT SCIP_LPS STREQUAL "SPX"
        AND NOT LPI_LIBRARIES
        AND LPICPX_LIBRARY)

    if(NOT SCIP_LPS STREQUAL "CPX"
       AND NOT LPI_LIBRARIES
       AND LPISPX_LIBRARY)
        find_package(SOPLEX QUIET)
        if(SOPLEX_FOUND)
            set(LPI_LIBRARIES ${LPISPX_LIBRARY} ${SOPLEX_LIBRARIES})
            message(STATUS "  using SOPLEX ${SOPLEX_VERSION} as the LP-Solver")
        else()
            message(STATUS "  SOPLEX not found")
        endif()
    endif(
        NOT SCIP_LPS STREQUAL "CPX"
        AND NOT LPI_LIBRARIES
        AND LPISPX_LIBRARY)

    if(NOT LPI_LIBRARIES)
        message(
            FATAL_ERROR
                "SCIP requires either SOPLEX or CPLEX as an LP-Solver.\nPlease make sure that SCIP is configured for the correct solver and that the corresponding solver-library is installed. You may also set SCIP_LPS, SOPLEX_ROOT, CPLEX_ROOT appropriately."
        )
    endif()
endif()

# Set the include dir variables and the libraries and let libfind_process do the
# rest. NOTE: Singular variables for this library, plural for libraries this
# this lib depends on.
set(SCIP_PROCESS_INCLUDES SCIP_INCLUDE_DIR ZLIB_INCLUDE_DIRS)
set(SCIP_PROCESS_LIBS SCIP_LIBRARY OBJSCIP_LIBRARY NLPI_LIBRARY LPI_LIBRARIES
                      ZLIB_LIBRARIES)
libfind_process(SCIP)
