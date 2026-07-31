#ifndef REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H
#define REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace storage {
class StorageManager;
}

namespace engine {

class INetworkClient;
class NetworkMonitor;

struct Segment {
    int index = 0;
    int64_t startByte = 0;
    int64_t endByte = 0;
    int64_t downloadedBytes = 0;
    std::string tempFilePath;
    enum class Status { Pending, Downloading, Completed, Failed, Retrying } status = Status::Pending;
};

struct DownloadRequest {
    std::string url;
    std::string filename;
    std::string savePath;
    int64_t fileSize = -1;
    int maxConnections = 4;
    int priority = 0;
    std::string categoryId;
    std::string etag;
    std::string lastModified;
    int maxRetries = 10;
    // When > 0, the engine uses this as the task ID instead of its atomic counter.
    // Used during startup recovery so the engine ID matches the existing DB record ID.
    int64_t hintId = -1;
};

struct DownloadProgress {
    int64_t totalSize = 0;
    int64_t downloadedSize = 0;
    double speedBytesPerSec = 0.0;
    double progressPercent = 0.0;
    int activeSegments = 0;
    int totalSegments = 0;
    int retryCount = 0;
    std::string statusMessage;
    std::string errorMessage;
};

class DownloadEngine {
public:
    explicit DownloadEngine(int maxConnections = 4);
    DownloadEngine(int maxConnections, std::unique_ptr<INetworkClient> networkClient);
    DownloadEngine(int maxConnections, std::unique_ptr<INetworkClient> networkClient,
                   remo::storage::StorageManager* storageManager);
    ~DownloadEngine();

    DownloadEngine(const DownloadEngine&) = delete;
    DownloadEngine& operator=(const DownloadEngine&) = delete;

    void setStorageManager(remo::storage::StorageManager* storageManager);
    void setNetworkMonitor(std::shared_ptr<NetworkMonitor> monitor);
    void setFastRetryMode(bool enabled);

    int recoverUnfinishedDownloads();

    std::string getLastError() const;

    // Starts a download asynchronously. Returns immediately with a download ID.
    // Returns -1 on failure (e.g., invalid request, network error on HEAD).
    int64_t startDownload(const DownloadRequest& request);

    bool pauseDownload(int64_t downloadId);
    bool resumeDownload(int64_t downloadId);
    bool cancelDownload(int64_t downloadId);
    DownloadProgress getProgress(int64_t downloadId);
    bool hasActiveDownloads() const;
    int activeDownloadCount() const;

    // Blocks until a specific download is finished (completed, failed, or cancelled).
    // Used for testing. Returns false if downloadId not found.
    bool waitForDownload(int64_t downloadId, int timeoutMs = 30000);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H
