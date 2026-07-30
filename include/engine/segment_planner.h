#ifndef REMO_DOWNLOAD_ENGINE_SEGMENT_PLANNER_H
#define REMO_DOWNLOAD_ENGINE_SEGMENT_PLANNER_H

#include "engine/download_engine.h"

#include <cstdint>
#include <vector>

namespace remo {
namespace engine {

class SegmentPlanner {
public:
    static std::vector<Segment> planSegments(int64_t fileSize,
                                             int requestedConnections,
                                             bool supportsRanges);
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_SEGMENT_PLANNER_H
