#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "melon/mapping.hpp"

namespace melon {

// promote()/demote() rewrite the priority stored *inside* an entry, so the
// entry-priority map has to hand back a reference into that entry. A map
// returning a prvalue (views::identity_map, or any map yielding a copy or a
// detached proxy) would make the write land on a temporary and vanish without
// a diagnostic, leaving the heap silently un-reordered. output_mapping is not
// enough to express this: assigning to a class prvalue is well-formed and even
// yields an lvalue, so identity_map satisfies it.
template <typename Map, typename Entry>
concept mutable_entry_priority_map =
    input_mapping<Map, Entry> && requires(Map & m, Entry & e) {
        requires std::is_lvalue_reference_v<decltype(m[e])>;
        requires !std::is_const_v<std::remove_reference_t<decltype(m[e])>>;
    };

template <typename Derived, std::size_t D, typename Entry,
          typename PriorityComparator = std::greater<Entry>,
          input_mapping<Entry> EntryPriorityMap = views::identity_map>
    requires std::strict_weak_order<PriorityComparator,
                                    mapped_value_t<EntryPriorityMap, Entry>,
                                    mapped_value_t<EntryPriorityMap, Entry>>
class d_ary_heap_base {
public:
    using value_type = Entry;
    using size_type = std::size_t;
    using priority_type = mapped_value_t<EntryPriorityMap, Entry>;

protected:
    std::vector<value_type> _heap_array;
    [[no_unique_address]] PriorityComparator _priority_cmp;
    [[no_unique_address]] EntryPriorityMap _entry_priority_map;

public:
    [[nodiscard]] constexpr d_ary_heap_base()
        : _heap_array(), _priority_cmp(), _entry_priority_map() {}

    // Constrained away from d_ary_heap_base itself: as an unconstrained
    // single-argument template this bound a non-const lvalue of the class type
    // better than the copy constructor (PC && deduces an exact match, the copy
    // constructor needs a const conversion), so copying a mutable heap tried to
    // build a comparator out of it.
    template <typename PC>
        requires(!std::same_as<std::remove_cvref_t<PC>, d_ary_heap_base>) &&
                    std::constructible_from<PriorityComparator, PC>
    [[nodiscard]] constexpr d_ary_heap_base(PC && priority_cmp)
        : _heap_array()
        , _priority_cmp(std::forward<PC>(priority_cmp))
        , _entry_priority_map() {}

    template <typename PC, typename EPM>
    [[nodiscard]] constexpr d_ary_heap_base(PC && priority_cmp,
                                            EPM && entry_priority_map)
        : _heap_array()
        , _priority_cmp(std::forward<PC>(priority_cmp))
        , _entry_priority_map(std::forward<EPM>(entry_priority_map)) {}

    [[nodiscard]] constexpr d_ary_heap_base(const d_ary_heap_base &) = default;
    [[nodiscard]] constexpr d_ary_heap_base(d_ary_heap_base &&) = default;

    d_ary_heap_base & operator=(const d_ary_heap_base &) = default;
    d_ary_heap_base & operator=(d_ary_heap_base &&) = default;

    [[nodiscard]] constexpr size_type size() const noexcept {
        return _heap_array.size();
    }
    [[nodiscard]] constexpr bool empty() const noexcept {
        return _heap_array.empty();
    }
    constexpr void clear() noexcept { _heap_array.clear(); }

protected:
    [[nodiscard]] static constexpr size_type parent_of(
        const size_type i) noexcept {
        return (i - sizeof(value_type)) / (sizeof(value_type) * D) *
               sizeof(value_type);
    }
    [[nodiscard]] static constexpr size_type first_child_of(
        const size_type i) noexcept {
        return i * D + sizeof(value_type);
    }
    template <int I = D>
    [[nodiscard]] size_type minimum_child(
        const size_type first_child) const noexcept {
        if constexpr(I == 1)
            return first_child;
        else if constexpr(I == 2)
            return first_child +
                   sizeof(value_type) *
                       _priority_cmp(
                           _entry_priority_map[entry_ref(first_child +
                                                         sizeof(value_type))],
                           _entry_priority_map[entry_ref(first_child)]);
        else {
            const size_type first_half_minimum =
                minimum_child<I / 2>(first_child);
            const size_type second_half_minimum = minimum_child<I - I / 2>(
                first_child + (I / 2) * sizeof(value_type));
            return _priority_cmp(
                       _entry_priority_map[entry_ref(second_half_minimum)],
                       _entry_priority_map[entry_ref(first_half_minimum)])
                       ? second_half_minimum
                       : first_half_minimum;
        }
    }
    [[nodiscard]] size_type minimum_remaining_child(
        const size_type first_child,
        const size_type num_children) const noexcept {
        if constexpr(D == 2)
            return first_child;
        else if constexpr(D == 4) {
            switch(num_children) {
                case 1:
                    return minimum_child<1>(first_child);
                case 2:
                    return minimum_child<2>(first_child);
                default:
                    return minimum_child<3>(first_child);
            }
        } else {
            switch(num_children) {
                case 1:
                    return minimum_child<1>(first_child);
                case 2:
                    return minimum_child<2>(first_child);
                default:
                    const size_type half = num_children / 2;
                    const size_type first_half_minimum =
                        minimum_remaining_child(first_child, half);
                    const size_type second_half_minimum =
                        minimum_remaining_child(
                            first_child + half * sizeof(value_type),
                            num_children - half);
                    return _priority_cmp(_entry_priority_map[entry_ref(
                                             second_half_minimum)],
                                         _entry_priority_map[entry_ref(
                                             first_half_minimum)])
                               ? second_half_minimum
                               : first_half_minimum;
            }
        }
    }

    // Not constexpr, and neither is anything reaching it: the heap addresses
    // its array by byte offset through reinterpret_cast, which is never a
    // constant expression. Marking these constexpr only advertised something
    // no caller could ever use.
    [[nodiscard]] value_type & entry_ref(const size_type i) noexcept {
        assert((i / sizeof(value_type)) < _heap_array.size());
        return *(reinterpret_cast<value_type *>(
            reinterpret_cast<std::byte *>(_heap_array.data()) + i));
    }
    [[nodiscard]] const value_type & entry_ref(
        const size_type i) const noexcept {
        assert((i / sizeof(value_type)) < _heap_array.size());
        return *(reinterpret_cast<const value_type *>(
            reinterpret_cast<const std::byte *>(_heap_array.data()) + i));
    }
    void heap_move(const size_type i, value_type && p) {
        static_cast<Derived *>(this)->heap_move(i, std::move(p));
    }
    void heap_push(size_type hole_index, value_type && p) {
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
                     value_type && p) {
        size_type child_end;
        if constexpr(D > 2)
            child_end = end > D * sizeof(value_type)
                            ? end - (D - 1) * sizeof(value_type)
                            : 0;
        else
            child_end = end - (D - 1) * sizeof(value_type);
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
            child = minimum_remaining_child(child,
                                            (end - child) / sizeof(value_type));
            if(_priority_cmp(_entry_priority_map[entry_ref(child)],
                             _entry_priority_map[p])) {
                heap_move(hole_index, std::move(entry_ref(child)));
                hole_index = child;
            }
        }
    ok:
        heap_move(hole_index, std::move(p));
    }

public:
    // Not noexcept: emplace_back may reallocate and throw. It also sifts
    // through the user's comparator and priority map.
    void push(value_type p) {
        const size_type n = _heap_array.size();
        _heap_array.emplace_back();
        heap_push(size_type(n * sizeof(value_type)), std::move(p));
    }
    [[nodiscard]] value_type top() const
        noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
        assert(!_heap_array.empty());
        return _heap_array.front();
    }
    void pop() {
        assert(!_heap_array.empty());
        const size_type n = _heap_array.size() - 1;
        if(n > 0)
            adjust_heap(size_type(0), n * sizeof(value_type),
                        std::move(_heap_array.back()));
        _heap_array.pop_back();
    }
};

template <std::size_t D, typename Entry,
          typename PriorityComparator = std::greater<Entry>,
          input_mapping<Entry> EntryPriorityMap = views::identity_map>
class d_ary_heap
    : public d_ary_heap_base<
          d_ary_heap<D, Entry, PriorityComparator, EntryPriorityMap>, D, Entry,
          PriorityComparator, EntryPriorityMap> {
private:
    using base_class = d_ary_heap_base<d_ary_heap, D, Entry, PriorityComparator,
                                       EntryPriorityMap>;

public:
    using typename base_class::size_type;
    using typename base_class::value_type;

    d_ary_heap() : base_class() {}

    template <typename PC, typename EPM>
    [[nodiscard]] constexpr d_ary_heap(PC && priority_cmp,
                                       EPM && entry_priority_map)
        : base_class(std::forward<PC>(priority_cmp),
                     std::forward<EPM>(entry_priority_map)) {}

private:
    void heap_move(const size_type i, value_type && p) {
        assert((i / sizeof(value_type)) < base_class::_heap_array.size());
        base_class::entry_ref(i) = std::move(p);
    }
    friend base_class;
};

template <std::size_t D, typename Entry,
          typename PriorityComparator = std::greater<Entry>,
          typename IndicesMap =
              mapping_owning_view<std::unordered_map<Entry, std::size_t>>,
          input_mapping<Entry> EntryPriorityMap = views::identity_map,
          input_mapping<Entry> EntryIdMap = views::identity_map>
    requires std::strict_weak_order<PriorityComparator,
                                    mapped_value_t<EntryPriorityMap, Entry>,
                                    mapped_value_t<EntryPriorityMap, Entry>> &&
             output_mapping<IndicesMap, mapped_value_t<EntryIdMap, Entry>>
class updatable_d_ary_heap
    : public d_ary_heap_base<
          updatable_d_ary_heap<D, Entry, PriorityComparator, IndicesMap,
                               EntryPriorityMap, EntryIdMap>,
          D, Entry, PriorityComparator, EntryPriorityMap> {
private:
    using base_class = d_ary_heap_base<updatable_d_ary_heap, D, Entry,
                                       PriorityComparator, EntryPriorityMap>;

public:
    using typename base_class::priority_type;
    using typename base_class::size_type;
    using typename base_class::value_type;

    using id_type = mapped_value_t<EntryIdMap, Entry>;

private:
    [[no_unique_address]] EntryIdMap _entry_id_map;
    [[no_unique_address]] IndicesMap _heap_index_map;

public:
    updatable_d_ary_heap() : base_class() {}

    template <typename PC, typename HIM>
    updatable_d_ary_heap(PC && priority_cmp, HIM && heap_index_map)
        : base_class(std::forward<PC>(priority_cmp))
        , _entry_id_map()
        , _heap_index_map(std::forward<HIM>(heap_index_map)) {}

protected:
    void heap_move(const size_type i, value_type && p) {
        assert((i / sizeof(value_type)) < base_class::_heap_array.size());
        _heap_index_map[_entry_id_map[p]] = i;
        base_class::entry_ref(i) = std::move(p);
    }
    friend base_class;

    [[nodiscard]] size_type index_of(const id_type & k) const {
        return _heap_index_map[k];
    }

public:
    [[nodiscard]] priority_type priority(const id_type & k) const {
        return base_class::_entry_priority_map[base_class::entry_ref(
            index_of(k))];
    }
    [[nodiscard]] bool contains(const id_type & k) const {
        const size_type i = index_of(k);
        if(i >= base_class::_heap_array.size()) return false;
        return _entry_id_map[base_class::_heap_array[i]] == k;
    }
    void promote(const id_type & k, const priority_type & p)
        requires mutable_entry_priority_map<EntryPriorityMap, Entry>
    {
        value_type e = std::move(base_class::entry_ref(_heap_index_map[k]));
        assert(
            !base_class::_priority_cmp(base_class::_entry_priority_map[e], p));
        base_class::_entry_priority_map[e] = p;
        base_class::heap_push(_heap_index_map[k], std::move(e));
    }
    void demote(const id_type & k, const priority_type & p)
        requires mutable_entry_priority_map<EntryPriorityMap, Entry>
    {
        value_type e = std::move(base_class::entry_ref(_heap_index_map[k]));
        assert(
            base_class::_priority_cmp(base_class::_entry_priority_map[e], p));
        base_class::_entry_priority_map[e] = p;
        // Unlike pop(), every slot of the array is still live here, so the
        // sifting range covers the whole heap.
        base_class::adjust_heap(
            _heap_index_map[k],
            base_class::_heap_array.size() * sizeof(value_type), std::move(e));
    }
};

}  // namespace melon
