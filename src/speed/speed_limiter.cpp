#include "speed/speed_limiter.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>

namespace remo {
namespace speed {

TokenBucket::TokenBucket(int64_t rateBytesPerSec, int64_t burstBytes)
    : rate(rateBytesPerSec)
    , capacity(burstBytes > 0 ? burstBytes : rateBytesPerSec)
    , tokens(0)
{
    auto now = std::chrono::steady_clock::now();
    lastRefillTime = std::chrono::duration<double>(now.time_since_epoch()).count();
}

void TokenBucket::refill(double seconds) {
    auto now = std::chrono::steady_clock::now();
    double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
    double elapsed = seconds > 0.0 ? seconds : currentTime - lastRefillTime;
    lastRefillTime = currentTime;

    tokens += rate * elapsed;
    if (tokens > capacity) {
        tokens = capacity;
    }
}

bool TokenBucket::consume(int64_t bytes) {
    refill(0.0);
    if (tokens >= bytes) {
        tokens -= bytes;
        return true;
    }
    return false;
}

int64_t TokenBucket::availableTokens() const {
    return tokens;
}

void TokenBucket::setRate(int64_t rateBytesPerSec) {
    rate = rateBytesPerSec;
    capacity = rate;
}

int64_t TokenBucket::getRate() const {
    return rate;
}

class SpeedLimiter::Impl {
public:
    mutable std::mutex mutex;
    int64_t globalLimit = 0;
    std::map<int64_t, int64_t> downloadLimits;
    std::map<int64_t, int64_t> categoryLimits;
    std::map<int64_t, TokenBucket> downloadBuckets;
    std::map<int64_t, TokenBucket> categoryBuckets;
    std::unique_ptr<TokenBucket> globalBucket;
};

SpeedLimiter::SpeedLimiter()
    : d(std::make_unique<Impl>())
{
}

SpeedLimiter::~SpeedLimiter() = default;

void SpeedLimiter::setGlobalLimit(int64_t bytesPerSec) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->globalLimit = std::max<int64_t>(0, bytesPerSec);
    if (d->globalLimit > 0) {
        d->globalBucket = std::make_unique<TokenBucket>(d->globalLimit);
    } else {
        d->globalBucket.reset();
    }
}

void SpeedLimiter::setPerDownloadLimit(int64_t downloadId, int64_t bytesPerSec) {
    std::lock_guard<std::mutex> lock(d->mutex);
    if (bytesPerSec <= 0) {
        d->downloadLimits.erase(downloadId);
        d->downloadBuckets.erase(downloadId);
        return;
    }
    d->downloadLimits[downloadId] = bytesPerSec;
    d->downloadBuckets.erase(downloadId);
    d->downloadBuckets.emplace(downloadId, TokenBucket(bytesPerSec));
}

void SpeedLimiter::setPerCategoryLimit(int64_t categoryId, int64_t bytesPerSec) {
    std::lock_guard<std::mutex> lock(d->mutex);
    if (bytesPerSec <= 0) {
        d->categoryLimits.erase(categoryId);
        d->categoryBuckets.erase(categoryId);
        return;
    }
    d->categoryLimits[categoryId] = bytesPerSec;
    d->categoryBuckets.erase(categoryId);
    d->categoryBuckets.emplace(categoryId, TokenBucket(bytesPerSec));
}

void SpeedLimiter::clearLimit(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->downloadLimits.erase(downloadId);
    d->downloadBuckets.erase(downloadId);
}

void SpeedLimiter::clearCategoryLimit(int64_t categoryId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->categoryLimits.erase(categoryId);
    d->categoryBuckets.erase(categoryId);
}

int64_t SpeedLimiter::getGlobalLimit() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->globalLimit;
}

int64_t SpeedLimiter::getEffectiveLimit(int64_t downloadId, int64_t categoryId) const {
    std::lock_guard<std::mutex> lock(d->mutex);
    int64_t effective = d->globalLimit;
    auto categoryIt = d->categoryLimits.find(categoryId);
    if (categoryIt != d->categoryLimits.end()) {
        effective = effective > 0 ? std::min(effective, categoryIt->second) : categoryIt->second;
    }
    auto downloadIt = d->downloadLimits.find(downloadId);
    if (downloadIt != d->downloadLimits.end()) {
        effective = effective > 0 ? std::min(effective, downloadIt->second) : downloadIt->second;
    }
    return effective;
}

bool SpeedLimiter::allowBytes(int64_t downloadId, int64_t categoryId, int64_t bytes) {
    if (bytes <= 0) {
        return true;
    }
    std::lock_guard<std::mutex> lock(d->mutex);
    if (d->globalBucket && !d->globalBucket->consume(bytes)) {
        return false;
    }
    auto categoryBucket = d->categoryBuckets.find(categoryId);
    if (categoryBucket != d->categoryBuckets.end() && !categoryBucket->second.consume(bytes)) {
        return false;
    }
    auto downloadBucket = d->downloadBuckets.find(downloadId);
    if (downloadBucket != d->downloadBuckets.end() && !downloadBucket->second.consume(bytes)) {
        return false;
    }
    return true;
}

} // namespace speed
} // namespace remo
