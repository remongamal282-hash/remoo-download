#ifndef REMO_DOWNLOAD_UI_IPC_SERVICE_CLIENT_H
#define REMO_DOWNLOAD_UI_IPC_SERVICE_CLIENT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace remo {
namespace ui {

/// \brief Represents the live status of a single download as reported by remo_service.
struct DownloadStatusInfo {
    int         id             = 0;
    std::string url;
    std::string filename;
    std::string savePath;
    std::string status;         ///< "queued","downloading","paused","completed","failed","cancelled"
    double      progressPct    = 0.0;
    double      speedBytesPerSec = 0.0;
    int64_t     totalBytes     = 0;
    int64_t     downloadedBytes = 0;
};

/// \brief Pure-C++ wrapper around NamedPipeIpcClient.
///
/// Sends JSON requests to remo_service and parses the JSON responses.
/// Does NOT contain any Qt types so that it can be used with the Impl pattern
/// without needing MOC / QObject inheritance.
class IpcServiceClient {
public:
    IpcServiceClient();
    ~IpcServiceClient();

    // Non-copyable
    IpcServiceClient(const IpcServiceClient&) = delete;
    IpcServiceClient& operator=(const IpcServiceClient&) = delete;

    /// \brief Try to connect to remo_service on the default pipe.
    /// \return true if connection succeeded, false otherwise.
    bool connectToService(const std::string& pipeName = "remo_download_ipc",
                          int timeoutMs = 500);

    /// \brief Check whether the last sendRequest succeeded.
    bool isConnected() const;

    /// \brief Disconnect from the service (client reconnects on next call automatically).
    void disconnect();

    // ---- Download commands -----------------------------------------------

    /// \brief Add a new download. Returns the new downloadId (0 on error).
    int addDownload(const std::string& url,
                    const std::string& filename,
                    const std::string& savePath,
                    const std::string& category = "");

    bool pauseDownload(int downloadId);
    bool resumeDownload(int downloadId);
    bool cancelDownload(int downloadId);

    /// \brief Poll all downloads (downloadId == 0) or a specific one.
    std::vector<DownloadStatusInfo> getStatus(int downloadId = 0);

private:
    class Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ui
} // namespace remo

#endif // REMO_DOWNLOAD_UI_IPC_SERVICE_CLIENT_H
