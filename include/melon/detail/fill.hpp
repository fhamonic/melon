#pragma once

#include <ranges>

namespace melon::detail {

template <typename Map, typename Value>
concept member_fillable =
    requires(Map & map, const Value & value) { map.fill(value); };

// Assign `value` at every key in `keys` -- the algorithms' reset primitive.
// A member fill(value) is taken as a whole-domain shortcut; without one the
// keys are written one by one, because the factories' contract
// (output_mapping) gives no way to enumerate a map's domain -- only the
// caller, holding the graph, can name the keys.
template <typename Map, std::ranges::input_range Keys, typename Value>
constexpr void fill(Map & map, Keys && keys, const Value & value) {
    if constexpr(member_fillable<Map, Value>) {
        map.fill(value);
    } else {
        for(auto && key : keys) map[key] = value;
    }
}

}  // namespace melon::detail
