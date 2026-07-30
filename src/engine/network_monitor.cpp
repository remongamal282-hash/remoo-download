#include "engine/network_monitor.h"

#include <thread>

namespace remo {
namespace engine {

class NetworkMonitor::Impl {
public:
    bool running = false;
};

NetworkMonitor::NetworkMonitor()
    : d(std::make_unique<Impl>())
{
}

NetworkMonitor::~NetworkMonitor() = default;

void NetworkMonitor::start() {
    d->running = true;
    status = NetworkStatus::Online;
    if (callback) {
        callback(status);
    }
}

void NetworkMonitor::stop() {
    d->running = false;
}

NetworkStatus NetworkMonitor::currentStatus() const {
    return status;
}

void NetworkMonitor::setStatusCallback(StatusCallback callback) {
    this->callback = callback;
}

} // namespace engine
} // namespace remo
