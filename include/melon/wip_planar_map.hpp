#ifndef MELON_PLANAR_MAP_HPP
#define MELON_PLANAR_MAP_HPP

#include <concepts>
#include <ranges>
#include <type_traits>

#include "melon/graph.hpp"

namespace fhamonic {
namespace melon {

namespace __cust_access {
template <typename _Tp>
concept __member_vertex_coordinates =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        __t.vertex_coordinates(__v, __v);
    };

template <typename _Tp>
concept __adl_vertex_coordinates =
    requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
        vertex_coordinates(__t, __v);
    };

struct _VertexCoordinates {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_vertex_coordinates<_Tp>)
            return noexcept(std::declval<_Tp &>().vertex_coordinates(
                std::declval<vertex_t<_Tp> &>()));
        else
            return noexcept(vertex_coordinates(
                std::declval<_Tp &>(), std::declval<vertex_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_vertex_coordinates<_Tp> ||
                 __adl_vertex_coordinates<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const vertex_t<_Tp> & __v) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_vertex_coordinates<_Tp>)
            return __t.vertex_coordinates(__v);
        else
            return vertex_coordinates(__t, __v);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_VertexCoordinates vertex_coordinates{};
}  // namespace __cust

template <typename _Tp>
using vertex_coordinates_t =
    decltype(melon::vertex_coordinates(std::declval<vertex_t<_Tp> &>()));

template <typename _Tp>
concept has_vertex_coordinates =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v) {
                      melon::vertex_coordinates(__t, __v);
                  };

namespace __cust_access {
template <typename _Tp>
concept __member_arc_source_coordinates =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        {
            __t.arc_source_coordinates(__a)
            } -> std::convertible_to<vertex_coordinates_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_arc_source_coordinates =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        {
            arc_source_coordinates(__t, __a)
            } -> std::convertible_to<vertex_coordinates_t<_Tp>>;
    };

struct _ArcSourceCoordinates {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_source_coordinates<_Tp>)
            return noexcept(std::declval<_Tp &>().arc_source_coordinates(
                std::declval<arc_t<_Tp> &>()));
        else if constexpr(__adl_arc_source_coordinates<_Tp>)
            return noexcept(arc_source_coordinates(
                std::declval<_Tp &>(), std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(melon::vertex_coordinates(
                std::declval<_Tp &>(),
                melon::arc_source(std::declval<_Tp &>(),
                                  std::declval<arc_t<_Tp> &>())))
    }

public:
    template <typename _Tp>
        requires __member_arc_source_coordinates<_Tp> ||
                 __adl_arc_source_coordinates<_Tp> ||
                 (has_arc_source<_Tp> && has_vertex_coordinates<_Tp>)
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_source_coordinates<_Tp>)
            return __t.arc_source_coordinates(__a);
        else if constexpr(__adl_arc_source_coordinates<_Tp>)
            return arc_source_coordinates(__t, __a);
        else
            return melon::vertex_coordinates(__t, melon::arc_source(__t, __a));
    }
};

template <typename _Tp>
concept __member_arc_target_coordinates =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        {
            __t.arc_target_coordinates(__a)
            } -> std::convertible_to<vertex_coordinates_t<_Tp>>;
    };

template <typename _Tp>
concept __adl_arc_target_coordinates =
    requires(const _Tp & __t, const arc_t<_Tp> & __a) {
        {
            arc_target_coordinates(__t, __a)
            } -> std::convertible_to<vertex_coordinates_t<_Tp>>;
    };

struct _ArcTargetCoordinates {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_target_coordinates<_Tp>)
            return noexcept(std::declval<_Tp &>().arc_target_coordinates(
                std::declval<arc_t<_Tp> &>()));
        else if constexpr(__adl_arc_target_coordinates<_Tp>)
            return noexcept(arc_target_coordinates(
                std::declval<_Tp &>(), std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(melon::vertex_coordinates(
                std::declval<_Tp &>(),
                melon::arc_target(std::declval<_Tp &>(),
                                  std::declval<arc_t<_Tp> &>())))
    }

public:
    template <typename _Tp>
        requires __member_arc_target_coordinates<_Tp> ||
                 __adl_arc_target_coordinates<_Tp> ||
                 (has_arc_target<_Tp> && has_vertex_coordinates<_Tp>)
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_target_coordinates<_Tp>)
            return __t.arc_target_coordinates(__a);
        else if constexpr(__adl_arc_target_coordinates<_Tp>)
            return arc_target_coordinates(__t, __a);
        else
            return melon::vertex_coordinates(__t, melon::arc_target(__t, __a));
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_ArcSourceCoordinates arc_source_coordinates{};
inline constexpr __cust_access::_ArcTargetCoordinates arc_target_coordinates{};
}  // namespace __cust

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
concept __adl_bounding_arcs =
    requires(const _Tp & __t, const face_t<_Tp> & __f) {
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
template <typename _Tp>
concept __member_arc_twin = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                                {
                                    __t.arc_twin(__a)
                                    } -> std::convertible_to<arc_t<_Tp>>;
                            };

template <typename _Tp>
concept __adl_arc_twin = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                             {
                                 arc_twin(__t, __a)
                                 } -> std::convertible_to<arc_t<_Tp>>;
                         };

struct _ArcTwin {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_twin<_Tp>)
            return noexcept(
                std::declval<_Tp &>().arc_twin(std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(
                arc_twin(std::declval<_Tp &>(), std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_arc_twin<_Tp> || __adl_arc_twin<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_twin<_Tp>)
            return __t.arc_twin(__a);
        else
            return arc_twin(__t, __a);
    }
};

template <typename _Tp>
concept __member_arc_face = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                                {
                                    __t.arc_face(__a)
                                    } -> std::convertible_to<face_t<_Tp>>;
                            };

template <typename _Tp>
concept __adl_arc_face = requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                             {
                                 arc_face(__t, __a)
                                 } -> std::convertible_to<face_t<_Tp>>;
                         };

struct _ArcFace {
private:
    template <typename _Tp>
    static constexpr bool _S_noexcept() {
        if constexpr(__member_arc_face<_Tp>)
            return noexcept(
                std::declval<_Tp &>().arc_face(std::declval<arc_t<_Tp> &>()));
        else
            return noexcept(
                arc_face(std::declval<_Tp &>(), std::declval<arc_t<_Tp> &>()));
    }

public:
    template <typename _Tp>
        requires __member_arc_face<_Tp> || __adl_arc_face<_Tp>
    constexpr auto operator()
        [[nodiscard]] (const _Tp & __t, const arc_t<_Tp> & __a) const
        noexcept(_S_noexcept<_Tp &>()) {
        if constexpr(__member_arc_face<_Tp>)
            return __t.arc_face(__a);
        else
            return arc_face(__t, __a);
    }
};
}  // namespace __cust_access

inline namespace __cust {
inline constexpr __cust_access::_ArcTwin arc_twin{};
inline constexpr __cust_access::_ArcFace arc_face{};
}  // namespace __cust

template <typename _Tp>
concept drawable_graph =
    graph<_Tp> && requires(const _Tp & __t, const vertex_t<_Tp> & __v,
                           const arc_t<_Tp> & __a) {
                      melon::vertex_coordinates(__t, __v);
                      melon::arc_source_coordinates(__t, __a);
                      melon::arc_target_coordinates(__t, __a);
                  };

template <typename _Tp>
concept planar_subdivision =
    drawable_graph<_Tp> && requires(const _Tp & __t, const face_t<_Tp> & __f) {
                               melon::faces(__t);
                               melon::bounding_arcs(__t, __f);
                           };

template <typename _Tp>
concept planar_map = planar_subdivision<_Tp> &&
                     requires(const _Tp & __t, const arc_t<_Tp> & __a) {
                         melon::arc_twin(__t, __a);
                         melon::arc_face(__t, __a);
                     };

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
            return noexcept(create_face_map<_ValueType>(std::declval<_Tp &>()));
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
    return __cust_access::_CreateFaceMap{}.template operator()<_ValueType>(__t);
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
    return __cust_access::_CreateFaceMap{}.template operator()<_ValueType>(__t,
                                                                           __d);
}
}  // namespace __cust

template <typename _Tp, typename _ValueType>
using face_map_t =
    decltype(melon::create_face_map<_ValueType>(std::declval<_Tp &>()));

template <typename _Tp, typename _ValueType = std::size_t>
concept has_face_map =
    planar_map<_Tp> && requires(const _Tp & __t, const _ValueType & __d) {
                           melon::create_face_map<_ValueType>(__t);
                           melon::create_face_map<_ValueType>(__t, __d);
                       };
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_PLANAR_MAP_HPP