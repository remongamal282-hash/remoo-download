#include "engine/segment_planner.h"

#include <algorithm>

namespace remo {
namespace engine {

std::vector<Segment> SegmentPlanner::planSegments(int64_t fileSize,
                                                  int requestedConnections,
                                                  bool supportsRanges) {
    const int connections = std::max(1, requestedConnections);
    if (!supportsRanges || fileSize <= 0) {
        Segment segment;
        segment.index = 0;
        segment.startByte = 0;
        segment.endByte = fileSize > 0 ? fileSize - 1 : 0;
        return {segment};
    }

    const int segmentCount = static_cast<int>(std::max<int64_t>(1, std::min<int64_t>(connections, fileSize)));
    const int64_t baseSize = fileSize / segmentCount;
    const int64_t remainder = fileSize % segmentCount;

    std::vector<Segment> segments;
    segments.reserve(static_cast<std::size_t>(segmentCount));

    int64_t nextStart = 0;
    for (int i = 0; i < segmentCount; ++i) {
        const int64_t currentSize = baseSize + (i < remainder ? 1 : 0);
        Segment segment;
        segment.index = i;
        segment.startByte = nextStart;
        segment.endByte = nextStart + currentSize - 1;
        segments.push_back(segment);
        nextStart = segment.endByte + 1;
    }

    return segments;
}

} // namespace engine
} // namespace remo
