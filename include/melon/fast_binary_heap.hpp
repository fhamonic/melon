#ifndef MELON_FAST_BINARY_HEAP_HPP
#define MELON_FAST_BINARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {


template <typename ND, typename PR, typename CMP = std::less<PR>>
class FastBinaryHeap {
public:
    using Node = ND;
    using Prio = PR;
    using Compare = CMP;
    using Pair = std::pair<Node, Prio>;

private:
    using Index = std::vector<Pair>::size_type;

public:
    enum State : Index {
        PRE_HEAP = Index(0),
        POST_HEAP = Index(1),
        IN_HEAP = Index(sizeof(Pair))
    };

    std::vector<Pair> heap_array;
    std::vector<Index> indices_map;
    Compare cmp;

public:
    FastBinaryHeap(const std::size_t nb_nodes)
        : heap_array(1), indices_map(nb_nodes, State::PRE_HEAP), cmp() {}

    FastBinaryHeap(const FastBinaryHeap & bin) = default;
    FastBinaryHeap(FastBinaryHeap && bin) = default;

    Index size() const noexcept { return heap_array.size() - 1; }
    bool empty() const noexcept { return size() == 0; }
    void clear() noexcept {
        heap_array.resize(1);
        std::ranges::fill(indices_map, State::PRE_HEAP);
    }

private:
    constexpr Pair & pair_ref(Index i) {
        return *(reinterpret_cast<Pair *>(
            reinterpret_cast<std::byte *>(heap_array.data()) + i));
    }
    constexpr const Pair & pair_ref(Index i) const {
        return *(reinterpret_cast<const Pair *>(
            reinterpret_cast<const std::byte *>(heap_array.data()) + i));
    }

    void heap_move(Index index, Pair && p) noexcept {
        indices_map[p.first] = index;
        pair_ref(index) = std::move(p);
    }

    void heap_push(Index holeIndex, Pair && p) noexcept {
        while(holeIndex > sizeof(Pair)) {
            Index parent = holeIndex / (2 * sizeof(Pair)) * sizeof(Pair);
            if(!cmp(p.second, pair_ref(parent).second)) break;
            heap_move(holeIndex, std::move(pair_ref(parent)));
            holeIndex = parent;
        }
        heap_move(holeIndex, std::move(p));
    }
    
    void adjust_heap(Index holeIndex, const Index end, Pair && p) noexcept {
        Index child = 2 * holeIndex;
        while(child < end) {
            child += sizeof(Pair) * cmp(pair_ref(child + sizeof(Pair)).second,
                                        pair_ref(child).second);
            if(!cmp(pair_ref(child).second, p.second)) {
                return heap_move(holeIndex, std::move(p));
            }
            heap_move(holeIndex, std::move(pair_ref(child)));
            holeIndex = child;
            child = 2 * holeIndex;
        }
        if(child == end && cmp(pair_ref(child).second, p.second)) {
            heap_move(holeIndex, std::move(pair_ref(child)));
            holeIndex = child;
        }
        heap_move(holeIndex, std::move(p));
    }

public:
    void push(Pair && p) noexcept {
        heap_array.emplace_back();
        heap_push(Index(size() * sizeof(Pair)), std::move(p));
    }
    void push(const Node i, const Prio p) noexcept { push(Pair(i, p)); }
    bool contains(const Node u) const noexcept { return indices_map[u] > 0; }
    Prio prio(const Node u) const noexcept {
        return pair_ref(indices_map[u]).second;
    }
    Pair top() const noexcept {
        assert(!empty());
        return heap_array[1];
    }
    Pair pop() noexcept {
        assert(!empty());
        const Index n = heap_array.size() - 1;
        const Pair p = std::move(heap_array[1]);
        indices_map[p.first] = POST_HEAP;
        if(n > 1)
            adjust_heap(Index(sizeof(Pair)), n * sizeof(Pair),
                        std::move(heap_array.back()));
        heap_array.pop_back();
        return p;
    }
    void decrease(const Node & u, const Prio & p) noexcept {
        heap_push(indices_map[u], Pair(u, p));
    }
    State state(const Node & u) const noexcept {
        return State(std::min(indices_map[u], Index(sizeof(Pair))));
    }
};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_FAST_BINARY_HEAP_HPP
