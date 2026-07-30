#include "engine/segment_planner.h"

#include <gtest/gtest.h>

using remo::engine::SegmentPlanner;

TEST(SegmentPlannerTest, CreatesSingleSegmentWhenRangesAreUnsupported) {
    const auto segments = SegmentPlanner::planSegments(1024, 8, false);

    ASSERT_EQ(segments.size(), 1U);
    EXPECT_EQ(segments[0].startByte, 0);
    EXPECT_EQ(segments[0].endByte, 1023);
}

TEST(SegmentPlannerTest, SplitsFileIntoContiguousRanges) {
    const auto segments = SegmentPlanner::planSegments(10, 4, true);

    ASSERT_EQ(segments.size(), 4U);
    EXPECT_EQ(segments[0].startByte, 0);
    EXPECT_EQ(segments[0].endByte, 2);
    EXPECT_EQ(segments[1].startByte, 3);
    EXPECT_EQ(segments[1].endByte, 5);
    EXPECT_EQ(segments[2].startByte, 6);
    EXPECT_EQ(segments[2].endByte, 7);
    EXPECT_EQ(segments[3].startByte, 8);
    EXPECT_EQ(segments[3].endByte, 9);
}

TEST(SegmentPlannerTest, DoesNotCreateMoreSegmentsThanBytes) {
    const auto segments = SegmentPlanner::planSegments(3, 8, true);

    ASSERT_EQ(segments.size(), 3U);
    EXPECT_EQ(segments[0].startByte, 0);
    EXPECT_EQ(segments[0].endByte, 0);
    EXPECT_EQ(segments[2].startByte, 2);
    EXPECT_EQ(segments[2].endByte, 2);
}
