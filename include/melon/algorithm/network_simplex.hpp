#pragma once

// Primal network simplex for the minimum-cost flow problem, in the
// implementation lineage of LEMON's NetworkSimplex (Boost Software License).
// Solves: minimize sum c_a * x_a  subject to  0 <= x_a <= u_a and, at every
// vertex v, (flow out of v) - (flow into v) = supply(v). Supplies must sum
// to zero (asserted); non-zero lower bounds are not implemented.
//
// Unlike LEMON, nothing is renumbered, no problem copy is made, and the
// artificial root is not materialized: `_parent[u] == u` marks u's pred as
// its virtual arc (the self-parent stands in for LEMON's -1 sentinel,
// which a generic id cannot hold), and the thread order is a cyclic ring
// over the real vertices. Ids of both spaces may be any copyable,
// equality-comparable, default-initializable types the graph's factories
// and the mappings accept. Arc endpoints the graph cannot answer are
// filled once from arcs_entries, so an arc list qualifies.
//
// The price of not copying:
// - the arcs() range's per-step cost lands in the innermost scan: a graph
//   that pointer-chases its arc list (mutable_digraph) measured 2-3x
//   whole-solve against make_static_digraph + solve, rebuild included;
// - the mappings are read live: they must keep answering, with unchanged
//   values, from reset() until the last query. Mutating one and calling
//   reset() solves the new problem (the feature); mutating one mid-run
//   corrupts the basis silently, and a lambda mapping is re-evaluated on
//   every read.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "melon/borrowed_graph.hpp"
#include "melon/detail/consumable_view.hpp"
#include "melon/detail/map_if.hpp"
#include "melon/graph.hpp"
#include "melon/mapping.hpp"
#include "melon/views/graph_view.hpp"

namespace melon {

// Entering-arc pivot rules, from LEMON's catalog. A rule is a value type,
// constructed from the arc count and handed a fresh search context each
// pivot; it must keep no references of its own, which would dangle when
// the algorithm moves. The context serves num_arcs(),
// state_reduced_cost(a) (negative ⟺ entering candidate) and scan(visit):
// a resumable wraparound walk that stops when visit returns true and
// resumes ON that arc -- a stopping rule re-examines it.
namespace pivot_rules {

// LEMON's default: the most negative reduced cost of a
// max(factor * sqrt(m), min)-sized block.
template <double BlockSizeFactor = 1.0, int MinBlockSize = 10>
class block_search {
private:
    int _block_size;

public:
    explicit constexpr block_search(const std::size_t num_arcs) noexcept
        : _block_size(std::max(
              static_cast<int>(BlockSizeFactor *
                               std::sqrt(static_cast<double>(num_arcs))),
              MinBlockSize)) {}

    template <typename Context>
    [[nodiscard]] constexpr auto find_entering_arc(Context & context) {
        using arc = typename Context::arc_type;
        using cost = typename Context::cost_type;
        std::optional<arc> best;
        cost min(0);
        int cnt = _block_size;
        context.scan([&](const arc & e) {
            const cost c = context.state_reduced_cost(e);
            // Both tests are cold; saying so keeps the scan's fast path
            // straight-line -- ~2-3% whole-run on the NETGEN instances.
            if(c < min) [[unlikely]] {
                min = c;
                best = e;
            }
            if(--cnt == 0) [[unlikely]] {
                if(min < cost(0)) return true;
                cnt = _block_size;
            }
            return false;
        });
        // engaged ⟺ some c went below 0.
        return best;
    }
};

// The first arc with negative reduced cost enters -- a block of one: the
// boundary check fires on every arc, so the scan stops at the first
// negative minimum. Resuming on the chosen arc is fine: the pivot just
// made it ineligible, so the next search skips past it.
using first_eligible = block_search<0.0, 1>;

// Dantzig's rule: the most negative reduced cost of a full scan -- a block
// no scan fills, so the boundary check never fires.
using best_eligible = block_search<0.0, std::numeric_limits<int>::max()>;

}  // namespace pivot_rules

template <typename Rule, typename Context>
concept network_simplex_pivot_rule =
    std::constructible_from<Rule, std::size_t> &&
    requires(Rule rule, Context & context) {
        {
            rule.find_entering_arc(context)
        } -> std::same_as<std::optional<typename Context::arc_type>>;
    };

template <typename Traits>
concept network_simplex_traits = requires {
    typename Traits::pivot_rule;
    requires std::constructible_from<typename Traits::pivot_rule, std::size_t>;
    { Traits::arc_mixing } -> std::convertible_to<bool>;
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
    using pivot_rule = pivot_rules::block_search<>;
    // LEMON's arc_mixing as a scan order: over a random-access arcs range,
    // visit with stride max(m/n, 3) so a block samples sources from the
    // whole graph even when the layout packs same-source arcs together
    // (elsewhere the flag is ignored). Off by default, unlike LEMON's,
    // whose mixed *storage* stays memory-sequential: a mixed scan order
    // strides through the state/cost/potential reads -- measured 7-15%
    // whole-solve loss on families whose packing never inflated the pivot
    // count. Flip it on when a packed layout measurably does.
    static constexpr bool arc_mixing = false;

    using total_cost_type =
        typename detail::network_simplex_total_cost<ValueType, CostType>::type;
};

enum class mcf_status : char { optimal = 0, infeasible = 1, unbounded = 2 };

// Precondition no concept can check: the value type's arithmetic must be
// exact enough that pushing delta around a cycle restores conservation --
// integral values are the intended use. A capacity of
// numeric_limits<value>::max() means +infinity. Worst-case exponential
// pivots like every simplex; in practice the fastest exact MCF method
// known.
//
// The value domain is the capacity/supply common_type (flows are compared
// to both) and must be signed like the cost type: in unsigned arithmetic
// no reduced cost ever tests negative, and every feasible instance comes
// back infeasible. default_initializable is not redundant either: the
// pivot scratch members are declared plain and assigned later, so dropping
// it only worsens the diagnostic.
template <graph_view Graph, mapping_view<arc_t<Graph>> UpperBoundMap,
          mapping_view<arc_t<Graph>> CostMap,
          mapping_view<vertex_t<Graph>> SupplyMap,
          network_simplex_traits Traits = network_simplex_default_traits<
              std::common_type_t<mapped_value_t<UpperBoundMap, arc_t<Graph>>,
                                 mapped_value_t<SupplyMap, vertex_t<Graph>>>,
              mapped_value_t<CostMap, arc_t<Graph>>>>
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
    using pivot_rule_t = typename Traits::pivot_rule;
    static constexpr bool _random_access_arcs =
        std::ranges::random_access_range<arcs_range_t<Graph>>;

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

    // spanning tree structure. `_parent[u] == u` ⟺ u's pred is its id-less
    // virtual arc ⟺ `_pred[u]` is indeterminate -- every _pred read is
    // guarded by that test or follows a pivot's write. _thread/_rev_thread
    // form one cyclic preorder ring over the real vertices, with no
    // distinguished start for any operation to rely on.
    vertex_map_t<Graph, vertex> _parent;
    vertex_map_t<Graph, arc> _pred;
    vertex_map_t<Graph, signed char> _pred_dir;
    vertex_map_t<Graph, vertex> _thread, _rev_thread, _last_succ;
    vertex_map_t<Graph, int> _succ_num;
    std::vector<vertex> _dirty_revs;

    // pivot state; the entering arc's endpoints are cached once per pivot
    // -- they are read at up to six places, and a graph may compute
    // arc_source per call (complete_digraph divides).
    arc _in_arc;
    // The search's resume position: index state for the strided walk over a
    // random-access arcs range, elsewhere a cursor into arcs(_graph) -- the
    // one member that may refer into _graph, rebuilt by the move operations
    // instead of member-moved (see there).
    struct strided_resume {
        std::size_t i = 0, j = 0, skip = 1;
    };
    std::conditional_t<_random_access_arcs, strided_resume, arc_cursor> _scan;
    std::size_t _arc_count;
    [[no_unique_address]] pivot_rule_t _pivot_rule;
    vertex _in_arc_src, _in_arc_tgt;
    // The root has no vertex value, so the cycle's apex is a discriminant:
    // _join is meaningful, and read, only when this is false.
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

    // One resumable wraparound walk: visit(e) at most _arc_count times,
    // stopping when it returns true with the resume position left ON e. The
    // strided branch is LEMON's mixing permutation as a visit order: chains
    // j, j+skip, ... partition [0, m), and the advance is an m-cycle, so a
    // walk that never stops ends where it began.
    template <typename Visit>
    constexpr void _scan_arcs(Visit && visit) {
        if constexpr(_random_access_arcs) {
            // A named range: it need not be borrowed, and its iterator may
            // point back into it.
            auto arcs_range = melon::arcs(_graph);
            const auto first = std::ranges::begin(arcs_range);
            using diff_t = std::ranges::range_difference_t<arcs_range_t<Graph>>;
            for(std::size_t remaining = _arc_count; remaining != 0;
                --remaining) {
                const arc e = first[static_cast<diff_t>(_scan.i)];
                if(visit(e)) return;
                _scan.i += _scan.skip;
                if(_scan.i >= _arc_count) {
                    ++_scan.j;
                    _scan.i = _scan.j;
                    if(_scan.j >= _scan.skip || _scan.i >= _arc_count) {
                        _scan.i = 0;
                        _scan.j = 0;
                    }
                }
            }
        } else {
            // `remaining` bounds the wrap because the cursor's iterators
            // need not be comparable across the re-seed.
            std::size_t remaining = _arc_count;
            while(remaining != 0 && !_scan.empty()) {
                --remaining;
                const arc e = _scan.current();
                if(visit(e)) return;
                _scan.advance();
            }
            if(remaining != 0) {
                // All `remaining` arcs sit before the old resume point, so
                // this walk cannot reach the range's end: no emptiness test.
                _scan = melon::arcs(_graph);
                while(remaining != 0) {
                    --remaining;
                    const arc e = _scan.current();
                    if(visit(e)) return;
                    _scan.advance();
                }
            }
        }
    }

    // What a pivot rule may read, built fresh per search so rules never
    // hold references into the algorithm.
    class entering_search_context {
    private:
        network_simplex & _ns;

    public:
        using arc_type = arc;
        using cost_type = cost_t;

        constexpr explicit entering_search_context(network_simplex & ns)
            : _ns(ns) {}

        [[nodiscard]] constexpr std::size_t num_arcs() const noexcept {
            return _ns._arc_count;
        }
        // Negative ⟺ entering candidate; 0 for a tree arc.
        [[nodiscard]] constexpr cost_type state_reduced_cost(
            const arc & e) const {
            return _ns._state[e] *
                   (_ns._cost_map[e] + _ns._pi[_ns._arc_source(e)] -
                    _ns._pi[_ns._arc_target(e)]);
        }
        template <typename Visit>
        constexpr void scan(Visit && visit) {
            _ns._scan_arcs(std::forward<Visit>(visit));
        }
    };

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
        , _scan(_initial_cursor())
        , _pivot_rule(std::size_t{0}) {
        // In the body, not at class scope: the concept needs the class
        // complete.
        static_assert(
            network_simplex_pivot_rule<pivot_rule_t, entering_search_context>);
        reset();
    }

    template <typename... Args>
        requires std::constructible_from<network_simplex, Args...>
    constexpr network_simplex(Traits, Args &&... args)
        : network_simplex(std::forward<Args>(args)...) {}

private:
    // Member-moving the cursor would reseek it by walking the moved-from
    // graph's range, so it is rebuilt here, O(m), at the old offset. No
    // rebuild when nothing dangles: index state, a borrowed graph (only a
    // reference to it moved), or a borrowed arcs range.
    [[nodiscard]] constexpr auto _relocated(auto && scan) {
        if constexpr(_random_access_arcs || borrowed_graph<Graph> ||
                     std::ranges::borrowed_range<arcs_range_t<Graph>>) {
            return std::move(scan);
        } else {
            return arc_cursor(melon::arcs(_graph), scan.consumed());
        }
    }
    [[nodiscard]] constexpr auto _initial_cursor() {
        if constexpr(_random_access_arcs)
            return strided_resume{};
        else
            return arc_cursor(melon::arcs(_graph));
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
        , _pivot_rule(std::move(o._pivot_rule))
        , _in_arc_src(std::move(o._in_arc_src))
        , _in_arc_tgt(std::move(o._in_arc_tgt))
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
        _pivot_rule = std::move(o._pivot_rule);
        _in_arc_src = std::move(o._in_arc_src);
        _in_arc_tgt = std::move(o._in_arc_tgt);
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
        // art_cost must beat every real route's cost, or feasible instances
        // come back infeasible. A route is a simple *residual* path -- at
        // most n-1 arcs, backward arcs contributing -c -- so the bound is
        // built from max|cost|, not max cost: min_cost matters as much as
        // max_cost.
        cost_t min_cost(0), max_cost(0);
        for(auto && a : melon::arcs(_graph)) {
            _flow[a] = value_t(0);
            _state[a] = STATE_LOWER;
            assert(static_cast<value_t>(_upper_bound_map[a]) >= value_t(0));
            const cost_t c = _cost_map[a];
            if(c > max_cost) max_cost = c;
            if(c < min_cost) min_cost = c;
        }
        // (max|cost|+1)*n when it fits under half the type's range, else the
        // widest constant that does -- it still dominates any path shorter
        // than itself -- else asserted: past that band every verdict would
        // be a guess. Bounds are tested by division so the overflowing
        // product is never formed.
        const cost_t art_ceiling = std::numeric_limits<cost_t>::max() / 2 + 1;
        const cost_t tight_bound =
            (n == 0) ? art_ceiling : (art_ceiling - 1) / static_cast<cost_t>(n);
        cost_t art_cost = art_ceiling;
        if(max_cost < tight_bound && min_cost > -tight_bound) {
            // -min_cost only forms here, above -tight_bound: the value
            // negation cannot represent never reaches it.
            const cost_t max_abs_cost =
                (max_cost > -min_cost) ? max_cost : -min_cost;
            art_cost = (max_abs_cost + 1) * static_cast<cost_t>(n);
        } else {
            const cost_t ceiling_bound =
                (n <= 1) ? art_ceiling
                         : (art_ceiling - 1) / static_cast<cost_t>(n - 1);
            assert(max_cost < ceiling_bound && min_cost > -ceiling_bound &&
                   "network_simplex: arc costs too large for the cost type -- "
                   "num_vertices * max|cost| must stay within its range");
        }

        // Hang each vertex off the implicit root, chaining the thread ring
        // in enumeration order. std::optional, not a sentinel, for "no
        // vertex seen yet": every vertex value can name a real vertex.
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

        _pivot_rule = pivot_rule_t(_arc_count);
        if constexpr(_random_access_arcs) {
            _scan = strided_resume{
                .i = 0,
                .j = 0,
                .skip = (Traits::arc_mixing && n > 1)
                            ? std::max<std::size_t>(_arc_count / n, 3)
                            : 1};
        } else {
            _scan = melon::arcs(_graph);
        }
        _status = mcf_status::optimal;
        _has_entering = find_entering_arc();
        if(!_has_entering) finalize();
        return *this;
    }

private:
    // ---- One pivot, in LEMON-shaped pieces ----------------------------------

    // Virtual arcs are outside arcs(_graph), so only real arcs are scanned;
    // the cost mapping and the arc endpoints are read live per candidate.
    bool find_entering_arc() {
        entering_search_context context(*this);
        const std::optional<arc> entering =
            _pivot_rule.find_entering_arc(context);
        if(!entering) return false;
        _in_arc = *entering;
        _in_arc_src = _arc_source(_in_arc);
        _in_arc_tgt = _arc_target(_in_arc);
        return true;
    }

    // The basis cycle's apex: the endpoints' common ancestor. An ancestor
    // has the strictly larger succ_num, which makes stepping the smaller
    // side safe and the early exit sound -- a self-parented vertex selected
    // as strictly smaller cannot be the other side's ancestor, so the walks
    // can only meet at the implicit root.
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

    // One walk up to the apex: exclusive of a real join, inclusive of the
    // top vertex when the join is the root (its virtual arc is on the
    // cycle). No self-parent test in the real-join form: below a real join
    // no vertex is self-parented.
    //
    // The parent is read *before* f(u), overlapping the next dependent load
    // with f's work -- these walks are pure pointer chasing. The contract:
    // f may not change the parent of the vertex it is handed.
    template <typename F>
    constexpr void _for_cycle_path(vertex u, F && f) const {
        if(_join_is_root) {
            for(;;) {
                const vertex p = _parent[u];
                f(u);
                if(p == u) break;
                u = p;
            }
        } else {
            while(u != _join) {
                const vertex p = _parent[u];
                f(u);
                u = p;
            }
        }
    }

    // The residual of u's pred arc in the walking direction: `residual_dir`
    // is the pred direction that means "walking against the arc", where the
    // residual is capacity minus flow -- and a virtual capacity is infinite.
    [[nodiscard]] value_t pred_residual(const vertex & u,
                                        const signed char residual_dir) const {
        if(_parent[u] == u) {
            return (_pred_dir[u] == residual_dir) ? MAX : _virtual_flow[u];
        }
        const arc e = _pred[u];
        const value_t f = _flow[e];
        if constexpr(std::is_trivially_copyable_v<value_t>) {
            // Selects, not an `if`, and don't simplify it back: the walk
            // direction is data-dependent per step, so the branch form
            // mispredicts -- measured 1-4% whole-run. The price, an
            // upper-bound read on every step instead of half, is why the
            // trivially-copyable gate exists.
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
            const auto push = [&](const value_t signed_val) {
                return [&, signed_val](const vertex & u) {
                    if(_parent[u] == u)
                        _virtual_flow[u] += _pred_dir[u] * signed_val;
                    else
                        _flow[_pred[u]] += _pred_dir[u] * signed_val;
                };
            };
            _for_cycle_path(_in_arc_src, push(-val));
            _for_cycle_path(_in_arc_tgt, push(val));
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

    // Re-hang the subtree cut off at _u_out under _v_in, re-rooted at
    // _u_in: reverse the stem path, splice the thread ring, repair succ_num
    // and last_succ on the two apex paths. Derived from LEMON's
    // updateTreeStructure with the root removed from the ring: every splice
    // is a local ring operation, so where LEMON's list passes through the
    // root this ring wraps -- the values differ, the links spliced are the
    // same -- and the walks stop on a self-parent instead of the -1
    // sentinel, dropping only iterations whose sole effect was a root slot.
    // When _u_out hangs off the root, v_out reads as _u_out itself and the
    // join is necessarily the root: every walk from v_out is skipped.
    void update_tree_structure() {
        const vertex old_rev_thread = _rev_thread[_u_out];
        const int old_succ_num = _succ_num[_u_out];
        const vertex old_last_succ = _last_succ[_u_out];
        const bool v_out_is_root = (_parent[_u_out] == _u_out);
        const vertex v_out = _parent[_u_out];

        if(_u_in == _u_out) {
            _parent[_u_in] = _v_in;
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
            _succ_num[_u_in] = old_succ_num;
        }
        // After both branches: the stem loop above copies pred values down
        // the old path, reading the slot this pair overwrites.
        _pred[_u_in] = _in_arc;
        _pred_dir[_u_in] = (_u_in == _in_arc_src) ? DIR_UP : DIR_DOWN;

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

    // Only the re-hung subtree changes potential, by the constant that
    // zeroes the entering arc's reduced cost. `end` is a valid boundary
    // because the subtree is a contiguous, proper ring segment -- proper
    // since _v_in sits outside it.
    void update_potential() {
        const cost_t sigma =
            _pi[_v_in] - _pi[_u_in] - _pred_dir[_u_in] * _cost_map[_in_arc];
        const vertex end = _thread[_last_succ[_u_in]];
        // The hottest loop of the run, latency-bound on the `_thread[u]`
        // dependent load: the cursor runs two vertices ahead of the update
        // to overlap the chain. Depth 2 measured best, and an explicit
        // prefetch on top measured worse. The lookahead running past `end`
        // is safe: the ring is cyclic, every vertex has a successor.
        vertex u0 = _u_in;
        vertex u1 = _thread[u0];
        vertex u2 = _thread[u1];
        while(u0 != end) {
            const vertex u3 = _thread[u2];
            _pi[u0] += sigma;
            u0 = u1;
            u1 = u2;
            u2 = u3;
        }
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

    // One advance() is one simplex pivot. The entering-arc search runs
    // eagerly at the end of reset() and advance(), which is what keeps
    // finished() a pure const read.
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
    // Refers into the algorithm: valid while this object lives and stays
    // put. Unconditionally noexcept: the closure captures only `this`,
    // whatever the graph's map type is.
    [[nodiscard]] constexpr auto flows_map() const & noexcept {
        return maps::mapping_all([this](const arc & a) { return _flow[a]; });
    }
    // Terminal, like std::move(alg).base(): the members left behind are
    // valid but empty, so no other member may be called afterwards.
    [[nodiscard]] constexpr auto flows_map() && noexcept(
        std::is_nothrow_move_constructible_v<arc_map_t<Graph, value_t>>) {
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
    // See flows_map().
    [[nodiscard]] constexpr auto potentials_map() const & noexcept {
        return maps::mapping_all([this](const vertex & v) { return _pi[v]; });
    }
    [[nodiscard]] constexpr auto potentials_map() && noexcept(
        std::is_nothrow_move_constructible_v<vertex_map_t<Graph, cost_t>>) {
        return maps::mapping_all(
            [pi = std::move(_pi)](const vertex & v) { return pi[v]; });
    }
    // The cost of the flow flow() reports, in the traits' widened type: the
    // optimum once converged with status() == optimal, otherwise a basic
    // intermediate's -- an infeasible instance leaves a plausible number
    // here, not a diagnosis.
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
