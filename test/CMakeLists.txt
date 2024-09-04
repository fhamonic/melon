cmake_minimum_required(VERSION 3.13)
project(MelonUnitTests CXX)

set(CMAKE_CXX_STANDARD 20)
 
# ################### Modules ####################
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(CompilerWarnings)

# ################### Packages ###################
find_package(range-v3)
find_package(fmt)
find_package(GTest)

include(GoogleTest)

# ################### Library ####################
add_library(melon INTERFACE)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(melon INTERFACE range-v3::range-v3 fmt::fmt)

set_project_warnings(melon)

# ################# TEST target ##################
add_executable(
  melon_test
  main_test.cpp
  cpo_test.cpp
  static_digraph_test.cpp
  static_forward_digraph_test.cpp
  dumb_digraph_test.cpp
  mutable_digraph_test.cpp
  static_map_test.cpp
  static_map_bool_test.cpp
  static_digraph_builder_test.cpp
  breadth_first_search_test.cpp
  depth_first_search_test.cpp
  d_ary_heap_test.cpp
  dijkstra_test.cpp
  bidirectional_dijkstra_test.cpp
  concurrent_dijkstras_test.cpp
  intrusive_view_test.cpp
  edmonds_karp_test.cpp
  erdos_renyi_test.cpp
  complete_digraph_test.cpp
  reverse_test.cpp
  topological_sort_test.cpp
  subgraph_test.cpp
  dinitz_test.cpp
  strongly_connected_components_test.cpp
  graph_view_test.cpp
  undirect_test.cpp
  kruskal_test.cpp
  disjoint_sets_test.cpp
  knapsack_bnb_test.cpp
  unbounded_knapsack_bnb_test.cpp)
target_link_libraries(melon_test GTest::gtest)
target_link_libraries(melon_test melon)
gtest_discover_tests(melon_test)

target_compile_options(melon_test INTERFACE -fconcepts-diagnostics-depth=10)

# execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
#                 OUTPUT_VARIABLE LIBASAN_PATH
#                 OUTPUT_STRIP_TRAILING_WHITESPACE)

# message("cxx : ${CMAKE_CXX_COMPILER}")
# message("libasan : ${LIBASAN_PATH}")

# if(NOT "${LIBASAN_PATH}" STREQUAL "libasan.so")
#   target_compile_options(melon_test PRIVATE -fsanitize=address)
#   target_link_options(melon_test PRIVATE -fno-omit-frame-pointer -fsanitize=address -lpthread)
#   target_link_libraries(melon_test "${LIBASAN_PATH}")
# endif()


