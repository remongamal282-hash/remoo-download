#include <gtest/gtest.h>

#include "categories/category_engine.h"

using namespace remo::categories;

TEST(CategoryEngineTest, ClassifiesByExtension) {
    CategoryEngine engine;
    const auto videoId = engine.addCategory("Video", "downloads/video");
    engine.addRule(videoId, "extension", "mp4");

    EXPECT_EQ(engine.classify("movie.mp4", "https://example.com/movie.mp4"), "Video");
}

TEST(CategoryEngineTest, ReturnsDefaultWhenNoRuleMatches) {
    CategoryEngine engine;

    EXPECT_EQ(engine.classify("notes.txt", "https://example.com/notes.txt"), "Default");
}
