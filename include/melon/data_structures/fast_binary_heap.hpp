#ifndef MELON_FAST_BINARY_HEAP_HPP
#define MELON_FAST_BINARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <ranges>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <typename ND, typename PR, typename CMP = std::less<PR>>
class FastBinaryHeap {
public:
    using vertex_t = ND;
    using Prio = PR;
    using Compare = CMP;
    using Pair = std::pair<vertex_t, Prio>;

private:
    using Index = std::vector<Pair>::size_type;

public:
    static_assert(sizeof(Pair) >= 2, "std::pair<vertex_t, Prio> is too small");
    enum State : Index {
        PRE_HEAP = static_cast<Index>(0),
        POST_HEAP = static_cast<Index>(1),
        IN_HEAP = static_cast<Index>(2)
    };

    std::vector<Pair> _heap_array;
    std::vector<Index> _indices_map;
    Compare _cmp;

public:
    FastBinaryHeap(const std::size_t nb_vertices)
        : _heap_array(1), _indices_map(nb_vertices, State::PRE_HEAP), _cmp() {}

    FastBinaryHeap(const FastBinaryHeap & bin) = default;
    FastBinaryHeap(FastBinaryHeap && bin) = default;

    Index size() const noexcept { return _heap_array.size() - 1; }
    auto entries() const noexcept {
        return std::ranges::views::drop(_heap_array, 1);
    }
    bool empty() const noexcept { return size() == 0; }
    void clear() noexcept {
        _heap_array.resize(1);
        std::ranges::fill(_indices_map, State::PRE_HEAP);
    }

private:
    constexpr Pair & pair_ref(Index i) {
        return *(reinterpret_cast<Pair *>(
            reinterpret_cast<std::byte *>(_heap_array.data()) + i));
    }
    constexpr const Pair & pair_ref(Index i) const {
        return *(reinterpret_cast<const Pair *>(
            reinterpret_cast<const std::byte *>(_heap_array.data()) + i));
    }

    void heap_move(Index index, Pair && p) noexcept {
        _indices_map[p.first] = index;
        pair_ref(index) = std::move(p);
    }

    void heap_push(Index holeIndex, Pair && p) noexcept {
        while(holeIndex > sizeof(Pair)) {
            const Index parent = holeIndex / (2 * sizeof(Pair)) * sizeof(Pair);
            if(!_cmp(p.second, pair_ref(parent).second)) break;
            heap_move(holeIndex, std::move(pair_ref(parent)));
            holeIndex = parent;
        }
        heap_move(holeIndex, std::move(p));
    }

    void adjust_heap(Index holeIndex, const Index end, Pair && p) noexcept {
        Index child = 2 * holeIndex;
        while(child < end) {
            child += sizeof(Pair) * _cmp(pair_ref(child + sizeof(Pair)).second,
                                         pair_ref(child).second);
            if(_cmp(pair_ref(child).second, p.second)) {
                heap_move(holeIndex, std::move(pair_ref(child)));
                holeIndex = child;
                child = 2 * holeIndex;
                continue;
            }
            goto ok;
        }
        if(child == end && _cmp(pair_ref(child).second, p.second)) {
            heap_move(holeIndex, std::move(pair_ref(child)));
            holeIndex = child;
        }
    ok:
        heap_move(holeIndex, std::move(p));
    }

public:
    void push(Pair && p) noexcept {
        _heap_array.emplace_back();
        heap_push(static_cast<Index>(size() * sizeof(Pair)), std::move(p));
    }
    void push(const vertex_t i, const Prio p) noexcept { push(Pair(i, p)); }
    bool contains(const vertex_t u) const noexcept {
        return _indices_map[u] > 0;
    }
    Prio prio(const vertex_t u) const noexcept {
        return pair_ref(_indices_map[u]).second;
    }
    Pair top() const noexcept {
        assert(!empty());
        return _heap_array[1];
    }
    void pop() noexcept {
        assert(!empty());
        const Index n = _heap_array.size() - 1;
        _indices_map[_heap_array[1].first] = POST_HEAP;
        if(n > 1)
            adjust_heap(static_cast<Index>(sizeof(Pair)), n * sizeof(Pair),
                        std::move(_heap_array.back()));
        _heap_array.pop_back();
    }
    void decrease(const vertex_t & u, const Prio & p) noexcept {
        heap_push(_indices_map[u], Pair(u, p));
    }
    State state(const vertex_t & u) const noexcept {
        return State(std::min(_indices_map[u], static_cast<Index>(IN_HEAP)));
    }
    void discard(const vertex_t & u) const noexcept {
        assert(_indices_map[u] < sizeof(Pair));
        _indices_map[u] = POST_HEAP;
    }
};  // class FastBinaryHeap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_FAST_BINARY_HEAP_HPP