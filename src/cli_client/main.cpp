#include "ipc/named_pipe_ipc.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void printUsage(const std::string& progName) {
    std::cout << "Usage: " << progName << " <command> [options]\n\n"
              << "Commands:\n"
              << "  add-download <url> [-d savePath] [-f filename] [-c category]\n"
              << "  pause <downloadId>\n"
              << "  resume <downloadId>\n"
              << "  cancel <downloadId>\n"
              << "  status [downloadId]\n\n"
              << "Examples:\n"
              << "  " << progName << " add-download https://example.com/file.zip -d D:\\Downloads\n"
              << "  " << progName << " pause 1\n"
              << "  " << progName << " resume 1\n"
              << "  " << progName << " status\n";
}

std::string escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else result += c;
    }
    return result;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string subcommand = argv[1];
    std::string pipeName = "remo_download_ipc";
    std::ostringstream jsonReq;

    if (subcommand == "add-download") {
        if (argc < 3) {
            std::cerr << "Error: URL is required for add-download\n";
            return 1;
        }
        std::string url = argv[2];
        std::string savePath;
        std::string filename;
        std::string category;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-d" && i + 1 < argc) savePath = argv[++i];
            else if (arg == "-f" && i + 1 < argc) filename = argv[++i];
            else if (arg == "-c" && i + 1 < argc) category = argv[++i];
            else if (arg == "--pipe" && i + 1 < argc) pipeName = argv[++i];
        }

        jsonReq << "{"
                << "\"command\":\"addDownload\","
                << "\"url\":\"" << escapeJson(url) << "\","
                << "\"savePath\":\"" << escapeJson(savePath) << "\","
                << "\"filename\":\"" << escapeJson(filename) << "\","
                << "\"category\":\"" << escapeJson(category) << "\""
                << "}";
    } else if (subcommand == "pause") {
        if (argc < 3) {
            std::cerr << "Error: downloadId is required for pause\n";
            return 1;
        }
        int downloadId = std::stoi(argv[2]);
        jsonReq << "{\"command\":\"pauseDownload\",\"downloadId\":" << downloadId << "}";
    } else if (subcommand == "resume") {
        if (argc < 3) {
            std::cerr << "Error: downloadId is required for resume\n";
            return 1;
        }
        int downloadId = std::stoi(argv[2]);
        jsonReq << "{\"command\":\"resumeDownload\",\"downloadId\":" << downloadId << "}";
    } else if (subcommand == "cancel") {
        if (argc < 3) {
            std::cerr << "Error: downloadId is required for cancel\n";
            return 1;
        }
        int downloadId = std::stoi(argv[2]);
        jsonReq << "{\"command\":\"cancelDownload\",\"downloadId\":" << downloadId << "}";
    } else if (subcommand == "status") {
        int downloadId = 0;
        if (argc >= 3) {
            try { downloadId = std::stoi(argv[2]); } catch (...) {}
        }
        jsonReq << "{\"command\":\"getStatus\",\"downloadId\":" << downloadId << "}";
    } else {
        std::cerr << "Error: Unknown subcommand '" << subcommand << "'\n";
        printUsage(argv[0]);
        return 1;
    }

    remo::ipc::NamedPipeIpcClient client;
    std::cout << "[remo_cli_client] Connecting to IPC pipe \\\\.\\pipe\\" << pipeName << "...\n";
    if (!client.connect(pipeName, 5000)) {
        std::cerr << "[remo_cli_client] ERROR: Could not connect to remo-service IPC pipe. Is remo_service running?\n";
        return 1;
    }

    std::string requestStr = jsonReq.str();
    std::cout << "[remo_cli_client Request]  " << requestStr << "\n";

    std::string responseStr = client.sendRequest(requestStr);
    std::cout << "[remo_cli_client Response] " << responseStr << "\n";

    client.disconnect();
    return 0;
}
