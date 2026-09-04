/**
 * @file all.hpp
 * @author François Hamonic (francois.hamonic@gmail.com)
 * @brief Includes the entire public API of the melon library.
 */
#pragma once

#include "melon/version.hpp"

#include "melon/borrowed_graph.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/undirected_graph.hpp"

#include "melon/maps/constant.hpp"
#include "melon/maps/element.hpp"
#include "melon/maps/transform.hpp"

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/disjoint_sets.hpp"
#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/container/static_filter_map.hpp"
#include "melon/container/static_forward_digraph.hpp"
#include "melon/container/static_map.hpp"

#include "melon/views/complete_digraph.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/reverse.hpp"
#include "melon/views/subgraph.hpp"
#include "melon/views/undirect.hpp"
#include "melon/views/undirected_graph_view.hpp"
#include "melon/views/with_maps.hpp"

#include "melon/algorithm/a_star.hpp"
#include "melon/algorithm/bellman_ford.hpp"
#include "melon/algorithm/bellman_ford_moore.hpp"
#include "melon/algorithm/bentley_ottmann.hpp"
#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/algorithm/biobjective_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/competing_dijkstras.hpp"
#include "melon/algorithm/connected_components.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/dinitz.hpp"
#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/algorithm/knapsack_bnb.hpp"
#include "melon/algorithm/kruskal.hpp"
#include "melon/algorithm/network_simplex.hpp"
#include "melon/algorithm/network_voronoi.hpp"
#include "melon/algorithm/strongly_connected_components.hpp"
#include "melon/algorithm/topological_sort.hpp"
#include "melon/algorithm/traversal_forest.hpp"
#include "melon/algorithm/unbounded_knapsack_bnb.hpp"

#include "melon/numeric/bounded_value.hpp"
#include "melon/numeric/geometry.hpp"
#include "melon/numeric/rational.hpp"
#include "melon/numeric/semiring.hpp"

#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/alias_method_sampler.hpp"
#include "melon/utility/erdos_renyi.hpp"
#include "melon/utility/graphviz_printer.hpp"
#include "melon/utility/make_static_digraph.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/static_digraph_builder.hpp"
