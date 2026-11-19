#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/container/d_ary_heap.hpp"
#include "melon/detail/fill.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/maps/element.hpp"
#include "melon/utility/priority_queue.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

struct onaga_roles {
    struct flow {};
    struct start_label {};
    struct label {};
    struct gain_from_source {};
    struct pred_arc {};
    struct status {};
    struct listed {};
    struct first_in {};
    struct first_out {};
    struct potential {};
    struct heap_index {};
};

template <typename Traits, typename ValueType>
concept onaga_traits =
    updatable_priority_queue<
        typename Traits::template heap<std::greater<ValueType>>> &&
    requires {
        { Traits::arc_full } -> std::convertible_to<double>;
        { Traits::negligible_flow } -> std::convertible_to<double>;
        { Traits::sparse_reset } -> std::convertible_to<bool>;
        { Traits::flowing_arc_lists } -> std::convertible_to<bool>;
    };

template <has_vertex_map Graph, typename ValueType>
struct onaga_default_traits {
    template <std::strict_weak_order<ValueType, ValueType> Compare>
    using heap = updatable_d_ary_heap<
        4, std::pair<vertex_t<Graph>, ValueType>, Compare,
        vertex_map_t<Graph, std::size_t, onaga_roles::heap_index>,
        maps::element<1>, maps::element<0>>;

    // Relative: an arc is full within arc_full of its capacity, and a flow
    // under negligible_flow times the supply is none. At 0, the residue of a
    // push that fills an arc leaves it open, and the searches go on
    // augmenting along it. 1e-11 and 1e-12 at double, scaled by epsilon for
    // the other types: a plain 1e-11 is below float's epsilon, and acts as 0.
    static constexpr ValueType arc_full =
        static_cast<ValueType>(1e-11) *
        (std::numeric_limits<ValueType>::epsilon() /
         static_cast<ValueType>(std::numeric_limits<double>::epsilon()));
    static constexpr ValueType negligible_flow =
        static_cast<ValueType>(1e-12) *
        (std::numeric_limits<ValueType>::epsilon() /
         static_cast<ValueType>(std::numeric_limits<double>::epsilon()));

    // reset() restores what the searches since the last reset() reached,
    // rather than every vertex and arc: 5 ms against 480 ms for 20,000
    // one-search runs on a 300x300 grid, within 1% of off elsewhere.
    static constexpr bool sparse_reset = true;
    // The searches find the reverse residual arcs in per-vertex lists of the
    // arcs that took flow, rather than among every in-arc (from s) and
    // out-arc (from t): single runs 13% faster on random and grid networks.
    // Ties between equal gains break in another order.
    static constexpr bool flowing_arc_lists = true;
};

// Generalized maximum flow by Onaga's algorithm: augment along a highest-gain
// residual path from s to t until the supply of s is sent or no path remains.
// An arc a = (u, v) carries 0 <= flow(a) <= capacity(a), measured at u, and
// delivers gain(a) * flow(a) at v.
//
// Preconditions no concept can check:
// - 0 < gain(a) <= 1 and capacity(a) >= 0. A gain above 1 can close a
//   flow-generating cycle, which augmenting paths never cancel: the flow
//   returned is then not maximum, silently.
// - The start labels, 1 by default, are feasible: label(t) = 1 and
//   label(u) >= gain(a) * label(v) on every arc of positive capacity.
//   Otherwise a vertex settles before its best path is known.
// - The gain and capacity maps are read live: change them between runs, then
//   reset(). A change during a run leaves flows over their capacities.
// An infinite capacity is unbounded, and a capacity of 0 removes its arc.
// The number of augmentations can be exponential: advance() makes one, so
// that a caller can bound them. Each is a Dijkstra search stopped once t
// settles, O((n + m) log n) at most.
template <
    graph_view Graph, mapping_view<arc_t<Graph>> GainMap,
    mapping_view<arc_t<Graph>> CapacityMap,
    typename Traits = onaga_default_traits<
        Graph, std::common_type_t<mapped_value_t<GainMap, arc_t<Graph>>,
                                  mapped_value_t<CapacityMap, arc_t<Graph>>>>>
    requires outward_incidence_graph<Graph> && inward_incidence_graph<Graph> &&
             has_vertex_map<Graph> && has_arc_map<Graph> &&
             std::floating_point<std::common_type_t<
                 mapped_value_t<GainMap, arc_t<Graph>>,
                 mapped_value_t<CapacityMap, arc_t<Graph>>>> &&
             onaga_traits<
                 Traits,
                 std::common_type_t<mapped_value_t<GainMap, arc_t<Graph>>,
                                    mapped_value_t<CapacityMap, arc_t<Graph>>>>
class onaga {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using value_t =
        std::common_type_t<mapped_value_t<GainMap, arc_t<Graph>>,
                           mapped_value_t<CapacityMap, arc_t<Graph>>>;
    // A max-heap on gains: a min-heap settles the worst paths first, and the
    // path found is not a highest-gain one.
    using heap = typename Traits::template heap<std::greater<value_t>>;

    static_assert(
        std::is_same_v<typename heap::value_type, std::pair<vertex, value_t>>,
        "onaga requires heap entries of type std::pair<vertex, value>.");
    static_assert(
        heap_index_map_agrees<
            heap, vertex_map_t<Graph, std::size_t, onaga_roles::heap_index>>,
        "onaga requires the heap index map to be the graph's answer for "
        "onaga_roles::heap_index.");

    static constexpr bool tracks_flowing_arcs =
        Traits::sparse_reset || Traits::flowing_arc_lists;
    static constexpr std::size_t no_link =
        std::numeric_limits<std::size_t>::max();

    // An arc that took flow since reset(), in the list of the arcs that took
    // flow into its target and in the list of those out of its source. An arc
    // whose flow came back to exactly 0 and takes flow again is linked twice:
    // its second relaxation is never better than the first.
    struct flow_link {
        arc a;
        std::size_t next_in;
        std::size_t next_out;
    };
    using flowing_entry =
        std::conditional_t<Traits::flowing_arc_lists, flow_link, arc>;
    struct no_flowing_arcs {};
    struct no_relabeled_vertices {};

    Graph _graph;
    GainMap _gain_map;
    CapacityMap _capacity_map;
    vertex _s;
    vertex _t;
    value_t _supply;
    // Unconditional, not `#ifndef NDEBUG`: melon is header-only, so a layout
    // that depends on NDEBUG differs between translation units of one program.
    bool _source_set;
    bool _target_set;
    bool _labels_need_reset;
    bool _exhausted;
    bool _potentials_ready;
    // _settled and the predecessors hold the tree of the last search from s,
    // which found a path, and the next search starts from what of it the
    // augmentation left residual. Anything else that changes the flow, the
    // labels or the terminals must clear it, or that search settles the tree
    // at a gain that is not its own.
    bool _tree_kept;
    // With tracks_flowing_arcs, 0 outside _flowing, which reset() walks
    // instead of the arcs: an arc that takes flow unrecorded keeps it across
    // reset().
    arc_map_t<Graph, value_t, onaga_roles::flow> _flow_map;
    [[no_unique_address]] std::conditional_t<tracks_flowing_arcs,
                                             std::vector<flowing_entry>,
                                             no_flowing_arcs> _flowing;
    // Indices in _flowing, no_link at the end of a list. Only the ends of the
    // arcs of an augmenting path get a list, and the search that found it
    // settled them: with sparse_reset, reset() clears the heads over
    // _relabeled alone.
    [[no_unique_address]] detail::vertex_map_if<
        Traits::flowing_arc_lists, Graph, std::size_t, onaga_roles::first_in>
        _first_in_map;
    [[no_unique_address]] detail::vertex_map_if<
        Traits::flowing_arc_lists, Graph, std::size_t, onaga_roles::first_out>
        _first_out_map;
    // Feasible labels of the residual graph: label(t) = 1 and
    // label(u) >= g * label(v) on each residual arc of gain g. With
    // sparse_reset, they are the start labels outside _relabeled, the
    // vertices settled since reset().
    vertex_map_t<Graph, value_t, onaga_roles::start_label> _start_label_map;
    vertex_map_t<Graph, value_t, onaga_roles::label> _label_map;
    [[no_unique_address]] detail::vertex_map_if<
        Traits::sparse_reset, Graph, bool, onaga_roles::listed> _listed_map;
    [[no_unique_address]] std::conditional_t<Traits::sparse_reset,
                                             std::vector<vertex>,
                                             no_relabeled_vertices> _relabeled;
    // In a search, a vertex is in the heap when its status is the epoch of
    // the search, settled at the epoch + 1, and unreached below. Each search
    // takes the epoch 2 above the last instead of clearing the statuses, which
    // happens once in 127 searches. The gains from s and the potentials are
    // never cleared: a read that skips the status test gets the value of an
    // earlier search.
    vertex_map_t<Graph, unsigned char, onaga_roles::status> _status_map;
    unsigned char _epoch;
    unsigned char _potentials_epoch;
    vertex_map_t<Graph, value_t, onaga_roles::gain_from_source>
        _gain_from_source_map;
    vertex_map_t<Graph, std::pair<arc, bool>, onaga_roles::pred_arc>
        _pred_arc_map;
    vertex_map_t<Graph, value_t, onaga_roles::potential> _potential_map;
    heap _heap;
    std::vector<vertex> _settled;
    // From t to s, each arc with whether it is taken forward: the augmentation
    // walks it reversed, as the amount it carries is measured from s.
    std::vector<std::pair<arc, bool>> _path;
    value_t _remaining;
    value_t _arrived;

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<GainMap> GM,
              mapping_for<CapacityMap> CM>
    constexpr onaga(G && g, GM && gm, CM && cm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _gain_map(maps::mapping_all(std::forward<GM>(gm)))
        , _capacity_map(maps::mapping_all(std::forward<CM>(cm)))
        , _supply(0)
        , _source_set(false)
        , _target_set(false)
        , _labels_need_reset(false)
        , _exhausted(false)
        , _potentials_ready(false)
        , _tree_kept(false)
        , _flow_map(
              create_arc_map<value_t, onaga_roles::flow>(_graph, value_t{0}))
        , _flowing()
        , _first_in_map(_graph, no_link)
        , _first_out_map(_graph, no_link)
        , _start_label_map(create_vertex_map<value_t, onaga_roles::start_label>(
              _graph, value_t{1}))
        , _label_map(create_vertex_map<value_t, onaga_roles::label>(_graph,
                                                                    value_t{1}))
        , _listed_map(_graph, false)
        , _relabeled()
        , _status_map(
              create_vertex_map<unsigned char, onaga_roles::status>(_graph, 0))
        , _epoch(0)
        , _potentials_epoch(0)
        , _gain_from_source_map(
              create_vertex_map<value_t, onaga_roles::gain_from_source>(_graph))
        , _pred_arc_map(
              create_vertex_map<std::pair<arc, bool>, onaga_roles::pred_arc>(
                  _graph))
        , _potential_map(
              create_vertex_map<value_t, onaga_roles::potential>(_graph))
        , _heap(std::greater<value_t>{},
                create_vertex_map<std::size_t, onaga_roles::heap_index>(_graph))
        , _remaining(0)
        , _arrived(0) {}

    template <graph_for<Graph> G, mapping_for<GainMap> GM,
              mapping_for<CapacityMap> CM>
    constexpr onaga(G && g, GM && gm, CM && cm, const vertex & s,
                    const vertex & t, const value_t & supply)
        : onaga(std::forward<G>(g), std::forward<GM>(gm),
                std::forward<CM>(cm)) {
        set_source(s, supply);
        set_target(t);
    }

    template <typename... Args>
        requires std::constructible_from<onaga, Args...>
    constexpr onaga(Traits, Args &&... args)
        : onaga(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept.
    constexpr onaga(const onaga &) = delete;
    constexpr onaga(onaga &&) = default;

    constexpr onaga & operator=(const onaga &) = delete;
    constexpr onaga & operator=(onaga &&) = default;

    // ---- Base access --------------------------------------------------------

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

    // ---- Setup --------------------------------------------------------------

    // The setters keep the flow: a new computation is reset() and the
    // setters, in either order. The supply is finite: the tolerances are
    // relative to it, and an infinite one sends nothing.
    constexpr onaga & set_source(const vertex & s, const value_t & supply) {
        assert(supply >= value_t{0} &&
               supply < std::numeric_limits<value_t>::infinity());
        _s = s;
        _supply = supply;
        _remaining = supply;
        _source_set = true;
        _exhausted = false;
        _potentials_ready = false;
        _tree_kept = false;
        return *this;
    }

    constexpr onaga & set_target(const vertex & t) {
        _t = t;
        _target_set = true;
        _exhausted = false;
        _potentials_ready = false;
        _tree_kept = false;
        return *this;
    }

    // O(n), and kept by reset(): labels shared by many runs, such as the
    // best gains to t over the arcs of positive capacity, are set once. On an
    // object that carries flow, reset() must come before the next search, as
    // asserted: the arcs that carry flow backward need labels that the start
    // labels need not be feasible for.
    template <typename LabelMap>
        requires mapping<maps::mapping_all_t<LabelMap>, vertex> &&
                 std::convertible_to<
                     mapped_value_t<maps::mapping_all_t<LabelMap>, vertex>,
                     value_t>
    constexpr onaga & set_start_labels(LabelMap && labels) {
        auto && label_map = maps::mapping_all(std::forward<LabelMap>(labels));
        for(auto && v : vertices(_graph))
            _start_label_map[v] = _label_map[v] =
                static_cast<value_t>(label_map[v]);
        _labels_need_reset = _arrived != value_t{0} || _remaining != _supply;
        _tree_kept = false;
        return *this;
    }

    constexpr onaga & reset() {
        if constexpr(Traits::sparse_reset) {
            for(const vertex & v : _relabeled) {
                _label_map[v] = _start_label_map[v];
                _listed_map[v] = false;
                if constexpr(Traits::flowing_arc_lists)
                    _first_in_map[v] = _first_out_map[v] = no_link;
            }
            _relabeled.clear();
        } else {
            for(auto && v : vertices(_graph))
                _label_map[v] = _start_label_map[v];
            if constexpr(Traits::flowing_arc_lists) {
                detail::fill(_first_in_map, vertices(_graph), no_link);
                detail::fill(_first_out_map, vertices(_graph), no_link);
            }
        }
        if constexpr(tracks_flowing_arcs) {
            for(const flowing_entry & entry : _flowing)
                _flow_map[_flowing_arc(entry)] = value_t{0};
            _flowing.clear();
        } else {
            detail::fill(_flow_map, arcs(_graph), value_t{0});
        }
        _remaining = _supply;
        _arrived = 0;
        _labels_need_reset = false;
        _exhausted = false;
        _potentials_ready = false;
        _tree_kept = false;
        return *this;
    }

private:
    [[nodiscard]] static constexpr const arc & _flowing_arc(
        const flowing_entry & entry) noexcept {
        if constexpr(Traits::flowing_arc_lists)
            return entry.a;
        else
            return entry;
    }
    [[nodiscard]] constexpr value_t _gain(const arc & a) const {
        return static_cast<value_t>(_gain_map[a]);
    }
    [[nodiscard]] constexpr value_t _capacity(const arc & a) const {
        return static_cast<value_t>(_capacity_map[a]);
    }
    [[nodiscard]] constexpr value_t _negligible() const noexcept {
        return static_cast<value_t>(Traits::negligible_flow) * _supply;
    }
    // One comparison, so an infinite capacity is unbounded: the form
    // capacity - flow > arc_full * capacity is false at infinity.
    [[nodiscard]] constexpr bool _open(const arc & a) const {
        return _flow_map[a] <
               (value_t{1} - static_cast<value_t>(Traits::arc_full)) *
                   _capacity(a);
    }
    constexpr unsigned char _next_epoch() {
        if(_epoch == 254) {
            detail::fill(_status_map, vertices(_graph),
                         static_cast<unsigned char>(0));
            _epoch = 0;
        }
        _epoch = static_cast<unsigned char>(_epoch + 2);
        return _epoch;
    }
    constexpr void _start_flowing(const arc & a) {
        if constexpr(Traits::flowing_arc_lists) {
            const vertex u = arc_source(_graph, a), v = arc_target(_graph, a);
            _flowing.push_back({a, _first_in_map[v], _first_out_map[u]});
            _first_in_map[v] = _first_out_map[u] = _flowing.size() - 1;
        } else {
            _flowing.push_back(a);
        }
    }

    // The best residual path from s to t into _path, by Dijkstra's algorithm
    // from s on the relabeled gains g * label(v) / label(u) <= 1, stopped once
    // t settles. The settled vertices, of relabeled gain D_u >= D_t, take the
    // label label(u) * D_t / D_u: feasible again, and tight on the path, so
    // that the arcs the augmentation opens backward have a relabeled gain of 1.
    constexpr bool _search_from_source() {
        // A start label of 0 says that s does not reach t; scanning s would
        // divide by it.
        if(!(_label_map[_s] > 0)) {
            _tree_kept = false;
            return false;
        }
        const value_t negligible = _negligible();
        const unsigned char in_heap = _next_epoch(),
                            done = static_cast<unsigned char>(in_heap + 1);
        const auto improve = [&](const vertex & v, const value_t gain,
                                 const arc & a, const bool forward) {
            if(_status_map[v] == in_heap) {
                if(gain <= _gain_from_source_map[v]) return;
                _gain_from_source_map[v] = gain;
                _pred_arc_map[v] = {a, forward};
                _heap.promote(v, gain);
                return;
            }
            if(_status_map[v] == done || gain <= 0) return;
            _gain_from_source_map[v] = gain;
            _pred_arc_map[v] = {a, forward};
            _status_map[v] = in_heap;
            _heap.push({v, gain});
        };
        const auto scan = [&](const vertex & u) {
            const value_t scale = _gain_from_source_map[u] / _label_map[u];
            for(auto && a : out_arcs(_graph, u))
                if(_open(a)) {
                    const vertex v = arc_target(_graph, a);
                    improve(v, scale * _gain(a) * _label_map[v], a, true);
                }
            const auto improve_backward = [&](const arc & a) {
                if(_flow_map[a] > negligible) {
                    const vertex w = arc_source(_graph, a);
                    improve(w, scale * _label_map[w] / _gain(a), a, false);
                }
            };
            if constexpr(Traits::flowing_arc_lists) {
                for(std::size_t i = _first_in_map[u]; i != no_link;
                    i = _flowing[i].next_in)
                    improve_backward(_flowing[i].a);
            } else {
                for(auto && a : in_arcs(_graph, u)) improve_backward(a);
            }
        };
        // After the rescale, each arc of the last tree has a relabeled gain of
        // 1, the largest there is: the vertices whose tree path is still
        // residual would be popped first, and settle without the heap. They
        // are in settle order, a parent before its children.
        std::size_t kept = 0;
        if(_tree_kept) {
            for(const vertex & u : _settled) {
                if(u != _s) {
                    const auto [a, forward] = _pred_arc_map[u];
                    const vertex parent =
                        forward ? arc_source(_graph, a) : arc_target(_graph, a);
                    if(_status_map[parent] != done ||
                       !(forward ? _open(a) : _flow_map[a] > negligible))
                        continue;
                }
                _status_map[u] = done;
                _gain_from_source_map[u] = 1;
                _settled[kept++] = u;
            }
        }
        _settled.resize(kept);
        if(kept == 0) {
            _gain_from_source_map[_s] = 1;
            _status_map[_s] = in_heap;
            _heap.push({_s, value_t{1}});
        } else if(_status_map[_t] != done) {
            for(std::size_t i = 0; i < kept; ++i) scan(_settled[i]);
        }
        while(_status_map[_t] != done && !_heap.empty()) {
            const vertex u = _heap.top().first;
            _heap.pop();
            _status_map[u] = done;
            _settled.push_back(u);
            if constexpr(Traits::sparse_reset) {
                if(!_listed_map[u]) {
                    _listed_map[u] = true;
                    _relabeled.push_back(u);
                }
            }
            if(u != _t) scan(u);
        }
        _heap.clear();
        _tree_kept = _status_map[_t] == done;
        if(!_tree_kept) return false;
        for(const vertex & u : _settled)
            _label_map[u] *=
                _gain_from_source_map[_t] / _gain_from_source_map[u];
        _path.clear();
        for(vertex v = _t; v != _s;) {
            const auto [a, forward] = _pred_arc_map[v];
            _path.emplace_back(a, forward);
            v = forward ? arc_source(_graph, a) : arc_target(_graph, a);
        }
        return true;
    }

    constexpr void _augment() {
        // the most the path takes, in units leaving s
        value_t amount = _remaining, gain = 1;
        for(const auto & [a, forward] : _path | std::views::reverse) {
            if(forward) {
                amount = std::min(amount, (_capacity(a) - _flow_map[a]) / gain);
                gain *= _gain(a);
            } else {
                amount = std::min(amount, _flow_map[a] * _gain(a) / gain);
                gain /= _gain(a);
            }
        }
        _remaining -= amount;
        for(const auto & [a, forward] : _path | std::views::reverse) {
            if(forward) {
                if constexpr(tracks_flowing_arcs) {
                    if(_flow_map[a] == 0) _start_flowing(a);
                }
                _flow_map[a] += amount;
                amount *= _gain(a);
            } else {
                amount /= _gain(a);
                // An arc taken backward as the bottleneck gives up its whole
                // flow, less the rounding of the gains along the path, which
                // can leave it an ulp below 0.
                _flow_map[a] = std::max(_flow_map[a] - amount, value_t{0});
            }
        }
        _arrived += amount;
    }

public:
    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const noexcept {
        return _exhausted || !(_remaining > _negligible());
    }

    // One augmentation, or none and finished() becomes true.
    constexpr void advance() {
        assert(_source_set && _target_set && _s != _t);
        assert(!_labels_need_reset);
        assert(!finished());
        _potentials_ready = false;
        if(!_search_from_source()) {
            _exhausted = true;
            return;
        }
        _augment();
    }

    // Also computes the potentials, one search more: a caller that needs only
    // the flow drives advance() until finished().
    constexpr onaga & run() {
        while(!finished()) advance();
        if(!_potentials_ready) compute_potentials();
        return *this;
    }

    // Dijkstra's algorithm from t on the residual graph. The heap orders
    // potential(u) / label(u), not potential(u): an arc taken backward has a
    // gain above 1, and a vertex would settle before its best path. After
    // advance() calls that stop short of finished(), it gives the potentials
    // of the residual graph reached.
    constexpr onaga & compute_potentials() {
        assert(_source_set && _target_set && _s != _t);
        assert(!_labels_need_reset);
        const value_t negligible = _negligible();
        const unsigned char in_heap = _next_epoch(),
                            done = static_cast<unsigned char>(in_heap + 1);
        _potentials_epoch = in_heap;
        _potential_map[_t] = 1;
        _status_map[_t] = in_heap;
        _heap.push({_t, value_t{1}});
        while(!_heap.empty()) {
            const vertex v = _heap.top().first;
            _heap.pop();
            _status_map[v] = done;
            const auto improve = [&](const vertex & u, const value_t gain) {
                if(_status_map[u] == in_heap) {
                    if(gain <= _potential_map[u]) return;
                    _potential_map[u] = gain;
                    _heap.promote(u, gain / _label_map[u]);
                    return;
                }
                if(_status_map[u] == done || gain <= 0) return;
                _potential_map[u] = gain;
                _status_map[u] = in_heap;
                _heap.push({u, gain / _label_map[u]});
            };
            for(auto && a : in_arcs(_graph, v))
                if(_open(a))
                    improve(arc_source(_graph, a),
                            _gain(a) * _potential_map[v]);
            const auto improve_backward = [&](const arc & a) {
                if(_flow_map[a] > negligible)
                    improve(arc_target(_graph, a),
                            _potential_map[v] / _gain(a));
            };
            if constexpr(Traits::flowing_arc_lists) {
                for(std::size_t i = _first_out_map[v]; i != no_link;
                    i = _flowing[i].next_out)
                    improve_backward(_flowing[i].a);
            } else {
                for(auto && a : out_arcs(_graph, v)) improve_backward(a);
            }
        }
        _potentials_ready = true;
        return *this;
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr value_t flow_value() const noexcept {
        return _arrived;
    }
    [[nodiscard]] constexpr value_t remaining_supply() const noexcept {
        return _remaining;
    }
    // Measured at the source of a: its target receives gain(a) * flow(a).
    [[nodiscard]] constexpr value_t flow(const arc & a) const
        noexcept(noexcept(_flow_map[a])) {
        return _flow_map[a];
    }
    // Refers into the algorithm: valid while this object lives and stays put.
    [[nodiscard]] constexpr auto flows_map() const & noexcept(
        noexcept(maps::mapping_all(_flow_map))) {
        return maps::mapping_all(_flow_map);
    }
    // Terminal, like std::move(alg).base(): the member left behind is valid but
    // empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto flows_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_flow_map)))) {
        return maps::mapping_all(std::move(_flow_map));
    }

    // Valid from compute_potentials(), which run() calls, to the next
    // advance(), reset() or setter, as asserted: the next search moves the
    // epoch it reads. 0 for a vertex that does not reach t.
    [[nodiscard]] constexpr value_t potential(const vertex & v) const {
        assert(_potentials_ready);
        return _status_map[v] >= _potentials_epoch ? _potential_map[v]
                                                   : value_t{0};
    }
    // See flows_map().
    [[nodiscard]] constexpr auto potentials_map() const & noexcept {
        return maps::function(
            [this](const vertex & v) { return potential(v); });
    }
    [[nodiscard]] constexpr auto potentials_map() && noexcept(
        std::is_nothrow_move_constructible_v<decltype(_status_map)> &&
        std::is_nothrow_move_constructible_v<decltype(_potential_map)>) {
        assert(_potentials_ready);
        return maps::function([status_map = std::move(_status_map),
                               potential_map = std::move(_potential_map),
                               epoch = _potentials_epoch](const vertex & v) {
            return status_map[v] >= epoch ? potential_map[v] : value_t{0};
        });
    }
};

template <typename Graph, typename GainMap, typename CapacityMap>
onaga(Graph &&, GainMap &&, CapacityMap &&)
    -> onaga<views::graph_all_t<Graph>, maps::mapping_all_t<GainMap>,
             maps::mapping_all_t<CapacityMap>>;

template <typename Graph, typename GainMap, typename CapacityMap,
          typename Value>
onaga(Graph &&, GainMap &&, CapacityMap &&, const vertex_t<Graph> &,
      const vertex_t<Graph> &, const Value &)
    -> onaga<views::graph_all_t<Graph>, maps::mapping_all_t<GainMap>,
             maps::mapping_all_t<CapacityMap>>;

template <typename Graph, typename GainMap, typename CapacityMap,
          typename Traits>
onaga(Traits, Graph &&, GainMap &&, CapacityMap &&)
    -> onaga<views::graph_all_t<Graph>, maps::mapping_all_t<GainMap>,
             maps::mapping_all_t<CapacityMap>, Traits>;

template <typename Graph, typename GainMap, typename CapacityMap,
          typename Traits, typename Value>
onaga(Traits, Graph &&, GainMap &&, CapacityMap &&, const vertex_t<Graph> &,
      const vertex_t<Graph> &, const Value &)
    -> onaga<views::graph_all_t<Graph>, maps::mapping_all_t<GainMap>,
             maps::mapping_all_t<CapacityMap>, Traits>;

}  // namespace melon
