cmake_minimum_required(VERSION 3.12)

project(
    MELON
    VERSION 1.0
    DESCRIPTION "Modern and Efficient Library for Optimization in Networks."
    LANGUAGES CXX)

# set(CMAKE_VERBOSE_MAKEFILE ON)

# ################### Options ####################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WARNINGS "" OFF)
option(HARDCORE_WARNINGS "" OFF)

set(WARNINGS_LIST "-Wall;-Wextra;")
set(HARDCORE_WARNINGS_LIST
    "-Wall;-Wextra;-Wshadow;-Wpedantic;-Wnon-virtual-dtor;-Wold-style-cast;-Wcast-align;-Wunused;-Woverloaded-virtual;-Wpedantic;-Wconversion;-Wsign-conversion;-Wmisleading-indentation;-Wduplicated-cond;-Wduplicated-branches;-Wlogical-op;-Wnull-dereference;-Wuseless-cast;-Wdouble-promotion;-Wformat=2;-Wlifetime"
)

# ################### Modules ####################
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(GNUInstallDirs)
include(CMakeBuildTypeUtils)

# ################### Packages ####################
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# ################### Library ####################
add_library(melon INTERFACE)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(melon INTERFACE CONAN_PKG::range-v3)
target_compile_options_if_exists(melon INTERFACE ${WARNINGS} ${WARNINGS_LIST})
target_compile_options_if_exists(melon INTERFACE ${HARDCORE_WARNINGS}
                                 ${HARDCORE_WARNINGS_LIST})

# ################## TEST target ####################
enable_testing()

add_executable(melon_test test/main_test.cpp test/static_digraph_test.cpp test/static_digraph_builder_test.cpp)
target_link_libraries(melon_test CONAN_PKG::gtest)
target_link_libraries(melon_test melon)

include(GoogleTest)
gtest_discover_tests(melon_test)

# ################## EXEC target ####################
add_executable(main test/main.cpp)
target_include_directories(main PUBLIC include)
target_link_libraries(main melon)
