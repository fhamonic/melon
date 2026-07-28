#pragma once

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
concept competing_dijkstras_trait = semiring<typename Traits::semiring> &&
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
                             views::element_map<1>, views::element_map<0>>;
};

// Two Dijkstras racing on the same graph with different length maps: blue
// sources spread using the blue map, red sources using the red one, and each
// vertex is claimed by whichever colour reaches it first -- blue wins ties.
// Iterating yields only the vertices blue claims, in order of increasing blue
// distance, and stops as soon as no blue candidate is left; red vertices are
// traversed, so that they can block blue, but never produced. Same
// non-negativity requirement as melon::dijkstra, on both maps.
// O((m + n) log n) with the default binary heap.
template <outward_incidence_graph Graph, input_mapping<arc_t<Graph>> BLM,
          input_mapping<arc_t<Graph>> RLM,
          competing_dijkstras_trait Traits = competing_dijkstras_default_traits<
              Graph, mapped_value_t<BLM, arc_t<Graph>>>>
    requires std::is_same_v<mapped_value_t<BLM, arc_t<Graph>>,
                            mapped_value_t<RLM, arc_t<Graph>>>
class competing_dijkstras : public algorithm_view_interface<
                                competing_dijkstras<Graph, BLM, RLM, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using length_type = mapped_value_t<BLM, arc_t<Graph>>;
    using entry_t = typename Traits::entry;
    using entry_cmp = typename Traits::entry_cmp;
    using heap = typename Traits::heap;

    static_assert(
        std::is_same_v<typename Traits::heap::value_type,
                       std::pair<vertex, std::pair<length_type, bool>>>,
        "competing_dijkstras requires matching value_type with heap.");

private:
    Graph _graph;
    BLM _blue_length_map;
    RLM _red_length_map;
    enum vertex_status : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };
    vertex_map_t<Graph, vertex_status> _vertex_status_map;
    heap _heap;
    std::size_t _num_blue_candidates;
    [[no_unique_address]] entry_cmp _entry_cmp;

public:
    template <typename G, typename BlueMap, typename RedMap>
    competing_dijkstras(G && g, BlueMap && l1, RedMap && l2)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _blue_length_map(views::mapping_all(std::forward<BlueMap>(l1)))
        , _red_length_map(views::mapping_all(std::forward<RedMap>(l2)))
        , _vertex_status_map(create_vertex_map<vertex_status>(_graph, PRE_HEAP))
        , _heap(_entry_cmp, create_vertex_map<std::size_t>(_graph))
        , _num_blue_candidates(0) {}

    template <typename... Args>
    [[nodiscard]] constexpr competing_dijkstras(Traits, Args &&... args)
        : competing_dijkstras(std::forward<Args>(args)...) {}

    template <typename BlueMap>
    competing_dijkstras & set_blue_length_map(
        BlueMap && blue_length_map) noexcept {
        _blue_length_map =
            views::mapping_all(std::forward<BlueMap>(blue_length_map));
        return *this;
    }

    template <typename RedMap>
    competing_dijkstras & set_red_length_map(
        RedMap && red_length_map) noexcept {
        _red_length_map =
            views::mapping_all(std::forward<RedMap>(red_length_map));
        return *this;
    }

    competing_dijkstras & reset() noexcept {
        _vertex_status_map.fill(PRE_HEAP);
        _heap.clear();
        _num_blue_candidates = 0;
        return *this;
    }

    competing_dijkstras & add_blue_source(
        const vertex & s,
        const length_type dist_v = Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(std::make_pair(s, entry_t{dist_v, true}));
        ++_num_blue_candidates;
        _vertex_status_map[s] = IN_HEAP;
        return *this;
    }
    competing_dijkstras & add_red_source(
        const vertex & s,
        const length_type dist_v = Traits::semiring::zero) noexcept {
        assert(_vertex_status_map[s] != IN_HEAP);
        _heap.push(std::make_pair(s, entry_t{dist_v, false}));
        _vertex_status_map[s] = IN_HEAP;
        return *this;
    }

    void relax_blue_vertex(const vertex & w,
                           const length_type new_dist_v) noexcept {
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

    void relax_red_vertex(const vertex & w,
                          const length_type new_dist_v) noexcept {
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

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _num_blue_candidates == 0;
    }

    [[nodiscard]] constexpr auto current() const noexcept {
        assert(!finished());
        return _heap.top();
    }

    constexpr void advance() noexcept {
        do {
            const auto && [t, t_dist] = _heap.top();
            _vertex_status_map[t] = POST_HEAP;
            auto && out_arcs_range = out_arcs(_graph, t);
            prefetch_range(out_arcs_range);
            prefetch_mapped_values(out_arcs_range, arc_targets_map(_graph));
            if(t_dist.second) {
                prefetch_mapped_values(out_arcs_range, _blue_length_map);
                _heap.pop();
                --_num_blue_candidates;
                for(const arc & a : out_arcs_range) {
                    const vertex & w = arc_target(_graph, a);
                    relax_blue_vertex(
                        w, Traits::semiring::plus(t_dist.first,
                                                  _blue_length_map[a]));
                }
            } else {
                prefetch_mapped_values(out_arcs_range, _red_length_map);
                _heap.pop();
                for(const arc a : out_arcs_range) {
                    const vertex & w = arc_target(_graph, a);
                    relax_red_vertex(w, Traits::semiring::plus(
                                            t_dist.first, _red_length_map[a]));
                }
            }
        } while(_num_blue_candidates > 0 && !_heap.top().second.second);
    }

    constexpr void init() noexcept {
        if(!_heap.top().second.second) advance();
    }

    constexpr void run() noexcept {
        while(!finished()) advance();
    }
};

template <typename Graph, typename BLM, typename RLM,
          typename Traits = competing_dijkstras_default_traits<
              Graph, mapped_value_t<views::mapping_all_t<BLM>, arc_t<Graph>>>>
competing_dijkstras(Graph &&, BLM &&, RLM &&)
    -> competing_dijkstras<views::graph_all_t<Graph>, views::mapping_all_t<BLM>,
                           views::mapping_all_t<RLM>, Traits>;

template <typename Graph, typename BLM, typename RLM, typename Traits>
competing_dijkstras(Traits, Graph &&, BLM &&, RLM &&)
    -> competing_dijkstras<views::graph_all_t<Graph>, views::mapping_all_t<BLM>,
                           views::mapping_all_t<RLM>, Traits>;

}  // namespace melon
