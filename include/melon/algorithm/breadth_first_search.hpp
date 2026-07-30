#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/map_if.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"

namespace melon {

// A traits concept, like the dijkstra family's: without it a misspelled flag
// in a user traits struct silently fell back to nothing instead of failing
// the constraint.
// clang-format off
template <typename Traits>
concept breadth_first_search_traits = requires() {
    { Traits::store_pred_vertices } -> std::convertible_to<bool>;
    { Traits::store_pred_arcs } -> std::convertible_to<bool>;
    { Traits::store_distances } -> std::convertible_to<bool>;
    { Traits::store_traversal_range } -> std::convertible_to<bool>;
};
// clang-format on

struct breadth_first_search_default_traits {
    static constexpr bool store_pred_vertices = false;
    static constexpr bool store_pred_arcs = false;
    static constexpr bool store_distances = false;
    static constexpr bool store_traversal_range = false;
};

namespace detail {
template <typename Graph, typename Traits>
concept enable_branchless_bfs =
    has_num_vertices<Graph> && std::is_trivially_copyable_v<vertex_t<Graph>> &&
    (!Traits::store_pred_vertices && !Traits::store_pred_arcs &&
     !Traits::store_distances);
}

template <graph_view Graph, breadth_first_search_traits Traits =
                                breadth_first_search_default_traits>
class breadth_first_search;

template <graph_view Graph, breadth_first_search_traits Traits>
    requires outward_adjacency_graph<Graph> && has_vertex_map<Graph> &&
             (!detail::enable_branchless_bfs<Graph, Traits>)
class breadth_first_search<Graph, Traits>
    : public algorithm_view_interface<breadth_first_search<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    static_assert(!Traits::store_pred_arcs || outward_incidence_graph<Graph>,
                  "storing predecessor arcs requires outward_incidence_graph.");

    // size_type, not int: the fallback is an index into _queue and was being
    // compared against _queue.size().
    using cursor = std::conditional_t<has_num_vertices<Graph>,
                                      typename std::vector<vertex>::iterator,
                                      typename std::vector<vertex>::size_type>;

private:
    Graph _graph;
    std::vector<vertex> _queue;
    [[no_unique_address]] std::conditional_t<Traits::store_traversal_range,
                                             cursor, std::monostate>
        _queue_traversal_begin;
    cursor _queue_current;
    vertex_map_t<Graph, bool> _reached_map;

    [[no_unique_address]] vertex_map_if<Traits::store_pred_vertices, Graph,
                                        vertex> _pred_vertices_map;
    [[no_unique_address]] vertex_map_if<Traits::store_pred_arcs, Graph, arc>
        _pred_arcs_map;
    [[no_unique_address]] vertex_map_if<Traits::store_distances, Graph, int>
        _dist_map;

public:
    // graph_storable_as, so std::is_constructible answers what the
    // mem-initializer actually does -- see dijkstra's constructor.
    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                     graph_storable_as<G, Graph>
    constexpr explicit breadth_first_search(G && g)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _queue()
        , _reached_map(create_vertex_map<bool>(_graph, false))
        , _pred_vertices_map(_graph)
        , _pred_arcs_map(_graph)
        , _dist_map(_graph) {
        if constexpr(has_num_vertices<Graph>) {
            _queue.reserve(num_vertices(_graph));
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
    }

    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                 graph_storable_as<G, Graph>
    constexpr breadth_first_search(G && g, const vertex & s)
        : breadth_first_search(std::forward<G>(g)) {
        add_source(s);
    }

    // Constrained on the delegate it forwards to, so the tag overload is
    // exactly as constructible as the constructor it names.
    template <typename... Args>
        requires std::constructible_from<breadth_first_search, Args...>
    constexpr breadth_first_search(Traits, Args &&... args)
        : breadth_first_search(std::forward<Args>(args)...) {}

    // With has_num_vertices the cursor is an iterator *into* _queue, and the
    // traversal relies on the constructor's reserve() to keep it stable across
    // the push_backs. The defaulted copy this replaces handed the new object an
    // iterator into the *source's* buffer, at a capacity only as large as the
    // source's size: finished() compared iterators from two different vectors
    // and advance() then read freed memory (ASan: heap-use-after-free). Copy
    // the queue, restore the capacity, then rebase -- the shape
    // topological_sort, connected_components, kruskal and the branchless
    // specialisation below all already had. This was the one member of the
    // family the sweep missed, because the branchless copy right below it was
    // hand-written and looked like the job was done. Move stays defaulted: the
    // buffer transfers.
    // Constrained on the copyability of what it copies: a user-provided
    // special member of a class template is only instantiated when called, so
    // without the requires-clause std::copyable answered true for a move-only
    // Graph (any graph_owning_view) and the failure moved to the call site.
    constexpr breadth_first_search(const breadth_first_search & o)
        requires std::copy_constructible<Graph>
        : _graph(o._graph)
        , _queue(o._queue)
        , _reached_map(o._reached_map)
        , _pred_vertices_map(o._pred_vertices_map)
        , _pred_arcs_map(o._pred_arcs_map)
        , _dist_map(o._dist_map) {
        _rebase_cursors_from(o);
    }
    constexpr breadth_first_search(breadth_first_search &&) = default;

    constexpr breadth_first_search & operator=(const breadth_first_search & o)
        requires std::copyable<Graph>
    {
        if(this == std::addressof(o)) return *this;
        _graph = o._graph;
        _queue = o._queue;
        _reached_map = o._reached_map;
        _pred_vertices_map = o._pred_vertices_map;
        _pred_arcs_map = o._pred_arcs_map;
        _dist_map = o._dist_map;
        _rebase_cursors_from(o);
        return *this;
    }
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

private:
    // _queue is already a copy of o._queue here; only the cursors are left.
    constexpr void _rebase_cursors_from(const breadth_first_search & o) {
        if constexpr(has_num_vertices<Graph>) {
            _queue.reserve(num_vertices(_graph));
            _queue_current =
                _queue.begin() + (o._queue_current - o._queue.begin());
            if constexpr(Traits::store_traversal_range)
                _queue_traversal_begin =
                    _queue.begin() +
                    (o._queue_traversal_begin - o._queue.begin());
        } else {
            _queue_current = o._queue_current;
            if constexpr(Traits::store_traversal_range)
                _queue_traversal_begin = o._queue_traversal_begin;
        }
    }

public:
    // The graph the traversal was built over. Lets a composing algorithm keep a
    // single copy of the graph view instead of storing its own alongside --
    // see traversal_forest, where the duplicate made an owned graph impossible
    // to use at all.
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

    constexpr breadth_first_search & reset() {
        _queue.resize(0);
        if constexpr(has_num_vertices<Graph>) {
            _queue_current = _queue.begin();
        } else {
            _queue_current = 0;
        }
        _reached_map.fill(false);
        return *this;
    }
    constexpr breadth_first_search & add_source(const vertex & s) {
        assert(!_reached_map[s]);
        _queue.push_back(s);
        _reached_map[s] = true;
        if constexpr(Traits::store_pred_vertices) _pred_vertices_map[s] = s;
        if constexpr(Traits::store_distances) _dist_map[s] = 0;
        if constexpr(Traits::store_traversal_range)
            _queue_traversal_begin = _queue_current;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const noexcept {
        if constexpr(has_num_vertices<Graph>) {
            return _queue_current == _queue.end();
        } else {
            return _queue_current == _queue.size();
        }
    }

private:
    // What advance() walks with. Kept separate from current() so that making
    // the public accessor return by value costs the hot loop nothing.
    [[nodiscard]] constexpr const vertex & _current_ref() const noexcept {
        if constexpr(has_num_vertices<Graph>) {
            return *_queue_current;
        } else {
            return _queue[_queue_current];
        }
    }

public:
    // By value, like depth_first_search::current() and
    // topological_sort::current(). A reference into _queue -- which the next
    // advance() writes into -- was the odd one out, and callers reaching it
    // through the range interface got a copy anyway.
    [[nodiscard]] constexpr vertex current() const noexcept {
        assert(!finished());
        return _current_ref();
    }
    constexpr void advance() {
        assert(!finished());
        // By reference only where _queue is reserved to num_vertices and so
        // cannot reallocate. Without has_num_vertices there is no reserve, and
        // the push_backs below would leave this reference -- read on every
        // iteration through _pred_vertices_map / _dist_map -- dangling.
        std::conditional_t<has_num_vertices<Graph>, const vertex &, vertex> u =
            _current_ref();
        ++_queue_current;
        if constexpr(Traits::store_pred_arcs) {
            for(auto && a : out_arcs(_graph, u)) {
                const vertex & w = arc_target(_graph, a);
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                _pred_arcs_map[w] = a;
                if constexpr(Traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(Traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        } else {
            for(auto && w : out_neighbors(_graph, u)) {
                if(_reached_map[w]) continue;
                _queue.push_back(w);
                _reached_map[w] = true;
                if constexpr(Traits::store_pred_vertices)
                    _pred_vertices_map[w] = u;
                if constexpr(Traits::store_distances)
                    _dist_map[w] = _dist_map[u] + 1;
            }
        }
    }

    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_reached_map[u])) {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr auto reached_map() const
        noexcept(noexcept(maps::mapping_all(_reached_map))) {
        return maps::mapping_all(_reached_map);
    }
    [[nodiscard]] constexpr vertex pred_vertex(const vertex & u) const
        noexcept(noexcept(_pred_vertices_map[u]))
        requires(Traits::store_pred_vertices)
    {
        assert(reached(u));
        return _pred_vertices_map[u];
    }
    [[nodiscard]] constexpr arc pred_arc(const vertex & u) const
        noexcept(noexcept(_pred_arcs_map[u]))
        requires(Traits::store_pred_arcs)
    {
        assert(reached(u));
        return _pred_arcs_map[u];
    }
    [[nodiscard]] constexpr int dist(const vertex & u) const
        noexcept(noexcept(_dist_map[u]))
        requires(Traits::store_distances)
    {
        assert(reached(u));
        return _dist_map[u];
    }
    // std::span<const vertex> in both specialisations. This one used to return
    // a subrange of _queue's own iterators and the branchless one a span, so
    // the same member of the same class template had two return types and the
    // window was writable through one of them -- the read-only rule
    // strongly_connected_components::current() and
    // connected_components::current() already follow. _queue is contiguous, so
    // a span spells both cursor shapes.
    [[nodiscard]] constexpr std::span<const vertex> traversal() const noexcept
        requires(Traits::store_traversal_range)
    {
        if constexpr(has_num_vertices<Graph>) {
            return std::span<const vertex>(
                std::to_address(_queue_traversal_begin),
                static_cast<std::size_t>(_queue_current -
                                         _queue_traversal_begin));
        } else {
            return std::span<const vertex>(
                _queue.data() + _queue_traversal_begin,
                static_cast<std::size_t>(_queue_current -
                                         _queue_traversal_begin));
        }
    }
};

template <graph_view Graph, breadth_first_search_traits Traits>
    requires outward_adjacency_graph<Graph> && has_vertex_map<Graph> &&
             detail::enable_branchless_bfs<Graph, Traits>
class breadth_first_search<Graph, Traits>
    : public algorithm_view_interface<breadth_first_search<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;

    Graph _graph;
    std::unique_ptr<vertex[]> _queue;
    vertex * _queue_traversal_begin;
    vertex * _queue_traversal_end;
    vertex * _queue_current;
    vertex_map_t<Graph, bool> _reached_map;

public:
    // See the generic specialisation: graph_storable_as keeps
    // std::is_constructible honest.
    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                     graph_storable_as<G, Graph>
    constexpr explicit breadth_first_search(G && g)
        : _graph(detail::store_graph<Graph>(std::forward<G>(g)))
        , _queue(std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) +
                                                          1))
        , _queue_traversal_begin(_queue.get())
        , _queue_traversal_end(_queue.get())
        , _queue_current(_queue.get())
        , _reached_map(create_vertex_map<bool>(_graph, false)) {}

    template <typename G>
        requires detail::not_self<G, breadth_first_search> &&
                 graph_storable_as<G, Graph>
    constexpr breadth_first_search(G && g, const vertex & s)
        : breadth_first_search(std::forward<G>(g)) {
        add_source(s);
    }

    template <typename... Args>
        requires std::constructible_from<breadth_first_search, Args...>
    constexpr breadth_first_search(Traits, Args &&... args)
        : breadth_first_search(std::forward<Args>(args)...) {}

    // See the generic specialisation for the requires-clause: without it,
    // std::copyable lied for a move-only Graph.
    constexpr breadth_first_search(const breadth_first_search & o)
        requires std::copy_constructible<Graph>
        : _graph(o._graph)
        , _queue(std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) +
                                                          1ul))
        , _queue_traversal_begin(_queue.get() +
                                 (o._queue_traversal_begin - o._queue.get()))
        , _queue_traversal_end(_queue.get() +
                               (o._queue_traversal_end - o._queue.get()))
        , _queue_current(_queue.get() + (o._queue_current - o._queue.get()))
        , _reached_map(o._reached_map) {
        std::copy(o._queue_traversal_begin, o._queue_traversal_end,
                  _queue_traversal_begin);
    }
    constexpr breadth_first_search(breadth_first_search &&) = default;

    // Self-assignment has to bail out before the reallocation: the new buffer
    // replaces the one std::copy would then read from. And the return was
    // missing entirely, which made this specialisation -- the one selected for
    // static_digraph with the default traits -- not copy-assignable at all.
    constexpr breadth_first_search & operator=(const breadth_first_search & o)
        requires std::copyable<Graph>
    {
        if(this == std::addressof(o)) return *this;
        _graph = o._graph;
        _queue =
            std::make_unique_for_overwrite<vertex[]>(num_vertices(_graph) + 1);
        _queue_traversal_begin =
            _queue.get() + (o._queue_traversal_begin - o._queue.get());
        _queue_current = _queue.get() + (o._queue_current - o._queue.get());
        _queue_traversal_end =
            _queue.get() + (o._queue_traversal_end - o._queue.get());
        _reached_map = o._reached_map;
        std::copy(o._queue_traversal_begin, o._queue_traversal_end,
                  _queue_traversal_begin);
        return *this;
    }
    constexpr breadth_first_search & operator=(breadth_first_search &&) =
        default;

    // The graph the traversal was built over. Lets a composing algorithm keep a
    // single copy of the graph view instead of storing its own alongside --
    // see traversal_forest, where the duplicate made an owned graph impossible
    // to use at all.
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

    constexpr breadth_first_search & reset() {
        _queue_traversal_begin = _queue_current = _queue_traversal_end =
            _queue.get();
        _reached_map.fill(false);
        return *this;
    }
    constexpr breadth_first_search & add_source(const vertex & s) {
        assert(!_reached_map[s]);
        _queue_traversal_begin = _queue_current;
        *_queue_traversal_end = s;
        ++_queue_traversal_end;
        _reached_map[s] = true;
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_queue_current == _queue_traversal_end)) {
        return _queue_current == _queue_traversal_end;
    }

    // By value, like the generic specialisation above and every other
    // algorithm's current().
    [[nodiscard]] constexpr vertex current() const
        noexcept(noexcept(*_queue_current)) {
        assert(!finished());
        return *_queue_current;
    }
    constexpr void advance() {
        assert(!finished());
        // Straight off the buffer, which never reallocates here: current()
        // returns a copy now, and this is the hot loop.
        const vertex & u = *_queue_current;
        ++_queue_current;
        for(auto && w : out_neighbors(_graph, u)) {
            *_queue_traversal_end = w;
            _queue_traversal_end += !_reached_map[w];
            _reached_map[w] = true;
        }
    }
    [[nodiscard]] constexpr bool reached(const vertex & u) const
        noexcept(noexcept(_reached_map[u])) {
        return _reached_map[u];
    }
    [[nodiscard]] constexpr auto reached_map() const
        noexcept(noexcept(maps::mapping_all(_reached_map))) {
        return maps::mapping_all(_reached_map);
    }
    // Guarded on store_traversal_range like the generic specialisation's. It
    // used to be unconditional here, so the flag was silently ignored by
    // whichever specialisation a graph happened to select: the same class
    // template offered a different member set for the same Traits.
    [[nodiscard]] constexpr std::span<const vertex> traversal() const noexcept
        requires(Traits::store_traversal_range)
    {
        return std::span<const vertex>(
            _queue_traversal_begin,
            static_cast<std::size_t>(_queue_current - _queue_traversal_begin));
    }
};

template <typename Graph, typename Traits = breadth_first_search_default_traits>
breadth_first_search(Graph &&)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits = breadth_first_search_default_traits>
breadth_first_search(Graph &&, const vertex_t<Graph> &)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
breadth_first_search(Traits, Graph &&)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
breadth_first_search(Traits, Graph &&, const vertex_t<Graph> &)
    -> breadth_first_search<views::graph_all_t<Graph>, Traits>;

}  // namespace melon
