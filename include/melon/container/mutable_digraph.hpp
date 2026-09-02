#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

#include "melon/container/static_map.hpp"
#include "melon/detail/intrusive_iterator_base.hpp"
#include "melon/mapping.hpp"

namespace melon {

class mutable_digraph {
private:
    using vertex = unsigned int;
    using arc = unsigned int;

    static constexpr vertex INVALID_VERTEX = std::numeric_limits<vertex>::max();
    static constexpr arc INVALID_ARC = std::numeric_limits<arc>::max();
    struct vertex_struct {
        arc first_in_arc;
        arc first_out_arc;
        vertex prev_vertex;
        vertex next_vertex;
    };
    struct arc_struct {
        vertex source;
        vertex target;
        arc prev_in_arc;
        arc next_in_arc;
        arc prev_out_arc;
        arc next_out_arc;
    };
    std::vector<vertex_struct> _vertices;
    std::vector<arc_struct> _arcs;
    std::vector<bool> _vertices_filter;
    std::vector<bool> _arcs_filter;
    vertex _first_vertex;
    vertex _first_free_vertex;
    arc _first_free_arc;
    std::size_t _num_vertices;
    std::size_t _num_arcs;

    class vertices_iterator
        : public detail::intrusive_iterator_base<mutable_digraph, vertex> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a friend comparing
        // against INVALID_VERTEX directly fails to compile there.
        static constexpr vertex _invalid_cursor = INVALID_VERTEX;

    public:
        using detail::intrusive_iterator_base<mutable_digraph,
                                              vertex>::intrusive_iterator_base;

        // At-end, not zero: std::ranges::subrange value-initializes its
        // stored iterator, and a zero cursor claims element 0 of a null
        // structure -- graph views stand in an empty incidence range with a
        // default-constructed one (unify_sources at its root).
        constexpr vertices_iterator() noexcept
            : vertices_iterator(nullptr, _invalid_cursor) {}

        constexpr vertices_iterator & operator++() noexcept {
            _cursor = _structure->_vertices[_cursor].next_vertex;
            return *this;
        }
        constexpr vertices_iterator operator++(int) noexcept {
            vertices_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const vertices_iterator & it, std::default_sentinel_t) noexcept {
            return it._cursor == _invalid_cursor;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const vertices_iterator & it1,
            const vertices_iterator & it2) noexcept {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

    class out_arcs_iterator
        : public detail::intrusive_iterator_base<mutable_digraph, arc> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a friend comparing
        // against INVALID_ARC directly fails to compile there.
        static constexpr arc _invalid_cursor = INVALID_ARC;

    public:
        using detail::intrusive_iterator_base<mutable_digraph,
                                              arc>::intrusive_iterator_base;

        // At-end, not zero -- see vertices_iterator.
        constexpr out_arcs_iterator() noexcept
            : out_arcs_iterator(nullptr, _invalid_cursor) {}

        constexpr out_arcs_iterator & operator++() noexcept {
            _cursor = _structure->_arcs[_cursor].next_out_arc;
            return *this;
        }
        constexpr out_arcs_iterator operator++(int) noexcept {
            out_arcs_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const out_arcs_iterator & it, std::default_sentinel_t) noexcept {
            return it._cursor == _invalid_cursor;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const out_arcs_iterator & it1,
            const out_arcs_iterator & it2) noexcept {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

    class in_arcs_iterator
        : public detail::intrusive_iterator_base<mutable_digraph, arc> {
        // MSVC (through at least VS 18.6) denies hidden friends of a nested
        // class the enclosing class's private access: a friend comparing
        // against INVALID_ARC directly fails to compile there.
        static constexpr arc _invalid_cursor = INVALID_ARC;

    public:
        using detail::intrusive_iterator_base<mutable_digraph,
                                              arc>::intrusive_iterator_base;

        // At-end, not zero -- see vertices_iterator.
        constexpr in_arcs_iterator() noexcept
            : in_arcs_iterator(nullptr, _invalid_cursor) {}

        constexpr in_arcs_iterator & operator++() noexcept {
            _cursor = _structure->_arcs[_cursor].next_in_arc;
            return *this;
        }
        constexpr in_arcs_iterator operator++(int) noexcept {
            in_arcs_iterator it(*this);
            operator++();
            return it;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const in_arcs_iterator & it, std::default_sentinel_t) noexcept {
            return it._cursor == _invalid_cursor;
        }
        [[nodiscard]] constexpr friend bool operator==(
            const in_arcs_iterator & it1,
            const in_arcs_iterator & it2) noexcept {
            assert(it1._structure == it2._structure);
            return it1._cursor == it2._cursor;
        }
    };

public:
    constexpr mutable_digraph() noexcept
        : _first_vertex(INVALID_VERTEX)
        , _first_free_vertex(INVALID_VERTEX)
        , _first_free_arc(INVALID_ARC)
        , _num_vertices(0)
        , _num_arcs(0) {};
    constexpr mutable_digraph(const mutable_digraph & graph) = default;
    // Hand-written moves: the vectors empty on move, but a defaulted
    // member-wise move keeps the counts and list heads, so a moved-from graph
    // claims vertices its vectors no longer hold. The scalars go back to the
    // default-constructed (empty) state instead.
    constexpr mutable_digraph(mutable_digraph && graph) noexcept
        : _vertices(std::move(graph._vertices))
        , _arcs(std::move(graph._arcs))
        , _vertices_filter(std::move(graph._vertices_filter))
        , _arcs_filter(std::move(graph._arcs_filter))
        , _first_vertex(std::exchange(graph._first_vertex, INVALID_VERTEX))
        , _first_free_vertex(
              std::exchange(graph._first_free_vertex, INVALID_VERTEX))
        , _first_free_arc(std::exchange(graph._first_free_arc, INVALID_ARC))
        , _num_vertices(std::exchange(graph._num_vertices, 0))
        , _num_arcs(std::exchange(graph._num_arcs, 0)) {}

    constexpr mutable_digraph & operator=(const mutable_digraph &) = default;
    constexpr mutable_digraph & operator=(mutable_digraph && graph) noexcept {
        _vertices = std::move(graph._vertices);
        _arcs = std::move(graph._arcs);
        _vertices_filter = std::move(graph._vertices_filter);
        _arcs_filter = std::move(graph._arcs_filter);
        _first_vertex = std::exchange(graph._first_vertex, INVALID_VERTEX);
        _first_free_vertex =
            std::exchange(graph._first_free_vertex, INVALID_VERTEX);
        _first_free_arc = std::exchange(graph._first_free_arc, INVALID_ARC);
        _num_vertices = std::exchange(graph._num_vertices, 0);
        _num_arcs = std::exchange(graph._num_arcs, 0);
        return *this;
    }

    [[nodiscard]] constexpr bool is_valid_vertex(
        const vertex v) const noexcept {
        if(v >= _vertices.size()) return false;
        return _vertices_filter[v];
    }
    [[nodiscard]] constexpr bool is_valid_arc(const arc a) const noexcept {
        if(a >= _arcs.size()) return false;
        return _arcs_filter[a];
    }
    [[nodiscard]] constexpr auto num_vertices() const noexcept {
        return _num_vertices;
    }
    [[nodiscard]] constexpr auto num_arcs() const noexcept { return _num_arcs; }

    [[nodiscard]] constexpr auto vertices() const noexcept {
        return std::ranges::subrange(vertices_iterator(this, _first_vertex),
                                     std::default_sentinel);
    }
    [[nodiscard]] constexpr vertex arc_source(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arcs[a].source;
    }
    // A lambda-backed mapping_owning_view rather than the mapping_ref_view
    // the two static digraphs return: the endpoints live inside arc_struct,
    // so there is no contiguous array of sources to hand a reference to.
    [[nodiscard]] constexpr auto arc_sources_map() const noexcept {
        return maps::function(
            [this](const arc a) -> vertex { return _arcs[a].source; });
    }
    [[nodiscard]] constexpr vertex arc_target(const arc a) const noexcept {
        assert(is_valid_arc(a));
        return _arcs[a].target;
    }
    [[nodiscard]] constexpr auto arc_targets_map() const noexcept {
        return maps::function(
            [this](const arc a) -> vertex { return _arcs[a].target; });
    }
    [[nodiscard]] constexpr auto out_arcs(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return std::ranges::subrange(
            out_arcs_iterator(this, _vertices[v].first_out_arc),
            std::default_sentinel);
    }
    [[nodiscard]] constexpr auto in_arcs(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return std::ranges::subrange(
            in_arcs_iterator(this, _vertices[v].first_in_arc),
            std::default_sentinel);
    }
    [[nodiscard]] constexpr auto out_neighbors(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return std::views::transform(
            out_arcs(v),
            [this](const arc & a) -> vertex { return _arcs[a].target; });
    }
    [[nodiscard]] constexpr auto in_neighbors(const vertex v) const noexcept {
        assert(is_valid_vertex(v));
        return std::views::transform(
            in_arcs(v),
            [this](const arc & a) -> vertex { return _arcs[a].source; });
    }

    [[nodiscard]] constexpr auto arcs() const noexcept {
        return std::views::join(std::views::transform(
            vertices(), [this](auto v) { return out_arcs(v); }));
    }
    [[nodiscard]] constexpr auto arcs_entries() const noexcept {
        return std::views::transform(arcs(), [this](const arc & a) {
            return std::make_pair(
                a, std::make_pair(_arcs[a].source, _arcs[a].target));
        });
    }

    // Neither create_vertex nor create_arc is noexcept: both may emplace_back
    // into _vertices / _arcs (and their filters), which reallocates.
    [[nodiscard]] constexpr vertex create_vertex() {
        vertex new_vertex;
        if(_first_free_vertex == INVALID_VERTEX) {
            new_vertex = static_cast<vertex>(_vertices.size());
            _vertices.emplace_back(INVALID_ARC, INVALID_ARC, INVALID_VERTEX,
                                   _first_vertex);
            _vertices_filter.emplace_back(true);
        } else {
            new_vertex = _first_free_vertex;
            _first_free_vertex = _vertices[_first_free_vertex].next_vertex;
            _vertices[new_vertex] = {INVALID_ARC, INVALID_ARC, INVALID_VERTEX,
                                     _first_vertex};
            _vertices_filter[new_vertex] = true;
        }
        if(_first_vertex != INVALID_VERTEX) {
            _vertices[_first_vertex].prev_vertex = new_vertex;
        }
        _first_vertex = new_vertex;
        ++_num_vertices;
        return new_vertex;
    }

    [[nodiscard]] constexpr arc create_arc(const vertex from, const vertex to) {
        assert(is_valid_vertex(from));
        assert(is_valid_vertex(to));
        arc new_arc;
        vertex_struct & tos = _vertices[to];
        vertex_struct & froms = _vertices[from];
        if(_first_free_arc == INVALID_ARC) {
            new_arc = static_cast<arc>(_arcs.size());
            _arcs.emplace_back(from, to, INVALID_ARC, tos.first_in_arc,
                               INVALID_ARC, froms.first_out_arc);
            _arcs_filter.emplace_back(true);
        } else {
            new_arc = _first_free_arc;
            _first_free_arc = _arcs[_first_free_arc].next_in_arc;
            _arcs[new_arc] = {from,        to,
                              INVALID_ARC, tos.first_in_arc,
                              INVALID_ARC, froms.first_out_arc};
            _arcs_filter[new_arc] = true;
        }
        if(tos.first_in_arc != INVALID_ARC)
            _arcs[tos.first_in_arc].prev_in_arc = new_arc;
        tos.first_in_arc = new_arc;
        if(froms.first_out_arc != INVALID_ARC)
            _arcs[froms.first_out_arc].prev_out_arc = new_arc;
        froms.first_out_arc = new_arc;
        ++_num_arcs;
        return new_arc;
    }

private:
    constexpr void remove_from_source_out_arcs(const arc a) noexcept {
        assert(is_valid_arc(a));
        const arc_struct & as = _arcs[a];
        if(as.next_out_arc != INVALID_ARC)
            _arcs[as.next_out_arc].prev_out_arc = as.prev_out_arc;
        if(as.prev_out_arc != INVALID_ARC)
            _arcs[as.prev_out_arc].next_out_arc = as.next_out_arc;
        else
            _vertices[as.source].first_out_arc = as.next_out_arc;
    }
    constexpr void remove_from_target_in_arcs(const arc a) noexcept {
        assert(is_valid_arc(a));
        const arc_struct & as = _arcs[a];
        if(as.next_in_arc != INVALID_ARC)
            _arcs[as.next_in_arc].prev_in_arc = as.prev_in_arc;
        if(as.prev_in_arc != INVALID_ARC)
            _arcs[as.prev_in_arc].next_in_arc = as.next_in_arc;
        else
            _vertices[as.target].first_in_arc = as.next_in_arc;
    }
    constexpr void remove_incident_arcs(const vertex v) noexcept {
        assert(is_valid_vertex(v));
        const arc first_in_arc = _vertices[v].first_in_arc;
        arc last_in_arc = INVALID_ARC;
        for(const arc & a : in_arcs(v)) {
            last_in_arc = a;
            remove_from_source_out_arcs(a);
            _arcs_filter[a] = false;
            --_num_arcs;
        }
        // Read after the loop above, not beside first_in_arc: a self-loop sits
        // in both incidence lists, and the loop just unlinked it from v's
        // out-list -- possibly the very arc first_out_arc named. Hoisting the
        // two reads together publishes an arc the out-loop never visits, whose
        // next_in_arc was never rewritten, so the free list runs into live
        // arcs. It is also what keeps the two chains disjoint, so no arc is
        // freed twice.
        const arc first_out_arc = _vertices[v].first_out_arc;
        arc last_out_arc = INVALID_ARC;
        for(const arc & a : out_arcs(v)) {
            last_out_arc = a;
            remove_from_target_in_arcs(a);
            // The free list below links through next_in_arc, and this arc's
            // next_in_arc still points into its target's in-list: spliced
            // un-rewritten, the free list threads through live arcs. The
            // in-chain above needs no rewrite -- it is already linked by
            // next_in_arc.
            _arcs[a].next_in_arc = _arcs[a].next_out_arc;
            _arcs_filter[a] = false;
            --_num_arcs;
        }
        // The new free-list head is the chain's *first* arc, not the tail the
        // loop above happens to be holding. Publishing the tail recycles one
        // arc per chain and strands the rest: nothing reads them again, so
        // _arcs grows without bound under churn, and so does every
        // create_arc_map, which sizes on _arcs.size() rather than num_arcs().
        if(last_in_arc != INVALID_ARC) {
            _arcs[last_in_arc].next_in_arc = _first_free_arc;
            _first_free_arc = first_in_arc;
        }
        if(last_out_arc != INVALID_ARC) {
            _arcs[last_out_arc].next_in_arc = _first_free_arc;
            _first_free_arc = first_out_arc;
        }
    }

public:
    // Both removals push the freed id onto a free list, and create_vertex /
    // create_arc pop from it: a handle held across a removal can silently
    // come back valid while denoting a different vertex or arc. is_valid_*
    // cannot see the difference -- it only tests the filter bit.
    constexpr void remove_vertex(const vertex v) noexcept {
        assert(is_valid_vertex(v));
        remove_incident_arcs(v);
        vertex_struct & vs = _vertices[v];
        if(vs.next_vertex != INVALID_VERTEX) {
            _vertices[vs.next_vertex].prev_vertex = vs.prev_vertex;
        }
        if(vs.prev_vertex != INVALID_VERTEX) {
            _vertices[vs.prev_vertex].next_vertex = vs.next_vertex;
        } else {
            _first_vertex = vs.next_vertex;
        }
        vs.next_vertex = _first_free_vertex;
        _first_free_vertex = v;
        _vertices_filter[v] = false;
        --_num_vertices;
    }
    constexpr void remove_arc(const arc a) noexcept {
        assert(is_valid_arc(a));
        remove_from_source_out_arcs(a);
        remove_from_target_in_arcs(a);
        _arcs[a].next_in_arc = _first_free_arc;
        _first_free_arc = a;
        _arcs_filter[a] = false;
        --_num_arcs;
    }
    // The std container shape: removes every vertex and arc but keeps the
    // allocated buffers, like std::vector::clear. Every previously obtained
    // vertex and arc handle is invalidated.
    constexpr void clear() noexcept {
        _vertices.clear();
        _arcs.clear();
        _vertices_filter.clear();
        _arcs_filter.clear();
        _first_vertex = INVALID_VERTEX;
        _first_free_vertex = INVALID_VERTEX;
        _first_free_arc = INVALID_ARC;
        _num_vertices = 0;
        _num_arcs = 0;
    }
    constexpr void change_arc_target(const arc a, const vertex t) noexcept {
        assert(is_valid_arc(a));
        assert(is_valid_vertex(t));
        arc_struct & as = _arcs[a];
        if(as.target == t) return;
        remove_from_target_in_arcs(a);
        vertex_struct & ts = _vertices[t];
        as.target = t;
        as.prev_in_arc = INVALID_ARC;
        as.next_in_arc = ts.first_in_arc;
        if(ts.first_in_arc != INVALID_ARC)
            _arcs[ts.first_in_arc].prev_in_arc = a;
        ts.first_in_arc = a;
    }
    constexpr void change_arc_source(const arc a, const vertex s) noexcept {
        assert(is_valid_arc(a));
        assert(is_valid_vertex(s));
        arc_struct & as = _arcs[a];
        if(as.source == s) return;
        remove_from_source_out_arcs(a);
        vertex_struct & ss = _vertices[s];
        as.source = s;
        as.prev_out_arc = INVALID_ARC;
        as.next_out_arc = ss.first_out_arc;
        if(ss.first_out_arc != INVALID_ARC)
            _arcs[ss.first_out_arc].prev_out_arc = a;
        ss.first_out_arc = a;
    }

    // None of the four below are noexcept: they allocate.
    template <typename T>
    [[nodiscard]] constexpr static_map<vertex, T> create_vertex_map() const {
        return static_map<vertex, T>(_vertices.size());
    }
    template <typename T>
    [[nodiscard]] constexpr static_map<vertex, T> create_vertex_map(
        const T & default_value) const {
        return static_map<vertex, T>(_vertices.size(), default_value);
    }
    template <typename T>
    [[nodiscard]] constexpr static_map<arc, T> create_arc_map() const {
        return static_map<arc, T>(_arcs.size());
    }
    template <typename T>
    [[nodiscard]] constexpr static_map<arc, T> create_arc_map(
        const T & default_value) const {
        return static_map<arc, T>(_arcs.size(), default_value);
    }
};

}  // namespace melon
