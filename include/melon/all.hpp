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

#include "melon/mutable_weighted_digraph.hpp"
#include "melon/static_digraph.hpp"
#include "melon/static_digraph_builder.hpp"
#include "melon/static_forward_digraph.hpp"
#include "melon/static_forward_weighted_digraph.hpp"

#include "melon/algorithm/bidirectional_dijkstra.hpp"
#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/algorithm/depth_first_search.hpp"
#include "melon/algorithm/dijkstra.hpp"
#include "melon/algorithm/strong_fiber.hpp"

#include "melon/data_structures/d_ary_heap.hpp"
#include "melon/data_structures/static_map.hpp"

#include "melon/utils/map_view.hpp"
#include "melon/utils/semirings.hpp"

#endif  // FHAMONIC_MELON_HPP