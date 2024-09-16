/**
 * @file all.hpp
 * @author François Hamonic (francois.hamonic@gmail.com)
 * @brief
 * @version 0.1
 * @date 2022-01-02
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef FHAMONIC_MELON_HPP
#define FHAMONIC_MELON_HPP

#include "melon/container/mutable_digraph.hpp"
#include "melon/container/static_digraph.hpp"
#include "melon/utility/static_digraph_builder.hpp"
#include "melon/container/static_forward_digraph.hpp"
#include "melon/container/static_forward_weighted_digraph.hpp"

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/dinitz.hpp"
#include "melon/algorithm/edmonds_karp.hpp"
#include "melon/algorithm/competing_dijkstras.hpp"

#include "melon/container/d_ary_heap.hpp"
#include "melon/container/static_map.hpp"

#include "melon/mapping.hpp"
#include "melon/utility/semirings.hpp"

#endif  // FHAMONIC_MELON_HPP