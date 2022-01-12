#ifndef MELON_D_ARY_HEAP_HPP
#define MELON_D_ARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <int D, typename ND, typename PR, typename CMP = std::less<PR>>
class DAryHeap {
public:
    using Node = ND;
    using Prio = PR;
    using Compare = CMP;
    using Pair = std::pair<Node, Prio>;

private:
    using Index = std::vector<Pair>::size_type;

public:
    enum State : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    std::vector<Pair> heap_array;
    std::vector<Index> indices_map;
    std::vector<State> states_map;
    Compare cmp;

public:
    DAryHeap(const std::size_t nb_nodes)
        : heap_array()
        , indices_map(nb_nodes)
        , states_map(nb_nodes, State::PRE_HEAP)
        , cmp() {}

    DAryHeap(const DAryHeap & bin) = default;
    DAryHeap(DAryHeap && bin) = default;

    Index size() const noexcept { return heap_array.size(); }
    bool empty() const noexcept { return heap_array.empty(); }
    void clear() noexcept {
        heap_array.resize(0);
        std::ranges::fill(states_map, State::PRE_HEAP);
    }

private:
    static constexpr Index parent_of(Index i) {
        return (i - sizeof(Pair)) / (sizeof(Pair) * D) * sizeof(Pair);
    }
    static constexpr Index first_child_of(Index i) {
        return i * D + sizeof(Pair);
    }
    template <int I = D>
    constexpr Index minimum_child(const Index first_child) {
        if constexpr(I == 1)
            return first_child;
        else if constexpr(I == 2)
            return first_child +
                   sizeof(Pair) *
                       cmp(pair_ref(first_child + sizeof(Pair)).second,
                           pair_ref(first_child).second);
        else {
            Index first_half_minimum = minimum_child<I / 2>(first_child);
            Index second_half_minimum =
                minimum_child<I - I / 2>(first_child + (I / 2) * sizeof(Pair));
            return cmp(pair_ref(second_half_minimum).second,
                       pair_ref(first_half_minimum).second)
                       ? second_half_minimum
                       : first_half_minimum;
        }
    }
    constexpr Index minimum_remaining_child(const Index first_child,
                                            const Index nb_children) {
        if constexpr(D == 2)
            return first_child;
        else if constexpr(D == 4) {
            switch(nb_children) {
                case 1:
                    return minimum_child<1>(first_child);
                case 2:
                    return minimum_child<2>(first_child);
                default:
                    return minimum_child<3>(first_child);
            }
        } else {
            switch(nb_children) {
                case 1:
                    return minimum_child<1>(first_child);
                case 2:
                    return minimum_child<2>(first_child);
                default:
                    const Index half = nb_children / 2;
                    const Index first_half_minimum =
                        minimum_remaining_child(first_child, half);
                    const Index second_half_minimum = minimum_remaining_child(
                        first_child + half * sizeof(Pair), nb_children - half);
                    return cmp(pair_ref(second_half_minimum).second,
                               pair_ref(first_half_minimum).second)
                               ? second_half_minimum
                               : first_half_minimum;
            }
        }
    }

    constexpr Pair & pair_ref(Index i) {
        assert(0 <= (i / sizeof(Pair)) &&
               (i / sizeof(Pair)) < heap_array.size());
        return *(reinterpret_cast<Pair *>(
            reinterpret_cast<std::byte *>(heap_array.data()) + i));
    }
    constexpr const Pair & pair_ref(Index i) const {
        assert(0 <= (i / sizeof(Pair)) &&
               (i / sizeof(Pair)) < heap_array.size());
        return *(reinterpret_cast<const Pair *>(
            reinterpret_cast<const std::byte *>(heap_array.data()) + i));
    }
    void heap_move(Index i, Pair && p) noexcept {
        assert(0 <= (i / sizeof(Pair)) &&
               (i / sizeof(Pair)) < heap_array.size());
        indices_map[p.first] = i;
        pair_ref(i) = std::move(p);
    }

    void heap_push(Index holeIndex, Pair && p) noexcept {
        while(holeIndex > 0) {
            Index parent = parent_of(holeIndex);
            if(!cmp(p.second, pair_ref(parent).second)) break;
            heap_move(holeIndex, std::move(pair_ref(parent)));
            holeIndex = parent;
        }
        heap_move(holeIndex, std::move(p));
    }

    void adjust_heap(Index holeIndex, const Index end, Pair && p) noexcept {
        Index child_end;
        if constexpr(D == 2)
            child_end = end - (D - 1) * sizeof(Pair);
        else
            child_end =
                end > D * sizeof(Pair) ? end - (D - 1) * sizeof(Pair) : 0;
        Index child = first_child_of(holeIndex);
        while(child < child_end) {
            child = minimum_child(child);
            if(cmp(p.second, pair_ref(child).second)) {
                return heap_move(holeIndex, std::move(p));
            }
            heap_move(holeIndex, std::move(pair_ref(child)));
            holeIndex = child;
            child = first_child_of(child);
        }
        if(child < end) {
            child =
                minimum_remaining_child(child, (end - child) / sizeof(Pair));
            if(cmp(pair_ref(child).second, p.second)) {
                heap_move(holeIndex, std::move(pair_ref(child)));
                holeIndex = child;
            }
        }
        heap_move(holeIndex, std::move(p));
    }

public:
    void push(Pair && p) noexcept {
        const Index n = heap_array.size();
        heap_array.emplace_back();
        states_map[p.first] = IN_HEAP;
        heap_push(Index(n * sizeof(Pair)), std::move(p));
    }
    void push(const Node i, const Prio p) noexcept { push(Pair(i, p)); }
    Prio prio(const Node u) const noexcept {
        return pair_ref(indices_map[u]).second;
    }
    Pair top() const noexcept {
        assert(!heap_array.empty());
        return heap_array.front();
    }
    Pair pop() noexcept {
        assert(!heap_array.empty());
        const Pair p = std::move(heap_array.front());
        states_map[p.first] = POST_HEAP;
        const Index n = heap_array.size() - 1;
        if(n > 0)
            adjust_heap(Index(0), n * sizeof(Pair),
                        std::move(heap_array.back()));
        heap_array.pop_back();
        return p;
    }
    void decrease(const Node & u, const Prio & p) noexcept {
        heap_push(indices_map[u], Pair(u, p));
    }
    State state(const Node & u) const noexcept { return states_map[u]; }
};  // class DAryHeap

template <typename ND, typename PR, typename CMP = std::less<PR>>
using BinaryHeap = DAryHeap<2, ND, PR, CMP>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_D_ARY_HEAP_HPP