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
class fast_binary_heap {
public:
    using vertex_t = ND;
    using priority_t = PR;
    using Compare = CMP;
    using entry = std::pair<vertex_t, priority_t>;

private:
    using index_t = std::size_t;

public:
    static_assert(sizeof(entry) >= 2, "std::pair<vertex_t, priority_t> is too small");
    enum State : index_t {
        PRE_HEAP = static_cast<index_t>(0),
        POST_HEAP = static_cast<index_t>(1),
        IN_HEAP = static_cast<index_t>(2)
    };

    std::vector<entry> _heap_array;
    std::vector<index_t> _indices_map;
    Compare _cmp;

public:
    fast_binary_heap(const std::size_t nb_vertices)
        : _heap_array(1), _indices_map(nb_vertices, State::PRE_HEAP), _cmp() {}

    fast_binary_heap(const fast_binary_heap & bin) = default;
    fast_binary_heap(fast_binary_heap && bin) = default;

    index_t size() const noexcept { return _heap_array.size() - 1; }
    auto entries() const noexcept {
        return std::ranges::views::drop(_heap_array, 1);
    }
    bool empty() const noexcept { return size() == 0; }
    void clear() noexcept {
        _heap_array.resize(1);
        std::ranges::fill(_indices_map, State::PRE_HEAP);
    }

private:
    constexpr entry & pair_ref(index_t i) {
        return *(reinterpret_cast<entry *>(
            reinterpret_cast<std::byte *>(_heap_array.data()) + i));
    }
    constexpr const entry & pair_ref(index_t i) const {
        return *(reinterpret_cast<const entry *>(
            reinterpret_cast<const std::byte *>(_heap_array.data()) + i));
    }

    void heap_move(index_t index, entry && p) noexcept {
        _indices_map[p.first] = index;
        pair_ref(index) = std::move(p);
    }

    void heap_push(index_t holeIndex, entry && p) noexcept {
        while(holeIndex > sizeof(entry)) {
            const index_t parent = holeIndex / (2 * sizeof(entry)) * sizeof(entry);
            if(!_cmp(p.second, pair_ref(parent).second)) break;
            heap_move(holeIndex, std::move(pair_ref(parent)));
            holeIndex = parent;
        }
        heap_move(holeIndex, std::move(p));
    }

    void adjust_heap(index_t holeIndex, const index_t end, entry && p) noexcept {
        index_t child = 2 * holeIndex;
        while(child < end) {
            child += sizeof(entry) * _cmp(pair_ref(child + sizeof(entry)).second,
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
    void push(entry && p) noexcept {
        _heap_array.emplace_back();
        heap_push(static_cast<index_t>(size() * sizeof(entry)), std::move(p));
    }
    void push(const vertex_t i, const priority_t p) noexcept { push(entry(i, p)); }
    bool contains(const vertex_t u) const noexcept {
        return _indices_map[u] > 0;
    }
    priority_t prio(const vertex_t u) const noexcept {
        return pair_ref(_indices_map[u]).second;
    }
    entry top() const noexcept {
        assert(!empty());
        return _heap_array[1];
    }
    void pop() noexcept {
        assert(!empty());
        const index_t n = _heap_array.size() - 1;
        _indices_map[_heap_array[1].first] = POST_HEAP;
        if(n > 1)
            adjust_heap(index_t{sizeof(entry)}, n * sizeof(entry),
                        std::move(_heap_array.back()));
        _heap_array.pop_back();
    }
    void decrease(const vertex_t & u, const priority_t & p) noexcept {
        heap_push(_indices_map[u], entry(u, p));
    }
    State state(const vertex_t & u) const noexcept {
        return State(std::min(_indices_map[u], static_cast<index_t>(IN_HEAP)));
    }
    void discard(const vertex_t & u) noexcept {
        assert(_indices_map[u] < sizeof(entry));
        _indices_map[u] = POST_HEAP;
    }
};  // class fast_binary_heap

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_FAST_BINARY_HEAP_HPP