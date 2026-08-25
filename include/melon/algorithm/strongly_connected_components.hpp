#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/borrowed_graph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <typename Traits>
concept strongly_connected_components_traits = requires {
    { Traits::store_component_ids } -> std::convertible_to<bool>;
};

struct strongly_connected_components_default_traits {
    static constexpr bool store_component_ids = false;
};

template <graph_view Graph, strongly_connected_components_traits Traits =
                                strongly_connected_components_default_traits>
    requires outward_adjacency_graph<Graph>
class strongly_connected_components
    : public algorithm_view_interface<
          strongly_connected_components<Graph, Traits>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using component_num = unsigned int;

    static constexpr component_num INVALID_COMPONENT =
        std::numeric_limits<component_num>::max();

private:
    Graph _graph;
    detail::consumable_input_view<vertices_range_t<Graph>> _remaining_vertices;
    std::vector<std::pair<
        vertex, detail::consumable_input_view<out_neighbors_range_t<Graph>>>>
        _dfs_stack;
    std::vector<vertex> _tarjan_stack;
    std::vector<vertex>::iterator _component_begin;
    component_num _index;
    component_num _num_components;
    vertex_map_t<Graph, bool> _in_tarjan_stack_map;
    vertex_map_t<Graph, component_num> _index_map;
    vertex_map_t<Graph, component_num> _lowlink_map;
    [[no_unique_address]] detail::vertex_map_if<
        Traits::store_component_ids, Graph, component_num> _component_id_map;

public:
    // ---- Construction -------------------------------------------------------

    template <typename G>
        requires detail::not_self<G, strongly_connected_components> &&
                     graph_for<G, Graph> && has_vertex_map<Graph>
    constexpr explicit strongly_connected_components(G && g)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _remaining_vertices(vertices(_graph))
        , _dfs_stack()
        , _tarjan_stack()
        , _component_begin()
        , _index(0)
        , _num_components(0)
        , _in_tarjan_stack_map(create_vertex_map<bool>(_graph, false))
        , _index_map(
              create_vertex_map<component_num>(_graph, INVALID_COMPONENT))
        , _lowlink_map(
              create_vertex_map<component_num>(_graph, INVALID_COMPONENT))
        , _component_id_map(_graph, INVALID_COMPONENT) {
        if constexpr(has_num_vertices<Graph>) {
            _dfs_stack.reserve(num_vertices(_graph));
            _tarjan_stack.reserve(num_vertices(_graph));
        }
        // Guarded: advance() reads _remaining_vertices.current(), which on a
        // vertex-less graph reads past the end of an empty view. Such a graph
        // is finished() from the start.
        if(!finished()) advance();
    }

    template <typename... Args>
        requires std::constructible_from<strongly_connected_components, Args...>
    constexpr strongly_connected_components(Traits, Args &&... args)
        : strongly_connected_components(std::forward<Args>(args)...) {}

    // The relocation policy: the cached cursors are re-askable --
    // _remaining_vertices from vertices(_graph), each _dfs_stack frame from the
    // vertex sitting next to it -- so after the members relocate,
    // _rebase_cursors() aims them at the *new* _graph and the _consumed
    // counters put them back where they were.
    //
    // Move-only; see the melon::traversal_algorithm concept for the ruling. The
    // move cannot be defaulted: a memberwise move leaves the cached cursors'
    // ranges pointing at the moved-from object's _graph member.
    // _component_begin needs nothing -- it is an iterator into _tarjan_stack,
    // whose buffer transfers with it.
    constexpr strongly_connected_components(
        const strongly_connected_components &) = delete;
    constexpr strongly_connected_components(strongly_connected_components && o)
        : _graph(std::move(o._graph))
        // Not a memberwise move: the cursor's move constructor reseeks
        // through the *old* range, whose predicate reads the graph member
        // moved away one line up. Built from the new graph's range and the
        // old consumed count instead. Container-held cursors (_dfs_stack) are
        // immune -- a vector move steals the buffer without touching them.
        , _remaining_vertices([&]() -> decltype(_remaining_vertices) {
            if constexpr(!borrowed_graph<Graph> &&
                         !std::ranges::borrowed_range<vertices_range_t<Graph>>)
                return decltype(_remaining_vertices)(
                    vertices(_graph), o._remaining_vertices.consumed());
            else
                return std::move(o._remaining_vertices);
        }())
        , _dfs_stack(std::move(o._dfs_stack))
        , _tarjan_stack(std::move(o._tarjan_stack))
        , _component_begin(std::move(o._component_begin))
        , _index(o._index)
        , _num_components(o._num_components)
        , _in_tarjan_stack_map(std::move(o._in_tarjan_stack_map))
        , _index_map(std::move(o._index_map))
        , _lowlink_map(std::move(o._lowlink_map))
        , _component_id_map(std::move(o._component_id_map)) {
        _rebase_cursors();
    }

    constexpr strongly_connected_components & operator=(
        const strongly_connected_components &) = delete;
    // See the move constructor.
    constexpr strongly_connected_components & operator=(
        strongly_connected_components && o) {
        if(this == std::addressof(o)) return *this;
        _graph = std::move(o._graph);
        // See the move constructor: never reseek through the gutted source.
        if constexpr(!borrowed_graph<Graph> &&
                     !std::ranges::borrowed_range<vertices_range_t<Graph>>)
            _remaining_vertices = decltype(_remaining_vertices)(
                vertices(_graph), o._remaining_vertices.consumed());
        else
            _remaining_vertices = std::move(o._remaining_vertices);
        _dfs_stack = std::move(o._dfs_stack);
        _tarjan_stack = std::move(o._tarjan_stack);
        _component_begin = std::move(o._component_begin);
        _index = o._index;
        _num_components = o._num_components;
        _in_tarjan_stack_map = std::move(o._in_tarjan_stack_map);
        _index_map = std::move(o._index_map);
        _lowlink_map = std::move(o._lowlink_map);
        _component_id_map = std::move(o._component_id_map);
        _rebase_cursors();
        return *this;
    }

private:
    // Compiles away when nothing points back at the graph object.
    // _remaining_vertices is absent on purpose: both move members already
    // rebuilt it against the new graph through the counted constructor, so a
    // rebase here would fetch vertices(_graph) and reseek a second time.
    constexpr void _rebase_cursors() {
        if constexpr(!borrowed_graph<Graph>) {
            if constexpr(!std::ranges::borrowed_range<
                             out_neighbors_range_t<Graph>>)
                for(auto & [v, cursor] : _dfs_stack)
                    cursor.rebase(out_neighbors(_graph, v));
        }
    }

public:
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

    // Must restore exactly the state the constructor leaves behind, first
    // component included: _index_map is what reached() consults, so a restarted
    // run that keeps it filled walks _remaining_vertices to the end and yields
    // nothing, and it is the trailing advance() that produces the first
    // component.
    constexpr strongly_connected_components & reset() {
        _index = 0;
        _num_components = 0;
        _remaining_vertices = vertices(_graph);
        _dfs_stack.clear();
        _tarjan_stack.resize(0);
        _component_begin = _tarjan_stack.begin();
        _in_tarjan_stack_map.fill(false);
        _index_map.fill(INVALID_COMPONENT);
        _lowlink_map.fill(INVALID_COMPONENT);
        if constexpr(Traits::store_component_ids)
            _component_id_map.fill(INVALID_COMPONENT);
        if(!finished()) advance();
        return *this;
    }

    // ---- Execution ----------------------------------------------------------

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_remaining_vertices.empty())) {
        return _remaining_vertices.empty();
    }

    // The component is a window onto _tarjan_stack, which the next advance()
    // erases from _component_begin onwards -- hand it out read-only. std::span
    // needs one pointer type, and _component_begin is a non-const iterator
    // while a const _tarjan_stack yields const ones, so the element type is
    // spelled out.
    [[nodiscard]] constexpr auto current() const noexcept(noexcept(
        std::span<const vertex>(std::to_address(_component_begin),
                                static_cast<std::size_t>(_tarjan_stack.end() -
                                                         _component_begin)))) {
        assert(!finished());
        return std::span<const vertex>(
            std::to_address(_component_begin),
            static_cast<std::size_t>(_tarjan_stack.end() - _component_begin));
    }

    // ---- Queries ------------------------------------------------------------

    // The number of components yielded so far, current() included -- the
    // component count of the graph once finished(). Not gated on
    // store_component_ids: the counter is what numbers the ids, but the count
    // is meaningful without them. When ids are stored, every assigned
    // component_id() is below this.
    [[nodiscard]] constexpr component_num num_components() const noexcept {
        return _num_components;
    }

    [[nodiscard]] constexpr bool reached(const vertex & v) const
        noexcept(noexcept(_index_map[v] != INVALID_COMPONENT)) {
        return _index_map[v] != INVALID_COMPONENT;
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put, mapping_ref_view's contract.
    [[nodiscard]] constexpr auto reached_map() const & {
        return maps::map([this](const vertex & v) { return reached(v); });
    }
    // Terminal, like std::move(alg).base() -- no other member may be called
    // afterwards.
    [[nodiscard]] constexpr auto reached_map() && {
        return maps::map([index_map = std::move(_index_map)](const vertex & v) {
            return index_map[v] != INVALID_COMPONENT;
        });
    }

private:
    constexpr void _push_tarjan(const vertex & v) {
        _tarjan_stack.push_back(v);
        _in_tarjan_stack_map[v] = true;
    }

public:
    // ---- Execution ----------------------------------------------------------

    constexpr void advance() {
        assert(!_remaining_vertices.empty());

        if(!_dfs_stack.empty()) {
            const vertex v = _dfs_stack.back().first;
            _tarjan_stack.erase(_component_begin, _tarjan_stack.end());
            _dfs_stack.pop_back();
            if(!_dfs_stack.empty()) {
                const vertex parent = _dfs_stack.back().first;
                _lowlink_map[parent] =
                    std::min(_lowlink_map[parent], _lowlink_map[v]);
            }
        }

        if(_dfs_stack.empty()) {
            assert(!_remaining_vertices.empty());
            while(reached(_remaining_vertices.current())) {
                _remaining_vertices.advance();
                if(_remaining_vertices.empty()) return;
            }
            const vertex s = _remaining_vertices.current();
            _index_map[s] = _lowlink_map[s] = _index;
            ++_index;
            _push_tarjan(s);
            _dfs_stack.emplace_back(s, out_neighbors(_graph, s));
        }

        for(;;) {
            while(!_dfs_stack.back().second.empty()) {
                const vertex w = _dfs_stack.back().second.current();
                _dfs_stack.back().second.advance();
                if(reached(w)) {
                    if(_in_tarjan_stack_map[w]) {
                        const vertex & v = _dfs_stack.back().first;
                        _lowlink_map[v] =
                            std::min(_lowlink_map[v], _index_map[w]);
                    }
                    continue;
                }
                _index_map[w] = _lowlink_map[w] = _index;
                ++_index;
                _push_tarjan(w);
                _dfs_stack.emplace_back(w, out_neighbors(_graph, w));
            }
            const vertex v = _dfs_stack.back().first;
            if(_index_map[v] == _lowlink_map[v]) {
                for(_component_begin = _tarjan_stack.end() - 1;
                    *_component_begin != v; --_component_begin) {
                    _in_tarjan_stack_map[*_component_begin] = false;
                    if constexpr(Traits::store_component_ids)
                        _component_id_map[*_component_begin] = _num_components;
                }
                _in_tarjan_stack_map[v] = false;
                if constexpr(Traits::store_component_ids)
                    _component_id_map[v] = _num_components;
                ++_num_components;
                return;
            }
            _dfs_stack.pop_back();
            const vertex & parent = _dfs_stack.back().first;
            _lowlink_map[parent] =
                std::min(_lowlink_map[parent], _lowlink_map[v]);
        }
    }

    // ---- Queries ------------------------------------------------------------

    // The id of the component u belongs to: dense, in emission order -- the
    // k-th component yielded is id k, reverse topological order of the
    // condensation. Only assigned once the component is popped, so asking
    // before u's component has been yielded is a precondition violation. The
    // same-component query is component_id(u) == component_id(v); the lowlinks
    // cannot answer it, as they are not uniform within a finished component.
    [[nodiscard]] constexpr component_num component_id(const vertex & u) const
        requires(Traits::store_component_ids)
    {
        assert(_component_id_map[u] != INVALID_COMPONENT);
        return _component_id_map[u];
    }
    // A view of the stored ids, reached_map()'s contract: valid while this
    // object lives and stays put. Unlike component_id() it cannot assert per
    // read, so vertices whose component has not been yielded still hold the
    // sentinel -- read it once the components of interest are out.
    [[nodiscard]] constexpr auto component_ids_map() const & noexcept(
        noexcept(maps::mapping_all(_component_id_map._map)))
        requires(Traits::store_component_ids)
    {
        return maps::mapping_all(_component_id_map._map);
    }
    // Terminal, like std::move(alg).base() -- the member left behind is valid
    // but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto component_ids_map() && noexcept(
        noexcept(maps::mapping_all(std::move(_component_id_map._map))))
        requires(Traits::store_component_ids)
    {
        return maps::mapping_all(std::move(_component_id_map._map));
    }
};

template <typename Graph,
          typename Traits = strongly_connected_components_default_traits>
strongly_connected_components(Graph &&)
    -> strongly_connected_components<views::graph_all_t<Graph>, Traits>;

template <typename Graph, typename Traits>
strongly_connected_components(Traits, Graph &&)
    -> strongly_connected_components<views::graph_all_t<Graph>, Traits>;

}  // namespace melon
