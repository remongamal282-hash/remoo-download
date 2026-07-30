#ifndef REMO_DOWNLOAD_ENGINE_NETWORK_CLIENT_H
#define REMO_DOWNLOAD_ENGINE_NETWORK_CLIENT_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace remo {
namespace engine {

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
                                const std::string& outputPath) = 0;
};

class CurlNetworkClient : public INetworkClient {
public:
    bool head(const std::string& url, NetworkResourceInfo& info) override;
    bool downloadToFile(const std::string& url,
                        const ByteRange& range,
                        const std::string& outputPath) override;
};

class MockNetworkClient : public INetworkClient {
public:
    void setResourceInfo(const NetworkResourceInfo& info);
    void setPayload(std::vector<char> data);
    void setFailHead(bool shouldFail);
    void setFailDownload(bool shouldFail);

    bool head(const std::string& url, NetworkResourceInfo& info) override;
    bool downloadToFile(const std::string& url,
                        const ByteRange& range,
                        const std::string& outputPath) override;

    int headCallCount() const;
    int downloadCallCount() const;
    std::vector<ByteRange> requestedRanges() const;

private:
    mutable std::mutex mutex;
    NetworkResourceInfo resourceInfo;
    std::vector<char> payload;
    bool failHead = false;
    bool failDownload = false;
    int headCalls = 0;
    int downloadCalls = 0;
    std::vector<ByteRange> ranges;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_NETWORK_CLIENT_H
