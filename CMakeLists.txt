cmake_minimum_required(VERSION 3.12)
project(
    Melon
    VERSION 0.6.0
    DESCRIPTION "Modern and Efficient Library for Optimization in Networks."
    LANGUAGES CXX)

# ################### Options ####################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ################### Modules ####################
include(GNUInstallDirs)

# ################### Packages ###################
find_package(range-v3)
find_package(fmt)

# ################### Library ####################
add_library(melon INTERFACE)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(melon INTERFACE range-v3::range-v3 fmt::fmt)
