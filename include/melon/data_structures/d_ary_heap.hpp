#ifndef MELON_D_ARY_HEAP_HPP
#define MELON_D_ARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fhamonic {
namespace melon {

template <int D, typename K, typename P, typename C = std::greater<P>,
          typename M = std::unordered_map<K, std::size_t>>
class d_ary_heap {
public:
    using key_type = K;
    using priority_type = P;
    using entry = std::pair<key_type, priority_type>;

private:
    using size_type = std::size_t;
    using indice_map = M;

public:
    std::vector<entry> _heap_array;
    indice_map _indices_map;
    C _cmp;

public:
    template <typename... indice_map_args>
    explicit d_ary_heap(indice_map_args &&... args)
        : _heap_array()
        , _indices_map(std::forward<indice_map_args>(args)...)
        , _cmp() {}

    d_ary_heap(const d_ary_heap & bin) = default;
    d_ary_heap(d_ary_heap && bin) = default;
    
    d_ary_heap & operator=(const d_ary_heap &) = default;
    d_ary_heap & operator=(d_ary_heap &&) = default;

    size_type size() const noexcept { return _heap_array.size(); }
    bool empty() const noexcept { return _heap_array.empty(); }
    void clear() noexcept { _heap_array.resize(0); }

private:
    static constexpr size_type parent_of(const size_type i) noexcept {
        return (i - sizeof(entry)) / (sizeof(entry) * D) * sizeof(entry);
    }
    static constexpr size_type first_child_of(const size_type i) noexcept {
        return i * D + sizeof(entry);
    }
    template <int I = D>
    constexpr size_type minimum_child(
        const size_type first_child) const noexcept {
        if constexpr(I == 1)
            return first_child;
        else if constexpr(I == 2)
            return first_child +
                   sizeof(entry) *
                       _cmp(pair_ref(first_child + sizeof(entry)).second,
                            pair_ref(first_child).second);
        else {
            const size_type first_half_minimum =
                minimum_child<I / 2>(first_child);
            const size_type second_half_minimum =
                minimum_child<I - I / 2>(first_child + (I / 2) * sizeof(entry));
            return _cmp(pair_ref(second_half_minimum).second,
                        pair_ref(first_half_minimum).second)
                       ? second_half_minimum
                       : first_half_minimum;
        }
    }
    constexpr size_type minimum_remaining_child(
        const size_type first_child,
        const size_type nb_children) const noexcept {
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
                    const size_type half = nb_children / 2;
                    const size_type first_half_minimum =
                        minimum_remaining_child(first_child, half);
                    const size_type second_half_minimum =
                        minimum_remaining_child(
                            first_child + half * sizeof(entry),
                            nb_children - half);
                    return _cmp(pair_ref(second_half_minimum).second,
                                pair_ref(first_half_minimum).second)
                               ? second_half_minimum
                               : first_half_minimum;
            }
        }
    }

    constexpr entry & pair_ref(const size_type i) noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        return *(reinterpret_cast<entry *>(
            reinterpret_cast<std::byte *>(_heap_array.data()) + i));
    }
    constexpr const entry & pair_ref(const size_type i) const noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        return *(reinterpret_cast<const entry *>(
            reinterpret_cast<const std::byte *>(_heap_array.data()) + i));
    }
    void heap_move(const size_type i, entry && p) noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        _indices_map[p.first] = i;
        pair_ref(i) = std::move(p);
    }
    void heap_push(size_type hole_index, entry && p) noexcept {
        while(hole_index > 0) {
            const size_type parent = parent_of(hole_index);
            if(!_cmp(p.second, pair_ref(parent).second)) break;
            heap_move(hole_index, std::move(pair_ref(parent)));
            hole_index = parent;
        }
        heap_move(hole_index, std::move(p));
    }
    void adjust_heap(size_type hole_index, const size_type & end,
                     entry && p) noexcept {
        size_type child_end;
        if constexpr(D > 2)
            child_end =
                end > D * sizeof(entry) ? end - (D - 1) * sizeof(entry) : 0;
        else
            child_end = end - (D - 1) * sizeof(entry);
        size_type child = first_child_of(hole_index);
        while(child < child_end) {
            child = minimum_child(child);
            if(_cmp(pair_ref(child).second, p.second)) {
                heap_move(hole_index, std::move(pair_ref(child)));
                hole_index = child;
                child = first_child_of(child);
                continue;
            }
            goto ok;
        }
        if(child < end) {
            child =
                minimum_remaining_child(child, (end - child) / sizeof(entry));
            if(_cmp(pair_ref(child).second, p.second)) {
                heap_move(hole_index, std::move(pair_ref(child)));
                hole_index = child;
            }
        }
    ok:
        heap_move(hole_index, std::move(p));
    }
    size_type index_of(const key_type & k) const noexcept {
        if constexpr(requires() { std::as_const(_indices_map)[k]; })
            return _indices_map[k];
        else
            return _indices_map.at(k);
    }
    void push(entry && p) noexcept {
        const size_type n = _heap_array.size();
        _heap_array.emplace_back();
        heap_push(size_type(n * sizeof(entry)), std::move(p));
    }

public:
    void push(const key_type & k, const priority_type & p) noexcept {
        push(entry(k, p));
    }
    priority_type priority(const key_type & k) const noexcept {
        return pair_ref(index_of(k)).second;
    }
    bool contains(const key_type & k) const noexcept {
        const size_type i = index_of(k);
        if(i >= _heap_array.size()) return false;
        return _heap_array[i].first == k;
    }
    entry top() const noexcept {
        assert(!_heap_array.empty());
        return _heap_array.front();
    }
    void pop() noexcept {
        assert(!_heap_array.empty());
        const size_type n = _heap_array.size() - 1;
        if(n > 0)
            adjust_heap(size_type(0), n * sizeof(entry),
                        std::move(_heap_array.back()));
        _heap_array.pop_back();
    }
    void promote(const key_type & k, const priority_type & p) noexcept {
        assert(_cmp(p, priority(k)));
        heap_push(_indices_map[k], entry(k, p));
    }
};  // class d_ary_heap

template <typename K, typename P, typename C = std::less<P>,
          typename M = std::unordered_map<K, std::size_t>>
using binary_heap = d_ary_heap<2, K, P, C, M>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_D_ARY_HEAP_HPP