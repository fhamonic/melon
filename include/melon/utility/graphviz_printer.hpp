#ifndef MELON_UTILITY_GRAPHVIZ_PRINTER_HPP
#define MELON_UTILITY_GRAPHVIZ_PRINTER_HPP

// #include <format>
#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

#include <fmt/format.h>

#include "melon/graph.hpp"
#include "melon/utility/value_map.hpp"

namespace fhamonic {
namespace melon {

template <graph
 G>
class graphviz_printer {
public:
    using vertex = vertex_t<G>;
    using arc = arc_t<G>;

    using point2d = std::pair<double, double>;
    using color = std::tuple<unsigned char, unsigned char, unsigned char>;

    static constexpr color default_vertex_color = {255, 255, 255};
    static constexpr color default_arc_color = {0, 0, 0};

    using vertex_label_map = vertex_map_t<G, std::string>;
    using vertex_pos_map = vertex_map_t<G, point2d>;
    using vertex_size_map = vertex_map_t<G, double>;
    using vertex_color_map = vertex_map_t<G, color>;
    using arc_label_map = arc_map_t<G, std::string>;
    using arc_size_map = arc_map_t<G, double>;
    using arc_color_map = arc_map_t<G, color>;

private:
    std::reference_wrapper<const G> _graph;
    std::optional<vertex_label_map> _vertex_label_map;
    std::optional<vertex_pos_map> _vertex_pos_map;
    vertex_size_map _vertex_size_map;
    vertex_color_map _vertex_color_map;
    std::optional<arc_label_map> _arc_label_map;
    arc_size_map _arc_size_map;
    arc_color_map _arc_color_map;

    double _page_width;
    double _page_height;

public:
    graphviz_printer(const G & g)
        : _graph(g)
        , _vertex_label_map()
        , _vertex_pos_map()
        , _vertex_size_map(create_vertex_map<double>(g, 1))
        , _vertex_color_map(create_vertex_map<color>(g, default_vertex_color))
        , _arc_size_map(create_arc_map<double>(g, 1))
        , _arc_color_map(create_arc_map<color>(g, default_arc_color))
        , _page_width(8)
        , _page_height(11) {}

    graphviz_printer<G> & set_vertex_label(const vertex & v,
                                           const std::string & l) {
        if(!_vertex_label_map.has_value())
            _vertex_label_map.emplace(
                create_vertex_map<std::string>(_graph.get(), ""));
        _vertex_label_map.value()[v] = l;
        return *this;
    }

    template <input_value_map<vertex> LM>
        requires std::convertible_to<mapped_value_t<LM, vertex>, std::string>
    graphviz_printer<G> & set_vertex_label_map(const LM & label_map) {
        if(!_vertex_label_map.has_value())
            _vertex_label_map.emplace(
                create_vertex_map<std::string>(_graph.get()));
        for(auto && u : vertices(_graph.get()))
            _vertex_label_map.value()[u] = label_map[u];
        return *this;
    }

    graphviz_printer<G> & set_vertex_pos(const vertex & v,
                                           const point2d & p) {
        if(!_vertex_pos_map.has_value())
            _vertex_pos_map.emplace(
                create_vertex_map<std::string>(_graph.get(), point2d{0.0,0.0}));
        _vertex_pos_map.value()[v] = p;
        return *this;
    }

    template <input_value_map<arc> PM>
        requires std::convertible_to<mapped_value_t<PM, vertex>, point2d>
    graphviz_printer<G> & set_vertex_pos_map(const PM & pos_map) {
        _vertex_pos_map.emplace(create_vertex_map<point2d>(_graph.get()));
        for(auto && u : vertices(_graph.get()))
            _vertex_pos_map.value()[u] = pos_map[u];
        return *this;
    }

    graphviz_printer<G> & set_vertex_size(const vertex & v,
                                           const double s) {
        _vertex_size_map[v] = s;
        return *this;
    }

    template <input_value_map<vertex> SM>
        requires std::convertible_to<mapped_value_t<SM, vertex>, double>
    graphviz_printer<G> & set_vertex_size_map(const SM & size_map) {
        for(auto && u : vertices(_graph.get()))
            _vertex_size_map[u] = size_map[u];
        return *this;
    }

    graphviz_printer<G> & set_vertex_color(const vertex & v,
                                           const color c) {
        _vertex_color_map[v] = c;
        return *this;
    }

    template <input_value_map<vertex> CM>
        requires std::convertible_to<mapped_value_t<CM, vertex>, color>
    graphviz_printer<G> & set_vertex_color_map(const CM & color_map) {
        for(auto && u : vertices(_graph.get()))
            _vertex_color_map[u] = color_map[u];
        return *this;
    }

    graphviz_printer<G> & set_arc_label(const arc & a,
                                           const std::string l) {
        _arc_label_map[a] = l;
        return *this;
    }

    template <input_value_map<arc> LM>
        requires std::convertible_to<mapped_value_t<LM, arc>, std::string>
    graphviz_printer<G> & set_arc_label_map(const LM & label_map) {
        _arc_label_map.emplace(create_arc_map<std::string>(_graph.get()));
        for(auto && a : arcs(_graph.get()))
            _arc_label_map.value()[a] = label_map[a];
        return *this;
    }

    graphviz_printer<G> & set_arc_size(const arc & a,
                                           const double s) {
        _arc_size_map[a] = s;
        return *this;
    }

    template <input_value_map<arc> SM>
        requires std::convertible_to<mapped_value_t<SM, arc>, double>
    graphviz_printer<G> & set_arc_size_map(const SM & size_map) {
        for(auto && a : arcs(_graph.get())) _arc_size_map[a] = size_map[a];
        return *this;
    }


    graphviz_printer<G> & set_arc_color(const arc & a,
                                           const color & c) {
        _arc_color_map[a] = c;
        return *this;
    }

    template <input_value_map<arc> CM>
        requires std::convertible_to<mapped_value_t<CM, arc>, color>
    graphviz_printer<G> & set_arc_color_map(const CM & color_map) {
        for(auto && u : arcs(_graph.get())) _arc_color_map[u] = color_map[u];
        return *this;
    }

    graphviz_printer<G> & page_size(double width, double height) {
        _page_width = width;
        _page_height = height;
        return *this;
    }

    template <typename OS>
    void print(OS && output) const {
        double min_x, max_x, min_y, max_y;
        min_x = min_y = std::numeric_limits<double>::max();
        max_x = max_y = std::numeric_limits<double>::min();
        double scale;
        if(_vertex_pos_map.has_value()) {
            for(auto && u : vertices(_graph.get())) {
                min_x = std::min(min_x, _vertex_pos_map.value()[u].first);
                max_x = std::max(max_x, _vertex_pos_map.value()[u].first);
                min_y = std::min(min_y, _vertex_pos_map.value()[u].second);
                max_y = std::max(max_y, _vertex_pos_map.value()[u].second);
            }
            scale = std::min(_page_width / (max_x - min_x),
                             _page_height / (max_y - min_y));
        } else
            scale = 1;
        auto scale_x = [&](double x) { return scale * (x - min_x); };
        auto scale_y = [&](double y) { return scale * (y - min_y); };
        auto scale_size = [&](double s) { return scale * s; };

        std::vector<vertex> color_sorted_vertices;
        for(auto && u : vertices(_graph.get()))
            color_sorted_vertices.push_back(u);
        std::ranges::sort(
            color_sorted_vertices, [&](const vertex & a, const vertex & b) {
                return _vertex_color_map[a] < _vertex_color_map[b];
            });

        std::vector<std::pair<arc, std::pair<vertex, vertex>>>
            color_sorted_arcs_entries;
        for(const auto & arc_entry : arcs_entries(_graph.get()))
            color_sorted_arcs_entries.emplace_back(arc_entry);
        std::sort(color_sorted_arcs_entries.begin(),
                  color_sorted_arcs_entries.end(), [&](auto && a, auto && b) {
                      return _arc_color_map[a.first] < _arc_color_map[b.first];
                  });

        fmt::format_to(output, "digraph {{size=\"{},{}\";\n", _page_width,
                       _page_height);
        fmt::format_to(
            output,
            "_graph [pad=\"0.2,0.1\" bgcolor=transparent overlap=scale]\n");
        fmt::format_to(output, "node [style=filled shape=\"circle\"]\n");
        fmt::format_to(output, "edge [style=filled]\n");

        std::optional<color> prev_color;
        for(vertex u : color_sorted_vertices) {
            if(!prev_color.has_value() ||
               _vertex_color_map[u] != prev_color.value()) {
                fmt::format_to(output,
                               "node [fillcolor=\"#{:02x}{:02x}{:02x}\"]\n",
                               std::get<0>(_vertex_color_map[u]),
                               std::get<1>(_vertex_color_map[u]),
                               std::get<2>(_vertex_color_map[u]));
                prev_color.emplace(_vertex_color_map[u]);
            }
            fmt::format_to(output, "{} [width=\"{}\"", u,
                           scale_size(std::sqrt(_vertex_size_map[u])));
            if(_vertex_label_map.has_value()) {
                fmt::format_to(output, " label=\"{}\"",
                               _vertex_label_map.value()[u]);
            }
            if(_vertex_pos_map.has_value()) {
                fmt::format_to(output, " pos=\"{},{}\"]\n",
                               scale_x(_vertex_pos_map.value()[u].first),
                               scale_y(_vertex_pos_map.value()[u].second));
            }
            fmt::format_to(output, "]\n");
        }

        prev_color.reset();
        for(const auto & [a, vertices_pair] : color_sorted_arcs_entries) {
            if(!prev_color.has_value() ||
               _arc_color_map[a] != prev_color.value()) {
                fmt::format_to(output, "edge [color=\"#{:02x}{:02x}{:02x}\"]\n",
                               std::get<0>(_arc_color_map[a]),
                               std::get<1>(_arc_color_map[a]),
                               std::get<2>(_arc_color_map[a]));
                prev_color.emplace(_arc_color_map[a]);
            }
            fmt::format_to(output, "{} -> {} [penwidth=\"{}\"",
                           vertices_pair.first, vertices_pair.second,
                           _arc_size_map[a]);

            if(_arc_label_map.has_value()) {
                fmt::format_to(output, " label=\"{}\"",
                               _arc_label_map.value()[a]);
            }
            fmt::format_to(output, "]\n");
        }

        fmt::format_to(output, "}}\n");
    }
};
}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_UTILITY_GRAPHVIZ_PRINTER_HPP