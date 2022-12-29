#ifndef MELON_ALGORITHM_DIJKSTA_HPP
#define MELON_ALGORITHM_DIJKSTA_HPP

#include <algorithm>
#include <cassert>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/constexpr_ternary.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/utility/traversal_iterator.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

// clang-format off
template <typename T>
concept edmonds_karp_trait = semiring<typename T::semiring> &&
    updatable_priority_queue<typename T::heap> && requires() {
    { T::store_distances } -> std::convertible_to<bool>;
    { T::store_paths } -> std::convertible_to<bool>;
};
// clang-format on

template <typename G, typename L>
struct edmonds_karp_default_traits {};

template <graph G, input_value_map<arc_t<G>> C,
          edmonds_karp_trait T = edmonds_karp_default_traits<G, C>>
    requires outward_incidence_graph<G> && inward_incidence_graph<G> &&
             has_arc_map<G>
class edmonds_karp {
private:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;
    using value_t = mapped_value_t<C, arc_t<G>>;

private:
    std::reference_wrapper<const G> _graph;
    std::reference_wrapper<const C> _capacity_map;

    vertex_t<G> _s;
    vertex_t<G> _t;
    arc_mapt_t<G, value_t> _carried_flow_map;
    arc_mapt_t<G, value_t> _reverse_flow_map;

public:
    [[nodiscard]] constexpr edmonds_karp(const G & g, const C & c)
        : _graph(g)
        , _capacity_map(c)
        , _carried_flow_map(create_arc_map<value_t>(g))
        , _reverse_flow_map(create_arc_map<value_t>(g)) {}

    [[nodiscard]] constexpr edmonds_karp(const G & g, const L & l,
                                         const vertex & s, const vertex & t)
        : edmonds_karp(g, l) {
        set_source(s);
        set_target(t);
    }

    [[nodiscard]] constexpr edmonds_karp(const edmonds_karp & bin) = default;
    [[nodiscard]] constexpr edmonds_karp(edmonds_karp && bin) = default;

    constexpr edmonds_karp & operator=(const edmonds_karp &) = default;
    constexpr edmonds_karp & operator=(edmonds_karp &&) = default;

    constexpr edmonds_karp & set_source(const vertex_t & s) noexcept {
        _s = s;
        return *this;
    }
    constexpr edmonds_karp & set_target(const vertex_t & t) noexcept {
        _t = t;
        return *this;
    }
    constexpr edmonds_karp & reset() noexcept {
        _carried_flow_map.fill(0);
        _reverse_flow_map.fill(0);
        return *this;
    }
    constexpr edmonds_karp & run() noexcept {
        _carried_flow_map.fill(0);
        // TODO : transform_view(reverse_view(_graph), std::identity, []())
        return *this;
    }
    constexpr value_t flow_value() noexcept {
        value_t sum{0};
        for(auto && a : out_arcs(_graph.get(), _s)) sum += _carried_flow_map[a];
        return sum;
    }
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_ALGORITHM_DIJKSTA_HPP
