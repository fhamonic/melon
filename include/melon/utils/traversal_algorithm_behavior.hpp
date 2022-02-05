#ifndef MELON_TRAVERSAL_ALGORITHM_BEHAVIOR_HPP
#define MELON_TRAVERSAL_ALGORITHM_BEHAVIOR_HPP

namespace fhamonic {
namespace melon {

enum TraversalAlgorithmBehavior : unsigned char {
    TRACK_NONE = 0b00000000,
    TRACK_PRED_NODES = 0b00000001,
    TRACK_PRED_ARCS = 0b00000010,
    TRACK_DISTANCES = 0b00000100
};

}  // namespace melon
}  // namespace fhamonic

#endif  // MELON_TRAVERSAL_ALGORITHM_BEHAVIOR_HPP
