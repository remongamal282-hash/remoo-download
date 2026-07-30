#ifndef REMO_DOWNLOAD_ENGINE_RECONNECT_MANAGER_H
#define REMO_DOWNLOAD_ENGINE_RECONNECT_MANAGER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace remo {
namespace engine {

class ReconnectManager {
public:
    using ReconnectCallback = std::function<void()>;

    ReconnectManager();
    ~ReconnectManager();

    void onNetworkFailure(const std::string& error);
    void onNetworkRestored();
    void resetRetryCount();
    int currentRetryDelay() const;
    bool isRetrying() const;

private:
    int retryCount = 0;
    int maxRetryDelay = 60;
    int baseDelay = 5;
    bool networkAvailable = true;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_RECONNECT_MANAGER_H