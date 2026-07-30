#ifndef REMO_DOWNLOAD_BROWSER_CLIPBOARD_MONITOR_H
#define REMO_DOWNLOAD_BROWSER_CLIPBOARD_MONITOR_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace remo {
namespace browser {

class ClipboardMonitor {
public:
    using LinkCallback = std::function<void(const std::string& url)>;

    ClipboardMonitor();
    ~ClipboardMonitor();

    void start();
    void stop();
    bool isMonitoring() const;
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setCallback(LinkCallback callback);
    void setMinFileSize(int64_t bytes);

private:
    bool enabled = true;
    bool monitoring = false;
    int64_t minFileSize = 0;
    std::string lastClipboardContent;
    LinkCallback callback;
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace browser
} // namespace remo

#endif // REMO_DOWNLOAD_BROWSER_CLIPBOARD_MONITOR_H