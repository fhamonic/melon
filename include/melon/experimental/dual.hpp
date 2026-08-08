#pragma once

// EXPERIMENTAL — ships and is tested (test/experimental.cpp), but carries no
// stability guarantee: anything in melon::experimental may change or
// disappear in any release, including a patch release.

#include <ranges>
#include <utility>

#include "melon/experimental/planar_map.hpp"

namespace melon::experimental {
namespace views {

template <planar_map P>
class dual {
private:
    using vertex = face_t<P>;
    using arc = arc_t<P>;
    using face = vertex_t<P>;

    std::reference_wrapper<const P> _planar_map;

public:
    [[nodiscard]] constexpr explicit dual(const P & g) : _planar_map(g) {}

    [[nodiscard]] constexpr dual(const dual &) = default;
    [[nodiscard]] constexpr dual(dual &&) = default;

    constexpr dual & operator=(const dual &) = default;
    constexpr dual & operator=(dual &&) = default;

    [[nodiscard]] constexpr auto num_vertices() const noexcept(
        noexcept(melon::experimental::num_faces(std::declval<const P &>())))
        requires requires(P g) { melon::experimental::num_faces(g); }
    {
        return melon::experimental::num_faces(_planar_map.get());
    }
    [[nodiscard]] constexpr auto num_arcs() const
        noexcept(noexcept(melon::num_arcs(std::declval<const P &>())))
        requires requires(P g) { melon::num_arcs(g); }
    {
        return melon::num_arcs(_planar_map.get());
    }
    [[nodiscard]] constexpr auto num_faces() const
        noexcept(noexcept(melon::num_vertices(std::declval<const P &>())))
        requires requires(P g) { melon::num_vertices(g); }
    {
        return melon::num_vertices(_planar_map.get());
    }

    [[nodiscard]] constexpr auto vertices() const noexcept(
        noexcept(melon::experimental::faces(std::declval<const P &>()))) {
        return melon::experimental::faces(_planar_map.get());
    }
    [[nodiscard]] constexpr auto arcs() const
        noexcept(noexcept(melon::arcs(std::declval<const P &>()))) {
        return melon::arcs(_planar_map.get());
    }
    [[nodiscard]] constexpr auto faces() const
        noexcept(noexcept(melon::vertices(std::declval<const P &>()))) {
        return melon::vertices(_planar_map.get());
    }

    [[nodiscard]] constexpr auto in_arcs(const vertex u) const
        noexcept(noexcept(
            melon::experimental::bounding_arcs(std::declval<const P &>(), u))) {
        return melon::experimental::bounding_arcs(_planar_map.get(), u);
    }
    [[nodiscard]] constexpr auto out_arcs(const vertex u) const {
        return std::views::transform(
            melon::experimental::bounding_arcs(_planar_map.get(), u),
            [this](auto && a) {
                return melon::experimental::arc_twin(_planar_map.get(), a);
            });
    }

    [[nodiscard]] constexpr vertex arc_source(arc a) const
        noexcept(noexcept(melon::experimental::arc_face(
            std::declval<const P &>(),
            melon::experimental::arc_twin(std::declval<const P &>(), a))))
        requires has_arc_face<P>
    {
        return melon::experimental::arc_face(
            _planar_map.get(),
            melon::experimental::arc_twin(_planar_map.get(), a));
    }
    [[nodiscard]] constexpr vertex arc_target(arc a) const noexcept(
        noexcept(melon::experimental::arc_face(std::declval<const P &>(), a)))
        requires has_arc_face<P>
    {
        return melon::experimental::arc_face(_planar_map.get(), a);
    }

    [[nodiscard]] constexpr arc arc_twin(arc a) const noexcept(
        noexcept(melon::experimental::arc_twin(std::declval<const P &>(), a)))
        requires has_arc_twin<P>
    {
        return melon::experimental::arc_twin(_planar_map.get(), a);
    }

    [[nodiscard]] constexpr face arc_face(arc a) const
        noexcept(noexcept(melon::arc_source(std::declval<const P &>(), a)))
        requires has_arc_source<P>
    {
        return melon::arc_source(_planar_map.get(), a);
    }

    // [[nodiscard]] constexpr auto sources_map()
    //     const noexcept requires has_arc_target<P> {
    //     return melon::arc_targets_map(_planar_map.get());
    // }
    // [[nodiscard]] constexpr auto targets_map()
    //     const noexcept requires has_arc_source<P> {
    //     return melon::arc_sources_map(_planar_map.get());
    // }

    // [[nodiscard]] constexpr auto out_neighbors(
    //     const vertex u) const noexcept requires
    //     inward_adjacency_planar_map<P> { return
    //     melon::in_neighbors(_planar_map.get(), u);
    // }
    // [[nodiscard]] constexpr auto in_neighbors(
    //     const vertex u) const noexcept requires
    //     outward_adjacency_planar_map<P> { return
    //     melon::out_neighbors(_planar_map.get(), u);
    // }

    [[nodiscard]] constexpr auto bounding_arcs(face f) const
        noexcept(noexcept(melon::out_arcs(std::declval<const P &>(), f)))
        requires has_out_arcs<P>
    {
        return melon::out_arcs(_planar_map.get(), f);
    }

    template <typename T>
        requires has_face_map<P, T>
    [[nodiscard]] constexpr auto create_vertex_map() const noexcept(noexcept(
        melon::experimental::create_face_map<T>(std::declval<const P &>()))) {
        return melon::experimental::create_face_map<T>(_planar_map.get());
    }
    template <typename T>
        requires has_face_map<P, T>
    [[nodiscard]] constexpr auto create_vertex_map(const T & default_value)
        const noexcept(noexcept(melon::experimental::create_face_map<T>(
            std::declval<const P &>(), default_value))) {
        return melon::experimental::create_face_map<T>(_planar_map.get(),
                                                       default_value);
    }

    template <typename T>
        requires has_arc_map<P, T>
    [[nodiscard]] constexpr auto create_arc_map() const noexcept(
        noexcept(melon::create_arc_map<T>(std::declval<const P &>()))) {
        return melon::create_arc_map<T>(_planar_map.get());
    }
    template <typename T>
        requires has_arc_map<P, T>
    [[nodiscard]] constexpr auto create_arc_map(const T & default_value) const
        noexcept(noexcept(melon::create_arc_map<T>(std::declval<const P &>(),
                                                   default_value))) {
        return melon::create_arc_map<T>(_planar_map.get(), default_value);
    }

    template <typename T>
        requires has_vertex_map<P, T>
    [[nodiscard]] constexpr auto create_face_map() const noexcept(
        noexcept(melon::create_vertex_map<T>(std::declval<const P &>()))) {
        return melon::create_vertex_map<T>(_planar_map.get());
    }
    template <typename T>
        requires has_vertex_map<P, T>
    [[nodiscard]] constexpr auto create_face_map(const T & default_value) const
        noexcept(noexcept(melon::create_vertex_map<T>(std::declval<const P &>(),
                                                      default_value))) {
        return melon::create_vertex_map<T>(_planar_map.get(), default_value);
    }
};

}  // namespace views
}  // namespace melon::experimental
