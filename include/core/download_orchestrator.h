#ifndef REMO_DOWNLOAD_CORE_DOWNLOAD_ORCHESTRATOR_H
#define REMO_DOWNLOAD_CORE_DOWNLOAD_ORCHESTRATOR_H

#include "engine/download_engine.h"
#include "engine/network_client.h"
#include "storage/storage_manager.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace remo {
namespace core {

class DownloadOrchestrator {
public:
    DownloadOrchestrator(const std::string& dbPath = "",
                         std::unique_ptr<engine::INetworkClient> networkClient = nullptr);
    ~DownloadOrchestrator();

    bool start();
    void stop();
    bool isRunning() const;

    // Entry point for IPC messages. Decoupled from transport.
    std::string processRequest(const std::string& requestJson);

    // Direct C++ API for tests & internal orchestrations
    int64_t addDownload(const std::string& url,
                        const std::string& savePath = "",
                        const std::string& filename = "",
                        const std::string& category = "");
    bool pauseDownload(int64_t downloadId);
    bool resumeDownload(int64_t downloadId);
    bool cancelDownload(int64_t downloadId);

    engine::DownloadEngine* getEngine();
    storage::StorageManager* getStorage();

private:
    std::string dbPath;
    std::unique_ptr<engine::INetworkClient> customNetworkClient;
    std::unique_ptr<storage::StorageManager> storageManager;
    std::unique_ptr<engine::DownloadEngine> downloadEngine;

    bool running = false;
    mutable std::mutex mutex;

    std::string handleAddDownload(const std::string& json);
    std::string handlePauseDownload(const std::string& json);
    std::string handleResumeDownload(const std::string& json);
    std::string handleCancelDownload(const std::string& json);
    std::string handleGetStatus(const std::string& json);
};

} // namespace core
} // namespace remo

#endif // REMO_DOWNLOAD_CORE_DOWNLOAD_ORCHESTRATOR_H
