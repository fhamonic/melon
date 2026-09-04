#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include "melon/detail/fill.hpp"
#include "melon/detail/movable_box.hpp"
#include "melon/detail/not_self.hpp"
#include "melon/graph.hpp"
#include "melon/undirected_graph.hpp"
#include "melon/views/graph_view.hpp"
#include "melon/views/undirected_graph_view.hpp"

namespace melon {
namespace detail {

// The provider protocol: the value type as an explicit template argument,
// the role as a value tag, the wrapped graph and, in the filled form, the
// default value. requires-expressions rather than std::invocable, which
// cannot spell an explicit template argument. A generic lambda without
// `<typename T>` is rejected on purpose: the explicit argument would bind its
// invented role parameter instead.
template <typename F, typename T, typename Role, typename G, typename Key>
concept map_provider_serves_bare = requires(const F & f, const G & g) {
    { f.template operator()<T>(Role{}, g) } -> output_mapping_of<Key, T>;
};
template <typename F, typename T, typename Role, typename G, typename Key>
concept map_provider_serves_filled =
    requires(const F & f, const G & g, const T & d) {
        { f.template operator()<T>(Role{}, g, d) } -> output_mapping_of<Key, T>;
    };

// Which lambda owns the request (T, Role): the first one, in the order given,
// serving it in either form. The requested form is called when the owner
// declares it and derived from the other one when it does not -- filled as
// bare + fill, bare as the filled call with a value-initialized T. Order is
// the whole rule: a role-generic lambda listed before a role-specific one
// shadows it for every request, silently.
template <typename T, typename Role, typename G, typename Key, typename... Fs>
struct map_dispatch {
private:
    static constexpr std::size_t n = sizeof...(Fs);
    static constexpr bool bare[n] = {
        map_provider_serves_bare<Fs, T, Role, G, Key>...};
    static constexpr bool filled[n] = {
        map_provider_serves_filled<Fs, T, Role, G, Key>...};

    static constexpr std::size_t first_serving() {
        for(std::size_t i = 0; i < n; ++i)
            if(bare[i] || filled[i]) return i;
        return n;
    }

public:
    static constexpr std::size_t owner = first_serving();
    static constexpr bool native_bare = owner < n && bare[owner];
    static constexpr bool native_filled = owner < n && filled[owner];
    static constexpr bool served =
        owner < n && (native_bare || std::default_initializable<T>);
};

template <typename G>
concept any_graph =
    graph<std::remove_cvref_t<G>> || undirected_graph<std::remove_cvref_t<G>>;

// A type modelling both graph and undirected_graph is wrapped as directed.
template <any_graph G>
[[nodiscard]] constexpr auto any_graph_all(G && g) {
    if constexpr(graph<std::remove_cvref_t<G>>)
        return melon::views::graph_all(std::forward<G>(g));
    else
        return melon::views::undirected_graph_all(std::forward<G>(g));
}

template <any_graph G>
using any_graph_all_t = decltype(any_graph_all(std::declval<G>()));

template <typename G, typename Graph>
concept any_graph_for =
    any_graph<G> && std::constructible_from<Graph, any_graph_all_t<G>>;

template <typename Derived, typename G>
using any_graph_forwarding_interface =
    std::conditional_t<graph<G>, graph_forwarding_interface<Derived, G>,
                       undirected_graph_forwarding_interface<Derived, G>>;

template <typename Derived, typename Graph, typename Key, typename... Fs>
class with_maps_base : public any_graph_forwarding_interface<Derived, Graph> {
private:
    friend any_graph_forwarding_interface<Derived, Graph>;

    using lambdas = std::tuple<Fs...>;

    Graph _graph;
    [[no_unique_address]] movable_box<lambdas> _fs;

    [[nodiscard]] constexpr const Graph & _forwarding_base() const noexcept {
        return _graph;
    }

protected:
    template <typename T, typename Role>
    using dispatch = map_dispatch<T, Role, Graph, Key, Fs...>;

    [[nodiscard]] constexpr const Graph & _wrapped() const noexcept {
        return _graph;
    }

    template <typename T, typename Role>
        requires dispatch<T, Role>::served
    [[nodiscard]] constexpr auto _provided_map() const {
        const auto & f = std::get<dispatch<T, Role>::owner>(*_fs);
        if constexpr(dispatch<T, Role>::native_bare)
            return f.template operator()<T>(Role{}, _graph);
        else
            return f.template operator()<T>(Role{}, _graph, T());
    }
    template <typename T, typename Role, std::ranges::input_range Keys>
        requires dispatch<T, Role>::served
    [[nodiscard]] constexpr auto _provided_map(Keys && keys,
                                               const T & default_value) const {
        const auto & f = std::get<dispatch<T, Role>::owner>(*_fs);
        if constexpr(dispatch<T, Role>::native_filled) {
            return f.template operator()<T>(Role{}, _graph, default_value);
        } else {
            auto map = f.template operator()<T>(Role{}, _graph);
            fill(map, std::forward<Keys>(keys), default_value);
            return map;
        }
    }

public:
    // Both not_self conjuncts first; see subgraph_view's constructor. The one
    // on the base is load-bearing too: declaring Derived's implicit copy
    // constructor resolves the copy of this subobject, where this template
    // is a candidate for G = const with_maps_base &, and any_graph<G> then
    // probes the forwarding members -- a static_cast to the still-incomplete
    // Derived -- as a hard error.
    template <typename G, typename... Fns>
        requires not_self<G, Derived> && not_self<G, with_maps_base> &&
                     any_graph_for<G, Graph> &&
                     (std::constructible_from<Fs, Fns> && ...)
    constexpr explicit with_maps_base(G && g, Fns &&... fns)
        : _graph(any_graph_all(std::forward<G>(g)))
        , _fs(lambdas(std::forward<Fns>(fns)...)) {}

    with_maps_base()
        requires std::default_initializable<Graph> &&
                     std::default_initializable<lambdas>
    = default;
    constexpr with_maps_base(const with_maps_base &) = default;
    constexpr with_maps_base(with_maps_base &&) = default;

    constexpr with_maps_base & operator=(const with_maps_base &) = default;
    constexpr with_maps_base & operator=(with_maps_base &&) = default;

    [[nodiscard]] constexpr Graph base() const &
        requires std::copy_constructible<Graph>
    {
        return _graph;
    }
    [[nodiscard]] constexpr Graph base() && { return std::move(_graph); }
};

}  // namespace detail

// A lambda handing out storage it does not own must point at a heap buffer,
// never at the view (algorithms move the view they hold), and must co-own it
// (shared_ptr): a map extracted from an expiring algorithm keeps only what the
// projection keeps alive, and a raw pointer leaves it reading freed memory.
template <typename Graph, typename... Fs>
    requires(graph_view<Graph> || undirected_graph_view<Graph>) &&
            (sizeof...(Fs) > 0)
class with_vertex_maps_view
    : public detail::with_maps_base<with_vertex_maps_view<Graph, Fs...>, Graph,
                                    vertex_t<Graph>, Fs...> {
private:
    using base_type = detail::with_maps_base<with_vertex_maps_view, Graph,
                                             vertex_t<Graph>, Fs...>;
    template <typename T, typename Role>
    static constexpr bool _served =
        base_type::template dispatch<T, Role>::served;

public:
    using base_type::base_type;

    template <typename T, typename Role = default_role>
        requires _served<T, Role> || has_vertex_map<Graph, T, Role>
    [[nodiscard]] constexpr auto create_vertex_map() const {
        if constexpr(_served<T, Role>)
            return this->template _provided_map<T, Role>();
        else
            return melon::create_vertex_map<T, Role>(this->_wrapped());
    }
    template <typename T, typename Role = default_role>
        requires _served<T, Role> || has_vertex_map<Graph, T, Role>
    [[nodiscard]] constexpr auto create_vertex_map(
        const T & default_value) const {
        if constexpr(_served<T, Role>)
            return this->template _provided_map<T, Role>(
                melon::vertices(this->_wrapped()), default_value);
        else
            return melon::create_vertex_map<T, Role>(this->_wrapped(),
                                                     default_value);
    }
};

template <typename G, typename... Fs>
with_vertex_maps_view(G &&, Fs &&...)
    -> with_vertex_maps_view<detail::any_graph_all_t<G>, std::decay_t<Fs>...>;

// Only forwards: its ranges are the wrapped graph's own, so it is borrowed
// exactly when that graph is.
template <typename G, typename... Fs>
inline constexpr bool enable_borrowed_graph<with_vertex_maps_view<G, Fs...>> =
    enable_borrowed_graph<G>;

template <graph_view Graph, typename... Fs>
    requires(sizeof...(Fs) > 0)
class with_arc_maps_view
    : public detail::with_maps_base<with_arc_maps_view<Graph, Fs...>, Graph,
                                    arc_t<Graph>, Fs...> {
private:
    using base_type =
        detail::with_maps_base<with_arc_maps_view, Graph, arc_t<Graph>, Fs...>;
    template <typename T, typename Role>
    static constexpr bool _served =
        base_type::template dispatch<T, Role>::served;

public:
    using base_type::base_type;

    template <typename T, typename Role = default_role>
        requires _served<T, Role> || has_arc_map<Graph, T, Role>
    [[nodiscard]] constexpr auto create_arc_map() const {
        if constexpr(_served<T, Role>)
            return this->template _provided_map<T, Role>();
        else
            return melon::create_arc_map<T, Role>(this->_wrapped());
    }
    template <typename T, typename Role = default_role>
        requires _served<T, Role> || has_arc_map<Graph, T, Role>
    [[nodiscard]] constexpr auto create_arc_map(const T & default_value) const {
        if constexpr(_served<T, Role>)
            return this->template _provided_map<T, Role>(
                melon::arcs(this->_wrapped()), default_value);
        else
            return melon::create_arc_map<T, Role>(this->_wrapped(),
                                                  default_value);
    }
};

template <typename G, typename... Fs>
with_arc_maps_view(G &&, Fs &&...)
    -> with_arc_maps_view<detail::any_graph_all_t<G>, std::decay_t<Fs>...>;

template <typename G, typename... Fs>
inline constexpr bool enable_borrowed_graph<with_arc_maps_view<G, Fs...>> =
    enable_borrowed_graph<G>;

template <undirected_graph_view Graph, typename... Fs>
    requires(sizeof...(Fs) > 0)
class with_edge_maps_view
    : public detail::with_maps_base<with_edge_maps_view<Graph, Fs...>, Graph,
                                    edge_t<Graph>, Fs...> {
private:
    using base_type = detail::with_maps_base<with_edge_maps_view, Graph,
                                             edge_t<Graph>, Fs...>;
    template <typename T, typename Role>
    static constexpr bool _served =
        base_type::template dispatch<T, Role>::served;

public:
    using base_type::base_type;

    template <typename T, typename Role = default_role>
        requires _served<T, Role> || has_edge_map<Graph, T, Role>
    [[nodiscard]] constexpr auto create_edge_map() const {
        if constexpr(_served<T, Role>)
            return this->template _provided_map<T, Role>();
        else
            return melon::create_edge_map<T, Role>(this->_wrapped());
    }
    template <typename T, typename Role = default_role>
        requires _served<T, Role> || has_edge_map<Graph, T, Role>
    [[nodiscard]] constexpr auto create_edge_map(
        const T & default_value) const {
        if constexpr(_served<T, Role>)
            return this->template _provided_map<T, Role>(
                melon::edges(this->_wrapped()), default_value);
        else
            return melon::create_edge_map<T, Role>(this->_wrapped(),
                                                   default_value);
    }
};

template <typename G, typename... Fs>
with_edge_maps_view(G &&, Fs &&...)
    -> with_edge_maps_view<detail::any_graph_all_t<G>, std::decay_t<Fs>...>;

template <typename G, typename... Fs>
inline constexpr bool enable_borrowed_graph<with_edge_maps_view<G, Fs...>> =
    enable_borrowed_graph<G>;

namespace views {

// The lambdas are copied into the view in the direct call too, unlike
// subgraph's lvalue maps (std::views::transform's rule): a view referencing a
// caller's lambda would dangle as soon as the closure or a temporary lambda
// dies.
struct with_vertex_maps_fn {
    template <typename G, typename... Fs>
        requires melon::detail::any_graph<G> && requires(G && g, Fs &&... fs) {
            with_vertex_maps_view(std::forward<G>(g), std::forward<Fs>(fs)...);
        }
    [[nodiscard]] constexpr auto operator()(G && g, Fs &&... fs) const {
        return with_vertex_maps_view(std::forward<G>(g),
                                     std::forward<Fs>(fs)...);
    }

    template <typename F, typename... Fs>
        requires(!melon::detail::any_graph<F>)
    [[nodiscard]] constexpr auto operator()(F && f, Fs &&... fs) const {
        return detail::adaptor_partial<with_vertex_maps_fn, std::decay_t<F>,
                                       std::decay_t<Fs>...>(
            std::forward<F>(f), std::forward<Fs>(fs)...);
    }
};

inline constexpr with_vertex_maps_fn with_vertex_maps{};

struct with_arc_maps_fn {
    template <typename G, typename... Fs>
        requires graph<std::remove_cvref_t<G>> && requires(G && g,
                                                           Fs &&... fs) {
            with_arc_maps_view(std::forward<G>(g), std::forward<Fs>(fs)...);
        }
    [[nodiscard]] constexpr auto operator()(G && g, Fs &&... fs) const {
        return with_arc_maps_view(std::forward<G>(g), std::forward<Fs>(fs)...);
    }

    template <typename F, typename... Fs>
        requires(!graph<std::remove_cvref_t<F>>)
    [[nodiscard]] constexpr auto operator()(F && f, Fs &&... fs) const {
        return detail::adaptor_partial<with_arc_maps_fn, std::decay_t<F>,
                                       std::decay_t<Fs>...>(
            std::forward<F>(f), std::forward<Fs>(fs)...);
    }
};

inline constexpr with_arc_maps_fn with_arc_maps{};

struct with_edge_maps_fn {
    template <typename G, typename... Fs>
        requires undirected_graph<std::remove_cvref_t<G>> &&
                 requires(G && g, Fs &&... fs) {
                     with_edge_maps_view(std::forward<G>(g),
                                         std::forward<Fs>(fs)...);
                 }
    [[nodiscard]] constexpr auto operator()(G && g, Fs &&... fs) const {
        return with_edge_maps_view(std::forward<G>(g), std::forward<Fs>(fs)...);
    }

    template <typename F, typename... Fs>
        requires(!undirected_graph<std::remove_cvref_t<F>>)
    [[nodiscard]] constexpr auto operator()(F && f, Fs &&... fs) const {
        return detail::adaptor_partial<with_edge_maps_fn, std::decay_t<F>,
                                       std::decay_t<Fs>...>(
            std::forward<F>(f), std::forward<Fs>(fs)...);
    }
};

inline constexpr with_edge_maps_fn with_edge_maps{};

}  // namespace views
}  // namespace melon
