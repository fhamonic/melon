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

    void __heap_move(Difference index, Pair && p) {
        indices_map[p.first] = index;
        heap_array[index] = std::move(p);
    }

    void __push_heap(Iterator first, Difference holeIndex, Difference topIndex,
                     Pair && p) {
        Difference parent = (holeIndex - 1) / 2;
        while(holeIndex > topIndex && cmp(p.second, (first + parent)->second)) {
            __heap_move(first + holeIndex, std::move(*(first + parent)));
            holeIndex = parent;
            parent = (holeIndex - 1) / 2;
        }
        __heap_move(holeIndex, std::move(p));
    }

    void __adjust_heap(Iterator first, Difference holeIndex, Difference len,
                       Pair && p) {
        const Difference topIndex = holeIndex;
        Difference secondChild = holeIndex;
        while(secondChild < (len - 1) / 2) {
            secondChild = 2 * (secondChild + 1);
            if(cmp((first + (secondChild - 1))->second,
                   (first + secondChild)->second))
                secondChild--;
            __heap_move(holeIndex, std::move(*(first + secondChild)));
            holeIndex = secondChild;
        }
        if((len & 1) == 0 && secondChild == (len - 2) / 2) {
            secondChild = 2 * (secondChild + 1);
            __heap_move(holeIndex, std::move(*(first + (secondChild - 1))));
            holeIndex = secondChild - 1;
        }
        __push_heap(first, holeIndex, topIndex, std::move(p));
    }

public:
    void push(Pair && p) {
        auto n = heap_array.size();
        heap_array.resize(n + 1);
        /*
        bubbleUp(n, p);
        /*/
        __push_heap(heap_array.begin(), Difference(n), Difference(0),
                    std::move(p));
        //*/
    }
    void push(const Node i, const Prio p) { push(Pair(i, p)); }
    bool contains(const Node u) const { return indices_map[u] > 0; }
    Prio prio(const Node u) const { return heap_array[indices_map[u]].second; }
    Pair top() const { return heap_array[0]; }
    Pair pop() {
        assert(!heap_array.empty());
        //*
        const unsigned int n = heap_array.size() - 1;
        Pair p = heap_array[0];
        indices_map[p.first] = POST_HEAP;
        if(n > 0) {
            bubbleDown(0, heap_array[n], n);
        }
        heap_array.pop_back();
        return p;
        /*/
        Pair p = std::move(heap_array.front());
        indices_map[p.first] = POST_HEAP;
        __adjust_heap(heap_array.begin(), Difference(0),
                       Difference(heap_array.size() - 1),
        std::move(heap_array.back())); heap_array.pop_back(); return p;
        //*/
    }
    void decrease(const Node & u, const Prio & p) {
        bubbleUp(indices_map[u], Pair(u, p));
    }
    State state(const Node & u) const {
        return State(std::min(indices_map[u], Difference(0)));
    }
};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_IT_BINARY_HEAP_HPP
