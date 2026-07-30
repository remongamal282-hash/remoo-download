#ifndef REMO_COMMON_TYPES_H
#define REMO_COMMON_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace remo {
namespace common {

using ByteSize = uint64_t;
using SpeedBytesPerSec = double;
using Timestamp = int64_t;
using DownloadId = int64_t;
using SegmentId = int64_t;
using TaskId = int64_t;

enum class DownloadStatus {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled,
    Retrying
};

enum class SegmentStatus {
    Pending,
    Downloading,
    Completed,
    Failed,
    Retrying
};

enum class Platform {
    Windows,
    macOS,
    Linux
};

Platform detectPlatform();

struct DownloadInfo {
    DownloadId id = 0;
    std::string url;
    std::string finalUrl;
    std::string filename;
    std::string savePath;
    ByteSize fileSize = 0;
    ByteSize downloadedSize = 0;
    DownloadStatus status = DownloadStatus::Queued;
    int priority = 0;
    std::string categoryId;
    int maxConnections = 4;
    SpeedBytesPerSec speedBytesPerSec = 0.0;
    double progressPercent = 0.0;
    std::string etag;
    std::string lastModified;
    std::string checksumMd5;
    std::string checksumSha256;
    std::string actualChecksumMd5;
    std::string actualChecksumSha256;
    int checksumValid = 0;
    int totalSegments = 1;
    int completedSegments = 0;
    int retryCount = 0;
    std::string errorMessage;
    Timestamp createdAt = 0;
    Timestamp updatedAt = 0;
};

struct SegmentInfo {
    SegmentId id = 0;
    DownloadId downloadId = 0;
    int index = 0;
    ByteSize startByte = 0;
    ByteSize endByte = 0;
    ByteSize downloadedBytes = 0;
    SegmentStatus status = SegmentStatus::Pending;
    std::string tempFilePath;
};

} // namespace common
} // namespace remo

#endif // REMO_COMMON_TYPES_H