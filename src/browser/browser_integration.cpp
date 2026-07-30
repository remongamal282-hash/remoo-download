#include "browser/browser_integration.h"

#include <algorithm>
#include <cctype>

namespace remo {
namespace browser {

BrowserIntegration::BrowserIntegration() = default;

BrowserIntegration::~BrowserIntegration() = default;

bool BrowserIntegration::start() {
    running = true;
    return true;
}

void BrowserIntegration::stop() {
    running = false;
}

bool BrowserIntegration::isRunning() const {
    return running;
}

bool BrowserIntegration::onLinkReceived(const std::string& url) {
    if (!filterLink(url)) {
        return false;
    }
    pendingLinks.push_back(url);
    return true;
}

bool BrowserIntegration::filterLink(const std::string& url) const {
    if (url.empty()) {
        return false;
    }
    if (minFileSize > 0) {
    }
    if (!allowedExtensions.empty()) {
        size_t dotPos = url.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = url.substr(dotPos + 1);
            const auto queryPos = ext.find_first_of("?#");
            if (queryPos != std::string::npos) {
                ext = ext.substr(0, queryPos);
            }
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            for (const auto& allowed : allowedExtensions) {
                std::string normalized = allowed;
                std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (normalized == ext || normalized == "." + ext) {
                    return true;
                }
            }
            return false;
        }
    }
    return true;
}

std::vector<std::string> BrowserIntegration::pendingLinks() const {
    return pendingLinks;
}

void BrowserIntegration::setMinFileSize(int64_t bytes) {
    minFileSize = bytes;
}

void BrowserIntegration::setAllowedExtensions(const std::vector<std::string>& extensions) {
    allowedExtensions = extensions;
}

} // namespace browser
} // namespace remo
