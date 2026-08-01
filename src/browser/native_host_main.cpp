#include "browser/native_messaging_host.h"
#include "ipc/named_pipe_ipc.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// Extract field from JSON (simple parser for our known format)
std::string extractJsonField(const std::string& json, const std::string& field) {
    const std::string searchKey = "\"" + field + "\":\"";
    const size_t pos = json.find(searchKey);
    if (pos == std::string::npos) {
        return "";
    }
    const size_t startPos = pos + searchKey.length();
    const size_t endPos = json.find("\"", startPos);
    if (endPos == std::string::npos) {
        return "";
    }
    return json.substr(startPos, endPos - startPos);
}

std::string escapeJson(const std::string& str) {
    std::string result;
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

std::string buildAddDownloadRequest(const std::string& url,
                                     const std::string& filename,
                                     const std::string& savePath) {
    std::ostringstream oss;
    oss << "{"
        << "\"command\":\"addDownload\","
        << "\"url\":\"" << escapeJson(url) << "\"";

    if (!filename.empty()) {
        oss << ",\"filename\":\"" << escapeJson(filename) << "\"";
    }
    if (!savePath.empty()) {
        oss << ",\"savePath\":\"" << escapeJson(savePath) << "\"";
    }

    oss << "}";
    return oss.str();
}

// Open folder in file explorer (Windows-specific)
bool openFolderInExplorer(const std::string& path) {
#ifdef _WIN32
    std::string command = "explorer.exe /select,\"" + path + "\"";
    const int result = system(command.c_str());
    return result == 0;
#else
    // TODO: Add support for macOS and Linux
    return false;
#endif
}

} // namespace

int main() {
    remo::browser::NativeMessagingHost host;
    if (!host.start()) {
        std::cerr << "[native_host] Failed to start Native Messaging Host\n";
        return 1;
    }

    // Connect to remo_service via Named Pipe IPC
    remo::ipc::NamedPipeIpcClient ipcClient;
    const std::string pipeName = "remo_download_ipc";

    if (!ipcClient.connect(pipeName, 5000)) {
        const std::string errorResponse = R"({"ok":false,"error":"service_not_running"})";
        host.writeMessage(errorResponse);
        return 1;
    }

    while (host.isRunning()) {
        const std::string browserMessage = host.readMessage();
        if (browserMessage.empty()) {
            break;
        }

        // Extract message type
        const std::string messageType = extractJsonField(browserMessage, "type");

        // Handle getStatus request
        if (messageType == "getStatus") {
            const std::string statusRequest = R"({"command":"getStatus","downloadId":0})";
            const std::string serviceResponse = ipcClient.sendRequest(statusRequest);

            // Forward service response with type marker
            std::string browserResponse = R"({"type":"status","ok":true,)";
            browserResponse += serviceResponse.substr(1); // Skip opening brace

            if (!host.writeMessage(browserResponse)) {
                break;
            }
            continue;
        }

        // Handle openFolder request
        if (messageType == "openFolder") {
            const std::string path = extractJsonField(browserMessage, "path");

            if (!path.empty()) {
                const bool success = openFolderInExplorer(path);
                const std::string response = success
                    ? R"({"ok":true})"
                    : R"({"ok":false,"error":"failed_to_open_folder"})";

                if (!host.writeMessage(response)) {
                    break;
                }
            } else {
                const std::string errorResponse = R"({"ok":false,"error":"missing_path"})";
                if (!host.writeMessage(errorResponse)) {
                    break;
                }
            }
            continue;
        }

        // Handle addDownload request (default)
        const std::string url = extractJsonField(browserMessage, "url");
        const std::string filename = extractJsonField(browserMessage, "filename");
        const std::string savePath = extractJsonField(browserMessage, "savePath");

        if (url.empty()) {
            const std::string errorResponse = R"({"ok":false,"error":"missing_url"})";
            if (!host.writeMessage(errorResponse)) {
                break;
            }
            continue;
        }

        // Build request for remo_service (same format as remo_cli_client)
        const std::string serviceRequest = buildAddDownloadRequest(url, filename, savePath);

        // Send to service and get response
        const std::string serviceResponse = ipcClient.sendRequest(serviceRequest);

        // Forward service response back to browser
        // If service returned success, wrap it; otherwise forward as-is
        std::string browserResponse;
        if (serviceResponse.find("\"ok\":true") != std::string::npos ||
            serviceResponse.find("\"status\":\"accepted\"") != std::string::npos) {
            browserResponse = R"({"ok":true,"status":"accepted"})";
        } else {
            browserResponse = R"({"ok":false,"error":"service_error","details":)" + serviceResponse + "}";
        }

        if (!host.writeMessage(browserResponse)) {
            break;
        }
    }

    ipcClient.disconnect();
    host.stop();
    return 0;
}
