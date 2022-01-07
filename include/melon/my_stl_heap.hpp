#ifndef _STL_HEAP_H
#define _STL_HEAP_H 1

#include <bits/move.h>
#include <bits/predefined_ops.h>
#include <debug/debug.h>

namespace melon {
namespace stl_heap {
// Heap-manipulation functions: push_heap, pop_heap, make_heap, sort_heap,
// + is_heap and is_heap_until in C++0x.

template <typename _RandomAccessIterator, typename _Distance, typename _Tp,
          typename _Compare>
constexpr void __push_heap(_RandomAccessIterator __first, _Distance __holeIndex,
                           _Distance __topIndex, _Tp __value,
                           _Compare & __comp) {
    _Distance __parent = (__holeIndex - 1) / 2;
    while(__holeIndex > __topIndex && __comp(*(__first + __parent), __value)) {
        *(__first + __holeIndex) = std::move(*(__first + __parent));
        __holeIndex = __parent;
        __parent = (__holeIndex - 1) / 2;
    }
    *(__first + __holeIndex) = std::move(__value);
}

/**
 *  This operation pushes the element at last-1 onto the valid heap
 *  over the range [__first,__last-1).  After completion,
 *  [__first,__last) is a valid heap.
 */
template <typename _RandomAccessIterator>
constexpr inline void push_heap(_RandomAccessIterator __first,
                                _RandomAccessIterator __last) {
    using _ValueType =
        typename iterator_traits<_RandomAccessIterator>::value_type;
    using _DistanceType =
        typename iterator_traits<_RandomAccessIterator>::difference_type;

    std::less<_ValueType> __comp;
    _ValueType __value = std::move(*(__last - 1));
    __push_heap(__first, _DistanceType((__last - __first) - 1),
                     _DistanceType(0), __value, __comp);
}

/**
 *  This operation pushes the element at __last-1 onto the valid
 *  heap over the range [__first,__last-1).  After completion,
 *  [__first,__last) is a valid heap.  Compare operations are
 *  performed using comp.
 */
template <typename _RandomAccessIterator, typename _Compare>
constexpr inline void push_heap(_RandomAccessIterator __first,
                                _RandomAccessIterator __last, _Compare __comp) {
    using _ValueType =
        typename iterator_traits<_RandomAccessIterator>::value_type;
    using _DistanceType =
        typename iterator_traits<_RandomAccessIterator>::difference_type;

    _ValueType __value = std::move(*(__last - 1));
    __push_heap(__first, _DistanceType((__last - __first) - 1),
                     _DistanceType(0), __value, __comp);
}

template <typename _RandomAccessIterator, typename _Distance, typename _Tp,
          typename _Compare>
constexpr void __adjust_heap(_RandomAccessIterator __first,
                                        _Distance __holeIndex, _Distance __len,
                                        _Tp __value, _Compare __comp) {
    const _Distance __topIndex = __holeIndex;
    _Distance __secondChild = __holeIndex;
    while(__secondChild < (__len - 1) / 2) {
        __secondChild = 2 * (__secondChild + 1);
        if(__comp(__first + __secondChild, __first + (__secondChild - 1)))
            __secondChild--;
        *(__first + __holeIndex) = std::move(*(__first + __secondChild));
        __holeIndex = __secondChild;
    }
    if((__len & 1) == 0 && __secondChild == (__len - 2) / 2) {
        __secondChild = 2 * (__secondChild + 1);
        *(__first + __holeIndex) =
            std::move(*(__first + (__secondChild - 1)));
        __holeIndex = __secondChild - 1;
    }
    std::__push_heap(__first, __holeIndex, __topIndex, std::move(__value),
                     __comp);
}

template <typename _RandomAccessIterator, typename _Compare>
constexpr inline void __pop_heap(_RandomAccessIterator __first,
                                            _RandomAccessIterator __last,
                                            _RandomAccessIterator __result,
                                            _Compare & __comp) {
    typedef
        typename iterator_traits<_RandomAccessIterator>::value_type _ValueType;
    typedef typename iterator_traits<_RandomAccessIterator>::difference_type
        _DistanceType;

    _ValueType __value = std::move(*__result);
    *__result = std::move(*__first);
    std::__adjust_heap(__first, _DistanceType(0),
                       _DistanceType(__last - __first), std::move(__value),
                       __comp);
}

/**
 *  This operation pops the top of the heap.  The elements __first
 *  and __last-1 are swapped and [__first,__last-1) is made into a
 *  heap.
 */
template <typename _RandomAccessIterator>
constexpr inline void pop_heap(_RandomAccessIterator __first,
                                          _RandomAccessIterator __last) {
    if(__last - __first > 1) {
        --__last;
        __gnu_cxx::__ops::_Iter_less_iter __comp;
        std::__pop_heap(__first, __last, __last, __comp);
    }
}

/**
 *  @brief  Pop an element off a heap using comparison functor.
 *  @param  __first  Start of heap.
 *  @param  __last   End of heap.
 *  @param  __comp   Comparison functor to use.
 *  @ingroup heap_algorithms
 *
 *  This operation pops the top of the heap.  The elements __first
 *  and __last-1 are swapped and [__first,__last-1) is made into a
 *  heap.  Comparisons are made using comp.
 */
template <typename _RandomAccessIterator, typename _Compare>
constexpr inline void pop_heap(_RandomAccessIterator __first,
                                          _RandomAccessIterator __last,
                                          _Compare __comp) {
    // concept requirements
    __glibcxx_function_requires(
        _Mutable_RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive_pred(__first, __last, __comp);
    __glibcxx_requires_non_empty_range(__first, __last);
    __glibcxx_requires_heap_pred(__first, __last, __comp);

    if(__last - __first > 1) {
        typedef __decltype(__comp) _Cmp;
        __gnu_cxx::__ops::_Iter_comp_iter<_Cmp> __cmp(std::move(__comp));
        --__last;
        std::__pop_heap(__first, __last, __last, __cmp);
    }
}

template <typename _RandomAccessIterator, typename _Compare>
constexpr void __make_heap(_RandomAccessIterator __first,
                                      _RandomAccessIterator __last,
                                      _Compare & __comp) {
    typedef
        typename iterator_traits<_RandomAccessIterator>::value_type _ValueType;
    typedef typename iterator_traits<_RandomAccessIterator>::difference_type
        _DistanceType;

    if(__last - __first < 2) return;

    const _DistanceType __len = __last - __first;
    _DistanceType __parent = (__len - 2) / 2;
    while(true) {
        _ValueType __value = std::move(*(__first + __parent));
        std::__adjust_heap(__first, __parent, __len, std::move(__value),
                           __comp);
        if(__parent == 0) return;
        __parent--;
    }
}

/**
 *  @brief  Construct a heap over a range.
 *  @param  __first  Start of heap.
 *  @param  __last   End of heap.
 *  @ingroup heap_algorithms
 *
 *  This operation makes the elements in [__first,__last) into a heap.
 */
template <typename _RandomAccessIterator>
constexpr inline void make_heap(_RandomAccessIterator __first,
                                           _RandomAccessIterator __last) {
    // concept requirements
    __glibcxx_function_requires(
        _Mutable_RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_function_requires(
            _LessThanComparableConcept<
                typename iterator_traits<_RandomAccessIterator>::value_type>)
            __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive(__first, __last);

    __gnu_cxx::__ops::_Iter_less_iter __comp;
    std::__make_heap(__first, __last, __comp);
}

/**
 *  @brief  Construct a heap over a range using comparison functor.
 *  @param  __first  Start of heap.
 *  @param  __last   End of heap.
 *  @param  __comp   Comparison functor to use.
 *  @ingroup heap_algorithms
 *
 *  This operation makes the elements in [__first,__last) into a heap.
 *  Comparisons are made using __comp.
 */
template <typename _RandomAccessIterator, typename _Compare>
constexpr inline void make_heap(_RandomAccessIterator __first,
                                           _RandomAccessIterator __last,
                                           _Compare __comp) {
    // concept requirements
    __glibcxx_function_requires(
        _Mutable_RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive_pred(__first, __last, __comp);

    typedef __decltype(__comp) _Cmp;
    __gnu_cxx::__ops::_Iter_comp_iter<_Cmp> __cmp(std::move(__comp));
    std::__make_heap(__first, __last, __cmp);
}

template <typename _RandomAccessIterator, typename _Compare>
constexpr void __sort_heap(_RandomAccessIterator __first,
                                      _RandomAccessIterator __last,
                                      _Compare & __comp) {
    while(__last - __first > 1) {
        --__last;
        std::__pop_heap(__first, __last, __last, __comp);
    }
}

/**
 *  @brief  Sort a heap.
 *  @param  __first  Start of heap.
 *  @param  __last   End of heap.
 *  @ingroup heap_algorithms
 *
 *  This operation sorts the valid heap in the range [__first,__last).
 */
template <typename _RandomAccessIterator>
constexpr inline void sort_heap(_RandomAccessIterator __first,
                                           _RandomAccessIterator __last) {
    // concept requirements
    __glibcxx_function_requires(
        _Mutable_RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_function_requires(
            _LessThanComparableConcept<
                typename iterator_traits<_RandomAccessIterator>::value_type>)
            __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive(__first, __last);
    __glibcxx_requires_heap(__first, __last);

    __gnu_cxx::__ops::_Iter_less_iter __comp;
    std::__sort_heap(__first, __last, __comp);
}

/**
 *  @brief  Sort a heap using comparison functor.
 *  @param  __first  Start of heap.
 *  @param  __last   End of heap.
 *  @param  __comp   Comparison functor to use.
 *  @ingroup heap_algorithms
 *
 *  This operation sorts the valid heap in the range [__first,__last).
 *  Comparisons are made using __comp.
 */
template <typename _RandomAccessIterator, typename _Compare>
constexpr inline void sort_heap(_RandomAccessIterator __first,
                                           _RandomAccessIterator __last,
                                           _Compare __comp) {
    // concept requirements
    __glibcxx_function_requires(
        _Mutable_RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive_pred(__first, __last, __comp);
    __glibcxx_requires_heap_pred(__first, __last, __comp);

    typedef __decltype(__comp) _Cmp;
    __gnu_cxx::__ops::_Iter_comp_iter<_Cmp> __cmp(std::move(__comp));
    std::__sort_heap(__first, __last, __cmp);
}

#if __cplusplus >= 201103L
/**
 *  @brief  Search the end of a heap.
 *  @param  __first  Start of range.
 *  @param  __last   End of range.
 *  @return  An iterator pointing to the first element not in the heap.
 *  @ingroup heap_algorithms
 *
 *  This operation returns the last iterator i in [__first, __last) for which
 *  the range [__first, i) is a heap.
 */
template <typename _RandomAccessIterator>
constexpr inline _RandomAccessIterator is_heap_until(
    _RandomAccessIterator __first, _RandomAccessIterator __last) {
    // concept requirements
    __glibcxx_function_requires(
        _RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_function_requires(
            _LessThanComparableConcept<
                typename iterator_traits<_RandomAccessIterator>::value_type>)
            __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive(__first, __last);

    __gnu_cxx::__ops::_Iter_less_iter __comp;
    return __first + std::__is_heap_until(
                         __first, std::distance(__first, __last), __comp);
}

/**
 *  @brief  Search the end of a heap using comparison functor.
 *  @param  __first  Start of range.
 *  @param  __last   End of range.
 *  @param  __comp   Comparison functor to use.
 *  @return  An iterator pointing to the first element not in the heap.
 *  @ingroup heap_algorithms
 *
 *  This operation returns the last iterator i in [__first, __last) for which
 *  the range [__first, i) is a heap.  Comparisons are made using __comp.
 */
template <typename _RandomAccessIterator, typename _Compare>
constexpr inline _RandomAccessIterator is_heap_until(
    _RandomAccessIterator __first, _RandomAccessIterator __last,
    _Compare __comp) {
    // concept requirements
    __glibcxx_function_requires(
        _RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive_pred(__first, __last, __comp);

    typedef __decltype(__comp) _Cmp;
    __gnu_cxx::__ops::_Iter_comp_iter<_Cmp> __cmp(std::move(__comp));
    return __first +
           std::__is_heap_until(__first, std::distance(__first, __last), __cmp);
}

/**
 *  @brief  Determines whether a range is a heap.
 *  @param  __first  Start of range.
 *  @param  __last   End of range.
 *  @return  True if range is a heap, false otherwise.
 *  @ingroup heap_algorithms
 */
template <typename _RandomAccessIterator>
constexpr inline bool is_heap(_RandomAccessIterator __first,
                                         _RandomAccessIterator __last) {
    return std::is_heap_until(__first, __last) == __last;
}

/**
 *  @brief  Determines whether a range is a heap using comparison functor.
 *  @param  __first  Start of range.
 *  @param  __last   End of range.
 *  @param  __comp   Comparison functor to use.
 *  @return  True if range is a heap, false otherwise.
 *  @ingroup heap_algorithms
 */
template <typename _RandomAccessIterator, typename _Compare>
constexpr inline bool is_heap(_RandomAccessIterator __first,
                                         _RandomAccessIterator __last,
                                         _Compare __comp) {
    // concept requirements
    __glibcxx_function_requires(
        _RandomAccessIteratorConcept<_RandomAccessIterator>)
        __glibcxx_requires_valid_range(__first, __last);
    __glibcxx_requires_irreflexive_pred(__first, __last, __comp);

    const auto __dist = std::distance(__first, __last);
    typedef __decltype(__comp) _Cmp;
    __gnu_cxx::__ops::_Iter_comp_iter<_Cmp> __cmp(std::move(__comp));
    return std::__is_heap_until(__first, __dist, __cmp) == __dist;
}
}
}

#endif /* _STL_HEAP_H */
