#ifndef MELON_D_ARY_HEAP_HPP
#define MELON_D_ARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <int D, typename K, typename P, typename C = std::less<P>, typename M = std::map<K, std::size_t>>
class d_ary_heap {
public:
    using key_t = K;
    using priority_t = P;
    using entry = std::pair<key_t, priority_t>;

private:
    using index_t = std::size_t;
    using indice_map = M;

public:
    enum State : char { PRE_HEAP = 0, IN_HEAP = 1, POST_HEAP = 2 };

    std::vector<entry> _heap_array;
    std::vector<index_t> _indices_map;
    std::vector<State> _states_map;
    C _cmp;

public:
    d_ary_heap(const std::size_t nb_vertices)
        : _heap_array()
        , _indices_map(nb_vertices)
        , _states_map(nb_vertices, State::PRE_HEAP)
        , _cmp() {}

    d_ary_heap(const d_ary_heap & bin) = default;
    d_ary_heap(d_ary_heap && bin) = default;

    index_t size() const noexcept { return _heap_array.size(); }
    bool empty() const noexcept { return _heap_array.empty(); }
    void clear() noexcept {
        _heap_array.resize(0);
        std::ranges::fill(_states_map, State::PRE_HEAP);
    }

private:
    static constexpr index_t parent_of(index_t i) {
        return (i - sizeof(entry)) / (sizeof(entry) * D) * sizeof(entry);
    }
    static constexpr index_t first_child_of(index_t i) {
        return i * D + sizeof(entry);
    }
    template <int I = D>
    constexpr index_t minimum_child(const index_t first_child) const {
        if constexpr(I == 1)
            return first_child;
        else if constexpr(I == 2)
            return first_child +
                   sizeof(entry) *
                       _cmp(pair_ref(first_child + sizeof(entry)).second,
                            pair_ref(first_child).second);
        else {
            const index_t first_half_minimum =
                minimum_child<I / 2>(first_child);
            const index_t second_half_minimum =
                minimum_child<I - I / 2>(first_child + (I / 2) * sizeof(entry));
            return _cmp(pair_ref(second_half_minimum).second,
                        pair_ref(first_half_minimum).second)
                       ? second_half_minimum
                       : first_half_minimum;
        }
    }
    constexpr index_t minimum_remaining_child(const index_t first_child,
                                              const index_t nb_children) const {
        if constexpr(D == 2)
            return first_child;
        else if constexpr(D == 4) {
            switch(nb_children) {
                case 1:
                    return minimum_child<1>(first_child);
                case 2:
                    return minimum_child<2>(first_child);
                default:
                    return minimum_child<3>(first_child);
            }
        } else {
            switch(nb_children) {
                case 1:
                    return minimum_child<1>(first_child);
                case 2:
                    return minimum_child<2>(first_child);
                default:
                    const index_t half = nb_children / 2;
                    const index_t first_half_minimum =
                        minimum_remaining_child(first_child, half);
                    const index_t second_half_minimum = minimum_remaining_child(
                        first_child + half * sizeof(entry), nb_children - half);
                    return _cmp(pair_ref(second_half_minimum).second,
                                pair_ref(first_half_minimum).second)
                               ? second_half_minimum
                               : first_half_minimum;
            }
        }
    }

    constexpr entry & pair_ref(index_t i) {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        return *(reinterpret_cast<entry *>(
            reinterpret_cast<std::byte *>(_heap_array.data()) + i));
    }
    constexpr const entry & pair_ref(index_t i) const {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        return *(reinterpret_cast<const entry *>(
            reinterpret_cast<const std::byte *>(_heap_array.data()) + i));
    }
    void heap_move(index_t i, entry && p) noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        _indices_map[p.first] = i;
        pair_ref(i) = std::move(p);
    }

    void heap_push(index_t holeIndex, entry && p) noexcept {
        while(holeIndex > 0) {
            const index_t parent = parent_of(holeIndex);
            if(!_cmp(p.second, pair_ref(parent).second)) break;
            heap_move(holeIndex, std::move(pair_ref(parent)));
            holeIndex = parent;
        }
        heap_move(holeIndex, std::move(p));
    }

    void adjust_heap(index_t holeIndex, const index_t end,
                     entry && p) noexcept {
        index_t child_end;
        if constexpr(D > 2)
            child_end =
                end > D * sizeof(entry) ? end - (D - 1) * sizeof(entry) : 0;
        else
            child_end = end - (D - 1) * sizeof(entry);
        index_t child = first_child_of(holeIndex);
        while(child < child_end) {
            child = minimum_child(child);
            if(_cmp(pair_ref(child).second, p.second)) {
                heap_move(holeIndex, std::move(pair_ref(child)));
                holeIndex = child;
                child = first_child_of(child);
                continue;
            }
            goto ok;
        }
        if(child < end) {
            child =
                minimum_remaining_child(child, (end - child) / sizeof(entry));
            if(_cmp(pair_ref(child).second, p.second)) {
                heap_move(holeIndex, std::move(pair_ref(child)));
                holeIndex = child;
            }
        }
    ok:
        heap_move(holeIndex, std::move(p));
    }

public:
    void push(entry && p) noexcept {
        const index_t n = _heap_array.size();
        _heap_array.emplace_back();
        _states_map[p.first] = IN_HEAP;
        heap_push(index_t(n * sizeof(entry)), std::move(p));
    }
    void push(const key_t i, const priority_t p) noexcept { push(entry(i, p)); }
    priority_t priority(const key_t u) const noexcept {
        return pair_ref(_indices_map[u]).second;
    }
    entry top() const noexcept {
        assert(!_heap_array.empty());
        return _heap_array.front();
    }
    void pop() noexcept {
        assert(!_heap_array.empty());
        _states_map[_heap_array.front().first] = POST_HEAP;
        const index_t n = _heap_array.size() - 1;
        if(n > 0)
            adjust_heap(index_t(0), n * sizeof(entry),
                        std::move(_heap_array.back()));
        _heap_array.pop_back();
    }
    void decrease(const key_t & u, const priority_t & p) noexcept {
        heap_push(_indices_map[u], entry(u, p));
    }
    State state(const key_t & u) const noexcept { return _states_map[u]; }
};  // class d_ary_heap

template <typename K, typename P, typename C = std::less<P>>
using binary_heap = d_ary_heap<2, K, P, C>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_D_ARY_HEAP_HPP