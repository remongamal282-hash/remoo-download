#include "queue/queue_manager.h"

#include <algorithm>
#include <mutex>

namespace remo {
namespace queue {

class QueueManager::Impl {
public:
    std::mutex mutex;
    std::vector<QueuedDownload> queue;
    int maxConcurrent = 4;
};

QueueManager::QueueManager()
    : d(std::make_unique<Impl>())
{
}

QueueManager::~QueueManager() = default;

void QueueManager::enqueue(int64_t downloadId, const std::string& filename, int priority) {
    std::lock_guard<std::mutex> lock(d->mutex);
    QueuedDownload item;
    item.id = downloadId;
    item.filename = filename;
    item.priority = priority;
    item.frozen = false;
    item.status = "queued";
    d->queue.push_back(item);
    std::sort(d->queue.begin(), d->queue.end(),
              [](const QueuedDownload& a, const QueuedDownload& b) {
                  return a.priority > b.priority;
              });
}

bool QueueManager::dequeue(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    auto it = std::remove_if(d->queue.begin(), d->queue.end(),
                             [downloadId](const QueuedDownload& item) {
                                 return item.id == downloadId;
                             });
    if (it != d->queue.end()) {
        d->queue.erase(it, d->queue.end());
        return true;
    }
    return false;
}

bool QueueManager::remove(int64_t downloadId) {
    return dequeue(downloadId);
}

void QueueManager::freeze(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& item : d->queue) {
        if (item.id == downloadId) {
            item.frozen = true;
            break;
        }
    }
}

void QueueManager::unfreeze(int64_t downloadId) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& item : d->queue) {
        if (item.id == downloadId) {
            item.frozen = false;
            break;
        }
    }
}

void QueueManager::reorder(int64_t downloadId, int newPriority) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto& item : d->queue) {
        if (item.id == downloadId) {
            item.priority = newPriority;
            break;
        }
    }
    std::sort(d->queue.begin(), d->queue.end(),
              [](const QueuedDownload& a, const QueuedDownload& b) {
                  return a.priority > b.priority;
              });
}

std::vector<QueuedDownload> QueueManager::getQueue() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->queue;
}

std::vector<QueuedDownload> QueueManager::getActiveDownloads() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    std::vector<QueuedDownload> active;
    for (const auto& item : d->queue) {
        if (!item.frozen && item.status == "queued") {
            active.push_back(item);
        }
    }
    return active;
}

int QueueManager::activeCount() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    int count = 0;
    for (const auto& item : d->queue) {
        if (item.status == "downloading") {
            count++;
        }
    }
    return count;
}

int QueueManager::maxConcurrent() const {
    return d->maxConcurrent;
}

void QueueManager::setMaxConcurrent(int max) {
    d->maxConcurrent = max;
}

bool QueueManager::canStartNew() const {
    return activeCount() < d->maxConcurrent;
}

} // namespace queue
} // namespace remo