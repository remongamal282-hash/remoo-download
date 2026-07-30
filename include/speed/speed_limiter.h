#ifndef REMO_DOWNLOAD_SPEED_SPEED_LIMITER_H
#define REMO_DOWNLOAD_SPEED_SPEED_LIMITER_H

#include <cstdint>
#include <memory>
#include <string>

namespace remo {
namespace speed {

class TokenBucket {
public:
    TokenBucket(int64_t rateBytesPerSec, int64_t burstBytes = 0);

    void refill(double seconds);
    bool consume(int64_t bytes);
    int64_t availableTokens() const;
    void setRate(int64_t rateBytesPerSec);
    int64_t getRate() const;

private:
    int64_t rate = 0;
    int64_t capacity = 0;
    int64_t tokens = 0;
    double lastRefillTime = 0.0;
};

class SpeedLimiter {
public:
    SpeedLimiter();
    ~SpeedLimiter();

    void setGlobalLimit(int64_t bytesPerSec);
    void setPerDownloadLimit(int64_t downloadId, int64_t bytesPerSec);
    void setPerCategoryLimit(int64_t categoryId, int64_t bytesPerSec);
    void clearLimit(int64_t downloadId);
    void clearCategoryLimit(int64_t categoryId);

    int64_t getGlobalLimit() const;
    int64_t getEffectiveLimit(int64_t downloadId, int64_t categoryId) const;
    bool allowBytes(int64_t downloadId, int64_t categoryId, int64_t bytes);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace speed
} // namespace remo

#endif // REMO_DOWNLOAD_SPEED_SPEED_LIMITER_H