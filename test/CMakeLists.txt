# ################### Packages ###################
find_package(GTest)
include(GoogleTest)

# ################# TEST target ##################
add_executable(
    melon_test
    main_test.cpp
    static_digraph_test.cpp
    static_forward_digraph_test.cpp
    static_map_test.cpp
    static_map_bool_test.cpp
    arc_list_builder_test.cpp
    dijkstra_test.cpp
    robust_fiber_test.cpp
    reverse_test.cpp)
target_link_libraries(melon_test GTest::gtest)
target_link_libraries(melon_test melon)
gtest_discover_tests(melon_test)
