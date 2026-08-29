#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/no_unique_address.hpp"
#include "melon/detail/prefetch.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <typename Traits>
concept bellman_ford_moore_traits =
    semiring<typename Traits::semiring> && requires {
        { Traits::store_paths } -> std::convertible_to<bool>;
        { Traits::detect_negative_cycles } -> std::convertible_to<bool>;
    };

template <typename Graph, typename ValueType>
struct bellman_ford_moore_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    static constexpr bool store_paths = false;
    static constexpr bool detect_negative_cycles = false;
};

// The queue variant of bellman_ford: each round scans the out-arcs of only the
// vertices the previous round improved, instead of every arc. On sparse graphs
// -- road networks -- most arcs are quiet in most rounds and this wins by a
// wide margin; on dense graphs the queue bookkeeping loses to bellman_ford's
// straight arc sweep, and the out_arcs requirement shuts out the arc-list-only
// structures bellman_ford accepts. Same lifecycle and same contract otherwise:
// without detect_negative_cycles, "no negative cycle reachable from the
// sources" is an uncheckable precondition whose violation silently yields
// meaningless distances -- with store_paths, even a path_to() that never
// terminates.
//
// No unreached-source guard is needed here: a vertex enters the queue only
// when its distance improves below infty, so the absorbing-element case the
// arc-sweep variant must skip never reaches a relaxation.
// O(n m) worst case, far below it when few vertices stay active per round.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          bellman_ford_moore_traits Traits = bellman_ford_moore_default_traits<
              Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
    requires outward_incidence_graph<Graph>
class bellman_ford_moore {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;

private:
    Graph _graph;
    LengthMap _length_map;
    vertex_map_t<Graph, length_type> _distances_map;
    std::vector<vertex> _queue;
    std::vector<vertex> _next_queue;
    vertex_map_t<Graph, bool> _in_queue_map;

    MELON_NO_UNIQUE_ADDRESS detail::vertex_map_if<
        Traits::store_paths && !has_arc_source<Graph>, Graph, vertex>
        _pred_vertices_map;
    MELON_NO_UNIQUE_ADDRESS
    detail::vertex_map_if<Traits::store_paths, Graph, std::optional<arc>>
        _pred_arcs_map;

    struct no_cycle_witness {};
    MELON_NO_UNIQUE_ADDRESS std::conditional_t<
        Traits::detect_negative_cycles, std::optional<vertex>, no_cycle_witness>
        _cycle_witness{};

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
        requires has_vertex_map<Graph>
    constexpr bellman_ford_moore(G && g, LM && lm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _distances_map(
              create_vertex_map<length_type>(_graph, Traits::semiring::infty))
        , _in_queue_map(create_vertex_map<bool>(_graph, false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr bellman_ford_moore(G && g, LM && lm, const vertex & s)
        : bellman_ford_moore(std::forward<G>(g), std::forward<LM>(lm)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<bellman_ford_moore, Args...>
    constexpr bellman_ford_moore(Traits, Args &&... args)
        : bellman_ford_moore(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr bellman_ford_moore(const bellman_ford_moore &) = delete;
    constexpr bellman_ford_moore(bellman_ford_moore &&) = default;

    constexpr bellman_ford_moore & operator=(const bellman_ford_moore &) =
        delete;
    constexpr bellman_ford_moore & operator=(bellman_ford_moore &&) = default;

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

    constexpr bellman_ford_moore & reset() {
        _distances_map.fill(Traits::semiring::infty);
        _queue.clear();
        _next_queue.clear();
        _in_queue_map.fill(false);
        if constexpr(Traits::store_paths) _pred_arcs_map.fill(std::nullopt);
        if constexpr(Traits::detect_negative_cycles) _cycle_witness.reset();
        return *this;
    }

    // Strict precondition, like every rooted algorithm in the family: the
    // vertex must be untouched. Re-seeding a reached one silently corrupts
    // stored paths and distances.
    constexpr bellman_ford_moore & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(!reached(s));
        _distances_map[s] = dist;
        _queue.push_back(s);
        _in_queue_map[s] = true;
        if constexpr(Traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<Graph>) _pred_vertices_map[s] = s;
        }
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    constexpr bellman_ford_moore & run() {
        if constexpr(Traits::detect_negative_cycles) {
            // Without this guard a second run() on a detected cycle would
            // keep relaxing it -- the family contract makes re-running a
            // no-op.
            if(_cycle_witness.has_value()) return *this;
        }

        auto process_round = [this]() {
            for(const vertex & u : _queue) _in_queue_map[u] = false;
            for(const vertex & u : _queue) {
                auto && out_arcs_range = melon::out_arcs(_graph, u);
                detail::prefetch_keys_and_values(
                    out_arcs_range, arc_targets_map(_graph), _length_map);
                // A reference, not a copy: an earlier relaxation of this
                // round -- or this vertex's own self-loop -- may improve u's
                // distance mid-scan, and relaxing from the fresher value
                // converges in fewer rounds.
                const length_type & u_dist = _distances_map[u];
                for(const arc & a : out_arcs_range) {
                    const vertex & w = melon::arc_target(_graph, a);
                    length_type & w_dist = _distances_map[w];
                    const length_type new_dist =
                        Traits::semiring::plus(u_dist, _length_map[a]);
                    if(Traits::semiring::less(new_dist, w_dist)) {
                        w_dist = new_dist;
                        if constexpr(Traits::store_paths) {
                            _pred_arcs_map[w].emplace(a);
                            if constexpr(!has_arc_source<Graph>)
                                _pred_vertices_map[w] = u;
                        }
                        if(!_in_queue_map[w]) {
                            _in_queue_map[w] = true;
                            _next_queue.push_back(w);
                        }
                    }
                }
            }
            _queue.swap(_next_queue);
            _next_queue.clear();
        };

        // A round is a relaxation pass restricted to the vertices the
        // previous round improved, so the pass-count arithmetic is
        // bellman_ford's: n-1 rounds compute every shortest distance, and
        // only a negative cycle reachable from the sources can keep the
        // queue busy past them.
        if constexpr(has_num_vertices<Graph>) {
            const std::size_t n = num_vertices(_graph);
            for(std::size_t round = 1; round < n; ++round) {
                if(_queue.empty()) break;
                process_round();
            }
        } else {
            for([[maybe_unused]] auto && v :
                vertices(_graph) | std::views::drop(1)) {
                if(_queue.empty()) break;
                process_round();
            }
        }
        if constexpr(Traits::detect_negative_cycles) {
            // The certifying round: an improvement after n-1 complete rounds
            // is something only a reachable negative cycle can produce.
            if(!_queue.empty()) process_round();
            if(!_queue.empty()) _cycle_witness.emplace(_queue.front());
        }
        return *this;
    }

    // ---- Queries ------------------------------------------------------------

    // False until run()'s certifying round still improves something. While
    // true, every stored distance is a mid-round snapshot with no meaning,
    // and the pred maps contain a cycle.
    [[nodiscard]] constexpr bool found_negative_cycle() const noexcept
        requires(Traits::detect_negative_cycles)
    {
        return _cycle_witness.has_value();
    }

    // The arcs of one reachable negative cycle, in path order: each arc's
    // target is the next one's source, the last closing on the first.
    // O(n) here rather than in run(): the witness is a vertex the certifying
    // round improved, so its estimate reflects a walk of at least n arcs and
    // its pred chain -- a functional graph, hence a rho shape whose cycle a
    // walk cannot leave -- is inside its cycle after n steps; every cycle of
    // the pred maps is a negative cycle. One more lap collects it.
    [[nodiscard]] constexpr std::vector<arc> negative_cycle() const
        requires(Traits::detect_negative_cycles && Traits::store_paths)
    {
        assert(_cycle_witness.has_value());
        vertex x = _cycle_witness.value();
        if constexpr(has_num_vertices<Graph>) {
            for(std::size_t i = 0; i < num_vertices(_graph); ++i)
                x = pred_vertex(x);
        } else {
            for([[maybe_unused]] auto && v : vertices(_graph))
                x = pred_vertex(x);
        }
        std::vector<arc> cycle;
        vertex y = x;
        do {
            cycle.push_back(pred_arc(y));
            y = pred_vertex(y);
        } while(y != x);
        std::ranges::reverse(cycle);
        return cycle;
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(Traits::semiring::less(_distances_map[u],
                                                 Traits::semiring::infty))) {
        return Traits::semiring::less(_distances_map[u],
                                      Traits::semiring::infty);
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put, mapping_ref_view's contract.
    [[nodiscard]] constexpr auto reached_map() const & {
        return maps::map([this](const vertex & u) { return reached(u); });
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::map([dists = std::move(_distances_map)](const vertex & u) {
            return Traits::semiring::less(dists[u], Traits::semiring::infty);
        });
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        requires(Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        return *_pred_arcs_map[u];
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const
        requires(Traits::store_paths)
    {
        assert(reached(u) && _pred_arcs_map[u].has_value());
        if constexpr(has_arc_source<Graph>)
            return melon::arc_source(_graph, pred_arc(u));
        else
            return _pred_vertices_map[u];
    }
    // For an unreached vertex this is the semiring's infty; under a violated
    // or detected negative-cycle precondition it is meaningless for every
    // vertex.
    [[nodiscard]] constexpr length_type dist(const vertex & u) const
        noexcept(noexcept(length_type(_distances_map[u]))) {
        return _distances_map[u];
    }
    [[nodiscard]] constexpr auto dists_map() const & noexcept(
        noexcept(maps::mapping_all(_distances_map))) {
        return maps::mapping_all(_distances_map);
    }
    [[nodiscard]] constexpr auto dists_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_distances_map)))) {
        return maps::mapping_all(std::move(_distances_map));
    }

private:
    class path_iterator
        : public detail::intrusive_iterator_base<bellman_ford_moore, vertex> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a sentinel friend
        // reading _pred_arcs_map directly fails to compile there.
        [[nodiscard]] constexpr bool _at_path_end() const {
            return !this->_structure->_pred_arcs_map[this->_cursor].has_value();
        }

    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<bellman_ford_moore,
                                              vertex>::intrusive_iterator_base;

        // A plain prvalue, not a `const` one: a const prvalue inhibits moves
        // and makes std::iterator_traits disagree with the `reference` typedef
        // above -- a std::indirectly_readable hazard.
        constexpr reference operator*() const {
            return this->_structure->_pred_arcs_map[this->_cursor].value();
        }
        constexpr path_iterator & operator++() {
            this->_cursor = this->_structure->pred_vertex(this->_cursor);
            return *this;
        }
        constexpr path_iterator operator++(int) {
            path_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it, std::default_sentinel_t) {
            return it._at_path_end();
        }
        [[nodiscard]] constexpr friend bool operator==(
            const path_iterator & it1,
            const path_iterator & it2) noexcept(noexcept(it1._cursor ==
                                                         it2._cursor)) {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

public:
    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr auto path_to(const vertex & t) const
        noexcept(noexcept(std::ranges::subrange(path_iterator(this, t),
                                                std::default_sentinel)))
        requires(Traits::store_paths)
    {
        // With a negative cycle the pred maps are cyclic and the iterator
        // never meets its sentinel -- checkable only when detection was
        // opted in. For an unreached vertex it meets the sentinel
        // immediately, which reads as "t sits at a source" -- both are
        // precondition violations, not empty answers.
        if constexpr(Traits::detect_negative_cycles)
            assert(!found_negative_cycle());
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

template <typename Graph, typename LengthMap>
bellman_ford_moore(Graph &&, LengthMap &&)
    -> bellman_ford_moore<views::graph_all_t<Graph>,
                          maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap>
bellman_ford_moore(Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> bellman_ford_moore<views::graph_all_t<Graph>,
                          maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Traits>
bellman_ford_moore(Traits, Graph &&, LengthMap &&)
    -> bellman_ford_moore<views::graph_all_t<Graph>,
                          maps::mapping_all_t<LengthMap>, Traits>;

template <typename Graph, typename LengthMap, typename Traits>
bellman_ford_moore(Traits, Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> bellman_ford_moore<views::graph_all_t<Graph>,
                          maps::mapping_all_t<LengthMap>, Traits>;

}  // namespace melon
