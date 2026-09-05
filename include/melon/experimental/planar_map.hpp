#pragma once

// EXPERIMENTAL — ships and is tested (test/experimental.cpp), but carries no
// stability guarantee: anything in melon::experimental may change or
// disappear in any release, including a patch release.

#include <concepts>
#include <cstddef>
#include <ranges>
#include <type_traits>
#include <utility>

#include "melon/graph.hpp"

namespace melon::experimental {

namespace cpo {
template <typename T>
concept has_member_vertex_coordinates =
    requires(const T & t, const vertex_t<T> & v) { t.vertex_coordinates(v); };

template <typename T>
concept has_adl_vertex_coordinates =
    requires(const T & t, const vertex_t<T> & v) { vertex_coordinates(t, v); };

struct vertex_coordinates_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_vertex_coordinates<T>)
            return noexcept(std::declval<const T &>().vertex_coordinates(
                std::declval<const vertex_t<T> &>()));
        else
            return noexcept(
                vertex_coordinates(std::declval<const T &>(),
                                   std::declval<const vertex_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_vertex_coordinates<T> ||
                 has_adl_vertex_coordinates<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const vertex_t<T> & v) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_vertex_coordinates<T>)
            return t.vertex_coordinates(v);
        else
            return vertex_coordinates(t, v);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::vertex_coordinates_fn vertex_coordinates{};
}  // namespace cust

template <typename T>
using vertex_coordinates_t = decltype(melon::experimental::vertex_coordinates(
    std::declval<const T &>(), std::declval<const vertex_t<T> &>()));

template <typename T>
concept has_vertex_coordinates =
    graph<T> && requires(const T & t, const vertex_t<T> & v) {
        melon::experimental::vertex_coordinates(t, v);
    };

namespace cpo {
template <typename T>
concept has_member_arc_source_coordinates =
    requires(const T & t, const arc_t<T> & a) {
        {
            t.arc_source_coordinates(a)
        } -> std::convertible_to<vertex_coordinates_t<T>>;
    };

template <typename T>
concept has_adl_arc_source_coordinates =
    requires(const T & t, const arc_t<T> & a) {
        {
            arc_source_coordinates(t, a)
        } -> std::convertible_to<vertex_coordinates_t<T>>;
    };

struct arc_source_coordinates_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_source_coordinates<T>)
            return noexcept(std::declval<const T &>().arc_source_coordinates(
                std::declval<const arc_t<T> &>()));
        else if constexpr(has_adl_arc_source_coordinates<T>)
            return noexcept(arc_source_coordinates(
                std::declval<const T &>(), std::declval<const arc_t<T> &>()));
        else
            return noexcept(melon::experimental::vertex_coordinates(
                std::declval<const T &>(),
                melon::arc_source(std::declval<const T &>(),
                                  std::declval<const arc_t<T> &>())));
    }

public:
    template <typename T>
        requires has_member_arc_source_coordinates<T> ||
                 has_adl_arc_source_coordinates<T> ||
                 (has_arc_source<T> && has_vertex_coordinates<T>)
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_source_coordinates<T>)
            return t.arc_source_coordinates(a);
        else if constexpr(has_adl_arc_source_coordinates<T>)
            return arc_source_coordinates(t, a);
        else
            return melon::experimental::vertex_coordinates(
                t, melon::arc_source(t, a));
    }
};

template <typename T>
concept has_member_arc_target_coordinates =
    requires(const T & t, const arc_t<T> & a) {
        {
            t.arc_target_coordinates(a)
        } -> std::convertible_to<vertex_coordinates_t<T>>;
    };

template <typename T>
concept has_adl_arc_target_coordinates =
    requires(const T & t, const arc_t<T> & a) {
        {
            arc_target_coordinates(t, a)
        } -> std::convertible_to<vertex_coordinates_t<T>>;
    };

struct arc_target_coordinates_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_target_coordinates<T>)
            return noexcept(std::declval<const T &>().arc_target_coordinates(
                std::declval<const arc_t<T> &>()));
        else if constexpr(has_adl_arc_target_coordinates<T>)
            return noexcept(arc_target_coordinates(
                std::declval<const T &>(), std::declval<const arc_t<T> &>()));
        else
            return noexcept(melon::experimental::vertex_coordinates(
                std::declval<const T &>(),
                melon::arc_target(std::declval<const T &>(),
                                  std::declval<const arc_t<T> &>())));
    }

public:
    template <typename T>
        requires has_member_arc_target_coordinates<T> ||
                 has_adl_arc_target_coordinates<T> ||
                 (has_arc_target<T> && has_vertex_coordinates<T>)
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_target_coordinates<T>)
            return t.arc_target_coordinates(a);
        else if constexpr(has_adl_arc_target_coordinates<T>)
            return arc_target_coordinates(t, a);
        else
            return melon::experimental::vertex_coordinates(
                t, melon::arc_target(t, a));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arc_source_coordinates_fn arc_source_coordinates{};
inline constexpr cpo::arc_target_coordinates_fn arc_target_coordinates{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_faces = requires(const T & t) {
    { t.faces() } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_faces = requires(const T & t) {
    { faces(t) } -> std::ranges::input_range;
};

struct faces_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_faces<T>)
            return noexcept(std::declval<const T &>().faces());
        else
            return noexcept(faces(std::declval<const T &>()));
    }

public:
    template <typename T>
        requires has_member_faces<T> || has_adl_faces<T>
    constexpr decltype(auto) operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_faces<T>)
            return t.faces();
        else if constexpr(has_adl_faces<T>)
            return faces(t);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::faces_fn faces{};
}  // namespace cust

template <typename T>
using faces_range_t =
    decltype(melon::experimental::faces(std::declval<const T &>()));

template <typename T>
using face_t = std::ranges::range_value_t<faces_range_t<T>>;

namespace cpo {
template <typename T>
concept has_member_num_faces = requires(const T & t) {
    { t.num_faces() } -> std::integral;
};

template <typename T>
concept has_adl_num_faces = requires(const T & t) {
    { num_faces(t) } -> std::integral;
};

struct nb_faces_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_num_faces<T>)
            return noexcept(std::declval<const T &>().num_faces());
        else if constexpr(has_adl_num_faces<T>)
            return noexcept(num_faces(std::declval<const T &>()));
        else
            return noexcept(std::ranges::size(
                melon::experimental::faces(std::declval<const T &>())));
    }

public:
    template <typename T>
        requires has_member_num_faces<T> || has_adl_num_faces<T> ||
                 std::ranges::sized_range<faces_range_t<T>>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_num_faces<T>)
            return t.num_faces();
        else if constexpr(has_adl_num_faces<T>)
            return num_faces(t);
        else
            return std::ranges::size(melon::experimental::faces(t));
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::nb_faces_fn num_faces{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_bounding_arcs = requires(const T & t, const face_t<T> & f) {
    { t.bounding_arcs(f) } -> std::ranges::input_range;
};

template <typename T>
concept has_adl_bounding_arcs = requires(const T & t, const face_t<T> & f) {
    { bounding_arcs(t, f) } -> std::ranges::input_range;
};

struct bounding_arcs_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_bounding_arcs<T>)
            return noexcept(std::declval<const T &>().bounding_arcs(
                std::declval<const face_t<T> &>()));
        else
            return noexcept(bounding_arcs(std::declval<const T &>(),
                                          std::declval<const face_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_bounding_arcs<T> || has_adl_bounding_arcs<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const face_t<T> & f) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_bounding_arcs<T>)
            return t.bounding_arcs(f);
        else
            return bounding_arcs(t, f);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::bounding_arcs_fn bounding_arcs{};
}  // namespace cust

namespace cpo {
template <typename T>
concept has_member_arc_twin = requires(const T & t, const arc_t<T> & a) {
    { t.arc_twin(a) } -> std::convertible_to<arc_t<T>>;
};

template <typename T>
concept has_adl_arc_twin = requires(const T & t, const arc_t<T> & a) {
    { arc_twin(t, a) } -> std::convertible_to<arc_t<T>>;
};

struct arc_twin_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_twin<T>)
            return noexcept(std::declval<const T &>().arc_twin(
                std::declval<const arc_t<T> &>()));
        else
            return noexcept(arc_twin(std::declval<const T &>(),
                                     std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_arc_twin<T> || has_adl_arc_twin<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_twin<T>)
            return t.arc_twin(a);
        else
            return arc_twin(t, a);
    }
};

template <typename T>
concept has_member_arc_face = requires(const T & t, const arc_t<T> & a) {
    { t.arc_face(a) } -> std::convertible_to<face_t<T>>;
};

template <typename T>
concept has_adl_arc_face = requires(const T & t, const arc_t<T> & a) {
    { arc_face(t, a) } -> std::convertible_to<face_t<T>>;
};

struct arc_face_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_arc_face<T>)
            return noexcept(std::declval<const T &>().arc_face(
                std::declval<const arc_t<T> &>()));
        else
            return noexcept(arc_face(std::declval<const T &>(),
                                     std::declval<const arc_t<T> &>()));
    }

public:
    template <typename T>
        requires has_member_arc_face<T> || has_adl_arc_face<T>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const arc_t<T> & a) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_arc_face<T>)
            return t.arc_face(a);
        else
            return arc_face(t, a);
    }
};
}  // namespace cpo

inline namespace cust {
inline constexpr cpo::arc_twin_fn arc_twin{};
inline constexpr cpo::arc_face_fn arc_face{};
}  // namespace cust

template <typename T>
concept has_arc_twin = graph<T> && requires(const T & t, const arc_t<T> & a) {
    melon::experimental::arc_twin(t, a);
};

template <typename T>
concept has_arc_face = graph<T> && requires(const T & t, const arc_t<T> & a) {
    melon::experimental::arc_face(t, a);
};

template <typename T>
concept drawable_graph =
    graph<T> &&
    requires(const T & t, const vertex_t<T> & v, const arc_t<T> & a) {
        melon::experimental::vertex_coordinates(t, v);
        melon::experimental::arc_source_coordinates(t, a);
        melon::experimental::arc_target_coordinates(t, a);
    };

template <typename T>
concept planar_subdivision =
    drawable_graph<T> && requires(const T & t, const face_t<T> & f) {
        melon::experimental::faces(t);
        melon::experimental::bounding_arcs(t, f);
    };

template <typename T>
concept planar_map =
    planar_subdivision<T> && requires(const T & t, const arc_t<T> & a) {
        melon::experimental::arc_twin(t, a);
        melon::experimental::arc_face(t, a);
    };

namespace cpo {
template <typename T, typename ValueType>
concept has_member_create_face_map =
    requires(const T & t, const ValueType & d) {
        {
            t.template create_face_map<ValueType>()
        } -> output_mapping_of<face_t<T>, ValueType>;
        {
            t.template create_face_map<ValueType>(d)
        } -> output_mapping_of<face_t<T>, ValueType>;
    };

template <typename T, typename ValueType>
concept has_adl_create_face_map = requires(const T & t, const ValueType & d) {
    {
        create_face_map<ValueType>(t)
    } -> output_mapping_of<face_t<T>, ValueType>;
    {
        create_face_map<ValueType>(t, d)
    } -> output_mapping_of<face_t<T>, ValueType>;
};

// Parameterised on ValueType so that the public name is a variable template,
// invisible to ADL: a function-shaped name would be found by
// has_adl_create_face_map, which then depends on itself.
template <typename ValueType>
struct create_face_map_fn {
private:
    template <typename T>
    static constexpr bool is_noexcept() {
        if constexpr(has_member_create_face_map<T, ValueType>)
            return noexcept(std::declval<const T &>()
                                .template create_face_map<ValueType>());
        else
            return noexcept(
                create_face_map<ValueType>(std::declval<const T &>()));
    }

    // The default-value overload probes the call it actually makes: the two
    // overloads can differ in noexcept-ness.
    template <typename T>
    static constexpr bool is_noexcept_default() {
        if constexpr(has_member_create_face_map<T, ValueType>)
            return noexcept(
                std::declval<const T &>().template create_face_map<ValueType>(
                    std::declval<const ValueType &>()));
        else
            return noexcept(create_face_map<ValueType>(
                std::declval<const T &>(), std::declval<const ValueType &>()));
    }

public:
    template <typename T>
        requires has_member_create_face_map<T, ValueType> ||
                 has_adl_create_face_map<T, ValueType>
    constexpr auto operator() [[nodiscard]] (const T & t) const
        noexcept(is_noexcept<T>()) {
        if constexpr(has_member_create_face_map<T, ValueType>)
            return t.template create_face_map<ValueType>();
        else
            return create_face_map<ValueType>(t);
    }

    template <typename T>
        requires has_member_create_face_map<T, ValueType> ||
                 has_adl_create_face_map<T, ValueType>
    constexpr auto operator()
        [[nodiscard]] (const T & t, const ValueType & d) const
        noexcept(is_noexcept_default<T>()) {
        if constexpr(has_member_create_face_map<T, ValueType>)
            return t.template create_face_map<ValueType>(d);
        else
            return create_face_map<ValueType>(t, d);
    }
};
}  // namespace cpo

inline namespace cust {
template <typename ValueType>
inline constexpr cpo::create_face_map_fn<ValueType> create_face_map{};
}  // namespace cust

template <typename T, typename ValueType = std::size_t>
concept has_face_map =
    planar_map<T> && requires(const T & t, const ValueType & d) {
        melon::experimental::create_face_map<ValueType>(t);
        melon::experimental::create_face_map<ValueType>(t, d);
    };
}  // namespace melon::experimental
