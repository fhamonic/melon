# ################### Packages ###################
find_package(GTest)

# ################# TEST target ##################
add_executable(
    melon_test
    main_test.cpp static_digraph_test.cpp static_map_test.cpp
    static_map_bool_test.cpp static_digraph_builder_test.cpp dijkstra_test.cpp)
target_link_libraries(melon_test GTest::gtest)
target_link_libraries(melon_test melon)

include(GoogleTest)
gtest_discover_tests(melon_test)
