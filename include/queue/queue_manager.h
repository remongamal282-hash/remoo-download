#ifndef REMO_DOWNLOAD_QUEUE_QUEUE_MANAGER_H
#define REMO_DOWNLOAD_QUEUE_QUEUE_MANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace queue {

struct QueuedDownload {
    int64_t id = 0;
    std::string filename;
    int priority = 0;
    bool frozen = false;
    std::string status;
};

class QueueManager {
public:
    QueueManager();
    ~QueueManager();

    void enqueue(int64_t downloadId, const std::string& filename, int priority = 0);
    bool dequeue(int64_t downloadId);
    bool remove(int64_t downloadId);
    void freeze(int64_t downloadId);
    void unfreeze(int64_t downloadId);
    void reorder(int64_t downloadId, int newPriority);

    std::vector<QueuedDownload> getQueue() const;
    std::vector<QueuedDownload> getActiveDownloads() const;
    int activeCount() const;
    int maxConcurrent() const;
    void setMaxConcurrent(int max);
    bool canStartNew() const;

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace queue
} // namespace remo

#endif // REMO_DOWNLOAD_QUEUE_QUEUE_MANAGER_H