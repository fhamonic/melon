# ################### Packages ###################
find_package(GTest)
include(GoogleTest)

# ################# TEST target ##################
add_executable(
  melon_test
  main_test.cpp
  cpo_test.cpp
  static_digraph_test.cpp
  static_forward_digraph_test.cpp
  static_forward_weighted_digraph_test.cpp
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
  strong_fiber_test.cpp
  intrusive_view_test.cpp
  reverse_test.cpp)
target_link_libraries(melon_test GTest::gtest)
target_link_libraries(melon_test melon)
gtest_discover_tests(melon_test)
