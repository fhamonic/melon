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

    enum State { PRE_HEAP = -1, POST_HEAP = -2 };

private:
    std::vector<Pair> heap;
    std::vector<int> indices_map;
    Compare _comp;

public:
    BinaryHeap(const std::size_t nb_nodes)
        : heap(), indices_map(nb_nodes, State::PRE_HEAP) {}

    BinaryHeap(const BinaryHeap & bin) = default;
    BinaryHeap(BinaryHeap && bin) = default;

    int size() const { return heap.size(); }
    bool empty() const { return heap.empty(); }
    bool contains(Node u) const { return indices_map[u] > 0; }
    void clear() { 
        heap.clear();
        std::ranges::fill(indices_map, State::PRE_HEAP);
    }

private:
    static int parent(const std::size_t i) { return (i - 1) / 2; }
    static int secondChild(const std::size_t i) { return (i + 1) * 2; }

    bool less(const Pair & p1, const Pair & p2) const {
        return _comp(p1.second, p2.second);
    }

    int bubbleUp(int hole, Pair p) {
        int par = parent(hole);
        while(hole > 0 && less(p, _data[par])) {
            move(_data[par], hole);
            hole = par;
            par = parent(hole);
        }
        move(p, hole);
        return hole;
    }

    int bubbleDown(int hole, Pair p, int length) {
        int child = secondChild(hole);
        while(child < length) {
            if(less(_data[child - 1], _data[child])) {
                --child;
            }
            if(!less(_data[child], p)) {
                move(p, hole);
                return hole;
            }
            move(_data[child], hole);
            hole = child;
            child = secondChild(hole);
        }
        --child;
        if(child < length && less(_data[child], p)) {
            move(_data[child], hole);
            hole = child;
        }
        move(p, hole);
        return hole;
    }

    void move(const Pair & p, int i) {
        _data[i] = p;
        _iim.set(p.first, i);
    }

public:
    void push(const Pair & p) {
        const int n = _data.size();
        _data.resize(n + 1);
        bubbleUp(n, p);
    }
    void push(const Item & i, const Prio & p) { push(Pair(i, p)); }
    Pair top() const { return _data[0]; }
    Pair pop() {
        Pair p = _data[0];
        const int n = _data.size() - 1;
        _iim.set(_data[0].first, POST_HEAP);
        if(n == 0) {
            _data.pop_back();
            return;
        }
        bubbleDown(0, _data[n], n);
        _data.pop_back();
        return p;
    }
    // void erase(const Item & i) {
    //     const int h = _iim[i];
    //     const int n = _data.size() - 1;
    //     _iim.set(_data[h].first, POST_HEAP);
    //     if(h < n) {
    //         if(bubbleUp(h, _data[n]) == h) {
    //             bubbleDown(h, _data[n], n);
    //         }
    //     }
    //     _data.pop_back();
    // }
    Prio operator[](const Item & i) const {
        const int idx = _iim[i];
        return _data[idx].second;
    }
    // void set(const Item & i, const Prio & p) {
    //     const int idx = _iim[i];
    //     if(idx < 0) {
    //         push(i, p);
    //         return;
    //     }
    //     if(_comp(p, _data[idx].second)) {
    //         bubbleUp(idx, Pair(i, p));
    //         return;
    //     }
    //     bubbleDown(idx, Pair(i, p), _data.size());
    // }
    void decrease(const Item & i, const Prio & p) {
        const int idx = _iim[i];
        bubbleUp(idx, Pair(i, p));
    }
    // void increase(const Item & i, const Prio & p) {
    //     const int idx = _iim[i];
    //     bubbleDown(idx, Pair(i, p), _data.size());
    // }

    State state(const Node u) const {
        return State(std::min(indices_map[u], 0));
    }

    void replace(const Item & i, const Item & j) {
        const int idx = _iim[i];
        _iim.set(i, _iim[j]);
        _iim.set(j, idx);
        _data[idx].first = j;
    }

};  // class BinHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // LEMON_MY_BIN_HEAP_H
