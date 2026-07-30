#ifndef REMO_DOWNLOAD_ENGINE_NETWORK_CLIENT_H
#define REMO_DOWNLOAD_ENGINE_NETWORK_CLIENT_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace remo {
namespace engine {

using ProgressCallback = std::function<bool(int64_t bytesDownloaded)>;

struct NetworkResourceInfo {
    int64_t contentLength = 0;
    bool supportsRanges = false;
    std::string etag;
    std::string lastModified;
    std::string finalUrl;
};

struct ByteRange {
    int64_t start = 0;
    int64_t end = 0;

    bool isValid() const;
};

class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    virtual bool head(const std::string& url, NetworkResourceInfo& info) = 0;
    virtual bool downloadToFile(const std::string& url,
                                const ByteRange& range,
                                const std::string& outputPath,
                                ProgressCallback progressCb = nullptr) = 0;
};

class CurlNetworkClient : public INetworkClient {
public:
    bool head(const std::string& url, NetworkResourceInfo& info) override;
    bool downloadToFile(const std::string& url,
                        const ByteRange& range,
                        const std::string& outputPath,
                        ProgressCallback progressCb = nullptr) override;
};

class MockNetworkClient : public INetworkClient {
public:
    void setResourceInfo(const NetworkResourceInfo& info);
    void setPayload(std::vector<char> data);
    void setFailHead(bool shouldFail);
    void setFailDownload(bool shouldFail);
    void setFailDownloadCount(int count);
    void setChunkDelay(std::chrono::milliseconds delay);

    bool head(const std::string& url, NetworkResourceInfo& info) override;
    bool downloadToFile(const std::string& url,
                        const ByteRange& range,
                        const std::string& outputPath,
                        ProgressCallback progressCb = nullptr) override;

    int headCallCount() const;
    int downloadCallCount() const;
    std::vector<ByteRange> requestedRanges() const;

private:
    mutable std::mutex mutex;
    NetworkResourceInfo resourceInfo;
    std::vector<char> payload;
    bool failHead = false;
    bool failDownload = false;
    int failDownloadCount = 0;
    std::chrono::milliseconds chunkDelay{0};
    int headCalls = 0;
    int downloadCalls = 0;
    std::vector<ByteRange> ranges;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_NETWORK_CLIENT_H
