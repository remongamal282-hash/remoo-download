#include "engine/download_engine.h"
#include "engine/http_engine.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

namespace remo {
namespace engine {

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* stream = static_cast<std::ofstream*>(userp);
    size_t totalSize = size * nmemb;
    stream->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t headerCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* headers = static_cast<std::vector<std::string>*>(userp);
    size_t totalSize = size * nmemb;
    std::string headerLine(static_cast<char*>(contents), totalSize);
    headers->push_back(headerLine);
    return totalSize;
}

} // namespace

class DownloadEngine::Impl {
public:
    CURLM* multiHandle = nullptr;
    int maxConnections = 4;
    int activeTransfers = 0;
    int64_t nextDownloadId = 1;
    mutable std::mutex mutex;
    std::map<int64_t, DownloadProgress> progressById;
    std::map<int64_t, std::vector<Segment>> segmentsById;
    std::vector<CURL*> easyHandles;
};

DownloadEngine::DownloadEngine(int maxConnections)
    : d(std::make_unique<Impl>())
{
    d->maxConnections = maxConnections;
    d->multiHandle = curl_multi_init();
}

DownloadEngine::~DownloadEngine() {
    if (d->multiHandle) {
        curl_multi_cleanup(d->multiHandle);
    }
}

bool DownloadEngine::startDownload(const DownloadRequest& request) {
    if (request.url.empty()) {
        return false;
    }

    int64_t fileSize = 0;
    std::string etag;
    std::string lastModified;

    if (!HttpEngine().sendHeadRequest(request.url, fileSize, etag, lastModified)) {
        return false;
    }

    HttpEngine http;
    bool supportsRange = http.supportsRangeRequests(request.url);
    std::vector<Segment> segments;

    if (supportsRange && fileSize > 0) {
        int numSegments = std::min(d->maxConnections, static_cast<int>(fileSize / (1024 * 1024)));
        if (numSegments < 1) {
            numSegments = 1;
        }
        int64_t segmentSize = fileSize / numSegments;

        for (int i = 0; i < numSegments; i++) {
            Segment seg;
            seg.index = i;
            seg.startByte = i * segmentSize;
            seg.endByte = (i == numSegments - 1) ? fileSize - 1 : (i + 1) * segmentSize - 1;
            seg.status = Segment::Status::Pending;
            segments.push_back(seg);
        }
    } else {
        Segment seg;
        seg.index = 0;
        seg.startByte = 0;
        seg.endByte = fileSize > 0 ? fileSize - 1 : 0;
        seg.status = Segment::Status::Pending;
        segments.push_back(seg);
    }

    const int64_t downloadId = d->nextDownloadId++;
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->activeTransfers++;
        DownloadProgress progress;
        progress.totalSize = fileSize;
        progress.totalSegments = static_cast<int>(segments.size());
        progress.statusMessage = "downloading";
        d->progressById[downloadId] = progress;
        d->segmentsById[downloadId] = segments;
    }

    const std::filesystem::path outputDir = request.savePath.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(request.savePath);
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    const std::filesystem::path finalPath = outputDir / request.filename;

    bool ok = true;
    int64_t downloaded = 0;
    for (auto& segment : segments) {
        segment.status = Segment::Status::Downloading;
        const std::filesystem::path partPath =
            finalPath.string() + ".part" + std::to_string(segment.index);
        segment.tempFilePath = partPath.string();

        if (!http.downloadSegment(request.url, segment.startByte, segment.endByte, segment.tempFilePath)) {
            segment.status = Segment::Status::Failed;
            ok = false;
            break;
        }

        segment.downloadedBytes = segment.endByte >= segment.startByte
            ? segment.endByte - segment.startByte + 1
            : 0;
        downloaded += segment.downloadedBytes;
        segment.status = Segment::Status::Completed;

        std::lock_guard<std::mutex> lock(d->mutex);
        auto& progress = d->progressById[downloadId];
        progress.downloadedSize = downloaded;
        progress.progressPercent = fileSize > 0 ? (static_cast<double>(downloaded) * 100.0 / fileSize) : 0.0;
        progress.activeSegments = 1;
    }

    if (ok) {
        std::ofstream output(finalPath, std::ios::binary | std::ios::trunc);
        ok = output.is_open();
        for (const auto& segment : segments) {
            if (!ok) {
                break;
            }
            std::ifstream input(segment.tempFilePath, std::ios::binary);
            if (!input.is_open()) {
                ok = false;
                break;
            }
            output << input.rdbuf();
            input.close();
            std::filesystem::remove(segment.tempFilePath, ec);
        }
    }

    {
        std::lock_guard<std::mutex> lock(d->mutex);
        auto& progress = d->progressById[downloadId];
        progress.activeSegments = 0;
        progress.statusMessage = ok ? "completed" : "failed";
        if (ok) {
            progress.downloadedSize = fileSize;
            progress.progressPercent = 100.0;
        }
        d->segmentsById[downloadId] = segments;
        d->activeTransfers = std::max(0, d->activeTransfers - 1);
    }

    return ok;
}

bool DownloadEngine::pauseDownload(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto progress = d->progressById.find(downloadId);
    if (progress == d->progressById.end()) {
        return false;
    }
    progress->second.statusMessage = "paused";
    return true;
}

bool DownloadEngine::resumeDownload(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto progress = d->progressById.find(downloadId);
    if (progress == d->progressById.end()) {
        return false;
    }
    progress->second.statusMessage = "queued";
    return true;
}

bool DownloadEngine::cancelDownload(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto progress = d->progressById.find(downloadId);
    if (progress == d->progressById.end()) {
        return false;
    }
    progress->second.statusMessage = "cancelled";
    return true;
}

DownloadProgress DownloadEngine::getProgress(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = d->progressById.find(downloadId);
    if (it != d->progressById.end()) {
        return it->second;
    }
    return {};
}

bool DownloadEngine::hasActiveDownloads() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->activeTransfers > 0;
}

int DownloadEngine::activeDownloadCount() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->activeTransfers;
}

} // namespace engine
} // namespace remo
