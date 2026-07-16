cmake_minimum_required(VERSION 3.12)
project(
    Melon
    VERSION 0.6.0
    DESCRIPTION "Modern and Efficient Library for Optimization in Networks."
    LANGUAGES CXX)

# ################### Options ####################
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(MELON_BUILD_TESTS "Build the melon test suite" ON)
option(MELON_FROM_CONAN "Indicates the project is configured from Conan" OFF)

set(CMAKE_CXX_STANDARD 23)

# ################### Modules ####################
include(GNUInstallDirs)

# ################### Library ####################
add_library(melon INTERFACE)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# ################### Tests ####################
if(MELON_BUILD_TESTS)
    add_subdirectory(test)    
endif()
