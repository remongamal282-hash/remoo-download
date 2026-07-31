#ifndef REMO_DOWNLOAD_ENGINE_HTTP_ENGINE_H
#define REMO_DOWNLOAD_ENGINE_HTTP_ENGINE_H

#include <cstdint>
#include <functional>
#include <string>

namespace remo {
namespace engine {

class HttpEngine {
public:
    HttpEngine();
    ~HttpEngine();

    // HEAD requests — last overload includes verbose errorMessage output
    bool sendHeadRequest(const std::string& url, int64_t& fileSize,
                         std::string& etag, std::string& lastModified);
    bool sendHeadRequest(const std::string& url, int64_t& fileSize,
                         std::string& etag, std::string& lastModified,
                         std::string& finalUrl);
    bool sendHeadRequest(const std::string& url, int64_t& fileSize,
                         std::string& etag, std::string& lastModified,
                         std::string& finalUrl, std::string& errorMessage);

    bool supportsRangeRequests(const std::string& url);

    // Download segment — legacy (no progressCb, no errorMessage)
    bool downloadSegment(const std::string& url, int64_t startByte,
                         int64_t endByte, const std::string& outputPath);

    // Download segment — with real progressCb (bytes downloaded so far → return false to abort)
    //                     and errorMessage out-param for diagnostics
    bool downloadSegment(const std::string& url, int64_t startByte, int64_t endByte,
                         const std::string& outputPath,
                         std::function<bool(int64_t)> progressCb,
                         std::string& errorMessage);

private:
    bool performHeadRequest(const std::string& url,
                            int64_t& fileSize,
                            std::string& etag,
                            std::string& lastModified,
                            std::string& finalUrl,
                            std::string& errorMessage);
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_HTTP_ENGINE_H
