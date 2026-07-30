#ifndef REMO_DOWNLOAD_ENGINE_HTTP_ENGINE_H
#define REMO_DOWNLOAD_ENGINE_HTTP_ENGINE_H

#include <cstdint>
#include <string>

namespace remo {
namespace engine {

class HttpEngine {
public:
    HttpEngine();
    ~HttpEngine();

    bool sendHeadRequest(const std::string& url, int64_t& fileSize, std::string& etag, std::string& lastModified);
    bool supportsRangeRequests(const std::string& url);
    bool downloadSegment(const std::string& url, int64_t startByte, int64_t endByte, const std::string& outputPath);

private:
    bool performHeadRequest(const std::string& url, int64_t& fileSize, std::string& etag, std::string& lastModified);
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_HTTP_ENGINE_H