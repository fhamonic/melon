#ifndef MELON_UTILITY_ERDOS_RENYI_HPP
#define MELON_UTILITY_ERDOS_RENYI_HPP

#include <random>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

namespace fhamonic {
namespace melon {

static_digraph erdos_renyi(const std::size_t nb_vertices,
                           const double expected_density) {
    using vertex = vertex_t<static_digraph>;

    static std::uniform_real_distribution<double> distr{0.0, 1.0};
    static std::mt19937 engine{std::random_device{}()};

    static_digraph_builder<static_digraph> builder(nb_vertices);

    for(std::size_t i = 0; i < nb_vertices; ++i) {
        for(std::size_t j = 0; j < nb_vertices; ++j) {
            if(i == j) continue;
            if(distr(engine) < expected_density)
                builder.add_arc(vertex(i), vertex(j));
        }
    }

    return std::get<0>(builder.build());
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILITY_ERDOS_RENYI_HPP
