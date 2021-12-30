# modeled after FindCOIN.cmake in the lemon project

# Written by: Matthew Gidden Last updated: 12/17/12

# This cmake file is designed to locate coin-related dependencies on a
# filesystem.
#
# If the coin dependencies were installed in a non-standard directory, e.g.
# installed from source perhaps, then the user can provide a prefix hint via the
# COIN_ROOT_DIR cmake variable: $> cmake ../src
# -DCOIN_ROOT_DIR=/path/to/coin/root

# To date, this install requires the following dev versions of the respective
# coin libraries: * coinor-libCbc-dev * coinor-libClp-dev *
# coinor-libcoinutils-dev * coinor-libOsi-dev

#
# Get the root directory hint if provided
#
if(NOT DEFINED COIN_ROOT_DIR)
    set(COIN_ROOT_DIR "$ENV{COIN_ROOT_DIR}")
    message("\tCOIN Root Dir: ${COIN_INCLUDE_DIR}")
endif(NOT DEFINED COIN_ROOT_DIR)
message(STATUS "COIN_ROOT_DIR hint is : ${COIN_ROOT_DIR}")

#
# Find the path based on a required header file
#
message(STATUS "Coin multiple library dependency status:")
find_path(
    COIN_INCLUDE_DIR coin/CbcModel.hpp
    HINTS "${COIN_INCLUDE_DIR}"
    HINTS "${COIN_ROOT_DIR}/include"
    HINTS /usr/
    HINTS /usr/include/
    HINTS /usr/local/
    HINTS /usr/local/include/
    HINTS /usr/coin/
    HINTS /usr/coin-Cbc/
    HINTS /usr/local/coin/
    HINTS /usr/local/coin-Cbc/)
set(COIN_INCLUDE_DIR ${COIN_INCLUDE_DIR}/coin)
message("\tCOIN Include Dir: ${COIN_INCLUDE_DIR}")

#
# Find all coin library dependencies
#
find_library(
    COIN_CBC_LIBRARY
    NAMES Cbc libCbc # libCbc.so.0
    HINTS "${COIN_INCLUDE_DIR}/../../lib/"
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN CBC: ${COIN_CBC_LIBRARY}")

find_library(
    COIN_CBC_SOLVER_LIBRARY
    NAMES CbcSolver libCbcSolver libCbcSolver.so.0
    HINTS ${COIN_INCLUDE_DIR}/../../lib/
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN CBC solver: ${COIN_CBC_SOLVER_LIBRARY}")

find_library(
    COIN_CGL_LIBRARY
    NAMES Cgl libCgl libCgl.so.0
    HINTS ${COIN_INCLUDE_DIR}/../../lib/
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN CGL: ${COIN_CGL_LIBRARY}")

find_library(
    COIN_CLP_LIBRARY
    NAMES Clp libClp # libClp.so.0
    HINTS ${COIN_INCLUDE_DIR}/../../lib/
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN CLP: ${COIN_CLP_LIBRARY}")

find_library(
    COIN_COIN_UTILS_LIBRARY
    NAMES CoinUtils libCoinUtils # libCoinUtils.so.0
    HINTS ${COIN_INCLUDE_DIR}/../../lib/
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN UTILS: ${COIN_COIN_UTILS_LIBRARY}")

find_library(
    COIN_OSI_LIBRARY
    NAMES Osi libOsi # libOsi.so.0
    HINTS ${COIN_INCLUDE_DIR}/../../lib/
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN OSI: ${COIN_OSI_LIBRARY}")

#
# Not required by cbc v2.5, but required by later versions
#
# FIND_LIBRARY(COIN_OSI_CBC_LIBRARY NAMES OsiCbc libOsiCbc libOsiCbc.so.0 HINTS
# ${COIN_INCLUDE_DIR}/../../lib/ HINTS "${COIN_ROOT_DIR}/lib" ) MESSAGE("\tCOIN
# OSI CBC: ${COIN_OSI_CBC_LIBRARY}")

find_library(
    COIN_OSI_CLP_LIBRARY
    NAMES OsiClp libOsiClp libOsiClp.so.0
    HINTS ${COIN_INCLUDE_DIR}/../../lib/
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN OSI CLP: ${COIN_OSI_CLP_LIBRARY}")

find_library(
    COIN_ZLIB_LIBRARY
    NAMES z libz libz.so.1
    HINTS ${COIN_ROOT_DIR}/lib
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN ZLIB: ${COIN_ZLIB_LIBRARY}")

find_library(
    COIN_BZ2_LIBRARY
    NAMES bz2 libz2 libz2.so.1
    HINTS ${COIN_ROOT_DIR}/lib
    HINTS "${COIN_ROOT_DIR}/lib")
message("\tCOIN BZ2: ${COIN_BZ2_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    COIN
    DEFAULT_MSG
    COIN_INCLUDE_DIR
    COIN_CBC_LIBRARY
    COIN_CBC_SOLVER_LIBRARY
    COIN_CGL_LIBRARY
    COIN_CLP_LIBRARY
    COIN_COIN_UTILS_LIBRARY
    COIN_OSI_LIBRARY
    # Not required by cbc v2.5, but required by later versions
    # COIN_OSI_CBC_LIBRARY
    COIN_OSI_CLP_LIBRARY
    COIN_ZLIB_LIBRARY
    COIN_BZ2_LIBRARY)

#
# Set all required cmake variables based on our findings
#
if(COIN_FOUND)
    set(COIN_INCLUDE_DIRS ${COIN_INCLUDE_DIR})
    set(COIN_CLP_LIBRARIES
        "${COIN_CLP_LIBRARY};${COIN_COIN_UTILS_LIBRARY};${COIN_ZLIB_LIBRARY}")
    if(COIN_ZLIB_LIBRARY)
        set(COIN_CLP_LIBRARIES "${COIN_CLP_LIBRARIES};${COIN_ZLIB_LIBRARY}")
    endif(COIN_ZLIB_LIBRARY)
    if(COIN_BZ2_LIBRARY)
        set(COIN_CLP_LIBRARIES "${COIN_CLP_LIBRARIES};${COIN_BZ2_LIBRARY}")
    endif(COIN_BZ2_LIBRARY)
    # Not required by cbc v2.5, but required by later versions in which case,
    # the lower line should be commented out and this line used
    # SET(COIN_CBC_LIBRARIES
    # "${COIN_CBC_LIBRARY};${COIN_CBC_SOLVER_LIBRARY};${COIN_CGL_LIBRARY};${COIN_OSI_LIBRARY};${COIN_OSI_CBC_LIBRARY};${COIN_OSI_CLP_LIBRARY};${COIN_CLP_LIBRARIES}")
    set(COIN_CBC_LIBRARIES
        "${COIN_CBC_LIBRARY};${COIN_CBC_SOLVER_LIBRARY};${COIN_CGL_LIBRARY};${COIN_OSI_LIBRARY};${COIN_OSI_CLP_LIBRARY};${COIN_CLP_LIBRARIES}"
    )
    set(COIN_LIBRARIES ${COIN_CBC_LIBRARIES})
endif(COIN_FOUND)

#
# Report a synopsis of our findings
#
if(COIN_INCLUDE_DIRS)
    message(STATUS "Found COIN Include Dirs: ${COIN_INCLUDE_DIRS}")
else(COIN_INCLUDE_DIRS)
    message(STATUS "COIN Include Dirs NOT FOUND")
endif(COIN_INCLUDE_DIRS)
