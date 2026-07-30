#ifndef REMO_DOWNLOAD_BROWSER_BROWSER_INTEGRATION_H
#define REMO_DOWNLOAD_BROWSER_BROWSER_INTEGRATION_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace browser {

class BrowserIntegration {
public:
    BrowserIntegration();
    ~BrowserIntegration();

    bool start();
    void stop();
    bool isRunning() const;

    bool onLinkReceived(const std::string& url);
    bool filterLink(const std::string& url) const;
    std::vector<std::string> pendingLinks() const;

    void setMinFileSize(int64_t bytes);
    void setAllowedExtensions(const std::vector<std::string>& extensions);

private:
    bool running = false;
    int64_t minFileSize = 0;
    std::vector<std::string> allowedExtensions;
    std::vector<std::string> pendingLinks_;
};

} // namespace browser
} // namespace remo

#endif // REMO_DOWNLOAD_BROWSER_BROWSER_INTEGRATION_H