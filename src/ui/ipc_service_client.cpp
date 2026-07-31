#include "ui/ipc_service_client.h"

#include "ipc/named_pipe_ipc.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace remo {
namespace ui {

// ---------------------------------------------------------------------------
// Minimal JSON helpers — no external library needed (same approach as cli_client)
// ---------------------------------------------------------------------------
namespace {

std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { out += "\\r"; }
        else { out += c; }
    }
    return out;
}

/// \brief Extract the string value for a given JSON key (simple, single-pass).
/// Returns empty string if key not found.
std::string jsonGetString(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    std::string val;
    for (; pos < json.size(); ++pos) {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            if (json[pos] == '"')  val += '"';
            else if (json[pos] == '\\') val += '\\';
            else if (json[pos] == 'n')  val += '\n';
            else val += json[pos];
        } else if (json[pos] == '"') {
            break;
        } else {
            val += json[pos];
        }
    }
    return val;
}

/// \brief Extract an integer/double value for a JSON key (unquoted number).
double jsonGetNumber(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0.0;
    pos += needle.size();
    // skip whitespace
    while (pos < json.size() && json[pos] == ' ') ++pos;
    // read number
    std::string numStr;
    while (pos < json.size() && (std::isdigit(static_cast<unsigned char>(json[pos])) || json[pos] == '.' || json[pos] == '-')) {
        numStr += json[pos++];
    }
    if (numStr.empty()) return 0.0;
    try { return std::stod(numStr); } catch (...) { return 0.0; }
}

bool jsonGetBool(const std::string& json, const std::string& key) {
    const std::string trueNeedle  = "\"" + key + "\":true";
    const std::string falseNeedle = "\"" + key + "\":false";
    return json.find(trueNeedle) != std::string::npos;
}

/// \brief Parse an array of download status objects from the getStatus response.
std::vector<DownloadStatusInfo> parseDownloadArray(const std::string& json) {
    std::vector<DownloadStatusInfo> result;

    // The response has a "downloads" array: [...] OR a single "download" object
    // We look for opening braces that indicate individual objects
    auto arrStart = json.find("\"downloads\":");
    if (arrStart == std::string::npos) {
        // Single download object in "download" key or root
        auto objStart = json.find('{');
        if (objStart == std::string::npos) return result;

        // Check if this is the top-level response (has "success") or a nested download
        // Try to parse a single object
        DownloadStatusInfo info;
        info.id             = static_cast<int>(jsonGetNumber(json, "downloadId"));
        info.url            = jsonGetString(json, "url");
        info.filename       = jsonGetString(json, "filename");
        info.savePath       = jsonGetString(json, "savePath");
        info.status         = jsonGetString(json, "status");
        info.totalBytes     = static_cast<int64_t>(jsonGetNumber(json, "totalSizeBytes"));
        info.downloadedBytes= static_cast<int64_t>(jsonGetNumber(json, "downloadedBytes"));
        info.speedBytesPerSec = jsonGetNumber(json, "speedBytesPerSec");

        if (info.totalBytes > 0) {
            info.progressPct = 100.0 * static_cast<double>(info.downloadedBytes)
                               / static_cast<double>(info.totalBytes);
        } else {
            info.progressPct = 0.0;
        }

        if (info.id > 0) {
            result.push_back(info);
        }
        return result;
    }

    // Parse the array — find matching '[' and split by '},{'
    auto arrBracket = json.find('[', arrStart);
    if (arrBracket == std::string::npos) return result;

    size_t depth = 0;
    size_t objStart = std::string::npos;
    for (size_t i = arrBracket; i < json.size(); ++i) {
        char c = json[i];
        if (c == '{') {
            if (depth == 0) objStart = i;
            ++depth;
        } else if (c == '}') {
            if (depth > 0) --depth;
            if (depth == 0 && objStart != std::string::npos) {
                std::string objJson = json.substr(objStart, i - objStart + 1);

                DownloadStatusInfo info;
                info.id              = static_cast<int>(jsonGetNumber(objJson, "downloadId"));
                info.url             = jsonGetString(objJson, "url");
                info.filename        = jsonGetString(objJson, "filename");
                info.savePath        = jsonGetString(objJson, "savePath");
                info.status          = jsonGetString(objJson, "status");
                info.totalBytes      = static_cast<int64_t>(jsonGetNumber(objJson, "totalSizeBytes"));
                info.downloadedBytes = static_cast<int64_t>(jsonGetNumber(objJson, "downloadedBytes"));
                info.speedBytesPerSec = jsonGetNumber(objJson, "speedBytesPerSec");

                if (info.totalBytes > 0) {
                    info.progressPct = 100.0 * static_cast<double>(info.downloadedBytes)
                                       / static_cast<double>(info.totalBytes);
                } else {
                    info.progressPct = (info.status == "completed") ? 100.0 : 0.0;
                }

                if (info.id > 0) {
                    result.push_back(info);
                }
                objStart = std::string::npos;
            }
        } else if (c == ']' && depth == 0) {
            break;
        }
    }
    return result;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// IpcServiceClient::Impl
// ---------------------------------------------------------------------------
class IpcServiceClient::Impl {
public:
    remo::ipc::NamedPipeIpcClient client;
    bool connected = false;
    std::string pipeName = "remo_download_ipc";

    /// Attempt a fresh connection (the pipe client creates a new HANDLE each call)
    bool tryConnect(int timeoutMs) {
        connected = client.connect(pipeName, timeoutMs);
        return connected;
    }

    /// Send a request; returns empty string and marks disconnected on failure.
    std::string send(const std::string& req) {
        std::string resp = client.sendRequest(req);
        // Detect failure response (no actual pipe open confirmation needed per request)
        if (resp.find("\"success\":false") != std::string::npos &&
            (resp.find("Failed to connect") != std::string::npos ||
             resp.find("Failed to write") != std::string::npos)) {
            connected = false;
        }
        return resp;
    }
};

// ---------------------------------------------------------------------------
// IpcServiceClient — public interface
// ---------------------------------------------------------------------------
IpcServiceClient::IpcServiceClient()
    : d(std::make_unique<Impl>())
{}

IpcServiceClient::~IpcServiceClient() = default;

bool IpcServiceClient::connectToService(const std::string& pipeName, int timeoutMs) {
    d->pipeName = pipeName;
    d->connected = d->tryConnect(timeoutMs);
    return d->connected;
}

bool IpcServiceClient::isConnected() const {
    return d->connected;
}

void IpcServiceClient::disconnect() {
    d->client.disconnect();
    d->connected = false;
}

int IpcServiceClient::addDownload(const std::string& url,
                                  const std::string& filename,
                                  const std::string& savePath,
                                  const std::string& category) {
    std::ostringstream req;
    req << "{\"command\":\"addDownload\","
        << "\"url\":\"" << escapeJson(url) << "\","
        << "\"filename\":\"" << escapeJson(filename) << "\","
        << "\"savePath\":\"" << escapeJson(savePath) << "\","
        << "\"category\":\"" << escapeJson(category) << "\"}";

    std::string resp = d->send(req.str());

    if (resp.find("\"success\":true") == std::string::npos) {
        d->connected = false;
        return 0;
    }
    d->connected = true;
    return static_cast<int>(jsonGetNumber(resp, "downloadId"));
}

bool IpcServiceClient::pauseDownload(int downloadId) {
    std::ostringstream req;
    req << "{\"command\":\"pauseDownload\",\"downloadId\":" << downloadId << "}";
    std::string resp = d->send(req.str());
    bool ok = resp.find("\"success\":true") != std::string::npos;
    if (ok) d->connected = true;
    return ok;
}

bool IpcServiceClient::resumeDownload(int downloadId) {
    std::ostringstream req;
    req << "{\"command\":\"resumeDownload\",\"downloadId\":" << downloadId << "}";
    std::string resp = d->send(req.str());
    bool ok = resp.find("\"success\":true") != std::string::npos;
    if (ok) d->connected = true;
    return ok;
}

bool IpcServiceClient::cancelDownload(int downloadId) {
    std::ostringstream req;
    req << "{\"command\":\"cancelDownload\",\"downloadId\":" << downloadId << "}";
    std::string resp = d->send(req.str());
    bool ok = resp.find("\"success\":true") != std::string::npos;
    if (ok) d->connected = true;
    return ok;
}

std::vector<DownloadStatusInfo> IpcServiceClient::getStatus(int downloadId) {
    std::ostringstream req;
    req << "{\"command\":\"getStatus\",\"downloadId\":" << downloadId << "}";
    std::string resp = d->send(req.str());

    if (resp.find("\"success\":false") != std::string::npos &&
        (resp.find("Failed to connect") != std::string::npos ||
         resp.find("Failed to write") != std::string::npos)) {
        d->connected = false;
        return {};
    }
    d->connected = true;
    return parseDownloadArray(resp);
}

} // namespace ui
} // namespace remo
