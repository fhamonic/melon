#ifndef MELON_PLANAR_GRAPH_HPP
#define MELON_PLANAR_GRAPH_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {

namespace __cust_access {
template <typename _Tp>
concept __member_faces = requires(const _Tp & __t) {
                                { __t.faces() } -> std::ranges::input_range;
                            };

template <typename _Tp>
concept __adl_faces = requires(const _Tp & __t) {
                             { faces(__t) } -> std::ranges::input_range;
                         };

struct _Faces {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_faces<_Tp>)
            return noexcept(std::declval<_Tp &>().faces());
        else
            return noexcept(faces(std::declval<_Tp &>()));
    }

public:
    template <typename _Tp>
        requires __member_faces<_Tp> || __adl_faces<_Tp>
    constexpr decltype(auto) operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_faces<_Tp>)
            return __t.faces();
        else if constexpr(__adl_faces<_Tp>)
            return faces(__t);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_Faces faces{};
}  // namespace __cust

template <typename _Tp>
using faces_range_t = decltype(melon::faces(std::declval<_Tp &>()));

template <typename _Tp>
using face_t = std::ranges::range_value_t<faces_range_t<_Tp>>;

namespace __cust_access {
template <typename _Tp>
concept __member_nb_faces = requires(const _Tp & __t) {
                                   { __t.nb_faces() } -> std::integral;
                               };

template <typename _Tp>
concept __adl_nb_faces = requires(const _Tp & __t) {
                                { nb_faces(__t) } -> std::integral;
                            };

struct _NbFaces {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_nb_faces<_Tp>)
            return noexcept(std::declval<_Tp &>().nb_faces());
        else if constexpr(__adl_nb_faces<_Tp>)
            return noexcept(nb_faces(std::declval<_Tp &>()));
        else
            return noexcept(
                std::ranges::size(melon::faces(std::declval<_Tp &>())));
    }

public:
    template <typename _Tp>
        requires __member_nb_faces<_Tp> || __adl_nb_faces<_Tp> ||
                 std::ranges::sized_range<faces_range_t<_Tp>>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_nb_faces<_Tp>)
            return __t.nb_faces();
        else if constexpr(__adl_nb_faces<_Tp>)
            return nb_faces(__t);
        else
            return std::ranges::size(melon::faces(__t));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_NbFaces nb_faces{};
}  // namespace __cust






namespace __cust_access {
template <typename _Tp>
concept __member_bounding_arcs =
    requires(const _Tp & __t, const face_t<_Tp> & __f) {
        { __t.bounding_arcs(__f) } -> std::ranges::input_range;
    };

template <typename _Tp>
concept __adl_bounding_arcs = requires(const _Tp & __t, const face_t<_Tp> & __f) {
                             { bounding_arcs(__t, __f) } -> std::ranges::input_range;
                         };

struct _BoundingArcs {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_bounding_arcs<_Tp>)
            return noexcept(std::declval<_Tp &>().bounding_arcs(
                std::declval<face_t<_Tp> &>()));
        else
            return noexcept(bounding_arcs(std::declval<_Tp &>(),
                                     std::declval<face_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_bounding_arcs<_Tp> || __adl_bounding_arcs<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const face_t<_Tp> & __f) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_bounding_arcs<_Tp>)
            return __t.bounding_arcs(__f);
        else
            return bounding_arcs(__t, __f);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_BoundingArcs bounding_arcs{};
}  // namespace __cust

template <typename _Tp>
using bounding_arcs_range_t = decltype(melon::bounding_arcs(
    std::declval<_Tp &>(), std::declval<face_t<_Tp> &>()));




















namespace __cust_access {
template <typename _Tp, typename _ValueType>
concept __member_create_face_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            __t.template create_face_map<_ValueType>()
            } -> output_value_map_of<face_t<_Tp>, _ValueType>;
        {
            __t.template create_face_map<_ValueType>(__d)
            } -> output_value_map_of<face_t<_Tp>, _ValueType>;
    };

template <typename _Tp, typename _ValueType>
concept __adl_create_face_map =
    requires(const _Tp & __t, const _ValueType & __d) {
        {
            create_face_map<_ValueType>(__t)
            } -> output_value_map_of<face_t<_Tp>, _ValueType>;
        {
            create_face_map<_ValueType>(__t, __d)
            } -> output_value_map_of<face_t<_Tp>, _ValueType>;
    };

struct _CreateFaceMap {
private:
    template <typename _ValueType, typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_create_face_map<_Tp, _ValueType>)
            return noexcept(
                std::declval<_Tp &>().template create_face_map<_ValueType>());
        else
            return noexcept(
                create_face_map<_ValueType>(std::declval<_Tp &>()));
    }

public:
    template <typename _ValueType, typename _Tp>
        requires __member_create_face_map<_Tp, _ValueType> ||
                 __adl_create_face_map<_Tp, _ValueType>
    constexpr auto operator() [[nodiscard]] (const _Tp & __t) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_face_map<_Tp, _ValueType>)
            return __t.template create_face_map<_ValueType>();
        else
            return create_face_map<_ValueType>(__t);
    }

    template <typename _ValueType, typename _Tp>
        requires __member_create_face_map<_Tp, _ValueType> ||
                 __adl_create_face_map<_Tp, _ValueType>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const _ValueType & __d) const
        noexcept(_S_noexcept<_ValueType, _Tp &>()) {
        if constexpr(__member_create_face_map<_Tp, _ValueType>)
            return __t.template create_face_map<_ValueType>(__d);
        else
            return create_face_map<_ValueType>(__t, __d);
    }
};
}  // namespace __cust_access

inline namespace __cust {
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t) {
                 __cust_access::_CreateFaceMap{}
                     .template operator()<_ValueType>(__t);
             }
inline constexpr auto create_face_map(const _Tp & __t) noexcept(
    noexcept(__cust_access::_CreateFaceMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>()))) {
    return __cust_access::_CreateFaceMap{}.template operator()<_ValueType>(
        __t);
}
template <typename _ValueType, typename _Tp>
    requires requires(const _Tp & __t, const _ValueType & __d) {
                 __cust_access::_CreateFaceMap{}
                     .template operator()<_ValueType>(__t, __d);
             }
inline constexpr auto
create_face_map(const _Tp & __t, const _ValueType & __d) noexcept(
    noexcept(__cust_access::_CreateFaceMap{}.template operator()<_ValueType>(
        std::declval<_Tp &>(), std::declval<_ValueType &>()))) {
    return __cust_access::_CreateFaceMap{}.template operator()<_ValueType>(
        __t, __d);
}
}  // namespace __cust

template <typename _Tp, typename _ValueType>
using face_map_t =
    decltype(melon::create_face_map<_ValueType>(std::declval<_Tp &>()));

template <typename _Tp, typename _ValueType = std::size_t>
concept has_face_map =
    graph<_Tp> && requires(const _Tp & __t, const _ValueType & __d) {
                      melon::create_face_map<_ValueType>(__t);
                      melon::create_face_map<_ValueType>(__t, __d);
                  };
}  // namespace melon
}  // namespace fhamonic

//  arc_twin : arc -> arc
//  arc_face : arc -> face


#endif  // MELON_PLANAR_GRAPH_HPP