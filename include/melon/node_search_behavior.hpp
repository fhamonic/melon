#ifndef MELON_NODE_SEARCH_BEHAVIOR
#define MELON_NODE_SEARCH_BEHAVIOR

namespace fhamonic {
namespace melon {

enum NodeSeachBehavior : unsigned char {
    TRACK_NONE = 0b00000000,
    TRACK_PRED_NODES = 0b00000001,
    TRACK_PRED_ARCS = 0b00000010,
    TRACK_DISTANCES = 0b00000100
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_NODE_SEARCH_BEHAVIOR
