#include "browser/clipboard_monitor.h"

namespace remo {
namespace browser {

ClipboardMonitor::ClipboardMonitor() = default;

ClipboardMonitor::~ClipboardMonitor() = default;

void ClipboardMonitor::start() {
    monitoring = true;
}

void ClipboardMonitor::stop() {
    monitoring = false;
}

bool ClipboardMonitor::isMonitoring() const {
    return monitoring;
}

void ClipboardMonitor::setEnabled(bool enabledValue) {
    enabled = enabledValue;
    if (!enabled) {
        stop();
    }
}

bool ClipboardMonitor::isEnabled() const {
    return enabled;
}

void ClipboardMonitor::setCallback(LinkCallback callback) {
    this->callback = callback;
}

void ClipboardMonitor::setMinFileSize(int64_t bytes) {
    minFileSize = bytes;
}

} // namespace browser
} // namespace remo
