#include "engine/reconnect_manager.h"

#include <algorithm>
#include <thread>

namespace remo {
namespace engine {

ReconnectManager::ReconnectManager() = default;

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

} // namespace engine
} // namespace remo
