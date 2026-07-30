#pragma once

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <stack>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "melon/detail/borrowed_graph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/utility/algorithmic_generator.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <graph_view Graph>
    requires outward_adjacency_graph<Graph> && has_vertex_map<Graph>
class strongly_connected_components
    : public algorithm_view_interface<strongly_connected_components<Graph>> {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;

    using component_num = unsigned int;

    static constexpr component_num INVALID_COMPONENT =
        std::numeric_limits<component_num>::max();

private:
    Graph _graph;
    consumable_input_view<vertices_range_t<Graph>> _remaining_vertices;
    std::vector<
        std::pair<vertex, consumable_input_view<out_neighbors_range_t<Graph>>>>
        _dfs_stack;
    std::vector<vertex> _tarjan_stack;
    std::vector<vertex>::iterator _component_begin;
    component_num _start_index;
    component_num _index;
    vertex_map_t<Graph, bool> _in_tarjan_stack_map;
    vertex_map_t<Graph, component_num> _index_map;
    vertex_map_t<Graph, component_num> _lowlink_map;

public:
    // graph_storable_as, so std::is_constructible answers what the
    // mem-initializer actually does -- see dijkstra's constructor.
    template <typename T>
        requires detail::not_self<T, strongly_connected_components> &&
                     graph_storable_as<T, Graph>
    constexpr explicit strongly_connected_components(T && g)
        : _graph(detail::store_graph<Graph>(std::forward<T>(g)))
        , _remaining_vertices(vertices(_graph))
        , _dfs_stack()
        , _tarjan_stack()
        , _component_begin()
        , _start_index(0)
        , _index(0)
        , _in_tarjan_stack_map(create_vertex_map<bool>(_graph, false))
        , _index_map(
              create_vertex_map<component_num>(_graph, INVALID_COMPONENT))
        , _lowlink_map(
              create_vertex_map<component_num>(_graph, INVALID_COMPONENT)) {
        if constexpr(has_num_vertices<Graph>) {
            _dfs_stack.reserve(num_vertices(_graph));
            _tarjan_stack.reserve(num_vertices(_graph));
        }
        // Guarded: advance() reads _remaining_vertices.current(), so on a graph
        // with no vertices the unconditional call read past the end of an empty
        // view (an assertion failure in a debug build, silent UB in a release
        // one). A vertex-less graph is finished() from the start.
        if(!finished()) advance();
    }

    // _component_begin is an iterator *into* _tarjan_stack, and current()
    // hands out the range from it to the stack's end. A memberwise copy leaves
    // it pointing into the source's buffer, so the copy's very first current()
    // reads someone else's memory; the reserve() the traversal relies on is
    // lost too, since a copied vector only has capacity for what it holds.
    // Same shape as branchless breadth_first_search. Move is fine: the
    // vector's buffer transfers with it.
    //
    // The relocation policy is depth_first_search's: the cached cursors are
    // re-askable -- _remaining_vertices from vertices(_graph), each _dfs_stack
    // frame from the vertex sitting next to it -- so after the members
    // relocate, _rebase_cursors() aims them at the *new* _graph and the
    // _consumed counters put them back where they were. Copy therefore no
    // longer requires borrowed_graph: rebasing is what makes an SCC over an
    // owned subgraph honestly copyable. The one combination that cannot
    // rebase -- a non-borrowed graph handing out std-borrowed ranges, which
    // have no counter -- keeps copy constrained away.
    constexpr strongly_connected_components(
        const strongly_connected_components & o)
        requires std::copy_constructible<Graph> &&
                     std::copy_constructible<
                         consumable_input_view<vertices_range_t<Graph>>> &&
                     std::copy_constructible<
                         consumable_input_view<out_neighbors_range_t<Graph>>> &&
                     (borrowed_graph<Graph> ||
                      (!std::ranges::borrowed_range<vertices_range_t<Graph>> &&
                       !std::ranges::borrowed_range<
                           out_neighbors_range_t<Graph>>))
        : _graph(o._graph)
        , _remaining_vertices(o._remaining_vertices)
        , _dfs_stack(o._dfs_stack)
        , _tarjan_stack(o._tarjan_stack)
        , _start_index(o._start_index)
        , _index(o._index)
        , _in_tarjan_stack_map(o._in_tarjan_stack_map)
        , _index_map(o._index_map)
        , _lowlink_map(o._lowlink_map) {
        if constexpr(has_num_vertices<Graph>) {
            _dfs_stack.reserve(num_vertices(_graph));
            _tarjan_stack.reserve(num_vertices(_graph));
        }
        _component_begin = _tarjan_stack.begin() +
                           (o._component_begin - o._tarjan_stack.begin());
        _rebase_cursors();
    }
    // Hand-written for the same reason as depth_first_search's move: a
    // memberwise move leaves the cached cursors' ranges pointing at the
    // moved-from object's _graph member. _component_begin needs nothing --
    // the vector's buffer transfers with it.
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
        , _start_index(o._start_index)
        , _index(o._index)
        , _in_tarjan_stack_map(std::move(o._in_tarjan_stack_map))
        , _index_map(std::move(o._index_map))
        , _lowlink_map(std::move(o._lowlink_map)) {
        _rebase_cursors();
    }

    constexpr strongly_connected_components & operator=(
        const strongly_connected_components & o)
        requires std::copyable<Graph> &&
                 std::copyable<
                     consumable_input_view<vertices_range_t<Graph>>> &&
                 std::copyable<
                     consumable_input_view<out_neighbors_range_t<Graph>>> &&
                 (borrowed_graph<Graph> ||
                  (!std::ranges::borrowed_range<vertices_range_t<Graph>> &&
                   !std::ranges::borrowed_range<out_neighbors_range_t<Graph>>))
    {
        if(this == std::addressof(o)) return *this;
        const auto offset = o._component_begin - o._tarjan_stack.begin();
        _graph = o._graph;
        _remaining_vertices = o._remaining_vertices;
        _dfs_stack = o._dfs_stack;
        _tarjan_stack = o._tarjan_stack;
        if constexpr(has_num_vertices<Graph>) {
            _dfs_stack.reserve(num_vertices(_graph));
            _tarjan_stack.reserve(num_vertices(_graph));
        }
        _component_begin = _tarjan_stack.begin() + offset;
        _start_index = o._start_index;
        _index = o._index;
        _in_tarjan_stack_map = o._in_tarjan_stack_map;
        _index_map = o._index_map;
        _lowlink_map = o._lowlink_map;
        _rebase_cursors();
        return *this;
    }
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
        _start_index = o._start_index;
        _index = o._index;
        _in_tarjan_stack_map = std::move(o._in_tarjan_stack_map);
        _index_map = std::move(o._index_map);
        _lowlink_map = std::move(o._lowlink_map);
        _rebase_cursors();
        return *this;
    }

private:
    // See depth_first_search::_rebase_stack: re-asks the *new* _graph for
    // each cached cursor's range; the _consumed counters restore positions.
    // Compiles away when nothing points back at the graph object.
    constexpr void _rebase_cursors() {
        if constexpr(!borrowed_graph<Graph>) {
            if constexpr(!std::ranges::borrowed_range<vertices_range_t<Graph>>)
                _remaining_vertices.rebase(vertices(_graph));
            if constexpr(!std::ranges::borrowed_range<
                             out_neighbors_range_t<Graph>>)
                for(auto & [v, cursor] : _dfs_stack)
                    cursor.rebase(out_neighbors(_graph, v));
        }
    }

public:
    // The graph the algorithm runs over. An algorithm owns its view rather
    // than adapting it, so this is the std::ranges::owning_view shape --
    // references, ref-qualified -- and not the filter_view shape the graph
    // *views* use. Returning a copy here would also put traversal_forest back
    // where it started: it reaches its sources through base(), and an owned
    // graph view is move-only.
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

    // Must restore exactly the state the constructor leaves behind, first
    // component included. Two things were missing: _index_map, which is what
    // reached() consults -- leaving it filled made every vertex look already
    // visited, so the restarted run walked _remaining_vertices to the end and
    // yielded nothing -- and the advance() that produces the first component.
    constexpr strongly_connected_components & reset() {
        _start_index = 0;
        _index = 0;
        _remaining_vertices = vertices(_graph);
        _dfs_stack.clear();
        _tarjan_stack.resize(0);
        _component_begin = _tarjan_stack.begin();
        _in_tarjan_stack_map.fill(false);
        _index_map.fill(INVALID_COMPONENT);
        _lowlink_map.fill(INVALID_COMPONENT);
        if(!finished()) advance();
        return *this;
    }

    [[nodiscard]] constexpr bool finished() const
        noexcept(noexcept(_remaining_vertices.empty())) {
        return _remaining_vertices.empty();
    }

    // The component is a window onto _tarjan_stack, which the next advance()
    // erases from _component_begin onwards -- hand it out read-only, as every
    // other algorithm's current() does. std::span needs one pointer type, and
    // _component_begin is a non-const iterator while a const _tarjan_stack
    // yields const ones, so the element type is spelled out.
    [[nodiscard]] constexpr auto current() const noexcept(noexcept(
        std::span<const vertex>(std::to_address(_component_begin),
                                static_cast<std::size_t>(_tarjan_stack.end() -
                                                         _component_begin)))) {
        assert(!finished());
        return std::span<const vertex>(
            std::to_address(_component_begin),
            static_cast<std::size_t>(_tarjan_stack.end() - _component_begin));
    }

    [[nodiscard]] constexpr bool reached(const vertex & v) const
        noexcept(noexcept(_index_map[v] != INVALID_COMPONENT)) {
        return _index_map[v] != INVALID_COMPONENT;
    }
    // A view of the reached state, like breadth_first_search's and
    // depth_first_search's. It refers into the algorithm, as every melon map
    // view refers into what it names: it is valid while this object lives and
    // stays put, exactly the contract mapping_ref_view carries.
    // See dijkstra::reached_map: computed, not stored.
    [[nodiscard]] constexpr auto reached_map() const {
        return maps::map([this](const vertex & v) { return reached(v); });
    }

private:
    // Tarjan's stack push: an implementation detail of advance(), not
    // something a caller may do behind the traversal's back.
    constexpr void _push_tarjan(const vertex & v) {
        _tarjan_stack.push_back(v);
        _in_tarjan_stack_map[v] = true;
    }

public:
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
            _index_map[s] = _lowlink_map[s] = _start_index = _index;
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
                // _component_begin =
                //     --std::ranges::find(std::views::reverse(_tarjan_stack),
                //                         _dfs_stack.back().first)
                //           .base();

                for(_component_begin = _tarjan_stack.end() - 1;
                    *_component_begin != v; --_component_begin) {
                    _in_tarjan_stack_map[*_component_begin] = false;
                }
                _in_tarjan_stack_map[v] = false;
                return;
            }
            _dfs_stack.pop_back();
            const vertex & parent = _dfs_stack.back().first;
            _lowlink_map[parent] =
                std::min(_lowlink_map[parent], _lowlink_map[v]);
        }
    }

    [[nodiscard]] constexpr bool same_component(const vertex & u,
                                                const vertex & v) const
        noexcept(noexcept(_lowlink_map[u] == _lowlink_map[v])) {
        return _lowlink_map[u] == _lowlink_map[v];
    }
};

template <typename Graph>
strongly_connected_components(Graph &&)
    -> strongly_connected_components<views::graph_all_t<Graph>>;

}  // namespace melon
