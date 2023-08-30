cmake_minimum_required(VERSION 3.12)

project(
    MELON
    VERSION 1.0
    DESCRIPTION "Modern and Efficient Library for Optimization in Networks."
    LANGUAGES CXX)

# ################### Options ####################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Enable Test Builds" OFF)

# ################### Modules ####################
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}" ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(GNUInstallDirs)
include(CompilerWarnings)

# ################### Packages ###################
find_package(range-v3)
find_package(fmt)

# ################### Library ####################
add_library(melon INTERFACE)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(melon INTERFACE range-v3::range-v3 fmt::fmt)
set_project_warnings(melon)

# #################### TESTS #####################
if(BUILD_TESTS)
    enable_testing()
    message("Building Tests.")
    target_compile_options(melon INTERFACE -fconcepts-diagnostics-depth=10)
    remove_definitions("-D NDEBUG")
    add_subdirectory(test)
endif()
