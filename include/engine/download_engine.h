# Remo Download - Core Headers
# Download Engine Interface

#ifndef REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H
#define REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace engine {

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
};

struct DownloadProgress {
    int64_t totalSize = 0;
    int64_t downloadedSize = 0;
    double speedBytesPerSec = 0.0;
    double progressPercent = 0.0;
    int activeSegments = 0;
    int totalSegments = 0;
    std::string statusMessage;
};

class DownloadEngine {
public:
    explicit DownloadEngine(int maxConnections = 4);
    ~DownloadEngine();

    DownloadEngine(const DownloadEngine&) = delete;
    DownloadEngine& operator=(const DownloadEngine&) = delete;

    bool startDownload(const DownloadRequest& request);
    bool pauseDownload(int64_t downloadId);
    bool resumeDownload(int64_t downloadId);
    bool cancelDownload(int64_t downloadId);
    DownloadProgress getProgress(int64_t downloadId);
    bool hasActiveDownloads() const;
    int activeDownloadCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace engine
} // namespace remo

#endif // REMO_DOWNLOAD_ENGINE_DOWNLOAD_ENGINE_H