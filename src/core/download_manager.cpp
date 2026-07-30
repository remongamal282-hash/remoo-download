#include "core/download_manager.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

namespace remo {
namespace core {

class DownloadManager::Impl {
public:
    std::mutex mutex;
    std::vector<DownloadInfo> downloads;
    int64_t nextId = 1;
    int maxConcurrent = 4;
    int activeCount = 0;
};

DownloadManager::DownloadManager()
    : d(std::make_unique<Impl>())
{
}

DownloadManager::~DownloadManager() = default;

int64_t DownloadManager::addDownload(const DownloadInfo& info) {
    std::lock_guard<std::mutex> lock(d->mutex);
    DownloadInfo record = info;
    record.id = d->nextId++;
    d->downloads.push_back(record);
    return record.id;
}

bool DownloadManager::removeDownload(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = std::remove_if(d->downloads.begin(), d->downloads.end(),
                             [id](const DownloadInfo& d) { return d.id == id; });
    if (it != d->downloads.end()) {
        d->downloads.erase(it, d->downloads.end());
        return true;
    }
    return false;
}

bool DownloadManager::startDownload(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.id == id) {
            if (dl.status == "downloading") {
                return true;
            }
            dl.status = "downloading";
            d->activeCount++;
            return true;
        }
    }
    return false;
}

bool DownloadManager::pauseDownload(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.id == id && dl.status == "downloading") {
            dl.status = "paused";
            d->activeCount--;
            return true;
        }
    }
    return false;
}

bool DownloadManager::resumeDownload(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.id == id && dl.status == "paused") {
            dl.status = "queued";
            return true;
        }
    }
    return false;
}

bool DownloadManager::cancelDownload(int64_t id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.id == id) {
            if (dl.status == "downloading") {
                d->activeCount--;
            }
            dl.status = "cancelled";
            return true;
        }
    }
    return false;
}

bool DownloadManager::pauseAll() {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.status == "downloading") {
            dl.status = "paused";
            d->activeCount--;
        }
    }
    return true;
}

bool DownloadManager::resumeAll() {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.status == "paused") {
            dl.status = "queued";
        }
    }
    return true;
}

bool DownloadManager::cancelAll() {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& dl : d->downloads) {
        if (dl.status == "downloading") {
            d->activeCount--;
        }
        dl.status = "cancelled";
    }
    return true;
}

DownloadInfo DownloadManager::getDownloadInfo(int64_t id) const {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (const auto& dl : d->downloads) {
        if (dl.id == id) {
            return dl;
        }
    }
    return DownloadInfo{};
}

std::vector<DownloadInfo> DownloadManager::getAllDownloads() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->downloads;
}

std::vector<DownloadInfo> DownloadManager::getActiveDownloads() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    std::vector<DownloadInfo> result;
    for (const auto& dl : d->downloads) {
        if (dl.status == "downloading" || dl.status == "queued" || dl.status == "paused") {
            result.push_back(dl);
        }
    }
    return result;
}

std::vector<DownloadInfo> DownloadManager::getQueuedDownloads() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    std::vector<DownloadInfo> result;
    for (const auto& dl : d->downloads) {
        if (dl.status == "queued") {
            result.push_back(dl);
        }
    }
    return result;
}

int DownloadManager::activeDownloadCount() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->activeCount;
}

int DownloadManager::queuedDownloadCount() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    int count = 0;
    for (const auto& dl : d->downloads) {
        if (dl.status == "queued") {
            count++;
        }
    }
    return count;
}

double DownloadManager::totalSpeed() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    double total = 0.0;
    for (const auto& dl : d->downloads) {
        if (dl.status == "downloading") {
            total += dl.speedBytesPerSec;
        }
    }
    return total;
}

void DownloadManager::processQueue() {
    std::lock_guard<std::mutex> lock(d->mutex);
    std::sort(d->downloads.begin(), d->downloads.end(),
              [](const DownloadInfo& a, const DownloadInfo& b) {
                  if (a.status == b.status) {
                      if (a.priority == b.priority) {
                          return a.id < b.id;
                      }
                      return a.priority > b.priority;
                  }
                  return a.status == "queued";
              });
    for (auto& dl : d->downloads) {
        if (d->activeCount >= d->maxConcurrent) {
            break;
        }
        if (dl.status == "queued") {
            dl.status = "downloading";
            d->activeCount++;
        }
    }
}

} // namespace core
} // namespace remo
