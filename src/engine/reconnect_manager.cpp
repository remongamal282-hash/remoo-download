#include "engine/reconnect_manager.h"

#include <algorithm>
#include <thread>

namespace remo {
namespace engine {

ReconnectManager::ReconnectManager() = default;

ReconnectManager::ReconnectManager(int maxRetries, int baseDelay, int maxRetryDelay)
    : maxRetries(maxRetries), maxRetryDelay(maxRetryDelay), baseDelay(baseDelay)
{
}

ReconnectManager::~ReconnectManager() = default;

void ReconnectManager::onNetworkFailure(const std::string& error) {
    (void)error;
    retryCount++;
    networkAvailable = false;
}

void ReconnectManager::onNetworkRestored() {
    retryCount = 0;
    networkAvailable = true;
}

void ReconnectManager::resetRetryCount() {
    retryCount = 0;
}

int ReconnectManager::currentRetryDelay() const {
    if (retryCount <= 0) {
        return baseDelay;
    }
    int delay = baseDelay;
    for (int i = 1; i < retryCount && delay < maxRetryDelay; i++) {
        delay *= 2;
    }
    return std::min(delay, maxRetryDelay);
}

bool ReconnectManager::isRetrying() const {
    return retryCount > 0;
}

int ReconnectManager::getRetryCount() const {
    return retryCount;
}

void ReconnectManager::setMaxRetries(int maxRetries) {
    this->maxRetries = maxRetries;
}

int ReconnectManager::getMaxRetries() const {
    return maxRetries;
}

bool ReconnectManager::isMaxRetriesReached() const {
    return retryCount > maxRetries;
}

void ReconnectManager::setBaseDelay(int seconds) {
    this->baseDelay = std::max(0, seconds);
}

void ReconnectManager::setMaxRetryDelay(int seconds) {
    this->maxRetryDelay = std::max(1, seconds);
}

bool ReconnectManager::isNetworkError(const std::string& error) {
    if (error.empty()) {
        return true;
    }
    std::string lower = error;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower.find("404") != std::string::npos ||
        lower.find("403") != std::string::npos ||
        lower.find("401") != std::string::npos ||
        lower.find("not found") != std::string::npos ||
        lower.find("forbidden") != std::string::npos ||
        lower.find("unauthorized") != std::string::npos) {
        return false;
    }
    return true;
}

} // namespace engine
} // namespace remo
