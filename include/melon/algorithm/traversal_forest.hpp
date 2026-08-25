#pragma once

#include <cassert>
#include <ranges>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"

namespace melon {

// forward_range, not range: reset() rewinds the sources, so a single-pass
// range must fail this constraint instead of hard-erroring inside the
// consumable_view member below.
template <graph_view Graph, std::ranges::forward_range Sources>
    requires std::same_as<std::ranges::range_value_t<Sources>, vertex_t<Graph>>
class traversal_forest
    : public algorithm_view_interface<traversal_forest<Graph, Sources>> {
private:
    using vertex = vertex_t<Graph>;
    struct bfs_traits {
        static constexpr bool store_pred_vertices = false;
        static constexpr bool store_pred_arcs = false;
        static constexpr bool store_distances = false;
        static constexpr bool store_traversal_range = true;
    };

private:
    // _bfs owns the only copy of the graph view; base() is how the source
    // ranges below reach it. A separate `Graph _graph` member that _bfs is
    // constructed *from* makes an owned graph unusable: graph_all on the stored
    // lvalue view has to return it by value, and graph_owning_view's copy
    // constructor is deleted, so `traversal_forest(static_digraph{...})` would
    // not compile. Declared first so it is initialised before
    // _remaining_sources reads it.
    breadth_first_search<Graph, bfs_traits> _bfs;
    detail::consumable_view<Sources> _remaining_sources;
    // True when _remaining_sources was derived from vertices(_bfs.base()) by
    // the single-argument constructor. Those are the only sources that can
    // point back *into this object*, so they are the only ones the relocation
    // rebase below may re-derive; a caller-supplied range (the two-argument
    // constructor) refers to storage the caller owns and must be left alone.
    // A runtime flag, not a type-level test: the caller-supplied range can
    // have exactly the type vertices(_bfs.base()) would produce.
    bool _sources_from_base = false;

public:
    // ---- Construction -------------------------------------------------------

    template <typename G>
        requires detail::not_self<G, traversal_forest> && graph_for<G, Graph> &&
                     std::constructible_from<
                         breadth_first_search<Graph, bfs_traits>, G &&>
    constexpr explicit traversal_forest(G && g)
        : _bfs(std::forward<G>(g))
        , _remaining_sources(vertices(_bfs.base()))
        , _sources_from_base(true) {
        // Guarded: advance() reads
        // _remaining_sources.current(), which on an
        // empty source range reads past the end of an empty
        // view.
        if(!finished()) advance();
    }

    // std::forward, not a bare `sources`: the parameter is a named rvalue
    // reference, hence an lvalue, so std::views::all() would hand back a
    // ref_view where the deduction guide has already committed Sources to
    // std::views::all_t<SR> -- an owning_view for an rvalue range, and a
    // temporary container would not compile at all.
    template <typename G, std::ranges::range SR>
        requires detail::not_self<G, traversal_forest> && graph_for<G, Graph> &&
                     std::constructible_from<detail::consumable_view<Sources>,
                                             std::views::all_t<SR>> &&
                     std::convertible_to<std::ranges::range_value_t<SR>,
                                         vertex_t<Graph>> &&
                     std::constructible_from<
                         breadth_first_search<Graph, bfs_traits>, G &&>
    constexpr traversal_forest(G && g, SR && sources)
        : _bfs(std::forward<G>(g))
        , _remaining_sources(std::views::all(std::forward<SR>(sources))) {
        if(!finished()) advance();
    }

    // The relocation policy, restricted to the one cursor that can point back
    // into this object (see _sources_from_base): after the members relocate,
    // the source range is re-derived from the *new* _bfs.base() and the
    // _consumed counter puts the cursor back where it was. The inner _bfs
    // carries its own policy.
    //
    // Move-only; see the melon::traversal_algorithm concept for the ruling.
    constexpr traversal_forest(const traversal_forest &) = delete;
    constexpr traversal_forest(traversal_forest && o)
        : _bfs(std::move(o._bfs))
        // Not always a memberwise move: for self-referential sources the
        // cursor's move constructor reseeks through the *old* range, whose
        // predicate reads the graph inside the _bfs moved away one line up.
        // Those are rebuilt from the new base's vertices and the old
        // consumed count; caller-supplied sources move as members.
        , _remaining_sources([&]() -> detail::consumable_view<Sources> {
            if constexpr(!borrowed_graph<Graph> &&
                         !std::ranges::borrowed_range<Sources> && requires {
                             detail::consumable_view<Sources>(
                                 vertices(_bfs.base()),
                                 o._remaining_sources.consumed());
                         }) {
                if(o._sources_from_base)
                    return detail::consumable_view<Sources>(
                        vertices(_bfs.base()), o._remaining_sources.consumed());
            }
            return std::move(o._remaining_sources);
        }())
        , _sources_from_base(o._sources_from_base) {}

    constexpr traversal_forest & operator=(const traversal_forest &) = delete;
    // See the move constructor.
    constexpr traversal_forest & operator=(traversal_forest && o) {
        if(this == std::addressof(o)) return *this;
        _bfs = std::move(o._bfs);
        _sources_from_base = o._sources_from_base;
        if constexpr(!borrowed_graph<Graph> &&
                     !std::ranges::borrowed_range<Sources> && requires {
                         detail::consumable_view<Sources>(
                             vertices(_bfs.base()),
                             o._remaining_sources.consumed());
                     }) {
            if(_sources_from_base) {
                _remaining_sources = detail::consumable_view<Sources>(
                    vertices(_bfs.base()), o._remaining_sources.consumed());
                return *this;
            }
        }
        _remaining_sources = std::move(o._remaining_sources);
        return *this;
    }

    // ---- Base access --------------------------------------------------------

    [[nodiscard]] constexpr Graph & base() & noexcept { return _bfs.base(); }
    [[nodiscard]] constexpr const Graph & base() const & noexcept {
        return _bfs.base();
    }
    [[nodiscard]] constexpr Graph && base() && noexcept {
        return std::move(_bfs).base();
    }
    [[nodiscard]] constexpr const Graph && base() const && noexcept {
        return std::move(_bfs).base();
    }

    // ---- Setup --------------------------------------------------------------

    // Rewinds the sources the constructor was given -- re-deriving them from
    // the graph would throw a caller-supplied source range away, and for the
    // two-argument form would not compile, the two ranges' iterators being
    // unrelated types. The advance() restores the constructor's postcondition,
    // that current() names the first tree.
    constexpr traversal_forest & reset() {
        _remaining_sources.rewind();
        _bfs.reset();
        if(!finished()) advance();
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_remaining_sources.empty())) {
        return _remaining_sources.empty();
    }

    [[nodiscard]] constexpr auto current() const
        noexcept(noexcept(_bfs.traversal())) {
        assert(!finished());
        return _bfs.traversal();
    }

    constexpr void advance() {
        assert(!finished());
        while(_bfs.reached(_remaining_sources.current())) {
            _remaining_sources.advance();
            if(_remaining_sources.empty()) return;
        }
        _bfs.add_source(_remaining_sources.current()).run();
    }

    // ---- Queries ------------------------------------------------------------

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_bfs.reached(u))) {
        return _bfs.reached(u);
    }
    [[nodiscard]] constexpr auto reached_map() const & noexcept(
        noexcept(_bfs.reached_map())) {
        return _bfs.reached_map();
    }
    // Terminal, like std::move(alg).base() -- no other member may be called
    // afterwards.
    [[nodiscard]] constexpr auto reached_map() && noexcept(
        noexcept(std::move(_bfs).reached_map())) {
        return std::move(_bfs).reached_map();
    }
};

template <typename Graph>
traversal_forest(Graph &&)
    -> traversal_forest<views::graph_all_t<Graph>, vertices_range_t<Graph>>;

template <typename Graph, typename Sources>
traversal_forest(Graph &&, Sources &&)
    -> traversal_forest<views::graph_all_t<Graph>, std::views::all_t<Sources>>;

}  // namespace melon
