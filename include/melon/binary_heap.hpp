#ifndef MELON_BINARY_HEAP_HPP
#define MELON_BINARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename ND, typename PR, typename CMP = std::less<PR>>
class BinaryHeap {
public:
    using Node = ND;
    using Prio = PR;
    using Compare = CMP;
    using Pair = std::pair<Node, Prio>;

private:
    using Difference = int;

public:
    enum State {
        IN_HEAP = Difference(1),
        PRE_HEAP = Difference(0),
        POST_HEAP = Difference(-1)
    };

    std::vector<Pair> heap_array;
    std::vector<Difference> indices_map;
    Compare cmp;

public:
    BinaryHeap(const std::size_t nb_nodes)
        : heap_array(), indices_map(nb_nodes, State::PRE_HEAP), cmp() {}

    BinaryHeap(const BinaryHeap & bin) = default;
    BinaryHeap(BinaryHeap && bin) = default;

    int size() const noexcept { return heap_array.size(); }
    bool empty() const noexcept { return heap_array.empty(); }
    void clear() noexcept {
        heap_array.clear();
        std::ranges::fill(indices_map, State::PRE_HEAP);
    }

private:
    void heap_move(Difference index, Pair && p) noexcept {
        indices_map[p.first] = index;
        heap_array[static_cast<std::size_t>(index - 1)] = std::move(p);
    }

    void heap_push(Difference holeIndex, Pair && p) noexcept {
        Difference parent = holeIndex / 2;
        while(holeIndex > 1 &&
              cmp(p.second,
                  heap_array[static_cast<std::size_t>(parent - 1)].second)) {
            heap_move(
                holeIndex,
                std::move(heap_array[static_cast<std::size_t>(parent - 1)]));
            holeIndex = parent;
            parent = holeIndex / 2;
        }
        heap_move(holeIndex, std::move(p));
    }

    void adjust_heap(Difference holeIndex, const Difference len,
                     Pair && p) noexcept {
        Difference child = 2 * holeIndex;
        while(child < len) {
            child +=
                cmp(heap_array[static_cast<std::size_t>(child)].second,
                    heap_array[static_cast<std::size_t>(child - 1)].second);
            if(!cmp(heap_array[static_cast<std::size_t>(child - 1)].second,
                    p.second)) {
                return heap_move(holeIndex, std::move(p));
            }
            heap_move(
                holeIndex,
                std::move(heap_array[static_cast<std::size_t>(child - 1)]));
            holeIndex = child;
            child = 2 * holeIndex;
        }
        if(child < len &&
           cmp(heap_array[static_cast<std::size_t>(child - 1)].second,
               p.second)) {
            heap_move(
                holeIndex,
                std::move(heap_array[static_cast<std::size_t>(child - 1)]));
            holeIndex = child;
        }
        heap_move(holeIndex, std::move(p));
    }

public:
    void push(Pair && p) noexcept {
        heap_array.emplace_back();
        heap_push(Difference(heap_array.size()), std::move(p));
    }
    void push(const Node i, const Prio p) noexcept { push(Pair(i, p)); }
    bool contains(const Node u) const noexcept { return indices_map[u] > 0; }
    Prio prio(const Node u) const noexcept {
        return heap_array[static_cast<std::size_t>(indices_map[u] - 1)].second;
    }
    Pair top() const noexcept { return heap_array.front(); }
    Pair pop() noexcept {
        assert(!heap_array.empty());
        const Difference n = Difference(heap_array.size());
        Pair p = heap_array.front();
        indices_map[p.first] = POST_HEAP;
        if(n > 1) adjust_heap(Difference(1), n, std::move(heap_array.back()));
        heap_array.pop_back();
        return p;
    }
    void decrease(const Node & u, const Prio & p) noexcept {
        heap_push(indices_map[u], Pair(u, p));
    }
    State state(const Node & u) const noexcept {
        return State(std::min(indices_map[u], Difference(1)));
    }
};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_BINARY_HEAP_HPP
