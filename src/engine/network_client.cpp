#include "engine/network_client.h"
#include "engine/http_engine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
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
    std::string errorMessage;

    if (!http.sendHeadRequest(url, fileSize, etag, lastModified, finalUrl, errorMessage)) {
        // Log the real curl error so it appears in service stderr/logs
        std::cerr << "[CurlNetworkClient::head] FAILED url=" << url
                  << " error=" << errorMessage << "\n";
        info.errorMessage = errorMessage;
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
                                       const std::string& outputPath,
                                       ProgressCallback progressCb) {
    // Ensure parent directory exists
    const std::filesystem::path outPath(outputPath);
    const std::filesystem::path parentDir = outPath.parent_path();
    if (!parentDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            std::cerr << "[CurlNetworkClient::downloadToFile] Failed to create directory: "
                      << parentDir.string() << " error=" << ec.message() << "\n";
            return false;
        }
    }

    HttpEngine http;
    std::string errorMessage;
    bool ok = http.downloadSegment(url, range.start, range.end, outputPath,
                                   progressCb, errorMessage);
    if (!ok) {
        std::cerr << "[CurlNetworkClient::downloadToFile] FAILED url=" << url
                  << " range=[" << range.start << "," << range.end << "]"
                  << " output=" << outputPath
                  << " error=" << errorMessage << "\n";
    }
    return ok;
}

// ---------------------------------------------------------------------------
// MockNetworkClient (unchanged below)
// ---------------------------------------------------------------------------

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

void MockNetworkClient::setFailDownloadCount(int count) {
    std::lock_guard<std::mutex> lock(mutex);
    failDownloadCount = count;
}

void MockNetworkClient::setChunkDelay(std::chrono::milliseconds delay) {
    std::lock_guard<std::mutex> lock(mutex);
    chunkDelay = delay;
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
                                       const std::string& outputPath,
                                       ProgressCallback progressCb) {
    std::vector<char> snapshot;
    std::chrono::milliseconds delay{0};
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++downloadCalls;
        ranges.push_back(range);
        if (failDownload || failDownloadCount > 0 || !range.isValid()) {
            if (failDownloadCount > 0) {
                --failDownloadCount;
            }
            return false;
        }
        snapshot = payload;
        delay = chunkDelay;
    }

    const std::filesystem::path path(outputPath);
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    const bool exists = std::filesystem::exists(path);
    std::ios_base::openmode mode = std::ios::binary;
    if (exists && range.start > 0) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
    }

    std::ofstream output(path, mode);
    if (!output.is_open()) {
        return false;
    }

    const auto start = static_cast<std::size_t>(std::min<int64_t>(range.start, snapshot.size()));
    const auto end = static_cast<std::size_t>(std::min<int64_t>(range.end + 1, snapshot.size()));

    constexpr std::size_t kChunkSize = 64;
    std::size_t current = start;

    while (current < end) {
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }

        const std::size_t chunkSize = std::min(kChunkSize, end - current);
        output.write(snapshot.data() + current, static_cast<std::streamsize>(chunkSize));
        output.flush();
        current += chunkSize;

        const int64_t downloadedSoFar = static_cast<int64_t>(current - start);
        if (progressCb) {
            if (!progressCb(downloadedSoFar)) {
                return false;
            }
        }
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
