#include <gtest/gtest.h>
#include "speed/speed_limiter.h"

using namespace remo::speed;

TEST(TokenBucketTest, InitialRate) {
    TokenBucket bucket(1024 * 1024);
    EXPECT_EQ(bucket.getRate(), 1024 * 1024);
}

TEST(TokenBucketTest, ConsumeWithinLimit) {
    TokenBucket bucket(1024 * 1024);
    bucket.refill(1.0);
    EXPECT_TRUE(bucket.consume(512 * 1024));
    EXPECT_GT(bucket.availableTokens(), 0);
}

TEST(TokenBucketTest, ExceedLimit) {
    TokenBucket bucket(1024 * 1024);
    bucket.refill(1.0);
    EXPECT_FALSE(bucket.consume(2 * 1024 * 1024));
}

TEST(TokenBucketTest, RefillOverTime) {
    TokenBucket bucket(1024 * 1024);
    bucket.refill(0.5);
    EXPECT_EQ(bucket.availableTokens(), 512 * 1024);
}

TEST(SpeedLimiterTest, GlobalLimit) {
    SpeedLimiter limiter;
    limiter.setGlobalLimit(1024 * 1024);
    EXPECT_EQ(limiter.getGlobalLimit(), 1024 * 1024);
}