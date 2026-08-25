# ################### Modules ####################
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(CompilerWarnings)

# ################### Packages ###################
find_package(GTest)
if(NOT GTest_FOUND)
  if(MELON_FROM_CONAN)
    message(
      FATAL_ERROR
        "GTest must be provided by Conan when MELON_FROM_CONAN is enabled.")
  endif()
  message(STATUS "GTest not found in registry. Fetching with FetchContent...")
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)
endif()

include(GoogleTest)

# ################# TEST target ##################
add_executable(
  melon_test
  main.cpp
  all.cpp
  cpo.cpp
  static_digraph.cpp
  static_forward_digraph.cpp
  dumb_digraph.cpp
  mutable_digraph.cpp
  static_map.cpp
  static_filter_map.cpp
  static_digraph_builder.cpp
  make_static_digraph.cpp
  breadth_first_search.cpp
  depth_first_search.cpp
  d_ary_heap.cpp
  dijkstra.cpp
  a_star.cpp
  bidirectional_dijkstra.cpp
  competing_dijkstras.cpp
  edmonds_karp.cpp
  erdos_renyi.cpp
  complete_digraph.cpp
  reverse.cpp
  topological_sort.cpp
  subgraph.cpp
  pipe_syntax.cpp
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
  concat_view_fallback.cpp
  biobjective_dijkstra.cpp
  rational.cpp
  experimental.cpp
  undirected_graph.cpp
  undirected_graph_view.cpp
  mapping.cpp
  semiring.cpp
  priority_queue.cpp
  graphviz_printer.cpp
  algorithmic_generator.cpp
  geometry.cpp
  api_consistency.cpp
  api_review.cpp
  differential.cpp
  bellman_ford.cpp
  bellman_ford_moore.cpp
)
target_include_directories(melon_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(melon_test GTest::gtest)
target_link_libraries(melon_test melon::melon)

set_project_warnings(melon_test)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(melon_test PRIVATE -fconcepts-diagnostics-depth=20)
endif()

# ################# Sanitizers ###################
set(MELON_SANITIZE
    ""
    CACHE STRING
          "Sanitizers to build the tests with, e.g. \"address\" or
           \"address,undefined\" (GCC/Clang only)")
if(MELON_SANITIZE)
  target_compile_options(melon_test PRIVATE -fsanitize=${MELON_SANITIZE})
  target_link_options(melon_test PRIVATE -fsanitize=${MELON_SANITIZE})
  # UBSan instrumentation defeats GCC's initialization analysis inside
  # libstdc++'s filter_view begin-cache, flooding ranges-heavy TUs with
  # -Wmaybe-uninitialized false positives. The plain (non-sanitize) GCC job
  # compiles the same code with the warning fully enabled, so nothing is
  # lost by silencing it here.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(melon_test PRIVATE -Wno-maybe-uninitialized)
  endif()
endif()

gtest_discover_tests(melon_test)

# ########## Header self-containment check ###########
# One translation unit per public header, generated at configure time: each
# header must compile on its own, with nothing included before it. This is what
# catches a header that only ever worked because some other one happened to be
# included first.
file(
  GLOB_RECURSE MELON_PUBLIC_HEADERS
  RELATIVE ${PROJECT_SOURCE_DIR}/include
  CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/melon/*.hpp)

# The two unfinished experimental headers do not compile yet; they are held
# back from the install rules too, see their file-level comments.
list(REMOVE_ITEM MELON_PUBLIC_HEADERS melon/experimental/scapegoat_tree.hpp
     melon/experimental/doubly_connected_digraph.hpp)

set(MELON_HEADER_CHECK_SOURCES "")
foreach(header ${MELON_PUBLIC_HEADERS})
  string(REGEX REPLACE "[/.]" "_" header_slug ${header})
  set(header_check_source
      ${CMAKE_CURRENT_BINARY_DIR}/header_check/${header_slug}.cpp)
  file(GENERATE OUTPUT ${header_check_source}
       CONTENT "#include \"${header}\"\n")
  list(APPEND MELON_HEADER_CHECK_SOURCES ${header_check_source})
endforeach()

add_library(melon_header_check OBJECT ${MELON_HEADER_CHECK_SOURCES})
target_link_libraries(melon_header_check melon::melon)
set_project_warnings(melon_header_check)
add_dependencies(melon_test melon_header_check)
