#include "core/download_orchestrator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace remo {
namespace core {

namespace {

std::string extractStringField(const std::string& json, const std::string& key) {
    const std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return "";
    }
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    if (pos < json.length() && json[pos] == '"') {
        pos++;
        size_t endPos = json.find('"', pos);
        if (endPos != std::string::npos) {
            return json.substr(pos, endPos - pos);
        }
    }
    return "";
}

int64_t extractIntField(const std::string& json, const std::string& key, int64_t defaultValue = 0) {
    const std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return defaultValue;
    }
    pos += pattern.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '"')) {
        pos++;
    }
    size_t endPos = pos;
    while (endPos < json.length() && (std::isdigit(static_cast<unsigned char>(json[endPos])) || json[endPos] == '-')) {
        endPos++;
    }
    if (endPos > pos) {
        try {
            return std::stoll(json.substr(pos, endPos - pos));
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

std::string escapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    for (char c : str) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    return result;
}

} // namespace

DownloadOrchestrator::DownloadOrchestrator(const std::string& dbPath,
                                           std::unique_ptr<engine::INetworkClient> networkClient)
    : dbPath(dbPath), customNetworkClient(std::move(networkClient))
{
}

DownloadOrchestrator::~DownloadOrchestrator() {
    stop();
}

bool DownloadOrchestrator::start() {
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        return true;
    }

    if (!dbPath.empty()) {
        storageManager = std::make_unique<storage::StorageManager>(dbPath);
        if (!storageManager->open()) {
            storageManager.reset();
        }
    }

    if (customNetworkClient) {
        downloadEngine = std::make_unique<engine::DownloadEngine>(4, std::move(customNetworkClient), storageManager.get());
    } else {
        downloadEngine = std::make_unique<engine::DownloadEngine>(4);
        if (storageManager) {
            downloadEngine->setStorageManager(storageManager.get());
        }
    }

    // Task 1.3 requirement: Startup Recovery execution on launch
    if (downloadEngine && storageManager) {
        downloadEngine->recoverUnfinishedDownloads();
    }

    running = true;
    return true;
}

void DownloadOrchestrator::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running) {
        return;
    }

    downloadEngine.reset();
    if (storageManager) {
        storageManager->close();
        storageManager.reset();
    }
    running = false;
}

bool DownloadOrchestrator::isRunning() const {
    return running;
}

std::string DownloadOrchestrator::processRequest(const std::string& requestJson) {
    const std::string cmd = extractStringField(requestJson, "command");
    const std::string reqId = extractStringField(requestJson, "requestId");

    std::string responseBody;
    if (cmd == "addDownload") {
        responseBody = handleAddDownload(requestJson);
    } else if (cmd == "pauseDownload") {
        responseBody = handlePauseDownload(requestJson);
    } else if (cmd == "resumeDownload") {
        responseBody = handleResumeDownload(requestJson);
    } else if (cmd == "cancelDownload") {
        responseBody = handleCancelDownload(requestJson);
    } else if (cmd == "getStatus") {
        responseBody = handleGetStatus(requestJson);
    } else {
        responseBody = "{\"success\":false,\"errorMessage\":\"Unknown or unsupported command\"}";
    }

    if (!reqId.empty()) {
        // Insert requestId if present
        size_t insertPos = responseBody.find('{');
        if (insertPos != std::string::npos) {
            responseBody.insert(insertPos + 1, "\"requestId\":\"" + escapeJson(reqId) + "\",");
        }
    }

    return responseBody;
}

int64_t DownloadOrchestrator::addDownload(const std::string& url,
                                          const std::string& savePath,
                                          const std::string& filename,
                                          const std::string& category) {
    if (!downloadEngine) {
        return -1;
    }

    std::string effectiveFilename = filename;
    if (effectiveFilename.empty()) {
        size_t lastSlash = url.find_last_of("/\\");
        if (lastSlash != std::string::npos && lastSlash + 1 < url.length()) {
            effectiveFilename = url.substr(lastSlash + 1);
            size_t queryPos = effectiveFilename.find_first_of("?#");
            if (queryPos != std::string::npos) {
                effectiveFilename = effectiveFilename.substr(0, queryPos);
            }
        }
        if (effectiveFilename.empty()) {
            effectiveFilename = "download.bin";
        }
    }

    std::string effectiveSavePath = savePath;
    if (effectiveSavePath.empty()) {
        effectiveSavePath = std::filesystem::current_path().string();
    }

    if (storageManager) {
        storage::DownloadRecord record;
        record.url = url;
        record.filename = effectiveFilename;
        record.savePath = effectiveSavePath;
        record.status = "queued";
        record.sourceExtension = category;
        storageManager->saveDownload(record);
    }

    engine::DownloadRequest req;
    req.url = url;
    req.filename = effectiveFilename;
    req.savePath = effectiveSavePath;
    req.categoryId = category;

    return downloadEngine->startDownload(req);
}

bool DownloadOrchestrator::pauseDownload(int64_t downloadId) {
    if (!downloadEngine) return false;
    bool ok = downloadEngine->pauseDownload(downloadId);
    if (ok && storageManager) {
        storageManager->updateDownloadStatus(downloadId, "paused");
    }
    return ok;
}

bool DownloadOrchestrator::resumeDownload(int64_t downloadId) {
    if (!downloadEngine) return false;
    bool ok = downloadEngine->resumeDownload(downloadId);
    if (ok && storageManager) {
        storageManager->updateDownloadStatus(downloadId, "downloading");
    }
    return ok;
}

bool DownloadOrchestrator::cancelDownload(int64_t downloadId) {
    if (!downloadEngine) return false;
    bool ok = downloadEngine->cancelDownload(downloadId);
    if (ok && storageManager) {
        storageManager->updateDownloadStatus(downloadId, "cancelled");
    }
    return ok;
}

engine::DownloadEngine* DownloadOrchestrator::getEngine() {
    return downloadEngine.get();
}

storage::StorageManager* DownloadOrchestrator::getStorage() {
    return storageManager.get();
}

std::string DownloadOrchestrator::handleAddDownload(const std::string& json) {
    const std::string url = extractStringField(json, "url");
    const std::string savePath = extractStringField(json, "savePath");
    const std::string filename = extractStringField(json, "filename");
    const std::string category = extractStringField(json, "category");

    if (url.empty()) {
        return "{\"success\":false,\"errorMessage\":\"URL is required\"}";
    }

    int64_t downloadId = addDownload(url, savePath, filename, category);
    if (downloadId <= 0) {
        return "{\"success\":false,\"errorMessage\":\"Failed to start download\"}";
    }

    std::ostringstream ss;
    ss << "{\"success\":true,\"downloadId\":" << downloadId << "}";
    return ss.str();
}

std::string DownloadOrchestrator::handlePauseDownload(const std::string& json) {
    int64_t downloadId = extractIntField(json, "downloadId", 0);
    if (downloadId <= 0) {
        return "{\"success\":false,\"errorMessage\":\"Invalid downloadId\"}";
    }

    bool ok = pauseDownload(downloadId);
    std::ostringstream ss;
    ss << "{\"success\":" << (ok ? "true" : "false") << ",\"downloadId\":" << downloadId << "}";
    return ss.str();
}

std::string DownloadOrchestrator::handleResumeDownload(const std::string& json) {
    int64_t downloadId = extractIntField(json, "downloadId", 0);
    if (downloadId <= 0) {
        return "{\"success\":false,\"errorMessage\":\"Invalid downloadId\"}";
    }

    bool ok = resumeDownload(downloadId);
    std::ostringstream ss;
    ss << "{\"success\":" << (ok ? "true" : "false") << ",\"downloadId\":" << downloadId << "}";
    return ss.str();
}

std::string DownloadOrchestrator::handleCancelDownload(const std::string& json) {
    int64_t downloadId = extractIntField(json, "downloadId", 0);
    if (downloadId <= 0) {
        return "{\"success\":false,\"errorMessage\":\"Invalid downloadId\"}";
    }

    bool ok = cancelDownload(downloadId);
    std::ostringstream ss;
    ss << "{\"success\":" << (ok ? "true" : "false") << ",\"downloadId\":" << downloadId << "}";
    return ss.str();
}

std::string DownloadOrchestrator::handleGetStatus(const std::string& json) {
    int64_t filterId = extractIntField(json, "downloadId", 0);

    std::ostringstream ss;
    ss << "{\"success\":true,\"downloads\":[";

    bool first = true;
    if (storageManager) {
        auto downloads = filterId > 0 ? std::vector<storage::DownloadRecord>{storageManager->getDownload(filterId)}
                                      : storageManager->getAllDownloads();
        for (const auto& record : downloads) {
            if (record.id <= 0) continue;
            if (!first) ss << ",";
            first = false;

            engine::DownloadProgress prog;
            if (downloadEngine) {
                prog = downloadEngine->getProgress(record.id);
            }

            std::string statusStr = !prog.statusMessage.empty() ? prog.statusMessage : record.status;
            int64_t downloaded = prog.downloadedSize > 0 ? prog.downloadedSize : record.downloadedBytes;
            int64_t total = prog.totalSize > 0 ? prog.totalSize : record.totalSizeBytes;

            ss << "{"
               << "\"id\":" << record.id << ","
               << "\"url\":\"" << escapeJson(record.url) << "\","
               << "\"filename\":\"" << escapeJson(record.filename) << "\","
               << "\"savePath\":\"" << escapeJson(record.savePath) << "\","
               << "\"status\":\"" << escapeJson(statusStr) << "\","
               << "\"progressPercent\":" << prog.progressPercent << ","
               << "\"downloadedBytes\":" << downloaded << ","
               << "\"totalBytes\":" << total << ","
               << "\"speedBytesPerSec\":" << prog.speedBytesPerSec
               << "}";
        }
    }

    ss << "]}";
    return ss.str();
}

} // namespace core
} // namespace remo
