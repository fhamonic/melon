#ifndef MELON_IT_BINARY_HEAP_HPP
#define MELON_IT_BINARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename ND, typename PR, typename CMP = std::less<PR>>
class ItBinaryHeap {
public:
    using Node = ND;
    using Prio = PR;
    using Compare = CMP;
    using Pair = std::pair<Node, Prio>;

    enum State { IN_HEAP = 0l, PRE_HEAP = -1l, POST_HEAP = -2l };

private:
    using Iterator = std::vector<Pair>::iterator;
    using Difference = std::vector<Pair>::difference_type;

    std::vector<Pair> heap_array;
    std::vector<Difference> indices_map;
    Compare cmp;

public:
    ItBinaryHeap(const std::size_t nb_nodes)
        : heap_array(), indices_map(nb_nodes, State::PRE_HEAP), cmp() {}

    ItBinaryHeap(const ItBinaryHeap & bin) = default;
    ItBinaryHeap(ItBinaryHeap && bin) = default;

    int size() const { return heap_array.size(); }
    bool empty() const { return heap_array.empty(); }
    void clear() {
        heap_array.clear();
        std::ranges::fill(indices_map, State::PRE_HEAP);
    }

private:
    void __heap_move(Difference index, Pair && p) {
        indices_map[p.first] = index;
        heap_array[index] = std::move(p);
    }

    void __push_heap(Difference holeIndex, Pair && p) {
        Difference parent = (holeIndex - 1) / 2;
        while(holeIndex > 0 &&
              cmp(p.second, heap_array[parent].second)) {
            __heap_move(holeIndex, std::move(heap_array[parent]));
            holeIndex = parent;
            parent = (holeIndex - 1) / 2;
        }
        __heap_move(holeIndex, std::move(p));
    }

    void __adjust_heap(Difference holeIndex, Difference len, Pair && p) {
        Difference child = 2 * (holeIndex + 1);
        while(child < len) {
            child -=
                cmp(heap_array[child - 1].second, heap_array[child].second);
            if(!cmp(heap_array[child].second, p.second)) {
                return __heap_move(holeIndex, std::move(p));
            }
            __heap_move(holeIndex, std::move(heap_array[child]));
            holeIndex = child;
            child = 2 * (holeIndex + 1);
        }
        --child;
        if(child < len && cmp(heap_array[child].second, p.second)) {
            __heap_move(holeIndex, std::move(heap_array[child]));
            holeIndex = child;
        }
        __heap_move(holeIndex, std::move(p));
    }

public:
    void push(Pair && p) {
        auto n = heap_array.size();
        heap_array.resize(n + 1);
        __push_heap(Difference(n), std::move(p));
    }
    void push(const Node i, const Prio p) { push(Pair(i, p)); }
    bool contains(const Node u) const { return indices_map[u] > 0; }
    Prio prio(const Node u) const { return heap_array[indices_map[u]].second; }
    Pair top() const { return heap_array.front(); }
    Pair pop() {
        assert(!heap_array.empty());
        const Difference n = heap_array.size() - 1;
        Pair p = heap_array.front();
        indices_map[p.first] = POST_HEAP;
        if(n > 0) __adjust_heap(Difference(0), n, std::move(heap_array.back()));
        heap_array.pop_back();
        return p;
    }
    void decrease(const Node & u, const Prio & p) {
        __push_heap(indices_map[u], Pair(u, p));
    }
    State state(const Node & u) const {
        return State(std::min(indices_map[u], Difference(0)));
    }
};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_IT_BINARY_HEAP_HPP
