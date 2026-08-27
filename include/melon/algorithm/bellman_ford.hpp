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
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/utility/semiring.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <typename Traits>
concept bellman_ford_traits = semiring<typename Traits::semiring> && requires {
    { Traits::store_paths } -> std::convertible_to<bool>;
    { Traits::detect_negative_cycles } -> std::convertible_to<bool>;
};

template <typename Graph, typename ValueType>
struct bellman_ford_default_traits {
    using semiring = shortest_path_semiring<ValueType>;
    static constexpr bool store_paths = false;
    static constexpr bool detect_negative_cycles = false;
};

// Unlike dijkstra there is no precondition on the mapped values: lengths that
// improve a distance when combined -- negative, under the default semiring --
// are the reason this algorithm exists. What remains is a precondition on the
// graph: no negative cycle reachable from the sources. It is uncheckable, and
// a violation silently yields meaningless distances -- with store_paths, even
// a path_to() that never terminates, the pred maps being cyclic. Traits with
// detect_negative_cycles lift the precondition: run() spends one certifying
// pass, found_negative_cycle() reports, and with store_paths as well,
// negative_cycle() returns the culprit.
//
// No incidence requirement: relaxation reads arcs_entries, which the base
// graph concept already provides, so arc-list-only structures qualify --
// re-tightening to an incidence concept would shut them out for nothing.
// O(n m), with the early exit typically far below it.
template <graph_view Graph, mapping_view<arc_t<Graph>> LengthMap,
          bellman_ford_traits Traits = bellman_ford_default_traits<
              Graph, mapped_value_t<LengthMap, arc_t<Graph>>>>
class bellman_ford {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using length_type = mapped_value_t<LengthMap, arc_t<Graph>>;

    // The witness only serves negative_cycle(), which needs the pred maps.
    static constexpr bool _stores_witness =
        Traits::detect_negative_cycles && Traits::store_paths;

private:
    Graph _graph;
    LengthMap _length_map;
    vertex_map_t<Graph, length_type> _distances_map;

    [[no_unique_address]] detail::vertex_map_if<
        Traits::store_paths && !has_arc_source<Graph>, Graph, vertex>
        _pred_vertices_map;
    [[no_unique_address]] detail::vertex_map_if<
        Traits::store_paths, Graph, std::optional<arc>> _pred_arcs_map;

    struct no_found_negative_cycle {};
    [[no_unique_address]] std::conditional_t<Traits::detect_negative_cycles,
                                             bool, no_found_negative_cycle>
        _found_negative_cycle{};
    struct no_cycle_witness {};
    [[no_unique_address]] std::conditional_t<_stores_witness,
                                             std::optional<vertex>,
                                             no_cycle_witness> _cycle_witness{};

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
        requires has_vertex_map<Graph>
    constexpr bellman_ford(G && g, LM && lm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _length_map(maps::mapping_all(std::forward<LM>(lm)))
        , _distances_map(
              create_vertex_map<length_type>(_graph, Traits::semiring::infty))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph) {}

    template <graph_for<Graph> G, mapping_for<LengthMap> LM>
    constexpr bellman_ford(G && g, LM && lm, const vertex & s)
        : bellman_ford(std::forward<G>(g), std::forward<LM>(lm)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<bellman_ford, Args...>
    constexpr bellman_ford(Traits, Args &&... args)
        : bellman_ford(std::forward<Args>(args)...) {}

    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr bellman_ford(const bellman_ford &) = delete;
    constexpr bellman_ford(bellman_ford &&) = default;

    constexpr bellman_ford & operator=(const bellman_ford &) = delete;
    constexpr bellman_ford & operator=(bellman_ford &&) = default;

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

    constexpr bellman_ford & reset() {
        _distances_map.fill(Traits::semiring::infty);
        if constexpr(Traits::store_paths) _pred_arcs_map.fill(std::nullopt);
        if constexpr(Traits::detect_negative_cycles)
            _found_negative_cycle = false;
        if constexpr(_stores_witness) _cycle_witness.reset();
        return *this;
    }

    // Strict precondition, like every rooted algorithm in the family: the
    // vertex must be untouched. Re-seeding a reached one silently corrupts
    // stored paths and distances.
    constexpr bellman_ford & add_source(
        const vertex & s, const length_type & dist = Traits::semiring::zero) {
        assert(!reached(s));
        _distances_map[s] = dist;
        if constexpr(Traits::store_paths) {
            _pred_arcs_map[s].reset();
            if constexpr(!has_arc_source<Graph>) _pred_vertices_map[s] = s;
        }
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    constexpr bellman_ford & run() {
        if constexpr(Traits::detect_negative_cycles) {
            // Without this guard a second run() on a detected cycle would
            // keep relaxing it -- the family contract makes re-running a
            // no-op.
            if(_found_negative_cycle) return *this;
        }

        // The witness recorder comes in as a callable, not a bool tag: an
        // `if constexpr` on such a tag around `_cycle_witness.emplace(v)`
        // makes clang type-check the non-dependent discarded branch while
        // substituting the generic lambda -- and no_cycle_witness has no
        // emplace.
        auto relax = [this](auto record_witness) -> bool {
            bool updated = false;
            for(auto && [a, uv] : arcs_entries(_graph)) {
                auto && [u, v] = uv;
                const length_type & u_dist = _distances_map[u];
                if constexpr(!has_absorbing_infty<typename Traits::semiring>) {
                    // Unless plus absorbs infty, an arc out of an unreached
                    // vertex must not be relaxed: infty + length is signed
                    // overflow for integral min-plus, and any finite result
                    // lets a nonexistent path improve a real one.
                    if(!Traits::semiring::less(u_dist, Traits::semiring::infty))
                        continue;
                }
                length_type & v_dist = _distances_map[v];
                const length_type new_dist =
                    Traits::semiring::plus(u_dist, _length_map[a]);
                if constexpr(std::is_trivially_copyable_v<length_type> &&
                             !Traits::store_paths) {
                    // The select compiles to a conditional move where the
                    // `if` form compiles to a branch that mispredicts while
                    // relaxation is still churning. It is confined to this
                    // configuration because the always-store is a wasted deep
                    // copy for non-trivial length types, and the pred-map
                    // writes below reintroduce the branch anyway.
                    const bool improve =
                        Traits::semiring::less(new_dist, v_dist);
                    v_dist = improve ? new_dist : v_dist;
                    updated |= improve;
                } else {
                    if(Traits::semiring::less(new_dist, v_dist)) {
                        v_dist = new_dist;
                        if constexpr(Traits::store_paths) {
                            _pred_arcs_map[v].emplace(a);
                            if constexpr(!has_arc_source<Graph>)
                                _pred_vertices_map[v] = u;
                        }
                        record_witness(v);
                        updated = true;
                    }
                }
            }
            return updated;
        };
        auto ignore_witness = [](const vertex &) {};

        // n-1 passes compute every shortest distance -- a shortest path has
        // at most n-1 arcs -- and stop at the first quiet one. An n-th pass
        // never does legitimate work; it exists only to certify, so it is
        // exactly the detection opt-in below.
        if constexpr(has_num_vertices<Graph>) {
            const std::size_t n = num_vertices(_graph);
            for(std::size_t pass = 1; pass < n; ++pass) {
                if(!relax(ignore_witness)) return *this;
            }
        } else {
            for([[maybe_unused]] auto && v :
                vertices(_graph) | std::views::drop(1)) {
                if(!relax(ignore_witness)) return *this;
            }
        }
        if constexpr(Traits::detect_negative_cycles) {
            // An improvement after n-1 complete passes is something only a
            // negative cycle reachable from the sources can produce.
            if constexpr(_stores_witness) {
                _found_negative_cycle = relax(
                    [this](const vertex & v) { _cycle_witness.emplace(v); });
            } else {
                _found_negative_cycle = relax(ignore_witness);
            }
        }
        return *this;
    }

    // ---- Queries ------------------------------------------------------------

    // False until run()'s certifying pass still improves something. While
    // true, every stored distance is a mid-pass snapshot with no meaning,
    // and the pred maps contain a cycle.
    [[nodiscard]] constexpr bool found_negative_cycle() const noexcept
        requires(Traits::detect_negative_cycles)
    {
        return _found_negative_cycle;
    }

    // The arcs of one reachable negative cycle, in path order: each arc's
    // target is the next one's source, the last closing on the first.
    // O(n) here rather than in run(): the witness is a vertex the certifying
    // pass improved, so its estimate reflects a walk of at least n arcs and
    // its pred chain -- a functional graph, hence a rho shape whose cycle a
    // walk cannot leave -- is inside its cycle after n steps; every cycle of
    // the pred maps is a negative cycle. One more lap collects it.
    [[nodiscard]] constexpr std::vector<arc> negative_cycle() const
        requires(Traits::detect_negative_cycles && Traits::store_paths)
    {
        assert(_found_negative_cycle && _cycle_witness.has_value());
        vertex x = *_cycle_witness;
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
        : public detail::intrusive_iterator_base<bellman_ford, vertex> {
    public:
        using value_type = arc;
        using reference = arc;
        using detail::intrusive_iterator_base<bellman_ford,
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
            return !it._structure->_pred_arcs_map[it._cursor].has_value();
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
            assert(!_found_negative_cycle);
        assert(reached(t));
        return std::ranges::subrange(path_iterator(this, t),
                                     std::default_sentinel);
    }
};

template <typename Graph, typename LengthMap>
bellman_ford(Graph &&, LengthMap &&)
    -> bellman_ford<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap>
bellman_ford(Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> bellman_ford<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>>;

template <typename Graph, typename LengthMap, typename Traits>
bellman_ford(Traits, Graph &&, LengthMap &&)
    -> bellman_ford<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
                    Traits>;

template <typename Graph, typename LengthMap, typename Traits>
bellman_ford(Traits, Graph &&, LengthMap &&, const vertex_t<Graph> &)
    -> bellman_ford<views::graph_all_t<Graph>, maps::mapping_all_t<LengthMap>,
                    Traits>;

}  // namespace melon
