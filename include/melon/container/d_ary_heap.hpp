#ifndef MELON_D_ARY_HEAP_HPP
#define MELON_D_ARY_HEAP_HPP

#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "melon/mapping.hpp"

namespace fhamonic {
namespace melon {

template <std::size_t D, typename _Entry,
          input_mapping<_Entry> _EntryPriorityMap = views::identity_map,
          std::strict_weak_order<mapped_value_t<_EntryPriorityMap, _Entry>,
                                 mapped_value_t<_EntryPriorityMap, _Entry>>
              _PriorityComparator =
                  std::greater<mapped_value_t<_EntryPriorityMap, _Entry>>,
          input_mapping<_Entry> _EntryIdMap = views::identity_map,
          output_mapping<mapped_value_t<_EntryIdMap, _Entry>> _IndicesMap =
              mapping_owning_view<std::unordered_map<
                  mapped_value_t<_EntryIdMap, _Entry>, std::size_t>>>
class d_ary_heap {
public:
    using entry = _Entry;
    using id_type = mapped_value_t<_EntryIdMap, _Entry>;
    using priority_type = mapped_value_t<_EntryPriorityMap, _Entry>;

private:
    using size_type = std::size_t;

    std::vector<entry> _heap_array;
    [[no_unique_address]] _EntryPriorityMap _entry_priority_map;
    [[no_unique_address]] _PriorityComparator _priority_cmp;
    [[no_unique_address]] _EntryIdMap _entry_id_map;
    [[no_unique_address]] _IndicesMap _heap_index_map;

public:
    [[nodiscard]] constexpr d_ary_heap()
        : _heap_array(), _priority_cmp(), _heap_index_map() {}

    template <typename IMA>
    [[nodiscard]] constexpr explicit d_ary_heap(IMA && indices_map_arg)
        : _heap_array()
        , _priority_cmp()
        , _heap_index_map(std::forward<IMA>(indices_map_arg)) {}

    template <typename IMA, typename CA>
    [[nodiscard]] constexpr d_ary_heap(IMA && indices_map_arg, CA && cmp_arg)
        : _heap_array()
        , _priority_cmp(std::forward<CA>(cmp_arg))
        , _heap_index_map(std::forward<IMA>(indices_map_arg)) {}

    [[nodiscard]] constexpr d_ary_heap(const d_ary_heap &) = default;
    [[nodiscard]] constexpr d_ary_heap(d_ary_heap &&) = default;

    d_ary_heap & operator=(const d_ary_heap &) = default;
    d_ary_heap & operator=(d_ary_heap &&) = default;

    [[nodiscard]] constexpr size_type size() const noexcept {
        return _heap_array.size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return _heap_array.empty();
    }
    constexpr void clear() noexcept { _heap_array.resize(0); }

private:
    [[nodiscard]] static constexpr size_type parent_of(
        const size_type i) noexcept {
        return (i - sizeof(entry)) / (sizeof(entry) * D) * sizeof(entry);
    }
    [[nodiscard]] static constexpr size_type first_child_of(
        const size_type i) noexcept {
        return i * D + sizeof(entry);
    }
    template <int I = D>
    [[nodiscard]] constexpr size_type minimum_child(
        const size_type first_child) const noexcept {
        if constexpr(I == 1)
            return first_child;
        else if constexpr(I == 2)
            return first_child +
                   sizeof(entry) *
                       _priority_cmp(
                           _entry_priority_map[entry_ref(first_child +
                                                         sizeof(entry))],
                           _entry_priority_map[entry_ref(first_child)]);
        else {
            const size_type first_half_minimum =
                minimum_child<I / 2>(first_child);
            const size_type second_half_minimum =
                minimum_child<I - I / 2>(first_child + (I / 2) * sizeof(entry));
            return _priority_cmp(
                       _entry_priority_map[entry_ref(second_half_minimum)],
                       _entry_priority_map[entry_ref(first_half_minimum)])
                       ? second_half_minimum
                       : first_half_minimum;
        }
    }
    [[nodiscard]] constexpr size_type minimum_remaining_child(
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
                    return _priority_cmp(_entry_priority_map[entry_ref(
                                             second_half_minimum)],
                                         _entry_priority_map[entry_ref(
                                             first_half_minimum)])
                               ? second_half_minimum
                               : first_half_minimum;
            }
        }
    }

    [[nodiscard]] constexpr entry & entry_ref(const size_type i) noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        return *(reinterpret_cast<entry *>(
            reinterpret_cast<std::byte *>(_heap_array.data()) + i));
    }
    [[nodiscard]] constexpr const entry & entry_ref(
        const size_type i) const noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        return *(reinterpret_cast<const entry *>(
            reinterpret_cast<const std::byte *>(_heap_array.data()) + i));
    }
    constexpr void heap_move(const size_type i, entry && p) noexcept {
        assert(0 <= (i / sizeof(entry)) &&
               (i / sizeof(entry)) < _heap_array.size());
        _heap_index_map[_entry_id_map[p]] = i;
        entry_ref(i) = std::move(p);
    }
    constexpr void heap_push(size_type hole_index, entry && p) noexcept {
        while(hole_index > 0) {
            const size_type parent = parent_of(hole_index);
            if(!_priority_cmp(_entry_priority_map[p],
                              _entry_priority_map[entry_ref(parent)]))
                break;
            heap_move(hole_index, std::move(entry_ref(parent)));
            hole_index = parent;
        }
        heap_move(hole_index, std::move(p));
    }
    // EXPECTED_CPP23 goto in constexpr functions
    void adjust_heap(size_type hole_index, const size_type end,
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
            if(_priority_cmp(_entry_priority_map[entry_ref(child)],
                             _entry_priority_map[p])) {
                heap_move(hole_index, std::move(entry_ref(child)));
                hole_index = child;
                child = first_child_of(child);
                continue;
            }
            goto ok;
        }
        if(child < end) {
            child =
                minimum_remaining_child(child, (end - child) / sizeof(entry));
            if(_priority_cmp(_entry_priority_map[entry_ref(child)],
                             _entry_priority_map[p])) {
                heap_move(hole_index, std::move(entry_ref(child)));
                hole_index = child;
            }
        }
    ok:
        heap_move(hole_index, std::move(p));
    }
    [[nodiscard]] constexpr size_type index_of(
        const id_type & k) const noexcept {
        return _heap_index_map[k];
    }

public:
    constexpr void push(entry p) noexcept {
        const size_type n = _heap_array.size();
        _heap_array.emplace_back();
        heap_push(size_type(n * sizeof(entry)), std::move(p));
    }
    [[nodiscard]] constexpr priority_type priority(
        const id_type & k) const noexcept {
        return _entry_priority_map[entry_ref(index_of(k))];
    }
    [[nodiscard]] constexpr bool contains(const id_type & k) const noexcept {
        const size_type i = index_of(k);
        if(i >= _heap_array.size()) return false;
        return _entry_id_map[_heap_array[i]] == k;
    }
    [[nodiscard]] constexpr entry top() const noexcept {
        assert(!_heap_array.empty());
        return _heap_array.front();
    }
    constexpr void pop() noexcept {
        assert(!_heap_array.empty());
        const size_type n = _heap_array.size() - 1;
        if(n > 0)
            adjust_heap(size_type(0), n * sizeof(entry),
                        std::move(_heap_array.back()));
        _heap_array.pop_back();
    }
    constexpr void promote(const id_type & k,
                           const priority_type & p) noexcept {
        entry e = entry_ref(_heap_index_map[k]);
        assert(!_priority_cmp(_entry_priority_map[e], p));
        _entry_priority_map[e] = p;
        heap_push(_heap_index_map[k], std::move(e));
    }
    void demote(const id_type & k, const priority_type & p) noexcept {
        entry e = entry_ref(_heap_index_map[k]);
        assert(_priority_cmp(_entry_priority_map[e], p));
        _entry_priority_map[e] = p;
        adjust_heap(_heap_index_map[k], std::move(e));
    }
};  // class d_ary_heap

// template <typename _Graph, typename _LengthMap, typename _Traits>
// d_ary_heap(_Graph &&, _LengthMap &&, const vertex_t<_Graph> &)
//     -> d_ary_heap<views::mapping_all_t<_LengthMap>, _Traits>;

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_D_ARY_HEAP_HPP