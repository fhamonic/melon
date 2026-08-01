cmake_minimum_required(VERSION 3.24)

# The version number is parsed from include/melon/version.hpp, its single
# source of truth (conanfile.py parses it too).
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/melon/version.hpp"
     _melon_version_hpp)
foreach(level MAJOR MINOR PATCH)
    if(NOT _melon_version_hpp MATCHES
       "#define MELON_VERSION_${level} ([0-9]+)")
        message(FATAL_ERROR
                "Failed to parse MELON_VERSION_${level} from version.hpp")
    endif()
    set(MELON_VERSION_${level} ${CMAKE_MATCH_1})
endforeach()

project(
    Melon
    VERSION
        ${MELON_VERSION_MAJOR}.${MELON_VERSION_MINOR}.${MELON_VERSION_PATCH}
    DESCRIPTION "Modern and Efficient Library for Optimization in Networks."
    HOMEPAGE_URL "https://github.com/fhamonic/melon"
    LANGUAGES CXX)

# ################### Options ####################
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(MELON_BUILD_TESTS "Build the melon test suite" ${PROJECT_IS_TOP_LEVEL})
option(MELON_FROM_CONAN "Indicates the project is configured from Conan" OFF)
# A C++26 standard library gives undirect_view::incidence(u) a sized, common,
# random-access range where C++23 gives a forward one; this option forces the
# C++23 shape so a C++26 build catches code that would not compile on the
# documented GCC 14 minimum. See include/melon/detail/concat_view.hpp.
option(MELON_PORTABLE_RANGE_SHAPES
       "Use melon's concat fallback even where std::views::concat exists" OFF)

# ################### Modules ####################
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ############### Exported target ################
add_library(melon INTERFACE)
add_library(melon::melon ALIAS melon)
target_compile_features(melon INTERFACE cxx_std_23)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# On the interface target, so every consumer of this build -- and of an
# install made from it, since the definition is exported into melonTargets --
# compiles with the same setting. A program in which only some translation
# units define it is ill-formed.
if(MELON_PORTABLE_RANGE_SHAPES)
    target_compile_definitions(melon INTERFACE MELON_PORTABLE_RANGE_SHAPES)
endif()

# ################# Installation #################
install(TARGETS melon EXPORT melonTargets)

# melon/experimental/ ships, but the two headers that are still unfinished
# (they do not compile) are held back; see their file-level comments.
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/melon
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PATTERN "scapegoat_tree.hpp" EXCLUDE
        PATTERN "doubly_connected_digraph.hpp" EXCLUDE)

# Distribution packagers expect the license next to the headers; the Conan
# recipe ships its own copy under licenses/. Hardcoded "melon" rather than
# ${PROJECT_NAME}, which is capitalised, to match include/melon and
# lib/cmake/melon.
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/melon)

install(
    EXPORT melonTargets
    NAMESPACE melon::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/melon)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/melonConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/melonConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/melon)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/melonConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/melonConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/melonConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/melon)

# ################### Tests ####################
if(MELON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
