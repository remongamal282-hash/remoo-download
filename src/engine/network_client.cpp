#include "engine/network_client.h"
#include "engine/http_engine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

namespace remo {
namespace engine {

bool ByteRange::isValid() const {
    return start >= 0 && end >= start;
}

bool CurlNetworkClient::head(const std::string& url, NetworkResourceInfo& info) {
    HttpEngine http;
    int64_t fileSize = 0;
    std::string etag;
    std::string lastModified;
    std::string finalUrl;
    if (!http.sendHeadRequest(url, fileSize, etag, lastModified, finalUrl)) {
        return false;
    }

    info.contentLength = fileSize;
    info.supportsRanges = http.supportsRangeRequests(url);
    info.etag = etag;
    info.lastModified = lastModified;
    info.finalUrl = finalUrl.empty() ? url : finalUrl;
    return true;
}

bool CurlNetworkClient::downloadToFile(const std::string& url,
                                       const ByteRange& range,
                                       const std::string& outputPath) {
    HttpEngine http;
    return http.downloadSegment(url, range.start, range.end, outputPath);
}

void MockNetworkClient::setResourceInfo(const NetworkResourceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex);
    resourceInfo = info;
}

void MockNetworkClient::setPayload(std::vector<char> data) {
    std::lock_guard<std::mutex> lock(mutex);
    payload = std::move(data);
}

void MockNetworkClient::setFailHead(bool shouldFail) {
    std::lock_guard<std::mutex> lock(mutex);
    failHead = shouldFail;
}

void MockNetworkClient::setFailDownload(bool shouldFail) {
    std::lock_guard<std::mutex> lock(mutex);
    failDownload = shouldFail;
}

bool MockNetworkClient::head(const std::string&, NetworkResourceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex);
    ++headCalls;
    if (failHead) {
        return false;
    }
    info = resourceInfo;
    if (info.contentLength == 0 && !payload.empty()) {
        info.contentLength = static_cast<int64_t>(payload.size());
    }
    return true;
}

bool MockNetworkClient::downloadToFile(const std::string&,
                                       const ByteRange& range,
                                       const std::string& outputPath) {
    std::vector<char> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++downloadCalls;
        ranges.push_back(range);
        if (failDownload || !range.isValid()) {
            return false;
        }
        snapshot = payload;
    }

    const std::filesystem::path path(outputPath);
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    const auto start = static_cast<std::size_t>(std::min<int64_t>(range.start, snapshot.size()));
    const auto end = static_cast<std::size_t>(std::min<int64_t>(range.end + 1, snapshot.size()));
    if (start < end) {
        output.write(snapshot.data() + start, static_cast<std::streamsize>(end - start));
    }
    return static_cast<bool>(output);
}

int MockNetworkClient::headCallCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return headCalls;
}

int MockNetworkClient::downloadCallCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return downloadCalls;
}

std::vector<ByteRange> MockNetworkClient::requestedRanges() const {
    std::lock_guard<std::mutex> lock(mutex);
    return ranges;
}

} // namespace engine
} // namespace remo
