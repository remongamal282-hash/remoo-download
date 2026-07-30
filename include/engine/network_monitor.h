#ifndef REMO_DOWNLOAD_ENGINE_NETWORK_MONITOR_H
#define REMO_DOWNLOAD_ENGINE_NETWORK_MONITOR_H

#include <functional>
#include <memory>
#include <string>

namespace remo {
namespace engine {

enum class NetworkStatus {
    Online,
    Offline,
    Limited
};

class NetworkMonitor {
public:
    using StatusCallback = std::function<void(NetworkStatus)>;

    NetworkMonitor();
    ~NetworkMonitor();

    void start();
    void stop();
    NetworkStatus currentStatus() const;
    void setStatusCallback(StatusCallback callback);

private:
    NetworkStatus status = NetworkStatus::Online;
    StatusCallback callback;
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_NETWORK_MONITOR_H