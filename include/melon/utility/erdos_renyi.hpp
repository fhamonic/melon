#pragma once

#include <cstddef>
#include <random>

#include "melon/container/static_digraph.hpp"
#include "melon/graph.hpp"
#include "melon/utility/static_digraph_builder.hpp"

namespace melon {

// The generator is taken by reference so the caller owns the seed: this
// overload is the only way to get a reproducible random graph, and the only
// one safe to call concurrently (each thread with its own generator).
// The distribution stays a local for the same reason: it carries state of its
// own, so a function-local `static` would race between concurrent calls.
template <typename G, std::uniform_random_bit_generator Generator>
[[nodiscard]] G erdos_renyi(const std::size_t num_vertices_,
                            const double expected_density, Generator & gen) {
    using vertex = vertex_t<G>;

    std::uniform_real_distribution<double> distr{0.0, 1.0};
    static_digraph_builder<G> builder(num_vertices_);

    for(std::size_t i = 0; i < num_vertices_; ++i) {
        for(std::size_t j = 0; j < num_vertices_; ++j) {
            if(i == j) continue;
            if(distr(gen) < expected_density)
                builder.add_arc(vertex(i), vertex(j));
        }
    }

    return std::get<0>(builder.build());
}

// Seeds a generator of its own from std::random_device, so successive calls
// differ and neither the sequence nor concurrent use is controllable. Use the
// three-argument form above whenever the graph has to be reproducible.
template <typename G>
[[nodiscard]] G erdos_renyi(const std::size_t num_vertices_,
                            const double expected_density) {
    std::mt19937 gen{std::random_device{}()};
    return erdos_renyi<G>(num_vertices_, expected_density, gen);
}

}  // namespace melon
