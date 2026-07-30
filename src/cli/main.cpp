#include "engine/download_engine.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

struct CliOptions {
    std::string url;
    std::filesystem::path outputPath;
    std::filesystem::path saveDirectory = std::filesystem::current_path();
    int connections = 4;
};

void printUsage(const char* executable) {
    std::cerr
        << "Usage: " << executable << " <url> [-o output-file] [-d directory] [-c connections]\n"
        << "\n"
        << "Examples:\n"
        << "  " << executable << " https://example.com/file.zip\n"
        << "  " << executable << " https://example.com/file.zip -d downloads -c 8\n";
}

std::string filenameFromUrl(const std::string& url) {
    std::string path = url;
    const auto queryPos = path.find_first_of("?#");
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);
    }

    const auto slashPos = path.find_last_of('/');
    std::string filename = slashPos == std::string::npos ? path : path.substr(slashPos + 1);
    if (filename.empty()) {
        filename = "download.bin";
    }
    return filename;
}

bool parsePositiveInt(const std::string& value, int& parsed) {
    char* end = nullptr;
    const long result = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || result < 1 || result > 64) {
        return false;
    }
    parsed = static_cast<int>(result);
    return true;
}

bool parseArguments(int argc, char* argv[], CliOptions& options) {
    if (argc < 2) {
        return false;
    }

    const std::string firstArg = argv[1];
    if (firstArg == "-h" || firstArg == "--help") {
        return false;
    }

    options.url = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            options.outputPath = argv[++i];
        } else if ((arg == "-d" || arg == "--directory") && i + 1 < argc) {
            options.saveDirectory = argv[++i];
        } else if ((arg == "-c" || arg == "--connections") && i + 1 < argc) {
            if (!parsePositiveInt(argv[++i], options.connections)) {
                std::cerr << "Invalid connection count. Use a value between 1 and 64.\n";
                return false;
            }
        } else if (arg == "-h" || arg == "--help") {
            return false;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }

    return !options.url.empty();
}

} // namespace

int main(int argc, char* argv[]) {
    const bool helpRequested = argc >= 2
        && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help");
    CliOptions options;
    if (!parseArguments(argc, argv, options)) {
        printUsage(argv[0]);
        return (argc < 2 || helpRequested) ? 0 : 2;
    }
    std::filesystem::path outputPath = options.outputPath;
    if (outputPath.empty()) {
        outputPath = options.saveDirectory / filenameFromUrl(options.url);
    } else if (!outputPath.has_parent_path()) {
        outputPath = options.saveDirectory / outputPath;
    }

    remo::engine::DownloadRequest request;
    request.url = options.url;
    request.filename = outputPath.filename().string();
    request.savePath = outputPath.parent_path().string();
    request.maxConnections = options.connections;

    std::cout << "Downloading " << request.url << "\n";
    std::cout << "Output: " << outputPath.string() << "\n";

    remo::engine::DownloadEngine engine(options.connections);
    if (!engine.startDownload(request)) {
        std::cerr << "Download failed.\n";
        return 1;
    }

    std::cout << "Download completed.\n";
    return 0;
}
