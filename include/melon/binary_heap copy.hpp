#ifndef LEMON_MY_BIN_HEAP_H
#define LEMON_MY_BIN_HEAP_H

#include <functional>
#include <utility>
#include <vector>

namespace lemon {

template <typename PR, typename IM, typename CMP = std::less<PR> >
class MyBinHeap {
public:
    /// Type of the item-int map.
    typedef IM ItemIntMap;
    /// Type of the priorities.
    typedef PR Prio;
    /// Type of the items stored in the heap.
    typedef typename ItemIntMap::Key Item;
    /// Type of the item-priority pairs.
    typedef std::pair<Item, Prio> Pair;
    /// Functor type for comparing the priorities.
    typedef CMP Compare;

    /// \brief Type to represent the states of the items.
    ///
    /// Each item has a state associated to it. It can be "in heap",
    /// "pre-heap" or "post-heap". The latter two are indifferent from the
    /// heap's point of view, but may be useful to the user.
    ///
    /// The item-int map must be initialized in such way that it assigns
    /// \c PRE_HEAP (<tt>-1</tt>) to any element to be put in the heap.
    enum State {
        IN_HEAP = 0,    ///< = 0.
        PRE_HEAP = -1,  ///< = -1.
        POST_HEAP = -2  ///< = -2.
    };

private:
    std::vector<Pair> _data;
    Compare _comp;
    ItemIntMap & _iim;

public:
    explicit MyBinHeap(ItemIntMap & map) : _iim(map) {}

    MyBinHeap(ItemIntMap & map, const Compare & comp)
        : _iim(map), _comp(comp) {}

    int size() const { return _data.size(); }
    bool empty() const { return _data.empty(); }
    void clear() { _data.clear(); }

private:
    static int parent(unsigned int i) { return (i - 1) / 2; }
    static int secondChild(unsigned int i) { return (i + 1) * 2; }

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
    /// \brief Insert a pair of item and priority into the heap.
    ///
    /// This function inserts \c p.first to the heap with priority
    /// \c p.second.
    /// \param p The pair to insert.
    /// \pre \c p.first must not be stored in the heap.
    void push(const Pair & p) {
        const int n = _data.size();
        _data.resize(n + 1);
        bubbleUp(n, p);
    }

    /// \brief Insert an item into the heap with the given priority.
    ///
    /// This function inserts the given item into the heap with the
    /// given priority.
    /// \param i The item to insert.
    /// \param p The priority of the item.
    /// \pre \e i must not be stored in the heap.
    void push(const Item & i, const Prio & p) { push(Pair(i, p)); }

    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Pair p_top() const { return _data[0]; }
    
    /// \brief Return the item having minimum priority.
    ///
    /// This function returns the item having minimum priority.
    /// \pre The heap must be non-empty.
    Item top() const { return _data[0].first; }

    /// \brief The minimum priority.
    ///
    /// This function returns the minimum priority.
    /// \pre The heap must be non-empty.
    Prio prio() const { return _data[0].second; }

    /// \brief Remove the item having minimum priority.
    ///
    /// This function removes the item having minimum priority.
    /// \pre The heap must be non-empty.
    void pop() {
        const int n = _data.size() - 1;
        _iim.set(_data[0].first, POST_HEAP);
        if(n == 0) {
            _data.pop_back();
            return;
        }
        bubbleDown(0, _data[n], n);
        _data.pop_back();
    }

    /// \brief Remove the given item from the heap.
    ///
    /// This function removes the given item from the heap if it is
    /// already stored.
    /// \param i The item to delete.
    /// \pre \e i must be in the heap.
    void erase(const Item & i) {
        const int h = _iim[i];
        const int n = _data.size() - 1;
        _iim.set(_data[h].first, POST_HEAP);
        if(h < n) {
            if(bubbleUp(h, _data[n]) == h) {
                bubbleDown(h, _data[n], n);
            }
        }
        _data.pop_back();
    }

    /// \brief The priority of the given item.
    ///
    /// This function returns the priority of the given item.
    /// \param i The item.
    /// \pre \e i must be in the heap.
    Prio operator[](const Item & i) const {
        const int idx = _iim[i];
        return _data[idx].second;
    }

    /// \brief Set the priority of an item or insert it, if it is
    /// not stored in the heap.
    ///
    /// This method sets the priority of the given item if it is
    /// already stored in the heap. Otherwise it inserts the given
    /// item into the heap with the given priority.
    /// \param i The item.
    /// \param p The priority.
    void set(const Item & i, const Prio & p) {
        const int idx = _iim[i];
        if(idx < 0) {
            push(i, p);
            return;
        }
        if(_comp(p, _data[idx].second)) {
            bubbleUp(idx, Pair(i, p));
            return;
        }
        bubbleDown(idx, Pair(i, p), _data.size());
    }

    /// \brief Decrease the priority of an item to the given value.
    ///
    /// This function decreases the priority of an item to the given value.
    /// \param i The item.
    /// \param p The priority.
    /// \pre \e i must be stored in the heap with priority at least \e p.
    void decrease(const Item & i, const Prio & p) {
        const int idx = _iim[i];
        bubbleUp(idx, Pair(i, p));
    }

    /// \brief Increase the priority of an item to the given value.
    ///
    /// This function increases the priority of an item to the given value.
    /// \param i The item.
    /// \param p The priority.
    /// \pre \e i must be stored in the heap with priority at most \e p.
    void increase(const Item & i, const Prio & p) {
        const int idx = _iim[i];
        bubbleDown(idx, Pair(i, p), _data.size());
    }

    /// \brief Return the state of an item.
    ///
    /// This method returns \c PRE_HEAP if the given item has never
    /// been in the heap, \c IN_HEAP if it is in the heap at the moment,
    /// and \c POST_HEAP otherwise.
    /// In the latter case it is possible that the item will get back
    /// to the heap again.
    /// \param i The item.
    State state(const Item & i) const {
        const int s = _iim[i];
        return State(std::min(s, 0));
    }

    /// \brief Set the state of an item in the heap.
    ///
    /// This function sets the state of the given item in the heap.
    /// It can be used to manually clear the heap when it is important
    /// to achive better time complexity.
    /// \param i The item.
    /// \param st The state. It should not be \c IN_HEAP.
    void state(const Item & i, State st) {
        if(st == IN_HEAP) return;
        if(state(i) == IN_HEAP) {
            erase(i);
        }
        _iim[i] = st;
    }

    /// \brief Replace an item in the heap.
    ///
    /// This function replaces item \c i with item \c j.
    /// Item \c i must be in the heap, while \c j must be out of the heap.
    /// After calling this method, item \c i will be out of the
    /// heap and \c j will be in the heap with the same prioriority
    /// as item \c i had before.
    void replace(const Item & i, const Item & j) {
        const int idx = _iim[i];
        _iim.set(i, _iim[j]);
        _iim.set(j, idx);
        _data[idx].first = j;
    }

};  // class BinHeap

}  // namespace lemon

#endif  // LEMON_MY_BIN_HEAP_H
