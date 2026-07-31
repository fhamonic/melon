#pragma once

#include <cassert>
#include <concepts>
#include <utility>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/utility/semiring.hpp"

namespace melon {

// clang-format off
template <typename Traits>
concept competing_dijkstras_traits = semiring<typename Traits::semiring> &&
    updatable_priority_queue<typename Traits::heap> && requires(typename Traits::entry & e) {
    e.first;
    { e.second } -> std::convertible_to<bool>;
} && std::strict_weak_order<typename Traits::entry_cmp, typename Traits::entry, typename Traits::entry>;
// clang-format on

template <outward_incidence_graph Graph, typename ValueType>
struct competing_dijkstras_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    using entry = std::pair<ValueType, bool>;
    struct entry_cmp {
        [[nodiscard]] constexpr bool operator()(const entry & e1,
                                                const entry & e2) const {
            if(e1.first == e2.first) {
                return e1.second && !e2.second;
            }
            return semiring::less(e1.first, e2.first);
        }
    };
    using heap =
        updatable_d_ary_heap<2, std::pair<vertex_t<Graph>, entry>, entry_cmp,
                             vertex_map_t<Graph, std::size_t>,
                             maps::element_map<1>, maps::element_map<0>>;
};

// Two Dijkstras racing on the same graph with different length maps: blue
// sources spread using the blue map, red sources using the red one, and each
// vertex is claimed by whichever colour reaches it first -- blue wins ties.
// Iterating yields only the vertices blue claims, in order of increasing blue
// distance, and stops as soon as no blue candidate is left; red vertices are
// traversed, so that they can block blue, but never produced.
// Same precondition as melon::dijkstra, on both maps and uncheckable by any
// concept: an arc length must never improve a distance when combined. Each
// vertex is claimed once, so a violation silently gives it to the wrong colour.
// O((m + n) log n) with the default binary heap.
template <graph_view Graph, mapping_view<arc_t<Graph>> BlueLengthMap,
          mapping_view<arc_t<Graph>> RedLengthMap,
          competing_dijkstras_traits Traits =
              competing_dijkstras_default_traits<
                  Graph, mapped_value_t<BlueLengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph> && has_vertex_map<Graph> &&
             std::is_same_v<mapped_value_t<BlueLengthMap, arc_t<Graph>>,
                            mapped_value_t<RedLengthMap, arc_t<Graph>>>
class competing_dijkstras
    : public algorithm_view_interface<
          competing_dijkstras<Graph, BlueLengthMap, RedLengthMap, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using length_type = mapped_value_t<BlueLengthMap, arc_t<Graph>>;
    using entry_t = typename Traits::entry;
    using entry_cmp = typename Traits::entry_cmp;
    using heap = typename Traits::heap;

    static_assert(
        std::is_same_v<typename Traits::heap::value_type,
                       std::pair<vertex, std::pair<length_type, bool>>>,
        "competing_dijkstras requires matching value_type with heap.");

private:
    Graph _graph;
    BlueLengthMap _blue_length_map;
    RedLengthMap _red_length_map;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };
    vertex_map_t<Graph, vertex_status> _vertex_status_map;
    heap _heap;
    std::size_t _num_blue_candidates;
    [[no_unique_address]] entry_cmp _entry_cmp;

public:
    template <graph_for<Graph> G, mapping_for<BlueLengthMap> BLM,
              mapping_for<RedLengthMap> RLM>
    competing_dijkstras(G && g, BLM && blm, RLM && rlm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _blue_length_map(maps::mapping_all(std::forward<BLM>(blm)))
        , _red_length_map(maps::mapping_all(std::forward<RLM>(rlm)))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _heap(_entry_cmp, create_vertex_map<std::size_t>(_graph))
        , _num_blue_candidates(0) {}

    template <typename... Args>
        requires std::constructible_from<competing_dijkstras, Args...>
    constexpr competing_dijkstras(Traits, Args &&... args)
        : competing_dijkstras(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr competing_dijkstras(const competing_dijkstras &) = delete;
    constexpr competing_dijkstras(competing_dijkstras &&) = default;

    constexpr competing_dijkstras & operator=(const competing_dijkstras &) =
        delete;
    constexpr competing_dijkstras & operator=(competing_dijkstras &&) = default;

    [[nodiscard]] constexpr Graph & base() & noexcept { return _graph; }
    [[nodiscard]] constexpr const Graph & base() const & noexcept {
        return _graph;
    }
    [[nodiscard]] constexpr Graph && base() && noexcept {
        return std::move(_graph);
    }
    [[nodiscard]] constexpr const Graph && base() const && noexcept {
        return std::move(_graph);
    }

    template <mapping_for<BlueLengthMap> BLM>
        requires std::assignable_from<BlueLengthMap &, BlueLengthMap>
    competing_dijkstras & set_blue_length_map(BLM && blue_length_map) {
        _blue_length_map =
            maps::mapping_all(std::forward<BLM>(blue_length_map));
        return *this;
    }

    template <mapping_for<RedLengthMap> RLM>
        requires std::assignable_from<RedLengthMap &, RedLengthMap>
    competing_dijkstras & set_red_length_map(RLM && red_length_map) {
        _red_length_map = maps::mapping_all(std::forward<RLM>(red_length_map));
        return *this;
    }

    competing_dijkstras & reset() {
        _vertex_status_map.fill(PRE_HEAP);
        _heap.clear();
        _num_blue_candidates = 0;
        return *this;
    }

    // Strict precondition: the vertex must be untouched. Re-seeding a settled
    // one silently re-processes it and corrupts the claims already made. Both
    // overloads end by restoring the class invariant -- "the heap top, if any
    // blue candidate remains, is blue" -- so a sourced object is immediately
    // ready to iterate.
    competing_dijkstras & add_blue_source(
        const vertex & s, const length_type dist_v = Traits::semiring::zero) {
        assert(_vertex_status_map[s] == PRE_HEAP);
        _heap.push(std::make_pair(s, entry_t{dist_v, true}));
        ++_num_blue_candidates;
        _vertex_status_map[s] = IN_HEAP;
        settle_red_prefix();
        return *this;
    }
    competing_dijkstras & add_red_source(
        const vertex & s, const length_type dist_v = Traits::semiring::zero) {
        assert(_vertex_status_map[s] == PRE_HEAP);
        _heap.push(std::make_pair(s, entry_t{dist_v, false}));
        _vertex_status_map[s] = IN_HEAP;
        settle_red_prefix();
        return *this;
    }

    void relax_blue_vertex(const vertex & w, const length_type new_dist_v) {
        const entry_t new_dist = {new_dist_v, true};
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const entry_t old_dist = _heap.priority(w);
            if(_entry_cmp(new_dist, old_dist)) {
                if(!old_dist.second) {
                    ++_num_blue_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _heap.push(std::make_pair(w, new_dist));
            _vertex_status_map[w] = IN_HEAP;
            ++_num_blue_candidates;
        }
    }

    void relax_red_vertex(const vertex & w, const length_type new_dist_v) {
        const entry_t new_dist = {new_dist_v, false};
        auto && w_status = _vertex_status_map[w];
        if(w_status == IN_HEAP) {
            const entry_t old_dist = _heap.priority(w);
            if(_entry_cmp(new_dist, old_dist)) {
                if(old_dist.second) {
                    --_num_blue_candidates;
                }
                _heap.promote(w, new_dist);
            }
        } else if(w_status == PRE_HEAP) {
            _heap.push(std::make_pair(w, new_dist));
            _vertex_status_map[w] = IN_HEAP;
        }
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_num_blue_candidates == 0)) {
        return _num_blue_candidates == 0;
    }

    // The noexcept measures the copy the by-value return performs, not just the
    // top() call.
    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(typename heap::value_type(_heap.top()))) {
        assert(!finished());
        return _heap.top();
    }

private:
    // Settles every red vertex sitting on top of the heap, so that the class
    // invariant -- "the heap top, if any blue candidate remains, is blue" --
    // holds at every observable point. Guarded on the candidate count before
    // touching top(), so it is safe on an empty heap.
    constexpr void settle_red_prefix() {
        while(_num_blue_candidates > 0 && !_heap.top().second.second) {
            // A copy, not a reference binding: top() returns a reference into
            // the heap array, and t_dist is read after the pop() below
            // reorders it.
            const auto [t, t_dist] = _heap.top();
            _vertex_status_map[t] = POST_HEAP;
            auto && out_arcs_range = out_arcs(_graph, t);
            prefetch_keys_and_values(out_arcs_range, arc_targets_map(_graph),
                                     _red_length_map);
            _heap.pop();
            for(const arc a : out_arcs_range) {
                const vertex & w = arc_target(_graph, a);
                relax_red_vertex(w, Traits::semiring::plus(t_dist.first,
                                                           _red_length_map[a]));
            }
        }
    }

public:
    constexpr void advance() {
        assert(!finished());
        // The class invariant leaves a blue vertex on top here.
        const auto [t, t_dist] = _heap.top();
        assert(t_dist.second);
        _vertex_status_map[t] = POST_HEAP;
        auto && out_arcs_range = out_arcs(_graph, t);
        prefetch_keys_and_values(out_arcs_range, arc_targets_map(_graph),
                                 _blue_length_map);
        _heap.pop();
        --_num_blue_candidates;
        for(const arc & a : out_arcs_range) {
            const vertex & w = arc_target(_graph, a);
            relax_blue_vertex(
                w, Traits::semiring::plus(t_dist.first, _blue_length_map[a]));
        }
        settle_red_prefix();
    }
};

template <typename Graph, typename BlueLengthMap, typename RedLengthMap>
competing_dijkstras(Graph &&, BlueLengthMap &&, RedLengthMap &&)
    -> competing_dijkstras<views::graph_all_t<Graph>,
                           maps::mapping_all_t<BlueLengthMap>,
                           maps::mapping_all_t<RedLengthMap>>;

template <typename Graph, typename BlueLengthMap, typename RedLengthMap,
          typename Traits>
competing_dijkstras(Traits, Graph &&, BlueLengthMap &&, RedLengthMap &&)
    -> competing_dijkstras<views::graph_all_t<Graph>,
                           maps::mapping_all_t<BlueLengthMap>,
                           maps::mapping_all_t<RedLengthMap>, Traits>;

}  // namespace melon
