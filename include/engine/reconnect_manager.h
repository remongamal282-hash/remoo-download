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
    explicit ReconnectManager(int maxRetries, int baseDelay = 5, int maxRetryDelay = 300);
    ~ReconnectManager();

    void onNetworkFailure(const std::string& error);
    void onNetworkRestored();
    void resetRetryCount();
    int currentRetryDelay() const;
    bool isRetrying() const;
    int getRetryCount() const;

    void setMaxRetries(int maxRetries);
    int getMaxRetries() const;
    bool isMaxRetriesReached() const;

    void setBaseDelay(int seconds);
    void setMaxRetryDelay(int seconds);

    static bool isNetworkError(const std::string& error);

private:
    int retryCount = 0;
    int maxRetries = 10;
    int maxRetryDelay = 300;
    int baseDelay = 5;
    bool networkAvailable = true;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_RECONNECT_MANAGER_H