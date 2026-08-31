#pragma once

// Primal network simplex for the minimum-cost flow problem, in the
// implementation lineage of LEMON's NetworkSimplex (Boost Software License):
// spanning-tree basis stored as parent/pred/thread/rev_thread/succ_num/
// last_succ arrays, block-search entering-arc rule, and Cunningham's
// strongly-feasible-basis leaving-arc rule to prevent cycling.
//
// Solves: minimize sum c_a * x_a  subject to  0 <= x_a <= u_a and, at every
// vertex v, (flow out of v) - (flow into v) = supply(v). Supplies must sum to
// zero (the EQ form); non-zero lower bounds are not implemented.
//
// Unlike LEMON, nothing is renumbered, no problem copy is made, and the
// artificial root is not even materialized: the algorithm runs in the
// graph's own id spaces, every map comes from the graph's own factories,
// and the root is as implicit as its virtual arcs. `_parent[u] == u` means
// "u's pred is its virtual arc" -- the self-parent stands in for LEMON's
// -1 sentinel, which a generic id cannot hold -- the basis cycle's apex is
// a root-or-vertex discriminant, and the thread order is a cyclic ring
// over the real vertices whose splices never need a distinguished start.
// Capacities, costs and supplies are read through the user's mappings; arc
// endpoints are answered by melon::arc_source / arc_target where the graph
// has them, and each endpoint the graph cannot answer gets a map created
// through the arc-map factory and filled from arcs_entries, so an arc list
// still qualifies.
//
// Neither id space carries an integrality requirement: vertex and arc ids
// may be any copyable, equality-comparable, default-initializable types
// the graph's factories and the mappings accept -- a mutable_digraph with
// removal holes qualifies, and so does a graph whose handles are structs.
// The price of not copying:
// - the per-step cost of the arcs() range lands in the innermost scan: a
//   graph that pointer-chases its arc list (mutable_digraph joins
//   intrusive per-vertex lists) pays it on every pivot -- measured 2-3x
//   whole-solve on the LEMON-parity instances against make_static_digraph
//   followed by the same solve, rebuild included;
// - the mappings are read live during the pivots: they must keep answering,
//   with unchanged values, from reset() until the last query. Mutating a map
//   and calling reset() solves the new problem (that is the feature);
//   mutating one mid-run corrupts the basis silently. A lambda mapping is
//   re-evaluated on every read, so cache anything expensive yourself.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/detail/consumable_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

template <typename Traits>
concept network_simplex_traits = requires {
    { Traits::block_size_factor } -> std::convertible_to<double>;
    { Traits::min_block_size } -> std::convertible_to<int>;
    typename Traits::total_cost_type;
};

namespace detail {
// The unconstrained fallback keeps the alias well-formed for value/cost
// types with no operator* between them: network_simplex's constraints are
// what must reject those, and an eager decltype here would hard-error while
// the default Traits argument is formed, before any constraint is read.
template <typename ValueType, typename CostType>
struct network_simplex_total_cost {
    using type = CostType;
};
template <typename ValueType, typename CostType>
    requires requires(CostType c, ValueType v) { c * v; }
struct network_simplex_total_cost<ValueType, CostType> {
    // c*x sums over up to num_arcs terms, so accumulate in a wider type than
    // the per-arc cost when both inputs are integral.
    using type = std::conditional_t<
        std::is_integral_v<ValueType> && std::is_integral_v<CostType>,
        std::int64_t,
        decltype(std::declval<CostType>() * std::declval<ValueType>())>;
};
}  // namespace detail

template <typename ValueType, typename CostType>
struct network_simplex_default_traits {
    // Entering arcs are searched in blocks of
    // max(block_size_factor * sqrt(num arcs), min_block_size), taking the most
    // negative reduced cost seen in the block -- LEMON's default rule, a
    // middle ground between Dantzig (scan everything) and first-eligible.
    static constexpr double block_size_factor = 1.0;
    static constexpr int min_block_size = 10;

    using total_cost_type =
        typename detail::network_simplex_total_cost<ValueType, CostType>::type;
};

enum class mcf_status : char { optimal = 0, infeasible = 1, unbounded = 2 };

// Precondition, uncheckable by any concept: capacities and the value type's
// arithmetic must be exact enough that pushing delta around a cycle restores
// conservation -- integral values are the intended use, as in LEMON. A
// capacity equal to numeric_limits<value>::max() is treated as +infinity.
// Worst case exponential pivots like every simplex; in practice the fastest
// exact MCF method known.
//
// Capacities and supplies share one value domain -- flows are compared to
// both -- so it is their common_type, and it must be a signed number like
// the cost type: in unsigned arithmetic no reduced cost ever tests negative,
// so the artificial basis is never left and every feasible instance is
// silently reported infeasible.
template <graph_view Graph, mapping_view<arc_t<Graph>> UpperBoundMap,
          mapping_view<arc_t<Graph>> CostMap,
          mapping_view<vertex_t<Graph>> SupplyMap,
          network_simplex_traits Traits = network_simplex_default_traits<
              std::common_type_t<mapped_value_t<UpperBoundMap, arc_t<Graph>>,
                                 mapped_value_t<SupplyMap, vertex_t<Graph>>>,
              mapped_value_t<CostMap, arc_t<Graph>>>>
// default_initializable is not redundant: the pivot scratch members
// (_join, _u_in, _in_arc, ...) are declared plain and assigned later, so
// dropping the constraint moves the same failure to those declarations
// with a worse diagnostic.
    requires std::default_initializable<vertex_t<Graph>> &&
             std::default_initializable<arc_t<Graph>> &&
             has_num_vertices<Graph> && has_num_arcs<Graph> &&
             has_vertex_map<Graph> && has_arc_map<Graph> &&
             std::numeric_limits<std::common_type_t<
                 mapped_value_t<UpperBoundMap, arc_t<Graph>>,
                 mapped_value_t<SupplyMap, vertex_t<Graph>>>>::is_specialized &&
             std::numeric_limits<std::common_type_t<
                 mapped_value_t<UpperBoundMap, arc_t<Graph>>,
                 mapped_value_t<SupplyMap, vertex_t<Graph>>>>::is_signed &&
             std::numeric_limits<
                 mapped_value_t<CostMap, arc_t<Graph>>>::is_specialized &&
             std::numeric_limits<
                 mapped_value_t<CostMap, arc_t<Graph>>>::is_signed
class network_simplex {
private:
    using vertex = vertex_t<Graph>;
    using arc = arc_t<Graph>;
    using value_t =
        std::common_type_t<mapped_value_t<UpperBoundMap, arc_t<Graph>>,
                           mapped_value_t<SupplyMap, vertex_t<Graph>>>;
    using cost_t = mapped_value_t<CostMap, arc_t<Graph>>;
    using total_cost_t = typename Traits::total_cost_type;
    using arc_cursor = detail::consumable_input_view_t<arcs_range_t<Graph>>;

    static constexpr value_t MAX = std::numeric_limits<value_t>::max();
    static constexpr signed char STATE_UPPER = -1;
    static constexpr signed char STATE_TREE = 0;
    static constexpr signed char STATE_LOWER = 1;
    static constexpr signed char DIR_DOWN = -1;  // pred arc parent -> vertex
    static constexpr signed char DIR_UP = 1;     // pred arc vertex -> parent

private:
    Graph _graph;
    UpperBoundMap _upper_bound_map;
    CostMap _cost_map;
    SupplyMap _supply_map;

    struct no_arc_src_map;
    [[no_unique_address]] detail::arc_map_if<!has_arc_source<Graph>, Graph,
                                             vertex, no_arc_src_map> _arc_src;
    struct no_arc_tgt_map;
    [[no_unique_address]] detail::arc_map_if<!has_arc_target<Graph>, Graph,
                                             vertex, no_arc_tgt_map> _arc_tgt;
    arc_map_t<Graph, value_t> _flow;
    arc_map_t<Graph, signed char> _state;
    vertex_map_t<Graph, value_t> _virtual_flow;
    vertex_map_t<Graph, cost_t> _pi;

    // spanning tree structure. A virtual arc is "the root arc of u" and has
    // no id; `_parent[u] == u` ⟺ u's pred is its virtual arc ⟺ `_pred[u]`
    // is indeterminate, and every _pred read is guarded by that test or
    // happens after a pivot wrote the slot. _thread/_rev_thread form one
    // cyclic ring over the real vertices in tree preorder -- the ring has
    // no distinguished start, and no operation may rely on one.
    vertex_map_t<Graph, vertex> _parent;
    vertex_map_t<Graph, arc> _pred;
    vertex_map_t<Graph, signed char> _pred_dir;
    vertex_map_t<Graph, vertex> _thread, _rev_thread, _last_succ;
    vertex_map_t<Graph, int> _succ_num;
    std::vector<vertex> _dirty_revs;

    // pivot state; the entering arc's endpoints are cached once per pivot at
    // the end of the search -- they are read at up to six places per pivot,
    // and a graph may compute arc_source per call (complete_digraph divides).
    arc _in_arc;
    // Where the entering-arc search resumes: a cursor into arcs(_graph) that
    // survives between pivots. The one member that may refer into _graph, so
    // the move operations rebuild it instead of member-moving it (see there).
    arc_cursor _scan;
    std::size_t _arc_count;
    vertex _in_arc_src, _in_arc_tgt;
    int _block_size;
    // The basis cycle's apex: the root has no vertex value to store, so
    // "the join is the root" is a discriminant, and _join is meaningful --
    // and read -- only when it is false.
    bool _join_is_root;
    vertex _join, _u_in, _v_in, _u_out;
    value_t _delta;
    bool _has_entering;
    mcf_status _status;

    [[nodiscard]] constexpr vertex _arc_source(const arc & a) const {
        if constexpr(has_arc_source<Graph>)
            return melon::arc_source(_graph, a);
        else
            return _arc_src[a];
    }
    [[nodiscard]] constexpr vertex _arc_target(const arc & a) const {
        if constexpr(has_arc_target<Graph>)
            return melon::arc_target(_graph, a);
        else
            return _arc_tgt[a];
    }

public:
    // ---- Construction -------------------------------------------------------

    template <graph_for<Graph> G, mapping_for<UpperBoundMap> UM,
              mapping_for<CostMap> CM, mapping_for<SupplyMap> SM>
    constexpr network_simplex(G && g, UM && um, CM && cm, SM && sm)
        : _graph(views::graph_all(std::forward<G>(g)))
        , _upper_bound_map(maps::mapping_all(std::forward<UM>(um)))
        , _cost_map(maps::mapping_all(std::forward<CM>(cm)))
        , _supply_map(maps::mapping_all(std::forward<SM>(sm)))
        , _arc_src(_graph)
        , _arc_tgt(_graph)
        , _flow(create_arc_map<value_t>(_graph))
        , _state(create_arc_map<signed char>(_graph))
        , _virtual_flow(create_vertex_map<value_t>(_graph))
        , _pi(create_vertex_map<cost_t>(_graph))
        , _parent(create_vertex_map<vertex>(_graph))
        , _pred(create_vertex_map<arc>(_graph))
        , _pred_dir(create_vertex_map<signed char>(_graph))
        , _thread(create_vertex_map<vertex>(_graph))
        , _rev_thread(create_vertex_map<vertex>(_graph))
        , _last_succ(create_vertex_map<vertex>(_graph))
        , _succ_num(create_vertex_map<int>(_graph))
        , _scan(melon::arcs(_graph)) {
        reset();
    }

    template <typename... Args>
        requires std::constructible_from<network_simplex, Args...>
    constexpr network_simplex(Traits, Args &&... args)
        : network_simplex(std::forward<Args>(args)...) {}

private:
    // Member-moving _scan would reseek it by walking the moved-from object's
    // range, whose lambdas may hold the graph address that was moved away an
    // instant earlier (see consumable_input_view's relocation constructor),
    // so the cursor is rebuilt against this object's own graph at the old
    // offset. A borrowed arcs range keeps self-contained iterators, which an
    // ordinary move preserves at no cost.
    [[nodiscard]] constexpr arc_cursor _relocated(arc_cursor && scan) {
        if constexpr(std::ranges::borrowed_range<arcs_range_t<Graph>>) {
            return std::move(scan);
        } else {
            return arc_cursor(melon::arcs(_graph), scan.consumed());
        }
    }

public:
    // Move-only, matching every melon algorithm. Hand-written for _scan; see
    // _relocated.
    constexpr network_simplex(const network_simplex &) = delete;
    constexpr network_simplex(network_simplex && o)
        : _graph(std::move(o._graph))
        , _upper_bound_map(std::move(o._upper_bound_map))
        , _cost_map(std::move(o._cost_map))
        , _supply_map(std::move(o._supply_map))
        , _arc_src(std::move(o._arc_src))
        , _arc_tgt(std::move(o._arc_tgt))
        , _flow(std::move(o._flow))
        , _state(std::move(o._state))
        , _virtual_flow(std::move(o._virtual_flow))
        , _pi(std::move(o._pi))
        , _parent(std::move(o._parent))
        , _pred(std::move(o._pred))
        , _pred_dir(std::move(o._pred_dir))
        , _thread(std::move(o._thread))
        , _rev_thread(std::move(o._rev_thread))
        , _last_succ(std::move(o._last_succ))
        , _succ_num(std::move(o._succ_num))
        , _dirty_revs(std::move(o._dirty_revs))
        , _in_arc(std::move(o._in_arc))
        , _scan(_relocated(std::move(o._scan)))
        , _arc_count(o._arc_count)
        , _in_arc_src(std::move(o._in_arc_src))
        , _in_arc_tgt(std::move(o._in_arc_tgt))
        , _block_size(o._block_size)
        , _join_is_root(o._join_is_root)
        , _join(std::move(o._join))
        , _u_in(std::move(o._u_in))
        , _v_in(std::move(o._v_in))
        , _u_out(std::move(o._u_out))
        , _delta(std::move(o._delta))
        , _has_entering(o._has_entering)
        , _status(o._status) {}
    constexpr network_simplex & operator=(const network_simplex &) = delete;
    constexpr network_simplex & operator=(network_simplex && o) {
        if(this == std::addressof(o)) return *this;
        _graph = std::move(o._graph);
        _upper_bound_map = std::move(o._upper_bound_map);
        _cost_map = std::move(o._cost_map);
        _supply_map = std::move(o._supply_map);
        _arc_src = std::move(o._arc_src);
        _arc_tgt = std::move(o._arc_tgt);
        _flow = std::move(o._flow);
        _state = std::move(o._state);
        _virtual_flow = std::move(o._virtual_flow);
        _pi = std::move(o._pi);
        _parent = std::move(o._parent);
        _pred = std::move(o._pred);
        _pred_dir = std::move(o._pred_dir);
        _thread = std::move(o._thread);
        _rev_thread = std::move(o._rev_thread);
        _last_succ = std::move(o._last_succ);
        _succ_num = std::move(o._succ_num);
        _dirty_revs = std::move(o._dirty_revs);
        _in_arc = std::move(o._in_arc);
        _scan = _relocated(std::move(o._scan));
        _arc_count = o._arc_count;
        _in_arc_src = std::move(o._in_arc_src);
        _in_arc_tgt = std::move(o._in_arc_tgt);
        _block_size = o._block_size;
        _join_is_root = o._join_is_root;
        _join = std::move(o._join);
        _u_in = std::move(o._u_in);
        _v_in = std::move(o._v_in);
        _u_out = std::move(o._u_out);
        _delta = std::move(o._delta);
        _has_entering = o._has_entering;
        _status = o._status;
        return *this;
    }

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

    // Rebuilds the initial strongly feasible basis: every vertex hangs off
    // the root, its supply carried on its virtual arc. The virtual costs --
    // 0 toward the root, a prohibitive art_cost away from it -- are folded
    // into the initial potentials and stored nowhere.
    constexpr network_simplex & reset() {
        const std::size_t n =
            static_cast<std::size_t>(melon::num_vertices(_graph));
        _arc_count = static_cast<std::size_t>(melon::num_arcs(_graph));

        // Factory-created maps hold indeterminate values, so this function
        // must write every slot a later read can reach -- the audited
        // exception is _pred, covered by the guarded-read invariant on the
        // member.
        if constexpr(!has_arc_source<Graph> || !has_arc_target<Graph>) {
            for(auto && [a, uv] : melon::arcs_entries(_graph)) {
                [[maybe_unused]] auto && [u, v] = uv;
                if constexpr(!has_arc_source<Graph>) _arc_src[a] = u;
                if constexpr(!has_arc_target<Graph>) _arc_tgt[a] = v;
            }
        }
        // art_cost must beat every real path's cost. For an exact cost type
        // max/2+1 does, and cannot overflow; for an inexact one that
        // magnitude would absorb every real cost in the potential
        // arithmetic, so a data-dependent bound is gathered from the arc
        // pass instead.
        cost_t art_cost;
        if constexpr(std::numeric_limits<cost_t>::is_exact) {
            art_cost = std::numeric_limits<cost_t>::max() / 2 + 1;
        } else {
            art_cost = cost_t(0);
        }
        for(auto && a : melon::arcs(_graph)) {
            _flow[a] = value_t(0);
            _state[a] = STATE_LOWER;
            assert(static_cast<value_t>(_upper_bound_map[a]) >= value_t(0));
            if constexpr(!std::numeric_limits<cost_t>::is_exact) {
                const cost_t c = _cost_map[a];
                if(c > art_cost) art_cost = c;
            }
        }
        if constexpr(!std::numeric_limits<cost_t>::is_exact)
            art_cost = (art_cost + 1) * static_cast<cost_t>(n);

        // One pass hangs each vertex off the implicit root and checks the
        // EQ-form precondition that supplies cancel. The thread ring closes
        // over the real vertices in vertices() enumeration order;
        // std::optional rather than a sentinel value for "no vertex seen
        // yet", because every vertex value -- the default-constructed one
        // included -- can name a real vertex.
        value_t sum_supply(0);
        std::optional<vertex> first, prev;
        for(auto && v : melon::vertices(_graph)) {
            const value_t supply = _supply_map[v];
            sum_supply += supply;
            _parent[v] = v;
            if(prev) {
                _thread[*prev] = v;
                _rev_thread[v] = *prev;
            } else {
                first = v;
            }
            prev = v;
            _succ_num[v] = 1;
            _last_succ[v] = v;
            if(supply >= value_t(0)) {
                _pred_dir[v] = DIR_UP;
                _pi[v] = cost_t(0);
                _virtual_flow[v] = supply;
            } else {
                _pred_dir[v] = DIR_DOWN;
                _pi[v] = art_cost;
                _virtual_flow[v] = -supply;
            }
        }
        if(first) {
            _thread[*prev] = *first;
            _rev_thread[*first] = *prev;
        }
        assert(sum_supply == value_t(0));

        _block_size = std::max(static_cast<int>(Traits::block_size_factor *
                                                std::sqrt(double(_arc_count))),
                               Traits::min_block_size);
        _scan = melon::arcs(_graph);
        _status = mcf_status::optimal;
        _has_entering = find_entering_arc();
        if(!_has_entering) finalize();
        return *this;
    }

private:
    // ---- One pivot, in five LEMON-shaped pieces -----------------------------

    // Virtual arcs are outside arcs(_graph), so only real arcs are scanned;
    // the cost mapping and the arc endpoints are read live per candidate.
    bool find_entering_arc() {
        cost_t min = cost_t(0);
        int cnt = _block_size;
        // _scan resumes on the arc where the previous search stopped; one
        // walk to the end of the range and one re-seeded from its start
        // together examine every arc once, counted by `remaining` because
        // the range's iterators need not be comparable across the re-seed.
        // The block counter and the running minimum carry across the wrap.
        // True means a block boundary was hit with a negative minimum in
        // hand -- the cursor then stays on the last examined arc.
        const auto examine = [&]() {
            const arc e = _scan.current();
            const cost_t c = _state[e] * (_cost_map[e] + _pi[_arc_source(e)] -
                                          _pi[_arc_target(e)]);
            if(c < min) {
                min = c;
                _in_arc = e;
            }
            if(--cnt == 0) {
                if(min < cost_t(0)) return true;
                cnt = _block_size;
            }
            return false;
        };
        std::size_t remaining = _arc_count;
        bool stopped = false;
        while(remaining != 0 && !_scan.empty()) {
            --remaining;
            if(examine()) {
                stopped = true;
                break;
            }
            _scan.advance();
        }
        if(!stopped && remaining != 0) {
            // The first walk ran dry, so `remaining` arcs sit before the old
            // resume point: the re-seeded walk cannot reach the range's end
            // and needs no emptiness test.
            _scan = melon::arcs(_graph);
            while(remaining != 0) {
                --remaining;
                if(examine()) {
                    stopped = true;
                    break;
                }
                _scan.advance();
            }
        }
        if(!stopped && min >= cost_t(0)) return false;
        _in_arc_src = _arc_source(_in_arc);
        _in_arc_tgt = _arc_target(_in_arc);
        return true;
    }

    // The common ancestor of the entering arc's endpoints: the vertex where
    // the basis cycle closes. Walking up from the smaller subtree is safe
    // because an ancestor always has the strictly larger succ_num -- which
    // is also what makes the early exit sound: a self-parented vertex
    // selected as the strictly smaller side cannot be the other side's
    // ancestor, so the walks can only meet at the implicit root.
    void find_join_vertex() {
        vertex u = _in_arc_src;
        vertex v = _in_arc_tgt;
        _join_is_root = false;
        while(u != v) {
            if(_succ_num[u] < _succ_num[v]) {
                if(_parent[u] == u) {
                    _join_is_root = true;
                    return;
                }
                u = _parent[u];
            } else {
                if(_parent[v] == v) {
                    _join_is_root = true;
                    return;
                }
                v = _parent[v];
            }
        }
        _join = u;
    }

    // One walk from a cycle vertex up to the apex, exclusive of a real
    // join, inclusive of the top vertex when the join is the root -- its
    // virtual arc is on the cycle. A self-parented vertex cannot appear
    // strictly below a real join (the join would be its ancestor), so the
    // real-join form needs no self-parent test.
    template <typename F>
    constexpr void _for_cycle_path(vertex u, F && f) const {
        if(_join_is_root) {
            for(;;) {
                f(u);
                if(_parent[u] == u) break;
                u = _parent[u];
            }
        } else {
            for(; u != _join; u = _parent[u]) f(u);
        }
    }

    // The residual of u's pred arc in the walking direction: the guarded
    // virtual branch is where a flat implementation would read its m+n-sized
    // arrays. `residual_dir` is the pred direction that means "walking
    // against the arc", where the residual is capacity minus flow -- and a
    // virtual capacity is infinite.
    [[nodiscard]] value_t pred_residual(const vertex & u,
                                        const signed char residual_dir) const {
        if(_parent[u] == u) {
            return (_pred_dir[u] == residual_dir) ? MAX : _virtual_flow[u];
        }
        const arc e = _pred[u];
        const value_t f = _flow[e];
        if constexpr(std::is_trivially_copyable_v<value_t>) {
            // Selects, not an `if`, and don't simplify it back: whether a
            // cycle walk runs with or against each pred arc is data-dependent
            // per step, so the branch form mispredicts (measured 1-4% of the
            // whole run on the LEMON-parity instances). The price is reading
            // the upper-bound mapping on every step instead of half of them,
            // which the trivially-copyable gate keeps to a plain load.
            const value_t c = _upper_bound_map[e];
            const value_t res = (c >= MAX) ? MAX : c - f;
            return (_pred_dir[u] == residual_dir) ? res : f;
        } else {
            if(_pred_dir[u] == residual_dir) {
                const value_t c = _upper_bound_map[e];
                return (c >= MAX) ? MAX : c - f;
            }
            return f;
        }
    }

    // Minimum residual around the cycle. The `<` on the first path against
    // `<=` on the second is Cunningham's strongly-feasible tie-break: among
    // equal residuals the leaving arc closest to the join on the second path
    // wins, which is what rules out degenerate cycling.
    bool find_leaving_arc() {
        const bool lower = (_state[_in_arc] == STATE_LOWER);
        const vertex first = lower ? _in_arc_src : _in_arc_tgt;
        const vertex second = lower ? _in_arc_tgt : _in_arc_src;
        _delta = _upper_bound_map[_in_arc];
        int result = 0;
        _for_cycle_path(first, [&](const vertex & u) {
            const value_t d = pred_residual(u, DIR_DOWN);
            if(d < _delta) {
                _delta = d;
                _u_out = u;
                result = 1;
            }
        });
        _for_cycle_path(second, [&](const vertex & u) {
            const value_t d = pred_residual(u, DIR_UP);
            if(d <= _delta) {
                _delta = d;
                _u_out = u;
                result = 2;
            }
        });
        if(result == 1) {
            _u_in = first;
            _v_in = second;
        } else {
            _u_in = second;
            _v_in = first;
        }
        return result != 0;
    }

    void change_flow(const bool change) {
        if(_delta > value_t(0)) {
            const value_t val = _state[_in_arc] * _delta;
            _flow[_in_arc] += val;
            _for_cycle_path(_in_arc_src, [&](const vertex & u) {
                if(_parent[u] == u)
                    _virtual_flow[u] -= _pred_dir[u] * val;
                else
                    _flow[_pred[u]] -= _pred_dir[u] * val;
            });
            _for_cycle_path(_in_arc_tgt, [&](const vertex & u) {
                if(_parent[u] == u)
                    _virtual_flow[u] += _pred_dir[u] * val;
                else
                    _flow[_pred[u]] += _pred_dir[u] * val;
            });
        }
        if(change) {
            _state[_in_arc] = STATE_TREE;
            // A leaving virtual arc records no state: it is never searched,
            // so LOWER against UPPER could never be read back.
            if(_parent[_u_out] != _u_out) {
                const arc e = _pred[_u_out];
                _state[e] =
                    (_flow[e] == value_t(0)) ? STATE_LOWER : STATE_UPPER;
            }
        } else {
            _state[_in_arc] = -_state[_in_arc];
        }
    }

    // Re-hang the subtree cut off at _u_out under _v_in, re-rooted at _u_in:
    // reverse the stem path, splice the thread ring, then repair succ_num and
    // last_succ on the two apex paths. Derived from LEMON's
    // updateTreeStructure with the root removed from the ring: every splice
    // is a local ring operation, so where LEMON's list passes through the
    // root, this ring wraps from the preorder-last vertex to the
    // preorder-first one -- the values differ, the links spliced are the
    // same. The walks that LEMON stops on a -1 parent sentinel stop on a
    // self-parent instead, dropping only iterations whose sole effect was a
    // root slot. When _u_out hangs off the root, v_out is meaningless (its
    // "parent" is _u_out itself, pre-rehang) and the join is necessarily the
    // root, so every walk from v_out is skipped -- in the rooted form those
    // walks only touched root slots.
    void update_tree_structure() {
        const vertex old_rev_thread = _rev_thread[_u_out];
        const int old_succ_num = _succ_num[_u_out];
        const vertex old_last_succ = _last_succ[_u_out];
        const bool v_out_is_root = (_parent[_u_out] == _u_out);
        const vertex v_out = _parent[_u_out];

        if(_u_in == _u_out) {
            _parent[_u_in] = _v_in;
            _pred[_u_in] = _in_arc;
            _pred_dir[_u_in] = (_u_in == _in_arc_src) ? DIR_UP : DIR_DOWN;
            if(_thread[_v_in] != _u_out) {
                vertex after = _thread[old_last_succ];
                _thread[old_rev_thread] = after;
                _rev_thread[after] = old_rev_thread;
                after = _thread[_v_in];
                _thread[_v_in] = _u_out;
                _rev_thread[_u_out] = _v_in;
                _thread[old_last_succ] = after;
                _rev_thread[after] = old_last_succ;
            }
        } else {
            const vertex thread_continue = (old_rev_thread == _v_in)
                                               ? _thread[old_last_succ]
                                               : _thread[_v_in];
            vertex stem = _u_in;
            vertex par_stem = _v_in;
            vertex last = _last_succ[_u_in];
            vertex after = _thread[last];
            _thread[_v_in] = _u_in;
            _dirty_revs.clear();
            _dirty_revs.push_back(_v_in);
            while(stem != _u_out) {
                const vertex next_stem = _parent[stem];
                _thread[last] = next_stem;
                _dirty_revs.push_back(last);

                const vertex before = _rev_thread[stem];
                _thread[before] = after;
                _rev_thread[after] = before;

                _parent[stem] = par_stem;
                par_stem = stem;
                stem = next_stem;

                last = (_last_succ[stem] == _last_succ[par_stem])
                           ? _rev_thread[par_stem]
                           : _last_succ[stem];
                after = _thread[last];
            }
            _parent[_u_out] = par_stem;
            _thread[last] = thread_continue;
            _rev_thread[thread_continue] = last;
            _last_succ[_u_out] = last;

            if(old_rev_thread != _v_in) {
                _thread[old_rev_thread] = after;
                _rev_thread[after] = old_rev_thread;
            }
            for(const vertex u : _dirty_revs) _rev_thread[_thread[u]] = u;

            int tmp_sc = 0;
            vertex tmp_ls = _last_succ[_u_out];
            for(vertex u = _u_out, p = _parent[u]; u != _u_in;
                u = p, p = _parent[u]) {
                _pred[u] = _pred[p];
                _pred_dir[u] = -_pred_dir[p];
                tmp_sc += _succ_num[u] - _succ_num[p];
                _succ_num[u] = tmp_sc;
                _last_succ[p] = tmp_ls;
            }
            _pred[_u_in] = _in_arc;
            _pred_dir[_u_in] = (_u_in == _in_arc_src) ? DIR_UP : DIR_DOWN;
            _succ_num[_u_in] = old_succ_num;
        }

        const bool up_limit_at_join =
            !_join_is_root && (_last_succ[_join] == _v_in);
        const vertex last_succ_out = _last_succ[_u_out];
        for(vertex u = _v_in;;) {
            if(_last_succ[u] != _v_in) break;
            _last_succ[u] = last_succ_out;
            if(_parent[u] == u) break;
            u = _parent[u];
        }

        const bool fill_from_rev_thread =
            (_join_is_root || _join != old_rev_thread) &&
            _v_in != old_rev_thread;
        if(!v_out_is_root &&
           (fill_from_rev_thread || last_succ_out != old_last_succ)) {
            const vertex fill =
                fill_from_rev_thread ? old_rev_thread : last_succ_out;
            for(vertex u = v_out;;) {
                if(up_limit_at_join && u == _join) break;
                if(_last_succ[u] != old_last_succ) break;
                _last_succ[u] = fill;
                if(_parent[u] == u) break;
                u = _parent[u];
            }
        }

        _for_cycle_path(
            _v_in, [&](const vertex & u) { _succ_num[u] += old_succ_num; });
        if(!v_out_is_root)
            _for_cycle_path(
                v_out, [&](const vertex & u) { _succ_num[u] -= old_succ_num; });
    }

    // Only the re-hung subtree changes potential, by the constant that zeroes
    // the entering arc's reduced cost; the ring successor of the subtree's
    // last vertex is a valid boundary because the subtree is a contiguous,
    // proper segment of the ring -- proper since _v_in sits outside it.
    void update_potential() {
        const cost_t sigma =
            _pi[_v_in] - _pi[_u_in] - _pred_dir[_u_in] * _cost_map[_in_arc];
        const vertex end = _thread[_last_succ[_u_in]];
        for(vertex u = _u_in; u != end; u = _thread[u]) _pi[u] += sigma;
    }

    void finalize() {
        for(auto && v : melon::vertices(_graph)) {
            if(_virtual_flow[v] != value_t(0)) {
                _status = mcf_status::infeasible;
                return;
            }
        }
        _status = mcf_status::optimal;
    }

public:
    // ---- Execution ----------------------------------------------------------

    // Steppable: one advance() is one simplex pivot, so pivots can be capped,
    // counted or watched. The entering-arc search that decides termination
    // runs eagerly at the end of reset() and advance(), so finished() is a
    // pure read, const like every melon algorithm's.
    [[nodiscard]] constexpr bool finished() const noexcept {
        return _status == mcf_status::unbounded || !_has_entering;
    }

    constexpr void advance() {
        assert(!finished());
        find_join_vertex();
        const bool change = find_leaving_arc();
        if(_delta >= MAX) {
            _status = mcf_status::unbounded;
            return;
        }
        change_flow(change);
        if(change) {
            update_tree_structure();
            update_potential();
        }
        _has_entering = find_entering_arc();
        if(!_has_entering) finalize();
    }

    constexpr network_simplex & run() {
        while(!finished()) advance();
        return *this;
    }

    // ---- Queries ------------------------------------------------------------

    // Meaningful once finished(): while pivots remain it still reads
    // `optimal`, because infeasibility is only detectable at termination.
    [[nodiscard]] constexpr mcf_status status() const noexcept {
        return _status;
    }

    // The flow carried by `a`: part of an optimal flow once run() has
    // converged with status() == optimal; a basic (not necessarily feasible)
    // intermediate otherwise.
    [[nodiscard]] constexpr value_t flow(const arc & a) const
        noexcept(noexcept(_flow[a])) {
        return _flow[a];
    }
    // Refers into the algorithm, like every melon map view: valid while this
    // object lives and stays put.
    [[nodiscard]] constexpr auto flows_map() const & {
        return maps::mapping_all([this](const arc & a) { return _flow[a]; });
    }
    // Terminal, like std::move(alg).base(): the members left behind are
    // valid but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto flows_map() && {
        return maps::mapping_all(
            [flow = std::move(_flow)](const arc & a) { return flow[a]; });
    }
    // The dual certificate: with status() == optimal, every arc satisfies
    // complementary slackness against these potentials, with reduced cost
    // c(a) + potential(source) - potential(target).
    [[nodiscard]] constexpr cost_t potential(const vertex & v) const
        noexcept(noexcept(_pi[v])) {
        return _pi[v];
    }
    [[nodiscard]] constexpr auto potentials_map() const & {
        return maps::mapping_all([this](const vertex & v) { return _pi[v]; });
    }
    [[nodiscard]] constexpr auto potentials_map() && {
        return maps::mapping_all(
            [pi = std::move(_pi)](const vertex & v) { return pi[v]; });
    }
    [[nodiscard]] constexpr total_cost_t total_cost() const {
        total_cost_t sum(0);
        for(auto && a : melon::arcs(_graph)) {
            sum += static_cast<total_cost_t>(_cost_map[a]) *
                   static_cast<total_cost_t>(_flow[a]);
        }
        return sum;
    }
};

template <typename G, typename UM, typename CM, typename SM>
network_simplex(G &&, UM &&, CM &&, SM &&)
    -> network_simplex<views::graph_all_t<G>, maps::mapping_all_t<UM>,
                       maps::mapping_all_t<CM>, maps::mapping_all_t<SM>>;

template <typename G, typename UM, typename CM, typename SM, typename Traits>
network_simplex(Traits, G &&, UM &&, CM &&, SM &&)
    -> network_simplex<views::graph_all_t<G>, maps::mapping_all_t<UM>,
                       maps::mapping_all_t<CM>, maps::mapping_all_t<SM>,
                       Traits>;

}  // namespace melon
