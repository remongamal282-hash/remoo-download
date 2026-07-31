#include "engine/download_engine.h"
#include "engine/network_client.h"
#include "engine/network_monitor.h"
#include "engine/reconnect_manager.h"
#include "engine/segment_planner.h"
#include "storage/storage_manager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace remo {
namespace engine {

struct DownloadTask {
    int64_t id = 0;
    DownloadRequest request;
    int64_t fileSize = 0;
    bool supportsRanges = true;
    std::vector<Segment> segments;

    std::atomic<bool> pauseRequested{false};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> isRunning{false};
    std::atomic<bool> isFinished{false};

    int currentRetryCount = 0;
    std::string lastErrorMessage;
    ReconnectManager reconnectManager;

    std::thread runnerThread;
};

class DownloadEngine::Impl {
public:
    int maxConnections = 4;
    std::atomic<int> activeTransfers{0};
    std::atomic<int64_t> nextDownloadId{1};
    mutable std::mutex mutex;

    std::map<int64_t, DownloadProgress> progressById;
    std::map<int64_t, std::vector<Segment>> segmentsById;
    std::map<int64_t, std::shared_ptr<DownloadTask>> tasksById;

    std::unique_ptr<INetworkClient> networkClient;
    remo::storage::StorageManager* storageManager = nullptr;
    std::shared_ptr<NetworkMonitor> networkMonitor;
    bool fastRetryMode = false;
    std::string lastError;

    static void runTask(Impl* d, std::shared_ptr<DownloadTask> task);
};

DownloadEngine::DownloadEngine(int maxConnections)
    : DownloadEngine(maxConnections, std::make_unique<CurlNetworkClient>())
{
}

DownloadEngine::DownloadEngine(int maxConnections, std::unique_ptr<INetworkClient> networkClient)
    : DownloadEngine(maxConnections, std::move(networkClient), nullptr)
{
}

DownloadEngine::DownloadEngine(int maxConnections,
                               std::unique_ptr<INetworkClient> networkClient,
                               remo::storage::StorageManager* storageManager)
    : d(std::make_unique<Impl>())
{
    d->maxConnections = std::max(1, maxConnections);
    d->networkClient = std::move(networkClient);
    d->storageManager = storageManager;
}

DownloadEngine::~DownloadEngine() {
    std::vector<std::shared_ptr<DownloadTask>> tasksToJoin;
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        for (auto& [id, task] : d->tasksById) {
            task->cancelRequested = true;
            tasksToJoin.push_back(task);
        }
    }
    for (auto& task : tasksToJoin) {
        if (task->runnerThread.joinable()) {
            task->runnerThread.join();
        }
    }
}

void DownloadEngine::setStorageManager(remo::storage::StorageManager* storageManager) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->storageManager = storageManager;
}

void DownloadEngine::setNetworkMonitor(std::shared_ptr<NetworkMonitor> monitor) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->networkMonitor = std::move(monitor);
}

void DownloadEngine::setFastRetryMode(bool enabled) {
    std::lock_guard<std::mutex> lock(d->mutex);
    d->fastRetryMode = enabled;
}

int DownloadEngine::recoverUnfinishedDownloads() {
    std::lock_guard<std::mutex> lock(d->mutex);
    if (!d->storageManager) {
        return 0;
    }

    auto unfinished = d->storageManager->getUnfinishedDownloads();
    int recoveredCount = 0;

    for (const auto& record : unfinished) {
        std::filesystem::path outputDir = record.savePath.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(record.savePath);
        std::filesystem::path finalPath = outputDir / record.filename;

        auto segRecords = d->storageManager->getSegments(record.id);
        int64_t totalDownloadedOnDisk = 0;

        for (auto& seg : segRecords) {
            std::filesystem::path partPath = finalPath.string() + ".part" + std::to_string(seg.segmentIndex);
            int64_t actualBytesOnDisk = 0;
            std::error_code ec;
            if (std::filesystem::exists(partPath, ec)) {
                actualBytesOnDisk = static_cast<int64_t>(std::filesystem::file_size(partPath, ec));
                if (ec) actualBytesOnDisk = 0;
            }

            // Disk is source of truth for downloaded_bytes
            if (actualBytesOnDisk != seg.downloadedBytes) {
                seg.downloadedBytes = actualBytesOnDisk;
                d->storageManager->updateSegment(seg.id, seg);
            }
            totalDownloadedOnDisk += seg.downloadedBytes;
        }

        remo::storage::DownloadRecord updatedRecord = record;
        updatedRecord.downloadedBytes = totalDownloadedOnDisk;
        updatedRecord.status = "queued";
        d->storageManager->updateDownload(record.id, updatedRecord);

        recoveredCount++;
    }

    return recoveredCount;
}

void DownloadEngine::Impl::runTask(DownloadEngine::Impl* d, std::shared_ptr<DownloadTask> task) {
    task->isRunning = true;
    task->isFinished = false;

    const auto& request = task->request;
    const std::filesystem::path outputDir = request.savePath.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(request.savePath);
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    const std::filesystem::path finalPath = outputDir / request.filename;

    bool ok = true;
    std::mutex stateMutex;
    std::size_t nextSegment = 0;
    const int workerCount = std::max(1, std::min<int>(d->maxConnections, static_cast<int>(task->segments.size())));

    auto worker = [&]() {
        while (true) {
            std::size_t segmentIndex = 0;
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                if (!ok || task->pauseRequested || task->cancelRequested || nextSegment >= task->segments.size()) {
                    return;
                }
                segmentIndex = nextSegment++;
                task->segments[segmentIndex].status = Segment::Status::Downloading;
            }

            auto& segment = task->segments[segmentIndex];
            const std::filesystem::path partPath =
                finalPath.string() + ".part" + std::to_string(segment.index);
            segment.tempFilePath = partPath.string();

            bool segmentDone = false;
            while (!segmentDone) {
                if (!ok || task->pauseRequested || task->cancelRequested) {
                    segment.status = Segment::Status::Pending;
                    return;
                }

                int64_t existingBytes = 0;
                if (std::filesystem::exists(partPath)) {
                    existingBytes = static_cast<int64_t>(std::filesystem::file_size(partPath, ec));
                    if (ec) existingBytes = 0;
                }
                segment.downloadedBytes = existingBytes;

                int64_t fetchStart = segment.startByte + existingBytes;
                if (fetchStart > segment.endByte) {
                    segmentDone = true;
                    break;
                }

                const ByteRange range{fetchStart, segment.endByte};

                auto progressCb = [&](int64_t newlyDownloaded) -> bool {
                    if (task->pauseRequested || task->cancelRequested) {
                        return false;
                    }

                    const int64_t currentSegmentBytes = existingBytes + newlyDownloaded;
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        segment.downloadedBytes = currentSegmentBytes;
                    }

                    int64_t totalDownloaded = 0;
                    {
                        std::lock_guard<std::mutex> lock(stateMutex);
                        for (const auto& seg : task->segments) {
                            totalDownloaded += seg.downloadedBytes;
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(d->mutex);
                        auto& progress = d->progressById[task->id];
                        progress.downloadedSize = totalDownloaded;
                        progress.progressPercent = task->fileSize > 0
                            ? (static_cast<double>(totalDownloaded) * 100.0 / task->fileSize)
                            : 0.0;
                        progress.activeSegments = workerCount;

                        if (d->storageManager) {
                            d->storageManager->saveCheckpoint(
                                task->id, static_cast<int64_t>(segment.index), std::to_string(currentSegmentBytes));
                        }
                    }

                    return true;
                };

                bool dlSuccess = d->networkClient->downloadToFile(request.url, range, segment.tempFilePath, progressCb);
                if (dlSuccess) {
                    segmentDone = true;
                    task->reconnectManager.resetRetryCount();
                    task->currentRetryCount = 0;
                    break;
                }

                if (task->pauseRequested || task->cancelRequested) {
                    segment.status = Segment::Status::Pending;
                    return;
                }

                // Connection/Network failure handling
                task->currentRetryCount++;
                task->reconnectManager.onNetworkFailure("Network download failed");

                const int maxAllowedRetries = request.maxRetries > 0 ? request.maxRetries : 10;
                if (task->currentRetryCount > maxAllowedRetries) {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    segment.status = Segment::Status::Failed;
                    task->lastErrorMessage = "Network retry limit reached (" + std::to_string(maxAllowedRetries) + " retries)";
                    ok = false;

                    {
                        std::lock_guard<std::mutex> lockD(d->mutex);
                        auto& progress = d->progressById[task->id];
                        progress.statusMessage = "failed";
                        progress.errorMessage = task->lastErrorMessage;
                        progress.retryCount = task->currentRetryCount;
                        if (d->storageManager) {
                            d->storageManager->updateDownloadStatus(task->id, "failed", task->lastErrorMessage);
                        }
                    }
                    return;
                }

                // Update status to RECONNECTING
                {
                    std::lock_guard<std::mutex> lockD(d->mutex);
                    auto& progress = d->progressById[task->id];
                    progress.statusMessage = "reconnecting";
                    progress.retryCount = task->currentRetryCount;
                    if (d->storageManager) {
                        d->storageManager->updateDownloadStatus(task->id, "reconnecting");
                    }
                }

                int delaySec = d->fastRetryMode ? 0 : task->reconnectManager.currentRetryDelay();

                // If NetworkMonitor is offline, wait until network is restored
                if (d->networkMonitor && d->networkMonitor->currentStatus() == NetworkStatus::Offline) {
                    while (!task->pauseRequested && !task->cancelRequested &&
                           d->networkMonitor->currentStatus() == NetworkStatus::Offline) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                    if (d->networkMonitor && d->networkMonitor->currentStatus() == NetworkStatus::Online) {
                        task->reconnectManager.onNetworkRestored();
                        delaySec = 0; // Immediate retry on network restoration!
                    }
                }

                // Backoff wait
                auto sleepStart = std::chrono::steady_clock::now();
                while (delaySec > 0 && !task->pauseRequested && !task->cancelRequested) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - sleepStart).count();
                    if (elapsed >= delaySec) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }

            const int64_t segmentTotalBytes = segment.endByte >= segment.startByte
                ? segment.endByte - segment.startByte + 1
                : 0;

            {
                std::lock_guard<std::mutex> lock(stateMutex);
                segment.downloadedBytes = segmentTotalBytes;
                segment.status = Segment::Status::Completed;
            }

            int64_t totalDownloaded = 0;
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                for (const auto& seg : task->segments) {
                    totalDownloaded += seg.downloadedBytes;
                }
            }

            {
                std::lock_guard<std::mutex> lock(d->mutex);
                auto& progress = d->progressById[task->id];
                progress.downloadedSize = totalDownloaded;
                progress.progressPercent = task->fileSize > 0
                    ? (static_cast<double>(totalDownloaded) * 100.0 / task->fileSize)
                    : 0.0;

                if (d->storageManager) {
                    d->storageManager->saveCheckpoint(
                        task->id, static_cast<int64_t>(segment.index), std::to_string(segmentTotalBytes));
                }
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(workerCount));
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back(worker);
    }
    for (auto& thread : workers) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (task->pauseRequested) {
        std::lock_guard<std::mutex> lock(d->mutex);
        auto& progress = d->progressById[task->id];
        progress.activeSegments = 0;
        progress.statusMessage = "paused";
        d->segmentsById[task->id] = task->segments;
        task->isRunning = false;
        task->isFinished = true;
        d->activeTransfers = std::max(0, d->activeTransfers - 1);
        return;
    }

    if (task->cancelRequested) {
        std::error_code removeEc;
        for (const auto& segment : task->segments) {
            if (!segment.tempFilePath.empty()) {
                std::filesystem::remove(segment.tempFilePath, removeEc);
            }
        }
        std::lock_guard<std::mutex> lock(d->mutex);
        auto& progress = d->progressById[task->id];
        progress.activeSegments = 0;
        progress.statusMessage = "cancelled";
        d->segmentsById[task->id] = task->segments;
        task->isRunning = false;
        task->isFinished = true;
        d->activeTransfers = std::max(0, d->activeTransfers - 1);
        return;
    }

    if (!ok) {
        std::error_code removeEc;
        for (const auto& segment : task->segments) {
            if (!segment.tempFilePath.empty()) {
                std::filesystem::remove(segment.tempFilePath, removeEc);
            }
        }
    }

    if (ok) {
        std::ofstream output(finalPath, std::ios::binary | std::ios::trunc);
        ok = output.is_open();
        for (const auto& segment : task->segments) {
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
        auto& progress = d->progressById[task->id];
        progress.activeSegments = 0;
        progress.statusMessage = ok ? "completed" : "failed";
        if (ok) {
            progress.downloadedSize = task->fileSize;
            progress.progressPercent = 100.0;
        }
        d->segmentsById[task->id] = task->segments;
        task->isRunning = false;
        task->isFinished = true;
        d->activeTransfers = std::max(0, d->activeTransfers - 1);
    }
}

std::string DownloadEngine::getLastError() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->lastError;
}

int64_t DownloadEngine::startDownload(const DownloadRequest& request) {
    if (request.url.empty() || request.filename.empty() || !d->networkClient) {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->lastError = "Invalid request: empty URL/filename or network client uninitialized";
        return 0;
    }

    NetworkResourceInfo resourceInfo;
    if (!d->networkClient->head(request.url, resourceInfo)) {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->lastError = !resourceInfo.errorMessage.empty()
            ? resourceInfo.errorMessage
            : "HEAD request failed for URL: " + request.url;
        return 0;
    }

    const int64_t fileSize = request.fileSize > 0 ? request.fileSize : resourceInfo.contentLength;
    std::vector<Segment> segments =
        SegmentPlanner::planSegments(fileSize, d->maxConnections, resourceInfo.supportsRanges);

    // Use hintId (from DB record) when recovering, so engine ID == DB record ID.
    // Otherwise auto-assign from the atomic counter.
    int64_t downloadId;
    if (request.hintId > 0) {
        downloadId = request.hintId;
        // Advance the counter past this ID to avoid future collisions.
        int64_t expected = d->nextDownloadId.load();
        while (expected <= downloadId) {
            d->nextDownloadId.compare_exchange_weak(expected, downloadId + 1);
            expected = d->nextDownloadId.load();
        }
    } else {
        downloadId = d->nextDownloadId.fetch_add(1);
    }

    auto task = std::make_shared<DownloadTask>();
    task->id = downloadId;
    task->request = request;
    task->fileSize = fileSize;
    task->supportsRanges = resourceInfo.supportsRanges;
    task->segments = segments;

    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->activeTransfers++;
        DownloadProgress progress;
        progress.totalSize = fileSize;
        progress.totalSegments = static_cast<int>(segments.size());
        progress.statusMessage = "downloading";
        d->progressById[downloadId] = progress;
        d->segmentsById[downloadId] = segments;
        d->tasksById[downloadId] = task;
    }

    // Launch asynchronously (Step 2 fix)
    task->runnerThread = std::thread([this, task]() {
        Impl::runTask(d.get(), task);
    });

    return downloadId;
}

bool DownloadEngine::pauseDownload(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = d->tasksById.find(downloadId);
    if (it == d->tasksById.end()) {
        return false;
    }
    it->second->pauseRequested = true;
    d->progressById[downloadId].statusMessage = "paused";
    return true;
}

bool DownloadEngine::resumeDownload(int64_t downloadId) {
    std::shared_ptr<DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        auto it = d->tasksById.find(downloadId);
        if (it == d->tasksById.end()) {
            return false;
        }
        task = it->second;
    }

    if (task->runnerThread.joinable()) {
        task->runnerThread.join();
    }

    if (task->isRunning) {
        return false;
    }

    task->pauseRequested = false;
    task->cancelRequested = false;
    task->isFinished = false;

    {
        std::lock_guard<std::mutex> lock(d->mutex);
        d->activeTransfers++;
        d->progressById[downloadId].statusMessage = "downloading";
    }

    task->runnerThread = std::thread([this, task]() {
        Impl::runTask(d.get(), task);
    });

    return true;
}

bool DownloadEngine::cancelDownload(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = d->tasksById.find(downloadId);
    if (it == d->tasksById.end()) {
        return false;
    }
    it->second->cancelRequested = true;
    d->progressById[downloadId].statusMessage = "cancelled";
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

bool DownloadEngine::waitForDownload(int64_t downloadId, int timeoutMs) {
    std::shared_ptr<DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(d->mutex);
        auto it = d->tasksById.find(downloadId);
        if (it == d->tasksById.end()) {
            return false;
        }
        task = it->second;
    }

    const auto start = std::chrono::steady_clock::now();
    while (!task->isFinished) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > timeoutMs) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (task->runnerThread.joinable()) {
        task->runnerThread.join();
    }

    return true;
}

} // namespace engine
} // namespace remo
