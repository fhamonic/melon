#ifndef LEMON_MY_BIN_HEAP_H
#define LEMON_MY_BIN_HEAP_H

#include <algorithm>
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

    enum State { IN_HEAP = 0, PRE_HEAP = -1, POST_HEAP = -2 };

private:
    std::vector<Pair> heap_array;
    std::vector<int> indices_map;
    Compare cmp;

public:
    BinaryHeap(const unsigned int nb_nodes)
        : heap(), indices_map(nb_nodes, State::PRE_HEAP) {}

    BinaryHeap(const BinaryHeap & bin) = default;
    BinaryHeap(BinaryHeap && bin) = default;

    int size() const { return heap_array.size(); }
    bool empty() const { return heap_array.empty(); }
    bool contains(Node u) const { return indices_map[u] > 0; }
    void clear() { 
        heap.clear();
        std::ranges::fill(indices_map, State::PRE_HEAP);
    }

private:
    static int parent(const unsigned int i) { return (i - 1) / 2; }
    static int secondChild(const unsigned int i) { return (i + 1) * 2; }

    bool less(const Pair & p1, const Pair & p2) const {
        return cmp(p1.second, p2.second);
    }

    int bubbleUp(unsigned int hole, Pair p) {
        int par = parent(hole);
        while(hole > 0 && less(p, heap_array[par])) {
            move(heap_array[par], hole);
            hole = par;
            par = parent(hole);
        }
        move(p, hole);
        return hole;
    }

    int bubbleDown(unsigned int hole, Pair p, unsigned int length) {
        unsigned int child = secondChild(hole);
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

    void move(const Pair & p, unsigned int i) {
        heap_array[i] = p;
        indices_map.set(p.first, i);
    }

public:
    void push(const Pair & p) {
        const int n = heap_array.size();
        heap_array.resize(n + 1);
        bubbleUp(n, p);
    }
    void push(const Node i, const Prio p) { push(Pair(i, p)); }
    Pair top() const { return heap_array[0]; }
    Pair pop() {
        Pair p = heap_array[0];
        const unsigned int n = heap_array.size() - 1;
        indices_map[p.first] = POST_HEAP;
        if(n == 0) {
            heap_array.pop_back();
            return;
        }
        bubbleDown(0, heap_array[n], n);
        heap_array.pop_back();
        return p;
    }
    // void erase(const Item & i) {
    //     const int h = indices_map[i];
    //     const int n = heap_array.size() - 1;
    //     indices_map.set(heap_array[h].first, POST_HEAP);
    //     if(h < n) {
    //         if(bubbleUp(h, heap_array[n]) == h) {
    //             bubbleDown(h, heap_array[n], n);
    //         }
    //     }
    //     heap_array.pop_back();
    // }
    // void set(const Item & i, const Prio & p) {
    //     const int idx = indices_map[i];
    //     if(idx < 0) {
    //         push(i, p);
    //         return;
    //     }
    //     if(_comp(p, heap_array[idx].second)) {
    //         bubbleUp(idx, Pair(i, p));
    //         return;
    //     }
    //     bubbleDown(idx, Pair(i, p), heap_array.size());
    // }
    void decrease(const Node u, const Prio p) {
        bubbleUp(indices_map[u], Pair(u, p));
    }
    // void increase(const Item & i, const Prio & p) {
    //     const int idx = indices_map[i];
    //     bubbleDown(idx, Pair(i, p), heap_array.size());
    // }

    State state(const Node u) const {
        return State(std::min(indices_map[u], 0));
    }

    // void replace(const Item & i, const Item & j) {
    //     const int idx = indices_map[i];
    //     indices_map.set(i, indices_map[j]);
    //     indices_map.set(j, idx);
    //     heap_array[idx].first = j;
    // }

};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // LEMON_MY_BIN_HEAP_H
