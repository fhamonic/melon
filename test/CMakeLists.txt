cmake_minimum_required(VERSION 3.13)
project(MelonUnitTests CXX)

set(CMAKE_CXX_STANDARD 26)
 
# ################### Modules ####################
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(CompilerWarnings)

# ################### Packages ###################
find_package(GTest)
# find_package(mp++ REQUIRED)

include(GoogleTest)

# ################### Library ####################
add_library(melon INTERFACE)
target_include_directories(
    melon INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/../include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# target_link_libraries(melon INTERFACE mp++::mp++)

set_project_warnings(melon)

# ################# TEST target ##################
add_executable(
  melon_test
  main.cpp
  cpo.cpp
  static_digraph.cpp
  static_forward_digraph.cpp
  dumb_digraph.cpp
  mutable_digraph.cpp
  static_map.cpp
  static_filter_map.cpp
  static_digraph_builder.cpp
  breadth_first_search.cpp
  depth_first_search.cpp
  d_ary_heap.cpp
  dijkstra.cpp
  bidirectional_dijkstra.cpp
  competing_dijkstras.cpp
  intrusive_view.cpp
  edmonds_karp.cpp
  erdos_renyi.cpp
  complete_digraph.cpp
  reverse.cpp
  topological_sort.cpp
  subgraph.cpp
  dinitz.cpp
  strongly_connected_components.cpp
  graph_view.cpp
  undirect.cpp
  kruskal.cpp
  disjoint_sets.cpp
  knapsack_bnb.cpp
  unbounded_knapsack_bnb.cpp
  bentley_ottmann.cpp
  bounded_value.cpp
  consumable_view.cpp
  network_voronoi.cpp
  alias_method_sampler.cpp
  connected_components.cpp
  traversal_forest.cpp
  )
target_link_libraries(melon_test GTest::gtest)
target_link_libraries(melon_test melon)
gtest_discover_tests(melon_test)

target_compile_options(melon_test INTERFACE -fconcepts-diagnostics-depth=20)

execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
                OUTPUT_VARIABLE LIBASAN_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

message("cxx : ${CMAKE_CXX_COMPILER}")
message("libasan : ${LIBASAN_PATH}")

if(NOT "${LIBASAN_PATH}" STREQUAL "libasan.so")
  target_compile_options(melon_test PRIVATE -fsanitize=address)
  target_link_options(melon_test PRIVATE -fsanitize=address -lpthread)
  target_link_libraries(melon_test "${LIBASAN_PATH}")
endif()


