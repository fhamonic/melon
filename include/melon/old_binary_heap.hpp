#ifndef MELON_OLD_BINARY_HEAP_HPP
#define MELON_OLD_BINARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename ND, typename PR, typename CMP = std::less<PR>>
class OldBinaryHeap {
public:
    using Node = ND;
    using Prio = PR;
    using Compare = CMP;
    using Pair = std::pair<Node, Prio>;

    enum State { IN_HEAP = 0, PRE_HEAP = -1, POST_HEAP = -2 };

private:
    std::vector<Pair> heap_array;
    std::vector<int> indices_map;
    Compare cmp;

public:
    OldBinaryHeap(const std::size_t nb_nodes)
        : heap_array(), indices_map(nb_nodes, State::PRE_HEAP), cmp() {}

    OldBinaryHeap(const OldBinaryHeap & bin) = default;
    OldBinaryHeap(OldBinaryHeap && bin) = default;

    int size() const { return heap_array.size(); }
    bool empty() const { return heap_array.empty(); }
    void clear() {
        heap_array.clear();
        std::ranges::fill(indices_map, State::PRE_HEAP);
    }

private:
    static int parent(const unsigned int i) { return (i - 1) / 2; }
    static int secondChild(const unsigned int i) { return (i + 1) * 2; }

    bool less(const Pair & p1, const Pair & p2) const {
        return cmp(p1.second, p2.second);
    }

    void move(const Pair & p, const unsigned int i) {
        heap_array[i] = p;
        indices_map[p.first] = i;
    }

    int bubbleUp(int hole, const Pair p) {
        int par = parent(hole);
        while(hole > 0 && less(p, heap_array[par])) {
            move(heap_array[par], hole);
            hole = par;
            par = parent(hole);
        }
        move(p, hole);
        return hole;
    }

    int bubbleDown(int hole, Pair p, const int length) {
        int child = secondChild(hole);
        while(child < length) {
            if(less(heap_array[child - 1], heap_array[child])) {
                --child;
            }
            if(!less(heap_array[child], p)) {
                move(p, hole);
                return hole;
            }
            move(heap_array[child], hole);
            hole = child;
            child = secondChild(hole);
        }
        --child;
        if(child < length && less(heap_array[child], p)) {
            move(heap_array[child], hole);
            hole = child;
        }
        move(p, hole);
        return hole;
    }

public:
    void push(const Pair & p) {
        const int n = heap_array.size();
        heap_array.resize(n + 1);
        bubbleUp(n, p);
    }
    void push(const Node i, const Prio p) { push(Pair(i, p)); }
    bool contains(const Node u) const { return indices_map[u] > 0; }
    Prio prio(const Node u) const { return heap_array[indices_map[u]].second; }
    Pair top() const { return heap_array[0]; }
    Pair pop() {
        assert(!heap_array.empty());
        const unsigned int n = heap_array.size() - 1;
        Pair p = heap_array[0];
        indices_map[p.first] = POST_HEAP;
        if(n > 0) {
            bubbleDown(0, heap_array[n], n);
        }
        heap_array.pop_back();
        return p;
    }
    void decrease(const Node & u, const Prio & p) {
        bubbleUp(indices_map[u], Pair(u, p));
    }
    State state(const Node & u) const {
        return State(std::min(indices_map[u], 0));
    }
};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELOOLD_N_BINARY_HEAP_HPP