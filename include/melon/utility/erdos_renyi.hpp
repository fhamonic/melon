#pragma once

#include <random>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

namespace melon {

// The generator is taken by reference so the caller owns the seed: this
// overload is the only way to get a reproducible random graph, and the only
// one safe to call concurrently (each thread with its own generator).
//
// The distribution is a local, not a function-local `static`. A `static`
// std::uniform_real_distribution is shared mutable state -- a distribution
// carries its own -- so concurrent calls raced on it, and a `static` engine
// made the whole function unseedable.
template <typename G, typename Generator>
[[nodiscard]] G erdos_renyi(const std::size_t num_vertices,
                            const double expected_density, Generator & gen) {
    using vertex = vertex_t<G>;

    std::uniform_real_distribution<double> distr{0.0, 1.0};
    static_digraph_builder<G> builder(num_vertices);

    for(std::size_t i = 0; i < num_vertices; ++i) {
        for(std::size_t j = 0; j < num_vertices; ++j) {
            if(i == j) continue;
            if(distr(gen) < expected_density)
                builder.add_arc(vertex(i), vertex(j));
        }
    }

    return std::get<0>(builder.build());
}

// Convenience overload: seeds a generator of its own from std::random_device,
// so successive calls differ. Reach for the three-argument form above whenever
// the graph has to be reproducible.
template <typename G>
[[nodiscard]] G erdos_renyi(const std::size_t num_vertices,
                            const double expected_density) {
    std::mt19937 gen{std::random_device{}()};
    return erdos_renyi<G>(num_vertices, expected_density, gen);
}

}  // namespace melon
