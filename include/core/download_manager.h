#ifndef REMO_DOWNLOAD_CORE_DOWNLOAD_MANAGER_H
#define REMO_DOWNLOAD_CORE_DOWNLOAD_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace core {

struct DownloadInfo {
    int64_t id = 0;
    std::string url;
    std::string filename;
    std::string savePath;
    int64_t fileSize = 0;
    int64_t downloadedSize = 0;
    std::string status;
    int priority = 0;
    std::string categoryId;
    int maxConnections = 4;
    double speedBytesPerSec = 0.0;
    double progressPercent = 0.0;
};

enum class DownloadStatus {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled
};

class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();

    int64_t addDownload(const DownloadInfo& info);
    bool removeDownload(int64_t id);
    bool startDownload(int64_t id);
    bool pauseDownload(int64_t id);
    bool resumeDownload(int64_t id);
    bool cancelDownload(int64_t id);
    bool pauseAll();
    bool resumeAll();
    bool cancelAll();

    DownloadInfo getDownloadInfo(int64_t id) const;
    std::vector<DownloadInfo> getAllDownloads() const;
    std::vector<DownloadInfo> getActiveDownloads() const;
    std::vector<DownloadInfo> getQueuedDownloads() const;

    int activeDownloadCount() const;
    int queuedDownloadCount() const;
    double totalSpeed() const;

    void processQueue();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace core
} // namespace remo

#endif // REMO_DOWNLOAD_CORE_DOWNLOAD_MANAGER_H