#pragma once

#include <type_traits>

namespace melon {

// Opt-in trait, mirroring std::ranges::enable_borrowed_range: true when the
// ranges a graph hands out stay valid independently of the *graph object*
// itself -- when the object is a handle to storage living elsewhere rather
// than something the ranges point back at.
//
// It is what tells an algorithm whether it may copy a cached incidence range.
// graph_ref_view is a bare pointer, so out_arcs(v) of the graph it names is
// unaffected by relocating the view. views::subgraph is not: its filtered
// ranges capture `this`, so a range obtained from a subgraph that an algorithm
// stores *by value* points back at that member, and a memberwise copy of the
// algorithm aims the new object's cursors at the old object's graph -- a
// use-after-free the moment the original dies.
//
// Not made redundant by consumable_input_view's own counter, which covers the
// other half of the problem: there the iterator refers into a range the cursor
// owns, so it can be re-derived. Here the range refers to something the cursor
// neither owns nor can re-obtain.
//
// Default false, so a graph type is presumed unsafe to copy cached ranges from
// until it says otherwise. Specialise it for a view whose ranges are
// self-contained or refer only to storage outside the view.
template <typename G>
inline constexpr bool enable_borrowed_graph = false;

template <typename G>
concept borrowed_graph = enable_borrowed_graph<std::remove_cvref_t<G>>;

}  // namespace melon
