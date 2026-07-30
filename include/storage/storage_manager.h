#ifndef REMO_DOWNLOAD_STORAGE_STORAGE_MANAGER_H
#define REMO_DOWNLOAD_STORAGE_STORAGE_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace storage {

struct DownloadRecord {
    int64_t id = 0;
    std::string url;
    std::string finalUrl;
    std::string filename;
    std::string savePath;
    int64_t categoryId = 0;
    int64_t totalSizeBytes = 0;
    int64_t downloadedBytes = 0;
    std::string status;
    int priority = 0;
    bool supportsResume = false;
    std::string checksumAlgorithm;
    std::string checksumExpected;
    std::string checksumActual;
    int retryCount = 0;
    int maxRetries = 10;
    int64_t speedLimitBytesPerSec = 0;
    std::string referrerUrl;
    bool authRequired = false;
    std::string authUsername;
    std::string authSecretRef;
    int64_t scheduleId = 0;
    std::string sourceExtension;
    std::string errorMessage;
    std::string createdAt;
    std::string completedAt;
    std::string lastCheckpointAt;
};

struct SegmentRecord {
    int64_t id = 0;
    int64_t downloadId = 0;
    int segmentIndex = 0;
    int64_t rangeStart = 0;
    int64_t rangeEnd = 0;
    int64_t downloadedBytes = 0;
    std::string status;
    std::string lastError;
    std::string updatedAt;
};

struct CategoryRecord {
    int64_t id = 0;
    std::string name;
    std::string defaultPath;
    std::string matchRule;
    int64_t parentCategoryId = 0;
    std::string icon;
    bool isSystemDefault = false;
    std::string createdAt;
};

class StorageManager {
public:
    explicit StorageManager(const std::string& dbPath);
    ~StorageManager();

    bool open();
    void close();
    bool isOpen() const;
    int getSchemaVersion() const;

    int64_t saveDownload(const DownloadRecord& record);
    bool updateDownload(int64_t id, const DownloadRecord& record);
    DownloadRecord getDownload(int64_t id) const;
    std::vector<DownloadRecord> getAllDownloads() const;
    std::vector<DownloadRecord> getDownloadsByStatus(const std::string& status) const;
    std::vector<DownloadRecord> getUnfinishedDownloads() const;
    bool updateDownloadStatus(int64_t id, const std::string& status, const std::string& errorMessage = "");
    std::vector<SegmentRecord> getSegments(int64_t downloadId) const;
    bool saveSegment(const SegmentRecord& segment);
    bool updateSegment(int64_t id, const SegmentRecord& segment);
    bool saveCheckpoint(int64_t downloadId, int64_t segmentId, const std::string& data);
    std::string restoreCheckpoint(int64_t downloadId, int64_t segmentId) const;
    int64_t saveCategory(const CategoryRecord& category);
    std::vector<CategoryRecord> getAllCategories() const;
    bool logDownloadEvent(int64_t downloadId, const std::string& eventType, const std::string& details = "");
    bool setSetting(const std::string& key, const std::string& value, const std::string& valueType);
    std::string getSetting(const std::string& key) const;
    bool deleteDownload(int64_t id);
    bool cleanupCompleted();

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace storage
} // namespace remo

#endif // REMO_DOWNLOAD_STORAGE_STORAGE_MANAGER_H
