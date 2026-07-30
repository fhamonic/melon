#pragma once

#include <cassert>
#include <ranges>

#include "melon/algorithm/breadth_first_search.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"

namespace melon {

template <graph_view Graph, std::ranges::range Sources>
    requires has_vertex_map<Graph> &&
             std::same_as<std::ranges::range_value_t<Sources>, vertex_t<Graph>>
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
    // ranges below reach it. There used to be a separate `Graph _graph` member
    // that _bfs was then constructed *from*, which is why an owned graph never
    // worked: graph_all on the stored lvalue view has to return it by value,
    // and graph_owning_view's copy constructor is deleted, so
    // `traversal_forest(static_digraph{...})` did not compile. Declared first
    // so it is initialised before _remaining_sources reads it.
    breadth_first_search<Graph, bfs_traits> _bfs;
    consumable_view<Sources> _remaining_sources;
    // True when _remaining_sources was derived from vertices(_bfs.base()) by
    // the single-argument constructor. Those are the only sources that can
    // point back *into this object*, so they are the only ones the relocation
    // rebase below may re-derive; a caller-supplied range (the two-argument
    // constructor) refers to storage the caller owns and must be left alone.
    // A runtime flag, not a type-level test: the caller-supplied range can
    // have exactly the type vertices(_bfs.base()) would produce.
    bool _sources_from_base = false;

public:
    // graph_storable_as, so std::is_constructible answers what constructing
    // the inner traversal actually does -- see dijkstra's constructor.
    template <typename G>
        requires detail::not_self<G, traversal_forest> &&
                 graph_storable_as<G, Graph>
    constexpr explicit traversal_forest(G && g)
        : _bfs(std::forward<G>(g))
        , _remaining_sources(vertices(_bfs.base()))
        , _sources_from_base(true) {
        // Guarded: advance() reads _remaining_sources.current(), so an empty
        // source range read past the end of an empty view (an assertion failure
        // in a debug build, silent UB in a release one).
        if(!finished()) advance();
    }

    // std::forward, not a bare `sources`: the parameter is a named rvalue
    // reference, hence an lvalue, so std::views::all() handed back a ref_view
    // where the deduction guide had already committed Sources to
    // std::views::all_t<S> -- an owning_view for an rvalue range. Passing a
    // temporary container did not compile at all.
    template <typename G, typename S>
        requires detail::not_self<G, traversal_forest> &&
                 graph_storable_as<G, Graph> &&
                 std::constructible_from<consumable_view<Sources>,
                                         std::views::all_t<S>>
    constexpr traversal_forest(G && g, S && sources)
        : _bfs(std::forward<G>(g))
        , _remaining_sources(std::views::all(std::forward<S>(sources))) {
        if(!finished()) advance();
    }

    // The relocation policy is depth_first_search's, restricted to the one
    // cursor that can point back into this object (see _sources_from_base):
    // after the members relocate, _rebase_sources() re-derives the source
    // range from the *new* _bfs.base() and the _consumed counter puts the
    // cursor back where it was. The inner _bfs carries its own policy.
    constexpr traversal_forest(const traversal_forest & o)
        requires std::copy_constructible<decltype(_bfs)> &&
                     std::copy_constructible<consumable_view<Sources>>
        : _bfs(o._bfs)
        , _remaining_sources(o._remaining_sources)
        , _sources_from_base(o._sources_from_base) {
        _rebase_sources();
    }
    constexpr traversal_forest(traversal_forest && o)
        : _bfs(std::move(o._bfs))
        // Not always a memberwise move: for self-referential sources the
        // cursor's move constructor reseeks through the *old* range, whose
        // predicate reads the graph inside the _bfs moved away one line up.
        // Those are rebuilt from the new base's vertices and the old
        // consumed count; caller-supplied sources move as members.
        , _remaining_sources([&]() -> consumable_view<Sources> {
            if constexpr(!borrowed_graph<Graph> &&
                         !std::ranges::borrowed_range<Sources> &&
                         requires {
                             consumable_view<Sources>(
                                 vertices(_bfs.base()),
                                 o._remaining_sources.consumed());
                         }) {
                if(o._sources_from_base)
                    return consumable_view<Sources>(
                        vertices(_bfs.base()),
                        o._remaining_sources.consumed());
            }
            return std::move(o._remaining_sources);
        }())
        , _sources_from_base(o._sources_from_base) {}

    constexpr traversal_forest & operator=(const traversal_forest & o)
        requires std::copyable<decltype(_bfs)> &&
                     std::copyable<consumable_view<Sources>>
    {
        if(this == std::addressof(o)) return *this;
        _bfs = o._bfs;
        _remaining_sources = o._remaining_sources;
        _sources_from_base = o._sources_from_base;
        _rebase_sources();
        return *this;
    }
    // See the move constructor.
    constexpr traversal_forest & operator=(traversal_forest && o) {
        if(this == std::addressof(o)) return *this;
        _bfs = std::move(o._bfs);
        _sources_from_base = o._sources_from_base;
        if constexpr(!borrowed_graph<Graph> &&
                     !std::ranges::borrowed_range<Sources> &&
                     requires {
                         consumable_view<Sources>(
                             vertices(_bfs.base()),
                             o._remaining_sources.consumed());
                     }) {
            if(_sources_from_base) {
                _remaining_sources = consumable_view<Sources>(
                    vertices(_bfs.base()), o._remaining_sources.consumed());
                return *this;
            }
        }
        _remaining_sources = std::move(o._remaining_sources);
        return *this;
    }

private:
    // See depth_first_search::_rebase_stack. Guarded three ways: compile-time
    // on the graph being non-borrowed and the source range being re-derivable
    // at all, and at runtime on the sources actually coming from this
    // object's own graph.
    constexpr void _rebase_sources() {
        if constexpr(!borrowed_graph<Graph> &&
                     !std::ranges::borrowed_range<Sources> &&
                     requires {
                         _remaining_sources.rebase(vertices(_bfs.base()));
                     }) {
            if(_sources_from_base)
                _remaining_sources.rebase(vertices(_bfs.base()));
        }
    }

public:

    // Delegated: _bfs holds the only copy of the graph view, which is the whole
    // point of reaching it through base() rather than storing it twice.
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

    // Rewinds the sources the constructor was given. Assigning vertices(_graph)
    // here threw a user-supplied source range away, and for the two-argument
    // form did not even compile: the two ranges' iterators are unrelated types.
    // The advance() restores the constructor's postcondition, that current()
    // names the first tree.
    constexpr traversal_forest & reset() {
        _remaining_sources.rewind();
        _bfs.reset();
        if(!finished()) advance();
        return *this;
    }

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

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_bfs.reached(u))) {
        return _bfs.reached(u);
    }
    // Delegated, like reached() right above.
    [[nodiscard]] constexpr auto reached_map() const
        noexcept(noexcept(_bfs.reached_map())) {
        return _bfs.reached_map();
    }
};

template <typename Graph>
traversal_forest(Graph &&)
    -> traversal_forest<views::graph_all_t<Graph>, vertices_range_t<Graph>>;

template <typename Graph, typename Sources>
traversal_forest(Graph &&, Sources &&)
    -> traversal_forest<views::graph_all_t<Graph>, std::views::all_t<Sources>>;

}  // namespace melon
