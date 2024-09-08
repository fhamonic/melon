#ifndef MELON_VIEWS_DUAL_HPP
#define MELON_VIEWS_DUAL_HPP

#include <algorithm>
#include <ranges>

#include "melon/planar_map.hpp"

namespace fhamonic {
namespace melon {
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

    [[nodiscard]] constexpr decltype(auto) num_vertices() const requires
        requires(P g) {
        melon::num_vertices(g);
    }
    { return melon::num_vertices(_planar_map.get()); }
    [[nodiscard]] constexpr decltype(auto) num_arcs() const noexcept requires
        requires(P g) {
        melon::num_arcs(g);
    }
    { return melon::num_arcs(_planar_map.get()); }

    [[nodiscard]] constexpr decltype(auto) vertices() const noexcept {
        return melon::faces(_planar_map.get());
    }
    [[nodiscard]] constexpr decltype(auto) arcs() const noexcept {
        return melon::arcs(_planar_map.get());
    }

    [[nodiscard]] constexpr vertex arc_source(
        arc a) const noexcept requires has_arc_face<P> {
        return melon::arc_face(_planar_map.get(), a);
    }
    [[nodiscard]] constexpr vertex arc_target(
        arc a) const noexcept requires has_arc_source<P> {
        return melon::arc_face(_planar_map.get(), melon::arc_twin(a));
    }

    // [[nodiscard]] constexpr decltype(auto) sources_map()
    //     const noexcept requires has_arc_target<P> {
    //     return melon::arc_targets_map(_planar_map.get());
    // }
    // [[nodiscard]] constexpr decltype(auto) targets_map()
    //     const noexcept requires has_arc_source<P> {
    //     return melon::arc_sources_map(_planar_map.get());
    // }

    [[nodiscard]] constexpr decltype(auto) in_arcs(
        const vertex u) const noexcept {
        return melon::bounding_arcs(_planar_map.get(), u);
    }
    [[nodiscard]] constexpr decltype(auto) out_arcs(
        const vertex u) const noexcept {
        return std::transform(in_arcs(u), [this](auto && a){ return melon::arc_twin(_planar_map.get(), a);});
    }

    // [[nodiscard]] constexpr decltype(auto) out_neighbors(
    //     const vertex u) const noexcept requires inward_adjacency_planar_map<P> {
    //     return melon::in_neighbors(_planar_map.get(), u);
    // }
    // [[nodiscard]] constexpr decltype(auto) in_neighbors(
    //     const vertex u) const noexcept requires outward_adjacency_planar_map<P> {
    //     return melon::out_neighbors(_planar_map.get(), u);
    // }

    [[nodiscard]] constexpr decltype(auto) faces() const noexcept {
        return melon::vertices(_planar_map.get());
    }

    [[nodiscard]] constexpr arc arc_twin(
        arc a) const noexcept requires has_arc_twin<P> {
        return melon::arc_twin(_planar_map.get(), a);
    }
    
    [[nodiscard]] constexpr face arc_face(
        arc a) const noexcept requires has_arc_source<P> {
        return melon::arc_source(_planar_map.get(), a);
    }

    template <typename T>
    requires has_face_map<P>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map() const noexcept {
        return melon::create_face_map<T>(_planar_map.get());
    }
    template <typename T>
    requires has_face_map<P>
    [[nodiscard]] constexpr decltype(auto) create_vertex_map(
        T default_value) const noexcept {
        return melon::create_face_map<T>(_planar_map.get(), default_value);
    }

    template <typename T>
    requires has_arc_map<P>
    [[nodiscard]] constexpr decltype(auto) create_arc_map() const noexcept {
        return melon::create_arc_map<T>(_planar_map.get());
    }
    template <typename T>
    requires has_arc_map<P>
    [[nodiscard]] constexpr decltype(auto) create_arc_map(
        T default_value) const noexcept {
        return melon::create_arc_map<T>(_planar_map.get(), default_value);
    }

    template <typename T>
    requires has_vertex_map<P>
    [[nodiscard]] constexpr decltype(auto) create_face_map() const noexcept {
        return melon::create_vertex_map<T>(_planar_map.get());
    }
    template <typename T>
    requires has_vertex_map<P>
    [[nodiscard]] constexpr decltype(auto) create_face_map(
        T default_value) const noexcept {
        return melon::create_vertex_map<T>(_planar_map.get(), default_value);
    }
};

}  // namespace views
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_VIEWS_DUAL_HPP