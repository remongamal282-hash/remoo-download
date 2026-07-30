#include "engine/download_engine.h"
#include "engine/network_client.h"
#include "engine/segment_planner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace remo {
namespace engine {

class DownloadEngine::Impl {
public:
    int maxConnections = 4;
    int activeTransfers = 0;
    int64_t nextDownloadId = 1;
    mutable std::mutex mutex;
    std::map<int64_t, DownloadProgress> progressById;
    std::map<int64_t, std::vector<Segment>> segmentsById;
    std::unique_ptr<INetworkClient> networkClient;
};

DownloadEngine::DownloadEngine(int maxConnections)
    : DownloadEngine(maxConnections, std::make_unique<CurlNetworkClient>())
{
}

DownloadEngine::DownloadEngine(int maxConnections, std::unique_ptr<INetworkClient> networkClient)
    : d(std::make_unique<Impl>())
{
    d->maxConnections = std::max(1, maxConnections);
    d->networkClient = std::move(networkClient);
}

DownloadEngine::~DownloadEngine() = default;

bool DownloadEngine::startDownload(const DownloadRequest& request) {
    if (request.url.empty() || request.filename.empty() || !d->networkClient) {
        return false;
    }

    NetworkResourceInfo resourceInfo;
    if (!d->networkClient->head(request.url, resourceInfo)) {
        return false;
    }

    const int64_t fileSize = request.fileSize > 0 ? request.fileSize : resourceInfo.contentLength;
    std::vector<Segment> segments =
        SegmentPlanner::planSegments(fileSize, d->maxConnections, resourceInfo.supportsRanges);

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
    std::mutex stateMutex;
    std::size_t nextSegment = 0;
    const int workerCount = std::max(1, std::min<int>(d->maxConnections, static_cast<int>(segments.size())));

    auto worker = [&]() {
        while (true) {
            std::size_t segmentIndex = 0;
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                if (!ok || nextSegment >= segments.size()) {
                    return;
                }
                segmentIndex = nextSegment++;
                segments[segmentIndex].status = Segment::Status::Downloading;
            }

            auto& segment = segments[segmentIndex];
            const std::filesystem::path partPath =
                finalPath.string() + ".part" + std::to_string(segment.index);
            segment.tempFilePath = partPath.string();

            const ByteRange range{segment.startByte, segment.endByte};
            if (!d->networkClient->downloadToFile(request.url, range, segment.tempFilePath)) {
                std::lock_guard<std::mutex> lock(stateMutex);
                segment.status = Segment::Status::Failed;
                ok = false;
                return;
            }

            const int64_t segmentBytes = segment.endByte >= segment.startByte
                ? segment.endByte - segment.startByte + 1
                : 0;

            {
                std::lock_guard<std::mutex> lock(stateMutex);
                segment.downloadedBytes = segmentBytes;
                segment.status = Segment::Status::Completed;
                downloaded += segment.downloadedBytes;

                std::lock_guard<std::mutex> progressLock(d->mutex);
                auto& progress = d->progressById[downloadId];
                progress.downloadedSize = downloaded;
                progress.progressPercent = fileSize > 0
                    ? (static_cast<double>(downloaded) * 100.0 / fileSize)
                    : 0.0;
                progress.activeSegments = workerCount;
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(workerCount));
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back(worker);
    }
    for (auto& thread : workers) {
        thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(d->mutex);
        auto& progress = d->progressById[downloadId];
        progress.activeSegments = 0;
    }

    if (!ok) {
        std::error_code removeEc;
        for (const auto& segment : segments) {
            if (!segment.tempFilePath.empty()) {
                std::filesystem::remove(segment.tempFilePath, removeEc);
            }
        }
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
